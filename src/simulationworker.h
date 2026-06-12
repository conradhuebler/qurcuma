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
struct SimulationConfig {
    enum class Mode { MolecularDynamics, GeometryOptimization };

    Mode mode = Mode::MolecularDynamics;
    QString method = "gfnff";     // Energy method: gfnff / uff / gfn2 / gfn1
    QString optimizer = "auto";   // Opt algorithm: auto / lbfgspp / native_lbfgs / diis / rfo / ancopt
    double temperature = 300.0;   // K (MD only)
    double timestep = 1.0;        // fs (MD only)
    int steps = 1000;             // Total MD steps or max opt iterations
    double convergence = 1e-6;    // Gradient convergence threshold (opt only)
    bool writeTrajectory = false; // Also write .trj.xyz file to disk
    int fpsLimit = 30;            // Simulation speed in steps/sec (0 = unlimited)
    bool performanceAnalysis = false; // Per-frame timing stats every N steps
    int performanceInterval = 100;   // Output summary every N frames
    QString gpu = "none";         // GPU acceleration: "none", "cuda", "auto"

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
     *  gradient of the next MD step. Thread-safe; may be called from the GUI
     *  thread via QueuedConnection. */
    void injectForce(int atomIndex, QVector3D force, double alpha, int maxShells);

    /** @brief Clear any pending injected force (mouse release / stop grab). */
    void clearInjectedForce();

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

    Eigen::MatrixXd drainPendingForces(int atomCount);

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

    // Injected force queue (produced by GUI thread, drained by worker thread)
    QMutex m_forceMutex;
    Eigen::MatrixXd m_pendingForces;
    bool m_pendingForcesValid = false;
};
