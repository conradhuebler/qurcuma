// remotebackend.cpp - SimulationBackend that runs on another machine
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R2)

#include "remotebackend.h"

#include "protocol.h"
#include "sshtunnel.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>
#include <QWebSocket>

namespace remote {

namespace {
constexpr int kConnectDeadlineMs = 15000;  // the ssh forward needs a moment before it listens
constexpr int kRetryMs = 200;
constexpr int kReconnectWindowMs = 60000;  // matches the server's default grace period
constexpr int kReconnectIntervalMs = 1000;
}

RemoteBackend::RemoteBackend(const QString& host, const QString& serverCommand, QObject* parent)
    : SimulationBackend(parent)
    , m_host(host)
    , m_serverCommand(serverCommand)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &RemoteBackend::connectSocket);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &RemoteBackend::tryReconnect);
}

RemoteBackend::RemoteBackend(const QUrl& url, const QString& token, QObject* parent)
    : SimulationBackend(parent)
    , m_url(url)
    , m_token(token)
{
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &RemoteBackend::connectSocket);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &RemoteBackend::tryReconnect);
}

RemoteBackend::~RemoteBackend()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
    delete m_tunnel;  // ends both ssh processes
    if (m_trajectory.isOpen())
        m_trajectory.close();
}

// ---- start ------------------------------------------------------------------------

void RemoteBackend::start(bool singleStep)
{
    m_singleStep = singleStep;
    m_finished = m_started = m_welcomed = m_stopRequested = m_reconnecting = false;
    m_framesReceived = 0;
    m_statusTimer.start();

    // Resolve and read the files on this machine first: a missing file ends the start here,
    // not on the server.
    QString error;
    SimulationConfig cfg = m_config;
    cfg.writeTrajectory = false;  // the server writes none; this side writes it from the frames
    QJsonObject cfgJson = configToJson(cfg);
    if (!prepareUploads(cfgJson, &error)) {
        QTimer::singleShot(0, this, [this, error] { emit errorOccurred(error); m_finished = true; });
        return;
    }
    m_startConfig = cfgJson;

    if (m_config.writeTrajectory) {
        if (m_trajectoryPath.isEmpty())
            m_trajectoryPath = QDir::current().absoluteFilePath(
                QStringLiteral("remote-%1.trj.xyz").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
        m_trajectory.setFileName(m_trajectoryPath);
        if (!m_trajectory.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            const QString msg = tr("Cannot write the trajectory file %1.").arg(m_trajectoryPath);
            QTimer::singleShot(0, this, [this, msg] { emit errorOccurred(msg); m_finished = true; });
            return;
        }
    }

    m_connectTimer.start();
    if (!m_host.isEmpty()) {
        emit statusText(tr("Starting qurcuma-server on %1 ...").arg(m_host));
        m_tunnel = new SshTunnel(m_host, m_serverCommand, this);
        connect(m_tunnel, &SshTunnel::ready, this, [this](quint16 port, const QString& token) {
            m_url = QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port));
            m_token = token;
            m_connectTimer.restart();
            connectSocket();
        });
        connect(m_tunnel, &SshTunnel::failed, this, [this](const QString& msg) {
            if (!m_finished) {
                m_finished = true;
                emit errorOccurred(msg);
            }
        });
        m_tunnel->start();
    } else {
        connectSocket();
    }
}

bool RemoteBackend::prepareUploads(QJsonObject& cfg, QString* error)
{
    m_uploads.clear();
    qint64 total = 0;
    auto take = [&](QJsonValue& v, const QString& key) -> bool {
        const QString path = v.toString();
        if (path.isEmpty() || path == QLatin1String("none"))
            return true;
        const QFileInfo fi(QDir::current().absoluteFilePath(path));
        if (!fi.isFile()) {
            *error = tr("%1: file '%2' not found on this computer.").arg(key, path);
            return false;
        }
        if (fi.size() > kMaxFileBytes || total + fi.size() > kMaxSessionBytes) {
            *error = tr("%1: '%2' is too large to send (limit %3 MB per file).").arg(key, path).arg(kMaxFileBytes >> 20);
            return false;
        }
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) {
            *error = tr("%1: cannot read '%2'.").arg(key, path);
            return false;
        }
        QString name = key + QLatin1Char('-') + fi.fileName();
        name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._+-]")), QStringLiteral("_"));
        name.replace(QStringLiteral(".."), QStringLiteral("_"));
        name = name.left(128);
        if (!isSafeFileName(name)) {
            *error = tr("%1: cannot form a file name for '%2'.").arg(key, path);
            return false;
        }
        m_uploads.append({ name, f.readAll() });
        total += fi.size();
        v = name;
        return true;
    };

    if (cfg.contains(QStringLiteral("rmsdMtdRefFile"))) {
        QJsonValue v = cfg.value(QStringLiteral("rmsdMtdRefFile"));
        if (!take(v, QStringLiteral("rmsd_mtd_ref_file")))
            return false;
        cfg[QStringLiteral("rmsdMtdRefFile")] = v;
    }
    QJsonObject extra = cfg.value(QStringLiteral("mdExtraParams")).toObject();
    for (const QString& key : uploadParamKeys()) {
        if (!extra.contains(key))
            continue;
        QJsonValue v = extra.value(key);
        if (!take(v, key))
            return false;
        extra[key] = v;
    }
    cfg[QStringLiteral("mdExtraParams")] = extra;
    return true;
}

