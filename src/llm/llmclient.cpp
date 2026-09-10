// llmclient.cpp - OpenAI-compatible chat completions with tool calling.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "llmclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

LlmClient::LlmClient(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

LlmClient::~LlmClient() = default;

void LlmClient::setProfile(const LlmProfile& profile)
{
    m_profile = profile;
}

void LlmClient::setApiKey(const QString& key)
{
    m_apiKey = key;
}

void LlmClient::setModel(const QString& model)
{
    m_model = model;
}

QString LlmClient::effectiveModel() const
{
    return m_model.isEmpty() ? m_profile.model : m_model;
}

QString LlmClient::nativeBase() const
{
    QString base = m_profile.baseUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    if (base.endsWith(QLatin1String("/v1")))
        base.chop(3);
    return base;
}

void LlmClient::listModels()
{
    if (!m_profile.isValid() || m_listReply)
        return;
    QString base = m_profile.baseUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);

    QNetworkRequest request { QUrl(base + QStringLiteral("/models")) };
    if (!m_apiKey.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());
    request.setTransferTimeout(15000);
    m_listReply = m_network->get(request);
    connect(m_listReply, &QNetworkReply::finished, this, &LlmClient::handleModelList);
}

void LlmClient::handleModelList()
{
    QNetworkReply* reply = m_listReply;
    if (!reply)
        return;
    m_listReply = nullptr;
    reply->deleteLater();

    QStringList models;
    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    for (const QJsonValue& value : root.value(QStringLiteral("data")).toArray()) {
        const QString id = value.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty())
            models << id;
    }
    models.sort(Qt::CaseInsensitive);
    emit modelsListed(models);
}

void LlmClient::describeModel(const QString& id)
{
    if (id.isEmpty() || m_detailReply)
        return;

    // Ollama's native endpoint. Anything else answers 404 or HTML, which is not an
    // error worth reporting -- the extra information is a bonus, not a requirement.
    QNetworkRequest request { QUrl(nativeBase() + QStringLiteral("/api/show")) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(15000);

    QJsonObject body;
    body.insert(QStringLiteral("model"), id);
    m_detailReply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_detailReply, &QNetworkReply::finished, this,
        [this, id] { handleModelDetails(id); });
}

void LlmClient::handleModelDetails(const QString& id)
{
    QNetworkReply* reply = m_detailReply;
    if (!reply)
        return;
    m_detailReply = nullptr;
    reply->deleteLater();

    LlmModelInfo info;
    info.id = id;

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray capabilities = root.value(QStringLiteral("capabilities")).toArray();
    for (const QJsonValue& value : capabilities) {
        const QString capability = value.toString();
        if (capability == QLatin1String("tools"))
            info.supportsTools = true;
        else if (capability == QLatin1String("vision"))
            info.supportsVision = true;
    }
    // The key is architecture-prefixed (glm5_next.context_length, llama.context_length,
    // ...), so look for the suffix rather than guessing the architecture.
    const QJsonObject modelInfo = root.value(QStringLiteral("model_info")).toObject();
    for (auto it = modelInfo.begin(); it != modelInfo.end(); ++it) {
        if (it.key().endsWith(QLatin1String(".context_length"))) {
            info.contextLength = it.value().toInt();
            break;
        }
    }
    info.detailsKnown = !capabilities.isEmpty() || info.contextLength > 0;
    emit modelDescribed(info);
}

void LlmClient::setReasoningEffort(const QString& effort)
{
    m_reasoningEffort = effort;
}

