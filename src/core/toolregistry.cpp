// toolregistry.cpp - The one catalogue of callable operations.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "toolregistry.h"

#include <QDebug>
#include <QJsonDocument>

#include <QJsonArray>
#include <QJsonValue>
#include <QMutexLocker>
#include <QSet>

bool needsApprovalAt(ToolEffect effect, ToolAutonomy autonomy)
{
    switch (autonomy) {
    case ToolAutonomy::Full:
        return false;
    case ToolAutonomy::InProgram:
        // Everything that stays inside the program. A structure edit lands in the
        // snapshot stack and Ctrl+Z takes it back; a calculation costs time and
        // nothing else. Writing a file and starting a program do not undo.
        return effect == ToolEffect::FileWrite || effect == ToolEffect::Process;
    case ToolAutonomy::Ask:
        return effect != ToolEffect::Read && effect != ToolEffect::Display;
    }
    return true;   // an unknown level is the careful one
}

QString toolAutonomyName(ToolAutonomy autonomy)
{
    switch (autonomy) {
    case ToolAutonomy::Ask:       return QStringLiteral("ask");
    case ToolAutonomy::InProgram: return QStringLiteral("auto in the program");
    case ToolAutonomy::Full:      return QStringLiteral("full auto");
    }
    return QStringLiteral("ask");
}

QString toolEffectName(ToolEffect effect)
{
    switch (effect) {
    case ToolEffect::Read:      return QStringLiteral("read");
    case ToolEffect::Display:   return QStringLiteral("display");
    case ToolEffect::Mutate:    return QStringLiteral("mutate");
    case ToolEffect::Compute:   return QStringLiteral("compute");
    case ToolEffect::FileWrite: return QStringLiteral("file_write");
    case ToolEffect::Process:   return QStringLiteral("process");
    }
    return QStringLiteral("read");
}

QString toolAffinityName(ToolAffinity affinity)
{
    return affinity == ToolAffinity::Gui ? QStringLiteral("gui") : QStringLiteral("any");
}

namespace {

const QStringList& allowedTypes()
{
    static const QStringList types = {
        QStringLiteral("string"), QStringLiteral("number"), QStringLiteral("integer"),
        QStringLiteral("boolean"), QStringLiteral("array"), QStringLiteral("object")
    };
    return types;
}

const QStringList& allowedPropertyKeys()
{
    static const QStringList keys = {
        QStringLiteral("type"), QStringLiteral("description"), QStringLiteral("enum"),
        QStringLiteral("minimum"), QStringLiteral("maximum")
    };
    return keys;
}

/// Does @p value satisfy the JSON type named by @p type?
bool typeMatches(const QString& type, const QJsonValue& value)
{
    if (type == QLatin1String("string"))  return value.isString();
    if (type == QLatin1String("boolean")) return value.isBool();
    if (type == QLatin1String("array"))   return value.isArray();
    if (type == QLatin1String("object"))  return value.isObject();
    if (type == QLatin1String("number"))  return value.isDouble();
    if (type == QLatin1String("integer")) {
        if (!value.isDouble())
            return false;
        const double d = value.toDouble();
        // JSON has no integer type; accept a whole number and nothing else, so
        // "steps": 10.5 is rejected instead of silently truncated.
        return d == static_cast<double>(static_cast<qint64>(d));
    }
    return false;
}

QString describe(const QJsonValue& value)
{
    if (value.isString())  return QStringLiteral("string");
    if (value.isBool())    return QStringLiteral("boolean");
    if (value.isDouble())  return QStringLiteral("number");
    if (value.isArray())   return QStringLiteral("array");
    if (value.isObject())  return QStringLiteral("object");
    if (value.isNull())    return QStringLiteral("null");
    return QStringLiteral("undefined");
}

}  // namespace

