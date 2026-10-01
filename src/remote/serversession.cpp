// serversession.cpp - One client connection of qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R1)

#include "serversession.h"

#include "../simulationbackend.h"
#include "protocol.h"

#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QThread>
#include <QUuid>
#include <QWebSocket>

namespace remote {

namespace {
/// Frames are dropped while more than this is waiting to be sent (latest wins).
constexpr qint64 kMaxPendingBytes = 4LL * 1024 * 1024;
}

ServerSession::ServerSession(const QString& token, const QString& rootDir, int graceSeconds, QObject* parent)
    : QObject(parent)
    , m_token(token)
    , m_graceMs(qMax(1, graceSeconds) * 1000)
{
    const QString name = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-"))
        + QUuid::createUuid().toString(QUuid::Id128).left(6);
    m_sessionDir = QDir(rootDir).absoluteFilePath(name);
    QDir().mkpath(m_sessionDir + QStringLiteral("/in"));
    QDir().mkpath(m_sessionDir + QStringLiteral("/out"));
    QDir::setCurrent(m_sessionDir);

    m_graceTimer.setSingleShot(true);
    connect(&m_graceTimer, &QTimer::timeout, this, &ServerSession::endSession);
}

ServerSession::~ServerSession()
{
    m_ended = true;
    stopRun();
}

QString ServerSession::sessionName() const
{
    return QFileInfo(m_sessionDir).fileName();
}

void ServerSession::bindSocket(QWebSocket* socket)
{
    m_socket = socket;
    m_socket->setParent(this);
    m_authenticated = false;
    // A client that never authenticates must not hold the session forever.
    m_graceTimer.start(qMax(m_graceMs, 60000));
    connect(m_socket, &QWebSocket::textMessageReceived, this, &ServerSession::onText);
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &ServerSession::onBinary);
    connect(m_socket, &QWebSocket::disconnected, this, &ServerSession::onDisconnected);
}

void ServerSession::closeClientConnection()
{
    if (m_socket)
        m_socket->close();
}

