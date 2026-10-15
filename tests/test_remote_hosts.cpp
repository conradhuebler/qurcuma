// test_remote_hosts.cpp - computers added in qurcuma: profiles, editing, ssh arguments, the connection test
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute). The connection test (HostProbe) runs against a
// shell script standing in for ssh (QURCUMA_SSH): it drops the options like ssh does, fails for
// the target "denied", and otherwise runs the remote command locally. No real ssh or host.

#include "remote/hostprobe.h"
#include "remote/hostprofiles.h"
#include "remote/sshtunnel.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;               \
            ++failures;                                                                     \
        }                                                                                   \
    } while (0)

static void writeExecutable(const QString& path, const QByteArray& content)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(content);
        f.close();
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
            | QFileDevice::ExeGroup);
    }
}

struct Outcome {
    bool done = false;
    remote::ProbeResult result;
    QStringList steps;  ///< "<step>:<state>" in the order they were reported
};

static Outcome probe(const remote::HostProfile& p)
{
    Outcome out;
    remote::HostProbe probe(p);
    QEventLoop loop;
    QTimer limit;
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&probe, &remote::HostProbe::stepChanged, [&](int step, int state, const QString&) {
        out.steps << QStringLiteral("%1:%2").arg(step).arg(state);
    });
    QObject::connect(&probe, &remote::HostProbe::finished, [&](const remote::ProbeResult& r) {
        out.result = r;
        out.done = true;
        loop.quit();
    });
    limit.start(90000);
    probe.start();
    loop.exec();
    return out;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("qurcuma-test"));
    app.setApplicationName(QStringLiteral("test_remote_hosts"));
    QStandardPaths::setTestModeEnabled(true);
    QSettings().remove(QStringLiteral("remote/hosts"));

    // ---- validation: names and addresses end up as ssh arguments -------------------------
    {
        remote::HostProfile p;
        p.name = QStringLiteral("gpu box");
        p.target = QStringLiteral("conrad@gpu1.example.org");
        CHECK(p.isValid());
        p.target = QStringLiteral("192.0.2.10");
        CHECK(p.isValid());
        p.target = QStringLiteral("-oProxyCommand=touch /tmp/x");  // would be read by ssh as an option
        CHECK(!p.isValid());
        p.target = QStringLiteral("host name");
        CHECK(!p.isValid());
        p.target = QStringLiteral("host;rm");
        CHECK(!p.isValid());
        p.target = QStringLiteral("gpu1");
        p.name = QStringLiteral("-x");
        CHECK(!p.isValid());
        p.name = QStringLiteral("gpu1");
        p.port = 70000;
        CHECK(!p.isValid());
        p.port = 2222;
        p.identityFile = QStringLiteral("-F/etc/passwd");
        CHECK(!p.isValid());
        p.identityFile = QStringLiteral("/home/u/.ssh/id key");
        CHECK(p.isValid());
        CHECK((p.sshArgs() == QStringList{ "-p", "2222", "-i", "/home/u/.ssh/id key" }));
        p.port = 0;
        p.identityFile.clear();
        CHECK(p.sshArgs().isEmpty());
        CHECK(p.effectiveServerCommand() == "qurcuma-server");
    }

    // ---- storage ----------------------------------------------------------------------------
    {
        remote::HostProfile p;
        p.name = QStringLiteral("gpu1");
        p.target = QStringLiteral("conrad@gpu1.example.org");
        p.port = 2222;
        p.serverCommand = QStringLiteral("/opt/q/qurcuma-server --root /scratch");
        QString why;
        CHECK(remote::HostProfiles::add(p, &why));
        CHECK(!remote::HostProfiles::add(p, &why) && !why.isEmpty());  // the name is taken
        remote::HostProfile bad = p;
        bad.name = QStringLiteral("bad");
        bad.target = QStringLiteral("-oX");
        CHECK(!remote::HostProfiles::add(bad, &why));
        const auto found = remote::HostProfiles::find(QStringLiteral("gpu1"));
        CHECK(found && found->target == p.target && found->port == 2222 && found->serverCommand == p.serverCommand);
        CHECK(!remote::HostProfiles::find(QStringLiteral("nope")));
        CHECK(remote::HostProfiles::load().size() == 1);

        // ssh gets the address and the extra options for a saved name, and the plain name otherwise
        const QStringList a = remote::SshTunnel::serverArgs(p.target, "qurcuma-server", p.sshArgs());
        CHECK(a.indexOf("-p") >= 0 && a.indexOf("-p") < a.indexOf(p.target));
        CHECK(a.contains("BatchMode=yes") && a.contains(p.target));
        const QStringList plain = remote::SshTunnel::serverArgs("hpc", "qurcuma-server");
        CHECK(!plain.contains("-p") && plain.contains("hpc"));

        CHECK(remote::HostProfiles::remove(QStringLiteral("gpu1")));
        CHECK(!remote::HostProfiles::remove(QStringLiteral("gpu1")));
        CHECK(remote::HostProfiles::load().isEmpty());
    }

    // ---- the failure texts ----------------------------------------------------------------------
    {
        remote::HostProfile p;
        p.name = QStringLiteral("a");
        p.target = QStringLiteral("u@h");
        CHECK(remote::HostProbe::explainSshFailure("u@h: Permission denied (publickey).", p).contains("ssh-copy-id u@h"));
        CHECK(remote::HostProbe::explainSshFailure("Host key verification failed.", p).contains("terminal"));
        CHECK(remote::HostProbe::explainSshFailure("ssh: Could not resolve hostname h: Name or service not known", p).contains("cannot be resolved"));
        CHECK(remote::HostProbe::explainSshFailure("ssh: connect to host h port 22: Connection refused", p).contains("refuses"));
    }

    // ---- editing a saved computer: a rename keeps the settings kept per computer -------------
    {
        remote::HostProfile p;
        p.name = QStringLiteral("old");
        p.target = QStringLiteral("u@old.example.org");
        QString why;
        CHECK(remote::HostProfiles::add(p, &why));
        remote::HostProfile other = p;
        other.name = QStringLiteral("other");
        CHECK(remote::HostProfiles::add(other, &why));
        QSettings s;
        s.setValue(QStringLiteral("remote/serverCommand/old"), QStringLiteral("/opt/q/qurcuma-server"));
        s.setValue(QStringLiteral("remote/gpu/old"), QStringList{ "cuda" });
        s.setValue(QStringLiteral("remote/host"), QStringLiteral("old"));

        remote::HostProfile changed = p;
        changed.name = QStringLiteral("new");
        changed.port = 2200;
        CHECK(remote::HostProfiles::update(QStringLiteral("old"), changed, &why));
        CHECK(!remote::HostProfiles::find(QStringLiteral("old")));
        const auto f = remote::HostProfiles::find(QStringLiteral("new"));
        CHECK(f && f->port == 2200);
        QSettings s2;
        CHECK(s2.value(QStringLiteral("remote/serverCommand/new")).toString() == "/opt/q/qurcuma-server");
        CHECK(!s2.contains(QStringLiteral("remote/serverCommand/old")));
        CHECK(s2.value(QStringLiteral("remote/gpu/new")).toStringList() == QStringList{ "cuda" });
        CHECK(s2.value(QStringLiteral("remote/host")).toString() == "new");
        // a name that another computer has, an invalid entry and a computer that is gone are refused
        changed.name = QStringLiteral("other");
        CHECK(!remote::HostProfiles::update(QStringLiteral("new"), changed, &why) && !why.isEmpty());
        changed.name = QStringLiteral("new");
        changed.target = QStringLiteral("-oX");
        CHECK(!remote::HostProfiles::update(QStringLiteral("new"), changed, &why));
        changed.target = QStringLiteral("u@h");
        CHECK(!remote::HostProfiles::update(QStringLiteral("gone"), changed, &why));
        // the same name with other values is an edit, not a duplicate
        changed.port = 22;
        CHECK(remote::HostProfiles::update(QStringLiteral("new"), changed, &why));
        remote::HostProfiles::remove(QStringLiteral("new"));
        remote::HostProfiles::remove(QStringLiteral("other"));
        s2.remove(QStringLiteral("remote/serverCommand/new"));
        s2.remove(QStringLiteral("remote/gpu/new"));
        s2.remove(QStringLiteral("remote/host"));
    }

    // ---- the connection test against a stand-in for ssh --------------------------------------
    // The stand-in drops the options like ssh does, fails for the target "denied", runs the remote
    // command locally and forwards the port of "-N -L local:127.0.0.1:remote". The server is the
    // qurcuma-server that was built next to this test: step 3 opens a real session with it.
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString fakeSsh = dir.path() + "/ssh";
    const QString log = dir.path() + "/ssh.log";
    writeExecutable(fakeSsh, R"PY(#!/usr/bin/env python3
import os, socket, sys, threading
args = sys.argv[1:]
with open(os.environ['QURCUMA_FAKE_LOG'], 'a') as f:
    f.write(' '.join(args) + '\n')
rest, i = [], 0
while i < len(args):
    if args[i] in ('-o', '-p', '-i'):
        i += 2
        continue
    rest.append(args[i]); i += 1
if rest and rest[0] == 'denied':
    sys.stderr.write('u@denied: Permission denied (publickey).\n'); sys.exit(255)
if '-N' in rest:
    lport, _, rport = rest[rest.index('-L') + 1].split(':')
    srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(('127.0.0.1', int(lport))); srv.listen(5)
    def pipe(a, b):
        try:
            while True:
                d = a.recv(65536)
                if not d: break
                b.sendall(d)
        except Exception: pass
        finally:
            try: b.shutdown(socket.SHUT_WR)
            except Exception: pass
    while True:
        c, _ = srv.accept(); u = socket.create_connection(('127.0.0.1', int(rport)))
        threading.Thread(target=pipe, args=(c, u), daemon=True).start()
        threading.Thread(target=pipe, args=(u, c), daemon=True).start()
else:
    os.execvp('/bin/sh', ['sh', '-c', rest[1]])
)PY");
    qputenv("QURCUMA_SSH", fakeSsh.toUtf8());
    qputenv("QURCUMA_FAKE_LOG", log.toUtf8());

    const QString emptyHome = dir.path() + "/home";
    QDir().mkpath(emptyHome);
    qputenv("HOME", emptyHome.toUtf8());
    const QByteArray basePath = "/usr/bin:/bin";
    qputenv("PATH", basePath);
    const QString realServer = QCoreApplication::applicationDirPath() + "/qurcuma-server";
    CHECK(QFile::exists(realServer));

    remote::HostProfile prof;
    prof.name = QStringLiteral("box");
    prof.target = QStringLiteral("box.example.org");
    prof.port = 2222;
    prof.identityFile = QStringLiteral("/k");

    {   // ssh works, no server anywhere: the first two steps are reported, the session is not tried
        const Outcome o = probe(prof);
        CHECK(o.done && o.result.sshOk && o.result.serverPath.isEmpty() && !o.result.sessionOk);
        CHECK(o.result.message.contains("not found"));
        CHECK(o.steps.contains("0:2") && o.steps.contains("1:3") && o.steps.contains("2:4"));  // login ok, server failed, session skipped
    }
    const QString bin = dir.path() + "/bin";
    QDir().mkpath(bin);
    {   // a server in the PATH: all three steps pass, with what the session told
        QFile::link(realServer, bin + "/qurcuma-server");
        qputenv("PATH", (bin.toUtf8() + ":" + basePath));
        const Outcome o = probe(prof);
        CHECK(o.done && o.result.sshOk && o.result.serverRuns && o.result.sessionOk);
        CHECK(o.result.serverPath == bin + "/qurcuma-server");
        CHECK(o.result.threads > 0 && o.result.rttMs >= 0);
        CHECK(o.result.message.contains("Ready to use"));
        const QStringList want{ "0:1", "0:2", "1:1", "1:2", "2:1", "2:2" };  // running, then ok, for each step in turn
        CHECK(o.steps == want);
        // the options of the profile reached ssh, before the target (login, search, server, forward)
        QFile f(log);
        CHECK(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        CHECK(text.count("-p 2222 -i /k box.example.org") >= 3);
        CHECK(text.contains("BatchMode=yes"));
    }
    {   // a server that starts with --help but does not listen: the session step fails with a message
        QFile::remove(bin + "/qurcuma-server");
        writeExecutable(bin + "/qurcuma-server", "#!/bin/sh\nexit 0\n");
        const Outcome o = probe(prof);
        CHECK(o.done && o.result.sshOk && o.result.serverRuns && !o.result.sessionOk);
        CHECK(o.steps.contains("2:3") && !o.result.message.isEmpty());
    }
    {   // a server that does not start (--help fails) and no command typed: stops at step 2
        writeExecutable(bin + "/qurcuma-server", "#!/bin/sh\nexit 1\n");
        const Outcome o = probe(prof);
        CHECK(o.done && o.result.sshOk && !o.result.serverRuns && !o.result.sessionOk);
        CHECK(o.result.message.contains("does not start") && o.steps.contains("1:3") && o.steps.contains("2:4"));
    }
    {   // a server in a place of the fixed list ($HOME/.local/bin), not in the PATH
        qputenv("PATH", basePath);
        QDir().mkpath(emptyHome + "/.local/bin");
        QFile::link(realServer, emptyHome + "/.local/bin/qurcuma-server");
        const Outcome o = probe(prof);
        CHECK(o.done && o.result.sessionOk && o.result.serverPath == emptyHome + "/.local/bin/qurcuma-server");
    }
    {   // a command typed by the user is used even where nothing is found
        QFile::remove(emptyHome + "/.local/bin/qurcuma-server");
        remote::HostProfile typed = prof;
        typed.serverCommand = realServer;
        const Outcome o = probe(typed);
        CHECK(o.done && o.result.sessionOk);
    }
    // a typed server command is checked on the remote side before the session is tried
    QFile::remove(bin + "/qurcuma-server");
    QFile::link(realServer, bin + "/qurcuma-server");
    qputenv("PATH", (bin.toUtf8() + ":" + basePath));
    {   // it does not exist (a wrong path): step 2 fails, and says where the server really is
        remote::HostProfile typed = prof;
        typed.serverCommand = QStringLiteral("/home/nobody/qurcuma/release/qurcuma-server --root /x");
        const Outcome o = probe(typed);
        CHECK(o.done && o.result.sshOk && !o.result.sessionOk);
        CHECK(o.steps.contains("1:3") && o.steps.contains("2:4"));
        CHECK(o.result.message.contains("does not exist") && o.result.message.contains(bin + "/qurcuma-server"));
    }
    {   // a directory without a server in it is refused
        const QString empty = dir.path() + "/empty";
        QDir().mkpath(empty);
        remote::HostProfile typed = prof;
        typed.serverCommand = empty;
        const Outcome o = probe(typed);
        CHECK(o.done && !o.result.sessionOk && o.result.message.contains("is a directory"));
    }
    {   // a directory that holds qurcuma-server is completed, the arguments stay, the session works
        remote::HostProfile typed = prof;
        typed.serverCommand = bin + "/ --grace 5";  // trailing slash, as typed
        const Outcome o = probe(typed);
        CHECK(o.done && o.result.sessionOk);
        CHECK(o.result.correctedCommand == bin + "/qurcuma-server --grace 5");
        // the same with a directory given as ~/...: expanded on the remote side
        QDir().mkpath(emptyHome + "/rel");
        QFile::link(realServer, emptyHome + "/rel/qurcuma-server");
        typed.serverCommand = QStringLiteral("~/rel");
        const Outcome o2 = probe(typed);
        CHECK(o2.done && o2.result.sessionOk && o2.result.correctedCommand == emptyHome + "/rel/qurcuma-server");
    }
    {   // it exists, with arguments: the first word is checked, the session works
        remote::HostProfile typed = prof;
        typed.serverCommand = realServer + " --grace 5";
        const Outcome o = probe(typed);
        CHECK(o.done && o.result.sessionOk);
    }
    {   // a command with characters a shell would interpret is not put into the check script
        const QByteArray script = remote::HostProbe::findServerScript(QStringLiteral("x;touch${IFS}/tmp/pwn"));
        CHECK(!script.contains("pwn") && !script.contains("CUSTOM"));
        CHECK(remote::HostProbe::findServerScript(QStringLiteral("/opt/q/qurcuma-server --root /s")).contains("tok='/opt/q/qurcuma-server'"));
    }
    {   // the tunnel names a missing server command instead of a bare exit code
        remote::HostProfile typed = prof;
        typed.serverCommand = QStringLiteral("/nonexistent/qurcuma-server");
        remote::SshTunnel tunnel(typed);
        QString failure;
        QEventLoop loop;
        QTimer limit;
        limit.setSingleShot(true);
        QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&tunnel, &remote::SshTunnel::failed, [&](const QString& m) { failure = m; loop.quit(); });
        QObject::connect(&tunnel, &remote::SshTunnel::ready, &loop, &QEventLoop::quit);
        limit.start(30000);
        tunnel.start();
        loop.exec();
        CHECK(failure.contains("does not exist") && failure.contains("/nonexistent/qurcuma-server") && failure.contains("Test"));
    }
    {   // a directory saved as the server command: the tunnel says so
        remote::HostProfile typed = prof;
        typed.serverCommand = dir.path() + "/empty";
        remote::SshTunnel tunnel(typed);
        QString failure;
        QEventLoop loop;
        QTimer limit;
        limit.setSingleShot(true);
        QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&tunnel, &remote::SshTunnel::failed, [&](const QString& m) { failure = m; loop.quit(); });
        QObject::connect(&tunnel, &remote::SshTunnel::ready, &loop, &QEventLoop::quit);
        limit.start(30000);
        tunnel.start();
        loop.exec();
        CHECK(failure.contains("cannot be run") && failure.contains("directory"));
    }
    {   // ssh refuses: step 1 fails with the way out, the rest is not tried
        remote::HostProfile denied = prof;
        denied.target = QStringLiteral("denied");
        const Outcome o = probe(denied);
        CHECK(o.done && !o.result.sshOk && o.result.message.contains("ssh-copy-id denied"));
        CHECK(o.steps.contains("0:3") && o.steps.contains("1:4") && o.steps.contains("2:4"));
    }
    {   // an invalid profile never reaches ssh
        remote::HostProfile bad = prof;
        bad.target = QStringLiteral("-oProxyCommand=x");
        const Outcome o = probe(bad);
        CHECK(o.done && !o.result.sshOk);
    }

    QSettings().remove(QStringLiteral("remote/hosts"));
    if (failures)
        std::cerr << failures << " check(s) failed" << std::endl;
    else
        std::cout << "All checks passed. (0 failed)" << std::endl;
    return failures ? 1 : 0;
}
