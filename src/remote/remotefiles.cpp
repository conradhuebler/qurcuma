// remotefiles.cpp - Browse and download files on the machine that runs qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R4)

#include "remotefiles.h"

#include "protocol.h"
#include "sshtunnel.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QWebSocket>

namespace remote {

namespace {
constexpr int kConnectDeadlineMs = 15000;
constexpr int kRetryMs = 200;
}

RemoteFiles::RemoteFiles(const QString& host, const QString& serverCommand, QObject* parent)
    : QObject(parent), m_host(host), m_serverCommand(serverCommand)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &RemoteFiles::connectSocket);
}

RemoteFiles::RemoteFiles(const QUrl& url, const QString& token, QObject* parent)
    : QObject(parent), m_url(url), m_token(token)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &RemoteFiles::connectSocket);
}

RemoteFiles::~RemoteFiles()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
    delete m_tunnel;
    if (m_out.isOpen()) {
        m_out.close();
        m_out.remove();  // the .part file
    }
}

void RemoteFiles::connectToHost()
{
    m_closed = false;
    m_welcomed = false;
    m_connectTimer.start();
    if (!m_host.isEmpty()) {
        m_tunnel = new SshTunnel(m_host, m_serverCommand, this);
        connect(m_tunnel, &SshTunnel::ready, this, [this](quint16 port, const QString& token) {
            m_url = QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port));
            m_token = token;
            m_connectTimer.restart();
            connectSocket();
        });
        connect(m_tunnel, &SshTunnel::failed, this, [this](const QString& msg) {
            if (!m_closed) {
                m_closed = true;
                emit failed(msg);
            }
        });
        m_tunnel->start();
    } else {
        connectSocket();
    }
}

void RemoteFiles::connectSocket()
{
    if (m_closed)
        return;
    if (!m_socket) {
        m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(m_socket, &QWebSocket::connected, this, [this] {
            sendJson({ { "type", "hello" }, { "protocol", kProtocolVersion }, { "token", m_token } });
        });
        connect(m_socket, &QWebSocket::textMessageReceived, this, &RemoteFiles::onText);
        connect(m_socket, &QWebSocket::binaryMessageReceived, this, &RemoteFiles::onBinary);
        connect(m_socket, &QWebSocket::errorOccurred, this, &RemoteFiles::onSocketError);
        connect(m_socket, &QWebSocket::disconnected, this, [this] {
            if (m_closed)
                return;
            m_closed = true;
            abortDownload(tr("Connection closed."));
            emit failed(tr("Connection to the remote computer closed."));
        });
    }
    m_socket->open(m_url);
}

void RemoteFiles::onSocketError()
{
    if (m_closed)
        return;
    if (!m_welcomed && m_connectTimer.elapsed() < kConnectDeadlineMs) {
        m_socket->abort();
        m_retry.start(kRetryMs);  // the forward is not listening yet
        return;
    }
    m_closed = true;
    abortDownload(tr("Connection lost."));
    emit failed(tr("Cannot connect to the remote computer: %1").arg(m_socket->errorString()));
}

void RemoteFiles::sendJson(const QJsonObject& obj)
{
    if (m_socket && m_socket->isValid())
        m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void RemoteFiles::list(const QString& remotePath)
{
    sendJson({ { "type", "list" }, { "path", remotePath } });
}

quint32 RemoteFiles::download(const QString& remotePath, const QString& localPath)
{
    const quint32 id = m_nextId++;
    if (m_activeId != 0) {
        emit downloadFailed(id, tr("Another download is running."));
        return id;
    }
    QDir().mkpath(QFileInfo(localPath).absolutePath());
    m_out.setFileName(localPath + QStringLiteral(".part"));
    if (!m_out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit downloadFailed(id, tr("Cannot write %1.").arg(m_out.fileName()));
        return id;
    }
    m_activeId = id;
    m_localPath = localPath;
    m_hash.reset();
    m_received = m_expected = 0;
    sendJson({ { "type", "getFile" }, { "id", double(id) }, { "path", remotePath } });
    return id;
}

void RemoteFiles::cancelDownload()
{
    if (m_activeId == 0)
        return;
    sendJson({ { "type", "cancelFile" }, { "id", double(m_activeId) } });
    abortDownload(tr("Cancelled."));
}

void RemoteFiles::abortDownload(const QString& message)
{
    if (m_activeId == 0)
        return;
    const quint32 id = m_activeId;
    m_activeId = 0;
    m_out.close();
    m_out.remove();
    emit downloadFailed(id, message);
}

void RemoteFiles::onText(const QString& text)
{
    const QJsonObject msg = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString type = msg.value("type").toString();
    if (type == QLatin1String("welcome")) {
        m_welcomed = true;
        list(QString());  // the first shared directory; its reply carries the roots
    } else if (type == QLatin1String("listing")) {
        if (msg.contains("error")) {
            emit listingFailed(msg.value("request").toString(), msg.value("error").toString());
            return;
        }
        QVector<Entry> entries;
        for (const QJsonValue& v : msg.value("entries").toArray()) {
            const QJsonObject o = v.toObject();
            entries.append({ o.value("name").toString(), o.value("dir").toBool(),
                qint64(o.value("size").toDouble()), qint64(o.value("mtime").toDouble()) });
        }
        if (!msg.value("roots").toArray().isEmpty() && !m_connectedEmitted) {
            m_connectedEmitted = true;
            QStringList roots;
            for (const QJsonValue& r : msg.value("roots").toArray())
                roots << r.toString();
            emit connected(roots);
        }
        emit listing(msg.value("path").toString(), entries, msg.value("truncated").toBool());
    } else if (type == QLatin1String("fileBegin")) {
        if (quint32(msg.value("id").toDouble()) == m_activeId) {
            m_expected = qint64(msg.value("size").toDouble());
            emit downloadProgress(m_activeId, 0, m_expected);
        }
    } else if (type == QLatin1String("fileEnd")) {
        if (quint32(msg.value("id").toDouble()) != m_activeId)
            return;
        const quint32 id = m_activeId;
        const QString sha = QString::fromLatin1(m_hash.result().toHex());
        m_out.close();
        if (m_received != m_expected || sha != msg.value("sha256").toString()) {
            m_activeId = 0;
            m_out.remove();
            emit downloadFailed(id, tr("The downloaded file does not match (size or checksum)."));
            return;
        }
        QFile::remove(m_localPath);
        if (!m_out.rename(m_localPath)) {
            m_activeId = 0;
            m_out.remove();
            emit downloadFailed(id, tr("Cannot write %1.").arg(m_localPath));
            return;
        }
        m_activeId = 0;
        emit downloadFinished(id, m_localPath);
    } else if (type == QLatin1String("fileError")) {
        if (quint32(msg.value("id").toDouble()) == m_activeId)
            abortDownload(msg.value("message").toString());
    } else if (type == QLatin1String("error")) {
        emit failed(msg.value("message").toString());
    }
}

void RemoteFiles::onBinary(const QByteArray& message)
{
    quint32 id = 0;
    QByteArray data;
    if (!decodeChunk(message, id, data) || id != m_activeId || !m_out.isOpen())
        return;
    if (m_out.write(data) != data.size()) {
        abortDownload(tr("Cannot write %1.").arg(m_out.fileName()));
        sendJson({ { "type", "cancelFile" }, { "id", double(id) } });
        return;
    }
    m_hash.addData(data);
    m_received += data.size();
    emit downloadProgress(id, m_received, m_expected);
}

} // namespace remote
