// simulationworker.h - QThread wrapper for curcuma MD and geometry optimization
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration

#pragma once

#include "forceinjector.h"
#include "simulationframe.h"
#include "view.h"

#include <Eigen/Dense>
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>
#include <QVector3D>

#include <limits>
#include <memory>

class QTimer;
class SimpleMD;  // Full type only in simulationworker.cpp — curcuma headers stay out of this TU.

/**
 * @brief Configuration struct for a simulation run.
 * Claude Generated - Interactive Simulation Integration
 */
/**
 * @brief One thermal region: an atom subset thermostatted to its own target temperature
 * (and optional ramp). Maps to a curcuma simplemd "temp_regions" array element.
 * Claude Generated 2026.
 */
struct TempRegion {
    QString atoms = "-1";       // selection string (FragString2Indicies grammar: "1:10,15", "F2", "-1"=all)
    double temperature = 300.0; // region start setpoint (K)
    QString schedule;           // optional ramp "T:mode:val;..." (empty = constant)
};

struct SimulationConfig {
    enum class Mode { MolecularDynamics, GeometryOptimization };

    Mode mode = Mode::MolecularDynamics;
    QString method = "gfnff";     // Energy method: gfnff / uff / gfn2 / gfn1
    QString optimizer = "auto";   // Opt algorithm: auto / lbfgspp / native_lbfgs / diis / rfo / ancopt
    double temperature = 300.0;   // K (MD only)
    double timestep = 1.0;        // fs (MD only)
    int steps = 1000;             // Total MD steps or max opt iterations
    double convergence = 1e-6;    // Gradient convergence threshold (opt only)
    // Interactive Opt: keep the force-field parameters/topology fixed across the
    // keep-alive restarts (no rebuild from grab-distorted geometry). Default ON —
    // rebuilding GFN-FF from a heavily distorted geometry is slow and can crash.
    bool optKeepParameters = true;
    bool writeTrajectory = false; // Also write .trj.xyz file to disk
    int fpsLimit = 30;            // Simulation speed in steps/sec (0 = unlimited)
    bool performanceAnalysis = false; // Per-frame timing stats every N steps
    int performanceInterval = 100;   // Output summary every N frames
    QString gpu = "none";         // GPU acceleration: "none", "cuda", "rocm", "vulkan", "auto"

    // RATTLE constraints (MD only)
    int rattleMode = 0;           // 0=off, 1=RATTLE, 2=RATTLE H-only
    bool rattle12 = true;         // Constrain 1-2 bond distances
    bool rattle13 = false;        // Constrain 1-3 angle distances
    double rattleTol12 = 1e-4;    // Tolerance for 1-2 constraints (Bohr²)
    double rattleTol13 = 1e-3;    // Tolerance for 1-3 constraints (Bohr²)
    int rattleMaxIter = 100;      // Max RATTLE iterations per step

    // GFN-FF topology mode (MD only)
    QString topologyMode = "auto"; // "auto" (two-tier caching) or "constant" (never recalculate)

    // Hydrogen mass scaling (MD only) - increases H mass to allow larger time steps
    double hmass = 1.0; // 1.0 = normal mass, 2.0 or 3.0 = scaled (common values)

    // RMSD metadynamics (MD only) - curcuma SimpleMD bias mode (rmsd_mtd=true).
    // Adds a bias potential in RMSD-to-reference space during the MD run, driving
    // exploration away from already-sampled geometries. Defaults mirror curcuma's
    // PARAM block (external/curcuma/src/capabilities/simplemd.h, "RMSD-MTD" category).
    bool   rmsdMtd              = false;   // rmsd_mtd: enable the RMSD-MTD bias
    double rmsdMtdK             = 0.01;    // rmsd_mtd_k: hill height W_i = k*counter (Eh)
    double rmsdMtdAlpha         = 10.0;    // rmsd_mtd_alpha: Gaussian width
    QString rmsdMtdAtoms        = "-1";   // rmsd_mtd_atoms: atom indices for RMSD ("-1"=all)
    QString rmsdMtdRefFile      = "none"; // rmsd_mtd_ref_file: reference structures file
    int    rmsdMtdMaxGaussians  = -1;     // rmsd_mtd_max_gaussians: cap stored bias structs (-1=unlimited)
    int    rmsdMtdMaxHeight     = 0;      // rmsd_mtd_max_height: cap per-struct counter (0=unbounded)
    double rmsdMtdEconv        = 1e8;    // rmsd_econv: bias-deposition convergence threshold (gates when a region is considered biased enough)
    int    rmsdMtdPace          = 1;      // rmsd_mtd_pace: unused in counter scheme (kept for compat)
    bool   rmsdMtdWtmtd         = false;  // wtmtd: well-tempered reporting (gates rmsdMtdDt)
    double rmsdMtdDt            = 2000.0; // rmsd_mtd_dt: well-tempered bias temp ΔT (K)
    bool   rmsdMtdFreezeInherited = false;// rmsd_mtd_freeze_inherited: freeze inherited hill heights

