// tools_simulation.cpp - Running the interactive MD and geometry optimisation.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_simulation.h"

#include "atomselection.h"
#include "core/tooldispatcher.h"
#include "core/toolregistry.h"
#include "measurements.h"
#include "moleculebridge.h"
#include "simulationcontrolwidget.h"
#include "simulationframe.h"
#include "view.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QMutex>
#include <QThread>
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
    if (quantity == QLatin1String("external_work")) {
        unit = QStringLiteral("Eh");
        return cache.state.externalWork;
    }
    if (quantity == QLatin1String("density")) {
        unit = QStringLiteral("g/cm^3");
        if (cache.state.containerVolume <= 0.0 || cache.atoms.isEmpty()) {
            ok = false;
            return 0.0;
        }
        // curcuma's own Molecule::Density: the masses, the defensive minimum for
        // coarse-grained particles and the unit conversion all live there, so the
        // number here and the one SimpleMD reports cannot come apart.
        return atomsToMolecule(cache.atoms).Density(cache.state.containerVolume);
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
    if (state.externalWork != 0.0) {
        // What the configured pulls have put into the system. This is the number a
        // Jarzynski or Crooks estimate is built from, not the energy.
        o.insert(QStringLiteral("external_work"), state.externalWork);
        o.insert(QStringLiteral("external_work_unit"), QStringLiteral("Eh"));
    }
    return o;
}

