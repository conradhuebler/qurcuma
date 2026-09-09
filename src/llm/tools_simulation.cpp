// tools_simulation.cpp - Running the interactive MD and geometry optimisation.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_simulation.h"

#include "core/toolregistry.h"
#include "simulationcontrolwidget.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QWaitCondition>

#include <memory>

namespace {

QJsonObject schema(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

/// Turn a property of @p schema into a closed enum over @p values. The lists come
/// from the dock's own combos, so the schema offers exactly what the run will
/// accept; writing them out here would be a third copy of the same list.
void restrictTo(QJsonObject& schemaObject, const char* property, const QStringList& values)
{
    if (values.isEmpty())
        return;   // combo not built yet; leave the property a free string
    QJsonObject properties = schemaObject.value(QStringLiteral("properties")).toObject();
    QJsonObject prop = properties.value(QLatin1String(property)).toObject();
    prop.insert(QStringLiteral("enum"), QJsonArray::fromStringList(values));
    properties.insert(QLatin1String(property), prop);
    schemaObject.insert(QStringLiteral("properties"), properties);
}

/// The live state, readable from the agent loop's thread.
///
/// The dock is a widget and may only be touched on the GUI thread, so its
/// liveStateChanged signal is copied in here under a lock. The wait condition is
/// what lets simulation_status wait for a run to end in one call instead of
/// polling once a second for the length of an MD.
struct StatusCache {
    QMutex mutex;
    QWaitCondition changed;
    SimulationControlWidget::LiveState state;
};

QJsonObject stateToJson(const SimulationControlWidget::LiveState& state)
{
    QJsonObject o;
    o.insert(QStringLiteral("running"), state.running);
    if (!state.running)
        return o;
    o.insert(QStringLiteral("paused"), state.paused);
    o.insert(QStringLiteral("mode"), state.mode);
    o.insert(QStringLiteral("method"), state.method);
    o.insert(QStringLiteral("step"), state.step);
    o.insert(QStringLiteral("total_steps"), state.totalSteps);
    o.insert(QStringLiteral("energy"), state.energy);
    o.insert(QStringLiteral("energy_unit"), QStringLiteral("Eh"));
    if (state.mode == QLatin1String("md")) {
        o.insert(QStringLiteral("kinetic_energy"), state.kineticEnergy);
        o.insert(QStringLiteral("temperature"), state.temperature);
        o.insert(QStringLiteral("target_temperature"), state.targetTemperature);
        o.insert(QStringLiteral("time_fs"), state.timeFs);
    }
    return o;
}

/// The config a run starts from: the dock's current settings with whatever the
/// model named written over them. Only the fields it can also see in
/// simulation_status are exposed; the rest of SimulationConfig stays where the
/// operator set it.
SimulationConfig configFromArgs(const SimulationConfig& base, const QJsonObject& args)
{
    SimulationConfig cfg = base;
    const QString mode = args.value(QStringLiteral("mode")).toString();
    if (mode == QLatin1String("opt"))
        cfg.mode = SimulationConfig::Mode::GeometryOptimization;
    else if (mode == QLatin1String("md"))
        cfg.mode = SimulationConfig::Mode::MolecularDynamics;

    const auto takeString = [&args](const char* key, QString& target) {
        const QString value = args.value(QLatin1String(key)).toString();
        if (!value.isEmpty())
            target = value;
    };
    const auto takeDouble = [&args](const char* key, double& target) {
        const QJsonValue value = args.value(QLatin1String(key));
        if (value.isDouble())
            target = value.toDouble();
    };
    const auto takeInt = [&args](const char* key, int& target) {
        const QJsonValue value = args.value(QLatin1String(key));
        if (value.isDouble())
            target = value.toInt();
    };

    takeString("method", cfg.method);
    takeString("optimizer", cfg.optimizer);
    takeString("thermostat", cfg.thermostat);
    takeInt("steps", cfg.steps);
    takeDouble("temperature", cfg.temperature);
    takeDouble("timestep", cfg.timestep);
    takeDouble("convergence", cfg.convergence);
    return cfg;
}

}  // namespace

int registerSimulationTools(ToolRegistry& registry, const SimulationToolContext& context)
{
    SimulationControlWidget* const control = context.control;
    if (!control)
        return 0;

    auto cache = std::make_shared<StatusCache>();
    QObject::connect(control, &SimulationControlWidget::liveStateChanged, control,
        [cache](const SimulationControlWidget::LiveState& state) {
            {
                QMutexLocker lock(&cache->mutex);
                cache->state = state;
            }
            cache->changed.wakeAll();
        });

    int added = 0;
    const auto add = [&registry, &added](const ToolSpec& spec) {
        if (registry.add(spec))
            ++added;
    };

    // --- run_simulation -----------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("run_simulation");
        spec.category = QStringLiteral("simulation");
        spec.description = QStringLiteral(
            "Start molecular dynamics or a geometry optimisation on the loaded structure. "
            "Returns at once -- the run keeps going and updates the view; ask "
            "simulation_status, with wait_seconds to be told when it ends. The geometry it "
            "leaves behind becomes the current structure. Anything not named keeps the "
            "setting standing in the Simulation dock.");
        spec.effect = ToolEffect::Compute;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "mode":        { "type": "string", "enum": ["md", "opt"],
                             "description": "molecular dynamics, or geometry optimisation" },
            "method":      { "type": "string", "description": "energy method" },
            "steps":       { "type": "integer", "minimum": 1, "maximum": 1000000,
                             "description": "MD steps, or the iteration ceiling for opt" },
            "temperature": { "type": "number", "minimum": 0, "maximum": 5000,
                             "description": "thermostat setpoint in K (md); adjustable while running via set_temperature" },
            "timestep":    { "type": "number", "minimum": 0.1, "maximum": 10,
                             "description": "fs (md)" },
            "thermostat":  { "type": "string", "description": "md only" },
            "optimizer":   { "type": "string", "description": "opt only" },
            "convergence": { "type": "number", "description": "gradient threshold (opt only)" }
          },
          "required": ["mode"]
        })JSON");
        restrictTo(spec.paramSchema, "method", control->methodValues());
        restrictTo(spec.paramSchema, "thermostat", control->thermostatValues());
        restrictTo(spec.paramSchema, "optimizer", control->optimizerValues());

        spec.handler = [control](const QJsonObject& args) {
            const SimulationConfig cfg = configFromArgs(control->currentConfig(), args);
            QString error;
            if (!control->startRun(cfg, &error))
                return ToolResult::failure(error);

            const bool md = cfg.mode == SimulationConfig::Mode::MolecularDynamics;
            QJsonObject data;
            data.insert(QStringLiteral("status"), QStringLiteral("started"));
            data.insert(QStringLiteral("mode"), md ? QStringLiteral("md") : QStringLiteral("opt"));
            data.insert(QStringLiteral("method"), cfg.method);
            data.insert(QStringLiteral("steps"), cfg.steps);
            if (md) {
                data.insert(QStringLiteral("temperature"), cfg.temperature);
                data.insert(QStringLiteral("timestep_fs"), cfg.timestep);
            }
            return ToolResult::success(data, md
                    ? QStringLiteral("MD started: %1 steps of %2 fs at %3 K with %4. Ask "
                                     "simulation_status with wait_seconds.")
                          .arg(cfg.steps).arg(cfg.timestep).arg(cfg.temperature).arg(cfg.method)
                    : QStringLiteral("Optimisation started: at most %1 iterations with %2. Ask "
                                     "simulation_status with wait_seconds.")
                          .arg(cfg.steps).arg(cfg.method));
        };
        add(spec);
    }

    // --- simulation_status --------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("simulation_status");
        spec.category = QStringLiteral("simulation");
        spec.description = QStringLiteral(
            "Step, energy and temperature of the running simulation. With wait_seconds it "
            "waits for the run to end rather than returning at once, which is one call "
            "instead of a poll per second.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;   // never blocks the GUI thread
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "wait_seconds": { "type": "integer", "minimum": 0, "maximum": 60,
                              "description": "wait up to this long for the run to end (default 0)" }
          }
        })JSON");

        spec.handler = [cache](const QJsonObject& args) {
            const int waitSeconds = qBound(0, args.value(QStringLiteral("wait_seconds")).toInt(), 60);
            QElapsedTimer clock;
            clock.start();

            QMutexLocker lock(&cache->mutex);
            forever {
                if (!cache->state.running || waitSeconds == 0)
                    break;
                const qint64 left = qint64(waitSeconds) * 1000 - clock.elapsed();
                if (left <= 0)
                    break;
                cache->changed.wait(&cache->mutex, qMin<qint64>(left, 500));
            }
            const SimulationControlWidget::LiveState state = cache->state;
            lock.unlock();

            const QJsonObject data = stateToJson(state);
            if (!state.running)
                return ToolResult::success(data, QStringLiteral("Nothing is running."));
            return ToolResult::success(data,
                QStringLiteral("%1, step %2 of %3.")
                    .arg(state.paused ? QStringLiteral("Paused") : QStringLiteral("Running"))
                    .arg(state.step).arg(state.totalSteps));
        };
        add(spec);
    }

    // --- pause / resume / stop / step ---------------------------------------
    const auto control_tool = [&add, control](const QString& name, const QString& description,
                                  std::function<ToolResult(SimulationControlWidget*)> act) {
        ToolSpec spec;
        spec.name = name;
        spec.category = QStringLiteral("simulation");
        spec.description = description;
        spec.effect = ToolEffect::Compute;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({ "type": "object", "properties": {} })JSON");
        spec.handler = [control, act](const QJsonObject&) { return act(control); };
        add(spec);
    };

    control_tool(QStringLiteral("pause_simulation"),
        QStringLiteral("Hold the running simulation after the current step. The structure "
                       "stays where it is and can be measured; resume_simulation continues."),
        [](SimulationControlWidget* c) {
            if (!c->isRunning())
                return ToolResult::failure(QStringLiteral("nothing is running"));
            if (c->liveState().paused)
                return ToolResult::success({}, QStringLiteral("Already paused."));
            c->pauseRun();
            return ToolResult::success({}, QStringLiteral("Paused after the current step."));
        });

    control_tool(QStringLiteral("resume_simulation"),
        QStringLiteral("Continue a paused simulation."),
        [](SimulationControlWidget* c) {
            if (!c->isRunning())
                return ToolResult::failure(QStringLiteral("nothing is running"));
            if (!c->liveState().paused)
                return ToolResult::success({}, QStringLiteral("Already running."));
            c->resumeRun();
            return ToolResult::success({}, QStringLiteral("Resumed."));
        });

    control_tool(QStringLiteral("stop_simulation"),
        QStringLiteral("End the running simulation after the current step. The geometry "
                       "reached stays as the current structure."),
        [](SimulationControlWidget* c) {
            if (!c->isRunning())
                return ToolResult::failure(QStringLiteral("nothing is running"));
            c->onStopClicked();
            return ToolResult::success({}, QStringLiteral("Stop requested; it ends after the "
                                                          "current step."));
        });

    control_tool(QStringLiteral("step_simulation"),
        QStringLiteral("Advance one MD step, or one optimiser iteration, and stop again. For "
                       "watching a change closely; it needs no run to be going."),
        [](SimulationControlWidget* c) {
            if (c->isRunning())
                return ToolResult::failure(QStringLiteral(
                    "a simulation is already running; pause or stop it to step by hand"));
            c->onStepClicked();
            return ToolResult::success({}, QStringLiteral("One step taken."));
        });

    // --- set_temperature ----------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("set_temperature");
        spec.category = QStringLiteral("simulation");
        spec.description = QStringLiteral(
            "Move the thermostat setpoint of the running MD. It takes effect at the next "
            "step and cancels a global temperature ramp for the rest of the run.");
        spec.effect = ToolEffect::Compute;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "temperature": { "type": "number", "minimum": 0, "maximum": 5000,
                             "description": "new setpoint in K" }
          },
          "required": ["temperature"]
        })JSON");

        spec.handler = [control](const QJsonObject& args) {
            const double kelvin = args.value(QStringLiteral("temperature")).toDouble();
            if (!control->isRunning())
                return ToolResult::failure(QStringLiteral("no simulation is running"));
            if (control->liveState().mode != QLatin1String("md")) {
                return ToolResult::failure(QStringLiteral(
                    "a geometry optimisation has no thermostat"));
            }
            control->setLiveTemperature(kelvin);
            QJsonObject data;
            data.insert(QStringLiteral("target_temperature"), kelvin);
            return ToolResult::success(data,
                QStringLiteral("Setpoint moved to %1 K; it takes effect at the next step.")
                    .arg(kelvin));
        };
        add(spec);
    }

    return added;
}
