// tools_script.cpp - `calculate`: the interpreter as the assistant's tool.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Read, not Compute. The tool touches nothing: it works on
// numbers it is handed and returns what the script produced. Asking for permission
// each time would teach the operator to click Allow without reading, which is worse
// than not asking, and the cost is bounded by the deadline rather than by a dialog.
//
// No host bridge is passed, so a script from a model is a calculator. The dock builds
// its interpreters with one; this is where the two paths divide.
#include "tools_script.h"

#include "core/tooldispatcher.h"
#include "core/toolregistry.h"
#include "scriptbuiltins.h"
#include "scriptinterpreter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariantList>

namespace {

/// The description carries the contract the schema cannot express: which functions
/// exist beyond JavaScript's own, that trigonometry is in radians, and where the run
/// ends. It is the only place a model can learn the builtin table from.
const char* const kDescription =
    "Run a JavaScript calculation and return its result. Use it for every number that "
    "comes out of other numbers: a difference, a ratio, a mean, a spread, a unit "
    "conversion, a line through measured points. The value of the last expression is "
    "the result; print(...) gives a labelled line; an object as the last expression "
    "gives named results, as in ({dE: ha_to_kjmol(e0 - e1)}). Math is JavaScript's, so "
    "Math.log is the natural logarithm, Math.log10 is base ten and trigonometry is in "
    "radians (radians(deg) converts). Added here: sum min max mean sd (sample, n-1) "
    "sem median len slope intercept r2, the unit conversions ha_to_kjmol kjmol_to_ha "
    "ha_to_ev ev_to_ha ha_to_kcal kcal_to_ha bohr_to_ang ang_to_bohr cm_to_kjmol, and "
    "the constants kB NA h c R. The optional data list is bound as the variable data. "
    "Limits: 5 seconds, a returned list is cut at 64 values.";

/// The schema states the two arguments and nothing else; the validator honours only
/// what it can enforce, so the element type of `data` is checked here rather than
/// promised there.
QJsonObject schema()
{
    QJsonObject source;
    source.insert(QStringLiteral("type"), QStringLiteral("string"));
    source.insert(QStringLiteral("description"), QStringLiteral(
        "The JavaScript to run, e.g. \"var e = [-40.12, -40.11]; ({dE: ha_to_kjmol(e[0] - e[1])})\"."));

    QJsonObject data;
    data.insert(QStringLiteral("type"), QStringLiteral("array"));
    data.insert(QStringLiteral("description"), QStringLiteral(
        "Numbers the script sees as the list `data`. A list of lists is allowed, for two "
        "series at once. Omitted means no such variable."));

    QJsonObject properties;
    properties.insert(QStringLiteral("source"), source);
    properties.insert(QStringLiteral("data"), data);

    QJsonObject out;
    out.insert(QStringLiteral("type"), QStringLiteral("object"));
    out.insert(QStringLiteral("properties"), properties);
    out.insert(QStringLiteral("required"), QJsonArray { QStringLiteral("source") });
    return out;
}

/// Nothing to report means the value is a number or a list of numbers. The check names
/// the place in the argument, which is more useful than the script's own complaint
/// about a position inside a list it never had to see.
QString checkNumbers(const QJsonValue& value, const QString& where, int depth = 0)
{
    if (value.isDouble())
        return QString();
    if (value.isArray() && depth < 2) {
        const QJsonArray array = value.toArray();
        for (int i = 0; i < array.size(); ++i) {
            const QString problem = checkNumbers(array.at(i),
                QStringLiteral("%1[%2]").arg(where).arg(i), depth + 1);
            if (!problem.isEmpty())
                return problem;
        }
        return QString();
    }
    return QStringLiteral("%1 is not a number: %2")
        .arg(where, QString::fromUtf8(QJsonDocument(QJsonArray { value }).toJson(QJsonDocument::Compact)));
}

QString describeValue(const QVariant& value)
{
    const auto type = static_cast<QMetaType::Type>(value.typeId());
    if (type == QMetaType::Double || type == QMetaType::Float || type == QMetaType::Int)
        return script::formatNumber(value.toDouble());
    if (type == QMetaType::QVariantMap || type == QMetaType::QVariantList)
        return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    return value.toString();
}

}  // namespace

int registerScriptTools(ToolRegistry& registry, const ScriptToolContext& context)
{
    ToolDispatcher* const dispatcher = context.dispatcher;

    ToolSpec spec;
    spec.name = QStringLiteral("calculate");
    spec.category = QStringLiteral("compute");
    spec.description = QString::fromUtf8(kDescription);
    spec.effect = ToolEffect::Read;
    // Pure computation on the calling thread: no viewer, no widget, nothing to marshal.
    spec.affinity = ToolAffinity::Any;
    // Deliberately without an availability predicate. A calculator has to work with no
    // structure loaded, which is exactly the state in which three numbers are handed in.
    spec.paramSchema = schema();

    spec.handler = [dispatcher](const QJsonObject& args) {
        const QString source = args.value(QStringLiteral("source")).toString();

        QVariantMap bindings;
        if (args.contains(QStringLiteral("data"))) {
            const QJsonValue data = args.value(QStringLiteral("data"));
            const QString problem = checkNumbers(data, QStringLiteral("data"));
            if (!problem.isEmpty())
                return ToolResult::failure(problem);
            bindings.insert(QStringLiteral("data"), data.toArray().toVariantList());
        }

        ScriptInterpreter interpreter;
        if (dispatcher)
            interpreter.setStopPoll([dispatcher] { return dispatcher->isInterrupted(); });

        const ScriptResult result = interpreter.run(source, bindings);
        if (!result.ok)
            return ToolResult::failure(result.error.message);

        QJsonObject out;
        out.insert(QStringLiteral("value"), QJsonValue::fromVariant(result.value));
        if (!result.prints.isEmpty())
            out.insert(QStringLiteral("prints"), QJsonArray::fromStringList(result.prints));
        if (result.truncated)
            out.insert(QStringLiteral("truncated"), true);

        // One line for the model to read first: what was printed, or the value.
        QString text = result.prints.join(QStringLiteral(" | "));
        if (text.isEmpty() && result.value.isValid())
            text = QStringLiteral("value = %1").arg(describeValue(result.value));

        return ToolResult::success(out, text);
    };

    return registry.add(spec) ? 1 : 0;
}
