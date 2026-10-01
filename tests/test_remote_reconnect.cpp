// test_remote_reconnect.cpp - a run survives a lost connection (grace period, attach)
// Claude Generated 2026 (WP remote compute R3). Server and client in one process, direct
// WebSocket (no ssh). MD of a water molecule with GFN-FF, throttled to ~50 steps/s.

#include "remote/remotebackend.h"
#include "remote/serversession.h"
#include "generated/parameter_registry.h"  // initialize_generated_registry()

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;                \
            ++failures;                                                                     \
        }                                                                                   \
    } while (0)

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

static QVector<MolAtom> water()
{
    QVector<MolAtom> a(3);
    a[0].element = "O"; a[0].position = QVector3D(0.0f, 0.0f, 0.0f);
    a[1].element = "H"; a[1].position = QVector3D(0.96f, 0.0f, 0.0f);
    a[2].element = "H"; a[2].position = QVector3D(-0.24f, 0.93f, 0.0f);
    return a;
}

static SimulationConfig mdConfig()
{
    SimulationConfig c;
    c.mode = SimulationConfig::Mode::MolecularDynamics;
    c.method = "gfnff";
    c.steps = 100000;
    c.fpsLimit = 50;
    c.writeTrajectory = false;
    return c;
}

int main(int argc, char* argv[])
{
    initialize_generated_registry();
    QCoreApplication app(argc, argv);
    QTemporaryDir work;
    QDir::setCurrent(work.path());
    const QString token = "0123456789abcdef-test";

    // ---- 1. connection lost mid-run, client returns within the grace period ----------
    {
        remote::RemoteServer server(token, work.path() + "/s1", 20);
        CHECK(server.listen(0));
        remote::RemoteBackend rb(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())), token);
        rb.setMolecule(water());
        rb.setConfig(mdConfig());
        int n = 0, lastStep = -1, stepAtDrop = -1, nAtDrop = 0;
        bool done = false, aborted = true;
        QStringList errs, status;
        QObject::connect(&rb, &SimulationBackend::frameReady, [&](SimulationFramePtr f) { ++n; lastStep = f->step; });
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        QObject::connect(&rb, &SimulationBackend::finished, [&](const QString&, bool a) { done = true; aborted = a; });
        QObject::connect(&rb, &remote::RemoteBackend::statusText, [&](const QString& t) { status << t; });
        rb.start();
        CHECK(waitFor([&] { return n >= 5 || !errs.isEmpty(); }, 60000));
        CHECK(server.activeSession() && server.activeSession()->runActive());

        stepAtDrop = lastStep;
        nAtDrop = n;
        server.activeSession()->closeClientConnection();
        CHECK(waitFor([&] { return !status.filter("lost").isEmpty(); }, 5000));
        // the client attaches again and frames resume; the run was not restarted
        CHECK(waitFor([&] { return !status.filter("Reconnected").isEmpty() && n >= nAtDrop + 5; }, 30000));
        CHECK(lastStep > stepAtDrop);
        CHECK(errs.isEmpty() && !done);

        rb.requestStop();
        CHECK(waitFor([&] { return done || !errs.isEmpty(); }, 30000));
        for (const QString& e : errs) std::cerr << "error: " << e.toStdString() << std::endl;
        CHECK(done && !aborted && errs.isEmpty());
        std::cout << "reconnect: step " << stepAtDrop << " at the drop, " << lastStep << " at the end" << std::endl;
    }

    // ---- 2. client cannot return: after the grace period the run is stopped -----------
    {
        remote::RemoteServer server(token, work.path() + "/s2", 2);
        CHECK(server.listen(0));
        remote::RemoteBackend rb(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())), token);
        rb.setMolecule(water());
        rb.setConfig(mdConfig());
        int n = 0;
        QStringList errs;
        bool ended = false;
        QObject::connect(&rb, &SimulationBackend::frameReady, [&](SimulationFramePtr) { ++n; });
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        QObject::connect(&server, &remote::RemoteServer::sessionEnded, [&] { ended = true; });
        rb.start();
        CHECK(waitFor([&] { return n >= 3 || !errs.isEmpty(); }, 60000));
        server.setAccepting(false);  // the client cannot get back in
        server.activeSession()->closeClientConnection();
        CHECK(waitFor([&] { return ended; }, 15000));  // grace period (2 s) over: run stopped, session gone
        CHECK(server.activeSession() == nullptr);
        server.setAccepting(true);
        // the client now finds a server without its session and gives up with an error
        CHECK(waitFor([&] { return !errs.isEmpty(); }, 30000));
        CHECK(errs.size() == 1);
    }

    // ---- 3. the run ends while the client is away: it learns of it on return ----------
    {
        SimulationConfig c = mdConfig();
        c.steps = 150;  // about 3 s at 50 steps/s
        remote::RemoteServer server(token, work.path() + "/s3", 30);
        CHECK(server.listen(0));
        remote::RemoteBackend rb(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())), token);
        rb.setMolecule(water());
        rb.setConfig(c);
        int n = 0;
        bool done = false;
        QStringList errs;
        QObject::connect(&rb, &SimulationBackend::frameReady, [&](SimulationFramePtr) { ++n; });
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        QObject::connect(&rb, &SimulationBackend::finished, [&] { done = true; });
        rb.start();
        CHECK(waitFor([&] { return n >= 3 || !errs.isEmpty(); }, 60000));
        server.setAccepting(false);
        server.activeSession()->closeClientConnection();
        CHECK(waitFor([&] { return server.activeSession() && !server.activeSession()->runActive(); }, 30000));
        server.setAccepting(true);
        CHECK(waitFor([&] { return done || !errs.isEmpty(); }, 30000));
        CHECK(done && errs.isEmpty());
    }

    std::cout << (failures == 0 ? "All checks passed." : "FAILED") << " (" << failures << " failed)" << std::endl;
    return failures == 0 ? 0 : 1;
}
