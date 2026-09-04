// simulationframe.h - Zero-copy payload for simulation frames
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Signal-path: ships positions via QSharedPointer to avoid per-frame copies
//
// Worker produces one SimulationFrame per dump step and wraps it in a QSharedPointer.
// Qt's queued-connection marshalling then only copies the pointer across threads, no
// per-atom data. Consumers read positions/energy/ekin/step read-only.

#pragma once

#include <QMetaType>
#include <QPair>
#include <QSharedPointer>
#include <QVector>
#include <QVector3D>
#include <vector>

#include "ncitypes.h"

// Claude Generated 2026 - Bond of the force field's own topology (reactive GFN-FF).
struct FrameBond {
    int a = 0;
    int b = 0;
    int order = 1;   // 1 + rounded Hueckel pi order, clamped to 1..3
};

// Claude Generated 2026 - One topology-change event of a reactive GFN-FF run.
struct ReactEventView {
    int step = 0;                              // MD step at which the rebuild happened
    QVector<QPair<int, int>> formed;           // canonical i<j, 0-based atom indices
    QVector<QPair<int, int>> broken;
    double deJumpKJmol = 0.0;                  // potential-energy discontinuity of the rebuild (NaN if unmeasured)
};

struct SimulationFrame {
    std::vector<QVector3D> positions;  // One entry per atom, same order as initial molecule
    double energy = 0.0;                // Potential energy [Hartree]
    double ekin = 0.0;                  // Kinetic energy [Hartree] (0 for geometry optimisation)
    int step = 0;                       // Current MD step / optimisation iteration
    double temperature = 0.0;           // Instantaneous temperature [K] (MD only; 0 for opt). Claude Generated 2026
    double targetTemperature = 0.0;     // Thermostat setpoint [K] (MD only; tracks the ramp). Claude Generated 2026
    // Claude Generated 2026 - Live non-covalent contacts read out of the running
    // GFN-FF force field. Empty unless the live overlay is switched on, and empty
    // for every other method, so the default MD path carries no extra cost.
    QVector<nci::Contact> nciContacts;
    // Claude Generated 2026 - The force field's live bond topology (reactive GFN-FF
    // runs only). Empty for every other run: the viewer then keeps re-deriving bonds
    // from the geometry. topologyVersion counts the force field's rebuilds so consumers
    // can skip frames whose bond list they already hold; events lists the rebuilds
    // that happened inside this step.
    std::vector<FrameBond> bonds;
    int topologyVersion = -1;
    QVector<ReactEventView> events;
};

using SimulationFramePtr = QSharedPointer<const SimulationFrame>;

Q_DECLARE_METATYPE(SimulationFramePtr)
