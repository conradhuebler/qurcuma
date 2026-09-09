// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — ToolRegistry: what a schema may say, what registration
// refuses, and that arguments are checked before a handler ever runs.

#include "core/toolregistry.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <cstdio>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

/// Readable JSON literals in the test body.
static QJsonObject obj(const char* json)
{
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json), &err);
    if (err.error != QJsonParseError::NoError)
        std::printf("  (test bug: bad JSON literal: %s)\n", qPrintable(err.errorString()));
    return doc.object();
}

static void schemaOk(const char* json, const QString& what)
{
    QString error;
    const bool ok = ToolRegistry::isValidSchema(obj(json), &error);
    check(ok, what + (ok ? QString() : QStringLiteral(" [%1]").arg(error)));
}

static void schemaRejected(const char* json, const QString& what)
{
    QString error;
    check(!ToolRegistry::isValidSchema(obj(json), &error), what);
}

static void argsOk(const char* schema, const char* args, const QString& what)
{
    const ToolValidation v = ToolRegistry::validateAgainst(obj(schema), obj(args));
    check(v.ok, what + (v.ok ? QString() : QStringLiteral(" [%1]").arg(v.error)));
}

static void argsRejected(const char* schema, const char* args, const QString& needle,
                         const QString& what)
{
    const ToolValidation v = ToolRegistry::validateAgainst(obj(schema), obj(args));
    const bool ok = !v.ok && v.error.contains(needle, Qt::CaseInsensitive);
    check(ok, what + (ok ? QString() : QStringLiteral(" [got: %1]").arg(v.ok ? "accepted" : v.error)));
}

