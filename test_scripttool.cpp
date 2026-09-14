// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - What the `calculate` tool promises, pinned: the schema, what
// a result looks like, what a bad argument is told, that the dispatcher's interrupt
// reaches a running script, and that this path has no host bridge. That last one is the
// difference between a calculator and a way around the approval policy, so it is a test
// and not a comment.

#include "core/loghub.h"
#include "core/tooldispatcher.h"
#include "core/toolregistry.h"
#include "script/scriptsyntax.h"
#include "script/tools_script.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

static bool mentions(const QString& haystack, const QString& needle)
{
    return haystack.contains(needle, Qt::CaseInsensitive);
}

static QJsonObject args(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    LogHub hub(500);
    ToolRegistry registry;
    ScriptToolContext context;  // no dispatcher yet: the tool itself does not need one

    check(registerScriptTools(registry, context) == 1, "the calculate tool registers");
    check(registry.contains(QStringLiteral("calculate")), "and is in the catalogue by name");

    {
        QString error;
        ToolSpec spec;
        registry.tool(QStringLiteral("calculate"), spec);
        check(ToolRegistry::isValidSchema(spec.paramSchema, &error),
            QStringLiteral("its schema is one the validator honours: %1").arg(error));
        check(spec.effect == ToolEffect::Read, "it is a read: it touches nothing");
        check(!spec.available, "and is always available: a calculator needs no structure");
    }

    // The catalogue goes out with every request, so its size is a budget rather than a
    // detail. One tool that displaces a family of narrow ones has to stay small.
    {
        const int bytes = registry.catalogueBytes();
        check(bytes <= 1500,
            QStringLiteral("the catalogue entry stays under 1500 bytes (%1)").arg(bytes));
    }

    // --- a value, a printed line, named results --------------------------------
    {
        const ToolResult r = registry.invoke(QStringLiteral("calculate"),
            args(R"j({"source": "1 + 1"})j"));
        check(r.ok, "a script runs through the tool");
        check(r.data.value(QStringLiteral("value")).toDouble() == 2.0, "and its value comes back");
        check(r.text == QStringLiteral("value = 2"), QStringLiteral("with a one-line summary: %1").arg(r.text));
    }
    {
        const ToolResult r = registry.invoke(QStringLiteral("calculate"),
            args(R"j({"source": "print(\"dE =\", 0.0032); 0"})j"));
        check(r.ok && r.text == QStringLiteral("dE = 0.0032"),
            QStringLiteral("a printed line is the summary: %1").arg(r.text));
        check(r.data.value(QStringLiteral("prints")).toArray().size() == 1, "and travels as a list too");
    }
    {
        const ToolResult r = registry.invoke(QStringLiteral("calculate"),
            args(R"j({"source": "var e = [-40.12, -40.11]; ({dE: e[0] - e[1], kJ: ha_to_kjmol(e[0] - e[1])})"})j"));
        const QJsonObject value = r.data.value(QStringLiteral("value")).toObject();
        check(r.ok && value.size() == 2, "an object result gives named results");
        // e[0] - e[1] is -0.01 Hartree: the second energy is the higher one, so the
        // difference is negative and no conversion may quietly swallow the sign.
        check(qAbs(value.value(QStringLiteral("dE")).toDouble() + 0.01) < 1e-12,
            "the difference keeps its sign");
        check(qAbs(value.value(QStringLiteral("kJ")).toDouble() + 26.254996394798254) < 1e-9,
            QStringLiteral("and is converted here rather than in the model's head: %1")
                .arg(value.value(QStringLiteral("kJ")).toDouble(), 0, 'g', 15));
    }

    // --- values handed in ------------------------------------------------------
    {
        const ToolResult r = registry.invoke(QStringLiteral("calculate"),
            args(R"j({"source": "mean(data)", "data": [1, 2, 3]})j"));
        check(r.ok && r.data.value(QStringLiteral("value")).toDouble() == 2.0,
            "data is bound as a list the script can reduce");
    }
    {
        const ToolResult r = registry.invoke(QStringLiteral("calculate"),
            args(R"j({"source": "slope(data[0], data[1])", "data": [[1, 2, 3], [2, 4, 6]]})j"));
        check(r.ok && r.data.value(QStringLiteral("value")).toDouble() == 2.0,
            "and a list of lists carries two series at once");
    }

    // --- what a bad call is told -----------------------------------------------
    {
        const ToolResult r = registry.invoke(QStringLiteral("calculate"),
            args(R"j({"source": "1", "data": [1, "zwei"]})j"));
        check(!r.ok && mentions(r.error, QStringLiteral("data[1]")),
            QStringLiteral("a non-number in data is named where it sits: %1").arg(r.error));
    }
    {
        const ToolResult r = registry.invoke(QStringLiteral("calculate"), args(R"j({"source": "a + b"})j"));
        check(!r.ok && mentions(r.error, QStringLiteral("line 1")),
            QStringLiteral("a script error reaches the model with its line: %1").arg(r.error));
    }
    {
        const ToolValidation missing = registry.validate(QStringLiteral("calculate"), QJsonObject {});
        check(!missing.ok, "source is required");
        const ToolValidation unknown = registry.validate(QStringLiteral("calculate"),
            args(R"j({"source": "1", "script": "2"})j"));
        check(!unknown.ok, "and an invented parameter is refused rather than ignored");
    }

    // --- what the dock can show before a script runs ---------------------------
    // Only a literal name can be previewed; it is what the operator is shown, not what
    // enforces anything, so a duplicate or a mention in a comment costs a line at most.
    {
        const QStringList names = script::namedToolsIn(
            QStringLiteral("var a = tool(\"measure\", {});\nvar b = tool('select_atoms', {});\ntool(\"measure\", {});\na"));
        check(names.size() == 2 && names.first() == QStringLiteral("measure")
                && names.last() == QStringLiteral("select_atoms"),
            QStringLiteral("the literal tool names are read in order, once each: %1")
                .arg(names.join(QStringLiteral(", "))));
        check(script::namedToolsIn(QStringLiteral("1 + 1")).isEmpty(),
            "a script that calls nothing names nothing");
        check(script::namedToolsIn(QStringLiteral("tool(\"mea\" + \"sure\", {})")).isEmpty(),
            "a name built at run time cannot be previewed, so it is not listed");
    }

    // --- the calculator is not a way around the policy -------------------------
    // The dock builds its interpreters with a host bridge; this path does not, and
    // that is what this asserts.
    {
        const ToolResult r = registry.invoke(QStringLiteral("calculate"),
            args(R"j({"source": "tool(\"run_single_point\", {})"})j"));
        check(!r.ok && mentions(r.error, QStringLiteral("calculation only")),
            QStringLiteral("a script from the model cannot call tools: %1").arg(r.error));
    }

    // --- the stop button reaches a running script ------------------------------
    // The handler is bound when the tool is registered, so a registry wired to a
    // dispatcher is what this check needs; MainWindow registers the same way.
    {
        ToolRegistry wired;
        ToolDispatcher dispatcher(&wired, &hub);
        ScriptToolContext wiredContext;
        wiredContext.dispatcher = &dispatcher;
        registerScriptTools(wired, wiredContext);

        dispatcher.requestInterrupt();

        QElapsedTimer clock;
        clock.start();
        const ToolResult r = dispatcher.dispatch(QStringLiteral("calculate"),
            args(R"j({"source": "while (true) {}"})j"));
        const qint64 took = clock.elapsed();

        check(!r.ok && mentions(r.error, QStringLiteral("operator")),
            QStringLiteral("the dispatcher's interrupt ends a runaway script: %1").arg(r.error));
        check(took < 2000, QStringLiteral("and does so promptly (%1 ms)").arg(took));

        int audited = 0;
        for (const LogRecord& record : hub.query(LogQuery {})) {
            if (record.source == QLatin1String("tool") && record.text.contains(QLatin1String("calculate")))
                ++audited;
        }
        check(audited >= 1, QStringLiteral("and the call is in the audit trail (%1 record(s))").arg(audited));
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAILED" : "OK", g_failed);
    return g_failed ? 1 : 0;
}
