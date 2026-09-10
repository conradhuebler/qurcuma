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
#include <src/core/energy_calculators/ff_methods/gfnff.h>
#include <src/core/energy_calculators/ff_methods/gfnff_parameters.h>
#include <src/core/energy_calculators/qm_methods/gfnff_method.h>
#include <src/core/units.h>
#include <src/core/elements.h>

#include <QCoreApplication>
#include <QFile>
#include <QDir>
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

// Claude Generated 2026 - Write curcuma's harmonic confinement-wall parameters
// into the simplemd controller block. Only emitted when walls are enabled, so
// curcuma's defaults (wall_type=none) stay intact otherwise. Mirrors the "Walls"
// PARAM category in external/curcuma/src/capabilities/simplemd.h. Rectangular
// bounds are always forwarded (curcuma auto-sizes when min==max==0); radius is
// spheric-only but harmless to set in the rect case (curcuma reads wall_radius
// only when wall_type=spheric).
void applyWallParams(const SimulationConfig& cfg, json& simplemd_params)
{
    if (!cfg.wallEnabled)
        return;
    simplemd_params["wall_type"] = cfg.wallType == 2 ? "rect"
                              : cfg.wallType == 1 ? "spheric" : "none";
    simplemd_params["wall_potential"] = cfg.wallPotential == 2 ? "pbc"
                                      : cfg.wallPotential == 1 ? "logfermi" : "harmonic";
    simplemd_params["wall_temp"] = cfg.wallTemp;
    simplemd_params["wall_beta"] = cfg.wallBeta;
    simplemd_params["wall_x_min"] = cfg.wallXmin;
    simplemd_params["wall_x_max"] = cfg.wallXmax;
    simplemd_params["wall_y_min"] = cfg.wallYmin;
    simplemd_params["wall_y_max"] = cfg.wallYmax;
    simplemd_params["wall_z_min"] = cfg.wallZmin;
    simplemd_params["wall_z_max"] = cfg.wallZmax;
    simplemd_params["wall_radius"] = cfg.wallRadius;
}

// Claude Generated 2026 - Write curcuma's temperature ramp + region parameters into the
// simplemd controller block. The global ramp (temp_ramp/temp_schedule) and the per-atom-subset
// regions (temp_regions array) are independent; either, both, or neither may be present. Mirrors
// the "Temperature Ramp" PARAM category + the temp_regions array in
// external/curcuma/src/capabilities/simplemd.h.
void applyTempRampParams(const SimulationConfig& cfg, json& simplemd_params)
{
    if (cfg.tempRamp && !cfg.tempSchedule.trimmed().isEmpty()) {
        simplemd_params["temp_ramp"] = true;
        simplemd_params["temp_schedule"] = cfg.tempSchedule.trimmed().toStdString();
    }
    if (!cfg.tempRegions.isEmpty()) {
        json regions = json::array();
        for (const TempRegion& r : cfg.tempRegions) {
            if (r.atoms.trimmed().isEmpty())
                continue;
            json reg;
            reg["atoms"] = r.atoms.trimmed().toStdString();
            reg["temperature"] = r.temperature;
            if (!r.schedule.trimmed().isEmpty())
                reg["temp_schedule"] = r.schedule.trimmed().toStdString();
            regions.push_back(reg);
        }
        if (!regions.empty())
            simplemd_params["temp_regions"] = regions;
    }
}

