// tools_core.cpp - Tools that need nothing but the core.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tools_core.h"

#include "loghub.h"
#include "toolregistry.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace {

QJsonObject schema(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

LogLevel levelFromName(const QString& name, LogLevel fallback)
{
    if (name.compare(QLatin1String("debug"), Qt::CaseInsensitive) == 0)   return LogLevel::Debug;
    if (name.compare(QLatin1String("info"), Qt::CaseInsensitive) == 0)    return LogLevel::Info;
    if (name.compare(QLatin1String("warning"), Qt::CaseInsensitive) == 0) return LogLevel::Warning;
    if (name.compare(QLatin1String("error"), Qt::CaseInsensitive) == 0)   return LogLevel::Error;
    return fallback;
}

QJsonObject recordToJson(const LogRecord& record)
{
    QJsonObject o;
    o.insert(QStringLiteral("seq"), static_cast<double>(record.seq));
    o.insert(QStringLiteral("time"), record.timestamp.toString(Qt::ISODate));
    o.insert(QStringLiteral("source"), record.source);
    o.insert(QStringLiteral("level"), LogHub::levelName(record.level).toLower());
    o.insert(QStringLiteral("text"), record.text);
    if (!record.jobId.isEmpty())
        o.insert(QStringLiteral("job_id"), record.jobId);
    return o;
}

// ---------------------------------------------------------------------------

ToolSpec makeReadLog(LogHub& hub)
{
    ToolSpec spec;
    spec.name = QStringLiteral("read_log");
    spec.category = QStringLiteral("diagnostics");
    spec.description = QStringLiteral(
        "Read recent log lines. Filter by source (qurcuma, curcuma, process, tool), by "
        "minimum level, by substring, and by sequence number to continue where a previous "
        "call stopped. The result is capped; use next_seq to page rather than raising limit.");
    spec.effect = ToolEffect::Read;
    spec.affinity = ToolAffinity::Any;
    spec.paramSchema = schema(R"JSON({
      "type": "object",
      "properties": {
        "source":    { "type": "string",  "description": "only this source; omit for all" },
        "min_level": { "type": "string",  "enum": ["debug", "info", "warning", "error"],
                       "description": "skip records below this level (default debug)" },
        "since_seq": { "type": "integer", "minimum": 0,
                       "description": "only records newer than this sequence number" },
        "contains":  { "type": "string",  "description": "case-insensitive substring filter" },
        "limit":     { "type": "integer", "minimum": 1, "maximum": 1000,
                       "description": "how many records at most (default 100)" }
      }
    })JSON");

    spec.handler = [&hub](const QJsonObject& args) {
        LogQuery q;
        q.source = args.value(QStringLiteral("source")).toString();
        q.minLevel = levelFromName(args.value(QStringLiteral("min_level")).toString(), LogLevel::Debug);
        q.sinceSeq = static_cast<quint64>(args.value(QStringLiteral("since_seq")).toDouble(0));
        q.contains = args.value(QStringLiteral("contains")).toString();
        q.limit = args.value(QStringLiteral("limit")).toInt(100);

        const QVector<LogRecord> records = hub.query(q);

        QJsonArray array;
        for (const LogRecord& record : records)
            array.append(recordToJson(record));

        // A full page means there may well be more behind it. Saying so, and handing
        // back where to continue, is what keeps a caller from simply raising limit.
        const bool full = records.size() >= qBound(1, q.limit, LogHub::kMaxQueryLimit);

        QJsonObject data;
        data.insert(QStringLiteral("records"), array);
        data.insert(QStringLiteral("count"), records.size());
        data.insert(QStringLiteral("next_seq"),
            static_cast<double>(records.isEmpty() ? q.sinceSeq : records.last().seq));
        data.insert(QStringLiteral("hub_last_seq"), static_cast<double>(hub.lastSeq()));
        data.insert(QStringLiteral("dropped_total"), static_cast<double>(hub.droppedCount()));

        ToolResult result = ToolResult::success(data);
        result.truncated = full;
        return result;
    };
    return spec;
}

ToolSpec makeDescribeTools(ToolRegistry& registry)
{
    ToolSpec spec;
    spec.name = QStringLiteral("describe_tools");
    spec.category = QStringLiteral("meta");
    spec.description = QStringLiteral(
        "List the available tools with their category and effect. Pass a name to get that "
        "one tool's full parameter schema; the listing omits schemas on purpose so the "
        "catalogue stays small.");
    spec.effect = ToolEffect::Read;
    spec.affinity = ToolAffinity::Any;
    spec.paramSchema = schema(R"JSON({
      "type": "object",
      "properties": {
        "name":     { "type": "string", "description": "a single tool; includes its schema" },
        "category": { "type": "string", "description": "only tools in this category" }
      }
    })JSON");

    spec.handler = [&registry](const QJsonObject& args) {
        const QString wanted = args.value(QStringLiteral("name")).toString();
        const QString category = args.value(QStringLiteral("category")).toString();

        const auto describe = [](const ToolSpec& t, bool withSchema) {
            QJsonObject o;
            o.insert(QStringLiteral("name"), t.name);
            o.insert(QStringLiteral("description"), t.description);
            o.insert(QStringLiteral("category"), t.category);
            o.insert(QStringLiteral("effect"), toolEffectName(t.effect));
            if (withSchema)
                o.insert(QStringLiteral("parameters"), t.paramSchema);
            return o;
        };

        if (!wanted.isEmpty()) {
            ToolSpec one;
            if (!registry.tool(wanted, one))
                return ToolResult::failure(QStringLiteral("unknown tool \"%1\"").arg(wanted));
            QJsonObject data;
            data.insert(QStringLiteral("tool"), describe(one, /*withSchema=*/true));
            return ToolResult::success(data);
        }

        QJsonArray array;
        for (const ToolSpec& t : registry.all()) {
            if (!category.isEmpty() && t.category != category)
                continue;
            array.append(describe(t, /*withSchema=*/false));
        }

        QJsonObject data;
        data.insert(QStringLiteral("tools"), array);
        data.insert(QStringLiteral("count"), array.size());
        return ToolResult::success(data,
            QStringLiteral("Call describe_tools with a name to see that tool's parameters."));
    };
    return spec;
}

}  // namespace

int registerCoreTools(ToolRegistry& registry, LogHub& hub)
{
    int added = 0;
    if (registry.add(makeReadLog(hub)))
        ++added;
    if (registry.add(makeDescribeTools(registry)))
        ++added;
    return added;
}
