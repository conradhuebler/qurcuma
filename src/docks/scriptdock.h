// scriptdock.h - Where a script is written and run.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The human half of the interpreter. The assistant gets one
// tool (`calculate`) that computes and nothing else; here the same interpreter runs
// with a tool bridge, because the operator pressing Run is the permission. Every call
// the script makes still goes through the tool dispatcher and the same approval
// policy the assistant is graded by, so the dock is not a softer path.
//
// Not behind USE_LLM: a script is a calculation, not a way to reach an endpoint.
#pragma once

#include <QDockWidget>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QPlainTextEdit;
class QPushButton;
class QThread;
class QTimer;
class ScriptInterpreter;
class ToolDispatcher;
class ToolRegistry;
class ToolSpec;

class ScriptDock : public QDockWidget {
    Q_OBJECT

public:
    explicit ScriptDock(QWidget* parent = nullptr);
    ~ScriptDock() override;

    /// The two things a script's tool calls need, borrowed and never owned. Set after
    /// construction, because the dispatcher is created after the docks are: until it is
    /// set, a script computes and cannot call anything.
    void setToolLayer(ToolRegistry* registry, ToolDispatcher* dispatcher);

    /// Who decides whether a call may run. The assistant's calls are decided by the
    /// same policy; this one is asked from the script's thread, so it has to marshal
    /// to the GUI thread on its own.
    void setApprovalPolicy(std::function<bool(const ToolSpec&, const QJsonObject&)> policy);

    QString source() const;
    void setSource(const QString& source);
    bool isRunning() const { return m_running; }

signals:
    /// One line per run for the audit trail: what ran and how it ended.
    void scriptRan(const QString& source, bool ok, const QString& summary);

private slots:
    void run();
    void stop();

private:
    void appendLine(const QString& line);
    void loadEditor();
    void saveEditorSoon();
    void saveEditor();
    /// Ask before a script starts when it names a tool that does more than read or
    /// display. Returns false when the operator cancels.
    bool confirmToolCalls(const QString& source);
    void insertExample(int index);

    ToolRegistry* m_registry = nullptr;
    ToolDispatcher* m_dispatcher = nullptr;
    QPlainTextEdit* m_editor = nullptr;
    QPlainTextEdit* m_output = nullptr;
    QPushButton* m_runButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QTimer* m_saveTimer = nullptr;

    /// The run in progress. Held by shared_ptr because the Stop button reaches it from
    /// the GUI thread while the script's own thread is inside it.
    std::shared_ptr<ScriptInterpreter> m_interpreter;
    QThread* m_thread = nullptr;
    std::function<bool(const ToolSpec&, const QJsonObject&)> m_approval;
    bool m_running = false;
};
