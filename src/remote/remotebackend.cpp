// remotebackend.cpp - SimulationBackend that runs on another machine
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R2)

#include "remotebackend.h"
#include <cmath>

#include "protocol.h"
#include "remotelog.h"
#include "sshtunnel.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
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
    m_perfTimer.setInterval(1000);
    connect(&m_perfTimer, &QTimer::timeout, this, &RemoteBackend::perfTick);
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
    m_perfTimer.setInterval(1000);
    connect(&m_perfTimer, &QTimer::timeout, this, &RemoteBackend::perfTick);
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

void RemoteBackend::log(const QString& text)
{
    emit logMessage(appendLog(m_host.isEmpty() ? QStringLiteral("remote") : QStringLiteral("remote ") + m_host, text));
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
        connect(m_tunnel, &SshTunnel::log, this, &RemoteBackend::log);
        connect(m_tunnel, &SshTunnel::ready, this, [this](quint16 port, const QString& token) {
            m_url = QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port));
            m_token = token;
            log(tr("server is up, tunnel on local port %1; connecting").arg(port));
            m_connectTimer.restart();
            connectSocket();
        });
        connect(m_tunnel, &SshTunnel::failed, this, [this](const QString& msg) {
            log(tr("tunnel failed: %1").arg(msg));
            if (!m_finished) {
                m_finished = true;
                emit errorOccurred(msg + tr("  (details: %1)").arg(logFilePath()));
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
        log(tr("websocket not ready (%1), retrying").arg(m_socket->errorString()));
        m_socket->abort();
        m_retry.start(kRetryMs);  // the forward is not listening yet
        return;
    }
    m_finished = true;
    log(tr("websocket error: %1").arg(m_socket->errorString()));
    emit errorOccurred(tr("Cannot connect to the remote computer: %1  (details: %2)").arg(m_socket->errorString(), logFilePath()));
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
    if (type == QLatin1String("pong")) {
        m_pfRttLast = m_clock.elapsed() - qint64(msg.value("t").toDouble());
        m_pfRttMax = qMax(m_pfRttMax, m_pfRttLast);
        return;
    }
    if (type == QLatin1String("linkStats")) {
        m_serverStats = msg;
        return;
    }
    log(tr("server message: %1").arg(type));
    if (type == QLatin1String("welcome") && m_reconnecting) {
        m_reconnecting = false;
        m_reconnectTimer.stop();
        emit statusText(tr("Reconnected to the running simulation."));
    } else if (type == QLatin1String("welcome")) {
        m_welcomed = true;
        m_clock.start();
        m_perfWindow.start();
        m_perfTimer.start();
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
        log(tr("server error: %1").arg(msg.value("message").toString()));
        if (!m_finished) {
            m_reconnecting = false;
            m_reconnectTimer.stop();
            m_finished = true;
            emit errorOccurred(msg.value("message").toString());
        }
    }
}

/// One diagnostic line per second (Claude Generated 2026). Reading it: `recv` below the
/// server's `sent` means the link or this thread is the limit; a large `pending` or RTT with
/// small `busy` points at the link, a large `busy` at decode, trajectory write or the viewer.
void RemoteBackend::perfTick()
{
    if (m_socket && m_socket->isValid() && m_welcomed)
        sendJson({ { "type", "ping" }, { "t", double(m_clock.elapsed()) } });
    const double s = qMax<qint64>(1, m_perfWindow.restart()) / 1000.0;
    const double n = qMax<quint64>(1, m_pfFrames);
    const auto ms = [&](qint64 ns) { return double(ns) / 1e6 / n; };
    const double gapMean = m_pfFrames ? m_pfGapSum / double(m_pfFrames) : 0.0;
    const double gapSd = std::sqrt(qMax(0.0, (m_pfFrames ? m_pfGapSq / double(m_pfFrames) : 0.0) - gapMean * gapMean));
    QString line = tr("perf: recv %1 fps %2 kB/s | this thread %3 % busy (decode %4, write %5, emit %6 ms/frame) | RTT %7 ms (max %8)")
                       .arg(m_pfFrames / s, 0, 'f', 1).arg(m_pfBytes / 1024.0 / s, 0, 'f', 0)
                       .arg(100.0 * double(m_pfBusyNs) / 1e9 / s, 0, 'f', 0)
                       .arg(ms(m_pfDecodeNs), 0, 'f', 2).arg(ms(m_pfWriteNs), 0, 'f', 2).arg(ms(m_pfEmitNs), 0, 'f', 2)
                       .arg(m_pfRttLast).arg(m_pfRttMax);
    line += tr(" | arrival gap %1 ms (sd %2, max %3)").arg(gapMean, 0, 'f', 1).arg(gapSd, 0, 'f', 1).arg(m_pfGapMax, 0, 'f', 0);
    if (!m_serverStats.isEmpty()) {
        const QJsonObject& o = m_serverStats;
        const double sec = qMax(1.0, o.value("intervalMs").toDouble()) / 1000.0;
        line += tr(" | server: produced %1/s sent %2/s dropped %3/s, %4 kB/s, pending %5 kB (max %6), encode %7 ms/frame, step %8, produced gap %9 ms (sd %10, max %11)")
                    .arg(o.value("produced").toDouble() / sec, 0, 'f', 1).arg(o.value("sent").toDouble() / sec, 0, 'f', 1)
                    .arg(o.value("dropped").toDouble() / sec, 0, 'f', 1).arg(o.value("bytes").toDouble() / 1024.0 / sec, 0, 'f', 0)
                    .arg(o.value("pendingBytes").toDouble() / 1024.0, 0, 'f', 0).arg(o.value("maxPendingBytes").toDouble() / 1024.0, 0, 'f', 0)
                    .arg(o.value("encodeUsPerFrame").toDouble() / 1000.0, 0, 'f', 3).arg(o.value("step").toInt())
                    .arg(o.value("gapMeanMs").toDouble(), 0, 'f', 1).arg(o.value("gapSdMs").toDouble(), 0, 'f', 1)
                    .arg(o.value("gapMaxMs").toDouble(), 0, 'f', 0);
        line += tr(" | thinned %1/s, link drains %2 kB/s, drop limit %3 kB")
                    .arg(o.value("thinned").toDouble() / sec, 0, 'f', 1)
                    .arg(o.value("drainBytesPerSec").toDouble() / 1024.0, 0, 'f', 0)
                    .arg(o.value("pendingLimitBytes").toDouble() / 1024.0, 0, 'f', 0);
        if (o.value("slowSteps").toInt() > 0) {
            QStringList slow;
            for (const QJsonValue& v : o.value("slowList").toArray())
                slow << QStringLiteral("%1:%2").arg(v.toArray().at(0).toInt()).arg(v.toArray().at(1).toInt());
            line += tr(" | slow steps (>3x mean) %1 [step:ms %2]").arg(o.value("slowSteps").toInt()).arg(slow.join(QLatin1Char(' ')));
        }
    }
    if (m_pfFrames > 0 || m_started)
        log(line);
    m_pfFrames = m_pfBytes = 0;
    m_pfBusyNs = m_pfDecodeNs = m_pfWriteNs = m_pfEmitNs = 0;
    m_pfRttMax = 0;
    m_pfGapSum = m_pfGapSq = m_pfGapMax = 0;
}

void RemoteBackend::sendStart()
{
    m_started = true;
    log(tr("sending start: %1 atoms, %2 uploaded file(s)").arg(m_atoms.size()).arg(m_uploads.size()));
    // The settings that decide how many steps per second the server can deliver, so a rate in
    // the perf lines can be read against them (Claude Generated 2026).
    log(tr("run: %1, method %2, gpu %3, fps limit %4, T %5 K (%6), dt %7 fs, steps %8, topology %9, live NCI %10, server threads %11")
            .arg(m_config.mode == SimulationConfig::Mode::MolecularDynamics ? tr("MD") : tr("optimisation"))
            .arg(m_config.method, m_config.gpu).arg(m_config.fpsLimit).arg(m_config.temperature, 0, 'f', 1)
            .arg(m_config.thermostat).arg(m_config.timestep, 0, 'f', 2).arg(m_config.steps)
            .arg(m_config.topologyMode).arg(m_liveNci ? tr("on") : tr("off")).arg(m_serverThreads));
    if (m_stopRequested) {
        endRun(tr("Stopped before the remote run started."), false);
        return;
    }
    sendJson({ { "type", "start" }, { "config", m_startConfig },
        { "atoms", atomsToJson(m_atoms) }, { "bonds", bondsToJson(m_bonds) },
        { "liveNci", m_liveNci }, { "singleStep", m_singleStep }, { "serverTrajectory", m_serverTrajectory },
        { "clientTrajectory", m_config.writeTrajectory && !m_serverTrajectory } });
}

void RemoteBackend::onBinary(const QByteArray& message)
{
    if (m_pfGapClock.isValid()) {
        const double gap = double(m_pfGapClock.nsecsElapsed()) / 1e6;
        m_pfGapSum += gap;
        m_pfGapSq += gap * gap;
        m_pfGapMax = qMax(m_pfGapMax, gap);
    }
    m_pfGapClock.restart();
    QElapsedTimer busy;
    busy.start();
    SimulationFrame f;
    QString err;
    if (!decodeFrame(message, f, &err))
        return;  // a corrupt frame is skipped, the run goes on
    ++m_framesReceived;
    ++m_pfFrames;
    m_pfBytes += quint64(message.size());
    m_pfDecodeNs += busy.nsecsElapsed();
    if (m_trajectory.isOpen()) {
        const qint64 t0 = busy.nsecsElapsed();
        writeTrajectoryFrame(f);
        m_pfWriteNs += busy.nsecsElapsed() - t0;
    }
    const qint64 t1 = busy.nsecsElapsed();
    emit frameReady(SimulationFramePtr::create(std::move(f)));  // direct slots run inside this call
    m_pfEmitNs += busy.nsecsElapsed() - t1;
    m_pfBusyNs += busy.nsecsElapsed();
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
    m_perfTimer.stop();
    const double dropped = stats.value("framesDropped").toDouble();
    if (!(m_serverTrajectory && stats.contains("sessionDir")))
        emit statusText(tr("Remote run ended: %1 frames received, %2 dropped by the server.")
                            .arg(m_framesReceived).arg(quint64(dropped)));
    if (m_trajectory.isOpen())
        m_trajectory.close();
    if (m_serverTrajectory && stats.contains("sessionDir")) {
        QStringList names;
        for (const QJsonValue& v : stats.value("sessionFiles").toObject().value("files").toArray()) {
            const QJsonObject o = v.toObject();
            names << QStringLiteral("%1 (%2 kB)").arg(o.value("name").toString()).arg(qint64(o.value("size").toDouble() / 1024));
        }
        emit statusText(tr("Files on %1 in %2: %3").arg(m_host.isEmpty() ? m_url.host() : m_host,
            stats.value("sessionDir").toString(), names.join(QStringLiteral(", "))));
    }
    if (!m_host.isEmpty() && !stats.value("sessionDir").toString().isEmpty())
        emit sessionResultsAvailable(m_host, stats.value("sessionDir").toString());
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

void RemoteBackend::injectForce(int source, int atomIndex, QVector3D force, double alpha, int maxShells)
{
    if (!m_started || m_finished)
        return;
    sendJson({ { "type", "injectForce" }, { "source", source }, { "atom", atomIndex }, { "fx", double(force.x()) },
        { "fy", double(force.y()) }, { "fz", double(force.z()) }, { "alpha", alpha }, { "maxShells", maxShells } });
}

void RemoteBackend::injectForces(QVector<int> atoms, QVector<QVector3D> forces, double alpha, int maxShells)
{
    if (!m_started || m_finished)
        return;
    QJsonArray a, fx, fy, fz;
    for (int i = 0; i < atoms.size() && i < forces.size(); ++i) {
        a.append(atoms[i]);
        fx.append(double(forces[i].x()));
        fy.append(double(forces[i].y()));
        fz.append(double(forces[i].z()));
    }
    sendJson({ { "type", "injectForces" }, { "atoms", a }, { "fx", fx }, { "fy", fy }, { "fz", fz },
        { "alpha", alpha }, { "maxShells", maxShells } });
}

void RemoteBackend::setExternalPotentials(QJsonArray potentials)
{
    if (m_started && !m_finished)
        sendJson({ { "type", "setPotentials" }, { "potentials", potentials } });
}

void RemoteBackend::clearInjectedForce(int source)
{
    if (m_started && !m_finished)
        sendJson({ { "type", "clearForce" }, { "source", source } });
}
void RemoteBackend::setTargetTemperature(double t) { if (m_started && !m_finished) sendJson({ { "type", "setTemperature" }, { "value", t } }); }
void RemoteBackend::setWallTemp(double t) { if (m_started && !m_finished) sendJson({ { "type", "setWallTemp" }, { "value", t } }); }
void RemoteBackend::setWallBeta(double b) { if (m_started && !m_finished) sendJson({ { "type", "setWallBeta" }, { "value", b } }); }

} // namespace remote
