// simulationworker.cpp - QThread wrapper for curcuma MD and geometry optimization
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration (stepwise API)

#include "simulationworker.h"

#include "external/json.hpp"
using json = nlohmann::json;

#include <src/core/molecule.h>
#include <src/capabilities/simplemd.h>
#include <src/capabilities/optimizer_factory.h>
#include <src/capabilities/optimizer_interface.h>
#include <src/core/energycalculator.h>
#include <src/core/elements.h>

#include <QCoreApplication>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <algorithm>
#include <limits>

// Claude Generated 2026 - Write curcuma's RMSD-MTD bias parameters into the
// simplemd controller block. Only emitted when RMSD-MTD is enabled, so the
// defaults stay curcuma-side otherwise. Mirrors the "RMSD-MTD" PARAM category
// in external/curcuma/src/capabilities/simplemd.h.
namespace {
void applyRmsdMtdParams(const SimulationConfig& cfg, json& simplemd_params)
{
    if (!cfg.rmsdMtd)
        return;
    simplemd_params["rmsd_mtd"] = true;
    simplemd_params["rmsd_mtd_k"] = cfg.rmsdMtdK;
    simplemd_params["rmsd_mtd_alpha"] = cfg.rmsdMtdAlpha;
    simplemd_params["rmsd_mtd_atoms"] = cfg.rmsdMtdAtoms.toStdString();
    simplemd_params["rmsd_mtd_ref_file"] = cfg.rmsdMtdRefFile.toStdString();
    simplemd_params["rmsd_mtd_max_gaussians"] = cfg.rmsdMtdMaxGaussians;
    simplemd_params["rmsd_mtd_max_height"] = cfg.rmsdMtdMaxHeight;
    simplemd_params["rmsd_econv"] = cfg.rmsdMtdEconv;  // bias-deposition convergence threshold (setEnergyConv)
    simplemd_params["rmsd_mtd_pace"] = cfg.rmsdMtdPace;  // unused in counter scheme (compat)
    if (cfg.rmsdMtdWtmtd) {
        simplemd_params["wtmtd"] = true;
        simplemd_params["rmsd_mtd_dt"] = cfg.rmsdMtdDt;  // only used when wtmtd
    }
    if (cfg.rmsdMtdFreezeInherited)
        simplemd_params["rmsd_mtd_freeze_inherited"] = true;
}
}  // namespace

SimulationWorker::SimulationWorker(QObject* parent)
    : QObject(parent)
{
    static const int kPtrTypeId = qRegisterMetaType<SimulationFramePtr>("SimulationFramePtr");
    Q_UNUSED(kPtrTypeId);
}

// Defined here (not =default in header) so unique_ptr<SimpleMD>'s deleter sees the complete type.
SimulationWorker::~SimulationWorker() = default;

// Forward declarations for helpers used by both stepOnce() (above their
// definition site) and the rest of the worker methods.
static Molecule atomsToMolecule(const QVector<MoleculeViewer::Atom>& atoms);
static SimulationFramePtr moleculeToFrame(
    const Molecule& mol, int referenceSize, double energy, double ekin, int step);
static Vector pendingForcesToFlatVector(const Eigen::MatrixXd& pending);

void SimulationWorker::setMolecule(const QVector<MoleculeViewer::Atom>& atoms)
{
    m_initialAtoms = atoms;
    m_adjacency = forceinjector::buildAdjacency(atoms.size(), m_bonds);
}

void SimulationWorker::setBonds(const QVector<MoleculeViewer::Bond>& bonds)
{
    m_bonds = bonds;
    m_adjacency = forceinjector::buildAdjacency(m_initialAtoms.size(), m_bonds);
}

