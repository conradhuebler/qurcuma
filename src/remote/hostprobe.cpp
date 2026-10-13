// hostprobe.cpp - see hostprobe.h
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute)

#include "hostprobe.h"

#include "sshtunnel.h"

#include <functional>

namespace remote {

namespace {
constexpr int kStepTimeoutMs = 20000;
}

QByteArray HostProbe::findServerScript()
{
    // Fixed text, no value from this program is put into it. Candidate places: the PATH of
    // the ssh session, then where a user build usually ends up.
    return QByteArrayLiteral(
        "for c in \"$(command -v qurcuma-server 2>/dev/null)\" \"$HOME/.local/bin/qurcuma-server\" "
        "\"$HOME/bin/qurcuma-server\" \"$HOME/src/qurcuma/release/qurcuma-server\" "
        "\"$HOME/src/qurcuma/build/qurcuma-server\" \"$HOME/src/qurcuma/debug/qurcuma-server\" "
        "\"$HOME/qurcuma/release/qurcuma-server\" /usr/local/bin/qurcuma-server /opt/qurcuma/bin/qurcuma-server; do\n"
        "  if [ -n \"$c\" ] && [ -x \"$c\" ]; then\n"
        "    echo \"SERVER=$c\"\n"
        "    if \"$c\" --help >/dev/null 2>&1; then echo RUNS=1; else echo RUNS=0; fi\n"
        "    exit 0\n"
        "  fi\n"
        "done\n"
        "echo SERVER=\n");
}

QString HostProbe::explainSshFailure(const QString& err, const HostProfile& p)
{
    const QString text = err.trimmed();
    if (text.contains(QStringLiteral("Permission denied")))
        return QObject::tr("ssh needs to log in without a password prompt (a key or the ssh agent). Set that up once in a terminal, "
                           "for example with: ssh-copy-id %1").arg(p.target);
    if (text.contains(QStringLiteral("Host key verification failed")) || text.contains(QStringLiteral("REMOTE HOST IDENTIFICATION")))
        return QObject::tr("ssh does not know this computer yet (or its key changed). Connect once in a terminal to check and accept the host key: ssh %1").arg(p.target);
    if (text.contains(QStringLiteral("Could not resolve hostname")) || text.contains(QStringLiteral("Name or service not known")))
        return QObject::tr("The address '%1' cannot be resolved. Check the spelling, or use the IP address.").arg(p.target);
    if (text.contains(QStringLiteral("Connection refused")))
        return QObject::tr("The computer refuses ssh connections on port %1. Is the ssh server running, and is the port right?").arg(p.port > 0 ? p.port : 22);
    if (text.contains(QStringLiteral("timed out")) || text.contains(QStringLiteral("No route to host")))
        return QObject::tr("No answer from %1 (network, firewall or VPN?).").arg(p.target);
    if (text.contains(QStringLiteral("identity file")) && text.contains(QStringLiteral("not accessible")))
        return QObject::tr("The key file cannot be read: %1").arg(p.identityFile);
    return text.isEmpty() ? QObject::tr("ssh failed without a message.") : text.section(QLatin1Char('\n'), 0, 2);
}

HostProbe::HostProbe(const HostProfile& profile, QObject* parent)
    : QObject(parent)
    , m_profile(profile)
{
    m_timeout.setSingleShot(true);
}

HostProbe::~HostProbe()
{
    cancel();
}

void HostProbe::cancel()
{
    m_done = true;
    m_timeout.stop();
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(500);
        delete m_process;
        m_process = nullptr;
    }
}

void HostProbe::finishWith(bool sshOk, const QString& path, bool runs, const QString& message)
{
    if (m_done)
        return;
    m_done = true;
    m_timeout.stop();
    emit finished(sshOk, path, runs, message);
}

void HostProbe::runStep(const QStringList& command, const QByteArray& input,
    std::function<void(int, const QString&, const QString&)> done)
{
    QString why;
    if (!m_profile.isValid(&why)) {
        finishWith(false, {}, false, why);
        return;
    }
    // Same options as the real connection (BatchMode: no prompt can hang the test), plus a
    // connect timeout.
    QStringList args = { QStringLiteral("-o"), QStringLiteral("BatchMode=yes"), QStringLiteral("-o"), QStringLiteral("ConnectTimeout=8") };
    args += m_profile.sshArgs();
    args << m_profile.target;
    args += command;

    m_process = new QProcess(this);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finishWith(false, {}, false, tr("Could not start '%1'. Is the ssh client installed?").arg(SshTunnel::sshProgram()));
    });
    connect(m_process, &QProcess::finished, this, [this, done](int code, QProcess::ExitStatus) {
        if (m_done)
            return;
        const QString out = QString::fromUtf8(m_process->readAllStandardOutput());
        const QString err = QString::fromUtf8(m_process->readAllStandardError());
        m_process->deleteLater();
        m_process = nullptr;
        done(code, out, err);
    });
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_process)
            m_process->kill();
        finishWith(false, {}, false, tr("No answer from %1 within %2 s.").arg(m_profile.target).arg(kStepTimeoutMs / 1000));
    });
    m_timeout.start(kStepTimeoutMs);
    m_process->start(SshTunnel::sshProgram(), args);
    if (!input.isEmpty())
        m_process->write(input);
    m_process->closeWriteChannel();
}

void HostProbe::start()
{
    m_done = false;
    emit progress(tr("Logging in to %1 ...").arg(m_profile.target));
    runStep({ QStringLiteral("echo qurcuma-ok") }, {}, [this](int code, const QString& out, const QString& err) {
        if (code != 0 || !out.contains(QStringLiteral("qurcuma-ok"))) {
            finishWith(false, {}, false, explainSshFailure(err, m_profile));
            return;
        }
        emit progress(tr("Logged in. Looking for qurcuma-server ..."));
        runStep({ QStringLiteral("sh -s") }, findServerScript(), [this](int code2, const QString& out2, const QString& err2) {
            if (code2 != 0) {
                finishWith(true, {}, false, tr("ssh works, but the search for qurcuma-server failed: %1").arg(err2.trimmed()));
                return;
            }
            QString path;
            bool runs = false;
            for (const QString& line : out2.split(QLatin1Char('\n'))) {
                if (line.startsWith(QStringLiteral("SERVER=")))
                    path = line.mid(7).trimmed();
                else if (line.trimmed() == QStringLiteral("RUNS=1"))
                    runs = true;
            }
            if (path.isEmpty())
                finishWith(true, {}, false, tr("ssh works, but qurcuma-server was not found on %1. Build it there "
                                               "(cmake --build <dir> --target qurcuma-server) and enter its full path as the server command.")
                                                .arg(m_profile.target));
            else if (!runs)
                finishWith(true, path, false, tr("Found %1, but it does not start (missing libraries?). Try running it with --help on that computer.").arg(path));
            else
                finishWith(true, path, true, tr("Ready: qurcuma-server found at %1.").arg(path));
        });
    });
}

} // namespace remote
