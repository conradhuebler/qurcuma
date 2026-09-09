// curcumajob.cpp - One curcuma calculation, in process, on a worker thread.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "curcumajob.h"

#include "core/loghub.h"
#include "moleculebridge.h"

#include <src/capabilities/rmsd.h>
#include <src/core/curcuma_logger.h>
#include <src/core/energycalculator.h>
#include <src/core/molecule.h>
#include <src/core/parameter_registry.h>

#include "generated/parameter_registry.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QThread>
#include <QUuid>

#include <atomic>

namespace {

/// Make sure curcuma's parameter registry is populated.
///
/// Claude Generated 2026 - ConfigManager throws when a module has no defaults, and
/// the registry is filled by a generated function that only qurcuma's main() used
/// to call. Anything else embedding curcuma -- a test, a headless run -- got a
/// throw from deep inside a driver instead. Checking for content rather than
/// keeping a flag makes this idempotent next to main()'s own call.
void ensureParameterRegistry()
{
    if (ParameterRegistry::getInstance().getForModule("rmsd").empty())
        initialize_generated_registry();
}

/// Convert qurcuma's render records into what curcuma computes with.
curcuma::Molecule toMolecule(const QVector<moldata::Atom>& atoms)
{
    return atomsToMolecule(atoms);
}

LogLevel toLogLevel(CurcumaLogger::SinkLevel level)
{
    switch (level) {
    case CurcumaLogger::SinkLevel::Debug:   return LogLevel::Debug;
    case CurcumaLogger::SinkLevel::Info:    return LogLevel::Info;
    case CurcumaLogger::SinkLevel::Warning: return LogLevel::Warning;
    case CurcumaLogger::SinkLevel::Error:   return LogLevel::Error;
    }
    return LogLevel::Info;
}

}  // namespace

/// Runs on its own thread and owns the stop flag.
class CurcumaJob::Runner : public QThread {
public:
    Runner(CurcumaJob* owner, LogHub* hub, CurcumaJobRequest request, QString jobId)
        : QThread(owner)
        , m_owner(owner)
        , m_hub(hub)
        , m_request(std::move(request))
        , m_jobId(std::move(jobId))
    {
    }

    void requestStop()
    {
        m_stopRequested.store(true);
        if (m_driver)
            m_driver->requestStop();
    }

    QString jobId() const { return m_jobId; }

protected:
    void run() override
    {
        CurcumaJobResult result;
        result.jobId = m_jobId;
        result.command = m_request.command;

        QElapsedTimer timer;
        timer.start();

        // Everything curcuma logs on THIS thread, for as long as this scope lives,
        // lands in the hub tagged with the job. That is the whole reason the sink
        // is per-thread: the interactive MD is logging at the same time.
        const QString jobId = m_jobId;
        LogHub* hub = m_hub;
        CurcumaLogger::SinkScope capture(
            [hub, jobId](CurcumaLogger::SinkLevel level, const std::string& text) {
                if (hub)
                    hub->append(QStringLiteral("curcuma"), toLogLevel(level),
                        QString::fromStdString(text), jobId);
            });
        // Loud enough to be worth capturing. The process default stays untouched --
        // this is the thread's own override.
        CurcumaLogger::set_thread_verbosity(2);

        try {
            ensureParameterRegistry();
            if (m_request.command == QLatin1String("sp"))
                runSinglePoint(result);
            else if (m_request.command == QLatin1String("rmsd"))
                runRmsd(result);
            else
                result.error = QStringLiteral("no in-process runner for \"%1\"").arg(m_request.command);
        } catch (const std::exception& e) {
            // curcuma throws on a missing parameter, among other things. A GUI must
            // not die because a model asked for something odd.
            result.error = QStringLiteral("curcuma raised: %1").arg(QString::fromUtf8(e.what()));
        } catch (...) {
            result.error = QStringLiteral("curcuma raised an unknown exception");
        }

        if (m_stopRequested.load() && result.error.isEmpty() && !result.ok)
            result.error = QStringLiteral("stopped on request");

        result.elapsedMs = timer.elapsed();
        CurcumaLogger::set_thread_verbosity(-1);
        emit m_owner->finished(result);
    }

private:
    void runSinglePoint(CurcumaJobResult& result)
    {
        if (m_request.atoms.isEmpty()) {
            result.error = QStringLiteral("no structure given");
            return;
        }
        const QJsonObject global = m_request.controller;
        const QString method = global.value(QStringLiteral("method")).toString(QStringLiteral("gfnff"));

        json controller = json::object();
        controller["method"] = method.toStdString();
        controller["verbosity"] = 2;

        const curcuma::Molecule molecule = toMolecule(m_request.atoms);
        EnergyCalculator calculator(method.toStdString(), controller);
        calculator.setMolecule(molecule.getMolInfo());
        const double energy = calculator.CalculateEnergy(false);

        // An energy calculator that failed still returns a number; asking it
        // outright is the difference between a result and a plausible-looking zero.
        if (calculator.Error()) {
            result.error = QStringLiteral("energy calculation failed: %1")
                               .arg(QString::fromStdString(calculator.ErrorMessage()));
            return;
        }

        QJsonObject data;
        data.insert(QStringLiteral("energy"), energy);
        data.insert(QStringLiteral("unit"), QStringLiteral("Eh"));
        data.insert(QStringLiteral("method"), method);
        data.insert(QStringLiteral("atom_count"), m_request.atoms.size());
        result.results = data;
        result.ok = true;
    }

