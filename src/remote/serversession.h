// serversession.h - One client connection of qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (WP remote compute R1, docs/WP-remote-compute-vr.md).
// Speaks the protocol of protocol.h on one QWebSocket and drives a SimulationBackend
// (LocalBackend: SimulationWorker in a thread of the server process). One session, one run.
//
// The session directory <root>/<name>/{in,out} becomes the process working directory:
// uploads land in in/, curcuma's outputs and caches in the CWD, and no stale `stop` file
// from another run can exist there. The working directory is process-wide, so a server
// process serves one session at a time.

#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class QWebSocket;
class SimulationBackend;

namespace remote {

class ServerSession : public QObject {
    Q_OBJECT
public:
    /// Takes over @p socket (reparented). @p token must match the client's hello.
    ServerSession(QWebSocket* socket, const QString& token, const QString& rootDir, QObject* parent = nullptr);
    ~ServerSession() override;

    QString sessionDir() const { return m_sessionDir; }

signals:
    void ended();

private slots:
    void onText(const QString& message);
    void onBinary(const QByteArray& message);
    void onDisconnected();

private:
    void send(const QJsonObject& obj);
    void sendError(const QString& message);
    void handleStart(const QJsonObject& msg);
    void stopRun();

    QWebSocket* m_socket = nullptr;
    QString m_token;
    QString m_sessionDir;
    bool m_authenticated = false;
    bool m_ended = false;
    SimulationBackend* m_backend = nullptr;
    QSet<QString> m_uploaded;
    qint64 m_uploadedBytes = 0;
    quint64 m_framesSent = 0;
    quint64 m_framesDropped = 0;
};

} // namespace remote
