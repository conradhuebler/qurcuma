// curcuma_schemas.cpp - Tool schemas derived from curcuma's parameter registry.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "curcuma_schemas.h"

#include <src/core/parameter_registry.h>

#include "generated/parameter_registry.h"

#include <QJsonArray>

namespace {

void ensureRegistry()
{
    if (ParameterRegistry::getInstance().getForModule("rmsd").empty())
        initialize_generated_registry();
}

/// Parameters that curcuma cannot mark as irrelevant because it has no way to say
/// so: `sp` and `opt` share one module, and the optimiser settings mean nothing for
/// a single point. Named here rather than filtered silently, and small on purpose --
/// a long list would mean the registry is missing something it should express.
QStringList notApplicable(const QString& command)
{
    if (command == QLatin1String("sp"))
        return { QStringLiteral("optimizer") };
    return {};
}

QString jsonTypeOf(ParamType type)
{
    switch (type) {
    case ParamType::Int:        return QStringLiteral("integer");
    case ParamType::Double:     return QStringLiteral("number");
    case ParamType::Bool:       return QStringLiteral("boolean");
    case ParamType::StringList: return QStringLiteral("array");
    case ParamType::Json:       return QStringLiteral("object");
    case ParamType::String:
    case ParamType::Selection:
    case ParamType::Path:       return QStringLiteral("string");
    }
    return QStringLiteral("string");
}

QString defaultAsText(const ParameterDefinition& p)
{
    try {
        switch (p.type) {
        case ParamType::Int:    return QString::number(std::any_cast<int>(p.defaultValue));
        case ParamType::Double: return QString::number(std::any_cast<double>(p.defaultValue));
        case ParamType::Bool:   return std::any_cast<bool>(p.defaultValue) ? QStringLiteral("true")
                                                                           : QStringLiteral("false");
        default:                return QString::fromStdString(std::any_cast<std::string>(p.defaultValue));
        }
    } catch (const std::bad_any_cast&) {
        return QString();
    }
}

/// The description a model reads. Unit and relevance go in here rather than as
/// extra schema keys: the tool registry honours only what it can enforce, and a
/// key it ignores would be a promise nobody keeps.
QString describe(const ParameterDefinition& p)
{
    QString text = QString::fromStdString(p.helpText);
    if (!p.unit.empty())
        text += QStringLiteral(" [%1]").arg(QString::fromStdString(p.unit));
    const QString fallback = defaultAsText(p);
    if (!fallback.isEmpty())
        text += QStringLiteral(" (default: %1)").arg(fallback);
    if (!p.relevantWhen.empty()) {
        text += QStringLiteral(" Only applies when %1.")
                    .arg(QString::fromStdString(p.relevantWhen));
    }
    return text;
}

QJsonObject propertyFor(const ParameterDefinition& p)
{
    QJsonObject prop;
    prop.insert(QStringLiteral("type"), jsonTypeOf(p.type));
    prop.insert(QStringLiteral("description"), describe(p));
    if (!p.allowed.empty()) {
        QJsonArray values;
        for (const std::string& value : p.allowed)
            values.append(QString::fromStdString(value));
        prop.insert(QStringLiteral("enum"), values);
    }
    if (p.hasMinimum)
        prop.insert(QStringLiteral("minimum"), p.minimum);
    if (p.hasMaximum)
        prop.insert(QStringLiteral("maximum"), p.maximum);
    return prop;
}

}  // namespace

QStringList curcumaJobCommands()
{
    return { QStringLiteral("sp"), QStringLiteral("rmsd") };
}

QJsonObject curcumaJobSchema(const QString& command)
{
    ensureRegistry();
    QJsonObject properties;
    const QStringList skip = notApplicable(command);

    for (const std::string& module : ParameterRegistry::getInstance()
                                         .modulesForCommand(command.toStdString())) {
        for (const ParameterDefinition& p : ParameterRegistry::getInstance().getForModule(module)) {
            if (p.tier != ParamTier::Primary || p.deprecated)
                continue;
            const QString name = QString::fromStdString(p.name);
            if (skip.contains(name))
                continue;
            properties.insert(name, propertyFor(p));
        }
    }

    QJsonObject schema;
    schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    schema.insert(QStringLiteral("properties"), properties);
    return schema;
}

QJsonObject curcumaJobDetails(const QString& command)
{
    ensureRegistry();
    QJsonObject out;
    out.insert(QStringLiteral("command"), command);

    QJsonArray modules;
    for (const std::string& module : ParameterRegistry::getInstance()
                                         .modulesForCommand(command.toStdString())) {
        QJsonObject entry;
        entry.insert(QStringLiteral("module"), QString::fromStdString(module));
        if (const ModuleDefinition* definition = ParameterRegistry::getInstance().findModule(module)) {
            entry.insert(QStringLiteral("description"),
                QString::fromStdString(definition->description));
            entry.insert(QStringLiteral("category"), QString::fromStdString(definition->category));
        }

        QJsonArray parameters;
        for (const ParameterDefinition& p : ParameterRegistry::getInstance().getForModule(module)) {
            QJsonObject one;
            one.insert(QStringLiteral("name"), QString::fromStdString(p.name));
            one.insert(QStringLiteral("type"), jsonTypeOf(p.type));
            one.insert(QStringLiteral("category"), QString::fromStdString(p.category));
            one.insert(QStringLiteral("description"), describe(p));
            one.insert(QStringLiteral("tier"),
                p.tier == ParamTier::Primary ? QStringLiteral("primary")
                    : p.tier == ParamTier::Expert ? QStringLiteral("expert")
                                                  : QStringLiteral("advanced"));
            if (!p.allowed.empty()) {
                QJsonArray values;
                for (const std::string& value : p.allowed)
                    values.append(QString::fromStdString(value));
                one.insert(QStringLiteral("allowed"), values);
            }
            if (p.deprecated)
                one.insert(QStringLiteral("deprecated"), true);
            parameters.append(one);
        }
        entry.insert(QStringLiteral("parameters"), parameters);
        entry.insert(QStringLiteral("parameter_count"), parameters.size());
        modules.append(entry);
    }

    out.insert(QStringLiteral("modules"), modules);
    out.insert(QStringLiteral("runs_in_process"), curcumaJobCommands().contains(command));
    return out;
}