    void runRmsd(CurcumaJobResult& result)
    {
        if (m_request.atoms.isEmpty() || m_request.secondAtoms.isEmpty()) {
            result.error = QStringLiteral("rmsd needs two structures");
            return;
        }

        json controller = json::object();
        const QJsonObject rmsdConfig = m_request.controller.value(QStringLiteral("rmsd")).toObject();
        for (auto it = rmsdConfig.begin(); it != rmsdConfig.end(); ++it) {
            const QJsonValue value = it.value();
            if (value.isBool())
                controller[it.key().toStdString()] = value.toBool();
            else if (value.isDouble())
                controller[it.key().toStdString()] = value.toDouble();
            else
                controller[it.key().toStdString()] = value.toString().toStdString();
        }

        RMSDDriver driver(controller, true);
        m_driver = &driver;
        // No BMT directory: a GUI asking for a number must not leave a timestamped
        // folder in the user's project.
        driver.pinOutputDir(QString().toStdString());
        driver.setReference(toMolecule(m_request.atoms));
        driver.setTarget(toMolecule(m_request.secondAtoms));
        driver.start();
        m_driver = nullptr;

        result.results = QJsonDocument::fromJson(
            QByteArray::fromStdString(driver.Results().dump())).object();
        result.ok = true;
    }

    CurcumaJob* m_owner = nullptr;
    LogHub* m_hub = nullptr;
    CurcumaJobRequest m_request;
    QString m_jobId;
    std::atomic<bool> m_stopRequested { false };
    CurcumaMethod* m_driver = nullptr;   ///< only while a driver is alive
};

CurcumaJob::CurcumaJob(LogHub* hub, QObject* parent)
    : QObject(parent)
    , m_hub(hub)
{
}

CurcumaJob::~CurcumaJob()
{
    m_queue.clear();
    if (m_runner && m_runner->isRunning()) {
        m_runner->requestStop();
        m_runner->wait(5000);
    }
}

QStringList CurcumaJob::supportedCommands()
{
    return { QStringLiteral("sp"), QStringLiteral("rmsd") };
}

bool CurcumaJob::isRunning() const
{
    return m_runner && m_runner->isRunning();
}

QString CurcumaJob::runningJobId() const
{
    return isRunning() ? m_runner->jobId() : QString();
}

CurcumaJobState CurcumaJob::state() const
{
    QMutexLocker lock(&m_stateMutex);
    return { m_runningId, m_queuedIds };
}

QString CurcumaJob::start(const CurcumaJobRequest& request, QString* error, int* position)
{
    if (!supportedCommands().contains(request.command)) {
        if (error) {
            *error = QStringLiteral("\"%1\" cannot run in process; supported: %2")
                         .arg(request.command, supportedCommands().join(QStringLiteral(", ")));
        }
        return QString();
    }

    const QString jobId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    // Only one job computes at a time -- curcuma_core is OpenMP-parallel and the
    // interactive MD already owns a pool -- but a further request waits its turn
    // instead of being thrown away. It gets its id straight away, so the caller can
    // ask after it before it has run.
    if (isRunning()) {
        m_queue.append({ request, jobId });
        {
            QMutexLocker lock(&m_stateMutex);
            m_queuedIds.append(jobId);
        }
        if (position)
            *position = m_queue.size();
        if (m_hub) {
            m_hub->append(QStringLiteral("curcuma"), LogLevel::Info,
                QStringLiteral("queued %1 behind %2").arg(request.command, m_runner->jobId()),
                jobId);
        }
        if (error)
            error->clear();
        return jobId;
    }

    if (position)
        *position = 0;
    launch(request, jobId);
    if (error)
        error->clear();
    return jobId;
}

void CurcumaJob::launch(const CurcumaJobRequest& request, const QString& jobId)
{
    m_runner = new Runner(this, m_hub, request, jobId);
    // One connection, not two: the runner is dropped and the next one taken from
    // the queue in the same place, so there is no window in which m_runner points
    // at a thread that has already stopped.
    connect(m_runner, &QThread::finished, this, &CurcumaJob::runnerDone);
    {
        QMutexLocker lock(&m_stateMutex);
        m_runningId = jobId;
        m_queuedIds.removeAll(jobId);
    }
    m_runner->start();

    if (m_hub) {
        m_hub->append(QStringLiteral("curcuma"), LogLevel::Info,
            QStringLiteral("started %1").arg(request.command), jobId);
    }
    emit started(jobId);
}

void CurcumaJob::runnerDone()
{
    if (m_runner) {
        m_runner->deleteLater();
        m_runner = nullptr;
    }
    {
        QMutexLocker lock(&m_stateMutex);
        m_runningId.clear();
    }
    startNext();
}

void CurcumaJob::startNext()
{
    if (m_runner || m_queue.isEmpty())
        return;
    const Pending next = m_queue.takeFirst();
    launch(next.request, next.jobId);
}

void CurcumaJob::requestStop()
{
    // Everything still queued was asked for under the assumption that the first job
    // was wanted; dropping the lot is the honest reading of "stop".
    const QList<Pending> dropped = m_queue;
    m_queue.clear();
    {
        QMutexLocker lock(&m_stateMutex);
        m_queuedIds.clear();
    }
    for (const Pending& pending : dropped) {
        CurcumaJobResult result;
        result.jobId = pending.jobId;
        result.command = pending.request.command;
        result.error = QStringLiteral("dropped from the queue when the running job was stopped");
        emit finished(result);
    }
    if (m_runner)
        m_runner->requestStop();
}