// ---- connection ----------------------------------------------------------------------

void RemoteBackend::connectSocket()
{
    if (m_finished)
        return;
    if (!m_socket) {
        m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(m_socket, &QWebSocket::connected, this, &RemoteBackend::onConnected);
        connect(m_socket, &QWebSocket::textMessageReceived, this, &RemoteBackend::onText);
        connect(m_socket, &QWebSocket::binaryMessageReceived, this, &RemoteBackend::onBinary);
        connect(m_socket, &QWebSocket::errorOccurred, this, &RemoteBackend::onSocketError);
        connect(m_socket, &QWebSocket::disconnected, this, [this] {
            if (m_finished || !m_welcomed || m_reconnecting)
                return;
            if (m_started)
                beginReconnect();  // the run goes on at the server for its grace period
            else {
                m_finished = true;
                emit errorOccurred(tr("Connection to the remote computer lost."));
            }
        });
    }
    m_socket->open(m_url);
}

void RemoteBackend::beginReconnect()
{
    m_reconnecting = true;
    m_lostTimer.start();
    emit statusText(tr("Connection lost, trying to reconnect (the run continues on the server) ..."));
    m_reconnectTimer.start(kReconnectIntervalMs);
}

void RemoteBackend::tryReconnect()
{
    if (m_finished || !m_reconnecting)
        return;
    if (m_lostTimer.elapsed() > kReconnectWindowMs) {
        m_reconnecting = false;
        m_finished = true;
        emit errorOccurred(tr("Connection to the remote computer lost; it did not come back within %1 s.")
                               .arg(kReconnectWindowMs / 1000));
        return;
    }
    if (m_tunnel)
        m_tunnel->restartForward();  // the ssh forward may have died with the network
    m_socket->abort();
    m_socket->open(m_url);
    m_reconnectTimer.start(kReconnectIntervalMs);  // next attempt if this one fails
}

void RemoteBackend::onSocketError()
{
    if (m_finished)
        return;
    if (m_reconnecting)
        return;  // the reconnect timer tries again
    if (!m_welcomed && m_connectTimer.elapsed() < kConnectDeadlineMs) {
        m_socket->abort();
        m_retry.start(kRetryMs);  // the forward is not listening yet
        return;
    }
    m_finished = true;
    emit errorOccurred(tr("Cannot connect to the remote computer: %1").arg(m_socket->errorString()));
}

void RemoteBackend::onConnected()
{
    QJsonObject hello{ { "type", "hello" }, { "protocol", kProtocolVersion }, { "token", m_token } };
    if (m_reconnecting) {
        hello["attach"] = true;
        hello["session"] = m_sessionName;
    }
    sendJson(hello);
}

