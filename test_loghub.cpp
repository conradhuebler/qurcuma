// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — LogHub: ring wraparound, the query filters, the hard
// cap, and concurrent appends (qDebug reaches the hub from the worker thread).

#include "core/loghub.h"

#include <QCoreApplication>
#include <cstdio>
#include <set>
#include <thread>
#include <vector>

static int g_failed = 0;

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    if (!ok)
        ++g_failed;
}

static void equal(long long got, long long want, const QString& what)
{
    const bool ok = (got == want);
    std::printf("  %s  %s (got %lld, want %lld)\n", ok ? "PASS" : "FAIL",
        qPrintable(what), got, want);
    if (!ok)
        ++g_failed;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // --- append and read back ----------------------------------------------
    {
        LogHub hub(10);
        hub.append("qurcuma", LogLevel::Info, "first");
        hub.append("qurcuma", LogLevel::Warning, "second");
        const QVector<LogRecord> all = hub.query({});
        equal(all.size(), 2, "two appends give two records");
        check(all.size() == 2 && all[0].text == "first" && all[1].text == "second",
            "records come back oldest first");
        check(all.size() == 2 && all[0].seq == 1 && all[1].seq == 2,
            "seq starts at 1 and increments");
        equal(hub.lastSeq(), 2, "lastSeq follows the newest record");
        equal(hub.droppedCount(), 0, "nothing dropped below capacity");
    }

    // --- ring wraparound ----------------------------------------------------
    {
        LogHub hub(3);
        for (int i = 1; i <= 5; ++i)
            hub.append("qurcuma", LogLevel::Info, QStringLiteral("m%1").arg(i));
        equal(hub.size(), 3, "size is capped at the capacity");
        equal(hub.droppedCount(), 2, "the two oldest were evicted");
        const QVector<LogRecord> all = hub.query({});
        equal(all.size(), 3, "query returns what the ring still holds");
        check(all.size() == 3 && all[0].text == "m3" && all[2].text == "m5",
            "the survivors are the newest, still oldest-first");
        check(all.size() == 3 && all[0].seq == 3 && all[2].seq == 5,
            "seq keeps counting across the wraparound");
    }

    // --- sinceSeq lets a reader page without re-reading ---------------------
    {
        LogHub hub(10);
        for (int i = 1; i <= 5; ++i)
            hub.append("qurcuma", LogLevel::Info, QStringLiteral("m%1").arg(i));
        LogQuery q;
        q.sinceSeq = 3;
        const QVector<LogRecord> tail = hub.query(q);
        equal(tail.size(), 2, "sinceSeq=3 yields the two newer records");
        check(tail.size() == 2 && tail[0].text == "m4", "and starts right after it");

        q.sinceSeq = hub.lastSeq();
        equal(hub.query(q).size(), 0, "sinceSeq at the head yields nothing");
    }

    // --- filters ------------------------------------------------------------
    {
        LogHub hub(20);
        hub.append("qurcuma", LogLevel::Debug, "a debug line");
        hub.append("curcuma", LogLevel::Info, "an info line");
        hub.append("curcuma", LogLevel::Error, "a BROKEN line");
        hub.append("process", LogLevel::Warning, "a warning line");

        LogQuery bySource;
        bySource.source = "curcuma";
        equal(hub.query(bySource).size(), 2, "source filter");

        LogQuery byLevel;
        byLevel.minLevel = LogLevel::Warning;
        equal(hub.query(byLevel).size(), 2, "minLevel keeps Warning and Error");

        LogQuery byText;
        byText.contains = "broken";
        equal(hub.query(byText).size(), 1, "contains is case-insensitive");

        LogQuery combined;
        combined.source = "curcuma";
        combined.minLevel = LogLevel::Error;
        equal(hub.query(combined).size(), 1, "filters combine");
    }

    // --- the hard cap, and which end it keeps -------------------------------
    {
        LogHub hub(50);
        for (int i = 1; i <= 40; ++i)
            hub.append("qurcuma", LogLevel::Info, QStringLiteral("m%1").arg(i));
        LogQuery q;
        q.limit = 5;
        const QVector<LogRecord> tail = hub.query(q);
        equal(tail.size(), 5, "limit is honoured");
        check(tail.size() == 5 && tail[0].text == "m36" && tail[4].text == "m40",
            "an over-full match returns the NEWEST records, not the oldest");

        q.limit = 100000;
        check(hub.query(q).size() <= LogHub::kMaxQueryLimit,
            "limit is clamped to kMaxQueryLimit");

        q.limit = 0;
        equal(hub.query(q).size(), 0, "limit 0 returns nothing");
    }

    // --- clear keeps the sequence counter running ---------------------------
    {
        LogHub hub(10);
        hub.append("qurcuma", LogLevel::Info, "before");
        const quint64 seqBefore = hub.lastSeq();
        hub.clear();
        equal(hub.size(), 0, "clear empties the ring");
        hub.append("qurcuma", LogLevel::Info, "after");
        check(hub.lastSeq() > seqBefore,
            "seq keeps counting past a clear (a stale sinceSeq must not replay)");
    }

    // --- concurrent appends -------------------------------------------------
    {
        const int threads = 4;
        const int perThread = 500;
        LogHub hub(threads * perThread);
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t) {
            pool.emplace_back([&hub, t, perThread] {
                for (int i = 0; i < perThread; ++i)
                    hub.append(QStringLiteral("t%1").arg(t), LogLevel::Info,
                        QStringLiteral("line %1").arg(i));
            });
        }
        for (auto& th : pool)
            th.join();

        equal(hub.size(), threads * perThread, "every concurrent append landed");
        equal(hub.lastSeq(), threads * perThread, "seq counted them all exactly once");

        LogQuery q;
        q.limit = LogHub::kMaxQueryLimit;
        const QVector<LogRecord> sample = hub.query(q);
        std::set<quint64> seen;
        for (const LogRecord& r : sample)
            seen.insert(r.seq);
        equal(static_cast<long long>(seen.size()), sample.size(),
            "no two records share a sequence number");
    }

    // --- the qInstallMessageHandler bridge ----------------------------------
    // The pass-through to Qt's own handler is deliberately NOT asserted here: on a
    // systemd system that handler goes to journald whenever stderr is not a
    // console, so there is nothing portable to capture. Checked by hand with
    // QT_LOGGING_TO_CONSOLE=1, where both lines below appear on stderr as well.
    {
        installLogHubMessageHandler();
        LogHub& hub = LogHub::instance();
        const quint64 before = hub.lastSeq();

        qWarning("loghub bridge check");

        LogQuery q;
        q.sinceSeq = before;
        const QVector<LogRecord> fresh = hub.query(q);
        check(!fresh.isEmpty(), "qWarning reaches the hub");
        check(!fresh.isEmpty() && fresh.last().text.contains("loghub bridge check"),
            "the message text arrives intact");
        check(!fresh.isEmpty() && fresh.last().level == LogLevel::Warning,
            "QtWarningMsg maps to LogLevel::Warning");
        check(!fresh.isEmpty() && fresh.last().source == QStringLiteral("qurcuma"),
            "and lands under the qurcuma source");

        const quint64 afterFirst = hub.lastSeq();
        installLogHubMessageHandler();  // second call must not chain onto itself
        qWarning("loghub bridge check twice");
        equal(hub.lastSeq() - afterFirst, 1,
            "installing twice records the next message once, not twice");
    }

    std::printf("%s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
