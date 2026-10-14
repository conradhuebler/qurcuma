// hostprobe.cpp - see hostprobe.h
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute)

#include "hostprobe.h"

#include "protocol.h"
#include "sshtunnel.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QWebSocket>

namespace remote {

namespace {
constexpr int kStepTimeoutMs = 20000;
constexpr int kSessionTimeoutMs = 45000;
constexpr int kMaxConnectAttempts = 25;  // the forward needs a moment before it listens
}

QByteArray HostProbe::findServerScript(const QString& typedCommand)
{
    QByteArray check;
    const QString word = typedCommand.trimmed().section(QLatin1Char(' '), 0, 0);
    static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_./~+@-]{1,300}$"));
    if (!word.isEmpty() && safe.match(word).hasMatch()) {
        check = "tok='" + word.toUtf8() + "'\n"
                "case \"$tok\" in \"~/\"*) tok=\"$HOME/${tok#\"~/\"}\";; esac\n"
                "if [ -d \"$tok\" ]; then echo CUSTOM=dir\n"
                "elif [ -x \"$tok\" ] || command -v \"$tok\" >/dev/null 2>&1; then echo CUSTOM=ok\n"
                "else echo CUSTOM=missing; fi\n";
    }
    // Fixed text, no value from this program is put into it (the typed word above passed the
    // character check). Candidate places: the PATH of the ssh session, then where a user build
    // usually ends up.
    return check + QByteArrayLiteral(
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
    qRegisterMetaType<remote::ProbeResult>();
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
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        delete m_socket;
        m_socket = nullptr;
    }
    if (m_tunnel) {
        m_tunnel->disconnect(this);
        m_tunnel->close();
        delete m_tunnel;
        m_tunnel = nullptr;
    }
}

void HostProbe::setStep(Step step, State state, const QString& text)
{
    if (state == Running)
        m_current = step;
    if (!m_done)
        emit stepChanged(int(step), int(state), text);
}

void HostProbe::finishWith(const QString& message)
{
    if (m_done)
        return;
    m_timeout.stop();
    m_result.message = message;
    m_done = true;
    emit finished(m_result);
}

void HostProbe::runStep(const QStringList& command, const QByteArray& input,
    std::function<void(int, const QString&, const QString&)> done)
{
    // Same options as the real connection (BatchMode: no prompt can hang the test), plus a
    // connect timeout.
    QStringList args = { QStringLiteral("-o"), QStringLiteral("BatchMode=yes"), QStringLiteral("-o"), QStringLiteral("ConnectTimeout=8") };
    args += m_profile.sshArgs();
    args << m_profile.target;
    args += command;

    m_process = new QProcess(this);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            failSession(tr("Could not start '%1'. Is the ssh client installed?").arg(SshTunnel::sshProgram()));
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
    m_timeout.disconnect(this);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_process)
            m_process->kill();
        failSession(tr("No answer from %1 within %2 s.").arg(m_profile.target).arg(kStepTimeoutMs / 1000));
    });
    m_timeout.start(kStepTimeoutMs);
    m_process->start(SshTunnel::sshProgram(), args);
    if (!input.isEmpty())
        m_process->write(input);
    m_process->closeWriteChannel();
}

/// Marks the step that is running as failed and ends the test; the steps after it stay pending.
void HostProbe::failSession(const QString& why)
{
    if (m_done)
        return;
    const Step running = m_current;  // the step that was reported as running
    setStep(running, Failed, why);
    for (int s = running + 1; s <= Session; ++s)
        setStep(Step(s), Skipped, tr("not tried"));
    finishWith(why);
}