// Claude Generated 2026 - Single source of truth for the SimpleMD controller
// block. Both startMD (continuous run) and stepOnce (single "Step" click) build
// their controller here, so the two paths can no longer silently diverge — a
// whole class of bug where the single-step path forgot thermostat/RATTLE/wall/
// ramp params (see the earlier stepOnce config-drop fix). @p singleStep captures
// the ONLY intended differences: run length (one step vs the configured count)
// and no trajectory file for a single click. dump_frequency=1 is required so
// SimpleMD::step() refreshes the molecule geometry every step (the interactive
// viewer needs each frame; the default 50 returns stale positions 49/50 steps).
json buildSimplemdParams(const SimulationConfig& cfg, bool singleStep)
{
    json p;
    p["method"] = cfg.method.toStdString();
    p["temperature"] = cfg.temperature;
    p["time_step"] = cfg.timestep;
    p["dump_frequency"] = 1;
    p["max_time"] = singleStep
        ? cfg.timestep
        : (cfg.steps > 0 ? static_cast<double>(cfg.steps) * cfg.timestep : 0.0);
    if (cfg.performanceAnalysis)
        p["print_frequency"] = 1;
    else if (singleStep)
        p["print_frequency"] = 1000;
    p["write_xyz"] = singleStep ? false : cfg.writeTrajectory;
    p["no_restart"] = true;
    p["no_center"] = true;
    p["rattle"] = cfg.rattleMode;
    p["rattle_12"] = cfg.rattle12;
    p["rattle_13"] = cfg.rattle13;
    p["rattle_tol_12"] = cfg.rattleTol12;
    p["rattle_tol_13"] = cfg.rattleTol13;
    p["rattle_max_iterations"] = cfg.rattleMaxIter;
    p["hmass"] = cfg.hmass;
    p["thermostat"] = cfg.thermostat.toStdString();
    p["coupling"] = cfg.thermostatCoupling;
    p["andersen_probability"] = cfg.andersenProbability;
    p["chain_length"] = cfg.noseChainLength;
    applyRmsdMtdParams(cfg, p);
    applyWallParams(cfg, p);
    applyTempRampParams(cfg, p);
    return p;
}

// Claude Generated 2026 - Wrap the simplemd params in the full curcuma controller
// (global method/gpu/verbosity + optional GFN-FF topology mode). Shared by startMD
// and stepOnce so both build an identical controller shape.
json buildMdController(const SimulationConfig& cfg, bool singleStep)
{
    json controller;
    controller["simplemd"] = buildSimplemdParams(cfg, singleStep);
    controller["global"]["method"] = cfg.method.toStdString();
    controller["global"]["gpu"] = cfg.gpu.toStdString();
    controller["global"]["verbosity"] = 0;
    controller["verbosity"] = 0;
    // GFN-FF topology mode (auto/constant/react); ignored for other methods.
    if (cfg.method == "gfnff") {
        controller["global"]["topology_mode"] = cfg.topologyMode.toStdString();
        // Reactive parameters travel in the gfnff scope (same route as
        // hb_update_force_every below) and only when the mode is actually react,
        // so a non-reactive run keeps curcuma's defaults untouched.
        if (cfg.topologyMode == QLatin1String("react")) {
            json& g = controller["gfnff"];
            g["react_bond_form_factor"] = cfg.reactFormFactor;
            g["react_bond_break_factor"] = cfg.reactBreakFactor;
            g["react_check_every"] = cfg.reactCheckEvery;
            g["react_refractory_scans"] = cfg.reactRefractoryScans;
            g["react_valence_cap"] = cfg.reactValenceCap;
            g["react_exchange_scans"] = cfg.reactExchangeScans;
        }
    }
    return controller;
}

// Claude Generated 2026 - Energy-calculator controller shared by the optimizer
// paths (runOptimization + single-step stepOnce).
json buildEnergyController(const SimulationConfig& cfg)
{
    json c;
    c["method"] = cfg.method.toStdString();
    c["gpu"] = cfg.gpu.toStdString();
    c["verbosity"] = 0;
    return c;
}

