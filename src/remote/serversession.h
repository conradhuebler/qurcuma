// serversession.h - Sessions and listener of qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (WP remote compute R1/R3, docs/WP-remote-compute-vr.md).
// ServerSession speaks the protocol of protocol.h on a QWebSocket and drives a
// SimulationBackend (LocalBackend: SimulationWorker in a thread of the server process).
// One session, one run.
//
// Connection loss: a run is NOT stopped when the client disappears. The sticky grab force
// is cleared at once, the run goes on (frames are not sent), and the session waits for the
// client to come back (hello with attach=true and the session name) for the grace period;
// after that the run is stopped and the session ends. A run that finishes while no client is
// attached keeps its "finished" message for the client that returns.
//
// The session directory <root>/<name>/{in,out} becomes the process working directory:
// uploads land in in/, curcuma's outputs and caches in the CWD, and no stale `stop` file
// from another run can exist there. The working directory is process-wide, so a server
// process serves one session at a time.

#pragma once

#include <QCryptographicHash>
#include <QFile>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWebSocketServer>

class QWebSocket;
class SimulationBackend;

namespace remote {

class ServerSession : public QObject {
    Q_OBJECT
public:
    /// @p browseRoots: directories a client may list and download from (the session root is
    /// always included).
    ServerSession(const QString& token, const QString& rootDir, int graceSeconds,
        const QStringList& browseRoots = {}, QObject* parent = nullptr);
    ~ServerSession() override;

    /// Take a freshly accepted connection (it still has to authenticate with hello).
    void bindSocket(QWebSocket* socket);
    /// True while no client is connected and the session is not over.
    bool canAttach() const { return !m_socket && !m_ended; }
    QString sessionDir() const { return m_sessionDir; }
    QString sessionName() const;
    bool runActive() const { return m_backend != nullptr; }
    /// Close the client connection from the server side (tests, shutdown).
    void closeClientConnection();

signals:
    void ended();

private slots:
    void onText(const QString& message);
    void onBinary(const QByteArray& message);
    void onDisconnected();

private:
    void send(const QJsonObject& obj);
    void sendError(const QString& message);
    void handleHello(const QJsonObject& msg);
    void handleStart(const QJsonObject& msg);
    void stopRun();
    void endSession();
    void handleList(const QJsonObject& msg);
    void handleGetFile(const QJsonObject& msg);
    void pumpTransfer();
    void finishTransfer(bool cancelled);
    QJsonObject sessionFilesJson() const;

    QWebSocket* m_socket = nullptr;
    QString m_token;
    QString m_sessionDir;
    int m_graceMs = 60000;
    QTimer m_graceTimer;  // waiting for a first client, or for the client to come back
    bool m_authenticated = false;
    bool m_everAuthenticated = false;
    bool m_ended = false;
    SimulationBackend* m_backend = nullptr;
    QStringList m_browseRoots;
    QFile m_transferFile;           // the file being downloaded by the client, if any
    quint32 m_transferId = 0;
    QCryptographicHash m_transferHash{ QCryptographicHash::Sha256 };
    QJsonObject m_finishedMsg;  // kept until a client has been told
    QSet<QString> m_uploaded;
    qint64 m_uploadedBytes = 0;
    quint64 m_framesSent = 0;
    quint64 m_framesDropped = 0;
};

/// Listener plus the one-session policy of qurcuma-server.
class RemoteServer : public QObject {
    Q_OBJECT
public:
    RemoteServer(const QString& token, const QString& rootDir, int graceSeconds,
        const QStringList& browseRoots = {}, QObject* parent = nullptr);

    bool listen(quint16 port = 0);  ///< 127.0.0.1 only; 0 = any free port
    quint16 port() const;
    QString errorString() const;
    ServerSession* activeSession() const { return m_active; }
    /// Stop/resume accepting new connections (tests: a client that cannot reconnect).
    void setAccepting(bool on);

signals:
    void sessionEnded();

private:
    void onNewConnection();

    QWebSocketServer m_server;
    QString m_token, m_root;
    int m_graceSeconds;
    QStringList m_browseRoots;
    ServerSession* m_active = nullptr;
};

} // namespace remote