void LlmClient::send(const QJsonArray& messages, const QJsonArray& tools)
{
    if (m_reply) {
        emit failed(tr("a request is already in flight"));
        return;
    }
    if (!m_profile.isValid()) {
        emit failed(tr("no endpoint profile is configured (name, base_url and model are needed)"));
        return;
    }

    const QString model = effectiveModel();
    if (model.isEmpty()) {
        emit failed(tr("no model is selected -- pick one in the Assistant dock, or set "
                       "\"model\" in the profile"));
        return;
    }

    m_streaming = m_profile.stream;
    m_streamBuffer.clear();
    m_streamedContent.clear();
    m_streamedReasoning.clear();
    m_streamedToolCalls.clear();
    m_streamFinished = false;

    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("messages"), messages);
    if (m_streaming)
        body.insert(QStringLiteral("stream"), true);
    if (!tools.isEmpty()) {
        body.insert(QStringLiteral("tools"), tools);
        body.insert(QStringLiteral("tool_choice"), QStringLiteral("auto"));
    }
    // How much the model should think, under whatever name this endpoint knows it
    // by. Left out entirely unless both a field name and a level are set: an
    // unknown key is rejected outright by some servers, and "the endpoint's own
    // default" is a real choice rather than a missing one. Claude Generated 2026.
    if (!m_reasoningEffort.isEmpty() && !m_profile.reasoningField.isEmpty()) {
        if (m_profile.reasoningField == QLatin1String("think")) {
            // Ollama's own field is a switch, not a level.
            body.insert(m_profile.reasoningField,
                m_reasoningEffort != QLatin1String("off"));
        } else if (m_reasoningEffort == QLatin1String("off")) {
            body.insert(m_profile.reasoningField, QStringLiteral("none"));
        } else {
            body.insert(m_profile.reasoningField, m_reasoningEffort);
        }
    }

    QNetworkRequest request { QUrl(m_profile.chatCompletionsUrl()) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    // Omitted rather than sent empty: a local endpoint that sees a bare "Bearer"
    // may reject the request outright.
    if (!m_apiKey.isEmpty()) {
        request.setRawHeader("Authorization",
            QByteArray("Bearer ") + m_apiKey.toUtf8());
    }
    request.setTransferTimeout(m_profile.requestTimeoutMs);

    m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (m_streaming)
        connect(m_reply, &QNetworkReply::readyRead, this, &LlmClient::handleStreamData);
    connect(m_reply, &QNetworkReply::finished, this, &LlmClient::handleReply);
}

void LlmClient::cancel()
{
    if (!m_reply)
        return;
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;                 // so handleReply() sees no request in flight
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}

void LlmClient::handleReply()
{
    QNetworkReply* reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    reply->deleteLater();

    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (m_streaming) {
        // Whatever arrived with the final read, plus any line without a trailing
        // newline. An HTTP error body is not SSE and falls through to the normal
        // error handling below.
        if (status < 400 && reply->error() == QNetworkReply::NoError) {
            m_streamBuffer += payload;
            while (true) {
                const int newline = m_streamBuffer.indexOf('\n');
                if (newline < 0)
                    break;
                const QByteArray line = m_streamBuffer.left(newline);
                m_streamBuffer.remove(0, newline + 1);
                consumeStreamLine(line);
            }
            if (!m_streamBuffer.trimmed().isEmpty())
                consumeStreamLine(m_streamBuffer);
            m_streamBuffer.clear();
            finishStream();
            return;
        }
        m_streaming = false;  // fall through and report the error properly
    }

    if (reply->error() != QNetworkReply::NoError && payload.isEmpty()) {
        emit failed(tr("request failed: %1").arg(reply->errorString()));
        return;
    }

    QJsonParseError parseError {};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        // Show a slice of what actually came back: an HTML error page or a proxy
        // notice is far more useful than "invalid JSON".
        emit failed(tr("the endpoint did not answer with JSON (HTTP %1): %2")
                        .arg(status)
                        .arg(QString::fromUtf8(payload.left(200))));
        return;
    }
    const QJsonObject root = doc.object();

    // An OpenAI-compatible error body carries its own message; prefer it over the
    // HTTP status, which rarely says what was actually wrong.
    if (root.contains(QStringLiteral("error"))) {
        const QJsonValue error = root.value(QStringLiteral("error"));
        const QString message = error.isObject()
            ? error.toObject().value(QStringLiteral("message")).toString()
            : error.toVariant().toString();
        emit failed(message.isEmpty()
                ? tr("the endpoint reported an error (HTTP %1)").arg(status)
                : tr("%1 (HTTP %2)").arg(message).arg(status));
        return;
    }
    if (status >= 400) {
        emit failed(tr("HTTP %1: %2").arg(status).arg(QString::fromUtf8(payload.left(200))));
        return;
    }

    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        emit failed(tr("the answer carried no \"choices\""));
        return;
    }
    const QJsonObject message = choices.first().toObject().value(QStringLiteral("message")).toObject();
    if (message.isEmpty()) {
        emit failed(tr("the first choice carried no \"message\""));
        return;
    }
    emit finished(message);
}

