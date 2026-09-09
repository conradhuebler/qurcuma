// llmclient.cpp - OpenAI-compatible chat completions with tool calling.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "llmclient.h"

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

    QJsonObject body;
    body.insert(QStringLiteral("model"), m_profile.model);
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
