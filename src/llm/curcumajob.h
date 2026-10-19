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
// One calculation at a time, but not one request at a time. curcuma_core is
// OpenMP-parallel and the interactive MD already owns a thread pool, so two jobs
// running at once would only take cores from each other -- a further request is
// therefore queued, not refused. Refusing it was a real failure in use: a model
// asks for the whole complex and both fragments in a single turn, which is how
// tool calling works, and two of the three answers were simply thrown away.
#pragma once

#include "core/moleculedata.h"

#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
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

/// What the job runner is doing right now. A snapshot, safe to ask for from any
/// thread -- the agent loop does not run on the GUI thread.
struct CurcumaJobState {
    QString running;        ///< empty when idle
    QStringList queued;     ///< waiting, in the order they will run
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

    /// Start @p request, or queue it behind whatever is already running. Returns
    /// the job id, or an empty string when the command is unknown -- @p error then
    /// says so. @p position, when given, is 0 if the job started at once and N if
    /// it is Nth in the queue.
    QString start(const CurcumaJobRequest& request, QString* error = nullptr,
                  int* position = nullptr);

    /// Ask the running job to stop and drop everything still queued. Per run, not
    /// the process-wide "stop" file.
    void requestStop();

    bool isRunning() const;
    QString runningJobId() const;

    /// Running job and queue, safe to call from any thread.
    CurcumaJobState state() const;

signals:
    /// A job left the queue and is now computing.
    void started(const QString& jobId);
    void finished(const CurcumaJobResult& result);

private:
    class Runner;

    /// One queued request, waiting for its turn. It keeps its id from the moment it
    /// is accepted, so the caller can ask after it before it ever runs.
    struct Pending {
        CurcumaJobRequest request;
        QString jobId;
    };

    void launch(const CurcumaJobRequest& request, const QString& jobId);
    void runnerDone();
    void startNext();

    Runner* m_runner = nullptr;
    LogHub* m_hub = nullptr;
    QList<Pending> m_queue;

    /// Guards the two fields below only. They are written on the thread that owns
    /// this object and read from the agent loop's thread.
    mutable QMutex m_stateMutex;
    QString m_runningId;
    QStringList m_queuedIds;
};
