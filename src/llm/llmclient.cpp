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

    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("messages"), messages);
    if (!tools.isEmpty()) {
        body.insert(QStringLiteral("tools"), tools);
        body.insert(QStringLiteral("tool_choice"), QStringLiteral("auto"));
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