void HostProbe::start()
{
    m_done = false;
    m_current = Login;
    m_result = {};
    QString why;
    if (!m_profile.isValid(&why)) {
        setStep(Login, Failed, why);
        finishWith(why);
        return;
    }
    setStep(Login, Running, tr("Logging in to %1 ...").arg(m_profile.target));
    runStep({ QStringLiteral("echo qurcuma-ok") }, {}, [this](int code, const QString& out, const QString& err) {
        if (code != 0 || !out.contains(QStringLiteral("qurcuma-ok"))) {
            failSession(explainSshFailure(err, m_profile));
            return;
        }
        m_result.sshOk = true;
        setStep(Login, Ok, tr("ssh logs in without a prompt."));
        setStep(Server, Running, tr("Looking for qurcuma-server ..."));
        runStep({ QStringLiteral("sh -s") }, findServerScript(m_profile.serverCommand), [this](int code2, const QString& out2, const QString& err2) {
            if (code2 != 0) {
                failSession(tr("The search for qurcuma-server failed: %1").arg(err2.trimmed()));
                return;
            }
            bool runs = false;
            QString custom_state;
            for (const QString& line : out2.split(QLatin1Char('\n'))) {
                if (line.startsWith(QStringLiteral("SERVER=")))
                    m_result.serverPath = line.mid(7).trimmed();
                else if (line.trimmed() == QStringLiteral("RUNS=1"))
                    runs = true;
                else if (line.startsWith(QStringLiteral("CUSTOM=")))
                    custom_state = line.mid(7).trimmed();
            }
            m_result.serverRuns = runs;
            // A server command typed by the user wins over the one found.
            const QString custom = m_profile.serverCommand.trimmed();
            // The command the user typed is checked first: a wrong one would otherwise only show up
            // as a failed session. If the search found a server, its path is offered.
            if (!custom.isEmpty() && (custom_state == QStringLiteral("missing") || custom_state == QStringLiteral("dir"))) {
                const QString word = custom.section(QLatin1Char(' '), 0, 0);
                QString msg = custom_state == QStringLiteral("dir")
                    ? tr("'%1' is a directory on %2; the server command needs the path of the qurcuma-server file.").arg(word, m_profile.target)
                    : tr("'%1' does not exist on %2.").arg(word, m_profile.target);
                if (!m_result.serverPath.isEmpty())
                    msg += tr(" qurcuma-server was found at %1: clear the field to use it.").arg(m_result.serverPath);
                else
                    msg += tr(" qurcuma-server was not found anywhere on that computer; build it there (cmake --build <dir> --target qurcuma-server).");
                failSession(msg);
                return;
            }
            if (m_result.serverPath.isEmpty() && custom.isEmpty()) {
                failSession(tr("qurcuma-server was not found on %1. Build it there (cmake --build <dir> --target qurcuma-server) "
                               "and enter its full path as the server command.").arg(m_profile.target));
                return;
            }
            if (!m_result.serverPath.isEmpty() && !runs && custom.isEmpty()) {
                failSession(tr("Found %1, but it does not start (missing libraries?). Try it with --help on that computer.").arg(m_result.serverPath));
                return;
            }
            setStep(Server, Ok, !custom.isEmpty() ? tr("The server command you entered exists.")
                                                  : tr("Found %1.").arg(m_result.serverPath));
            runSession(custom.isEmpty() ? m_result.serverPath : custom);
        });
    });
}

/// Step 3: the same way a run starts it (ssh, server with a token on stdin, port forward), then
/// hello -> welcome and one ping. A session that ends with the client leaves no process behind:
/// the server ends it when an authenticated client leaves without a run.
void HostProbe::runSession(const QString& serverCommand)
{
    setStep(Session, Running, tr("Starting qurcuma-server and opening a session ..."));
    HostProfile p = m_profile;
    p.serverCommand = serverCommand;
    m_tunnel = new SshTunnel(p, this);
    connect(m_tunnel, &SshTunnel::failed, this, [this](const QString& msg) { failSession(msg); });
    connect(m_tunnel, &SshTunnel::ready, this, [this](quint16 port, const QString& token) {
        m_port = port;
        m_token = token;
        m_attempts = 0;
        connectSocket();
    });
    m_timeout.disconnect(this);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        failSession(tr("The server did not answer within %1 s.").arg(kSessionTimeoutMs / 1000));
    });
    m_timeout.start(kSessionTimeoutMs);
    m_tunnel->start();
}

void HostProbe::connectSocket()
{
    if (m_done)
        return;
    if (!m_socket) {
        m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        connect(m_socket, &QWebSocket::connected, this, [this] {
            const QJsonObject hello{ { "type", "hello" }, { "protocol", kProtocolVersion }, { "token", m_token } };
            m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(hello).toJson(QJsonDocument::Compact)));
        });
        connect(m_socket, &QWebSocket::textMessageReceived, this, &HostProbe::onSocketText);
        connect(m_socket, &QWebSocket::errorOccurred, this, [this] {
            if (m_welcomed || m_done)
                return;
            if (++m_attempts < kMaxConnectAttempts)
                QTimer::singleShot(300, this, &HostProbe::connectSocket);
            else
                failSession(tr("The connection through the ssh tunnel was not accepted: %1").arg(m_socket->errorString()));
        });
    }
    m_socket->open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(m_port)));
}

void HostProbe::onSocketText(const QString& text)
{
    const QJsonObject msg = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString type = msg.value("type").toString();
    if (type == QLatin1String("welcome")) {
        m_welcomed = true;
        m_result.threads = msg.value("threads").toInt();
        for (const QJsonValue& v : msg.value("gpuBackends").toArray())
            m_result.gpuBackends << v.toString();
        m_pingSentMs = QDateTime::currentMSecsSinceEpoch();
        m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{ { "type", "ping" }, { "t", double(m_pingSentMs) } })
                                                        .toJson(QJsonDocument::Compact)));
    } else if (type == QLatin1String("pong")) {
        m_result.rttMs = int(QDateTime::currentMSecsSinceEpoch() - m_pingSentMs);
        m_result.sessionOk = true;
        const QString gpu = m_result.gpuBackends.isEmpty() ? tr("no GPU backend") : tr("GPU: %1").arg(m_result.gpuBackends.join(QStringLiteral(", ")));
        const QString line = tr("Session works: %1 threads, %2, round trip %3 ms.").arg(m_result.threads).arg(gpu).arg(m_result.rttMs);
        setStep(Session, Ok, line);
        m_socket->close();  // the server ends the session when the client leaves
        finishWith(tr("Ready to use. %1").arg(line));
    } else if (type == QLatin1String("error")) {
        failSession(tr("The server refused the session: %1").arg(msg.value("message").toString()));
    }
}

} // namespace remote