// Claude Generated 2026 - Optimizer config shared by runOptimization (continuous
// keep-alive loop) and stepOnce (single iteration). @p singleStep selects one
// iteration + single_step_mode and no trajectory file. max_energy_rise is relaxed
// in both so a bounded mouse-grab (which raises the energy) is not discarded.
json buildOptConfig(const SimulationConfig& cfg, bool singleStep)
{
    json c;
    c["max_iterations"] = singleStep ? 1 : cfg.steps;
    c["gradient_threshold"] = cfg.convergence;
    // Set explicitly rather than left to the engine's fallback: an optimisation
    // that has stopped changing energy should say so through a criterion the
    // caller can see and adjust, not through one nobody wrote down.
    c["energy_threshold"] = cfg.energyConvergence;
    c["write_trajectory"] = singleStep ? false : cfg.writeTrajectory;
    c["verbosity"] = 0;
    c["max_energy_rise"] = 1.0e12;
    if (singleStep)
        c["single_step_mode"] = true;  // break after one iteration
    return c;
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
static Molecule atomsToMolecule(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>* bonds = nullptr);
static SimulationFramePtr moleculeToFrame(
    const Molecule& mol, int referenceSize, double energy, double ekin, int step,
    double temperature = 0.0, double targetTemperature = 0.0);
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

void SimulationWorker::injectForces(QVector<int> atoms, QVector<QVector3D> forces,
    double alpha, int maxShells)
{
    if (m_initialAtoms.isEmpty() || atoms.size() != forces.size())
        return;
    if (atoms.isEmpty()) {
        clearInjectedForce();
        return;
    }

    // Each pull is distributed through the bond graph on its own and the results
    // are summed: the atoms may be in the same molecule, and a shell that two
    // pulls both reach should feel both.
    Eigen::MatrixXd total = Eigen::MatrixXd::Zero(m_initialAtoms.size(), 3);
    for (int i = 0; i < atoms.size(); ++i) {
        const int index = atoms.at(i);
        if (index < 0 || index >= m_initialAtoms.size())
            continue;
        const QVector3D& f = forces.at(i);
        total += forceinjector::distributeForce(index,
            Eigen::Vector3d(f.x(), f.y(), f.z()),
            m_adjacency, alpha, maxShells, m_initialAtoms.size());
    }

    QMutexLocker lock(&m_forceMutex);
    m_pendingForces = total;
    m_pendingForcesValid = true;
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

// Claude Generated 2026 - Live global temperature setpoint. Stored under a mutex; the value is
// pushed into the running SimpleMD in performMDStep() before the next step (mirrors the sticky
// mouse-grab force path). SimpleMD::setTargetTemperature() cancels any active global ramp.
void SimulationWorker::setTargetTemperature(double temperature)
{
    QMutexLocker lock(&m_tempMutex);
    m_pendingTemperature = temperature;
    m_pendingTemperatureValid = true;
}

// Claude Generated 2026 - Live wall potential parameters. Same mutex-buffered pattern as
// setTargetTemperature: stored here, pushed into SimpleMD in performMDStep().
void SimulationWorker::setWallTemp(double T)
{
    QMutexLocker lock(&m_wallParamMutex);
    m_pendingWallTemp = T;
    m_pendingWallTempValid = true;
}

void SimulationWorker::setWallBeta(double beta)
{
    QMutexLocker lock(&m_wallParamMutex);
    m_pendingWallBeta = beta;
    m_pendingWallBetaValid = true;
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
        // Single-step MD uses the SAME controller builder as startMD (singleStep =
        // one step + no trajectory file), so the two paths cannot diverge again.
        json controller = buildMdController(m_config, /*singleStep=*/true);

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
                md->potentialEnergy(), md->kineticEnergy(), md->stepCount(),
                md->currentTemperature(), md->targetTemperature()));
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
        // Single iteration; shares the optimizer/energy config builders with
        // runOptimization (singleStep = one iteration + single_step_mode).
        json opt_config = buildOptConfig(m_config, /*singleStep=*/true);
        json energy_controller = buildEnergyController(m_config);

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

    // Claude Generated 2026 - Remove a stale curcuma "stop" file from the
    // working directory before starting. curcuma's SimpleMD::step() aborts the
    // run immediately when CheckStop() sees a file named "stop" in the CWD
    // (CurcumaMethod::CheckStop, curcumamethod.cpp). That file is the CLI
    // interrupt mechanism for standalone curcuma; it is NOT used by qurcuma's
    // interactive driver, which terminates via requestStop() instead. A "stop"
    // file left over from a prior curcuma CLI run in the same directory would
    // otherwise abort every interactive MD/Opt at step 0 (step() returns false
    // on its first call, m_run_prepared stays true, so finalizeRun prints the
    // initial frame + "Exchange with heat bath" with zero integration steps —
    // the "MD doesn't start" symptom). Deleting it here gives each interactive
    // run a clean stop-file state. A "stop" file created DURING the run is still
    // honoured by curcuma (we only clear a pre-existing one).
    QFile::remove(QDir::currentPath() + QStringLiteral("/stop"));
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
        emit finished(m_stopRequested.loadRelaxed()
                ? tr("stopped by the user")
                : tr("optimization ended (converged or iteration limit reached)"));
        break;
    }
}

