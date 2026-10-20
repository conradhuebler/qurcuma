// setupcheck.cpp - see setupcheck.h.
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026

#include "setupcheck.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <cmath>

namespace setupcheck {

namespace {

using S = State;

const QVector<Capability> kCapabilities = {
    { "md.nvt", S::Available,
        "MD with thermostat csvr, berendsen, andersen, nosehover or none (none = NVE); time step and "
        "temperature as run_simulation arguments. Driven from code and tests, not checked against a reference.",
        "qurcuma:src/llm/tools_simulation.cpp", "" },
    { "opt", S::Available, "Geometry optimisation with run_simulation mode=opt.",
        "qurcuma:src/llm/tools_simulation.cpp", "" },
    { "run.long", S::Partial,
        "run_simulation takes at most 1,000,000 steps per call; 30 ns at 1 fs is 30,000,000. Longer runs "
        "need segments (restart_file) or a job on another machine.",
        "qurcuma:src/llm/tools_simulation.cpp", "G" },
    { "md.temperature_schedule", S::Partial,
        "A temperature ramp exists in SimulationConfig and curcuma SimpleMD; run_simulation has no argument for it.",
        "qurcuma:src/simulationworker.h", "" },
    { "md.npt", S::Missing,
        "No barostat and no variable cell in SimpleMD; the Simulation dock has no pressure control.",
        "curcuma:src/capabilities/simplemd.h", "C" },
    { "md.periodic", S::Missing,
        "Non-bonded terms are not periodic. wall_potential=pbc only wraps molecules back into the box "
        "and does not make the interactions periodic.",
        "curcuma:src/capabilities/simplemd.h", "C" },
    { "md.longrange_electrostatics", S::Missing, "No Ewald, PME or PPPM.",
        "curcuma:src/capabilities/simplemd.h", "C" },
    { "ff.gfnff", S::Available, "Native GFN-FF; the force field chosen for this work.",
        "curcuma:src/core/energy_calculators/method_factory.cpp", "" },
    { "ff.pcff", S::Missing,
        "No PCFF and no atom typing for class II force fields; GFN-FF is used instead, so numbers differ from a PCFF paper.",
        "curcuma:src/core/energy_calculators/method_factory.cpp", "" },
    { "polymer.load", S::Available, "A finished polymer is loaded with open_structure.",
        "qurcuma:src/llm/tools_files.cpp", "" },
    { "polymer.build", S::Partial,
        "curcuma has the polymerbuild capability (sequence, fragments, caps); no qurcuma tool wraps it.",
        "curcuma:src/capabilities/polymerbuild.h", "E" },
    { "solvent.fill", S::Partial,
        "fill_container packs copies of a library molecule into a box at random orientations. It is not "
        "equilibrated and does not aim at a density; 9000 molecules were not tried.",
        "qurcuma:src/scenefiller.h", "" },
    { "ions.counter", S::Available,
        "add_counterions replaces solvent molecules by monovalent ions (curcuma ionize). Tested on a "
        "constructed water box only.",
        "qurcuma:src/llm/tools_edit.cpp", "" },
    { "analysis.rg", S::Available,
        "analyze_trajectory computes the radius of gyration of a selection over a trajectory file, streamed "
        "frame by frame, with a block error. Tested on constructed geometry and a generated 28,000-atom "
        "file; real MD trajectories not tried; no unwrapping across periodic boundaries.",
        "qurcuma:src/llm/tools_compute.cpp", "" },
    { "analysis.rdf", S::Partial, "RDF exists in the curcuma Analysis capability; no tool.",
        "curcuma:src/capabilities/analysis.h", "D" },
    { "analysis.hbonds", S::Partial, "Hydrogen-bond detection exists in curcuma; no tool.",
        "curcuma:src/core/hbonds.h", "D" },
    { "analysis.wasa", S::Missing, "No water-accessible surface area output (only a GBSA grid in solvation).",
        "curcuma:src/core/solvation/lebedev_grid.h", "D" },
    { "analysis.water_dynamics", S::Missing, "No water relaxation or residence-time analysis.",
        "curcuma:src/capabilities/analysis.h", "D" },
};

const QStringList kThermostats = { "csvr", "berendsen", "andersen", "nosehover", "none" };
const QStringList kMethods = { "gfnff", "gfnff-fast", "uff", "uff-d3", "qmdff", "gfn1", "gfn2" };
const QStringList kMissingForceFields = { "pcff", "cvff", "opls", "amber", "charmm" };
const QStringList kAnalysisIds = { "radius_of_gyration", "rdf", "hbonds", "wasa", "water_relaxation" };
const QStringList kStates = { "pending", "running", "done", "failed" };

QString analysisCapability(const QString& quantity)
{
    if (quantity == QLatin1String("radius_of_gyration"))
        return QStringLiteral("analysis.rg");
    if (quantity == QLatin1String("rdf"))
        return QStringLiteral("analysis.rdf");
    if (quantity == QLatin1String("hbonds"))
        return QStringLiteral("analysis.hbonds");
    if (quantity == QLatin1String("wasa"))
        return QStringLiteral("analysis.wasa");
    if (quantity == QLatin1String("water_relaxation"))
        return QStringLiteral("analysis.water_dynamics");
    return {};
}

QJsonObject blocker(const QString& id)
{
    const Capability* c = findCapability(id);
    QJsonObject o;
    o.insert(QStringLiteral("capability"), id);
    o.insert(QStringLiteral("reason"), c ? c->note : QStringLiteral("unknown capability"));
    o.insert(QStringLiteral("where"), c ? c->where : QString());
    o.insert(QStringLiteral("package"), c ? c->package : QString());
    return o;
}

/// Missing capability -> blocker; partial -> note; available -> nothing.
void need(const QString& id, QJsonArray& blockers, QJsonArray& notes)
{
    const Capability* c = findCapability(id);
    if (!c)
        return;
    if (c->state == State::Missing) {
        for (const QJsonValue& b : blockers)
            if (b.toObject().value(QStringLiteral("capability")).toString() == id)
                return;
        blockers.append(blocker(id));
    } else if (c->state == State::Partial) {
        notes.append(id + QStringLiteral(": ") + c->note);
    }
}

QJsonObject toolCall(const QString& tool, const QJsonObject& arguments, const QString& label = {})
{
    QJsonObject o;
    if (!label.isEmpty())
        o.insert(QStringLiteral("label"), label);
    o.insert(QStringLiteral("tool"), tool);
    o.insert(QStringLiteral("arguments"), arguments);
    return o;
}

} // namespace

const QVector<Capability>& capabilities()
{
    return kCapabilities;
}

const Capability* findCapability(const QString& id)
{
    for (const Capability& c : kCapabilities)
        if (c.id == id)
            return &c;
    return nullptr;
}

QString stateName(State state)
{
    switch (state) {
    case State::Available: return QStringLiteral("available");
    case State::Partial: return QStringLiteral("partial");
    case State::Missing: return QStringLiteral("missing");
    }
    return {};
}

QJsonObject checkProtocol(const QJsonObject& protocol)
{
    QJsonArray errors;
    QJsonArray prepare;
    QJsonArray stagesOut;
    QJsonArray analysisOut;

    const QJsonObject system = protocol.value(QStringLiteral("system")).toObject();
    const QJsonObject execution = protocol.value(QStringLiteral("execution")).toObject();
    const QString method = protocol.value(QStringLiteral("method")).toString(QStringLiteral("gfnff"));
    const bool periodic = system.value(QStringLiteral("periodic")).toBool(false);
    const QString electrostatics = system.value(QStringLiteral("electrostatics")).toString(QStringLiteral("cutoff"));
    const bool parallel = execution.value(QStringLiteral("parallel")).toBool(false);

    // --- system preparation ------------------------------------------------------
    const QString solute = system.value(QStringLiteral("solute_file")).toString();
    if (!solute.isEmpty()) {
        QJsonArray blockers, notes;
        need(QStringLiteral("polymer.load"), blockers, notes);
        QJsonObject call = toolCall(QStringLiteral("open_structure"), { { QStringLiteral("path"), solute } });
        prepare.append(call);
    }
    const int solventCount = system.value(QStringLiteral("solvent_count")).toInt(0);
    const QString solvent = system.value(QStringLiteral("solvent")).toString(QStringLiteral("H2O"));
    if (solventCount > 0) {
        QJsonArray blockers, notes;
        need(QStringLiteral("solvent.fill"), blockers, notes);
        QJsonObject args { { QStringLiteral("count"), solventCount },
            { QStringLiteral("set_wall"), true }, { QStringLiteral("wall_potential"), QStringLiteral("pbc") } };
        if (solvent == QLatin1String("H2O"))
            args.insert(QStringLiteral("molecule"), QStringLiteral("Water"));
        QJsonObject call = toolCall(QStringLiteral("fill_container"), args);
        call.insert(QStringLiteral("notes"), notes);
        if (system.contains(QStringLiteral("density_g_cm3"))) {
            QJsonArray n = notes;
            n.append(QStringLiteral("density_g_cm3 %1 is a target, fill_container does not aim at it: set the box with "
                                    "padding/radius and read the density from the answer")
                         .arg(system.value(QStringLiteral("density_g_cm3")).toDouble()));
            call.insert(QStringLiteral("notes"), n);
        }
        prepare.append(call);
    }
    if (system.contains(QStringLiteral("charge")) && system.value(QStringLiteral("charge")).toInt() != 0) {
        QJsonObject args { { QStringLiteral("charge"), system.value(QStringLiteral("charge")).toInt() } };
        const QJsonObject ions = system.value(QStringLiteral("ions")).toObject();
        if (ions.contains(QStringLiteral("anion")))
            args.insert(QStringLiteral("anion"), ions.value(QStringLiteral("anion")));
        if (ions.contains(QStringLiteral("cation")))
            args.insert(QStringLiteral("cation"), ions.value(QStringLiteral("cation")));
        if (solvent != QLatin1String("H2O"))
            args.insert(QStringLiteral("solvent"), solvent);
        prepare.append(toolCall(QStringLiteral("add_counterions"), args));
    }

    // --- method (blocks every stage) --------------------------------------------
    QJsonArray methodBlockers;
    QJsonArray methodNotes;
    if (kMissingForceFields.contains(method.toLower()))
        need(QStringLiteral("ff.pcff"), methodBlockers, methodNotes);
    else if (!kMethods.contains(method))
        errors.append(QStringLiteral("unknown method \"%1\"; known: %2").arg(method, kMethods.join(QStringLiteral(", "))));

    // --- stages ------------------------------------------------------------------
    const QJsonArray stages = protocol.value(QStringLiteral("stages")).toArray();
    if (stages.isEmpty())
        errors.append(QStringLiteral("the protocol has no stages"));
    QStringList seen;
    int ready = 0, blockedCount = 0, invalid = 0;
    for (const QJsonValue& value : stages) {
        const QJsonObject stage = value.toObject();
        const QString name = stage.value(QStringLiteral("name")).toString();
        const QString type = stage.value(QStringLiteral("type")).toString();
        QJsonArray blockers = methodBlockers;
        QJsonArray notes = methodNotes;
        QStringList problems;

        if (name.isEmpty())
            problems << QStringLiteral("a stage has no name");
        else if (seen.contains(name))
            problems << QStringLiteral("stage name \"%1\" is used twice").arg(name);
        seen << name;

        QJsonArray runs;
        if (type == QLatin1String("minimize")) {
            QJsonObject args { { QStringLiteral("mode"), QStringLiteral("opt") }, { QStringLiteral("method"), method } };
            runs.append(toolCall(QStringLiteral("run_simulation"), args));
        } else if (type == QLatin1String("md")) {
            const QString ensemble = stage.value(QStringLiteral("ensemble")).toString(QStringLiteral("nvt"));
            if (ensemble != QLatin1String("nvt") && ensemble != QLatin1String("nve") && ensemble != QLatin1String("npt"))
                problems << QStringLiteral("unknown ensemble \"%1\" (nvt, nve, npt)").arg(ensemble);
            if (ensemble == QLatin1String("npt"))
                need(QStringLiteral("md.npt"), blockers, notes);

            QString thermostat = stage.value(QStringLiteral("thermostat")).toString(
                ensemble == QLatin1String("nve") ? QStringLiteral("none") : QStringLiteral("csvr"));
            if (!kThermostats.contains(thermostat))
                problems << QStringLiteral("unknown thermostat \"%1\" (%2)").arg(thermostat, kThermostats.join(QStringLiteral(", ")));

            // temperature: number or list
            QVector<double> temperatures;
            const QJsonValue t = stage.value(QStringLiteral("temperature_K"));
            if (t.isArray()) {
                for (const QJsonValue& v : t.toArray())
                    temperatures.append(v.toDouble());
            } else if (t.isDouble()) {
                temperatures.append(t.toDouble());
            }
            if (temperatures.isEmpty() && ensemble != QLatin1String("nve"))
                problems << QStringLiteral("temperature_K is missing");
            if (temperatures.isEmpty() && ensemble == QLatin1String("nve"))
                temperatures.append(0.0);

            const double lengthPs = stage.value(QStringLiteral("length_ps")).toDouble(0.0);
            const double timestep = stage.value(QStringLiteral("timestep_fs")).toDouble(1.0);
            if (lengthPs <= 0.0)
                problems << QStringLiteral("length_ps is missing or not positive");
            if (timestep <= 0.0)
                problems << QStringLiteral("timestep_fs must be positive");

            // series: only meaningful with several temperatures
            QString series = stage.value(QStringLiteral("series")).toString(QStringLiteral("independent"));
            if (series != QLatin1String("independent") && series != QLatin1String("chained") && series != QLatin1String("ramp"))
                problems << QStringLiteral("unknown series \"%1\" (independent, chained, ramp)").arg(series);
            if (temperatures.size() > 1 && !stage.contains(QStringLiteral("series")))
                problems << QStringLiteral("several temperatures need \"series\": independent, chained or ramp -- "
                                           "that is a scientific statement the protocol has to make");
            if (parallel && (series == QLatin1String("chained") || series == QLatin1String("ramp")) && temperatures.size() > 1)
                problems << QStringLiteral("execution.parallel cannot run a %1 series: each run needs the end of the previous").arg(series);
            if (series == QLatin1String("ramp"))
                need(QStringLiteral("md.temperature_schedule"), blockers, notes);

            if (problems.isEmpty()) {
                const double steps = std::round(lengthPs * 1000.0 / timestep);
                for (double temperature : temperatures) {
                    QJsonObject args { { QStringLiteral("mode"), QStringLiteral("md") },
                        { QStringLiteral("method"), method },
                        { QStringLiteral("steps"), steps },
                        { QStringLiteral("timestep"), timestep },
                        { QStringLiteral("thermostat"), thermostat } };
                    if (ensemble != QLatin1String("nve"))
                        args.insert(QStringLiteral("temperature"), temperature);
                    runs.append(toolCall(QStringLiteral("run_simulation"), args,
                        temperatures.size() > 1 ? QStringLiteral("%1 K").arg(temperature) : QString()));
                }
                if (steps > 1000000.0)
                    notes.append(QStringLiteral("run.long: %1 steps per run exceed the 1,000,000 of one run_simulation call; "
                                                "run it in segments from a restart file or as a job on another machine")
                                     .arg(steps, 0, 'f', 0));
            }
        } else {
            problems << QStringLiteral("type must be \"minimize\" or \"md\", not \"%1\"").arg(type);
        }

        // Whatever the type, a periodic system or long-range electrostatics asks for more than there is.
        if (periodic && problems.isEmpty())
            need(QStringLiteral("md.periodic"), blockers, notes);
        if (periodic && problems.isEmpty() && (electrostatics == QLatin1String("pme") || electrostatics == QLatin1String("pppm")
                || electrostatics == QLatin1String("ewald")))
            need(QStringLiteral("md.longrange_electrostatics"), blockers, notes);

        QString state;
        if (!problems.isEmpty()) {
            state = QStringLiteral("invalid");
            ++invalid;
            for (const QString& p : problems)
                errors.append(QStringLiteral("stage \"%1\": %2").arg(name, p));
        } else if (!blockers.isEmpty()) {
            state = QStringLiteral("blocked");
            ++blockedCount;
        } else {
            state = QStringLiteral("ready");
            ++ready;
        }

        QJsonObject out;
        out.insert(QStringLiteral("name"), name);
        out.insert(QStringLiteral("state"), state);
        out.insert(QStringLiteral("blockers"), blockers);
        out.insert(QStringLiteral("notes"), notes);
        out.insert(QStringLiteral("runs"), runs);
        stagesOut.append(out);
    }

    // --- analysis ----------------------------------------------------------------
    for (const QJsonValue& value : protocol.value(QStringLiteral("analysis")).toArray()) {
        const QJsonObject item = value.toObject();
        const QString quantity = item.value(QStringLiteral("quantity")).toString();
        QJsonObject out;
        out.insert(QStringLiteral("quantity"), quantity);
        const QString id = analysisCapability(quantity);
        if (id.isEmpty()) {
            errors.append(QStringLiteral("unknown analysis quantity \"%1\" (%2)").arg(quantity, kAnalysisIds.join(QStringLiteral(", "))));
            out.insert(QStringLiteral("state"), QStringLiteral("invalid"));
        } else {
            QJsonArray blockers, notes;
            need(id, blockers, notes);
            out.insert(QStringLiteral("state"), !blockers.isEmpty() ? QStringLiteral("blocked")
                    : !notes.isEmpty()                              ? QStringLiteral("partial")
                                                                    : QStringLiteral("ready"));
            out.insert(QStringLiteral("blockers"), blockers);
            out.insert(QStringLiteral("notes"), notes);
        }
        analysisOut.append(out);
    }

    QJsonObject summary;
    summary.insert(QStringLiteral("stages"), stages.size());
    summary.insert(QStringLiteral("ready"), ready);
    summary.insert(QStringLiteral("blocked"), blockedCount);
    summary.insert(QStringLiteral("invalid"), invalid);

    QJsonObject result;
    result.insert(QStringLiteral("ok"), errors.isEmpty());
    result.insert(QStringLiteral("errors"), errors);
    result.insert(QStringLiteral("prepare"), prepare);
    result.insert(QStringLiteral("stages"), stagesOut);
    result.insert(QStringLiteral("analysis"), analysisOut);
    result.insert(QStringLiteral("summary"), summary);
    return result;
}

// --- stage progress ------------------------------------------------------------

QString statusPath(const QString& directory)
{
    return QDir(directory).filePath(QStringLiteral("status.json"));
}

QJsonObject readStatus(const QString& directory)
{
    QFile file(statusPath(directory));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

namespace {
bool writeStatus(const QString& directory, const QJsonObject& status, QString* error)
{
    QSaveFile file(statusPath(directory));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("cannot write %1").arg(statusPath(directory));
        return false;
    }
    file.write(QJsonDocument(status).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("cannot write %1").arg(statusPath(directory));
        return false;
    }
    return true;
}
} // namespace

bool initStatus(const QString& directory, const QStringList& stages, QString* error)
{
    const QJsonObject old = readStatus(directory).value(QStringLiteral("stages")).toObject();
    QJsonObject fresh;
    for (const QString& name : stages) {
        if (old.contains(name)) {
            fresh.insert(name, old.value(name));
        } else {
            QJsonObject entry;
            entry.insert(QStringLiteral("state"), QStringLiteral("pending"));
            entry.insert(QStringLiteral("updated"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            fresh.insert(name, entry);
        }
    }
    QJsonObject status;
    status.insert(QStringLiteral("stages"), fresh);
    return writeStatus(directory, status, error);
}

bool setStageState(const QString& directory, const QString& stage, const QString& state,
    const QString& note, QString* error)
{
    if (!kStates.contains(state)) {
        if (error)
            *error = QStringLiteral("state must be one of: %1").arg(kStates.join(QStringLiteral(", ")));
        return false;
    }
    QJsonObject status = readStatus(directory);
    QJsonObject stages = status.value(QStringLiteral("stages")).toObject();
    if (!stages.contains(stage)) {
        if (error)
            *error = QStringLiteral("no stage \"%1\" in %2; save the protocol first").arg(stage, statusPath(directory));
        return false;
    }
    QJsonObject entry;
    entry.insert(QStringLiteral("state"), state);
    entry.insert(QStringLiteral("updated"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!note.isEmpty())
        entry.insert(QStringLiteral("note"), note);
    stages.insert(stage, entry);
    status.insert(QStringLiteral("stages"), stages);
    return writeStatus(directory, status, error);
}

} // namespace setupcheck
