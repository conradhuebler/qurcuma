// test_remote_loopback.cpp - server and client on one machine: the final geometry of a
// remote optimisation equals that of the local worker.
// Claude Generated 2026 (WP remote compute R1). Needs curcuma (gfnff), no display.

#include "remote/protocol.h"
#include "remote/remotebackend.h"
#include "remote/serversession.h"
#include "simulationbackend.h"
#include "generated/parameter_registry.h"  // initialize_generated_registry()

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
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

static QVector<MolAtom> distortedWater()
{
    QVector<MolAtom> a(3);
    a[0].element = "O"; a[0].position = QVector3D(0.0f, 0.0f, 0.0f);
    a[1].element = "H"; a[1].position = QVector3D(1.05f, 0.1f, 0.0f);
    a[2].element = "H"; a[2].position = QVector3D(-0.4f, 0.95f, 0.2f);
    return a;
}

static SimulationConfig optConfig()
{
    SimulationConfig c;
    c.mode = SimulationConfig::Mode::GeometryOptimization;
    c.method = "gfnff";
    c.steps = 50;
    c.optSingleShot = true;
    c.writeTrajectory = false;
    c.fpsLimit = 0;
    return c;
}

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
    initialize_generated_registry();  // curcuma's parameter defaults (as main.cpp does)
    QCoreApplication app(argc, argv);
    QTemporaryDir work;
    QDir::setCurrent(work.path());

    // reference: the local backend
    SimulationFrame local;
    bool localDone = false;
    QStringList localErrors;
    {
        LocalBackend b;
        b.setMolecule(distortedWater());
        b.setBonds({ { 0, 1, 1 }, { 0, 2, 1 } });
        b.setConfig(optConfig());
        QObject::connect(&b, &SimulationBackend::frameReady, [&](SimulationFramePtr f) { local = *f; });
        QObject::connect(&b, &SimulationBackend::errorOccurred, [&](const QString& e) { localErrors << e; std::cerr << "local error: " << e.toStdString() << std::endl; });
        QObject::connect(&b, &SimulationBackend::finished, [&] { localDone = true; });
        b.start();
        CHECK(waitFor([&] { return localDone; }, 120000));
    }
    CHECK(local.positions.size() == 3);
    CHECK(localErrors.isEmpty() && local.energy != 0.0 && local.step > 0);  // a real optimisation happened
    std::cout << "local run: step " << local.step << ", E " << local.energy << " Eh" << std::endl;

    // remote: server session in this process, client socket
    const QString token = "0123456789abcdef-test";
    QWebSocketServer server("test", QWebSocketServer::NonSecureMode);
    CHECK(server.listen(QHostAddress::LocalHost, 0));
    remote::ServerSession* session = nullptr;
    QObject::connect(&server, &QWebSocketServer::newConnection, [&] {
        session = new remote::ServerSession(server.nextPendingConnection(), token, work.path() + "/sessions", &server);
    });

    QWebSocket client;
    SimulationFrame remoteLast;
    int frames = 0;
    bool welcomed = false, finished = false, aborted = true;
    QStringList errors;
    QObject::connect(&client, &QWebSocket::binaryMessageReceived, [&](const QByteArray& m) {
        SimulationFrame f;
        if (remote::decodeFrame(m, f)) { remoteLast = f; ++frames; }
    });
    QObject::connect(&client, &QWebSocket::textMessageReceived, [&](const QString& t) {
        const QJsonObject o = QJsonDocument::fromJson(t.toUtf8()).object();
        const QString type = o.value("type").toString();
        if (type == "welcome") welcomed = true;
        else if (type == "finished") { finished = true; aborted = o.value("aborted").toBool(); }
        else if (type == "error") errors << o.value("message").toString();
    });
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort())));
    CHECK(waitFor([&] { return client.state() == QAbstractSocket::ConnectedState; }, 5000));

    // wrong token is refused
    {
        QWebSocket bad;
        QStringList badErrors;
        bool closed = false;
        QObject::connect(&bad, &QWebSocket::textMessageReceived, [&](const QString& t) {
            badErrors << QJsonDocument::fromJson(t.toUtf8()).object().value("type").toString(); });
        QObject::connect(&bad, &QWebSocket::disconnected, [&] { closed = true; });
        bad.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort())));
        waitFor([&] { return bad.state() == QAbstractSocket::ConnectedState; }, 5000);
        bad.sendTextMessage(R"({"type":"hello","protocol":1,"token":"wrong"})");
        CHECK(waitFor([&] { return closed; }, 5000));
        CHECK(badErrors.contains("error") && !badErrors.contains("welcome"));
    }

    client.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{ { "type", "hello" },
        { "protocol", remote::kProtocolVersion }, { "token", token } }).toJson(QJsonDocument::Compact)));
    CHECK(waitFor([&] { return welcomed; }, 5000));

    // a start with a path-like file parameter is refused before anything runs
    {
        QJsonObject cfg = remote::configToJson(optConfig());
        cfg["rmsdMtdRefFile"] = "/etc/passwd";
        client.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{ { "type", "start" }, { "config", cfg },
            { "atoms", remote::atomsToJson(distortedWater()) }, { "bonds", remote::bondsToJson({}) } }).toJson(QJsonDocument::Compact)));
        CHECK(waitFor([&] { return !errors.isEmpty(); }, 5000));
        CHECK(frames == 0 && !finished);
        errors.clear();
    }

    client.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{ { "type", "start" },
        { "config", remote::configToJson(optConfig()) }, { "atoms", remote::atomsToJson(distortedWater()) },
        { "bonds", remote::bondsToJson({ { 0, 1, 1 }, { 0, 2, 1 } }) } }).toJson(QJsonDocument::Compact)));
    CHECK(waitFor([&] { return finished; }, 120000));
    for (const QString& e : errors) std::cerr << "server error: " << e.toStdString() << std::endl;
    CHECK(errors.isEmpty());
    CHECK(frames > 0 && remoteLast.positions.size() == 3);

    double maxDev = 0.0;
    for (size_t i = 0; i < 3 && i < local.positions.size() && i < remoteLast.positions.size(); ++i)
        maxDev = std::max(maxDev, double((local.positions[i] - remoteLast.positions[i]).length()));
    std::cout << "frames received: " << frames << ", max |local - remote| = " << maxDev << " A, "
              << "energy diff = " << std::fabs(local.energy - remoteLast.energy) << " Eh" << std::endl;
    CHECK(maxDev < 1e-4);  // float32 positions on the wire
    CHECK(std::fabs(local.energy - remoteLast.energy) < 1e-10);
    CHECK(local.step == remoteLast.step);


    // ---- RemoteBackend (client class) against the same server -------------------------
    {
        QFile ref(work.path() + "/ref.xyz");
        CHECK(ref.open(QIODevice::WriteOnly) && ref.write("1\nx\nH 0 0 0\n") > 0);
        ref.close();
        SimulationConfig cfg = optConfig();
        cfg.writeTrajectory = true;
        cfg.rmsdMtdRefFile = work.path() + "/ref.xyz";

        remote::RemoteBackend rb(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort())), token);
        rb.setTrajectoryPath(work.path() + "/client.trj.xyz");
        rb.setMolecule(distortedWater());
        rb.setBonds({ { 0, 1, 1 }, { 0, 2, 1 } });
        rb.setConfig(cfg);
        SimulationFrame viaBackend;
        int n = 0;
        bool done = false, aborted = true;
        QStringList errs;
        QObject::connect(&rb, &SimulationBackend::frameReady, [&](SimulationFramePtr f) { viaBackend = *f; ++n; });
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        QObject::connect(&rb, &SimulationBackend::finished, [&](const QString&, bool a) { done = true; aborted = a; });
        rb.start();
        CHECK(waitFor([&] { return done || !errs.isEmpty(); }, 120000));
        for (const QString& e : errs) std::cerr << "RemoteBackend error: " << e.toStdString() << std::endl;
        CHECK(errs.isEmpty() && done && !aborted);
        CHECK(viaBackend.positions.size() == 3 && viaBackend.step == local.step);
        double dev = 0.0;
        for (size_t i = 0; i < 3 && i < viaBackend.positions.size(); ++i)
            dev = std::max(dev, double((local.positions[i] - viaBackend.positions[i]).length()));
        CHECK(dev < 1e-4 && std::fabs(local.energy - viaBackend.energy) < 1e-10);

        // trajectory written on this side: n frames of 3 atoms (count + comment + 3 lines)
        QFile trj(work.path() + "/client.trj.xyz");
        CHECK(trj.open(QIODevice::ReadOnly));
        const QList<QByteArray> lines = trj.readAll().split('\n');
        CHECK(lines.size() == n * 5 + 1);
        CHECK(!lines.isEmpty() && lines[0] == "3");
        // the reference file reached the session directory under its upload name
        CHECK(session && QFile::exists(session->sessionDir() + "/in/rmsd_mtd_ref_file-ref.xyz"));
    }

    // a missing file parameter ends the start on this side, nothing is sent
    {
        SimulationConfig cfg = optConfig();
        cfg.rmsdMtdRefFile = work.path() + "/does-not-exist.xyz";
        remote::RemoteBackend rb(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort())), token);
        rb.setMolecule(distortedWater());
        rb.setConfig(cfg);
        QStringList errs;
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        rb.start();
        CHECK(waitFor([&] { return !errs.isEmpty(); }, 5000));
        CHECK(errs.size() == 1 && errs[0].contains("not found"));
    }

    // no server: the run ends with an error, there is no fallback to a local run
    {
        remote::RemoteBackend rb(QUrl(QStringLiteral("ws://127.0.0.1:1")), token);
        rb.setMolecule(distortedWater());
        rb.setConfig(optConfig());
        QStringList errs;
        int frames2 = 0;
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        QObject::connect(&rb, &SimulationBackend::frameReady, [&](SimulationFramePtr) { ++frames2; });
        rb.start();
        CHECK(waitFor([&] { return !errs.isEmpty(); }, 30000));
        CHECK(errs.size() == 1 && frames2 == 0);
    }

    std::cout << (failures == 0 ? "All checks passed." : "FAILED") << " (" << failures << " failed)" << std::endl;
    return failures == 0 ? 0 : 1;
}