void SimulationWorker::injectForce(int atomIndex, QVector3D force, double alpha, int maxShells)
{
    if (m_initialAtoms.isEmpty())
        return;
    // [GRAB-DEBUG] temporary: confirm the GUI->worker cross-thread call arrives.
    // qDebug().nospace() << "[GRAB] injectForce atom=" << atomIndex
    //                    << " f=(" << force.x() << "," << force.y() << "," << force.z() << ")"
    //                    << " |f|=" << force.length()
    //                    << " alpha=" << alpha << " shells=" << maxShells
    //                    << " thread=" << QThread::currentThread();
    Eigen::Vector3d f(force.x(), force.y(), force.z());
    Eigen::MatrixXd distributed = forceinjector::distributeForce(
        atomIndex, f, m_adjacency, alpha, maxShells, m_initialAtoms.size());

    QMutexLocker lock(&m_forceMutex);
    m_pendingForces = distributed;
    m_pendingForcesValid = true;
}

void SimulationWorker::clearInjectedForce()
{
    // qDebug() << "[GRAB] clearInjectedForce (grab released)";
    QMutexLocker lock(&m_forceMutex);
    m_pendingForcesValid = false;
    m_pendingForces.resize(0, 0);
}

// Claude Generated 2026 - One-shot step from the dock's Step button.
//
// Spawns a fresh worker state for a single iteration:
//   - MD mode: builds a new SimpleMD on the current geometry, runs exactly one
//     md.step(), emits one frame, then emits finished().
//   - Opt mode: builds the OptimizerDriver fresh on the current geometry, calls
//     Optimize(max_iterations=1, single_step_mode=true) so curcuma exits after
//     one iteration, emits one frame, then emits finished().
//
// Safe to call repeatedly — each click is independent. The dock throttles
// clicks by 1/fpsLimit to honour the user's "max XXX FPS" preference.
void SimulationWorker::stepOnce()
{
    if (m_initialAtoms.isEmpty()) {
        emit errorOccurred(tr("No molecule loaded. Please open a molecule file first."));
        emit finished();
        return;
    }

    m_stopRequested.storeRelaxed(0);
    m_lastEmitTimer.start();

    switch (m_config.mode) {
    case SimulationConfig::Mode::MolecularDynamics: {
        // Use a single-step MD: build SimpleMD on the worker's current geometry
        // (which is the same as the initial geometry the dock fed in, since we
        // run each Step click independently). For MD, the geometry is whatever
        // the viewer has — for now we restart from m_initialAtoms. The dock
        // should re-spawn the worker with the current viewer geometry; this is
        // handled by the standard setMolecule() path before the call.
        json simplemd_params;
        simplemd_params["method"] = m_config.method.toStdString();
        simplemd_params["temperature"] = m_config.temperature;
        simplemd_params["time_step"] = m_config.timestep;
        simplemd_params["dump_frequency"] = 1;
        simplemd_params["max_time"] = m_config.timestep;  // one step only
        simplemd_params["print_frequency"] = m_config.performanceAnalysis ? 1 : 1000;
        simplemd_params["write_xyz"] = false;
        simplemd_params["no_restart"] = true;
        simplemd_params["no_center"] = true;
        simplemd_params["hmass"] = m_config.hmass;
        applyRmsdMtdParams(m_config, simplemd_params);

        json controller;
        controller["simplemd"] = simplemd_params;
        controller["global"]["method"] = m_config.method.toStdString();
        controller["global"]["gpu"] = m_config.gpu.toStdString();
        controller["global"]["verbosity"] = 0;
        controller["verbosity"] = 0;

        auto md = std::make_unique<SimpleMD>(controller, true);
        md->setMolecule(atomsToMolecule(m_initialAtoms));
        if (!md->Initialise()) {
            emit errorOccurred(tr("MD initialization failed for single step."));
            emit finished();
            return;
        }
        md->prepareRun();
        // Apply the currently held grab force for this single step
        Eigen::MatrixXd pending = currentInjectedForces(m_initialAtoms.size());
        if (pending.rows() > 0) {
            Geometry ext = pending;
            md->applyExternalForces(ext);
        }
        if (md->step()) {
            emit frameReady(moleculeToFrame(
                md->currentMolecule(), m_initialAtoms.size(),
                md->potentialEnergy(), md->kineticEnergy(), md->stepCount()));
        }
        md->finalizeRun();
        emit finished();
        return;
    }
    case SimulationConfig::Mode::GeometryOptimization: {
        // Build a fresh OptimizerDriver on the current geometry and run exactly
        // one iteration via single_step_mode. This restarts LBFGS history from
        // scratch each click — expensive for big systems, but matches the dock's
        // "manual convergence" UX. The user can also click Start to run a full
        // auto-converge in one shot.
        json opt_config;
        opt_config["max_iterations"] = 1;
        opt_config["gradient_threshold"] = m_config.convergence;
        opt_config["single_step_mode"] = true;  // break after one iteration
        opt_config["write_trajectory"] = false;
        opt_config["verbosity"] = 0;

        json energy_controller;
        energy_controller["method"] = m_config.method.toStdString();
        energy_controller["gpu"] = m_config.gpu.toStdString();
        energy_controller["verbosity"] = 0;

        try {
            EnergyCalculator calc(m_config.method.toStdString(), energy_controller);
            Optimization::OptimizerType opt_type =
                Optimization::parseOptimizerType(m_config.optimizer.toStdString());
            auto optimizer = Optimization::OptimizerFactory::createOptimizer(opt_type, &calc);
            if (!optimizer) {
                emit errorOccurred(tr("Failed to create optimizer '%1'").arg(m_config.optimizer));
                emit finished();
                return;
            }
            json merged = optimizer->GetDefaultConfiguration();
            for (auto it = opt_config.begin(); it != opt_config.end(); ++it)
                merged[it.key()] = it.value();
            optimizer->LoadConfiguration(merged);

            Molecule mol = atomsToMolecule(m_initialAtoms);
            emit frameReady(moleculeToFrame(mol, m_initialAtoms.size(), 0.0, 0.0, 0));
            if (!optimizer->InitializeOptimization(mol)) {
                emit errorOccurred(tr("Optimizer initialization failed for single step."));
                emit finished();
                return;
            }
            // Claude Generated 2026 - Apply the currently held mouse-grab force
            // and feed it to the optimizer before the single iteration.
            Vector ext = pendingForcesToFlatVector(currentInjectedForces(m_initialAtoms.size()));
            if (ext.size() > 0)
                optimizer->setExternalForces(ext);
            Optimization::OptimizationResult result = optimizer->Optimize(false, 0);
            optimizer->clearExternalForces();
            if (result.iterations_performed > 0) {
                emit frameReady(moleculeToFrame(
                    result.final_molecule, m_initialAtoms.size(),
                    result.final_energy, 0.0, result.iterations_performed));
            }
        } catch (const std::exception& e) {
            emit errorOccurred(tr("Single-step optimization threw: %1").arg(QString::fromUtf8(e.what())));
        }
        emit finished();
        return;
    }
    }
}