// ---------------------------------------------------------------------------
// Streaming  -  Claude Generated 2026
//
// Shapes measured against Ollama (glm-5.3-flash:cloud, 09.09.2026): lines of
// "data: {json}" ending with "data: [DONE]"; choices[0].delta carries role,
// content and -- for a reasoning model -- "reasoning". Ollama delivered a tool
// call complete in one chunk, but OpenAI splits function.arguments across chunks
// with only the index tying them together, so the accumulation handles both.
// ---------------------------------------------------------------------------

void LlmClient::handleStreamData()
{
    if (!m_reply)
        return;
    m_streamBuffer += m_reply->readAll();
    while (true) {
        const int newline = m_streamBuffer.indexOf('\n');
        if (newline < 0)
            break;
        const QByteArray line = m_streamBuffer.left(newline);
        m_streamBuffer.remove(0, newline + 1);
        consumeStreamLine(line);
    }
}

void LlmClient::consumeStreamLine(const QByteArray& rawLine)
{
    const QByteArray line = rawLine.trimmed();
    if (line.isEmpty() || !line.startsWith("data:"))
        return;   // comments and keep-alives

    const QByteArray payload = line.mid(5).trimmed();
    if (payload == "[DONE]") {
        m_streamFinished = true;
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(payload).object();
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty())
        return;
    const QJsonObject delta = choices.first().toObject().value(QStringLiteral("delta")).toObject();

    const QString content = delta.value(QStringLiteral("content")).toString();
    if (!content.isEmpty()) {
        m_streamedContent += content;
        emit contentChunk(content);
    }
    const QString reasoning = delta.value(QStringLiteral("reasoning")).toString();
    if (!reasoning.isEmpty()) {
        m_streamedReasoning += reasoning;
        emit reasoningChunk(reasoning);
    }

    for (const QJsonValue& value : delta.value(QStringLiteral("tool_calls")).toArray()) {
        const QJsonObject piece = value.toObject();
        const int index = piece.value(QStringLiteral("index")).toInt(0);
        QJsonObject call = m_streamedToolCalls.value(index);

        if (piece.contains(QStringLiteral("id")))
            call.insert(QStringLiteral("id"), piece.value(QStringLiteral("id")));
        call.insert(QStringLiteral("type"), QStringLiteral("function"));

        const QJsonObject function = piece.value(QStringLiteral("function")).toObject();
        QJsonObject merged = call.value(QStringLiteral("function")).toObject();
        if (function.contains(QStringLiteral("name")))
            merged.insert(QStringLiteral("name"), function.value(QStringLiteral("name")));
        if (function.contains(QStringLiteral("arguments"))) {
            // Appended, not replaced: this is the piece OpenAI splits.
            merged.insert(QStringLiteral("arguments"),
                merged.value(QStringLiteral("arguments")).toString()
                    + function.value(QStringLiteral("arguments")).toString());
        }
        call.insert(QStringLiteral("function"), merged);
        m_streamedToolCalls.insert(index, call);
    }
}

QJsonObject LlmClient::assembleStreamedMessage() const
{
    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    message.insert(QStringLiteral("content"), m_streamedContent);

    if (!m_streamedToolCalls.isEmpty()) {
        QJsonArray calls;
        for (auto it = m_streamedToolCalls.constBegin(); it != m_streamedToolCalls.constEnd(); ++it)
            calls.append(it.value());   // QMap iterates by key, so index order is kept
        message.insert(QStringLiteral("tool_calls"), calls);
    }

    // The reasoning is deliberately NOT part of the message. It is worth showing
    // while it happens and worth leaving out of the next request: the history is
    // resent every round, and nothing downstream needs the model's scratch work.
    return message;
}

void LlmClient::finishStream()
{
    m_streaming = false;
    if (m_streamedContent.isEmpty() && m_streamedToolCalls.isEmpty() && !m_streamFinished) {
        emit failed(tr("the stream ended without an answer"));
        return;
    }
    emit finished(assembleStreamedMessage());
}
