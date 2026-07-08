// calculationhistory.h - Persistence for the per-directory calculation history.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Extracted from MainWindow (WP T3, model layer): the
// calculations.json load/append logic, which is pure file I/O with no UI. The
// calculation *orchestration* (runSimulation/runCommand/processOutput) stays in
// MainWindow — it is deeply coupled to the input widgets and process lifetime.

#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

/// One recorded calculation run (persisted in <dir>/calculations.json).
struct CalculationEntry {
    QString id;            // unique id (e.g. timestamp)
    QString program;
    QString command;
    QString structureFile;
    QString inputFile;
    QString outputFile;
    QDateTime timestamp;
    QString status;        // "started", "completed", "error"
};

namespace CalculationHistory {

/// Load all entries from <dir>/calculations.json (empty if missing/unreadable).
QList<CalculationEntry> load(const QString& dir);

/// Append @p entry (or update the existing entry with the same id) and rewrite
/// <dir>/calculations.json.
void add(const QString& dir, const CalculationEntry& entry, bool uniqueFileNames);

}  // namespace CalculationHistory
