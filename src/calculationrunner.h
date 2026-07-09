// calculationrunner.h - External-process orchestration for calculations.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - WP T3 orchestration layer, extracted from MainWindow.
// MainWindow validates the user's input and assembles a CalculationRequest from
// its widgets + settings; CalculationRunner writes the structure/input files,
// builds the per-program (curcuma/orca/xtb) argument list, drives the QProcess,
// and reports back via signals so MainWindow keeps every piece of UI feedback
// (progress dialog, timer, status bar, output view, workflow state).

#pragma once

#include "calculationhistory.h"  // CalculationEntry

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

/// All data the runner needs to launch one calculation. Assembled by MainWindow
/// from its input widgets + settings, so the runner depends on neither.
struct CalculationRequest {
    QString program;         ///< "curcuma" | "orca" | "xtb"
    QString command;         ///< trimmed command-line argument string
    QString structureText;   ///< structure-editor contents (written to disk)
    QString inputText;       ///< input-editor contents (written to disk)
    QString structureBase;   ///< base name of the structure file
    QString structureExt;    ///< structure-file extension (no dot)
    QString inputBase;       ///< base name of the input file
    QString inputExt;        ///< input-file extension (no dot)
    int threads = 1;         ///< OMP_NUM_THREADS (curcuma/xtb)
    bool uniqueFileNames = false;  ///< append a timestamp to generated file names
    QString programPath;     ///< resolved curcuma/xtb executable (empty for orca)
    QString orcaBinaryPath;  ///< ORCA install dir (orca / orca_* live here)
    QString calcDir;         ///< absolute calculation directory
};

/// Owns the calculation QProcess and the per-program completer command lists.
/// Runs one calculation at a time; MainWindow validates the request beforehand.
class CalculationRunner : public QObject {
    Q_OBJECT
public:
    explicit CalculationRunner(QObject* parent = nullptr);

    /// Write the structure/input files, build the argument list, and start the
    /// process. Returns the CalculationEntry describing the started run (status
    /// "started"); the same entry is echoed back by finished(). Assumes @p req
    /// has already been validated by the caller.
    CalculationEntry start(const CalculationRequest& req);

    /// Kill the running process (no-op if nothing is running).
    void cancel();

    /// True while the process is running.
    bool isRunning() const;

    /// Completer commands for @p program ("curcuma"/"xtb"); empty otherwise.
    QStringList commandsFor(const QString& program) const;

signals:
    /// A stdout chunk arrived (only fires for processes that don't redirect
    /// stdout to a file).
    void outputReceived(const QString& text);
    /// A stderr chunk arrived.
    void errorReceived(const QString& text);
    /// The process finished; @p entry is the run from start(), @p exitCode its
    /// exit code.
    void finished(const CalculationEntry& entry, int exitCode);

private:
    void handleFinished(int exitCode);

    QProcess* m_process = nullptr;
    QMap<QString, QStringList> m_programCommands;
    CalculationEntry m_entry;  ///< the currently-running run (echoed by finished())
    QString m_trjFile;         ///< xtb: rename target for xtbopt.xyz/.log on finish
};
