// curcumajob.h - One curcuma calculation, in process, on a worker thread.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The in-process path is the one qurcuma keeps; the
// QProcess route stays for ORCA and xtb. Running curcuma inside the GUI means four
// things a child process would have handed over for free, and each is now
// provided: CurcumaLogger::SinkScope captures the output, the verbosity and the
// stop request are per-run rather than per-process, pinOutputDir() keeps the
// user's project clean, and Results() hands the outcome back without a file.
//
// One calculation at a time. curcuma_core is OpenMP-parallel and the interactive
// MD already owns a thread pool; two compute jobs would simply compete for the
// same cores.
#pragma once

#include "core/moleculedata.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

class LogHub;

/// What to calculate. The structure travels as atoms rather than a path, because
/// the interesting case is "the thing currently on screen", which has no file.
struct CurcumaJobRequest {
    QString command;                    ///< "sp" | "rmsd"
    QVector<moldata::Atom> atoms;       ///< the structure, or the reference for rmsd
    QVector<moldata::Atom> secondAtoms; ///< the target, for commands that take two
    QJsonObject controller;             ///< curcuma controller, module-nested
};

struct CurcumaJobResult {
    QString jobId;
    QString command;
    bool ok = false;
    QJsonObject results;   ///< what CurcumaMethod::Results() returned
    QString error;
    qint64 elapsedMs = 0;
};

class CurcumaJob : public QObject {
    Q_OBJECT

public:
    explicit CurcumaJob(LogHub* hub, QObject* parent = nullptr);
    ~CurcumaJob() override;

    /// Commands this can run in process. Anything else is refused by name rather
    /// than attempted and failed halfway.
    static QStringList supportedCommands();

    /// Start @p request. Returns the job id, or an empty string when another job
    /// is already running or the command is unknown -- @p error then says which.
    QString start(const CurcumaJobRequest& request, QString* error = nullptr);

    /// Ask the running job to stop. Per run, not the process-wide "stop" file.
    void requestStop();

    bool isRunning() const;
    QString runningJobId() const;

signals:
    void finished(const CurcumaJobResult& result);

private:
    class Runner;
    Runner* m_runner = nullptr;
    LogHub* m_hub = nullptr;
};
