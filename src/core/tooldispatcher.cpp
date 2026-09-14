// tooldispatcher.cpp - Runs a registered tool on the right thread, and records it.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "tooldispatcher.h"

#include "loghub.h"

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSemaphore>
#include <QThread>

#include <memory>

namespace {

/// Who is making the calls on this thread. Thread-local on purpose: the assistant's
/// agent loop and the GUI thread dispatch independently, and neither may overwrite the
/// other's answer. Claude Generated 2026.
thread_local QString t_origin;

/// Shared state of one marshalled call.
///
/// Held by shared_ptr because the waiting side may give up while the posted lambda
/// is still queued: the lambda then writes into state that must still exist. The
/// semaphore also orders the write before the read, so no extra lock is needed --
/// the waiter only ever touches @a result after acquiring.
struct MarshalledCall {
    ToolResult result;
    QSemaphore done { 0 };
};

/// Arguments for the audit line, short enough not to bury the log.
QString shortArgs(const QJsonObject& args)
{
    if (args.isEmpty())
        return QStringLiteral("{}");
    const QByteArray json = QJsonDocument(args).toJson(QJsonDocument::Compact);
    constexpr int kMaxChars = 300;
    if (json.size() <= kMaxChars)
        return QString::fromUtf8(json);
    return QString::fromUtf8(json.left(kMaxChars)) + QStringLiteral("… (%1 bytes)").arg(json.size());
}

}  // namespace

ToolDispatcher::ToolDispatcher(ToolRegistry* registry, LogHub* hub, QObject* parent)
    : QObject(parent)
    , m_registry(registry)
    , m_hub(hub)
{
}

void ToolDispatcher::setTimeoutMs(int ms)
{
    m_timeoutMs = ms;
}

ToolResult ToolDispatcher::dispatch(const QString& name, const QJsonObject& args)
{
    if (!m_registry)
        return ToolResult::failure(QStringLiteral("no tool registry"));

    ToolSpec spec;
    if (!m_registry->tool(name, spec)) {
        const ToolResult result = ToolResult::failure(QStringLiteral("unknown tool \"%1\"").arg(name));
        if (m_hub) {
            m_hub->append(QStringLiteral("tool"), LogLevel::Warning,
                QStringLiteral("%1 -> %2").arg(name, result.error));
        }
        ++m_callCount;
        return result;
    }

    const ToolValidation validation = ToolRegistry::validateAgainst(spec.paramSchema, args);
    if (!validation.ok) {
        const ToolResult result = ToolResult::failure(validation.error);
        record(spec, args, result, 0, false);
        return result;
    }

    QElapsedTimer timer;
    timer.start();

    // Direct when the handler does not care, and also when we are already on the
    // target thread: posting to our own thread and then waiting for it would block
    // the very event loop that has to run the lambda.
    const bool needsMarshalling =
        spec.affinity == ToolAffinity::Gui && QThread::currentThread() != thread();

    ToolResult result;
    if (!needsMarshalling) {
        result = spec.handler(args);
    } else {
        auto call = std::make_shared<MarshalledCall>();
        const auto handler = spec.handler;
        QMetaObject::invokeMethod(this, [call, handler, args]() {
            call->result = handler(args);
            call->done.release();
        }, Qt::QueuedConnection);

        const bool finished = (m_timeoutMs > 0)
            ? call->done.tryAcquire(1, m_timeoutMs)
            : (call->done.acquire(1), true);

        if (finished) {
            result = call->result;
        } else {
            // The lambda may still run and write into `call`; the shared_ptr keeps
            // that legal. Its result is simply never read.
            result = ToolResult::failure(
                QStringLiteral("tool \"%1\" did not finish on the GUI thread within %2 ms")
                    .arg(name)
                    .arg(m_timeoutMs));
        }
    }

    record(spec, args, result, timer.elapsed(), needsMarshalling);
    return result;
}

void ToolDispatcher::record(const ToolSpec& spec, const QJsonObject& args,
                            const ToolResult& result, qint64 elapsedMs, bool marshalled)
{
    ++m_callCount;

    // Consumers that watch what runs. The script dock's recorder is the first: it turns
    // the operator's own tool use into a script that can be replayed. Emitted whether or
    // not there is a log hub, because a recorder does not need one.
    emit callRecorded(spec.name, args, result.ok, elapsedMs, callOrigin());

    if (!m_hub)
        return;

    // Every call is recorded, approved or not, succeeded or not. In a tool where a
    // model can change a structure, "what did it do to my molecule" is a
    // reproducibility question, so this line is the answer to it.
    const QString outcome = result.ok
        ? QStringLiteral("ok")
        : QStringLiteral("error: %1").arg(result.error);
    const QString where = marshalled ? QStringLiteral(" [gui]") : QString();
    const QString origin = callOrigin();
    const QString by = origin.isEmpty() ? QString() : QStringLiteral(" [%1]").arg(origin);

    m_hub->append(QStringLiteral("tool"),
        result.ok ? LogLevel::Info : LogLevel::Warning,
        QStringLiteral("%1 (%2)%3%4 args=%5 -> %6 in %7 ms")
            .arg(spec.name, toolEffectName(spec.effect), where, by, shortArgs(args), outcome)
            .arg(elapsedMs));
}

QString ToolDispatcher::callOrigin()
{
    return t_origin;
}

void ToolDispatcher::setCallOrigin(const QString& origin)
{
    t_origin = origin;
}