void ServerSession::send(const QJsonObject& obj)
{
    if (m_socket && m_socket->isValid())
        m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void ServerSession::sendError(const QString& message)
{
    send({ { "type", "error" }, { "message", message } });
}

void ServerSession::handleHello(const QJsonObject& msg)
{
    auto refuse = [this](const QString& why) {
        sendError(why);
        m_socket->close(QWebSocketProtocol::CloseCodePolicyViolated);
    };
    if (msg.value("token").toString() != m_token || m_token.isEmpty())
        return refuse(QStringLiteral("authentication failed"));
    if (msg.value("protocol").toInt() != kProtocolVersion)
        return refuse(QStringLiteral("protocol version %1 not supported (server speaks %2)")
                          .arg(msg.value("protocol").toInt()).arg(kProtocolVersion));
    const bool attach = msg.value("attach").toBool();
    if (attach && (!m_everAuthenticated || msg.value("session").toString() != sessionName()))
        return refuse(QStringLiteral("no such session to attach to"));
    if (!attach && m_everAuthenticated)
        return refuse(QStringLiteral("session busy: a run of an earlier client is waiting for it to return"));

    m_authenticated = m_everAuthenticated = true;
    m_graceTimer.stop();
    QJsonArray gpus;
#if defined(USE_CUDA)
    gpus.append("cuda");
#endif
#if defined(USE_ROCM)
    gpus.append("rocm");
#endif
#if defined(USE_VULKAN)
    gpus.append("vulkan");
#endif
    send({ { "type", "welcome" }, { "protocol", kProtocolVersion }, { "session", sessionName() },
        { "threads", QThread::idealThreadCount() }, { "gpuBackends", gpus },
        { "attached", attach }, { "running", m_backend != nullptr } });
    if (!m_finishedMsg.isEmpty())
        send(m_finishedMsg);  // the run ended while the client was away
}

void ServerSession::onText(const QString& text)
{
    const QJsonObject msg = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString type = msg.value("type").toString();

    if (!m_authenticated) {
        if (type != QLatin1String("hello")) {
            sendError(QStringLiteral("authentication failed"));
            m_socket->close(QWebSocketProtocol::CloseCodePolicyViolated);
            return;
        }
        handleHello(msg);
        return;
    }

    if (type == QLatin1String("start")) {
        handleStart(msg);
    } else if (!m_backend) {
        sendError(QStringLiteral("'%1' needs a running simulation").arg(type));
    } else if (type == QLatin1String("stop")) {
        m_backend->requestStop();
    } else if (type == QLatin1String("pause")) {
        m_backend->requestPause();
    } else if (type == QLatin1String("resume")) {
        m_backend->requestResume();
    } else if (type == QLatin1String("injectForce")) {
        m_backend->injectForce(msg.value("atom").toInt(),
            QVector3D(float(msg.value("fx").toDouble()), float(msg.value("fy").toDouble()), float(msg.value("fz").toDouble())),
            msg.value("alpha").toDouble(), msg.value("maxShells").toInt());
    } else if (type == QLatin1String("clearForce")) {
        m_backend->clearInjectedForce();
    } else if (type == QLatin1String("setTemperature")) {
        m_backend->setTargetTemperature(msg.value("value").toDouble());
    } else if (type == QLatin1String("setWallTemp")) {
        m_backend->setWallTemp(msg.value("value").toDouble());
    } else if (type == QLatin1String("setWallBeta")) {
        m_backend->setWallBeta(msg.value("value").toDouble());
    } else {
        sendError(QStringLiteral("unknown message type '%1'").arg(type));
    }
}

void ServerSession::onBinary(const QByteArray& message)
{
    if (!m_authenticated) {
        m_socket->close(QWebSocketProtocol::CloseCodePolicyViolated);
        return;
    }
    if (m_backend || !m_finishedMsg.isEmpty()) {
        sendError(QStringLiteral("files cannot be uploaded while a run is active"));
        return;
    }
    QString name, err;
    QByteArray data;
    if (!decodeFile(message, name, data, &err)) {
        sendError(err);
        return;
    }
    if (m_uploadedBytes + data.size() > kMaxSessionBytes) {
        sendError(QStringLiteral("session upload limit reached"));
        return;
    }
    QFile f(m_sessionDir + QStringLiteral("/in/") + name);  // name checked by decodeFile
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
        sendError(QStringLiteral("could not store '%1'").arg(name));
        return;
    }
    m_uploadedBytes += data.size();
    m_uploaded.insert(name);
    send({ { "type", "fileStored" }, { "name", name } });
}

