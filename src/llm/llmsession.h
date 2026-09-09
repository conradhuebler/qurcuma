// llmsession.h - The agent loop: ask, run what the model names, answer again.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - This is where the pieces meet. It keeps the conversation,
// builds the tool catalogue from the registry, sends it with every request, and
// when the model names a tool it runs it through the dispatcher and sends the
// result back as another message. The model itself executes nothing.
//
// It also owns the approval policy, because that is a decision about a
// conversation, not about a tool: the same select_atoms is unremarkable in one
// context and worth a question in another. ToolEffect is what it keys off.
#pragma once

#include "core/loghub.h"  // LogLevel, used in the private note() helper
#include "core/tool.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class LlmClient;
class ToolDispatcher;
class ToolRegistry;

class LlmSession : public QObject {
    Q_OBJECT

public:
    /// Everything is borrowed. @p client must outlive the session.
    LlmSession(LlmClient* client, ToolRegistry* registry, ToolDispatcher* dispatcher,
               LogHub* hub, QObject* parent = nullptr);

    /// Asked for every tool whose effect is not Read or Display. Return false to
    /// refuse; the model is told it was refused and can carry on differently.
    using ApprovalFn = std::function<bool(const ToolSpec&, const QJsonObject& args)>;
    /// Without one, anything beyond Read and Display is refused. The safe default
    /// is the one where nobody has to remember to switch it on.
    void setApprovalPolicy(ApprovalFn approve);

    /// Whether the selected model may be sent images. A tool that renders one is
    /// otherwise answered with a note saying so, rather than with a picture nobody
    /// looks at. Claude Generated 2026.
    void setVisionCapable(bool capable) { m_visionCapable = capable; }
    bool visionCapable() const { return m_visionCapable; }

    void setSystemPrompt(const QString& prompt);
    /// Cap on send/run rounds within one turn. Reached, the model is asked once
    /// more without any tools, so the turn ends with an answer built from what it
    /// already gathered instead of with the work thrown away.
    void setMaxIterations(int iterations);
    int maxIterations() const { return m_maxIterations; }

    /// Largest tool result, in characters, that goes into the conversation. Beyond
    /// it the payload is cut and marked, because history is resent every round and
    /// one fat result is then paid for again and again.
    void setMaxToolResultChars(int chars) { m_maxToolResultChars = chars; }

    void ask(const QString& userMessage);
    void cancel();
    /// Forget the conversation. The system prompt stays.
    void reset();

    bool isBusy() const { return m_busy; }
    QJsonArray conversation() const { return m_messages; }

signals:
    void assistantMessage(const QString& text);
    /// Pieces of the answer and of the model's reasoning as they arrive. Only
    /// emitted while streaming; assistantMessage() always follows.
    void assistantChunk(const QString& text);
    void reasoningChunk(const QString& text);
    void toolStarted(const QString& name, const QJsonObject& args);
    void toolFinished(const QString& name, const ToolResult& result);
    void toolRefused(const QString& name, const QString& reason);
    void finished();
    void failed(const QString& error);
    void busyChanged(bool busy);

private:
    void sendRound();
    void onClientFinished(const QJsonObject& assistantMessage);
    void onClientFailed(const QString& error);
    void runToolCalls(const QJsonArray& toolCalls);
    /// Answer every pending tool_call with a refusal, so the history stays valid
    /// when the loop stops before the calls are run.
    void declineRemainingCalls(const QJsonArray& toolCalls, const QString& reason);
    /// Put @p image in front of the model as its own user message, and drop the one
    /// before it: the history is resent every round, so keeping every picture ever
    /// rendered would be paid for again on each of them.
    void showImage(const QByteArray& image, const QString& mimeType, const QString& toolName);
    QJsonArray toolCatalogue() const;
    void setBusy(bool busy);
    void endTurn();
    void note(LogLevel level, const QString& text);

    LlmClient* m_client = nullptr;
    ToolRegistry* m_registry = nullptr;
    ToolDispatcher* m_dispatcher = nullptr;
    LogHub* m_hub = nullptr;

    ApprovalFn m_approve;
    QString m_systemPrompt;
    QJsonArray m_messages;
    // 12 was too few for real work: comparing three methods on a complex and its
    // two fragments is nine calculations before a word is written, and the turn
    // died mid-way with everything discarded. 30 then ran out on a dissociation
    // study that had already done the hard part. Each round is one request with the
    // whole history in it, so this is a real cost -- but a task abandoned two steps
    // from the end costs all of it.
    int m_maxIterations = 50;
    int m_iteration = 0;
    /// Set for the one round that runs without tools, to force a closing answer.
    bool m_finalRound = false;
    bool m_visionCapable = false;
    int m_maxToolResultChars = 8000;
    bool m_busy = false;
};
