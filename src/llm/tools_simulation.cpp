// tools_simulation.cpp - Running the interactive MD and geometry optimisation.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_simulation.h"

#include "atomselection.h"
#include "core/tooldispatcher.h"
#include "core/toolregistry.h"
#include "measurements.h"
#include "moleculebridge.h"

#include <src/core/elements.h>
#include "simulationcontrolwidget.h"
#include "simulationframe.h"
#include "view.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QWaitCondition>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

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

/// The live state and the live geometry, readable from the agent loop's thread.
///
/// The dock is a widget and may only be touched on the GUI thread, so its signals
/// are copied in here under a lock: the summary from liveStateChanged, the
/// coordinates from every frame. The agent loop then measures on a consistent
/// snapshot without ever reaching into a widget, and the MD keeps stepping while
/// it does -- the two run alongside each other rather than taking turns.
///
/// The wait condition is what makes that useful: watch_simulation sleeps here
/// until a frame moves the quantity it cares about past a threshold, instead of
/// asking once a second and burning a round each time.
struct StatusCache {
    QMutex mutex;
    QWaitCondition changed;
    SimulationControlWidget::LiveState state;

    QVector<moldata::Atom> atoms;   ///< live geometry, elements included
    QVector<moldata::Atom> start;   ///< first frame of the current run, for RMSD
    int frameSeen = 0;              ///< bumped on every frame, so a waiter can tell

    /// The run that just ended. "Nothing is running" on its own sent a model
    /// looking through the log and the working directory for fifteen rounds,
    /// because a finished run and a run that never started read the same.
    SimulationControlWidget::LiveState lastRun;
    QString lastReason;
    bool lastAborted = false;
    bool everRan = false;
};

/// Quantities watch_simulation can wait on. Deliberately the ones a docking run
/// asks about: how close the guest is, how compact a fragment is, how far the
/// structure has travelled.
double evaluate(const QString& quantity, const StatusCache& cache,
                const QVector<int>& setA, const QVector<int>& setB, QString& unit, bool& ok)
{
    ok = true;
    unit = QStringLiteral("A");
    const auto positionsOf = [&cache](const QVector<int>& set) {
        std::vector<QVector3D> out;
        out.reserve(set.size());
        for (int index : set) {
            if (index >= 0 && index < cache.atoms.size())
                out.push_back(cache.atoms.at(index).position);
        }
        return out;
    };
    const auto centroid = [](const std::vector<QVector3D>& p) {
        QVector3D c;
        for (const QVector3D& v : p)
            c += v;
        return p.empty() ? c : c / float(p.size());
    };

    if (quantity == QLatin1String("step")) {
        unit = QString();
        return cache.state.step;
    }
    if (quantity == QLatin1String("energy")) {
        unit = QStringLiteral("Eh");
        return cache.state.energy;
    }
    if (quantity == QLatin1String("temperature")) {
        unit = QStringLiteral("K");
        return cache.state.temperature;
    }
    if (quantity == QLatin1String("density")) {
        unit = QStringLiteral("g/cm^3");
        if (cache.state.containerVolume <= 0.0 || cache.atoms.isEmpty()) {
            ok = false;
            return 0.0;
        }
        // Mass of everything in the box over the container volume. 1 u/A^3 is
        // 1.66053906660 g/cm^3 (the atomic mass unit in grams, times 10^24 A^3
        // per cm^3).
        double mass = 0.0;
        for (const moldata::Atom& atom : cache.atoms) {
            const int z = Elements::String2Element(atom.element.toStdString());
            if (z > 0 && z < int(Elements::AtomicMass.size()))
                mass += Elements::AtomicMass[z];
        }
        return mass / cache.state.containerVolume * 1.66053906660;
    }
    if (quantity == QLatin1String("gyration_radius"))
        return measure::gyrationRadius(positionsOf(setA));
    if (quantity == QLatin1String("rmsd_to_start")) {
        if (cache.start.size() != cache.atoms.size()) {
            ok = false;
            return 0.0;
        }
        std::vector<QVector3D> now;
        std::vector<QVector3D> then;
        now.reserve(cache.atoms.size());
        then.reserve(cache.atoms.size());
        for (int i = 0; i < cache.atoms.size(); ++i) {
            now.push_back(cache.atoms.at(i).position);
            then.push_back(cache.start.at(i).position);
        }
        return measure::rmsdToReference(now, then);
    }
    if (quantity == QLatin1String("centroid_distance")) {
        return centroid(positionsOf(setA)).distanceToPoint(centroid(positionsOf(setB)));
    }
    if (quantity == QLatin1String("min_distance")) {
        const std::vector<QVector3D> a = positionsOf(setA);
        const std::vector<QVector3D> b = positionsOf(setB);
        if (a.empty() || b.empty()) {
            ok = false;
            return 0.0;
        }
        double closest = std::numeric_limits<double>::max();
        for (const QVector3D& p : a)
            for (const QVector3D& q : b)
                closest = std::min<double>(closest, p.distanceToPoint(q));
        return closest;
    }
    ok = false;
    return 0.0;
}

