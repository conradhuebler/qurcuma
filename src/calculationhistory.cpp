// calculationhistory.cpp - Calculation-history persistence.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026.

#include "calculationhistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QList<CalculationEntry> CalculationHistory::load(const QString& dir)
{
    QList<CalculationEntry> history;
    QFile file(dir + "/calculations.json");
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray calculations = doc.object()["calculations"].toArray();
        for (const auto& calcRef : calculations) {
            const QJsonObject calc = calcRef.toObject();
            CalculationEntry entry;
            entry.id = calc["id"].toString();
            entry.program = calc["program"].toString();
            entry.command = calc["command"].toString();
            entry.structureFile = calc["structureFile"].toString();
            entry.outputFile = calc["outputFile"].toString();
            entry.timestamp = QDateTime::fromString(calc["timestamp"].toString(), Qt::ISODate);
            entry.status = calc["status"].toString();
            history.append(entry);
        }
        file.close();
    }
    return history;
}

void CalculationHistory::add(const QString& dir, const CalculationEntry& entry, bool uniqueFileNames)
{
    QList<CalculationEntry> history = load(dir);

    // Update the existing entry with the same id, or append a new one.
    bool updated = false;
    for (int i = 0; i < history.size(); ++i) {
        if (history[i].id == entry.id) {
            history[i] = entry;
            updated = true;
            break;
        }
    }
    if (!updated)
        history.append(entry);

    QJsonArray jsonArray;
    for (const auto& calc : history) {
        QJsonObject calcObj;
        calcObj["id"] = calc.id;
        calcObj["program"] = calc.program;
        calcObj["command"] = calc.command;
        calcObj["structureFile"] = calc.structureFile;
        calcObj["outputFile"] = calc.outputFile;
        calcObj["timestamp"] = calc.timestamp.toString(Qt::ISODate);
        calcObj["status"] = calc.status;
        calcObj["unqiueFileNames"] = uniqueFileNames;  // legacy write-only key (kept as-is)
        jsonArray.append(calcObj);
    }

    QJsonObject rootObj;
    rootObj["calculations"] = jsonArray;

    QFile file(dir + QDir::separator() + "calculations.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
        file.close();
    }
}
