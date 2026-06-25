// simulationframe.h - Zero-copy payload for simulation frames
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated - Signal-path: ships positions via QSharedPointer to avoid per-frame copies
//
// Worker produces one SimulationFrame per dump step and wraps it in a QSharedPointer.
// Qt's queued-connection marshalling then only copies the pointer across threads, no
// per-atom data. Consumers read positions/energy/ekin/step read-only.

#pragma once

#include <QMetaType>
#include <QSharedPointer>
#include <QVector3D>
#include <vector>

struct SimulationFrame {
    std::vector<QVector3D> positions;  // One entry per atom, same order as initial molecule
    double energy = 0.0;                // Potential energy [Hartree]
    double ekin = 0.0;                  // Kinetic energy [Hartree] (0 for geometry optimisation)
    int step = 0;                       // Current MD step / optimisation iteration
    double temperature = 0.0;           // Instantaneous temperature [K] (MD only; 0 for opt). Claude Generated 2026
    double targetTemperature = 0.0;     // Thermostat setpoint [K] (MD only; tracks the ramp). Claude Generated 2026
};

using SimulationFramePtr = QSharedPointer<const SimulationFrame>;

Q_DECLARE_METATYPE(SimulationFramePtr)