    // Harmonic confinement walls (MD only) - curcuma SimpleMD wall_* params
    // (external/curcuma/src/capabilities/simplemd.h "Walls" category). Manually
    // configured in the Simulation dock; visualized live in the 3D viewer.
    bool   wallEnabled  = false; // master switch (writes wall_* to the curcuma config)
    int    wallType      = 0;    // 0=none, 1=spheric, 2=rect (curcuma m_wall_type)
    bool   wallHarmonic  = true; // true=harmonic, false=logfermi (wall_potential)
    double wallXmin = 0.0, wallXmax = 0.0; // Å, rectangular bounds (curcuma auto-sizes when 0/0)
    double wallYmin = 0.0, wallYmax = 0.0;
    double wallZmin = 0.0, wallZmax = 0.0;
    double wallRadius = 0.0; // Å, spheric wall radius (origin-centred; 0 = auto-size)

    // Temperature ramp + regions (MD only) - curcuma SimpleMD temp_* params.
    // The global ramp drives the (live-adjustable) global setpoint over a multi-stage
    // schedule; regions thermostat atom subsets to their own target/ramp. Claude Generated 2026.
    bool    tempRamp = false;        // temp_ramp: enable the global multi-stage ramp
    QString tempSchedule;            // temp_schedule: "T:mode:val;..." (mode=steps|reach)
    QVector<TempRegion> tempRegions; // temp_regions: per-atom-subset thermostats (empty = none)
};

/**
 * @brief Worker object that runs curcuma simulations in a QThread.
 *
 * Claude Generated - Interactive Simulation Integration
 *
 * Converts qurcuma Atom geometry → curcuma Molecule, runs SimpleMD or CurcumaOpt
 * with a step callback, and emits frameReady() signals for live viewer updates.
 *
 * Usage:
 * @code
 * SimulationWorker* worker = new SimulationWorker;
 * QThread* thread = new QThread;
 * worker->moveToThread(thread);
 *
 * worker->setMolecule(atoms);
 * worker->setConfig(config);
 *
 * connect(thread, &QThread::started, worker, &SimulationWorker::run);
 * connect(worker, &SimulationWorker::frameReady, this, &MyClass::onFrame);
 * connect(worker, &SimulationWorker::finished, thread, &QThread::quit);
 * connect(worker, &SimulationWorker::finished, worker, &QObject::deleteLater);
 * connect(thread, &QThread::finished, thread, &QObject::deleteLater);
 *
 * thread->start();
 * @endcode
 */
class SimulationWorker : public QObject {
    Q_OBJECT

public:
    explicit SimulationWorker(QObject* parent = nullptr);
    ~SimulationWorker() override;  // non-default: unique_ptr<SimpleMD> needs full type in .cpp

    /** @brief Set the initial molecular geometry from viewer atoms. */
    void setMolecule(const QVector<MoleculeViewer::Atom>& atoms);

    /** @brief Set the bond topology used for shell-based force distribution.
     *  Must be called on the GUI thread before the worker is started. */
    void setBonds(const QVector<MoleculeViewer::Bond>& bonds);

    /** @brief Set simulation parameters before calling run(). */
    void setConfig(const SimulationConfig& config) { m_config = config; }

    /** @brief Request simulation stop after current step completes. Thread-safe. */
    void requestStop() { m_stopRequested.storeRelaxed(1); }

    /** @brief Request pause after current step. Thread-safe. */
    void requestPause() { m_pauseRequested.storeRelaxed(1); }

    /** @brief Resume from pause. Thread-safe. */
    void requestResume() { m_pauseRequested.storeRelaxed(0); }

public slots:
    /** @brief Start the simulation. Connect to QThread::started. */
    void run();

    /** @brief Run exactly one step and emit a frame, then return. Does not start
     *  or stop the auto-run timer — safe to call from any state. For Opt, runs
     *  one optimizer iteration against the persistent OptimizerDriver. */
    void stepOnce();