bool ToolRegistry::isValidSchema(const QJsonObject& schema, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };

    if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object"))
        return fail(QStringLiteral("schema must have \"type\": \"object\""));

    for (const QString& key : schema.keys()) {
        if (key != QLatin1String("type") && key != QLatin1String("properties")
            && key != QLatin1String("required") && key != QLatin1String("description")) {
            return fail(QStringLiteral("schema key \"%1\" is not honoured by the validator").arg(key));
        }
    }

    const QJsonValue propsValue = schema.value(QStringLiteral("properties"));
    if (!propsValue.isUndefined() && !propsValue.isObject())
        return fail(QStringLiteral("\"properties\" must be an object"));
    const QJsonObject properties = propsValue.toObject();

    for (auto it = properties.begin(); it != properties.end(); ++it) {
        if (!it.value().isObject())
            return fail(QStringLiteral("property \"%1\" must be an object").arg(it.key()));
        const QJsonObject prop = it.value().toObject();

        const QString type = prop.value(QStringLiteral("type")).toString();
        if (!allowedTypes().contains(type)) {
            return fail(QStringLiteral("property \"%1\" has unsupported type \"%2\"")
                            .arg(it.key(), type));
        }
        for (const QString& key : prop.keys()) {
            if (!allowedPropertyKeys().contains(key)) {
                return fail(QStringLiteral("property \"%1\": key \"%2\" is not honoured")
                                .arg(it.key(), key));
            }
        }
        const QJsonValue enumValue = prop.value(QStringLiteral("enum"));
        if (!enumValue.isUndefined()) {
            if (!enumValue.isArray() || enumValue.toArray().isEmpty())
                return fail(QStringLiteral("property \"%1\": \"enum\" must be a non-empty array").arg(it.key()));
        }
        for (const QString& bound : { QStringLiteral("minimum"), QStringLiteral("maximum") }) {
            const QJsonValue v = prop.value(bound);
            if (v.isUndefined())
                continue;
            if (!v.isDouble())
                return fail(QStringLiteral("property \"%1\": \"%2\" must be a number").arg(it.key(), bound));
            if (type != QLatin1String("number") && type != QLatin1String("integer")) {
                return fail(QStringLiteral("property \"%1\": \"%2\" only applies to numbers")
                                .arg(it.key(), bound));
            }
        }
    }

    const QJsonValue requiredValue = schema.value(QStringLiteral("required"));
    if (!requiredValue.isUndefined()) {
        if (!requiredValue.isArray())
            return fail(QStringLiteral("\"required\" must be an array"));
        for (const QJsonValue& entry : requiredValue.toArray()) {
            if (!entry.isString())
                return fail(QStringLiteral("\"required\" must hold strings"));
            if (!properties.contains(entry.toString())) {
                return fail(QStringLiteral("\"required\" names \"%1\", which is not a declared property")
                                .arg(entry.toString()));
            }
        }
    }

    if (error)
        error->clear();
    return true;
}

ToolValidation ToolRegistry::validateAgainst(const QJsonObject& schema, const QJsonObject& args)
{
    ToolValidation result;
    const auto fail = [&result](const QString& message) {
        result.ok = false;
        result.error = message;
        return result;
    };

    const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();

    // Unknown keys are rejected rather than dropped: a model that invents a
    // parameter has misunderstood the tool and needs to hear so.
    for (const QString& key : args.keys()) {
        if (!properties.contains(key))
            return fail(QStringLiteral("unknown parameter \"%1\"").arg(key));
    }

    for (const QJsonValue& entry : schema.value(QStringLiteral("required")).toArray()) {
        const QString name = entry.toString();
        if (!args.contains(name))
            return fail(QStringLiteral("missing required parameter \"%1\"").arg(name));
    }

    for (auto it = args.begin(); it != args.end(); ++it) {
        const QJsonObject prop = properties.value(it.key()).toObject();
        const QString type = prop.value(QStringLiteral("type")).toString();
        const QJsonValue value = it.value();

        if (!typeMatches(type, value)) {
            return fail(QStringLiteral("parameter \"%1\" expects %2, got %3")
                            .arg(it.key(), type, describe(value)));
        }

        const QJsonValue enumValue = prop.value(QStringLiteral("enum"));
        if (enumValue.isArray()) {
            const QJsonArray allowed = enumValue.toArray();
            if (!allowed.contains(value)) {
                QStringList shown;
                for (const QJsonValue& option : allowed)
                    shown << option.toVariant().toString();
                return fail(QStringLiteral("parameter \"%1\" must be one of: %2")
                                .arg(it.key(), shown.join(QStringLiteral(", "))));
            }
        }

        const QJsonValue minimum = prop.value(QStringLiteral("minimum"));
        if (minimum.isDouble() && value.toDouble() < minimum.toDouble()) {
            return fail(QStringLiteral("parameter \"%1\" must be >= %2")
                            .arg(it.key()).arg(minimum.toDouble()));
        }
        const QJsonValue maximum = prop.value(QStringLiteral("maximum"));
        if (maximum.isDouble() && value.toDouble() > maximum.toDouble()) {
            return fail(QStringLiteral("parameter \"%1\" must be <= %2")
                            .arg(it.key()).arg(maximum.toDouble()));
        }
    }

    result.ok = true;
    return result;
}

