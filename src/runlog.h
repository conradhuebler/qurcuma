// runlog.h - Which simulation parameters each run changed (UX stage 6 S4).
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026. Header-only and Qt-only (tested in test_recipes).
//
// Every MD run and every optimization records the parameters it sent with a value
// different from curcuma's default, and where each came from: the Simulation tab,
// the All parameters tab, or a setting qurcuma fixes for the interactive run. The
// records are appended as one JSON object per line to a local file, so that the
// decision which parameters belong to the basic view and which to the expert view
// can rest on counts over many runs.
//
// Record: {"time": ISO-8601, "mode": "md"|"opt", "method": ..., "optimizer": ... (opt),
//          "atoms": N, "changed": [{"name", "value", "default", "source"}]}
// source: "simulation", "all-parameters" or "qurcuma"; "default" is null when curcuma
// has no default for the key.
#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>

#include "simparameters.h"

namespace runlog {

/// The log file: run-parameters.jsonl in qurcuma's application data directory.
inline QString defaultPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("run-parameters.jsonl"));
}

/// Append one record as a line; creates the directory when needed.
inline bool append(const QString& path, const QJsonObject& record)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;
    f.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
    f.write("\n");
    return true;
}

/// All records of the file; lines that are not a JSON object are skipped.
inline QVector<QJsonObject> readAll(const QString& path)
{
    QVector<QJsonObject> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject())
            out.append(doc.object());
    }
    return out;
}

inline QString sourceLabel(const QString& source)
{
    if (source == QLatin1String("all-parameters"))
        return QStringLiteral("All parameters");
    if (source == QLatin1String("qurcuma"))
        return QStringLiteral("fixed by qurcuma");
    return QStringLiteral("Simulation tab");
}

/// The record as text for the Output panel.
inline QString summary(const QJsonObject& record)
{
    const bool md = record.value("mode").toString() == QLatin1String("md");
    QString head = md ? QStringLiteral("MD run") : QStringLiteral("Optimization");
    QStringList what = { record.value("method").toString() };
    if (!md && record.contains("optimizer"))
        what << QStringLiteral("optimizer %1").arg(record.value("optimizer").toString());
    what << QStringLiteral("%1 atoms").arg(record.value("atoms").toInt());
    const QJsonArray changed = record.value("changed").toArray();
    QString text = QStringLiteral("%1 (%2): %3 parameter(s) differ from curcuma's defaults\n")
                       .arg(head, what.join(QStringLiteral(", ")))
                       .arg(changed.size());
    for (const QJsonValue& v : changed) {
        const QJsonObject c = v.toObject();
        const QJsonValue def = c.value("default");
        text += QStringLiteral("  %1 = %2 (default %3) [%4]\n")
                    .arg(c.value("name").toString(), simparams::valueText(c.value("value")),
                         def.isNull() || def.isUndefined() ? QStringLiteral("none")
                                                           : simparams::valueText(def),
                         sourceLabel(c.value("source").toString()));
    }
    return text;
}

/// How often a parameter differed from its default, per run mode and source.
struct Count {
    QString mode;     // "md" or "opt"
    QString name;
    QString source;
    int changed = 0;  // runs of this mode in which it differed
    int runs = 0;     // all logged runs of this mode
};

/// Counts over @p records, most frequently changed first.
inline QVector<Count> tally(const QVector<QJsonObject>& records)
{
    QHash<QString, int> runsPerMode;
    QHash<QString, Count> counts;
    QStringList order;
    for (const QJsonObject& r : records) {
        const QString mode = r.value("mode").toString();
        ++runsPerMode[mode];
        for (const QJsonValue& v : r.value("changed").toArray()) {
            const QJsonObject c = v.toObject();
            const QString source = c.value("source").toString();
            const QString key = mode + QLatin1Char('\x1f') + c.value("name").toString()
                + QLatin1Char('\x1f') + source;
            if (!counts.contains(key)) {
                counts.insert(key, { mode, c.value("name").toString(), source, 0, 0 });
                order << key;
            }
            ++counts[key].changed;
        }
    }
    QVector<Count> out;
    for (const QString& key : order) {
        Count c = counts.value(key);
        c.runs = runsPerMode.value(c.mode);
        out.append(c);
    }
    std::stable_sort(out.begin(), out.end(), [](const Count& a, const Count& b) {
        const double sa = a.runs ? double(a.changed) / a.runs : 0.0;
        const double sb = b.runs ? double(b.changed) / b.runs : 0.0;
        return sa > sb;
    });
    return out;
}

} // namespace runlog
