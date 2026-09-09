// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — the two core tools: read_log's filters, its cap and the
// paging handle it hands back, and that describe_tools stays small by default.

#include "core/loghub.h"
#include "core/toolregistry.h"
#include "core/tools_core.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

static QJsonObject obj(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    LogHub hub(500);
    ToolRegistry registry;

    check(registerCoreTools(registry, hub) == 2, "both core tools register");
    check(registry.contains(QStringLiteral("read_log"))
            && registry.contains(QStringLiteral("describe_tools")),
        "and are in the catalogue by name");

    // Registration already validates the schemas, but say so explicitly: a tool
    // whose schema the validator cannot honour must never reach a model.
    for (const ToolSpec& t : registry.all()) {
        QString error;
        check(ToolRegistry::isValidSchema(t.paramSchema, &error),
            QStringLiteral("%1 has a schema the validator honours").arg(t.name));
    }

    // --- read_log -----------------------------------------------------------
    for (int i = 1; i <= 12; ++i) {
        hub.append(i % 2 ? QStringLiteral("qurcuma") : QStringLiteral("curcuma"),
            i > 9 ? LogLevel::Error : LogLevel::Info,
            QStringLiteral("line %1").arg(i));
    }

    {
        const ToolResult all = registry.invoke(QStringLiteral("read_log"), QJsonObject {});
        check(all.ok, "read_log runs with no arguments");
        check(all.data.value(QStringLiteral("count")).toInt() == 12, "and returns every record");
        check(!all.truncated, "a partial page is not marked truncated");
    }
    {
        const ToolResult bySource = registry.invoke(QStringLiteral("read_log"),
            obj(R"({"source":"curcuma"})"));
        check(bySource.data.value(QStringLiteral("count")).toInt() == 6, "source filter reaches through");
    }
    {
        const ToolResult byLevel = registry.invoke(QStringLiteral("read_log"),
            obj(R"({"min_level":"error"})"));
        check(byLevel.data.value(QStringLiteral("count")).toInt() == 3, "min_level filter reaches through");
    }
    {
        const ToolResult byText = registry.invoke(QStringLiteral("read_log"),
            obj(R"({"contains":"line 7"})"));
        check(byText.data.value(QStringLiteral("count")).toInt() == 1, "substring filter reaches through");
    }

    // --- the cap, and paging instead of raising the limit -------------------
    {
        const ToolResult page = registry.invoke(QStringLiteral("read_log"), obj(R"({"limit":5})"));
        check(page.data.value(QStringLiteral("count")).toInt() == 5, "limit is honoured");
        check(page.truncated, "a full page is flagged truncated so the caller knows to page");

        const quint64 next = static_cast<quint64>(page.data.value(QStringLiteral("next_seq")).toDouble());
        const ToolResult rest = registry.invoke(QStringLiteral("read_log"),
            obj(QStringLiteral(R"({"since_seq":%1})").arg(next).toUtf8().constData()));
        check(rest.data.value(QStringLiteral("count")).toInt() == 0,
            "next_seq points past the newest record already seen");

        // Paging backwards through the buffer: ask for the oldest five, then continue.
        const ToolResult first = registry.invoke(QStringLiteral("read_log"),
            obj(R"({"since_seq":0,"limit":1000})"));
        check(first.data.value(QStringLiteral("count")).toInt() == 12,
            "a generous limit still returns only what exists");
    }
    {
        const ToolResult bad = registry.invoke(QStringLiteral("read_log"), obj(R"({"limit":99999})"));
        check(!bad.ok && bad.error.contains(QStringLiteral("<=")),
            "a limit beyond the schema maximum is refused, not silently clamped");
    }
    {
        const ToolResult bad = registry.invoke(QStringLiteral("read_log"), obj(R"({"min_level":"chatty"})"));
        check(!bad.ok && bad.error.contains(QStringLiteral("must be one of")),
            "an invented level is refused by the enum");
    }

    // --- describe_tools -----------------------------------------------------
    {
        const ToolResult listing = registry.invoke(QStringLiteral("describe_tools"), QJsonObject {});
        check(listing.ok, "describe_tools runs");
        const QJsonArray tools = listing.data.value(QStringLiteral("tools")).toArray();
        check(tools.size() == 2, "it lists both tools");

        bool anySchema = false;
        for (const QJsonValue& v : tools) {
            if (v.toObject().contains(QStringLiteral("parameters")))
                anySchema = true;
        }
        check(!anySchema, "the listing carries NO schemas -- that is what keeps the catalogue small");
        check(tools.first().toObject().value(QStringLiteral("effect")).toString()
                == QLatin1String("read"),
            "but it does carry the effect, which the approval policy needs");
    }
    {
        const ToolResult one = registry.invoke(QStringLiteral("describe_tools"),
            obj(R"({"name":"read_log"})"));
        check(one.ok, "asking for one tool works");
        const QJsonObject tool = one.data.value(QStringLiteral("tool")).toObject();
        check(tool.contains(QStringLiteral("parameters")),
            "and that one does come with its schema");
        check(tool.value(QStringLiteral("parameters")).toObject()
                  .value(QStringLiteral("properties")).toObject()
                  .contains(QStringLiteral("since_seq")),
            "the schema is the real one, not a stub");
    }
    {
        const ToolResult byCategory = registry.invoke(QStringLiteral("describe_tools"),
            obj(R"({"category":"meta"})"));
        check(byCategory.data.value(QStringLiteral("tools")).toArray().size() == 1,
            "the category filter works");
    }
    {
        const ToolResult missing = registry.invoke(QStringLiteral("describe_tools"),
            obj(R"({"name":"nope"})"));
        check(!missing.ok && missing.error.contains(QStringLiteral("unknown tool")),
            "asking about a tool that does not exist fails cleanly");
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
