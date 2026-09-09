// tools_simulation.cpp - Running the interactive MD and geometry optimisation.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_simulation.h"

#include "atomselection.h"
#include "core/toolregistry.h"
#include "measurements.h"
#include "moleculebridge.h"
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

    MoleculeViewer* const viewer = context.viewer;

    auto cache = std::make_shared<StatusCache>();
    QObject::connect(control, &SimulationControlWidget::liveStateChanged, control,
        [cache](const SimulationControlWidget::LiveState& state) {
            {
                QMutexLocker lock(&cache->mutex);
                cache->state = state;
                if (!state.running)
                    cache->start.clear();   // the next run gets its own reference
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
                              "description": "[fx, fy, fz] in Eh/Bohr, applied to each atom of the selection" },
            "add":          { "type": "boolean",
                              "description": "keep the pulls already set (default false: replace them)" },
            "alpha":        { "type": "number", "minimum": 0, "maximum": 1,
                              "description": "how much of the force reaches the next bonded shell (default 0.4)" },
            "max_shells":   { "type": "integer", "minimum": 0, "maximum": 10,
                              "description": "how many bonded shells it spreads through (default 3)" }
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
            const int shells = args.contains(QStringLiteral("max_shells"))
                ? args.value(QStringLiteral("max_shells")).toInt() : 3;
            control->requestExternalForces(indices, forces, alpha, shells);

            QJsonObject data;
            data.insert(QStringLiteral("pulled_now"), wanted.size());
            data.insert(QStringLiteral("pulled_total"), indices.size());
            data.insert(QStringLiteral("force"), forceArray);
            return ToolResult::success(data,
                QStringLiteral("Pulling on %1 atom(s) in total; it acts from the next step until "
                               "clear_forces. Watch what it does with watch_simulation.")
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
            "Measure the running simulation, and wait for it to reach something. Give below or "
            "above and it returns when the quantity crosses it -- so \"run until the guest is "
            "within 3 A of the host\" is one call, not a poll every second. Without a threshold "
            "it answers straight away. Every answer also carries the fragment picture: how many "
            "there are, how big they are, how close they come.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;   // it waits, so it must not be the GUI thread
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "quantity": { "type": "string",
                          "enum": ["min_distance", "centroid_distance", "gyration_radius",
                                   "rmsd_to_start", "energy", "temperature", "step"],
                          "description": "what to measure on every frame" },
            "atoms":    { "type": "string",
                          "description": "first selection, e.g. \"F1\"; needed by the geometric quantities" },
            "atoms_b":  { "type": "string",
                          "description": "second selection, for min_distance and centroid_distance" },
            "below":    { "type": "number", "description": "return once the quantity drops under this" },
            "above":    { "type": "number", "description": "return once the quantity rises over this" },
            "wait_seconds": { "type": "integer", "minimum": 0, "maximum": 60,
                              "description": "how long to wait for that (default 0: answer at once)" }
          },
          "required": ["quantity"]
        })JSON");

        spec.handler = [cache](const QJsonObject& args) {
            const QString quantity = args.value(QStringLiteral("quantity")).toString();
            const bool hasBelow = args.contains(QStringLiteral("below"));
            const bool hasAbove = args.contains(QStringLiteral("above"));
            const double below = args.value(QStringLiteral("below")).toDouble();
            const double above = args.value(QStringLiteral("above")).toDouble();
            const int waitSeconds = qBound(0, args.value(QStringLiteral("wait_seconds")).toInt(), 60);

            QElapsedTimer clock;
            clock.start();
            QMutexLocker lock(&cache->mutex);

            double value = 0.0;
            QString unit;
            bool met = false;
            bool ran = false;
            forever {
                if (cache->atoms.isEmpty()) {
                    if (!cache->state.running)
                        return ToolResult::failure(QStringLiteral(
                            "nothing is running, and no frame has arrived to measure"));
                } else {
                    QVector<int> setA;
                    QVector<int> setB;
                    QString error;
                    const QString a = args.value(QStringLiteral("atoms")).toString();
                    const QString b = args.value(QStringLiteral("atoms_b")).toString();
                    if (!a.isEmpty() && !resolveAtomSet(cache->atoms, a, {}, setA, error))
                        return ToolResult::failure(error);
                    if (!b.isEmpty() && !resolveAtomSet(cache->atoms, b, {}, setB, error))
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
                }

                if (met || (!hasBelow && !hasAbove))
                    break;
                if (!cache->state.running)
                    break;   // the run ended before the threshold was reached
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
            const QJsonObject fragments = fragmentSummary(cache->atoms);
            if (!fragments.isEmpty())
                data.insert(QStringLiteral("fragments"), fragments);
            const bool running = cache->state.running;
            lock.unlock();

            QString note = ran
                ? QStringLiteral("%1 = %2 %3").arg(quantity)
                      .arg(value, 0, 'f', 4).arg(unit)
                : QStringLiteral("%1 could not be measured").arg(quantity);
            if (hasBelow || hasAbove) {
                note += met ? QStringLiteral(" -- the threshold was reached.")
                            : running ? QStringLiteral(" -- not there yet within the time given.")
                                      : QStringLiteral(" -- the run ended before it got there.");
            }
            return ToolResult::success(data, note);
        };
        add(spec);
    }

    return added;
}