// Claude Generated 2026 - @p bonds seeds curcuma's topology (reactive runs only, see
// the call sites): setTopologyMatrix fills Molecule::m_bonds, getMolInfo() carries it
// into Mol::m_bonds, and GFN-FF adopts it as its forced-bond list. Deliberately NOT
// done in auto/constant mode: forced bonds are honoured in every topology mode and
// would silently replace GFN-FF's own detection for a loaded structure. In react mode
// the drawn topology IS the intended starting point, and the hysteresis owns it from
// the first scan on.
static Molecule atomsToMolecule(const QVector<MoleculeViewer::Atom>& atoms,
    const QVector<MoleculeViewer::Bond>* bonds)
{
    Molecule mol;
    for (const auto& atom : atoms) {
        int Z = Elements::String2Element(atom.element.toStdString());
        Position pos(atom.position.x(), atom.position.y(), atom.position.z());
        mol.addPair({ Z, pos });
    }
    if (bonds && !bonds->isEmpty()) {
        const int n = atoms.size();
        Eigen::MatrixXd topology = Eigen::MatrixXd::Zero(n, n);
        for (const MoleculeViewer::Bond& b : *bonds) {
            if (b.atom1 >= 0 && b.atom2 >= 0 && b.atom1 < n && b.atom2 < n && b.atom1 != b.atom2) {
                topology(b.atom1, b.atom2) = 1.0;
                topology(b.atom2, b.atom1) = 1.0;
            }
        }
        mol.setTopologyMatrix(topology);
    }
    return mol;
}

// Claude Generated 2026 - true when the drawn bond list should seed curcuma.
static bool seedsTopology(const SimulationConfig& cfg)
{
    return cfg.method == QLatin1String("gfnff") && cfg.topologyMode == QLatin1String("react");
}

