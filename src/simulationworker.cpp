// simulationworker.cpp - QThread wrapper for curcuma MD and geometry optimization
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Interactive Simulation Integration

#include "simulationworker.h"

// Claude Generated - json must be included before curcuma headers (they use nlohmann types)
// HACK: curcuma uses external/json.hpp (not nlohmann/ subdir). This is a known limitation
//        of building curcuma as a submodule without its precompiled header.
#include "external/json.hpp"
using json = nlohmann::json;

#include <src/capabilities/simplemd.h>
#include <src/capabilities/curcumaopt.h>
#include <src/core/molecule.h>
#include <src/core/elements.h>
#include <src/core/parameter_registry.h>  // Claude Generated - for getDefaultJson()

#include <QThread>
#include <QDebug>

SimulationWorker::SimulationWorker(QObject* parent)
    : QObject(parent)
{
}

void SimulationWorker::setMolecule(const QVector<MoleculeViewer::Atom>& atoms)
{
    m_initialAtoms = atoms;
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

// Claude Generated - Convert qurcuma Atom vector → curcuma Molecule
// HACK: curcuma addPair() takes {atomic_number, Position}, not {symbol, pos}
//       We use String2Element() from elements.h to convert symbol → Z
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

// Claude Generated - File-local helper; curcuma Molecule type stays in this .cpp only
// Takes reference atoms for element names (positions are replaced from curcuma geometry)
static QVector<MoleculeViewer::Atom> moleculeToAtoms(
    const Molecule& mol, const QVector<MoleculeViewer::Atom>& reference)
{
    QVector<MoleculeViewer::Atom> result;
    result.reserve(reference.size());

    Geometry geo = mol.getGeometry();
    int n = std::min(static_cast<int>(geo.rows()), static_cast<int>(reference.size()));
    for (int i = 0; i < n; ++i) {
        MoleculeViewer::Atom a = reference[i]; // copy element, charge
        a.position = QVector3D(
            static_cast<float>(geo(i, 0)),
            static_cast<float>(geo(i, 1)),
            static_cast<float>(geo(i, 2)));
        result.append(a);
    }
    return result;
}

void SimulationWorker::runMD()
{
    m_externalStop.store(false);

    // Build curcuma JSON controller for SimpleMD.
    // Claude Generated - The layout must match what curcuma's CLI2Json produces:
    //   controller["simplemd"] = { ...module parameters... }
    //   controller["global"]   = { method, threads, verbosity, ... }
    // SimpleMD's ConfigManager looks for user_input["simplemd"] and falls back
    // to user_input["global"]; passing a flat json leaves both branches empty
    // and causes nlohmann::json::value() to be called on null members downstream.
    // Canonical snake_case names + correct JSON types. SimpleMD uses
    // m_config.get<bool>("write_xyz") etc.; passing 0/1 for a bool or using
    // an alias that the registry can't resolve triggers a nlohmann type_error
    // that ConfigManager reports as "Parameter not found".
    json simplemd_params;
    simplemd_params["method"] = m_config.method.toStdString();
    simplemd_params["temperature"] = m_config.temperature;
    simplemd_params["time_step"] = m_config.timestep;
    if (m_config.steps > 0)
        simplemd_params["max_time"] = static_cast<double>(m_config.steps) * m_config.timestep;
    else
        simplemd_params["max_time"] = 0.0;
    simplemd_params["dump_frequency"] = m_config.dumpFrequency;
    simplemd_params["write_xyz"] = m_config.writeTrajectory;

    json controller;
    controller["simplemd"] = simplemd_params;
    controller["global"]["method"] = m_config.method.toStdString();
    controller["global"]["verbosity"] = 0;
    controller["verbosity"] = 0;

    SimpleMD md(controller, true);
    md.setMolecule(atomsToMolecule(m_initialAtoms));

    // Pass our atomic stop flag so SimpleMD can check it in the loop (unlimited MD)
    md.setExternalStopFlag(&m_externalStop);

    // Register callback: runs on curcuma's thread, emit Qt signal to GUI thread
    md.setStepCallback([this](const Molecule& mol, int step, double Epot, double Ekin) {
        // Handle stop: set the atomic flag so the MD loop exits naturally
        if (m_stopRequested.loadRelaxed()) {
            m_externalStop.store(true, std::memory_order_relaxed);
            return;
        }
        // Handle pause: busy-wait on worker thread (non-blocking to GUI)
        while (m_pauseRequested.loadRelaxed()) {
            if (m_stopRequested.loadRelaxed()) {
                m_externalStop.store(true, std::memory_order_relaxed);
                break;
            }
            QThread::msleep(50);
        }
        emit frameReady(moleculeToAtoms(mol, m_initialAtoms), Epot, Ekin, step);
    });

    if (!md.Initialise()) {
        emit errorOccurred(tr("MD initialization failed. Method '%1' may not be available.")
                               .arg(m_config.method));
        return;
    }

    md.start();
}

void SimulationWorker::runOptimization()
{
    // Build curcuma JSON controller for CurcumaOpt
    // HACK: Use ParameterRegistry defaults as base (same pattern as runMD)
    json optDefaults = ParameterRegistry::getInstance().getDefaultJson("opt");
    json controller;
    controller["method"] = m_config.method.toStdString();
    controller["opt"] = optDefaults;

    // Override with user-selected parameters
    controller["opt"]["MaxIter"] = m_config.steps;
    controller["opt"]["GradNorm"] = m_config.convergence;
    controller["opt"]["writeXYZ"] = m_config.writeTrajectory ? 1 : 0;
    controller["opt"]["silent"] = 1;
    controller["verbosity"] = 0;

    CurcumaOpt opt(controller, true);

    Molecule mol = atomsToMolecule(m_initialAtoms);
    opt.addMolecule(mol);

    // Register callback: fires after each accepted optimization step
    opt.setStepCallback([this](const Molecule& mol, int iter, double energy) {
        if (m_stopRequested.loadRelaxed())
            return;
        while (m_pauseRequested.loadRelaxed()) {
            if (m_stopRequested.loadRelaxed())
                break;
            QThread::msleep(50);
        }
        emit frameReady(moleculeToAtoms(mol, m_initialAtoms), energy, 0.0, iter);
    });

    opt.start();
}
