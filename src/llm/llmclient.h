// llmclient.h - OpenAI-compatible chat completions with tool calling.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The transport, and nothing else. It knows how to send a
// conversation plus a tool catalogue and how to read back what the model said or
// which tool it wants called. It does not run tools, does not decide whether it is
// allowed to, and keeps no conversation of its own -- that is LlmSession's job.
//
// One thing worth being clear about: the model never executes anything. It names a
// tool and its arguments; qurcuma runs it and sends the result back as another
// message. There is no discovery either -- the whole catalogue travels with every
// single request, which is why keeping it small matters.
#pragma once

#include "llmconfig.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class LlmClient : public QObject {
    Q_OBJECT

public:
    explicit LlmClient(QObject* parent = nullptr);
    ~LlmClient() override;

    void setProfile(const LlmProfile& profile);
    LlmProfile profile() const { return m_profile; }

    /// Key for the Authorization header. Empty means the header is omitted, which
    /// is what a local endpoint wants.
    void setApiKey(const QString& key);

    /// POST one chat completion. @p messages and @p tools are already in the
    /// OpenAI shape. Exactly one of finished()/failed() follows.
    void send(const QJsonArray& messages, const QJsonArray& tools = {});

    /// Abort the request in flight. failed() is not emitted for a cancellation.
    void cancel();
    bool isBusy() const { return m_reply != nullptr; }

signals:
    /// The assistant message: either "content" with text, or "tool_calls" with
    /// what the model wants run, or both.
    void finished(const QJsonObject& assistantMessage);
    /// Transport, HTTP or protocol failure, in words a user can act on.
    void failed(const QString& error);

private:
    void handleReply();

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
    LlmProfile m_profile;
    QString m_apiKey;
};