/// The config a run starts from: the dock's current settings with whatever the
/// model named written over them. Exposed are the run parameters plus the MD
/// integration settings (RATTLE, hydrogen mass, GFN-FF topology, GPU), which
/// run_simulation reports back; walls, ramps and metadynamics stay where the
/// operator set them.
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

    // MD integration settings from the dock's RATTLE / hydrogen-mass / topology /
    // GPU controls. The rattle names map onto the combo's data values 0/1/2.
    // Claude Generated 2026.
    const QString rattle = args.value(QStringLiteral("rattle")).toString();
    if (rattle == QLatin1String("off"))
        cfg.rattleMode = 0;
    else if (rattle == QLatin1String("all"))
        cfg.rattleMode = 1;
    else if (rattle == QLatin1String("h_only"))
        cfg.rattleMode = 2;
    if (args.value(QStringLiteral("rattle_angles")).isBool())
        cfg.rattle13 = args.value(QStringLiteral("rattle_angles")).toBool();
    takeDouble("hydrogen_mass", cfg.hmass);
    takeString("topology", cfg.topologyMode);
    takeString("gpu", cfg.gpu);
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
                             "description": "opt only: keep restarting the optimiser instead of stopping when it converges. Needed only to pull on atoms against it; it never ends by itself, so stop_simulation is the only way out (default false)" },
            "rattle":      { "type": "string", "enum": ["off", "all", "h_only"],
                             "description": "md only: RATTLE bond-length constraints. 'h_only' freezes only X-H bonds (the fastest vibrations), 'all' every bond; either allows a larger timestep (about 2 fs instead of 0.5-1). Not combinable with topology 'react'" },
            "rattle_angles": { "type": "boolean",
                             "description": "md only, with rattle: also constrain 1-3 distances, i.e. freeze bond angles" },
            "hydrogen_mass": { "type": "number", "minimum": 1, "maximum": 5,
                             "description": "md only: hydrogen mass in amu (1 = physical). 2-3 slows the X-H vibrations so a larger timestep stays stable; dynamics are no longer physical in time" },
            "topology":    { "type": "string",
                             "description": "md with gfnff only: 'auto' rebuilds the force-field topology when needed, 'constant' keeps the initial one, 'react' lets bonds form and break" },
            "gpu":         { "type": "string",
                             "description": "compute backend; only backends that actually load on this machine are listed" }
          },
          "required": ["mode"]
        })JSON");
        restrictTo(spec.paramSchema, "method", control->methodValues());
        restrictTo(spec.paramSchema, "thermostat", control->thermostatValues());
        restrictTo(spec.paramSchema, "optimizer", control->optimizerValues());
        restrictTo(spec.paramSchema, "topology", control->topologyValues());
        restrictTo(spec.paramSchema, "gpu", control->gpuValues());

        spec.handler = [control](const QJsonObject& args) {
            const SimulationConfig cfg = configFromArgs(control->currentConfig(), args);
            // The dock drops RATTLE silently for a reactive GFN-FF run (curcuma refuses
            // the combination), so say so instead of starting something else.
            if (cfg.mode == SimulationConfig::Mode::MolecularDynamics && cfg.rattleMode != 0
                && cfg.method == QLatin1String("gfnff") && cfg.topologyMode == QLatin1String("react")) {
                return ToolResult::failure(QStringLiteral(
                    "rattle cannot be combined with topology 'react': constrained bonds cannot "
                    "break. Set rattle 'off' or choose topology 'auto'/'constant'."));
            }
            QString error;
            if (!control->startRun(cfg, &error))
                return ToolResult::failure(error);

            const bool md = cfg.mode == SimulationConfig::Mode::MolecularDynamics;
            QJsonObject data;
            data.insert(QStringLiteral("status"), QStringLiteral("started"));
            data.insert(QStringLiteral("mode"), md ? QStringLiteral("md") : QStringLiteral("opt"));
            data.insert(QStringLiteral("method"), cfg.method);
            data.insert(QStringLiteral("steps"), cfg.steps);
            // Read back from the dock, so the model sees what it runs with -- including
            // what it did not name and what the dock kept standing.
            const SimulationConfig effective = control->currentConfig();
            static const char* const rattleNames[] = { "off", "all", "h_only" };
            data.insert(QStringLiteral("gpu"), effective.gpu);
            if (md) {
                data.insert(QStringLiteral("temperature"), cfg.temperature);
                data.insert(QStringLiteral("timestep_fs"), cfg.timestep);
                data.insert(QStringLiteral("rattle"),
                    QLatin1String(rattleNames[qBound(0, effective.rattleMode, 2)]));
                if (effective.rattleMode != 0)
                    data.insert(QStringLiteral("rattle_angles"), effective.rattle13);
                data.insert(QStringLiteral("hydrogen_mass"), effective.hmass);
                if (effective.method == QLatin1String("gfnff"))
                    data.insert(QStringLiteral("topology"), effective.topologyMode);
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
        // Only while something is running: pausing, resuming, stopping and stepping
        // can do nothing otherwise, and a tool that can only refuse still costs its
        // schema in every request. Claude Generated 2026.
        spec.available = [control] { return control && control->isRunning(); };
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

        spec.available = [control] {
            return control && control->isRunning()
                && control->liveState().mode == QLatin1String("md");
        };
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
        // Keyed by label so a second call replaces the same potential rather than
        // stacking another copy of it.
        auto pulls = std::make_shared<QMap<QString, QJsonObject>>();

        ToolSpec spec;
        spec.name = QStringLiteral("pull_atoms");
        spec.category = QStringLiteral("simulation");
        spec.description = QStringLiteral(
            "Pull on atoms of the running simulation. The force is added to the gradient of "
            "every following step and stays until clear_forces, so the structure answers with "
            "its own forces instead of being teleported. It stands in the run's own "
            "configuration, so the run can be reproduced from it, and the work it does is "
            "accumulated. Call it again with add=true to pull on another set at the same time.");
        spec.effect = ToolEffect::Compute;
        spec.affinity = ToolAffinity::Gui;
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "atoms":  { "type": "string",
                        "description": "selection grammar, e.g. \"F2\" for the second fragment. Re-resolved by the engine on the live geometry" },
            "force":  { "type": "array",
                        "description": "[fx, fy, fz] in Eh/Angstrom, applied to EACH atom of the selection, so a 27-atom fragment feels 27 times this in total" },
            "label":  { "type": "string",
                        "description": "name it, so a later call with the same label replaces this pull instead of adding another" },
            "add":    { "type": "boolean",
                        "description": "keep the potentials already set (default false: replace them)" }
          },
          "required": ["atoms", "force"]
        })JSON");

        spec.available = [control] { return control && control->isRunning(); };
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

            const QString expression = args.value(QStringLiteral("atoms")).toString();

            // A pre-check, not the resolution that counts. curcuma resolves the
            // selection itself when it takes the potential, and it is the authority;
            // this only catches an obviously wrong expression here and now, because
            // otherwise the refusal comes back asynchronously from the worker and
            // the model has moved on. The count below is therefore "as of the frame
            // on screen".
            QVector<int> wanted;
            QString error;
            const QVector<moldata::Atom> atoms = viewer ? viewer->getCurrentFrameAtoms()
                                                        : control->currentAtoms();
            if (!expression.isEmpty()
                && !resolveAtomSet(atoms, expression, {}, wanted, error)) {
                return ToolResult::failure(error);
            }
            if (expression.isEmpty()) {
                return ToolResult::failure(QStringLiteral(
                    "a configured pull is named by a selection: pass atoms, e.g. \"F2\""));
            }

            // A declarative potential, not an injection: it stands in the run's own
            // configuration, so the run can be reproduced from it, and curcuma
            // accumulates the work it does. curcuma resolves the selection itself
            // against the live geometry.
            QJsonObject entry;
            entry.insert(QStringLiteral("kind"), QStringLiteral("constant_force"));
            entry.insert(QStringLiteral("atoms"), expression);
            entry.insert(QStringLiteral("label"),
                args.value(QStringLiteral("label")).toString(
                    QStringLiteral("pull on %1").arg(expression)));
            const double length = force.length();
            entry.insert(QStringLiteral("direction"), QJsonArray {
                force.x() / length, force.y() / length, force.z() / length });
            entry.insert(QStringLiteral("magnitude"), length);

            if (!args.value(QStringLiteral("add")).toBool())
                pulls->clear();
            pulls->insert(entry.value(QStringLiteral("label")).toString(), entry);

            QJsonArray list;
            for (auto it = pulls->constBegin(); it != pulls->constEnd(); ++it)
                list.append(it.value());
            control->requestExternalPotentials(list);

            QJsonObject data;
            data.insert(QStringLiteral("kind"), QStringLiteral("constant_force"));
            data.insert(QStringLiteral("atoms"), expression);
            data.insert(QStringLiteral("atom_count"), wanted.size());
            data.insert(QStringLiteral("magnitude"), length);
            data.insert(QStringLiteral("active_potentials"), list.size());
            return ToolResult::success(data,
                QStringLiteral("Pulling on %1 (%2 atoms) with %3 Eh/A per atom; %4 potential(s) "
                               "active. It acts from the next step until clear_forces, and the "
                               "work it does is accumulated -- simulation_status reports it.")
                    .arg(expression).arg(wanted.size()).arg(length).arg(list.size()));
        };
        add(spec);

        // --- restrain_atoms -------------------------------------------------
        //
        // One tool for both harmonic forms rather than two, because every tool is
        // paid for in the catalogue on every turn.
        ToolSpec restrain;
        restrain.name = QStringLiteral("restrain_atoms");
        restrain.category = QStringLiteral("simulation");
        restrain.description = QStringLiteral(
            "Hold part of the running structure with a harmonic restraint, instead of pushing "
            "it with a constant force. \"point\" ties a selection's centroid to a place -- that "
            "is how a guest is held in a cavity while the rest relaxes around it. \"distance\" "
            "ties two selections' centroids to a separation, which draws them together or keeps "
            "them apart. Both stay until clear_forces.");
        restrain.effect = ToolEffect::Compute;
        restrain.affinity = ToolAffinity::Gui;
        restrain.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "kind":     { "type": "string", "enum": ["point", "distance"],
                          "description": "restrain the centroid to a point, or two centroids to a distance" },
            "atoms":    { "type": "string", "description": "selection grammar, for example F2" },
            "atoms_b":  { "type": "string", "description": "second selection, for kind=distance" },
            "target":   { "type": "array", "description": "[x, y, z] in Angstrom, for kind=point" },
            "distance": { "type": "number", "minimum": 0, "maximum": 1000,
                          "description": "the separation to hold, in Angstrom, for kind=distance" },
            "k":        { "type": "number", "minimum": 0, "maximum": 1000,
                          "description": "force constant in Eh/Angstrom^2 (default 1)" },
            "label":    { "type": "string",
                          "description": "name it, so a later call with the same label replaces this restraint" },
            "add":      { "type": "boolean",
                          "description": "keep the potentials already set (default true here: a restraint usually joins a pull rather than replacing it)" }
          },
          "required": ["kind", "atoms"]
        })JSON");

        restrain.available = [control] { return control && control->isRunning(); };
        restrain.handler = [control, pulls](const QJsonObject& args) {
            if (!control->isRunning()) {
                return ToolResult::failure(QStringLiteral(
                    "nothing is running: a restraint acts on a calculation in progress. Start "
                    "one with run_simulation."));
            }
            const QString kind = args.value(QStringLiteral("kind")).toString();
            const QString expression = args.value(QStringLiteral("atoms")).toString();
            if (expression.isEmpty())
                return ToolResult::failure(QStringLiteral("pass atoms, e.g. \"F2\""));

            QJsonObject entry;
            entry.insert(QStringLiteral("atoms"), expression);
            entry.insert(QStringLiteral("k"), args.value(QStringLiteral("k")).toDouble(1.0));

            QString what;
            if (kind == QLatin1String("point")) {
                const QJsonArray target = args.value(QStringLiteral("target")).toArray();
                if (target.size() != 3)
                    return ToolResult::failure(QStringLiteral("kind=point needs target as [x, y, z]"));
                entry.insert(QStringLiteral("kind"), QStringLiteral("centroid_harmonic"));
                entry.insert(QStringLiteral("target"), target);
                what = QStringLiteral("%1 held at (%2, %3, %4)").arg(expression)
                           .arg(target.at(0).toDouble()).arg(target.at(1).toDouble())
                           .arg(target.at(2).toDouble());
            } else {
                const QString other = args.value(QStringLiteral("atoms_b")).toString();
                if (other.isEmpty())
                    return ToolResult::failure(QStringLiteral("kind=distance needs atoms_b"));
                if (!args.contains(QStringLiteral("distance")))
                    return ToolResult::failure(QStringLiteral("kind=distance needs distance"));
                entry.insert(QStringLiteral("kind"), QStringLiteral("distance_harmonic"));
                entry.insert(QStringLiteral("atoms_b"), other);
                entry.insert(QStringLiteral("r0"), args.value(QStringLiteral("distance")).toDouble());
                what = QStringLiteral("%1 and %2 held at %3 A").arg(expression, other)
                           .arg(args.value(QStringLiteral("distance")).toDouble());
            }
            const QString label = args.value(QStringLiteral("label"))
                                      .toString(QStringLiteral("restrain %1").arg(expression));
            entry.insert(QStringLiteral("label"), label);

            if (args.contains(QStringLiteral("add"))
                && !args.value(QStringLiteral("add")).toBool()) {
                pulls->clear();
            }
            pulls->insert(label, entry);

            QJsonArray list;
            for (auto it = pulls->constBegin(); it != pulls->constEnd(); ++it)
                list.append(it.value());
            control->requestExternalPotentials(list);

            QJsonObject data;
            data.insert(QStringLiteral("kind"), entry.value(QStringLiteral("kind")));
            data.insert(QStringLiteral("label"), label);
            data.insert(QStringLiteral("active_potentials"), list.size());
            return ToolResult::success(data,
                QStringLiteral("%1 with k = %2 Eh/A^2; %3 potential(s) active. It acts from the "
                               "next step until clear_forces.")
                    .arg(what).arg(entry.value(QStringLiteral("k")).toDouble()).arg(list.size()));
        };
        add(restrain);

        ToolSpec clear;
        clear.name = QStringLiteral("clear_forces");
        clear.category = QStringLiteral("simulation");
        clear.description = QStringLiteral(
            "Drop every pull. The run carries on without the bias.");
        clear.effect = ToolEffect::Compute;
        clear.affinity = ToolAffinity::Gui;
        clear.paramSchema = schema(R"JSON({ "type": "object", "properties": {} })JSON");
        clear.available = [control] { return control && control->isRunning(); };
        clear.handler = [control, pulls](const QJsonObject&) {
            const int had = pulls->size();
            pulls->clear();
            control->requestExternalPotentials(QJsonArray());
            control->clearExternalForces();   // and any transient injection from the mouse
            return ToolResult::success({}, had == 0
                    ? QStringLiteral("There was nothing to clear.")
                    : QStringLiteral("Dropped %1 potential(s).").arg(had));
        };
        add(clear);
    }

    // --- watch_simulation ---------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("watch_simulation");
        spec.category = QStringLiteral("simulation");
        spec.description = QStringLiteral(
            "Measure the running simulation while it keeps going. Without a threshold it "
            "answers now; with below/above it returns when the quantity crosses it; with "
            "every_steps it returns a trace of value against step. Every answer also carries "
            "the fragment picture (count, sizes, closest approaches).");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;   // it waits, so it must not be the GUI thread
        spec.paramSchema = schema(R"JSON({
          "type": "object",
          "properties": {
            "quantity": { "type": "string",
                          "enum": ["min_distance", "centroid_distance", "gyration_radius",
                                   "rmsd_to_start", "energy", "temperature", "step", "density", "external_work"],
                          "description": "step only rises, so wait on it with above. density needs a wall and is constant in a fixed box (liquid water is 1.00 g/cm^3). With nothing running every quantity is measured on the current scene and container. To wait for a run to end use simulation_status" },
            "atoms":    { "type": "string", "description": "selection, e.g. F1; the geometric quantities need it" },
            "atoms_b":  { "type": "string", "description": "second selection, for the two-set quantities" },
            "atom_indices":   { "type": "array",
                                "description": "explicit 0-based indices instead of atoms; use these to follow a fixed set, since fragment numbering moves as the structure does" },
            "atom_indices_b": { "type": "array", "description": "explicit indices instead of atoms_b" },
            "below":    { "type": "number", "description": "return once it drops under this" },
            "above":    { "type": "number", "description": "return once it rises over this" },
            "every_steps": { "type": "integer", "minimum": 1, "maximum": 100000,
                             "description": "sample at least this many steps apart and return the trace" },
            "max_samples": { "type": "integer", "minimum": 1, "maximum": 100,
                             "description": "samples before returning (default 20)" },
            "wait_seconds": { "type": "integer", "minimum": 0, "maximum": 600,
                              "description": "how long to wait (default 30 with a trace or threshold). One long wait is one round; asking again every minute is one each time" }
          },
          "required": ["quantity"]
        })JSON");

        spec.handler = [cache, dispatcher, control, viewer](const QJsonObject& args) {
            const QString quantity = args.value(QStringLiteral("quantity")).toString();

            // With nothing running, measure the scene as it is now, not the last
            // frame of the last run: after an edit (a fill, a delete) those differ,
            // and the density went on reporting the old box. Scene and dock are GUI
            // objects, so they are read over there, before the cache lock is taken
            // -- the GUI thread takes that lock for every frame. Claude Generated 2026.
            bool idle = false;
            {
                QMutexLocker lock(&cache->mutex);
                idle = !cache->state.running;
            }
            if (idle && viewer) {
                QVector<moldata::Atom> scene;
                double volume = 0.0;
                const auto grab = [&scene, &volume, control, viewer] {
                    scene = viewer->getCurrentFrameAtoms();
                    volume = control->currentConfig().containerVolume();
                };
                if (QThread::currentThread() == control->thread())
                    grab();
                else
                    QMetaObject::invokeMethod(control, grab, Qt::BlockingQueuedConnection);
                QMutexLocker lock(&cache->mutex);
                if (!cache->state.running) {   // a run may have started meanwhile
                    cache->atoms = scene;
                    cache->state.containerVolume = volume;
                }
            }
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
                            "nothing is running and the scene is empty: nothing to measure"));
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
                    if (!ok && quantity == QLatin1String("density")) {
                        return ToolResult::failure(QStringLiteral(
                            "density needs a container with an explicit size, and none is set: "
                            "fill_container with set_wall, or the Confinement Walls group"));
                    }
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
