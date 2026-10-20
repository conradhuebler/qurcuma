// tools_protocol.cpp - see tools_protocol.h.
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026

#include "tools_protocol.h"

#include "setupcheck.h"

#include <cutechat/toolregistry.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

QJsonObject schema(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

const char* kProtocolSchema = R"JSON({
  "type": "object",
  "description": "the protocol: source, method, system {solute_file, charge, solvent, solvent_count, density_g_cm3, periodic, electrostatics, ions{anion,cation}}, stages[] {name, type minimize|md, ensemble nvt|nve|npt, temperature_K number or list, series independent|chained|ramp, pressure_bar, length_ps, timestep_fs, thermostat, coupling_fs, barostat_coupling_fs}, execution {parallel}, analysis[] {quantity, last_ns}"
})JSON";

QString directoryOf(const std::function<QString()>& workingDirectory)
{
    return workingDirectory ? workingDirectory() : QString();
}

QString protocolPath(const QString& directory)
{
    return QDir(directory).filePath(QStringLiteral("protocol.json"));
}

} // namespace

int registerProtocolTools(ToolRegistry& registry, const ProtocolToolContext& context)
{
    int added = 0;
    const auto add = [&registry, &added](const ToolSpec& spec) {
        if (registry.add(spec))
            ++added;
    };
    const auto workingDirectory = context.workingDirectory;

    // --- list_capabilities --------------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("list_capabilities");
        spec.category = QStringLiteral("protocol");
        spec.description = QStringLiteral(
            "What this program and its curcuma back end can and cannot do for a published MD setup: "
            "each capability is available, partial or missing, with a note, where it lives in the code "
            "and the work package that would close it. Use it before promising that a setup can be run.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;
        spec.paramSchema = schema(R"JSON({"type":"object","properties":{
            "state": { "type": "string", "enum": ["available","partial","missing"],
                       "description": "only entries in this state" } }})JSON");
        spec.handler = [](const QJsonObject& args) {
            const QString wanted = args.value(QStringLiteral("state")).toString();
            QJsonArray list;
            int available = 0, partial = 0, missing = 0;
            for (const setupcheck::Capability& c : setupcheck::capabilities()) {
                available += c.state == setupcheck::State::Available;
                partial += c.state == setupcheck::State::Partial;
                missing += c.state == setupcheck::State::Missing;
                if (!wanted.isEmpty() && setupcheck::stateName(c.state) != wanted)
                    continue;
                QJsonObject o;
                o.insert(QStringLiteral("id"), c.id);
                o.insert(QStringLiteral("state"), setupcheck::stateName(c.state));
                o.insert(QStringLiteral("note"), c.note);
                o.insert(QStringLiteral("where"), c.where);
                if (!c.package.isEmpty())
                    o.insert(QStringLiteral("package"), c.package);
                list.append(o);
            }
            QJsonObject data;
            data.insert(QStringLiteral("capabilities"), list);
            data.insert(QStringLiteral("available"), available);
            data.insert(QStringLiteral("partial"), partial);
            data.insert(QStringLiteral("missing"), missing);
            return ToolResult::success(data);
        };
        add(spec);
    }

    // --- check_protocol -----------------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("check_protocol");
        spec.category = QStringLiteral("protocol");
        spec.description = QStringLiteral(
            "Check a simulation protocol (the setup of a paper written as structured JSON) against what "
            "can be run here. Per stage: ready, blocked (with the missing capability and the work package "
            "that closes it) or invalid (the protocol itself is wrong). Returns the tool calls that prepare "
            "the system and the run_simulation arguments for each stage; it starts nothing. Without "
            "'protocol' it reads protocol.json from the project folder and adds each stage's progress "
            "from status.json. Several temperatures in one stage need 'series' (independent, chained, "
            "ramp): that is a scientific choice, not an execution detail.");
        spec.effect = ToolEffect::Read;
        spec.affinity = ToolAffinity::Any;
        spec.paramSchema = schema(R"JSON({"type":"object","properties":{
            "protocol": { "type": "object" } }})JSON");
        spec.paramSchema.insert(QStringLiteral("properties"),
            QJsonObject { { QStringLiteral("protocol"), schema(kProtocolSchema) } });
        spec.handler = [workingDirectory](const QJsonObject& args) {
            const QString directory = directoryOf(workingDirectory);
            QJsonObject protocol = args.value(QStringLiteral("protocol")).toObject();
            bool fromFile = false;
            if (protocol.isEmpty()) {
                QFile file(protocolPath(directory));
                if (directory.isEmpty() || !file.open(QIODevice::ReadOnly))
                    return ToolResult::failure(QStringLiteral(
                        "no protocol given and no protocol.json in the project folder"));
                protocol = QJsonDocument::fromJson(file.readAll()).object();
                fromFile = true;
            }
            QJsonObject result = setupcheck::checkProtocol(protocol);

            // Progress from status.json, matched by stage name.
            const QJsonObject progress = setupcheck::readStatus(directory).value(QStringLiteral("stages")).toObject();
            QJsonArray stages = result.value(QStringLiteral("stages")).toArray();
            for (int i = 0; i < stages.size(); ++i) {
                QJsonObject stage = stages[i].toObject();
                const QString name = stage.value(QStringLiteral("name")).toString();
                if (progress.contains(name))
                    stage.insert(QStringLiteral("progress"), progress.value(name).toObject().value(QStringLiteral("state")));
                stages[i] = stage;
            }
            result.insert(QStringLiteral("stages"), stages);
            result.insert(QStringLiteral("from_file"), fromFile);

            const QJsonObject s = result.value(QStringLiteral("summary")).toObject();
            const QString text = QStringLiteral("%1 stage(s): %2 ready, %3 blocked, %4 invalid.")
                                     .arg(s.value(QStringLiteral("stages")).toInt())
                                     .arg(s.value(QStringLiteral("ready")).toInt())
                                     .arg(s.value(QStringLiteral("blocked")).toInt())
                                     .arg(s.value(QStringLiteral("invalid")).toInt());
            return ToolResult::success(result, text);
        };
        add(spec);
    }

    // --- save_protocol ------------------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("save_protocol");
        spec.category = QStringLiteral("protocol");
        spec.description = QStringLiteral(
            "Write the protocol to protocol.json in the project folder and create status.json with every "
            "stage pending (stages already in status.json keep their state). A protocol with blocked stages "
            "is saved; a malformed one is refused.");
        spec.effect = ToolEffect::FileWrite;
        spec.affinity = ToolAffinity::Any;
        spec.paramSchema = schema(R"JSON({"type":"object","required":["protocol"]})JSON");
        spec.paramSchema.insert(QStringLiteral("properties"),
            QJsonObject { { QStringLiteral("protocol"), schema(kProtocolSchema) } });
        spec.handler = [workingDirectory](const QJsonObject& args) {
            const QString directory = directoryOf(workingDirectory);
            if (directory.isEmpty() || !QDir(directory).exists())
                return ToolResult::failure(QStringLiteral("there is no project folder to write into"));
            const QJsonObject protocol = args.value(QStringLiteral("protocol")).toObject();
            const QJsonObject check = setupcheck::checkProtocol(protocol);
            if (!check.value(QStringLiteral("ok")).toBool()) {
                QJsonObject data;
                data.insert(QStringLiteral("errors"), check.value(QStringLiteral("errors")));
                return ToolResult::failure(QStringLiteral("the protocol is malformed, nothing was written: %1")
                                               .arg(QJsonDocument(check.value(QStringLiteral("errors")).toArray())
                                                        .toJson(QJsonDocument::Compact)));
            }
            QSaveFile file(protocolPath(directory));
            if (!file.open(QIODevice::WriteOnly))
                return ToolResult::failure(QStringLiteral("cannot write %1").arg(protocolPath(directory)));
            file.write(QJsonDocument(protocol).toJson(QJsonDocument::Indented));
            if (!file.commit())
                return ToolResult::failure(QStringLiteral("cannot write %1").arg(protocolPath(directory)));

            QStringList names;
            for (const QJsonValue& v : protocol.value(QStringLiteral("stages")).toArray())
                names << v.toObject().value(QStringLiteral("name")).toString();
            QString error;
            if (!setupcheck::initStatus(directory, names, &error))
                return ToolResult::failure(error);

            QJsonObject data;
            data.insert(QStringLiteral("protocol"), protocolPath(directory));
            data.insert(QStringLiteral("status"), setupcheck::statusPath(directory));
            data.insert(QStringLiteral("summary"), check.value(QStringLiteral("summary")));
            return ToolResult::success(data, QStringLiteral("Saved %1 and %2.")
                                                 .arg(protocolPath(directory), setupcheck::statusPath(directory)));
        };
        add(spec);
    }

    // --- protocol_stage -----------------------------------------------------------
    {
        ToolSpec spec;
        spec.name = QStringLiteral("protocol_stage");
        spec.category = QStringLiteral("protocol");
        spec.description = QStringLiteral(
            "Record how far a stage of the saved protocol is: pending, running, done or failed, in "
            "status.json. Call it when a stage starts and when it ends, so that a later session -- or "
            "another machine -- continues from the first stage that is not done.");
        spec.effect = ToolEffect::FileWrite;
        spec.affinity = ToolAffinity::Any;
        spec.paramSchema = schema(R"JSON({"type":"object","properties":{
            "stage": { "type": "string" },
            "state": { "type": "string", "enum": ["pending","running","done","failed"] },
            "note":  { "type": "string", "description": "optional, e.g. the output file or the reason for a failure" }
          },"required":["stage","state"]})JSON");
        spec.handler = [workingDirectory](const QJsonObject& args) {
            const QString directory = directoryOf(workingDirectory);
            QString error;
            if (directory.isEmpty()
                || !setupcheck::setStageState(directory, args.value(QStringLiteral("stage")).toString(),
                    args.value(QStringLiteral("state")).toString(), args.value(QStringLiteral("note")).toString(), &error))
                return ToolResult::failure(error.isEmpty() ? QStringLiteral("there is no project folder") : error);

            // Next open stage, so the answer says where to go on.
            QString next;
            const QJsonObject stages = setupcheck::readStatus(directory).value(QStringLiteral("stages")).toObject();
            QJsonObject protocolDoc;
            QFile file(protocolPath(directory));
            QStringList order;
            if (file.open(QIODevice::ReadOnly)) {
                for (const QJsonValue& v : QJsonDocument::fromJson(file.readAll()).object()
                                               .value(QStringLiteral("stages")).toArray())
                    order << v.toObject().value(QStringLiteral("name")).toString();
            }
            for (const QString& name : order) {
                if (stages.value(name).toObject().value(QStringLiteral("state")).toString() != QLatin1String("done")) {
                    next = name;
                    break;
                }
            }
            QJsonObject data;
            data.insert(QStringLiteral("stage"), args.value(QStringLiteral("stage")));
            data.insert(QStringLiteral("state"), args.value(QStringLiteral("state")));
            data.insert(QStringLiteral("next_open_stage"), next);
            return ToolResult::success(data, next.isEmpty()
                    ? QStringLiteral("Recorded. No stage is left open.")
                    : QStringLiteral("Recorded. Next stage that is not done: %1.").arg(next));
        };
        add(spec);
    }

    return added;
}
