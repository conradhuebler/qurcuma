// recipe.h - Simulation recipes: named simulation protocols (UX stage 6 S2).
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026.
//
// A recipe is a named simulation *protocol*: mode, temperature and its control
// (thermostat, ramp, regions), time step, run length, constraints, bias and walls.
// It never carries the *system*, the machine or the user's working preferences:
// method, charge, unpaired electrons and the GFN-FF topology mode stay as they are,
// as do GPU, speed, performance output, trajectory writing and the interactive
// keep-parameters option. Applying a recipe therefore cannot turn a charged GFN2
// setup into a neutral GFN-FF one, just as a Look never switches a quick toggle.
//
// Storage uses the protocol part of simConfigToJson() (lesson.h), the same lossless
// round trip that lesson structures carry, so a recipe and a lesson condition have
// one format.
#pragma once

#include "lesson.h"  // SimulationConfig, simConfigToJson / simConfigFromJson

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct SimulationRecipe {
    QString name;
    QString description;   ///< one line: what the recipe sets
    QJsonObject protocol;  ///< simConfigToJson() keys without recipes::keptKeys()
};

namespace recipes {

/// simConfigToJson() keys a recipe leaves alone: the system (method, charge, spin,
/// topology mode), the machine (GPU) and working preferences (speed, performance
/// output, trajectory file, keeping parameters while dragging).
inline const QStringList& keptKeys()
{
    static const QStringList keys = {
        QStringLiteral("method"), QStringLiteral("charge"), QStringLiteral("spin"),
        QStringLiteral("topologyMode"), QStringLiteral("gpu"), QStringLiteral("fpsLimit"),
        QStringLiteral("performanceAnalysis"), QStringLiteral("performanceInterval"),
        QStringLiteral("writeTrajectory"), QStringLiteral("optKeepParameters"),
    };
    return keys;
}

/// The protocol part of a configuration (everything a recipe stores).
inline QJsonObject protocolOf(const SimulationConfig& cfg)
{
    QJsonObject o = simConfigToJson(cfg);
    for (const QString& key : keptKeys())
        o.remove(key);
    return o;
}

/// @p current with the recipe's protocol laid over it. Kept keys are ignored even if
/// a stored recipe carries them, so method, charge and spin always survive.
inline SimulationConfig apply(const SimulationConfig& current, const SimulationRecipe& recipe)
{
    QJsonObject o = simConfigToJson(current);
    for (auto it = recipe.protocol.begin(); it != recipe.protocol.end(); ++it)
        if (!keptKeys().contains(it.key()))
            o.insert(it.key(), it.value());
    return simConfigFromJson(o);
}

/// Built-in recipes. Each is a complete protocol built from SimulationConfig's
/// defaults, so it also switches off every feature it does not use (ramp, regions,
/// RATTLE, RMSD-MTD, walls), whatever was set before.
inline QVector<SimulationRecipe> builtIn()
{
    QVector<SimulationRecipe> out;
    auto add = [&out](const QString& name, const QString& description, const SimulationConfig& cfg) {
        out.append({ name, description, protocolOf(cfg) });
    };

    // curcuma's "loose" optimizer preset: gradient norm 1e-3 Eh/Bohr, 1000 iterations
    // (external/curcuma/src/capabilities/curcumaopt.cpp, convergence_preset).
    SimulationConfig relax;
    relax.mode = SimulationConfig::Mode::GeometryOptimization;
    relax.optimizer = QStringLiteral("auto");
    relax.convergence = 1e-3;
    relax.steps = 1000;
    add(QStringLiteral("Quick relax"),
        QStringLiteral("Geometry optimization, automatic optimizer, gradient 1e-3 Eh/Bohr, "
                       "at most 1000 iterations (curcuma's loose preset)."), relax);

    SimulationConfig md;
    md.mode = SimulationConfig::Mode::MolecularDynamics;
    md.temperature = 300.0;
    md.thermostat = QStringLiteral("csvr");
    md.thermostatCoupling = 10.0;
    md.timestep = 1.0;
    md.steps = 10000;
    add(QStringLiteral("MD 300 K"),
        QStringLiteral("MD at 300 K, CSVR thermostat (10 fs coupling), 1 fs time step, "
                       "10000 steps (10 ps), no ramp, constraints, bias or walls."), md);

    // Start cold, ramp the setpoint to 300 K, then hold it until the running-mean
    // temperature is within 10 K (curcuma temp_schedule grammar; the last setpoint
    // is held after the schedule ends, docs/TEMPERATURE_RAMP.md).
    SimulationConfig heat = md;
    heat.temperature = 50.0;
    heat.tempRamp = true;
    heat.tempSchedule = QStringLiteral("300:steps:10000;300:reach:10");
    heat.steps = 20000;
    add(QStringLiteral("Heat up"),
        QStringLiteral("MD starting at 50 K, setpoint ramped to 300 K over 10000 steps, then "
                       "held until the temperature is within 10 K; 20000 steps in total."), heat);

    // Spherical harmonic wall around the origin; radius 0 lets curcuma size it from
    // the molecule (not drawn in the viewer, which needs explicit bounds).
    SimulationConfig confined = md;
    confined.wallEnabled = true;
    confined.wallType = 1;
    confined.wallHarmonic = true;
    confined.wallRadius = 0.0;
    add(QStringLiteral("Confined"),
        QStringLiteral("MD at 300 K inside a spherical harmonic wall that curcuma sizes "
                       "from the molecule (radius 0 = automatic)."), confined);
    return out;
}

inline bool isBuiltInName(const QString& name)
{
    for (const SimulationRecipe& r : builtIn())
        if (r.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

inline QJsonObject toJson(const SimulationRecipe& recipe)
{
    QJsonObject o;
    o["name"] = recipe.name;
    o["description"] = recipe.description;
    o["protocol"] = recipe.protocol;
    return o;
}

inline SimulationRecipe fromJson(const QJsonObject& o)
{
    return { o.value("name").toString(), o.value("description").toString(),
             o.value("protocol").toObject() };
}

} // namespace recipes
