// simulationworker.h - QThread wrapper for curcuma MD and geometry optimization
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration

#pragma once

#include "view.h"

#include <QAtomicInt>
#include <QObject>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <atomic>

/**
 * @brief Configuration struct for a simulation run.
 * Claude Generated - Interactive Simulation Integration
 */
struct SimulationConfig {
    enum class Mode { MolecularDynamics, GeometryOptimization };

    Mode mode = Mode::MolecularDynamics;
    QString method = "gfnff";     // Default: GFN-FF (always available, no external deps)
    double temperature = 300.0;   // K (MD only)
    double timestep = 1.0;        // fs (MD only)
    int steps = 1000;             // Total MD steps or max opt iterations
    int dumpFrequency = 10;       // Callback/save every N steps
    double convergence = 1e-6;    // Gradient convergence threshold (opt only)
    bool writeTrajectory = false; // Also write .trj.xyz file to disk
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
    ~SimulationWorker() override = default;

    /** @brief Set the initial molecular geometry from viewer atoms. */
    void setMolecule(const QVector<MoleculeViewer::Atom>& atoms);

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

signals:
    /**
     * @brief Emitted after each dump step with updated atom positions.
     * @param atoms Updated positions (same count/order as initial molecule)
     * @param energy Potential energy [Hartree]
     * @param ekin   Kinetic energy [Hartree] (0.0 for geometry optimization)
     * @param step   Current step / iteration number
     */
    void frameReady(QVector<MoleculeViewer::Atom> atoms, double energy, double ekin, int step);

    /** @brief Emitted when the simulation completes normally or is stopped. */
    void finished();

    /** @brief Emitted on fatal error (method unavailable, convergence failure, etc.). */
    void errorOccurred(QString message);

    /** @brief Emitted when simulation enters paused state. */
    void paused();

private:
    void runMD();
    void runOptimization();

    // moleculeToAtoms() is defined in simulationworker.cpp only (uses curcuma Molecule type,
    // which must not be exposed in this header to avoid include pollution).
    // Claude Generated - curcuma types are encapsulated in the .cpp translation unit.

    QVector<MoleculeViewer::Atom> m_initialAtoms;
    SimulationConfig m_config;
    QAtomicInt m_stopRequested{ 0 };
    QAtomicInt m_pauseRequested{ 0 };
    std::atomic<bool> m_externalStop{ false };  // Claude Generated - Passed to SimpleMD for unlimited MD
};
