// simparameters.h - curcuma parameter values and conditions for the "All parameters"
// tab (UX stage 6 S3).
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026. Header-only and Qt-only, so the rules are testable without
// curcuma (test_recipes).
#pragma once

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <cmath>

namespace simparams {

/// A parameter value as text: strings unquoted, true/false, numbers in their shortest
/// form (up to 15 significant digits), arrays and objects as compact JSON.
inline QString valueText(const QJsonValue& v)
{
    switch (v.type()) {
    case QJsonValue::Bool:
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Double:
        return QString::number(v.toDouble(), 'g', 15);
    case QJsonValue::String:
        return v.toString();
    case QJsonValue::Array:
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    case QJsonValue::Object:
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    default:
        return QString();
    }
}

/// Equal as parameter values: numbers compare numerically (1 and 1.0 are the same).
inline bool sameValue(const QJsonValue& a, const QJsonValue& b)
{
    if (a.isDouble() && b.isDouble())
        return a.toDouble() == b.toDouble()
            || std::fabs(a.toDouble() - b.toDouble()) <= 1e-12 * std::fmax(std::fabs(a.toDouble()), std::fabs(b.toDouble()));
    return a == b;
}

/// Does a curcuma relevantWhen condition hold for @p values (the effective parameter
/// values)? Grammar of the PARAM annotation "requires=" (parameter_macros.h):
/// "key=value", "key!=value", or a bare "key" meaning a switch that is on. An empty
/// condition always holds; a key missing from @p values counts as off / empty.
inline bool conditionHolds(const QString& condition, const QJsonObject& values)
{
    const QString c = condition.trimmed();
    if (c.isEmpty())
        return true;
    const int ne = c.indexOf(QStringLiteral("!="));
    if (ne > 0)
        return valueText(values.value(c.left(ne).trimmed())) != c.mid(ne + 2).trimmed();
    const int eq = c.indexOf(QLatin1Char('='));
    if (eq > 0)
        return valueText(values.value(c.left(eq).trimmed())) == c.mid(eq + 1).trimmed();
    const QJsonValue v = values.value(c);
    if (v.isBool())
        return v.toBool();
    if (v.isDouble())
        return v.toDouble() != 0.0;
    if (v.isString())
        return v.toString() == QStringLiteral("true");
    return false;
}

} // namespace simparams
