// test_remote_ssh.cpp - RemoteBackend through SshTunnel with a stand-in for ssh
// Claude Generated 2026 (WP remote compute R2). QURCUMA_SSH points at a small python script
// that behaves like ssh for the two invocations SshTunnel makes: it runs the remote command
// locally, and implements "-N -L local:127.0.0.1:remote" as a TCP forwarder. This exercises
// the token handover over stdin, the "listening <port>" handshake, the forward, the retry
// until the forward listens, and the teardown. A real ssh/host is not involved.

#include "remote/remotebackend.h"
#include "remote/sshtunnel.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>
#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;                \
            ++failures;                                                                     \
        }                                                                                   \
    } while (0)

static const char* kFakeSsh = R"PY(#!/usr/bin/env python3
import os, socket, sys, threading
args = sys.argv[1:]
rest, i = [], 0
while i < len(args):
    if args[i] == '-o':
        i += 2
        continue
    rest.append(args[i]); i += 1
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
)PY";

template <typename Pred> static bool waitFor(Pred done, int ms)
{
    QEventLoop loop;
    QTimer poll, limit;
    QObject::connect(&poll, &QTimer::timeout, [&] { if (done()) loop.quit(); });
    QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start(10);
    limit.start(ms);
    loop.exec();
    return done();
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // the argument lists: the token is never on a command line
    {
        const QStringList a = remote::SshTunnel::serverArgs("hpc", "qurcuma-server --root /scratch");
        CHECK(a.contains("BatchMode=yes") && a.contains("hpc"));
        CHECK(a.last() == "qurcuma-server --root /scratch --port 0 --token-stdin --once");
        const QStringList f = remote::SshTunnel::forwardArgs("hpc", 41000, 42000);
        CHECK(f.contains("-N") && f.contains("41000:127.0.0.1:42000") && f.contains("ExitOnForwardFailure=yes"));
        CHECK(!a.join(' ').contains("--token ") && !f.join(' ').contains("--token"));
    }

    if (QStandardPaths::findExecutable("python3").isEmpty()) {
        std::cout << "SKIP: python3 not found (needed for the ssh stand-in)" << std::endl;
        return 0;
    }
    const QString server = QCoreApplication::applicationDirPath() + "/qurcuma-server";
    if (!QFile::exists(server)) {
        std::cerr << "qurcuma-server not found next to the test" << std::endl;
        return 1;
    }

    QTemporaryDir work;
    QFile script(work.path() + "/fake-ssh");
    CHECK(script.open(QIODevice::WriteOnly) && script.write(kFakeSsh) > 0);
    script.close();
    script.setPermissions(QFile::permissions(script.fileName()) | QFileDevice::ExeOwner);
    qputenv("QURCUMA_SSH", script.fileName().toUtf8());
    QDir::setCurrent(work.path());

    SimulationConfig cfg;
    cfg.mode = SimulationConfig::Mode::GeometryOptimization;
    cfg.method = "gfnff";
    cfg.steps = 50;
    cfg.optSingleShot = true;
    cfg.fpsLimit = 0;
    QVector<MolAtom> atoms(3);
    atoms[0].element = "O"; atoms[0].position = QVector3D(0.0f, 0.0f, 0.0f);
    atoms[1].element = "H"; atoms[1].position = QVector3D(1.05f, 0.1f, 0.0f);
    atoms[2].element = "H"; atoms[2].position = QVector3D(-0.4f, 0.95f, 0.2f);

    {
        remote::RemoteBackend rb("testhost", server + " --root " + work.path() + "/sessions");
        rb.setMolecule(atoms);
        rb.setBonds({ { 0, 1, 1 }, { 0, 2, 1 } });
        rb.setConfig(cfg);
        SimulationFrame last;
        int n = 0;
        bool done = false, aborted = true;
        QStringList errs;
        QObject::connect(&rb, &SimulationBackend::frameReady, [&](SimulationFramePtr f) { last = *f; ++n; });
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        QObject::connect(&rb, &SimulationBackend::finished, [&](const QString&, bool a) { done = true; aborted = a; });
        rb.start();
        CHECK(waitFor([&] { return done || !errs.isEmpty(); }, 120000));
        for (const QString& e : errs) std::cerr << "error: " << e.toStdString() << std::endl;
        CHECK(errs.isEmpty() && done && !aborted);
        std::cout << "via fake ssh: " << n << " frames, final step " << last.step << ", E " << last.energy << " Eh" << std::endl;
        CHECK(n > 0 && last.step == 8 && std::fabs(last.energy - (-0.327657)) < 1e-5);  // value of the local run (test_remote_loopback)
    }

    // unreachable server command: the start fails with a message, no frames
    {
        remote::RemoteBackend rb("testhost", "/nonexistent/qurcuma-server");
        rb.setMolecule(atoms);
        rb.setConfig(cfg);
        QStringList errs;
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        rb.start();
        CHECK(waitFor([&] { return !errs.isEmpty(); }, 30000));
        CHECK(errs.size() == 1);
    }

    std::cout << (failures == 0 ? "All checks passed." : "FAILED") << " (" << failures << " failed)" << std::endl;
    return failures == 0 ? 0 : 1;
}
