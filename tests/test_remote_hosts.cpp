// test_remote_hosts.cpp - computers added in qurcuma: profiles, ssh arguments, the connection test
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute). The connection test (HostProbe) runs against a
// shell script standing in for ssh (QURCUMA_SSH): it drops the options like ssh does, fails for
// the target "denied", and otherwise runs the remote command locally. No real ssh or host.

#include "remote/hostprobe.h"
#include "remote/hostprofiles.h"
#include "remote/sshtunnel.h"

#include <QCoreApplication>
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
    bool done = false, sshOk = false, runs = false;
    QString path, message;
};

static Outcome probe(const remote::HostProfile& p)
{
    Outcome out;
    remote::HostProbe probe(p);
    QEventLoop loop;
    QTimer limit;
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&probe, &remote::HostProbe::finished, [&](bool ok, const QString& path, bool runs, const QString& msg) {
        out = { true, ok, runs, path, msg };
        loop.quit();
    });
    limit.start(30000);
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

    // ---- the connection test against a stand-in for ssh ------------------------------------------
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString fakeSsh = dir.path() + "/ssh";
    const QString log = dir.path() + "/ssh.log";
    writeExecutable(fakeSsh,
        "#!/bin/sh\n"
        "echo \"$@\" >> \"$QURCUMA_FAKE_LOG\"\n"
        "while [ $# -gt 0 ]; do case \"$1\" in -o|-p|-i) shift 2;; *) break;; esac; done\n"
        "target=\"$1\"; shift\n"
        "if [ \"$target\" = denied ]; then echo 'u@denied: Permission denied (publickey).' >&2; exit 255; fi\n"
        "exec sh -c \"$1\"\n");
    qputenv("QURCUMA_SSH", fakeSsh.toUtf8());
    qputenv("QURCUMA_FAKE_LOG", log.toUtf8());

    const QString emptyHome = dir.path() + "/home";
    QDir().mkpath(emptyHome);
    qputenv("HOME", emptyHome.toUtf8());
    const QByteArray basePath = "/usr/bin:/bin";
    qputenv("PATH", basePath);

    remote::HostProfile prof;
    prof.name = QStringLiteral("box");
    prof.target = QStringLiteral("box.example.org");
    prof.port = 2222;
    prof.identityFile = QStringLiteral("/k");

    {   // ssh works, no server anywhere
        const Outcome o = probe(prof);
        CHECK(o.done && o.sshOk && o.path.isEmpty() && o.message.contains("not found"));
    }
    {   // a server in the PATH that starts
        const QString bin = dir.path() + "/bin";
        QDir().mkpath(bin);
        writeExecutable(bin + "/qurcuma-server", "#!/bin/sh\nexit 0\n");
        qputenv("PATH", (bin.toUtf8() + ":" + basePath));
        const Outcome o = probe(prof);
        CHECK(o.done && o.sshOk && o.runs && o.path == bin + "/qurcuma-server");
        // the options of the profile reached ssh, before the target
        QFile f(log);
        CHECK(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        CHECK(text.contains("-p 2222 -i /k box.example.org"));
        CHECK(text.contains("BatchMode=yes"));
    }
    {   // a server that does not start
        writeExecutable(dir.path() + "/bin/qurcuma-server", "#!/bin/sh\nexit 1\n");
        const Outcome o = probe(prof);
        CHECK(o.done && o.sshOk && !o.runs && o.message.contains("does not start"));
    }
    {   // a server in a place of the fixed list ($HOME/.local/bin), not in the PATH
        qputenv("PATH", basePath);
        QDir().mkpath(emptyHome + "/.local/bin");
        writeExecutable(emptyHome + "/.local/bin/qurcuma-server", "#!/bin/sh\nexit 0\n");
        const Outcome o = probe(prof);
        CHECK(o.done && o.sshOk && o.runs && o.path == emptyHome + "/.local/bin/qurcuma-server");
    }
    {   // ssh refuses: the message names the way out
        remote::HostProfile denied = prof;
        denied.target = QStringLiteral("denied");
        const Outcome o = probe(denied);
        CHECK(o.done && !o.sshOk && o.message.contains("ssh-copy-id denied"));
    }
    {   // an invalid profile never reaches ssh
        remote::HostProfile bad = prof;
        bad.target = QStringLiteral("-oProxyCommand=x");
        const Outcome o = probe(bad);
        CHECK(o.done && !o.sshOk);
    }

    QSettings().remove(QStringLiteral("remote/hosts"));
    if (failures)
        std::cerr << failures << " check(s) failed" << std::endl;
    else
        std::cout << "All checks passed. (0 failed)" << std::endl;
    return failures ? 1 : 0;
}