// Copy the currently held mouse-grab force out from under the mutex so the
// worker can hand it to curcuma without holding the lock across the call.
// "Sticky": unlike a drain, this does NOT clear the force — the same grab keeps
// biasing every step until clearInjectedForce() (mouse release) drops it. This
// is what makes the force act for as long as the button is held, instead of
// only on the step that coincides with a mouse-move event.
Eigen::MatrixXd SimulationWorker::currentInjectedForces(int atomCount)
{
    QMutexLocker lock(&m_forceMutex);
    if (!m_pendingForcesValid || m_pendingForces.rows() != atomCount)
        return Eigen::MatrixXd();
    return m_pendingForces;  // copy; stays valid until clearInjectedForce()
}

// Claude Generated 2026 - Convert the (N_atoms × 3) force matrix produced by
// forceinjector::distributeForce (column-major Eigen) into a flat atom-major
// Vector the optimizer can subtract from its gradient. Returns an empty
// vector when there are no pending forces for this molecule size.
static Vector pendingForcesToFlatVector(const Eigen::MatrixXd& pending)
{
    if (pending.rows() == 0)
        return Vector();
    const Eigen::Index n = pending.rows();
    Vector flat(3 * n);
    for (Eigen::Index i = 0; i < n; ++i) {
        flat[3 * i + 0] = pending(i, 0);
        flat[3 * i + 1] = pending(i, 1);
        flat[3 * i + 2] = pending(i, 2);
    }
    return flat;
}

