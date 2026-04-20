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
#include <QElapsedTimer>
#include <QDebug>
#include <limits>

SimulationWorker::SimulationWorker(QObject* parent)
    : QObject(parent)
{
    static const int kPtrTypeId = qRegisterMetaType<SimulationFramePtr>("SimulationFramePtr");
    Q_UNUSED(kPtrTypeId);
}

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
        runMD();
        break;
    case SimulationConfig::Mode::GeometryOptimization:
        runOptimization();
        break;
    }

    emit finished();
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

void SimulationWorker::runMD()
{
    // Build controller JSON. dump_frequency is intentionally omitted: the
    // worker drives the loop step-by-step, so curcuma's internal dump cadence
    // only affects its XYZ trajectory output (gated by write_xyz).
    json simplemd_params;
    simplemd_params["method"] = m_config.method.toStdString();
    simplemd_params["temperature"] = m_config.temperature;
    simplemd_params["time_step"] = m_config.timestep;
    if (m_config.steps > 0)
        simplemd_params["max_time"] = static_cast<double>(m_config.steps) * m_config.timestep;
    else
        simplemd_params["max_time"] = 0.0;
    if (m_config.performanceAnalysis)
        simplemd_params["print_frequency"] = 1;
    simplemd_params["write_xyz"] = m_config.writeTrajectory;
    simplemd_params["no_restart"] = true;
    simplemd_params["no_center"] = true;

    json controller;
    controller["simplemd"] = simplemd_params;
    controller["global"]["method"] = m_config.method.toStdString();
    controller["global"]["gpu"] = m_config.gpu.toStdString();
    controller["global"]["verbosity"] = 0;
    controller["verbosity"] = 0;

    SimpleMD md(controller, true);
    md.setMolecule(atomsToMolecule(m_initialAtoms));

    if (!md.Initialise()) {
        emit errorOccurred(tr("MD initialization failed. Method '%1' may not be available.")
                               .arg(m_config.method));
        return;
    }
    md.prepareRun();

    QElapsedTimer stepTimer;
    stepTimer.start();
    int frameCount = 0;
    qint64 totalStepTime = 0;
    qint64 totalConvertTime = 0;
    qint64 minStepTime = std::numeric_limits<qint64>::max();
    qint64 maxStepTime = 0;

    while (!m_stopRequested.loadRelaxed()) {
        while (m_pauseRequested.loadRelaxed()) {
            if (m_stopRequested.loadRelaxed())
                break;
            QThread::msleep(50);
        }
        if (m_stopRequested.loadRelaxed())
            break;

        // Apply any force the GUI queued since the previous step (mouse grab).
        // Converted from MatrixXd (col-major) to Geometry (row-major) on assign.
        Eigen::MatrixXd pending = drainPendingForces(m_initialAtoms.size());
        if (pending.rows() > 0) {
            Geometry ext = pending;
            md.applyExternalForces(ext);
        }

        if (!md.step())
            break;

        qint64 stepTime = stepTimer.restart();

        const bool fpsOk = (m_config.fpsLimit <= 0)
            || (m_lastEmitTimer.elapsed() >= 1000 / m_config.fpsLimit);
        if (!fpsOk)
            continue;

        QElapsedTimer convertTimer;
        convertTimer.start();
        SimulationFramePtr frame = moleculeToFrame(
            md.currentMolecule(), m_initialAtoms.size(),
            md.potentialEnergy(), md.kineticEnergy(), md.stepCount());
        qint64 convertTime = convertTimer.elapsed();

        if (m_config.performanceAnalysis) {
            totalStepTime += stepTime;
            totalConvertTime += convertTime;
            if (stepTime < minStepTime) minStepTime = stepTime;
            if (stepTime > maxStepTime) maxStepTime = stepTime;
            ++frameCount;

            if (frameCount >= m_config.performanceInterval) {
                qint64 avgStep = totalStepTime / frameCount;
                qint64 avgConvert = totalConvertTime / frameCount;
                qint64 avgOverhead = avgStep - avgConvert;
                double fps = 1000.0 * frameCount / (totalStepTime + totalConvertTime);
                qDebug() << "=== Performance [last" << frameCount << "frames @ step" << md.stepCount() << "] ==="
                         << "atoms:" << static_cast<int>(frame->positions.size())
                         << "avg_step:" << avgStep << "ms"
                         << "(compute:" << avgOverhead << "ms, convert:" << avgConvert << "ms)"
                         << "min:" << minStepTime << "max:" << maxStepTime
                         << "fps:" << fps;
                frameCount = 0;
                totalStepTime = 0;
                totalConvertTime = 0;
                minStepTime = std::numeric_limits<qint64>::max();
                maxStepTime = 0;
            }
        }

        emit frameReady(frame);
        m_lastEmitTimer.restart();
    }

    md.finalizeRun();
}

void SimulationWorker::runOptimization()
{
    // Migrated from the removed CurcumaOpt class to the unified
    // OptimizerFactory / OptimizationDispatcher API.
    // TODO: this path is still synchronous — interactive step-by-step opt
    // with mouse-injected forces requires exposing a step() API on
    // OptimizerDriver (analogous to SimpleMD::step()). Until then, only
    // the pre-opt and post-opt frames are emitted.
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

    // Emit the starting geometry so the viewer reflects the pre-opt state.
    emit frameReady(moleculeToFrame(mol, m_initialAtoms.size(), 0.0, 0.0, 0));

    try {
        EnergyCalculator calc(m_config.method.toStdString(), energy_controller);
        // Note: do NOT call calc.setMolecule() here — OptimizerDriver::InitializeOptimization
        // performs the initialization and a double-init crashes GFN-FF.

        Optimization::OptimizerType opt_type =
            Optimization::parseOptimizerType(m_config.optimizer.toStdString());
        Optimization::OptimizationResult result =
            Optimization::OptimizationDispatcher::optimizeStructure(
                &mol, opt_type, &calc, opt_config);

        if (!result.success) {
            emit errorOccurred(tr("Optimization failed: %1")
                                   .arg(QString::fromStdString(result.error_message)));
            return;
        }

        emit frameReady(moleculeToFrame(result.final_molecule, m_initialAtoms.size(),
            result.final_energy, 0.0, result.iterations_performed));
    } catch (const std::exception& e) {
        emit errorOccurred(tr("Optimization threw: %1").arg(QString::fromUtf8(e.what())));
    }
}
