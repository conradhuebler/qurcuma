// sshtunnel.cpp - Start qurcuma-server on a remote host and tunnel to it over ssh
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R2)

#include "sshtunnel.h"

#include <QRegularExpression>
#include <QTcpServer>
#include <QUuid>

namespace remote {

namespace {
constexpr int kStartTimeoutMs = 30000;

quint16 freeLocalPort()
{
    QTcpServer probe;
    return probe.listen(QHostAddress::LocalHost, 0) ? probe.serverPort() : quint16(0);
}

const QStringList kCommonOptions = { QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
    QStringLiteral("-o"), QStringLiteral("ServerAliveInterval=15") };
}

QString SshTunnel::sshProgram()
{
    const QString env = qEnvironmentVariable("QURCUMA_SSH");
    return env.isEmpty() ? QStringLiteral("ssh") : env;
}

QStringList SshTunnel::serverArgs(const QString& host, const QString& serverCommand)
{
    return kCommonOptions + QStringList{ host,
        serverCommand + QStringLiteral(" --port 0 --token-stdin --once") };
}

QStringList SshTunnel::forwardArgs(const QString& host, quint16 localPort, quint16 remotePort)
{
    return kCommonOptions + QStringList{ QStringLiteral("-o"), QStringLiteral("ExitOnForwardFailure=yes"),
        QStringLiteral("-N"), QStringLiteral("-L"),
        QStringLiteral("%1:127.0.0.1:%2").arg(localPort).arg(remotePort), host };
}

SshTunnel::SshTunnel(const QString& host, const QString& serverCommand, QObject* parent)
    : QObject(parent)
    , m_host(host)
    , m_serverCommand(serverCommand.trimmed().isEmpty() ? QStringLiteral("qurcuma-server") : serverCommand.trimmed())
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        fail(tr("No answer from qurcuma-server on %1 after %2 s.").arg(m_host).arg(kStartTimeoutMs / 1000));
    });
}

SshTunnel::~SshTunnel()
{
    close();
}

void SshTunnel::start()
{
    // 32 hex characters from two random UUIDs (QUuid uses the system random source).
    m_token = QUuid::createUuid().toString(QUuid::Id128) + QUuid::createUuid().toString(QUuid::Id128);
    m_token.truncate(48);

    m_server = new QProcess(this);
    connect(m_server, &QProcess::readyReadStandardOutput, this, &SshTunnel::onServerOutput);
    connect(m_server, &QProcess::readyReadStandardError, this, [this] {
        const QString text = QString::fromUtf8(m_server->readAllStandardError());
        m_stderr = (m_stderr + text).right(2000);
        for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            emit log(tr("remote side: %1").arg(line));
    });
    connect(m_server, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            fail(tr("Could not start '%1'.").arg(sshProgram()));
    });
    connect(m_server, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        if (!m_done)
            fail(tr("ssh to %1 ended before the server was ready (exit %2). %3").arg(m_host).arg(code)
                     .arg(m_stderr.trimmed().isEmpty() ? tr("ssh printed nothing; the server command may not exist or may have exited.")
                                                      : m_stderr.trimmed()));
    });
    connect(m_server, &QProcess::started, this, [this] { m_server->write((m_token + QLatin1Char('\n')).toUtf8()); });
    emit log(tr("running: %1 %2").arg(sshProgram(), serverArgs(m_host, m_serverCommand).join(QLatin1Char(' '))));
    m_server->start(sshProgram(), serverArgs(m_host, m_serverCommand));
    m_timeout.start(kStartTimeoutMs);
}

void SshTunnel::onServerOutput()
{
    const QByteArray fresh = m_server->readAllStandardOutput();
    for (const QByteArray& line : fresh.split('\n'))
        if (!line.trimmed().isEmpty())
            emit log(tr("server says: %1").arg(QString::fromUtf8(line.trimmed())));
    m_outBuf += fresh;
    if (m_forward || m_done)
        return;
    static const QRegularExpression re(QStringLiteral("^listening (\\d+)$"), QRegularExpression::MultilineOption);
    const auto m = re.match(QString::fromUtf8(m_outBuf));
    if (!m.hasMatch())
        return;
    m_remotePort = quint16(m.captured(1).toUInt());
    m_localPort = freeLocalPort();
    if (m_localPort == 0) {
        fail(tr("No free local port."));
        return;
    }
    startForward();
    m_timeout.stop();
    m_done = true;
    emit ready(m_localPort, m_token);
}

void SshTunnel::startForward()
{
    delete m_forward;
    m_forward = new QProcess(this);
    connect(m_forward, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        if (!m_done)
            fail(tr("The ssh port forward to %1 ended (exit %2).").arg(m_host).arg(code));
    });
    connect(m_forward, &QProcess::readyReadStandardError, this, [this] {
        for (const QString& line : QString::fromUtf8(m_forward->readAllStandardError()).split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            emit log(tr("forward: %1").arg(line));
    });
    emit log(tr("running: %1 %2").arg(sshProgram(), forwardArgs(m_host, m_localPort, m_remotePort).join(QLatin1Char(' '))));
    m_forward->start(sshProgram(), forwardArgs(m_host, m_localPort, m_remotePort));
}

void SshTunnel::restartForward()
{
    if (!m_done || m_localPort == 0 || (m_forward && m_forward->state() != QProcess::NotRunning))
        return;
    startForward();
}

void SshTunnel::fail(const QString& message)
{
    if (m_done)
        return;
    m_done = true;
    m_timeout.stop();
    close();
    emit failed(message);
}

void SshTunnel::close()
{
    for (QProcess* p : { m_forward, m_server }) {
        if (p && p->state() != QProcess::NotRunning) {
            p->disconnect(this);  // no failed() from our own shutdown
            p->terminate();
            if (!p->waitForFinished(1500))
                p->kill();
        }
    }
}

} // namespace remote
