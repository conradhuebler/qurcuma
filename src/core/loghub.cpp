// loghub.cpp - Structured, in-process log buffer.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "loghub.h"

#include <QMutexLocker>

#include <algorithm>  // std::reverse
#include <cstdio>

LogHub::LogHub(int capacity, QObject* parent)
    : QObject(parent)
    , m_capacity(capacity > 0 ? capacity : kDefaultCapacity)
{
    // Records cross threads: qDebug() is called from the SimulationWorker thread,
    // so appended() is delivered as a queued connection and needs the metatype.
    static const int typeId = qRegisterMetaType<LogRecord>("LogRecord");
    Q_UNUSED(typeId)
    m_ring.reserve(m_capacity);
}

LogHub& LogHub::instance()
{
    static LogHub hub;
    return hub;
}

void LogHub::append(const QString& source, LogLevel level, const QString& text,
                    const QString& jobId)
{
    LogRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.source = source;
    record.level = level;
    record.text = text;
    record.jobId = jobId;

    {
        QMutexLocker lock(&m_mutex);
        record.seq = m_nextSeq++;
        if (m_ring.size() < m_capacity) {
            m_ring.append(record);
        } else {
            m_ring[m_head] = record;
            m_head = (m_head + 1) % m_capacity;
            ++m_dropped;
        }
    }

    // Emitted outside the lock: a subscriber is free to call back into the hub.
    emit appended(record);
}

QVector<LogRecord> LogHub::query(const LogQuery& q) const
{
    const int limit = qBound(0, q.limit, kMaxQueryLimit);
    QVector<LogRecord> out;
    if (limit == 0)
        return out;

    QMutexLocker lock(&m_mutex);
    const int n = m_ring.size();
    if (n == 0)
        return out;

    // Walk newest to oldest and stop once the limit is reached, so a large ring
    // with a small limit costs only as much as it must; reverse at the end to
    // hand back oldest-first.
    for (int i = n - 1; i >= 0 && out.size() < limit; --i) {
        const LogRecord& r = m_ring.at((m_head + i) % n);
        if (r.seq <= q.sinceSeq)
            break;  // older records can only have smaller seq
        if (static_cast<int>(r.level) < static_cast<int>(q.minLevel))
            continue;
        if (!q.source.isEmpty() && r.source != q.source)
            continue;
        if (!q.contains.isEmpty() && !r.text.contains(q.contains, Qt::CaseInsensitive))
            continue;
        out.append(r);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

int LogHub::size() const
{
    QMutexLocker lock(&m_mutex);
    return m_ring.size();
}

quint64 LogHub::lastSeq() const
{
    QMutexLocker lock(&m_mutex);
    return m_nextSeq - 1;
}

quint64 LogHub::droppedCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_dropped;
}

void LogHub::clear()
{
    QMutexLocker lock(&m_mutex);
    m_ring.clear();
    m_ring.reserve(m_capacity);
    m_head = 0;
    m_dropped = 0;
    // m_nextSeq deliberately keeps counting: a reader holding a sinceSeq from
    // before the clear must not be handed records it has already seen.
}

QString LogHub::levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:   return QStringLiteral("Debug");
    case LogLevel::Info:    return QStringLiteral("Info");
    case LogLevel::Warning: return QStringLiteral("Warning");
    case LogLevel::Error:   return QStringLiteral("Error");
    }
    return QStringLiteral("Info");
}

QString LogHub::formatForView(const LogRecord& record)
{
    const QString time = record.timestamp.toString(QStringLiteral("hh:mm:ss"));
    const QString job = record.jobId.isEmpty() ? QString()
                                               : QStringLiteral(" (%1)").arg(record.jobId);
    // Only the levels a reader has to act on are named; Debug and Info would just
    // add noise to every single line.
    const QString level = (record.level == LogLevel::Warning || record.level == LogLevel::Error)
        ? QStringLiteral("%1: ").arg(levelName(record.level))
        : QString();
    return QStringLiteral("[%1] [%2]%3 %4%5")
        .arg(time, record.source, job, level, record.text);
}

// ---------------------------------------------------------------------------
// qInstallMessageHandler bridge
// ---------------------------------------------------------------------------
namespace {

QtMessageHandler g_previousHandler = nullptr;
bool g_installed = false;

LogLevel levelFor(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return LogLevel::Debug;
    case QtInfoMsg:     return LogLevel::Info;
    case QtWarningMsg:  return LogLevel::Warning;
    case QtCriticalMsg: return LogLevel::Error;
    case QtFatalMsg:    return LogLevel::Error;
    }
    return LogLevel::Info;
}

void logHubMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    // A subscriber that logs while handling appended() would recurse forever.
    // Nothing does today; the guard makes sure a future one cannot blow the stack.
    static thread_local bool inHandler = false;
    if (!inHandler) {
        inHandler = true;
        LogHub::instance().append(QStringLiteral("qurcuma"), levelFor(type), message);
        inHandler = false;
    }

    // Always pass through, so the terminal keeps showing what it showed before.
    //
    // Note when checking this by eye: on a systemd system Qt's default handler
    // sends to journald as soon as stderr is not a console, so redirecting stderr
    // to a file looks like the messages vanished. They have not -- run with
    // QT_LOGGING_TO_CONSOLE=1 to see them on stderr. Measured here on 2026-09-09.
    //
    // The nullptr branch is defensive: qInstallMessageHandler returns the handler
    // it replaced, which is non-null here (Qt hands back its own default), but the
    // API permits null and then nothing at all would reach the terminal.
    if (g_previousHandler) {
        g_previousHandler(type, context, message);
    } else {
        const QString formatted = qFormatLogMessage(type, context, message);
        std::fputs(qPrintable(formatted), stderr);
        std::fputc('\n', stderr);
        std::fflush(stderr);
    }
}

}  // namespace

void installLogHubMessageHandler()
{
    if (g_installed)
        return;
    g_installed = true;
    g_previousHandler = qInstallMessageHandler(logHubMessageHandler);
}