void SimulationWorker::run()
{
    if (m_initialAtoms.isEmpty()) {
        emit errorOccurred(tr("No molecule loaded. Please open a molecule file first."));
        emit finished();
        return;
    }

    m_stopRequested.storeRelaxed(0);
    m_pauseRequested.storeRelaxed(0);
    m_lastEmitTimer.start();

    switch (m_config.mode) {
    case SimulationConfig::Mode::MolecularDynamics:
        // Async: startMD() creates the QTimer and returns. The worker thread's event loop
        // then drives performMDStep() at the target cadence; finished() is emitted later
        // from finalizeMDRun() when the user stops or md.step() signals end-of-run.
        startMD();
        break;
    case SimulationConfig::Mode::GeometryOptimization:
        // Synchronous: Optimizer::Optimize() runs its own step loop via the callback.
        runOptimization();
        emit finished();
        break;
    }
}

static Molecule atomsToMolecule(const QVector<MoleculeViewer::Atom>& atoms)
{
    Molecule mol;
    for (const auto& atom : atoms) {
        int Z = Elements::String2Element(atom.element.toStdString());
        Position pos(atom.position.x(), atom.position.y(), atom.position.z());
        mol.addPair({ Z, pos });
    }
    return mol;
}

static SimulationFramePtr moleculeToFrame(
    const Molecule& mol, int referenceSize, double energy, double ekin, int step)
{
    auto frame = QSharedPointer<SimulationFrame>::create();
    frame->energy = energy;
    frame->ekin = ekin;
    frame->step = step;

    Geometry geo = mol.getGeometry();
    const int n = std::min(static_cast<int>(geo.rows()), referenceSize);
    frame->positions.reserve(n);
    for (int i = 0; i < n; ++i) {
        frame->positions.emplace_back(
            static_cast<float>(geo(i, 0)),
            static_cast<float>(geo(i, 1)),
            static_cast<float>(geo(i, 2)));
    }
    return frame;
}

