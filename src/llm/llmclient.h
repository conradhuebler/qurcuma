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
#include <QMap>
#include <QObject>
#include <QStringList>
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

    /// Override the profile's model for this session. Empty falls back to the
    /// profile's own, and if that is empty too a send() fails with a clear reason.
    void setModel(const QString& model);
    QString effectiveModel() const;

    /// Ask the endpoint which models it serves (GET /v1/models). One request.
    void listModels();

    /// Ask about one model: context length and capabilities. Ollama answers this
    /// on its native /api/show; other endpoints do not, and then the reply carries
    /// detailsKnown = false rather than a guess. One request, on demand -- asking
    /// for all of them on every profile switch would be two dozen.
    void describeModel(const QString& id);

    /// Abort the request in flight. failed() is not emitted for a cancellation.
    void cancel();
    bool isBusy() const { return m_reply != nullptr; }

signals:
    /// The assistant message: either "content" with text, or "tool_calls" with
    /// what the model wants run, or both.
    void finished(const QJsonObject& assistantMessage);
    /// Transport, HTTP or protocol failure, in words a user can act on.
    void failed(const QString& error);
    /// Answer to listModels(); empty when the endpoint offers no listing.
    void modelsListed(const QStringList& models);
    /// Answer to describeModel().
    void modelDescribed(const LlmModelInfo& info);

    /// A piece of the answer, while streaming. The complete message still arrives
    /// through finished(), so a consumer may ignore these entirely.
    void contentChunk(const QString& text);
    /// A piece of the model's reasoning, while streaming. Deliberately kept apart
    /// from the answer: it is worth showing, and worth NOT sending back in the
    /// next request.
    void reasoningChunk(const QString& text);

private:
    void handleReply();
    void handleStreamData();
    void consumeStreamLine(const QByteArray& line);
    /// Build the assistant message from what the stream delivered.
    QJsonObject assembleStreamedMessage() const;
    void finishStream();
    void handleModelList();
    void handleModelDetails(const QString& id);
    /// Base URL with the trailing "/v1" removed, for Ollama's native endpoints.
    QString nativeBase() const;

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
    QNetworkReply* m_listReply = nullptr;
    QNetworkReply* m_detailReply = nullptr;
    LlmProfile m_profile;
    QString m_model;   ///< session override; empty = use the profile's
    QString m_apiKey;

    // Streaming state, reset per request.
    bool m_streaming = false;
    QByteArray m_streamBuffer;      ///< bytes not yet forming a whole line
    QString m_streamedContent;
    QString m_streamedReasoning;
    /// Tool calls accumulated by their delta index. OpenAI splits `arguments`
    /// across chunks and only the index ties the pieces together; Ollama sends the
    /// whole call at once. Both end up here.
    QMap<int, QJsonObject> m_streamedToolCalls;
    bool m_streamFinished = false;
};
