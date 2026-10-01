// sshtunnel.h - Start qurcuma-server on a remote host and tunnel to it over ssh
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (WP remote compute R2, docs/WP-remote-compute-vr.md).
// Two ssh processes, using the user's ~/.ssh/config (keys, ProxyJump, ...), always with
// BatchMode (no password prompts; use a key or an agent):
//   1. ssh <host> '<server command> --port 0 --token-stdin --once'
//      The token goes through stdin (not onto the remote command line, where other users
//      could read it); the server answers "listening <port>" on stdout.
//   2. ssh -N -L <local>:127.0.0.1:<port> <host>
// The ssh binary can be replaced with the environment variable QURCUMA_SSH (tests).

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace remote {

class SshTunnel : public QObject {
    Q_OBJECT
public:
    /// @p serverCommand is run by the remote shell, e.g. "qurcuma-server" or
    /// "/opt/qurcuma/bin/qurcuma-server --root /scratch/me".
    SshTunnel(const QString& host, const QString& serverCommand, QObject* parent = nullptr);
    ~SshTunnel() override;

    void start();
    /// Ends both ssh processes.
    void close();
    /// Starts the port forward again if its ssh process has ended (network change, sleep);
    /// the server process and its ports are unchanged. No-op while the forward is running.
    void restartForward();

    QString host() const { return m_host; }

    /// Arguments of the two ssh invocations (also used by the tests).
    static QStringList serverArgs(const QString& host, const QString& serverCommand);
    static QStringList forwardArgs(const QString& host, quint16 localPort, quint16 remotePort);
    static QString sshProgram();

signals:
    /// The forward is started; the first connection may need a retry until ssh listens.
    void ready(quint16 localPort, const QString& token);
    void failed(const QString& message);

private:
    void onServerOutput();
    void fail(const QString& message);
    void startForward();

    QString m_host, m_serverCommand, m_token;
    QProcess* m_server = nullptr;
    QProcess* m_forward = nullptr;
    QTimer m_timeout;
    QByteArray m_outBuf;
    QString m_stderr;
    bool m_done = false;
    quint16 m_localPort = 0, m_remotePort = 0;
};

} // namespace remote
