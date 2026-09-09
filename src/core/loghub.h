// loghub.h - Structured, in-process log buffer.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - qurcuma had no log abstraction: qDebug/qWarning went to
// the terminal and were lost to the GUI, and curcuma's in-process output was
// discarded outright (every controller is built with verbosity 0 because there was
// nowhere to put the text -- which is why SimpleMD::stopReason() had to be
// retrofitted just to surface why a run ended).
//
// LogHub is the one place records land: the qInstallMessageHandler bridge, the
// QProcess pipes of external programs, and later curcuma's logger sink. The Output
// dock is a view onto it, and a tool layer can query it with filters and a hard
// cap instead of shipping a whole MD log into a model's context.
//
// QtCore only, so it links into a widget-free core and is testable headless.
#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3
};

/// One log line. @a seq is assigned by the hub, increases monotonically and keeps
/// increasing across ring wraparound, so a reader can ask for "everything after
/// what I last saw" without holding on to indices that shift.
struct LogRecord {
    quint64 seq = 0;
    QDateTime timestamp;
    QString source;   ///< "qurcuma" | "curcuma" | "process" | "tool" | ...
    LogLevel level = LogLevel::Info;
    QString text;
    QString jobId;    ///< empty unless the record belongs to a specific run
};
Q_DECLARE_METATYPE(LogRecord)

/// Filter for LogHub::query(). Every field is optional; the defaults match
/// everything and return the newest kDefaultLimit records.
struct LogQuery {
    QString source;                        ///< empty = any source
    LogLevel minLevel = LogLevel::Debug;   ///< records below this are skipped
    quint64 sinceSeq = 0;                  ///< only records with seq > sinceSeq
    QString contains;                      ///< case-insensitive substring; empty = any
    int limit = 200;                       ///< clamped to LogHub::kMaxQueryLimit
};

/// Fixed-capacity ring of log records, safe to append to from any thread.
class LogHub : public QObject {
    Q_OBJECT

public:
    static constexpr int kDefaultCapacity = 5000;
    /// Upper bound on what a single query can return, whatever LogQuery::limit says.
    /// A caller that wants more has to page with sinceSeq.
    static constexpr int kMaxQueryLimit = 1000;

    explicit LogHub(int capacity = kDefaultCapacity, QObject* parent = nullptr);

    /// Process-wide hub. First touched from main() on the GUI thread, so that is
    /// the instance's thread affinity and queued connections to widgets behave.
    static LogHub& instance();

    /// Append one record and emit appended(). Thread-safe; the signal is emitted
    /// after the internal lock is released, so a slot may call back into the hub.
    void append(const QString& source, LogLevel level, const QString& text,
                const QString& jobId = QString());

    /// Matching records, oldest first, at most min(q.limit, kMaxQueryLimit) of them.
    /// When more match than fit, the *newest* ones are returned -- a log reader
    /// wants the tail, not the head.
    QVector<LogRecord> query(const LogQuery& q) const;

    int capacity() const { return m_capacity; }
    int size() const;
    /// Sequence number of the most recent record (0 when empty).
    quint64 lastSeq() const;
    /// How many records the ring has evicted since construction.
    quint64 droppedCount() const;
    void clear();

    /// Human-readable one-liner for the Output dock.
    static QString formatForView(const LogRecord& record);
    static QString levelName(LogLevel level);

signals:
    void appended(const LogRecord& record);

private:
    mutable QMutex m_mutex;
    QVector<LogRecord> m_ring;  ///< grows to m_capacity, then overwrites at m_head
    int m_capacity;
    int m_head = 0;             ///< index of the oldest record once the ring is full
    quint64 m_nextSeq = 1;
    quint64 m_dropped = 0;
};

/// Route qDebug()/qWarning()/qCritical()/qFatal() into LogHub::instance() under the
/// source "qurcuma", then hand the message to whichever handler was installed
/// before. Terminal output is therefore unchanged and qFatal still aborts -- the
/// hub only gets a copy. Idempotent; safe to call from any thread, though main()
/// is where it belongs so the hub's thread affinity is the GUI thread.
void installLogHubMessageHandler();
