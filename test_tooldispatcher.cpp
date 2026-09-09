// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 — ToolDispatcher: that a Gui-affinity handler really runs
// on the dispatcher's thread, that calling from that thread does not deadlock,
// that a stuck handler times out instead of hanging, and that every call is
// recorded.

#include "core/loghub.h"
#include "core/tooldispatcher.h"
#include "core/toolregistry.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QThread>

#include <atomic>
#include <cstdio>
#include <thread>

static std::atomic<int> g_failed { 0 };

static void check(bool ok, const QString& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
    std::fflush(stdout);
    if (!ok)
        ++g_failed;
}

static QJsonObject emptySchema()
{
    return QJsonDocument::fromJson(R"({"type":"object"})").object();
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    LogHub hub(1000);
    ToolRegistry registry;
    ToolDispatcher dispatcher(&registry, &hub);  // lives on this, the "GUI", thread

    QThread* const guiThread = QThread::currentThread();
    std::atomic<QThread*> anyRanOn { nullptr };
    std::atomic<QThread*> guiRanOn { nullptr };
    std::atomic<int> slowCalls { 0 };

    {
        ToolSpec spec;
        spec.name = QStringLiteral("any_tool");
        spec.paramSchema = emptySchema();
        spec.affinity = ToolAffinity::Any;
        spec.effect = ToolEffect::Read;
        spec.handler = [&anyRanOn](const QJsonObject&) {
            anyRanOn.store(QThread::currentThread());
            return ToolResult::success();
        };
        check(registry.add(spec), "an Any-affinity tool registers");
    }
    {
        ToolSpec spec;
        spec.name = QStringLiteral("gui_tool");
        spec.paramSchema = emptySchema();
        spec.affinity = ToolAffinity::Gui;
        spec.effect = ToolEffect::Display;
        spec.handler = [&guiRanOn](const QJsonObject&) {
            guiRanOn.store(QThread::currentThread());
            return ToolResult::success(QJsonObject { { "done", true } });
        };
        check(registry.add(spec), "a Gui-affinity tool registers");
    }
    {
        ToolSpec spec;
        spec.name = QStringLiteral("slow_gui_tool");
        spec.paramSchema = emptySchema();
        spec.affinity = ToolAffinity::Gui;
        spec.effect = ToolEffect::Compute;
        spec.handler = [&slowCalls](const QJsonObject&) {
            ++slowCalls;
            QThread::msleep(400);
            return ToolResult::success();
        };
        check(registry.add(spec), "a deliberately slow Gui tool registers");
    }

    // --- from the dispatcher's own thread: direct, and no self-deadlock ------
    {
        const ToolResult r = dispatcher.dispatch(QStringLiteral("gui_tool"), QJsonObject {});
        check(r.ok, "a Gui tool dispatched from the dispatcher's own thread runs");
        check(guiRanOn.load() == guiThread, "and runs right there, not via the queue");
    }
    guiRanOn.store(nullptr);

    // --- everything else happens on a worker thread --------------------------
    std::thread worker([&] {
        QThread* const workerThread = QThread::currentThread();
        check(workerThread != guiThread, "the worker really is a different thread");

        const ToolResult anyResult = dispatcher.dispatch(QStringLiteral("any_tool"), QJsonObject {});
        check(anyResult.ok, "an Any tool dispatched from the worker succeeds");
        check(anyRanOn.load() == workerThread,
            "and stays on the calling thread -- no needless marshalling");

        const ToolResult guiResult = dispatcher.dispatch(QStringLiteral("gui_tool"), QJsonObject {});
        check(guiResult.ok, "a Gui tool dispatched from the worker succeeds");
        check(guiRanOn.load() == guiThread,
            "and was marshalled onto the dispatcher's thread");
        check(guiResult.data.value(QStringLiteral("done")).toBool(),
            "its result travels back to the caller");

        // --- unknown tool and bad arguments -------------------------------
        const ToolResult unknown = dispatcher.dispatch(QStringLiteral("nope"), QJsonObject {});
        check(!unknown.ok && unknown.error.contains(QStringLiteral("unknown tool")),
            "an unknown tool fails cleanly from the worker too");

        // --- the timeout ----------------------------------------------------
        dispatcher.setTimeoutMs(80);
        const ToolResult slow = dispatcher.dispatch(QStringLiteral("slow_gui_tool"), QJsonObject {});
        check(!slow.ok && slow.error.contains(QStringLiteral("did not finish")),
            "a handler that overruns the limit fails instead of hanging");
        check(slowCalls.load() == 1, "the handler was still started, it just was not waited for");

        QMetaObject::invokeMethod(qApp, [] { QCoreApplication::quit(); }, Qt::QueuedConnection);
    });

    app.exec();
    worker.join();

    // --- the audit trail ----------------------------------------------------
    {
        LogQuery q;
        q.source = QStringLiteral("tool");
        q.limit = LogHub::kMaxQueryLimit;
        const QVector<LogRecord> audit = hub.query(q);
        check(audit.size() == static_cast<int>(dispatcher.callCount()),
            QStringLiteral("every dispatch left exactly one audit record (%1)").arg(audit.size()));

        bool sawEffect = false;
        bool sawMarshalled = false;
        bool sawFailure = false;
        for (const LogRecord& r : audit) {
            if (r.text.contains(QStringLiteral("gui_tool (display)")))
                sawEffect = true;
            if (r.text.contains(QStringLiteral("[gui]")))
                sawMarshalled = true;
            if (r.level == LogLevel::Warning)
                sawFailure = true;
        }
        check(sawEffect, "the audit line names the tool and its effect");
        check(sawMarshalled, "and marks the calls that were marshalled");
        check(sawFailure, "a failed call is recorded at Warning, not silently");
    }

    std::printf("%s (%d failed)\n", g_failed.load() ? "FAIL" : "PASS", g_failed.load());
    return g_failed.load() ? 1 : 0;
}
