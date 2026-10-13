// serversession.cpp - One client connection of qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R1)

#include "serversession.h"
#include <cmath>

#include "../simulationbackend.h"
#include "protocol.h"

#include <QDateTime>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QThread>
#include <QUuid>
#include <QWebSocket>

#include "core/energy_calculators/gpu_plugin.h"

namespace remote {

namespace {
/// Frames are dropped while more than this is waiting to be sent (latest wins).
constexpr qint64 kMaxPendingBytes = 4LL * 1024 * 1024;
/// Frames are sent at most this often when nothing needs every step (the viewer draws at most
/// at screen rate anyway). Frames that carry reaction events are never thinned.
constexpr int kMaxSendFps = 60;
/// How much unsent data is tolerated, as time at the rate the link drains.
constexpr double kPendingSeconds = 0.25;
}

ServerSession::ServerSession(const QString& token, const QString& rootDir, int graceSeconds,
    const QStringList& browseRoots, QObject* parent)
    : QObject(parent)
    , m_token(token)
    , m_graceMs(qMax(1, graceSeconds) * 1000)
{
    m_browseRoots = QStringList{ rootDir } + browseRoots;
    const QString name = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-"))
        + QUuid::createUuid().toString(QUuid::Id128).left(6);
    m_sessionDir = QDir(rootDir).absoluteFilePath(name);
    QDir().mkpath(m_sessionDir + QStringLiteral("/in"));
    QDir().mkpath(m_sessionDir + QStringLiteral("/out"));
    QDir::setCurrent(m_sessionDir);

    m_graceTimer.setSingleShot(true);
    connect(&m_graceTimer, &QTimer::timeout, this, &ServerSession::endSession);
    m_statsTimer.setInterval(1000);
    m_holdTimer.setSingleShot(true);
    connect(&m_holdTimer, &QTimer::timeout, this, &ServerSession::flushHeldFrame);
    connect(&m_statsTimer, &QTimer::timeout, this, &ServerSession::sendLinkStats);
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
    connect(m_socket, &QWebSocket::bytesWritten, this, [this](qint64 n) {
        m_drainBytesIv += n;
        pumpTransfer();
    });
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
    for (const std::string& b : gpu_plugin::knownBackends())  // plugins found next to the server binary
        if (gpu_plugin::available(b))
            gpus.append(QString::fromStdString(b));
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

    if (type == QLatin1String("ping")) {
        // Answered at once, but queued behind whatever frames are already waiting on the
        // socket: the round trip the client measures is the delay a frame would see.
        send({ { "type", "pong" }, { "t", msg.value("t") } });
    } else if (type == QLatin1String("start")) {
        handleStart(msg);
    } else if (type == QLatin1String("list")) {
        handleList(msg);
    } else if (type == QLatin1String("getFile")) {
        handleGetFile(msg);
    } else if (type == QLatin1String("cancelFile")) {
        if (m_transferFile.isOpen() && quint32(msg.value("id").toDouble()) == m_transferId)
            finishTransfer(true);
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
    SimulationConfig cfg = configFromJson(cfgJson);
    // The client writes its own trajectory from the frames; a complete one on this machine
    // is only written when asked for (serverTrajectory), and stays in the session directory.
    cfg.writeTrajectory = msg.value("serverTrajectory").toBool();
    m_clientTrajectory = msg.value("clientTrajectory").toBool();
    m_backend->setConfig(cfg);
    m_backend->setLiveNci(msg.value("liveNci").toBool());

    connect(m_backend, &SimulationBackend::frameReady, this, [this](SimulationFramePtr frame) {
        if (!frame || !m_socket || !m_socket->isValid())
            return;
        ++m_ivProduced;
        m_lastStep = frame->step;
        if (m_gapClock.isValid()) {
            const double gap = double(m_gapClock.nsecsElapsed()) / 1e6;
            m_ivGapSum += gap;
            m_ivGapSq += gap * gap;
            m_ivGapMax = qMax(m_ivGapMax, gap);
            // An outlier step (more than three times the running mean): which step, how long.
            if (m_gapEma > 0 && gap > 3.0 * m_gapEma) {
                ++m_ivSlow;
                if (m_ivSlowList.size() < 12)
                    m_ivSlowList.append(QJsonArray{ frame->step, qRound(gap) });
            }
            m_gapEma = m_gapEma > 0 ? 0.95 * m_gapEma + 0.05 * gap : gap;
        }
        m_gapClock.restart();
        // Rate limit with trailing edge: the viewer cannot show more than screen rate, and the
        // link is better used for frames it can show. A frame that comes too soon is held (a
        // newer one replaces it) and sent when the spacing is over, so a source near the limit
        // keeps an even rhythm. Not with a client-side trajectory (it wants every frame), and a
        // frame that carries a reaction event is always sent at once.
        const qint64 interval = 1000 / kMaxSendFps;
        const bool exempt = m_clientTrajectory || !frame->events.isEmpty();
        if (!exempt && m_sendClock.isValid() && m_sendClock.elapsed() < interval) {
            if (m_heldFrame)
                ++m_ivThinned;
            m_heldFrame = frame;
            if (!m_holdTimer.isActive())
                m_holdTimer.start(int(qMax<qint64>(1, interval - m_sendClock.elapsed())));
            return;
        }
        m_heldFrame.reset();
        m_holdTimer.stop();
        transmitFrame(frame);
    });
    m_ivProduced = m_ivSent = m_ivDropped = m_ivBytes = 0;
    m_ivEncodeNs = m_ivMaxPending = 0;
    m_ivThinned = 0;
    m_drainBytesIv = 0;
    m_drainRate = 0;
    m_avgFrameBytes = 0;
    m_sendClock.invalidate();
    m_statsClock.start();
    m_statsTimer.start();
    connect(m_backend, &SimulationBackend::runParameters, this, [this](const QJsonObject& rec) {
        send({ { "type", "runParameters" }, { "record", rec } });
    });
    connect(m_backend, &SimulationBackend::paused, this, [this]() { send({ { "type", "paused" } }); });
    connect(m_backend, &SimulationBackend::errorOccurred, this, [this](const QString& e) { sendError(e); });
    connect(m_backend, &SimulationBackend::finished, this, [this](const QString& reason, bool aborted) {
        m_finishedMsg = { { "type", "finished" }, { "reason", reason }, { "aborted", aborted },
            { "framesSent", double(m_framesSent) }, { "framesDropped", double(m_framesDropped) },
            { "sessionDir", m_sessionDir }, { "sessionFiles", sessionFilesJson() } };
        m_statsTimer.stop();
        flushHeldFrame();
        send(m_finishedMsg);  // kept: a client that is away gets it when it returns
        m_backend->deleteLater();
        m_backend = nullptr;
    });

    m_backend->start(msg.value("singleStep").toBool());
}

/// Encodes and sends one frame unless more is waiting on the socket than pendingLimit() allows.
void ServerSession::transmitFrame(const SimulationFramePtr& frame)
{
    if (!frame || !m_socket || !m_socket->isValid())
        return;
    const qint64 pending = m_socket->bytesToWrite();
    m_ivMaxPending = qMax(m_ivMaxPending, pending);
    if (pending > pendingLimit()) {
        ++m_framesDropped;  // slow link: latest wins, the simulation is never slowed down
        ++m_ivDropped;
        return;
    }
    QElapsedTimer encodeTimer;
    encodeTimer.start();
    const QByteArray bytes = encodeFrame(*frame);
    m_ivEncodeNs += encodeTimer.nsecsElapsed();
    m_ivBytes += quint64(bytes.size());
    m_avgFrameBytes = m_avgFrameBytes > 0 ? 0.9 * m_avgFrameBytes + 0.1 * double(bytes.size()) : double(bytes.size());
    m_sendClock.restart();
    m_socket->sendBinaryMessage(bytes);
    ++m_framesSent;
    ++m_ivSent;
}

/// A held frame goes out before anything that ends the run, so the client's last frame is the
/// run's last frame.
void ServerSession::flushHeldFrame()
{
    m_holdTimer.stop();
    if (m_heldFrame) {
        const SimulationFramePtr f = m_heldFrame;
        m_heldFrame.reset();
        transmitFrame(f);
    }
}

/// Unsent bytes tolerated before a frame is dropped: what the link drains in kPendingSeconds,
/// at least three frames and at most 4 MiB. Without a measurement yet (the first second) 4 MiB.
/// A fixed 4 MiB meant seconds of delay on a slow link (RTT 5 s at 2.3 MB/s, remote.log).
qint64 ServerSession::pendingLimit() const
{
    if (m_drainRate <= 0 || m_avgFrameBytes <= 0)
        return kMaxPendingBytes;
    const double limit = qMax(m_drainRate * kPendingSeconds, 3.0 * m_avgFrameBytes);
    return qint64(qMin(limit, double(kMaxPendingBytes)));
}

/// Counters of the last second (Claude Generated 2026). `pendingBytes` is what the socket still
/// holds, so a link that cannot keep up shows as a pending figure that grows towards the 4 MiB
/// drop limit; `produced` against `sent` + `dropped` shows what the worker delivered.
void ServerSession::sendLinkStats()
{
    if (!m_socket || !m_authenticated)
        return;
    const double ms = qMax<qint64>(1, m_statsClock.restart());
    {
        const double rate = double(m_drainBytesIv) / (ms / 1000.0);
        m_drainRate = m_drainRate > 0 ? 0.5 * m_drainRate + 0.5 * rate : rate;
        m_drainBytesIv = 0;
    }
    const double gaps = qMax<quint64>(1, m_ivProduced);
    const double mean = m_ivGapSum / gaps;
    const double sd = std::sqrt(qMax(0.0, m_ivGapSq / gaps - mean * mean));
    send({ { "type", "linkStats" }, { "intervalMs", ms }, { "produced", double(m_ivProduced) },
        { "sent", double(m_ivSent) }, { "dropped", double(m_ivDropped) }, { "bytes", double(m_ivBytes) },
        { "encodeUsPerFrame", m_ivSent ? double(m_ivEncodeNs) / 1e3 / double(m_ivSent) : 0.0 },
        { "pendingBytes", double(m_socket->bytesToWrite()) }, { "maxPendingBytes", double(m_ivMaxPending) },
        { "step", m_lastStep }, { "gapMeanMs", mean }, { "gapSdMs", sd }, { "gapMaxMs", m_ivGapMax },
        { "slowSteps", m_ivSlow }, { "slowList", m_ivSlowList },
        { "thinned", double(m_ivThinned) }, { "drainBytesPerSec", m_drainRate }, { "pendingLimitBytes", double(pendingLimit()) } });
    m_ivThinned = 0;
    m_ivGapSum = m_ivGapSq = m_ivGapMax = 0;
    m_ivSlow = 0;
    m_ivSlowList = QJsonArray();
    m_ivProduced = m_ivSent = m_ivDropped = m_ivBytes = 0;
    m_ivEncodeNs = m_ivMaxPending = 0;
}

void ServerSession::stopRun()
{
    if (!m_backend)
        return;
    m_backend->requestStop();
    delete m_backend;  // LocalBackend waits for its worker thread
    m_backend = nullptr;
}

QJsonObject ServerSession::sessionFilesJson() const
{
    // Files of the session directory except the uploads in `in/`, hidden ones included: the
    // worker's molecule has no basename, so curcuma writes .snapshots/.trj.xyz. Names are
    // relative to the session directory.
    QJsonArray files;
    QDirIterator it(m_sessionDir, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    const QDir base(m_sessionDir);
    constexpr int kMaxFiles = 200;
    while (it.hasNext() && files.size() < kMaxFiles) {
        it.next();
        const QString rel = base.relativeFilePath(it.filePath());
        if (rel.startsWith(QStringLiteral("in/")))
            continue;  // the client's own uploads
        files.append(QJsonObject{ { "name", rel }, { "size", double(it.fileInfo().size()) } });
    }
    return QJsonObject{ { "files", files } };
}

void ServerSession::handleList(const QJsonObject& msg)
{
    const QString requested = msg.value("path").toString();
    QString err;
    const QString dir = resolveBrowsePath(requested, m_browseRoots, &err);
    QJsonObject reply{ { "type", "listing" }, { "request", requested } };
    if (dir.isEmpty()) {
        reply["error"] = err;
        send(reply);
        return;
    }
    const QFileInfo fi(dir);
    if (!fi.isDir()) {
        reply["error"] = QStringLiteral("not a directory");
        send(reply);
        return;
    }
    constexpr int kMaxEntries = 5000;
    QJsonArray entries;
    const QFileInfoList list = QDir(dir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo& e : list) {
        if (entries.size() >= kMaxEntries)
            break;
        entries.append(QJsonObject{ { "name", e.fileName() }, { "dir", e.isDir() }, { "size", double(e.size()) },
            { "mtime", double(e.lastModified().toSecsSinceEpoch()) } });
    }
    QJsonArray roots;
    for (const QString& r : m_browseRoots)
        roots.append(QFileInfo(r).canonicalFilePath());
    reply["path"] = dir;
    reply["entries"] = entries;
    reply["truncated"] = list.size() > kMaxEntries;
    reply["roots"] = roots;
    send(reply);
}

void ServerSession::handleGetFile(const QJsonObject& msg)
{
    const quint32 id = quint32(msg.value("id").toDouble());
    auto fail = [&](const QString& why) {
        send({ { "type", "fileError" }, { "id", double(id) }, { "message", why } });
    };
    if (m_transferFile.isOpen())
        return fail(QStringLiteral("another download is in progress"));
    QString err;
    const QString path = resolveBrowsePath(msg.value("path").toString(), m_browseRoots, &err);
    if (path.isEmpty())
        return fail(err);
    if (!QFileInfo(path).isFile())
        return fail(QStringLiteral("not a regular file"));
    m_transferFile.setFileName(path);
    if (!m_transferFile.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("cannot read the file"));
    m_transferId = id;
    m_transferHash.reset();
    send({ { "type", "fileBegin" }, { "id", double(id) }, { "name", QFileInfo(path).fileName() },
        { "size", double(m_transferFile.size()) } });
    pumpTransfer();
}

void ServerSession::pumpTransfer()
{
    // Chunks are sent while the socket has room, so a slow link never makes the server
    // hold the whole file in memory.
    while (m_transferFile.isOpen() && m_socket && m_socket->isValid() && m_socket->bytesToWrite() < 4 * 1024 * 1024) {
        const QByteArray chunk = m_transferFile.read(kChunkBytes);
        if (chunk.isEmpty()) {
            send({ { "type", "fileEnd" }, { "id", double(m_transferId) },
                { "sha256", QString::fromLatin1(m_transferHash.result().toHex()) } });
            finishTransfer(false);
            return;
        }
        m_transferHash.addData(chunk);
        m_socket->sendBinaryMessage(encodeChunk(m_transferId, chunk));
    }
}

void ServerSession::finishTransfer(bool cancelled)
{
    if (cancelled)
        send({ { "type", "fileError" }, { "id", double(m_transferId) }, { "message", QStringLiteral("cancelled") } });
    m_transferFile.close();
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
    m_transferFile.close();
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
    m_statsTimer.stop();
    stopRun();
    emit ended();
}

// ---- RemoteServer -------------------------------------------------------------------

RemoteServer::RemoteServer(const QString& token, const QString& rootDir, int graceSeconds,
    const QStringList& browseRoots, QObject* parent)
    : QObject(parent)
    , m_server(QStringLiteral("qurcuma-server"), QWebSocketServer::NonSecureMode)
    , m_token(token)
    , m_root(rootDir)
    , m_graceSeconds(graceSeconds)
    , m_browseRoots(browseRoots)
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
    m_active = new ServerSession(m_token, m_root, m_graceSeconds, m_browseRoots, this);
    connect(m_active, &ServerSession::ended, this, [this] {
        m_active->deleteLater();
        m_active = nullptr;
        emit sessionEnded();
    });
    m_active->bindSocket(socket);
}

} // namespace remote