/// What the run looks like right now, in the shape a docking loop keeps asking
/// for: how many fragments there are, how big each is, and how close they come.
/// Delivered with every observation rather than needing three more calls.
QJsonObject fragmentSummary(const QVector<moldata::Atom>& atoms)
{
    QJsonObject out;
    if (atoms.isEmpty())
        return out;
    curcuma::Molecule molecule = atomsToMolecule(atoms);
    const std::vector<std::vector<int>> fragments = molecule.GetFragments();
    out.insert(QStringLiteral("count"), int(fragments.size()));
    if (fragments.size() < 2 || fragments.size() > 4)
        return out;   // one fragment says nothing; many would be a wall of numbers

    QJsonArray sizes;
    QJsonArray radii;
    for (const std::vector<int>& fragment : fragments) {
        sizes.append(int(fragment.size()));
        std::vector<QVector3D> positions;
        positions.reserve(fragment.size());
        for (int index : fragment) {
            if (index >= 0 && index < atoms.size())
                positions.push_back(atoms.at(index).position);
        }
        radii.append(measure::gyrationRadius(positions));
    }
    out.insert(QStringLiteral("sizes"), sizes);
    out.insert(QStringLiteral("gyration_radii"), radii);

    QJsonArray closest;
    for (size_t i = 0; i < fragments.size(); ++i) {
        for (size_t j = i + 1; j < fragments.size(); ++j) {
            double best = std::numeric_limits<double>::max();
            for (int a : fragments[i])
                for (int b : fragments[j])
                    best = std::min<double>(best, atoms.at(a).position
                                                      .distanceToPoint(atoms.at(b).position));
            QJsonObject pair;
            pair.insert(QStringLiteral("between"),
                QStringLiteral("F%1-F%2").arg(i + 1).arg(j + 1));
            pair.insert(QStringLiteral("min_distance"), best);
            closest.append(pair);
        }
    }
    out.insert(QStringLiteral("closest_approach"), closest);
    return out;
}

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

    // An optimisation started from here runs once and stops. The keep-alive loop it
    // would otherwise get restarts for ever -- that exists so a mouse grab can drag
    // atoms against the optimiser, and with nobody holding a mouse it is a process
    // that never ends on its own.
    if (cfg.mode == SimulationConfig::Mode::GeometryOptimization) {
        cfg.optSingleShot = !args.value(QStringLiteral("keep_alive")).toBool();
        // The steps box belongs to the MD and stands at 10000. As an iteration
        // ceiling for an optimisation that is not a limit, it is an afternoon.
        if (!args.contains(QStringLiteral("steps")))
            cfg.steps = 500;
    }

    takeString("method", cfg.method);
    takeString("optimizer", cfg.optimizer);
    takeString("thermostat", cfg.thermostat);
    takeInt("steps", cfg.steps);
    takeDouble("temperature", cfg.temperature);
    takeDouble("timestep", cfg.timestep);
    takeDouble("convergence", cfg.convergence);
    takeDouble("energy_convergence", cfg.energyConvergence);
    return cfg;
}

}  // namespace