void SimulationWorker::startMD()
{
    // dump_frequency=1 is required: SimpleMD::step() only refreshes m_molecule's
    // geometry inside `if (m_step % m_dump == 0)` blocks. With the default (50)
    // currentMolecule() returns stale positions 49 out of 50 steps, producing
    // the "99% frames dropped" appearance. The interactive viewer needs every
    // step's geometry; the per-step overhead is negligible vs. the MD step itself.
    json simplemd_params;
    simplemd_params["method"] = m_config.method.toStdString();
    simplemd_params["temperature"] = m_config.temperature;
    simplemd_params["time_step"] = m_config.timestep;
    simplemd_params["dump_frequency"] = 1;
    if (m_config.steps > 0)
        simplemd_params["max_time"] = static_cast<double>(m_config.steps) * m_config.timestep;
    else
        simplemd_params["max_time"] = 0.0;
    if (m_config.performanceAnalysis)
        simplemd_params["print_frequency"] = 1;
    simplemd_params["write_xyz"] = m_config.writeTrajectory;
    simplemd_params["no_restart"] = true;
    simplemd_params["no_center"] = true;
    simplemd_params["rattle"] = m_config.rattleMode;
    simplemd_params["rattle_12"] = m_config.rattle12;
    simplemd_params["rattle_13"] = m_config.rattle13;
    simplemd_params["rattle_tol_12"] = m_config.rattleTol12;
    simplemd_params["rattle_tol_13"] = m_config.rattleTol13;
    simplemd_params["rattle_max_iterations"] = m_config.rattleMaxIter;
    simplemd_params["hmass"] = m_config.hmass;
    applyRmsdMtdParams(m_config, simplemd_params);

    json controller;
    controller["simplemd"] = simplemd_params;
    controller["global"]["method"] = m_config.method.toStdString();
    controller["global"]["gpu"] = m_config.gpu.toStdString();
    controller["global"]["verbosity"] = 0;
    controller["verbosity"] = 0;

    // GFN-FF topology mode: "auto" (two-tier caching) or "constant" (never recalculate)
    // Only applies when method is gfnff, ignored otherwise
    if (m_config.method == "gfnff") {
        controller["global"]["topology_mode"] = m_config.topologyMode.toStdString();
    }

    m_md = std::make_unique<SimpleMD>(controller, true);
    m_md->setMolecule(atomsToMolecule(m_initialAtoms));

    if (!m_md->Initialise()) {
        emit errorOccurred(tr("MD initialization failed. Method '%1' may not be available.")
                               .arg(m_config.method));
        m_md.reset();
        emit finished();
        return;
    }
    m_md->prepareRun();

    // Reset perf-stat accumulators for this run
    m_mdFrameCount = 0;
    m_mdTotalStepTime = 0;
    m_mdMinStepTime = std::numeric_limits<qint64>::max();
    m_mdMaxStepTime = 0;
    m_mdPerfTimer.start();

    // QTimer lives in this (worker) thread because 'this' is moveToThread'd before run().
    // PreciseTimer gives ms-level accuracy on Linux (default is 1 ms coarse).
    int effectiveFps = m_config.fpsLimit > 0 ? m_config.fpsLimit : 60;
    m_mdTimer = new QTimer(this);
    m_mdTimer->setTimerType(Qt::PreciseTimer);
    m_mdTimer->setInterval(1000 / effectiveFps);
    connect(m_mdTimer, &QTimer::timeout, this, &SimulationWorker::performMDStep);
    m_mdTimer->start();
}

void SimulationWorker::performMDStep()
{
    if (!m_md) return;

    if (m_stopRequested.loadRelaxed()) {
        finalizeMDRun();
        return;
    }
    if (m_pauseRequested.loadRelaxed()) {
        return;  // skip this tick; timer keeps firing, observes resume automatically
    }

    QElapsedTimer stepClock;
    stepClock.start();

    // Apply the currently held mouse-grab force (sticky: re-applied every step
    // for as long as the user holds the grab, not only on mouse-move steps).
    // Converted from MatrixXd (col-major) to Geometry (row-major) on assign.
    Eigen::MatrixXd pending = currentInjectedForces(m_initialAtoms.size());
    if (pending.rows() > 0) {
        Geometry ext = pending;
        m_md->applyExternalForces(ext);
    }

    if (!m_md->step()) {
        finalizeMDRun();
        return;
    }

    SimulationFramePtr frame = moleculeToFrame(
        m_md->currentMolecule(), m_initialAtoms.size(),
        m_md->potentialEnergy(), m_md->kineticEnergy(), m_md->stepCount());

    emit frameReady(frame);

    if (m_config.performanceAnalysis) {
        qint64 stepTime = stepClock.elapsed();
        m_mdTotalStepTime += stepTime;
        if (stepTime < m_mdMinStepTime) m_mdMinStepTime = stepTime;
        if (stepTime > m_mdMaxStepTime) m_mdMaxStepTime = stepTime;
        ++m_mdFrameCount;

        if (m_mdFrameCount >= m_config.performanceInterval) {
            qint64 avgStep = m_mdTotalStepTime / m_mdFrameCount;
            double fps = 1000.0 * m_mdFrameCount / std::max<qint64>(1, m_mdPerfTimer.elapsed());
            qDebug() << "=== Performance [last" << m_mdFrameCount << "frames @ step" << m_md->stepCount() << "] ==="
                     << "atoms:" << static_cast<int>(frame->positions.size())
                     << "avg_step:" << avgStep << "ms"
                     << "min:" << m_mdMinStepTime << "max:" << m_mdMaxStepTime
                     << "fps:" << fps;
            m_mdFrameCount = 0;
            m_mdTotalStepTime = 0;
            m_mdMinStepTime = std::numeric_limits<qint64>::max();
            m_mdMaxStepTime = 0;
            m_mdPerfTimer.restart();
        }
    }
}

