// simulationworker.cpp - QThread wrapper for curcuma MD and geometry optimization
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration (stepwise API)

#include "simulationworker.h"

#include "external/json.hpp"
using json = nlohmann::json;

#include <src/capabilities/simplemd.h>
#include <src/capabilities/optimizer_factory.h>
#include <src/capabilities/optimizer_interface.h>
#include <src/core/energycalculator.h>
#include <src/core/molecule.h>
#include <src/core/elements.h>

#include <QMutexLocker>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <limits>

SimulationWorker::SimulationWorker(QObject* parent)
    : QObject(parent)
{
    static const int kPtrTypeId = qRegisterMetaType<SimulationFramePtr>("SimulationFramePtr");
    Q_UNUSED(kPtrTypeId);
}

// Defined here (not =default in header) so unique_ptr<SimpleMD>'s deleter sees the complete type.
SimulationWorker::~SimulationWorker() = default;

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
    Eigen::Vector3d f(force.x(), force.y(), force.z());
    Eigen::MatrixXd distributed = forceinjector::distributeForce(
        atomIndex, f, m_adjacency, alpha, maxShells, m_initialAtoms.size());

    QMutexLocker lock(&m_forceMutex);
    m_pendingForces = distributed;
    m_pendingForcesValid = true;
}

void SimulationWorker::clearInjectedForce()
{
    QMutexLocker lock(&m_forceMutex);
    m_pendingForcesValid = false;
    m_pendingForces.resize(0, 0);
}

// Move the pending force matrix out from under the mutex so the worker can
// hand it to SimpleMD without holding the lock across curcuma calls.
Eigen::MatrixXd SimulationWorker::drainPendingForces(int atomCount)
{
    QMutexLocker lock(&m_forceMutex);
    if (!m_pendingForcesValid || m_pendingForces.rows() != atomCount)
        return Eigen::MatrixXd();
    Eigen::MatrixXd out = std::move(m_pendingForces);
    m_pendingForces.resize(0, 0);
    m_pendingForcesValid = false;
    return out;
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

    // Apply any force the GUI queued since the previous step (mouse grab).
    // Converted from MatrixXd (col-major) to Geometry (row-major) on assign.
    Eigen::MatrixXd pending = drainPendingForces(m_initialAtoms.size());
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

        auto optimizer = Optimization::OptimizerFactory::createOptimizer(opt_type, &calc);
        if (!optimizer) {
            emit errorOccurred(tr("Failed to create optimizer '%1'").arg(m_config.optimizer));
            return;
        }

        // Merge user config on top of driver defaults so subclass settings are preserved.
        json merged = optimizer->GetDefaultConfiguration();
        for (auto it = opt_config.begin(); it != opt_config.end(); ++it)
            merged[it.key()] = it.value();
        optimizer->LoadConfiguration(merged);

        // Per-step callback: throttle-then-emit, same cadence model as runMD().
        // If the optimizer step itself exceeds the fps budget, every iteration emits
        // immediately (throttle is a no-op when remaining ≤ 0).
        m_lastEmitTimer.restart();
        optimizer->setStepCallback([this](int iter, const Molecule& mol, double energy) -> bool {
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
            emit frameReady(moleculeToFrame(mol, m_initialAtoms.size(), energy, 0.0, iter));
            m_lastEmitTimer.restart();
            return true;
        });

        if (!optimizer->InitializeOptimization(mol)) {
            emit errorOccurred(tr("Optimizer initialization failed."));
            return;
        }

        Optimization::OptimizationResult result = optimizer->Optimize(m_config.writeTrajectory, 0);

        if (!result.success && !m_stopRequested.loadRelaxed()) {
            emit errorOccurred(tr("Optimization failed: %1")
                                   .arg(QString::fromStdString(result.error_message)));
            return;
        }

        if (result.success)
            emit frameReady(moleculeToFrame(result.final_molecule, m_initialAtoms.size(),
                result.final_energy, 0.0, result.iterations_performed));
    } catch (const std::exception& e) {
        emit errorOccurred(tr("Optimization threw: %1").arg(QString::fromUtf8(e.what())));
    }
}