ToolRegistry& ToolRegistry::instance()
{
    static ToolRegistry registry;
    return registry;
}

int ToolRegistry::catalogueBytes() const
{
    QJsonArray catalogue;
    for (const ToolSpec& spec : all()) {
        QJsonObject function;
        function.insert(QStringLiteral("name"), spec.name);
        function.insert(QStringLiteral("description"), spec.description);
        function.insert(QStringLiteral("parameters"), spec.paramSchema);
        QJsonObject entry;
        entry.insert(QStringLiteral("type"), QStringLiteral("function"));
        entry.insert(QStringLiteral("function"), function);
        catalogue.append(entry);
    }
    return QJsonDocument(catalogue).toJson(QJsonDocument::Compact).size();
}

bool ToolRegistry::add(const ToolSpec& spec, QString* error)
{
    // Claude Generated 2026 - Warn as well as report. Registration sites count how
    // many tools were added and mostly pass no error pointer, so a schema the
    // validator refuses used to make a tool disappear without a word -- which is
    // exactly what an unenforceable key like "items" did. The warning reaches the
    // LogHub, so a missing tool is visible in the Output dock.
    const auto fail = [error](const QString& message) {
        qWarning().noquote() << "tool not registered:" << message;
        if (error)
            *error = message;
        return false;
    };

    if (spec.name.isEmpty())
        return fail(QStringLiteral("a tool needs a name"));
    if (!spec.handler)
        return fail(QStringLiteral("tool \"%1\" has no handler").arg(spec.name));

    QString schemaError;
    if (!isValidSchema(spec.paramSchema, &schemaError))
        return fail(QStringLiteral("tool \"%1\": %2").arg(spec.name, schemaError));

    QMutexLocker lock(&m_mutex);
    if (m_tools.contains(spec.name))
        return fail(QStringLiteral("tool \"%1\" is already registered").arg(spec.name));
    m_tools.insert(spec.name, spec);

    if (error)
        error->clear();
    return true;
}

bool ToolRegistry::contains(const QString& name) const
{
    QMutexLocker lock(&m_mutex);
    return m_tools.contains(name);
}

bool ToolRegistry::tool(const QString& name, ToolSpec& out) const
{
    QMutexLocker lock(&m_mutex);
    const auto it = m_tools.constFind(name);
    if (it == m_tools.constEnd())
        return false;
    out = it.value();  // a copy: the caller must not hold a pointer into the table
    return true;
}

QStringList ToolRegistry::names() const
{
    QMutexLocker lock(&m_mutex);
    QStringList result = m_tools.keys();
    result.sort();
    return result;
}

QVector<ToolSpec> ToolRegistry::all() const
{
    QMutexLocker lock(&m_mutex);
    QVector<ToolSpec> result;
    result.reserve(m_tools.size());
    QStringList sorted = m_tools.keys();
    sorted.sort();
    for (const QString& name : sorted)
        result.append(m_tools.value(name));
    return result;
}

int ToolRegistry::size() const
{
    QMutexLocker lock(&m_mutex);
    return m_tools.size();
}

void ToolRegistry::clear()
{
    QMutexLocker lock(&m_mutex);
    m_tools.clear();
}

ToolValidation ToolRegistry::validate(const QString& name, const QJsonObject& args) const
{
    ToolSpec spec;
    if (!tool(name, spec)) {
        ToolValidation v;
        v.ok = false;
        v.error = QStringLiteral("unknown tool \"%1\"").arg(name);
        return v;
    }
    return validateAgainst(spec.paramSchema, args);
}

ToolResult ToolRegistry::invoke(const QString& name, const QJsonObject& args) const
{
    ToolSpec spec;
    if (!tool(name, spec))
        return ToolResult::failure(QStringLiteral("unknown tool \"%1\"").arg(name));

    const ToolValidation validation = validateAgainst(spec.paramSchema, args);
    if (!validation.ok)
        return ToolResult::failure(validation.error);

    return spec.handler(args);
}