void SimulationWorker::finalizeMDRun()
{
    if (m_mdTimer) {
        m_mdTimer->stop();
        m_mdTimer->deleteLater();
        m_mdTimer = nullptr;
    }
    if (m_md) {
        m_md->finalizeRun();
        m_md.reset();
    }
    emit finished();
}

void SimulationWorker::runOptimization()
{
    json opt_config;
    opt_config["max_iterations"] = m_config.steps;
    opt_config["gradient_threshold"] = m_config.convergence;
    opt_config["write_trajectory"] = m_config.writeTrajectory;
    opt_config["verbosity"] = 0;
    // Claude Generated 2026 - Interactive grab intentionally pulls atoms away from
    // the minimum, which RAISES the true energy. The driver's default energy-rise
    // guard (100 kJ/mol) then aborts with an EMPTY failed_result, discarding the
    // whole step — so the grabbed geometry snapped back every keep-alive cycle.
    // Disable the guard here; the grab force is bounded and the objective still
    // rejects NaN coordinates, so the optimisation cannot diverge silently.
    opt_config["max_energy_rise"] = 1.0e12;

    json energy_controller;
    energy_controller["method"] = m_config.method.toStdString();
    energy_controller["gpu"] = m_config.gpu.toStdString();
    energy_controller["verbosity"] = 0;

    Molecule mol = atomsToMolecule(m_initialAtoms);

    // Emit starting geometry so the viewer reflects the pre-opt state.
    emit frameReady(moleculeToFrame(mol, m_initialAtoms.size(), 0.0, 0.0, 0));

    try {
        EnergyCalculator calc(m_config.method.toStdString(), energy_controller);
        // Note: do NOT call calc.setMolecule() here — OptimizerDriver::InitializeOptimization
        // performs the initialization and a double-init crashes GFN-FF.

        Optimization::OptimizerType opt_type =
            Optimization::parseOptimizerType(m_config.optimizer.toStdString());

        // Claude Generated 2026 - Keep-alive loop (interactive Opt). A single
        // Optimize() returns once it converges (or its line search stalls under a
        // hard grab), leaving the mouse grab with no running optimization to push
        // against. So we loop Optimize() and continue from the latest geometry,
        // exiting only on Stop. Two crashes shaped this design:
        //   1. Re-running Optimize() on a spent LBFGSpp solver re-enters dead
        //      single-step state (SIGSEGV) — so each cycle re-initialises the
        //      solver (recreates it) before optimising again.
        //   2. Rebuilding the force field (GFN-FF setMolecule) from a heavily
        //      grab-distorted geometry crashes. With m_config.optKeepParameters
        //      (default ON) we keep the FF parameters/topology fixed and only move
        //      atoms (ReinitializeKeepCalculator → updateGeometry). The FF is built
        //      ONCE up front from the original (undistorted) geometry.
        // The optimizer object + callback are created once; only the solver state
        // is reset per cycle.
        Molecule current = mol;          // geometry carried across cycles
        Molecule lastSeen = current;     // latest accepted geometry from the callback
        // [GRAB-DEBUG] temporary: max per-coordinate displacement between two geometries.
        auto maxDisp = [](const Molecule& a, const Molecule& b) -> double {
            Geometry ga = a.getGeometry();
            Geometry gb = b.getGeometry();
            if (ga.rows() != gb.rows() || ga.rows() == 0)
                return -1.0;
            return (ga - gb).cwiseAbs().maxCoeff();
        };

        auto optimizer = Optimization::OptimizerFactory::createOptimizer(opt_type, &calc);
        if (!optimizer) {
            emit errorOccurred(tr("Failed to create optimizer '%1'").arg(m_config.optimizer));
            return;
        }

        // Merge user config on top of driver defaults so subclass settings are preserved.
        {
            json merged = optimizer->GetDefaultConfiguration();
            for (auto it = opt_config.begin(); it != opt_config.end(); ++it)
                merged[it.key()] = it.value();
            optimizer->LoadConfiguration(merged);
        }

        // Per-step callback: throttle-then-emit, same cadence model as runMD().
        // If the optimizer step itself exceeds the fps budget, every iteration emits
        // immediately (throttle is a no-op when remaining ≤ 0).
        // lastSeen captures the latest accepted geometry so we can carry it into the
        // next cycle even if Optimize() returns an empty failed_result —
        // result.final_molecule is empty on any failure path.
        m_lastEmitTimer.restart();
        optimizer->setStepCallback([this, optimizer_ptr = optimizer.get(), &lastSeen](int iter, const Molecule& mol, double energy) -> bool {
                if (m_stopRequested.loadRelaxed())
                    return false;
                while (m_pauseRequested.loadRelaxed()) {
                    if (m_stopRequested.loadRelaxed())
                        return false;
                    QThread::msleep(50);
                }
                int effectiveFps = m_config.fpsLimit > 0 ? m_config.fpsLimit : 60;
                qint64 targetMs = 1000 / effectiveFps;
                qint64 remaining = targetMs - m_lastEmitTimer.elapsed();
                while (remaining > 10 && !m_stopRequested.loadRelaxed()) {
                    QThread::msleep(static_cast<unsigned long>(std::min(remaining, qint64(50))));
                    remaining = targetMs - m_lastEmitTimer.elapsed();
                }
                Q_EMIT frameReady(moleculeToFrame(mol, m_initialAtoms.size(), energy, 0.0, iter));
                m_lastEmitTimer.restart();
                lastSeen = mol;  // remember the latest geometry for robust carry-forward

                // Claude Generated 2026 - Deliver queued cross-thread mouse-grab calls.
                // Optimize() runs synchronously on this worker thread, so the thread's
                // event loop is NOT spinning and the QueuedConnection injectForce()/
                // clearInjectedForce() slots would never run during the optimization
                // (unlike MD, which is QTimer-driven and processes events between steps).
                // Pump the worker thread's posted events here so the read below sees
                // the latest grab force. Without this, interactive Opt never reacts.
                QCoreApplication::processEvents();

                // Claude Generated 2026 - Re-apply the currently held mouse-grab force
                // every iteration so the optimizer reacts live while the user holds an
                // atom. The force is sticky (held until mouse release), so it keeps
                // biasing the gradient even when the cursor is not moving — without
                // this, a still grab would relax straight back to the minimum. When
                // the grab is released, clearInjectedForce() empties the force and we
                // drop the optimizer bias on the next iteration.
                Vector ext = pendingForcesToFlatVector(currentInjectedForces(m_initialAtoms.size()));
                if (ext.size() > 0) {
                    optimizer_ptr->setExternalForces(ext);
                    // [GRAB-DEBUG] temporary: confirm the worker reads a live grab force.
                    double maxabs = 0.0;
                    for (int k = 0; k < ext.size(); ++k)
                        maxabs = std::max(maxabs, std::abs(ext[k]));
                    qDebug().nospace() << "[GRAB] opt iter " << iter
                                       << " ext.size=" << static_cast<int>(ext.size())
                                       << " maxAbsForce=" << maxabs;
                } else {
                    optimizer_ptr->clearExternalForces();
                }

                return true;
            });

        // First-time full init: builds the force field ONCE from the original,
        // undistorted geometry (current == mol here). Subsequent cycles reuse it.
        if (!optimizer->InitializeOptimization(current)) {
            emit errorOccurred(tr("Optimizer initialization failed."));
            return;
        }

        int cycle = 0;
        while (!m_stopRequested.loadRelaxed()) {
            // Restart the optimiser for this cycle from the latest geometry.
            // optKeepParameters (default ON): keep the force-field parameters and
            // only move atoms (no GFN-FF rebuild from the distorted geometry).
            // Off: rebuild the FF each cycle (adaptive topology, slower, can crash
            // on large distortions). Cycle 0 already did the full init above.
            if (cycle > 0) {
                const bool ok = m_config.optKeepParameters
                    ? optimizer->ReinitializeKeepCalculator(current)
                    : optimizer->InitializeOptimization(current);
                if (!ok) {
                    emit errorOccurred(tr("Optimizer re-initialization failed."));
                    return;
                }
            }
            // [GRAB-DEBUG] temporary: does the displaced geometry carry into this cycle?
            qDebug().nospace() << "[GRAB] cycle " << cycle
                               << " START dispFromOrig=" << maxDisp(current, mol);

            // Claude Generated 2026 - Seed iteration 1 with the force held right now
            // (mouse-grab in Opt mode); the callback above keeps it refreshed. Always
            // set or clear so a release immediately drops the bias even when no force
            // is held.
            Vector ext = pendingForcesToFlatVector(currentInjectedForces(m_initialAtoms.size()));
            if (ext.size() > 0)
                optimizer->setExternalForces(ext);
            else
                optimizer->clearExternalForces();

            Optimization::OptimizationResult result = optimizer->Optimize(m_config.writeTrajectory, 0);
            optimizer->clearExternalForces();

            // Carry the geometry we just reached into the next cycle so the grab's
            // displacement (and any optimisation progress) is not lost on restart.
            // Prefer the callback's last-seen geometry (always populated, even when
            // Optimize() returns an empty failed_result), fall back to final_molecule.
            const bool carried = lastSeen.AtomCount() == static_cast<std::size_t>(m_initialAtoms.size());
            if (carried)
                current = lastSeen;
            else if (result.final_molecule.AtomCount() == static_cast<std::size_t>(m_initialAtoms.size()))
                current = result.final_molecule;

            // [GRAB-DEBUG] temporary: did this cycle move the geometry, and did it carry?
            qDebug().nospace() << "[GRAB] cycle " << cycle
                               << " END success=" << result.success
                               << " iters=" << result.iterations_performed
                               << " carried=" << carried
                               << " dispFromOrig=" << maxDisp(current, mol);

            emit frameReady(moleculeToFrame(current, m_initialAtoms.size(),
                result.final_energy, 0.0, result.iterations_performed));

            // Anti-spin: when it converged in ~0 iterations (idle at the minimum,
            // no grab), throttle the restart to the FPS budget so we don't busy
            // re-evaluate the energy. A held grab does many iterations → no sleep.
            if (result.iterations_performed < 2 && !m_stopRequested.loadRelaxed()) {
                int fps = m_config.fpsLimit > 0 ? m_config.fpsLimit : 60;
                QThread::msleep(static_cast<unsigned long>(1000 / fps));
            }
            if ((cycle++ % 50) == 0)
                qDebug().nospace() << "[GRAB] opt keep-alive cycle " << cycle
                                   << " lastIters=" << result.iterations_performed;
        }
    } catch (const std::exception& e) {
        emit errorOccurred(tr("Optimization threw: %1").arg(QString::fromUtf8(e.what())));
    }
}