static const char* kSchema = R"({
  "type": "object",
  "properties": {
    "path":    { "type": "string",  "description": "file to load" },
    "frame":   { "type": "integer", "minimum": 0 },
    "opacity": { "type": "number",  "minimum": 0.0, "maximum": 1.0 },
    "mode":    { "type": "string",  "enum": ["ball", "stick", "spacefill"] },
    "force":   { "type": "boolean" }
  },
  "required": ["path"]
})";

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // --- what a schema may say ---------------------------------------------
    schemaOk(R"({"type":"object"})", "a schema with no properties is fine");
    schemaOk(kSchema, "the reference schema is well-formed");

    schemaRejected(R"({"type":"array"})", "top-level type must be object");
    schemaRejected(R"({"type":"object","additionalProperties":false})",
        "an unhonoured top-level key is refused, not ignored");
    schemaRejected(R"({"type":"object","properties":{"a":{"type":"date"}}})",
        "an unsupported property type is refused");
    schemaRejected(R"({"type":"object","properties":{"a":{"type":"string","pattern":"x"}}})",
        "an unhonoured property key is refused");
    schemaRejected(R"({"type":"object","properties":{"a":{"type":"string"}},"required":["b"]})",
        "required naming an undeclared property is refused");
    schemaRejected(R"({"type":"object","properties":{"a":{"type":"string","enum":[]}}})",
        "an empty enum is refused");
    schemaRejected(R"({"type":"object","properties":{"a":{"type":"string","minimum":1}}})",
        "minimum on a string is refused");

    // --- checking arguments -------------------------------------------------
    argsOk(kSchema, R"({"path":"a.xyz"})", "the required parameter alone is enough");
    argsOk(kSchema, R"({"path":"a.xyz","frame":3,"opacity":0.5,"mode":"stick","force":true})",
        "a fully populated call passes");

    argsRejected(kSchema, R"({})", "missing required", "a missing required parameter is caught");
    argsRejected(kSchema, R"({"path":"a.xyz","colour":"red"})", "unknown parameter",
        "an invented parameter is refused, not dropped");
    argsRejected(kSchema, R"({"path":42})", "expects string", "a wrong type is caught");
    argsRejected(kSchema, R"({"path":"a.xyz","frame":2.5})", "expects integer",
        "a fractional value for an integer is caught, not truncated");
    argsRejected(kSchema, R"({"path":"a.xyz","mode":"cartoon"})", "must be one of",
        "a value outside the enum is caught");
    argsRejected(kSchema, R"({"path":"a.xyz","opacity":1.5})", "<=", "maximum is enforced");
    argsRejected(kSchema, R"({"path":"a.xyz","frame":-1})", ">=", "minimum is enforced");

    // --- registration -------------------------------------------------------
    {
        ToolRegistry reg;
        ToolSpec spec;
        spec.name = QStringLiteral("load_structure");
        spec.description = QStringLiteral("Load a structure file");
        spec.category = QStringLiteral("structure");
        spec.paramSchema = obj(kSchema);
        spec.effect = ToolEffect::Mutate;
        spec.affinity = ToolAffinity::Gui;
        spec.handler = [](const QJsonObject& args) {
            return ToolResult::success(QJsonObject { { "loaded", args.value("path") } });
        };

        QString error;
        check(reg.add(spec, &error), "a well-formed tool registers");
        check(reg.size() == 1 && reg.contains("load_structure"), "and is then found by name");
        check(!reg.add(spec, &error) && error.contains("already"),
            "registering the same name twice is refused");

        ToolSpec unnamed = spec;
        unnamed.name.clear();
        check(!reg.add(unnamed, &error), "a tool without a name is refused");

        ToolSpec handlerless = spec;
        handlerless.name = QStringLiteral("handlerless");
        handlerless.handler = nullptr;
        check(!reg.add(handlerless, &error) && error.contains("handler"),
            "a tool without a handler is refused");

        ToolSpec badSchema = spec;
        badSchema.name = QStringLiteral("bad_schema");
        badSchema.paramSchema = obj(R"({"type":"object","properties":{"a":{"type":"date"}}})");
        check(!reg.add(badSchema, &error),
            "a malformed schema fails at registration, not at call time");
        check(reg.size() == 1, "and none of the refused tools landed in the catalogue");

        ToolSpec fetched;
        check(reg.tool("load_structure", fetched) && fetched.effect == ToolEffect::Mutate
                && fetched.affinity == ToolAffinity::Gui,
            "effect and affinity survive registration");
        check(!reg.tool("nope", fetched), "an unknown name is reported, not invented");
    }

    // --- invoking -----------------------------------------------------------
    {
        ToolRegistry reg;
        int calls = 0;
        ToolSpec spec;
        spec.name = QStringLiteral("echo");
        spec.paramSchema = obj(R"({"type":"object","properties":{"text":{"type":"string"}},"required":["text"]})");
        spec.handler = [&calls](const QJsonObject& args) {
            ++calls;
            return ToolResult::success(QJsonObject { { "echoed", args.value("text") } });
        };
        QString error;
        check(reg.add(spec, &error), "echo registers");

        const ToolResult good = reg.invoke("echo", obj(R"({"text":"hello"})"));
        check(good.ok && good.data.value("echoed").toString() == QLatin1String("hello"),
            "invoke runs the handler and returns its data");
        check(calls == 1, "exactly once");

        const ToolResult bad = reg.invoke("echo", obj(R"({"text":7})"));
        check(!bad.ok && bad.error.contains("expects string"),
            "a bad argument comes back as a failed result");
        check(calls == 1, "and the handler was NOT reached");

        const ToolResult missing = reg.invoke("nope", QJsonObject {});
        check(!missing.ok && missing.error.contains("unknown tool"),
            "invoking an unknown tool fails cleanly");
    }

    // --- catalogue order and names ------------------------------------------
    {
        ToolRegistry reg;
        for (const char* n : { "zeta", "alpha", "mid" }) {
            ToolSpec s;
            s.name = QString::fromLatin1(n);
            s.paramSchema = obj(R"({"type":"object"})");
            s.handler = [](const QJsonObject&) { return ToolResult::success(); };
            reg.add(s);
        }
        const QStringList names = reg.names();
        check(names == QStringList({ "alpha", "mid", "zeta" }),
            "names() is sorted, so catalogues and tests are reproducible");
        const QVector<ToolSpec> all = reg.all();
        check(all.size() == 3 && all.first().name == QLatin1String("alpha"),
            "all() follows the same order");
        reg.clear();
        check(reg.size() == 0, "clear empties the catalogue");
    }

    check(toolEffectName(ToolEffect::FileWrite) == QLatin1String("file_write")
            && toolAffinityName(ToolAffinity::Gui) == QLatin1String("gui"),
        "effect and affinity have stable spellings for logs and catalogues");

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