static SimulationFramePtr moleculeToFrame(
    const Molecule& mol, int referenceSize, double energy, double ekin, int step,
    double temperature, double targetTemperature)
{
    auto frame = QSharedPointer<SimulationFrame>::create();
    frame->energy = energy;
    frame->ekin = ekin;
    frame->step = step;
    frame->temperature = temperature;
    frame->targetTemperature = targetTemperature;

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
    // Continuous run: build the controller from the shared source of truth (see
    // buildSimplemdParams). singleStep=false → full run length + trajectory file
    // per the user's writeTrajectory setting.
    json controller = buildMdController(m_config, /*singleStep=*/false);

    // Claude Generated 2026 - Live interaction overlay: GFN-FF rebuilds its HB/XB
    // lists only when its RMSD trigger fires, so the lists would visibly lag the
    // geometry. Forcing a rebuild on every gradient step keeps the drawn contacts
    // in step with what is on screen; that cost is why the option is opt-in.
    if (m_liveNci && m_config.method == QLatin1String("gfnff"))
        controller["gfnff"]["hb_update_force_every"] = 1;

    m_md = std::make_unique<SimpleMD>(controller, true);
    m_md->setMolecule(atomsToMolecule(m_initialAtoms, seedsTopology(m_config) ? &m_bonds : nullptr));

    if (!m_md->Initialise()) {
        emit errorOccurred(tr("MD initialization failed. Method '%1' may not be available.")
                               .arg(m_config.method));
        m_md.reset();
        emit finished();
        return;
    }
    m_md->prepareRun();

    // Reactive GFN-FF: the force field owns the bond topology, so its list (not a
    // geometric re-guess) is what the viewer should draw. Claude Generated 2026.
    m_reactLive = (m_config.method == QLatin1String("gfnff")
        && m_config.topologyMode == QLatin1String("react") && liveGfnff() != nullptr);
    m_lastTopologyVersion = -1;
    m_lastFfBonds.clear();

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

// Claude Generated 2026 - The GFN-FF instance behind the running MD, whichever
// wrapper drives it (CPU, CUDA or ROCm). Everything that reads live force-field
// state goes through here; nullptr for any other method.
GFNFF* SimulationWorker::liveGfnff() const
{
    if (!m_md)
        return nullptr;
    EnergyCalculator* calc = m_md->energyCalculator();
    if (!calc)
        return nullptr;
    ComputationalMethod* method = calc->Interface();
    // One virtual call instead of a cast per backend: the CUDA and ROCm wrappers
    // live in dlopen plugins and derive from an extern-template base, so an inline
    // accessor on the concrete class is not resolvable here — only the vtable is.
    // Returns nullptr for every method that is not GFN-FF. Claude Generated 2026.
    return method ? method->gfnffInstance() : nullptr;
}

// Claude Generated 2026 - Live non-covalent contacts straight out of the running
// force field. GFN-FF keeps its own hydrogen- and halogen-bond lists (the very
// terms it evaluates), so this shows the interactions the simulation actually
// feels, not a geometric re-derivation. Only the atom triples are taken; the
// distances and angles are re-fitted on the GUI side against the drawn frame.
void SimulationWorker::collectLiveNci(SimulationFrame& frame) const
{
    GFNFF* ff = liveGfnff();
    if (!ff)
        return;   // any method other than GFN-FF has no such list

    for (const GFNFFHydrogenBond& hb : ff->getLastHBonds()) {
        nci::Contact c;
        c.kind = nci::Kind::HydrogenBond;
        c.donor = hb.i;
        c.bridge = hb.j;
        c.acceptor = hb.k;
        c.motif = hb.case_type;
        frame.nciContacts.append(c);
    }
    for (const GFNFFHalogenBond& xb : ff->getLastXBonds()) {
        nci::Contact c;
        c.kind = nci::Kind::HalogenBond;
        c.donor = xb.i;
        c.bridge = xb.j;
        c.acceptor = xb.k;
        frame.nciContacts.append(c);
    }
}

// Claude Generated 2026 - Reactive GFN-FF: hand the force field's own bond list and
// its topology events to the frame. Runs in the worker thread right after step(), so
// nothing else touches the force field concurrently. The bond list is copied only
// when the force field rebuilt its topology (rare); events carry the discontinuity
// each rebuild introduced, converted to kJ/mol.
void SimulationWorker::collectReactiveTopology(SimulationFrame& frame)
{
    GFNFF* ff = liveGfnff();
    if (!ff)
        return;
    const int version = ff->reactiveRebuildCount();
    if (version != m_lastTopologyVersion) {
        const auto& bonds = ff->reactiveBonds();
        const auto& orders = ff->reactiveBondOrders();
        m_lastFfBonds.clear();
        m_lastFfBonds.reserve(bonds.size());
        for (size_t k = 0; k < bonds.size(); ++k) {
            FrameBond b;
            b.a = bonds[k].first;
            b.b = bonds[k].second;
            b.order = (k < orders.size()) ? orders[k] : 1;
            m_lastFfBonds.push_back(b);
        }
        m_lastTopologyVersion = version;
    }
    frame.bonds = m_lastFfBonds;
    frame.topologyVersion = version;

    for (const GFNFF::ReactEvent& ev : ff->consumeReactEvents()) {
        ReactEventView v;
        v.step = m_md->stepCount();
        for (const auto& p : ev.formed)
            v.formed.append(qMakePair(p.first, p.second));
        for (const auto& p : ev.broken)
            v.broken.append(qMakePair(p.first, p.second));
        v.deJumpKJmol = ev.de_jump_eh * CurcumaUnit::Energy::HARTREE_TO_KJMOL;
        frame.events.append(v);
    }
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

    // Apply a live temperature change (slider drag during the run). Pushed once and consumed;
    // SimpleMD::setTargetTemperature() also cancels any active global ramp. Claude Generated 2026.
    {
        QMutexLocker lock(&m_tempMutex);
        if (m_pendingTemperatureValid) {
            m_md->setTargetTemperature(m_pendingTemperature);
            m_pendingTemperatureValid = false;
        }
    }

    // Apply live wall parameter changes (slider drag during the run). Claude Generated 2026.
    {
        QMutexLocker lock(&m_wallParamMutex);
        if (m_pendingWallTempValid) {
            m_md->setWallTemp(m_pendingWallTemp);
            m_pendingWallTempValid = false;
        }
        if (m_pendingWallBetaValid) {
            m_md->setWallBeta(m_pendingWallBeta);
            m_pendingWallBetaValid = false;
        }
    }

    if (!m_md->step()) {
        finalizeMDRun();
        return;
    }

    SimulationFramePtr frame = moleculeToFrame(
        m_md->currentMolecule(), m_initialAtoms.size(),
        m_md->potentialEnergy(), m_md->kineticEnergy(), m_md->stepCount(),
        m_md->currentTemperature(), m_md->targetTemperature());

    // moleculeToFrame returns a shared pointer to const; the live-overlay and
    // reactive-topology fields are filled after construction, so take a writable
    // handle here (the frame has not been shared with any other thread yet).
    auto* mutableFrame = const_cast<SimulationFrame*>(frame.data());
    if (m_liveNci)
        collectLiveNci(*mutableFrame);
    if (m_reactLive)
        collectReactiveTopology(*mutableFrame);

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
    // Why the engine stopped. curcuma prints its abort messages only from
    // verbosity 1 upwards and says nothing at all when the configured time is up,
    // so without asking the engine a finished run and a run that fell apart look
    // identical here. Claude Generated 2026.
    QString reason;
    bool aborted = false;
    if (m_md) {
        const SimpleMD::StopReason code = m_md->stopReason();
        aborted = (code != SimpleMD::StopReason::MaxTime
            && code != SimpleMD::StopReason::StopFile
            && code != SimpleMD::StopReason::Running);
        if (m_stopRequested.loadRelaxed()) {
            reason = tr("stopped by the user after %1 steps").arg(m_md->stepCount());
            aborted = false;
        } else {
            reason = tr("%1 (after %2 steps)")
                         .arg(QString::fromStdString(m_md->stopReasonText()))
                         .arg(m_md->stepCount());
        }
        m_md->finalizeRun();
        m_md.reset();
    }
    emit finished(reason, aborted);
}

void SimulationWorker::runOptimization()
{
    // Continuous keep-alive optimisation; shares the config builders with the
    // single-step stepOnce path. singleStep=false → run up to m_config.steps
    // iterations and write the trajectory per the user's setting. max_energy_rise
    // is relaxed inside buildOptConfig because an interactive grab pulls atoms away
    // from the minimum (raising the energy); the default guard would otherwise
    // abort with an empty result and snap the grabbed geometry back each cycle.
    json opt_config = buildOptConfig(m_config, /*singleStep=*/false);
    json energy_controller = buildEnergyController(m_config);

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

            emit frameReady(moleculeToFrame(current, m_initialAtoms.size(),
                result.final_energy, 0.0, result.iterations_performed));

            // Claude Generated 2026 - Builder "Relax": one bounded pass, then done.
            // The keep-alive restarts below exist only for the interactive grab.
            if (m_config.optSingleShot)
                break;

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