    /** @brief Inject an external force on @p atomIndex, spread through the
     *  bond graph with exponential decay (@p alpha per shell, cut at
     *  @p maxShells). The force is in Eh/Bohr and is applied additively to the
     *  gradient of EVERY subsequent MD step / Opt iteration until
     *  clearInjectedForce() is called — i.e. the force is "sticky" and acts for
     *  as long as the user holds the mouse grab, not just on the step that
     *  happens to coincide with a mouse-move event. Thread-safe; may be called
     *  from the GUI thread via QueuedConnection. */
    void injectForce(int atomIndex, QVector3D force, double alpha, int maxShells);

    /** @brief Drop the sticky injected force (mouse release / stop grab). After
     *  this the next step/iteration applies no external bias. */
    void clearInjectedForce();

    /** @brief Live-set the global thermostat target temperature (Kelvin). Thread-safe;
     *  called from the GUI thread via QueuedConnection when the user drags the temperature
     *  slider during a run. Stored as a pending value and pushed into the running SimpleMD
     *  before the next step (mirrors the sticky-force pattern). Overriding the setpoint this
     *  way cancels any active global temperature ramp for the rest of the run. */
    void setTargetTemperature(double temperature);

signals:
    /**
     * @brief Emitted after each dump step with updated atom positions.
     * Claude Generated - Zero-copy signal: QSharedPointer marshals only the pointer across threads.
     * Consumers read positions/energy/ekin/step from the shared frame without copying atom data.
     */
    void frameReady(SimulationFramePtr frame);

    /** @brief Emitted when the simulation completes normally or is stopped. */
    void finished();

    /** @brief Emitted on fatal error (method unavailable, convergence failure, etc.). */
    void errorOccurred(QString message);

    /** @brief Emitted when simulation enters paused state. */
    void paused();

private slots:
    // Timer-driven MD: fires every 1000/fpsLimit ms. One fire = one md.step() + one emit.
    // If md.step() runs longer than the interval, Qt fires the timer again immediately and
    // the visible rate drops to the actual compute rate — no frames skipped, no accumulation.
    void performMDStep();

private:
    void startMD();             // build SimpleMD, start m_mdTimer; returns so the thread's event loop can drive it
    void finalizeMDRun();       // stop timer, finalizeRun, emit finished
    void runOptimization();     // synchronous — drives its own step callback inside Optimizer::Optimize()

    // moleculeToAtoms() is defined in simulationworker.cpp only (uses curcuma Molecule type,
    // which must not be exposed in this header to avoid include pollution).
    // Claude Generated - curcuma types are encapsulated in the .cpp translation unit.

    // Returns a copy of the currently held mouse-grab force (N_atoms × 3), or an
    // empty matrix when none is active. "Sticky": does NOT consume the force, so
    // the same grab keeps biasing every step until clearInjectedForce() drops it.
    Eigen::MatrixXd currentInjectedForces(int atomCount);

    QVector<MoleculeViewer::Atom> m_initialAtoms;
    QVector<MoleculeViewer::Bond> m_bonds;
    forceinjector::Adjacency m_adjacency;
    SimulationConfig m_config;
    QAtomicInt m_stopRequested{ 0 };
    QAtomicInt m_pauseRequested{ 0 };
    QElapsedTimer m_lastEmitTimer;  // FPS throttle for OPT step callback

    // MD state persisted across QTimer fires (lives in the worker thread)
    std::unique_ptr<SimpleMD> m_md;
    QTimer* m_mdTimer = nullptr;    // parent = this, auto-cleaned

    // Performance-analysis accumulators for MD (timer-driven, so counters must persist)
    QElapsedTimer m_mdPerfTimer;
    int m_mdFrameCount = 0;
    qint64 m_mdTotalStepTime = 0;
    qint64 m_mdMinStepTime = std::numeric_limits<qint64>::max();
    qint64 m_mdMaxStepTime = 0;

    // Sticky mouse-grab force: written by the GUI thread (injectForce /
    // clearInjectedForce), read every step by the worker thread. Held until the
    // grab is released so it biases every MD step / Opt iteration in between.
    QMutex m_forceMutex;
    Eigen::MatrixXd m_pendingForces;
    bool m_pendingForcesValid = false;

    // Live global temperature: written by the GUI thread (setTargetTemperature), read once per
    // step by the worker thread in performMDStep() and pushed into SimpleMD. Claude Generated 2026.
    QMutex m_tempMutex;
    double m_pendingTemperature = 0.0;
    bool m_pendingTemperatureValid = false;
};