void ServerSession::handleStart(const QJsonObject& msg)
{
    if (m_backend) {
        sendError(QStringLiteral("a run is already active"));
        return;
    }
    QString err;
    QVector<MolAtom> atoms;
    QVector<MolBond> bonds;
    if (!atomsFromJson(msg.value("atoms"), atoms, &err) || !bondsFromJson(msg.value("bonds"), bonds, &err)) {
        sendError(err);
        return;
    }
    if (atoms.isEmpty()) {
        sendError(QStringLiteral("no atoms"));
        return;
    }
    QJsonObject cfgJson = msg.value("config").toObject();
    if (!applyFilePolicy(cfgJson, m_uploaded, m_sessionDir + QStringLiteral("/in"), &err)) {
        sendError(err);
        return;
    }

    m_framesSent = m_framesDropped = 0;
    m_finishedMsg = QJsonObject();
    m_backend = new LocalBackend(this);
    m_backend->setMolecule(atoms);
    m_backend->setBonds(bonds);
    m_backend->setConfig(configFromJson(cfgJson));
    m_backend->setLiveNci(msg.value("liveNci").toBool());

    connect(m_backend, &SimulationBackend::frameReady, this, [this](SimulationFramePtr frame) {
        if (!frame || !m_socket || !m_socket->isValid())
            return;
        if (m_socket->bytesToWrite() > kMaxPendingBytes) {
            ++m_framesDropped;  // slow link: latest wins, the simulation is never slowed down
            return;
        }
        m_socket->sendBinaryMessage(encodeFrame(*frame));
        ++m_framesSent;
    });
    connect(m_backend, &SimulationBackend::runParameters, this, [this](const QJsonObject& rec) {
        send({ { "type", "runParameters" }, { "record", rec } });
    });
    connect(m_backend, &SimulationBackend::paused, this, [this]() { send({ { "type", "paused" } }); });
    connect(m_backend, &SimulationBackend::errorOccurred, this, [this](const QString& e) { sendError(e); });
    connect(m_backend, &SimulationBackend::finished, this, [this](const QString& reason, bool aborted) {
        m_finishedMsg = { { "type", "finished" }, { "reason", reason }, { "aborted", aborted },
            { "framesSent", double(m_framesSent) }, { "framesDropped", double(m_framesDropped) } };
        send(m_finishedMsg);  // kept: a client that is away gets it when it returns
        m_backend->deleteLater();
        m_backend = nullptr;
    });

    m_backend->start(msg.value("singleStep").toBool());
}

void ServerSession::stopRun()
{
    if (!m_backend)
        return;
    m_backend->requestStop();
    delete m_backend;  // LocalBackend waits for its worker thread
    m_backend = nullptr;
}

void ServerSession::onDisconnected()
{
    const bool hadClient = m_authenticated;
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_authenticated = false;
    if (!hadClient) {  // a probe or a refused hello: keep waiting for the real client
        m_graceTimer.start(qMax(m_graceMs, 60000));
        return;
    }
    if (m_backend) {
        // Keep the run going for the grace period. The sticky grab force dies at once so
        // a lost client cannot leave the molecule pulled.
        m_backend->clearInjectedForce();
        m_graceTimer.start(m_graceMs);
        return;
    }
    endSession();
}

void ServerSession::endSession()
{
    if (m_ended)
        return;
    m_ended = true;
    m_graceTimer.stop();
    stopRun();
    emit ended();
}

// ---- RemoteServer -------------------------------------------------------------------

RemoteServer::RemoteServer(const QString& token, const QString& rootDir, int graceSeconds, QObject* parent)
    : QObject(parent)
    , m_server(QStringLiteral("qurcuma-server"), QWebSocketServer::NonSecureMode)
    , m_token(token)
    , m_root(rootDir)
    , m_graceSeconds(graceSeconds)
{
    connect(&m_server, &QWebSocketServer::newConnection, this, &RemoteServer::onNewConnection);
}

bool RemoteServer::listen(quint16 port) { return m_server.listen(QHostAddress::LocalHost, port); }
quint16 RemoteServer::port() const { return m_server.serverPort(); }
QString RemoteServer::errorString() const { return m_server.errorString(); }

void RemoteServer::setAccepting(bool on)
{
    if (on)
        m_server.resumeAccepting();
    else
        m_server.pauseAccepting();
}

void RemoteServer::onNewConnection()
{
    QWebSocket* socket = m_server.nextPendingConnection();
    if (!socket)
        return;
    if (m_active && m_active->canAttach()) {  // a client returning to its session
        m_active->bindSocket(socket);
        return;
    }
    if (m_active) {  // busy: one session per process (the working directory is process-wide)
        socket->close(QWebSocketProtocol::CloseCodeGoingAway);
        socket->deleteLater();
        return;
    }
    m_active = new ServerSession(m_token, m_root, m_graceSeconds, this);
    connect(m_active, &ServerSession::ended, this, [this] {
        m_active->deleteLater();
        m_active = nullptr;
        emit sessionEnded();
    });
    m_active->bindSocket(socket);
}

} // namespace remote