int registerSimulationTools(ToolRegistry& registry, const SimulationToolContext& context)
{
    SimulationControlWidget* const control = context.control;
    if (!control)
        return 0;

    MoleculeViewer* const viewer = context.viewer;
    ToolDispatcher* const dispatcher = context.dispatcher;

    auto cache = std::make_shared<StatusCache>();
    QObject::connect(control, &SimulationControlWidget::liveStateChanged, control,
        [cache](const SimulationControlWidget::LiveState& state) {
            {
                QMutexLocker lock(&cache->mutex);
                cache->state = state;
                if (state.running) {
                    // Kept up to date while it runs, so when it ends the numbers of
                    // its last frame are still there to report.
                    cache->lastRun = state;
                    cache->everRan = true;
                } else {
                    cache->start.clear();   // the next run gets its own reference
                }
            }
            cache->changed.wakeAll();
        });

    // Why the last run ended, so "nothing is running" can say whether that is
    // because one finished, one fell apart, or none was ever started.
    QObject::connect(control, &SimulationControlWidget::runEnded, control,
        [cache](const QString& reason, bool aborted) {
            {
                QMutexLocker lock(&cache->mutex);
                cache->lastReason = reason;
                cache->lastAborted = aborted;
                cache->everRan = true;
            }
            cache->changed.wakeAll();
        });

    // Every frame, on the GUI thread: copy the geometry into the cache so the agent
    // loop can measure on it without touching a widget. Elements come from the
    // dock's atom list, coordinates from the frame -- the frame carries positions
    // only, in the order of the initial molecule.
    QObject::connect(control, &SimulationControlWidget::frameObserved, control,
        [cache, control](const SimulationFramePtr& frame) {
            if (!frame)
                return;
            QVector<moldata::Atom> atoms = control->currentAtoms();
            if (atoms.size() != int(frame->positions.size()))
                return;   // an atom count change means the caches disagree; skip it
            for (int i = 0; i < atoms.size(); ++i)
                atoms[i].position = frame->positions[size_t(i)];
            {
                QMutexLocker lock(&cache->mutex);
                cache->atoms = atoms;
                if (cache->start.isEmpty())
                    cache->start = atoms;   // first frame of this run
                ++cache->frameSeen;
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
            "convergence": { "type": "number", "minimum": 1e-8, "maximum": 0.1,
                             "description": "opt only: gradient norm below which it is converged, in Eh/Bohr (default 5e-4). Much tighter is below what the methods resolve, and the run then goes to its iteration ceiling with the energy long flat" },
            "energy_convergence": { "type": "number", "minimum": 1e-4, "maximum": 10,
                             "description": "opt only: energy change between iterations below which it is converged, in kJ/mol (default 0.1)" },
            "keep_alive":  { "type": "boolean",
                             "description": "opt only: keep restarting the optimiser instead of stopping when it converges. Needed only to pull on atoms against it; it never ends by itself, so stop_simulation is the only way out (default false)" }
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
            if (!md)
                data.insert(QStringLiteral("keep_alive"), !cfg.optSingleShot);
            QString note = md
                ? QStringLiteral("MD started: %1 steps of %2 fs at %3 K with %4. Ask "
                                 "simulation_status with wait_seconds.")
                      .arg(cfg.steps).arg(cfg.timestep).arg(cfg.temperature).arg(cfg.method)
                : QStringLiteral("Optimisation started: %1 with %2, converged at %3 Eh/Bohr or "
                                 "%4 kJ/mol, at most %5 iterations. Ask simulation_status with "
                                 "wait_seconds.")
                      .arg(cfg.method).arg(cfg.optimizer)
                      .arg(cfg.convergence).arg(cfg.energyConvergence).arg(cfg.steps);
            if (!md && !cfg.optSingleShot) {
                note += QStringLiteral(" keep_alive is set, so it restarts for ever: it will not "
                                       "end on its own and stop_simulation is the only way out.");
            }
            return ToolResult::success(data, note);
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
            "wait_seconds": { "type": "integer", "minimum": 0, "maximum": 600,
                              "description": "wait up to this long for the run to end (default 0). One long wait costs one round; asking again every minute costs one each time." }
          }
        })JSON");

        spec.handler = [cache, dispatcher](const QJsonObject& args) {
            const int waitSeconds = qBound(0, args.value(QStringLiteral("wait_seconds")).toInt(), 600);
            QElapsedTimer clock;
            clock.start();

            QMutexLocker lock(&cache->mutex);
            forever {
                if (!cache->state.running || waitSeconds == 0)
                    break;
                if (dispatcher && dispatcher->isInterrupted())
                    break;
                const qint64 left = qint64(waitSeconds) * 1000 - clock.elapsed();
                if (left <= 0)
                    break;
                cache->changed.wait(&cache->mutex, qMin<qint64>(left, 500));
            }
            const SimulationControlWidget::LiveState state = cache->state;
            const SimulationControlWidget::LiveState last = cache->lastRun;
            const QString lastReason = cache->lastReason;
            const bool lastAborted = cache->lastAborted;
            const bool everRan = cache->everRan;
            lock.unlock();

            QJsonObject data = stateToJson(state);
            if (!state.running) {
                if (!everRan) {
                    return ToolResult::success(data,
                        QStringLiteral("Nothing is running, and nothing has run yet."));
                }
                // What it did, not just that it is over. A finished run and a run
                // that never started used to read the same.
                QJsonObject finished;
                finished.insert(QStringLiteral("mode"), last.mode);
                finished.insert(QStringLiteral("method"), last.method);
                finished.insert(QStringLiteral("steps_done"), last.step);
                finished.insert(QStringLiteral("steps_planned"), last.totalSteps);
                finished.insert(QStringLiteral("energy"), last.energy);
                finished.insert(QStringLiteral("energy_unit"), QStringLiteral("Eh"));
                if (last.mode == QLatin1String("md")) {
                    finished.insert(QStringLiteral("temperature"), last.temperature);
                    finished.insert(QStringLiteral("time_fs"), last.timeFs);
                }
                finished.insert(QStringLiteral("aborted"), lastAborted);
                if (!lastReason.isEmpty())
                    finished.insert(QStringLiteral("reason"), lastReason);
                data.insert(QStringLiteral("last_run"), finished);

                QString note = lastAborted
                    ? QStringLiteral("Nothing is running. The last run was aborted after %1 "
                                     "steps: %2").arg(last.step).arg(lastReason)
                    : QStringLiteral("Nothing is running. The last run finished after %1 of %2 "
                                     "steps at %3 Eh.")
                          .arg(last.step).arg(last.totalSteps).arg(last.energy, 0, 'f', 6);
                if (!lastAborted && !lastReason.isEmpty())
                    note += QStringLiteral(" %1.").arg(lastReason);
                note += QStringLiteral(" The geometry it reached is the current structure.");
                return ToolResult::success(data, note);
            }
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

    // --- pull_atoms ---------------------------------------------------------
    //
    // The mouse grab, without a mouse. SimulationWorker spreads each pull through
    // the bond graph and adds the sum to the gradient of every following step, so
    // this is a bias on a running calculation, not a displacement: the structure
    // answers with its own forces and what comes out is still a trajectory.
    {
        auto pulls = std::make_shared<QHash<int, QVector3D>>();

        ToolSpec spec;
        spec.name = QStringLiteral("pull_atoms");
        spec.category = QStringLiteral("simulation");
        spec.description = QStringLiteral(
            "Pull on atoms of the running simulation. The force is added to the gradient of "
            "every following step and stays until clear_forces, so the structure answers with "
            "its own forces instead of being teleported. Call it again with add=true to pull "
            "on another set in another direction at the same time.");
        spec.effect = ToolEffect::Compute;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "atoms":        { "type": "string",
                              "description": "selection grammar, e.g. \"F2\" for the second fragment" },
            "atom_indices": { "type": "array", "description": "explicit 0-based indices" },
            "force":        { "type": "array",
                              "description": "[fx, fy, fz] in Eh/Bohr, applied to EACH atom of the selection, so a 27-atom fragment feels 27 times this in total" },
            "add":          { "type": "boolean",
                              "description": "keep the pulls already set (default false: replace them)" },
            "alpha":        { "type": "number", "minimum": 0, "maximum": 1,
                              "description": "how much of the force reaches the next bonded shell (default 0.4)" },
            "max_shells":   { "type": "integer", "minimum": 0, "maximum": 10,
                              "description": "how many bonded shells the force spreads through; default 3 for a single atom, 0 for a set (spreading a whole fragment's pulls would multiply them)" }
          },
          "required": ["force"]
        })JSON");

        spec.handler = [control, viewer, pulls](const QJsonObject& args) {
            if (!control->isRunning())
                return ToolResult::failure(QStringLiteral(
                    "nothing is running: a pull is a bias on a calculation in progress. Start "
                    "one with run_simulation, or move the atoms outright with transform_atoms."));

            const QJsonArray forceArray = args.value(QStringLiteral("force")).toArray();
            if (forceArray.size() != 3)
                return ToolResult::failure(QStringLiteral("force needs three numbers"));
            const QVector3D force(float(forceArray.at(0).toDouble()),
                float(forceArray.at(1).toDouble()), float(forceArray.at(2).toDouble()));

            QVector<int> wanted;
            QString error;
            const QVector<moldata::Atom> atoms = viewer ? viewer->getCurrentFrameAtoms()
                                                        : control->currentAtoms();
            if (!resolveAtomSet(atoms, args.value(QStringLiteral("atoms")).toString(),
                    args.value(QStringLiteral("atom_indices")).toArray(), wanted, error)) {
                return ToolResult::failure(error);
            }

            if (!args.value(QStringLiteral("add")).toBool())
                pulls->clear();
            for (int index : wanted)
                pulls->insert(index, force);

            QVector<int> indices;
            QVector<QVector3D> forces;
            indices.reserve(pulls->size());
            forces.reserve(pulls->size());
            for (auto it = pulls->constBegin(); it != pulls->constEnd(); ++it) {
                indices.append(it.key());
                forces.append(it.value());
            }
            const double alpha = args.contains(QStringLiteral("alpha"))
                ? args.value(QStringLiteral("alpha")).toDouble() : 0.4;
            // Spreading through the bond graph is what makes a single-atom mouse grab
            // move a molecule instead of tearing one atom off. Applied to a whole
            // fragment it does the opposite: every atom's pull also leaks onto its
            // neighbours, which are being pulled themselves, so a 27-atom selection
            // ends up with several times the force asked for. Default it off unless
            // exactly one atom was named.
            const int defaultShells = indices.size() == 1 ? 3 : 0;
            const int shells = args.contains(QStringLiteral("max_shells"))
                ? args.value(QStringLiteral("max_shells")).toInt() : defaultShells;
            control->requestExternalForces(indices, forces, alpha, shells);

            QJsonArray pulled;
            for (int index : indices)
                pulled.append(index);

            QJsonObject data;
            data.insert(QStringLiteral("pulled_now"), wanted.size());
            data.insert(QStringLiteral("pulled_total"), indices.size());
            data.insert(QStringLiteral("force"), forceArray);
            data.insert(QStringLiteral("max_shells"), shells);
            // The indices, so the same set can be followed afterwards: fragment
            // numbering is geometric and moves when the structure does, but an index
            // list does not.
            data.insert(QStringLiteral("atom_indices"), pulled);
            return ToolResult::success(data,
                QStringLiteral("Pulling on %1 atom(s) in total; it acts from the next step until "
                               "clear_forces. Follow it with watch_simulation -- pass these "
                               "atom_indices rather than \"F2\", because pulling the parts "
                               "apart is exactly what renumbers the fragments.")
                    .arg(indices.size()));
        };
        add(spec);

        ToolSpec clear;
        clear.name = QStringLiteral("clear_forces");
        clear.category = QStringLiteral("simulation");
        clear.description = QStringLiteral(
            "Drop every pull. The run carries on without the bias.");
        clear.effect = ToolEffect::Compute;
        clear.affinity = ToolAffinity::Gui;
        clear.paramSchema = schema(R"JSON({ "type": "object", "properties": {} })JSON");
        clear.handler = [control, pulls](const QJsonObject&) {
            const int had = pulls->size();
            pulls->clear();
            control->clearExternalForces();
            return ToolResult::success({}, had == 0
                    ? QStringLiteral("There was nothing to clear.")
                    : QStringLiteral("Dropped the pulls on %1 atom(s).").arg(had));
        };
        add(clear);
    }

    // --- watch_simulation ---------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("watch_simulation");
        spec.category = QStringLiteral("simulation");
        spec.description = QStringLiteral(
            "Measure the running simulation while it keeps going. Three ways to use it: ask "
            "once and get the value now; give below or above and it returns when the quantity "
            "crosses it (\"run until the guest is within 3 A of the host\"); give every_steps "
            "and it reports back regularly, returning a trace of the value against step rather "
            "than a single number, which is how a run is followed rather than sampled. The two "
            "can be combined -- it then returns on whichever comes first. Every answer also "
            "carries the fragment picture: how many there are, how big they are, how close they "
            "come.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;   // it waits, so it must not be the GUI thread
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "quantity": { "type": "string",
                          "enum": ["min_distance", "centroid_distance", "gyration_radius",
                                   "rmsd_to_start", "energy", "temperature", "step", "density"],
                          "description": "what to measure on every frame. step only ever rises, so wait on it with above, never below; to wait for a run to end, use simulation_status with wait_seconds instead. density is the mass in the confinement container over its volume, so it needs a wall to be set -- in a fixed container with a fixed number of molecules it does not change during a run, and its use is checking a packing before starting one (liquid water is 1.00 g/cm^3)" },
            "atoms":    { "type": "string",
                          "description": "first selection, e.g. \"F1\"; needed by the geometric quantities" },
            "atoms_b":  { "type": "string",
                          "description": "second selection, for min_distance and centroid_distance" },
            "atom_indices":   { "type": "array",
                                "description": "explicit 0-based indices instead of atoms -- use these to follow a fixed set, since fragment numbering is geometric and changes as the structure does" },
            "atom_indices_b": { "type": "array", "description": "explicit indices instead of atoms_b" },
            "below":    { "type": "number", "description": "return once the quantity drops under this" },
            "above":    { "type": "number", "description": "return once the quantity rises over this" },
            "every_steps": { "type": "integer", "minimum": 1, "maximum": 100000,
                             "description": "sample the quantity at least this many steps apart and return the trace" },
            "max_samples": { "type": "integer", "minimum": 1, "maximum": 100,
                             "description": "how many samples to collect before returning (default 20)" },
            "wait_seconds": { "type": "integer", "minimum": 0, "maximum": 600,
                              "description": "how long to wait for that (default 30 when a trace or a threshold was asked for, else 0). One long wait costs one round; asking again every minute costs one each time." }
          },
          "required": ["quantity"]
        })JSON");

        spec.handler = [cache, dispatcher](const QJsonObject& args) {
            const QString quantity = args.value(QStringLiteral("quantity")).toString();
            const bool hasBelow = args.contains(QStringLiteral("below"));
            const bool hasAbove = args.contains(QStringLiteral("above"));
            const double below = args.value(QStringLiteral("below")).toDouble();
            const double above = args.value(QStringLiteral("above")).toDouble();
            const int everySteps = qBound(0, args.value(QStringLiteral("every_steps")).toInt(), 100000);
            const bool wantsToWait = everySteps > 0
                || args.contains(QStringLiteral("below")) || args.contains(QStringLiteral("above"));
            // Asking for a trace, or for a threshold, with no time to wait in is a
            // request that cannot be answered. Default to a useful span there and to
            // "answer now" for a plain read.
            const int waitSeconds = qBound(0,
                args.value(QStringLiteral("wait_seconds")).toInt(wantsToWait ? 30 : 0), 600);
            const int maxSamples = qBound(1, args.value(QStringLiteral("max_samples")).toInt(20), 100);

            QElapsedTimer clock;
            clock.start();
            QMutexLocker lock(&cache->mutex);

            double value = 0.0;
            QString unit;
            bool met = false;
            bool ran = false;
            QJsonArray trace;          // step/value pairs when every_steps was asked for
            int lastSampled = 0;
            bool sampledOnce = false;
            forever {
                if (cache->atoms.isEmpty()) {
                    if (!cache->state.running)
                        return ToolResult::failure(QStringLiteral(
                            "nothing is running, and no frame has arrived to measure"));
                    // Running but no frame yet: a run needs a moment to produce its
                    // first one, and returning "cannot measure" here would have the
                    // caller abandon the run it just started.
                } else {
                    QVector<int> setA;
                    QVector<int> setB;
                    QString error;
                    const QString a = args.value(QStringLiteral("atoms")).toString();
                    const QString b = args.value(QStringLiteral("atoms_b")).toString();
                    const QJsonArray indicesA = args.value(QStringLiteral("atom_indices")).toArray();
                    const QJsonArray indicesB = args.value(QStringLiteral("atom_indices_b")).toArray();

                    // A selection is re-resolved on the current frame, and fragment
                    // perception is geometric: pulling two parts apart is precisely
                    // what makes "F2" stop meaning what it meant. Say so instead of
                    // reporting an empty match and leaving the model to guess.
                    const auto resolve = [&](const QString& expression, const QJsonArray& explicitIndices,
                                             QVector<int>& target, const char* which) -> bool {
                        if (expression.isEmpty() && explicitIndices.isEmpty())
                            return true;
                        if (resolveAtomSet(cache->atoms, expression, explicitIndices, target, error))
                            return true;
                        if (!expression.isEmpty() && expression.startsWith(QLatin1Char('F'))) {
                            error += QStringLiteral(" -- %1 is resolved again on the current frame, "
                                                    "and fragments are perceived from the geometry, "
                                                    "so they renumber as the structure moves. Pass "
                                                    "%2 to follow a fixed set.")
                                         .arg(expression, QLatin1String(which));
                        }
                        return false;
                    };
                    if (!resolve(a, indicesA, setA, "atom_indices"))
                        return ToolResult::failure(error);
                    if (!resolve(b, indicesB, setB, "atom_indices_b"))
                        return ToolResult::failure(error);

                    bool ok = false;
                    value = evaluate(quantity, *cache, setA, setB, unit, ok);
                    if (!ok) {
                        return ToolResult::failure(QStringLiteral(
                            "\"%1\" cannot be measured with what was given -- the geometric "
                            "quantities need atoms, and the two-set ones need atoms_b as well")
                                .arg(quantity));
                    }
                    ran = true;
                    met = (hasBelow && value < below) || (hasAbove && value > above);

                    // The waiter is woken on every frame, so every_steps is a
                    // minimum spacing rather than an exact stride: a sample is
                    // taken at the first frame that is far enough from the last.
                    if (everySteps > 0
                        && (!sampledOnce || cache->state.step - lastSampled >= everySteps)) {
                        QJsonObject sample;
                        sample.insert(QStringLiteral("step"), cache->state.step);
                        sample.insert(QStringLiteral("value"), value);
                        trace.append(sample);
                        lastSampled = cache->state.step;
                        sampledOnce = true;
                    }
                }

                if (met)
                    break;
                if (everySteps > 0) {
                    if (trace.size() >= maxSamples)
                        break;
                } else if (!hasBelow && !hasAbove) {
                    break;   // a plain read: the value now, no waiting
                }
                if (!cache->state.running)
                    break;   // the run ended first
                if (dispatcher && dispatcher->isInterrupted())
                    break;
                const qint64 left = qint64(waitSeconds) * 1000 - clock.elapsed();
                if (left <= 0)
                    break;
                cache->changed.wait(&cache->mutex, qMin<qint64>(left, 500));
            }

            QJsonObject data = stateToJson(cache->state);
            data.insert(QStringLiteral("quantity"), quantity);
            if (ran) {
                data.insert(QStringLiteral("value"), value);
                if (!unit.isEmpty())
                    data.insert(QStringLiteral("unit"), unit);
            }
            if (hasBelow || hasAbove)
                data.insert(QStringLiteral("condition_met"), met);
            if (!trace.isEmpty()) {
                data.insert(QStringLiteral("trace"), trace);
                data.insert(QStringLiteral("samples"), trace.size());
            }
            const QJsonObject fragments = fragmentSummary(cache->atoms);
            if (!fragments.isEmpty())
                data.insert(QStringLiteral("fragments"), fragments);
            const bool running = cache->state.running;
            lock.unlock();

            QString note = ran
                ? QStringLiteral("%1 = %2 %3").arg(quantity)
                      .arg(value, 0, 'f', 4).arg(unit)
                : QStringLiteral("%1 could not be measured").arg(quantity);
            if (!trace.isEmpty()) {
                const QJsonObject first = trace.first().toObject();
                note += QStringLiteral(", %1 samples from step %2 to %3")
                            .arg(trace.size())
                            .arg(first.value(QStringLiteral("step")).toInt())
                            .arg(trace.last().toObject().value(QStringLiteral("step")).toInt());
            }
            if (hasBelow || hasAbove) {
                note += met ? QStringLiteral(" -- the threshold was reached.")
                            : running ? QStringLiteral(" -- not there yet within the time given.")
                                      : QStringLiteral(" -- the run ended before it got there.");
            } else if (!trace.isEmpty() && !running) {
                note += QStringLiteral(" -- the run ended.");
            }
            return ToolResult::success(data, note);
        };
        add(spec);
    }

    return added;
}
