// tooldispatcher.h - Runs a registered tool on the right thread, and records it.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - ToolRegistry::invoke() runs a handler on whatever thread
// asks. That is fine for a test and wrong for the application: the agent loop does
// not live on the GUI thread, and a handler reaching MoleculeViewer from anywhere
// else is a data race on the viewer's frame storage -- silent, not a crash.
//
// The dispatcher is the one place that knows about threads and about the audit
// trail. It must LIVE on the GUI thread; ToolAffinity::Gui handlers are posted to
// it and run there.
#pragma once

#include "tool.h"
#include "toolregistry.h"

#include <QObject>
#include <QString>

class LogHub;

class ToolDispatcher : public QObject {
    Q_OBJECT

public:
    /// Default wait for a marshalled call before giving up.
    static constexpr int kDefaultTimeoutMs = 15000;

    /// @p registry and @p hub are borrowed, not owned. Create this on the GUI
    /// thread -- that is the thread Gui-affinity handlers will run on.
    explicit ToolDispatcher(ToolRegistry* registry, LogHub* hub, QObject* parent = nullptr);

    /// How long dispatch() waits for a marshalled handler. 0 or less disables the
    /// limit (the call then waits indefinitely, as BlockingQueuedConnection would).
    void setTimeoutMs(int ms);
    int timeoutMs() const { return m_timeoutMs; }

    /// Validate, run on the appropriate thread, and record the call.
    ///
    /// A ToolAffinity::Gui handler called from another thread is posted to this
    /// object's thread and waited for. A timeout comes back as a failed ToolResult
    /// rather than a hang -- the handler may still complete afterwards, its result
    /// is simply dropped.
    ToolResult dispatch(const QString& name, const QJsonObject& args);

    /// How many calls this dispatcher has run (audit/diagnostics).
    quint64 callCount() const { return m_callCount; }

private:
    void record(const ToolSpec& spec, const QJsonObject& args, const ToolResult& result,
                qint64 elapsedMs, bool marshalled);

    ToolRegistry* m_registry = nullptr;
    LogHub* m_hub = nullptr;
    int m_timeoutMs = kDefaultTimeoutMs;
    quint64 m_callCount = 0;
};