void RemoteBackend::sendJson(const QJsonObject& obj)
{
    if (m_socket && m_socket->isValid())
        m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void RemoteBackend::onText(const QString& text)
{
    const QJsonObject msg = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString type = msg.value("type").toString();
    if (type == QLatin1String("welcome") && m_reconnecting) {
        m_reconnecting = false;
        m_reconnectTimer.stop();
        emit statusText(tr("Reconnected to the running simulation."));
    } else if (type == QLatin1String("welcome")) {
        m_welcomed = true;
        m_sessionName = msg.value("session").toString();
        emit capabilities(msg);
        m_serverThreads = msg.value("threads").toInt();
        emit statusText(tr("Connected (%1 threads on the server).").arg(m_serverThreads));
        m_pendingStored = m_uploads.size();
        for (const Upload& u : m_uploads)
            m_socket->sendBinaryMessage(encodeFile(u.name, u.data));
        if (m_pendingStored == 0)
            sendStart();
    } else if (type == QLatin1String("fileStored")) {
        if (--m_pendingStored == 0)
            sendStart();
    } else if (type == QLatin1String("runParameters")) {
        emit runParameters(msg.value("record").toObject());
    } else if (type == QLatin1String("paused")) {
        emit paused();
    } else if (type == QLatin1String("finished")) {
        endRun(msg.value("reason").toString(), msg.value("aborted").toBool(), msg);
    } else if (type == QLatin1String("error")) {
        if (!m_finished) {
            m_reconnecting = false;
            m_reconnectTimer.stop();
            m_finished = true;
            emit errorOccurred(msg.value("message").toString());
        }
    }
}

void RemoteBackend::sendStart()
{
    m_started = true;
    if (m_stopRequested) {
        endRun(tr("Stopped before the remote run started."), false);
        return;
    }
    sendJson({ { "type", "start" }, { "config", m_startConfig },
        { "atoms", atomsToJson(m_atoms) }, { "bonds", bondsToJson(m_bonds) },
        { "liveNci", m_liveNci }, { "singleStep", m_singleStep } });
}

void RemoteBackend::onBinary(const QByteArray& message)
{
    SimulationFrame f;
    QString err;
    if (!decodeFrame(message, f, &err))
        return;  // a corrupt frame is skipped, the run goes on
    ++m_framesReceived;
    if (m_trajectory.isOpen())
        writeTrajectoryFrame(f);
    emit frameReady(SimulationFramePtr::create(std::move(f)));
    if (m_statusTimer.elapsed() >= 1000) {
        m_statusTimer.restart();
        emit statusText(tr("%1: %2 frames received").arg(m_host.isEmpty() ? m_url.host() : m_host).arg(m_framesReceived));
    }
}

void RemoteBackend::writeTrajectoryFrame(const SimulationFrame& f)
{
    QTextStream out(&m_trajectory);
    const int n = int(f.positions.size());
    out << n << '\n' << "step " << f.step << " E=" << QString::number(f.energy, 'f', 10) << " Eh\n";
    for (int i = 0; i < n; ++i) {
        const QString el = i < m_atoms.size() ? m_atoms[i].element : QStringLiteral("X");
        out << el << ' ' << QString::number(f.positions[size_t(i)].x(), 'f', 6) << ' '
            << QString::number(f.positions[size_t(i)].y(), 'f', 6) << ' '
            << QString::number(f.positions[size_t(i)].z(), 'f', 6) << '\n';
    }
}

void RemoteBackend::endRun(const QString& reason, bool aborted, const QJsonObject& stats)
{
    if (m_finished)
        return;
    m_finished = true;
    const double dropped = stats.value("framesDropped").toDouble();
    emit statusText(tr("Remote run ended: %1 frames received, %2 dropped by the server.")
                        .arg(m_framesReceived).arg(quint64(dropped)));
    if (m_trajectory.isOpen())
        m_trajectory.close();
    if (m_socket)
        m_socket->close();
    emit finished(reason, aborted);
}

// ---- run control ---------------------------------------------------------------------

void RemoteBackend::requestStop()
{
    m_stopRequested = true;
    if (m_finished)
        return;
    if (m_reconnecting) {  // cannot reach the server; the run ends there after its grace period
        m_reconnectTimer.stop();
        m_reconnecting = false;
        endRun(tr("Disconnected while the connection was lost; the run on the server stops after its grace period."), false);
        return;
    }
    if (m_started)
        sendJson({ { "type", "stop" } });
    else if (!m_welcomed) {
        // never reached the server: nothing to stop remotely
        m_retry.stop();
        endRun(tr("Stopped before the remote run started."), false);
    }
}

void RemoteBackend::requestPause() { if (m_started && !m_finished) sendJson({ { "type", "pause" } }); }
void RemoteBackend::requestResume() { if (m_started && !m_finished) sendJson({ { "type", "resume" } }); }

void RemoteBackend::injectForce(int atomIndex, QVector3D force, double alpha, int maxShells)
{
    if (!m_started || m_finished)
        return;
    sendJson({ { "type", "injectForce" }, { "atom", atomIndex }, { "fx", double(force.x()) },
        { "fy", double(force.y()) }, { "fz", double(force.z()) }, { "alpha", alpha }, { "maxShells", maxShells } });
}

void RemoteBackend::clearInjectedForce() { if (m_started && !m_finished) sendJson({ { "type", "clearForce" } }); }
void RemoteBackend::setTargetTemperature(double t) { if (m_started && !m_finished) sendJson({ { "type", "setTemperature" }, { "value", t } }); }
void RemoteBackend::setWallTemp(double t) { if (m_started && !m_finished) sendJson({ { "type", "setWallTemp" }, { "value", t } }); }
void RemoteBackend::setWallBeta(double b) { if (m_started && !m_finished) sendJson({ { "type", "setWallBeta" }, { "value", b } }); }

} // namespace remote
