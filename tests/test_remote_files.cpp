// test_remote_files.cpp - browsing and downloading files on the server machine
// Claude Generated 2026 (WP remote compute R4). Server and client in one process (direct
// WebSocket). Covers the path policy, listing, chunked download with checksum, refusal of
// anything outside the shared directories, and the full trajectory written on the server.

#include "generated/parameter_registry.h"
#include "remote/protocol.h"
#include "remote/remotebackend.h"
#include "remote/remotefiles.h"
#include "remote/serversession.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QRandomGenerator>
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

static void write(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(data);
}

int main(int argc, char* argv[])
{
    initialize_generated_registry();
    QCoreApplication app(argc, argv);
    QTemporaryDir work;
    QDir::setCurrent(work.path());
    const QString token = "0123456789abcdef-test";

    // ---- path policy (no server) ---------------------------------------------------
    {
        const QString share = work.path() + "/share";
        const QString outside = work.path() + "/outside";
        write(share + "/a/file.txt", "x");
        write(outside + "/secret.txt", "s");
        QFile::link(outside, share + "/link");             // symlink leaving the root
        QFile::link(share + "/a", share + "/inner-link");  // symlink staying inside
        QString err;
        const QStringList roots{ share };
        CHECK(!remote::resolveBrowsePath("", roots, &err).isEmpty());
        CHECK(remote::resolveBrowsePath("a/file.txt", roots, &err) == QFileInfo(share + "/a/file.txt").canonicalFilePath());
        CHECK(remote::resolveBrowsePath(share + "/a", roots, &err) == QFileInfo(share + "/a").canonicalFilePath());
        CHECK(remote::resolveBrowsePath("inner-link/file.txt", roots, &err) == QFileInfo(share + "/a/file.txt").canonicalFilePath());
        CHECK(remote::resolveBrowsePath("../outside/secret.txt", roots, &err).isEmpty());
        CHECK(remote::resolveBrowsePath(outside + "/secret.txt", roots, &err).isEmpty());
        CHECK(remote::resolveBrowsePath("link/secret.txt", roots, &err).isEmpty());
        CHECK(remote::resolveBrowsePath("/etc/passwd", roots, &err).isEmpty());
        CHECK(remote::resolveBrowsePath("a/missing.txt", roots, &err).isEmpty());
        CHECK(remote::resolveBrowsePath("../share-evil", roots, &err).isEmpty());  // prefix is not containment
        CHECK(!remote::resolveBrowsePath("a/file.txt", { share, outside }, &err).isEmpty());
        CHECK(remote::resolveBrowsePath("x", {}, &err).isEmpty());
        // chunk framing
        QByteArray out;
        quint32 id = 0;
        CHECK(remote::decodeChunk(remote::encodeChunk(7, "abc"), id, out) && id == 7 && out == "abc");
        CHECK(!remote::decodeChunk(QByteArray(2, 1), id, out));
    }

    const QString sessions = work.path() + "/sessions";
    const QString shared = work.path() + "/data";
    // a file larger than several chunks, with content that exposes reordering
    QByteArray big(3 * remote::kChunkBytes + 12345, 0);
    for (int i = 0; i < big.size(); ++i)
        big[i] = char((i * 31 + (i >> 8)) & 0xff);
    write(shared + "/big.bin", big);
    write(shared + "/small.xyz", "1\nc\nH 0 0 0\n");
    write(shared + "/sub/inner.txt", "inner");

    remote::RemoteServer server(token, sessions, 5, { shared });
    CHECK(server.listen(0));
    const QUrl url(QStringLiteral("ws://127.0.0.1:%1").arg(server.port()));

    // ---- listing and download ----------------------------------------------------
    {
        remote::RemoteFiles rf(url, token);
        QStringList roots, listedPaths, failures2;
        QVector<remote::RemoteFiles::Entry> entries;
        qint64 lastProgress = -1;
        bool done = false;
        QString got;
        QString dlError;
        QObject::connect(&rf, &remote::RemoteFiles::connected, [&](const QStringList& r) { roots = r; });
        QObject::connect(&rf, &remote::RemoteFiles::listing, [&](const QString& p, const QVector<remote::RemoteFiles::Entry>& e, bool) {
            listedPaths << p; entries = e; });
        QObject::connect(&rf, &remote::RemoteFiles::listingFailed, [&](const QString&, const QString& m) { failures2 << m; });
        QObject::connect(&rf, &remote::RemoteFiles::downloadProgress, [&](quint32, qint64 r, qint64) { lastProgress = r; });
        QObject::connect(&rf, &remote::RemoteFiles::downloadFinished, [&](quint32, const QString& p) { done = true; got = p; });
        QObject::connect(&rf, &remote::RemoteFiles::downloadFailed, [&](quint32, const QString& m) { dlError = m; });
        rf.connectToHost();
        CHECK(waitFor([&] { return !roots.isEmpty(); }, 10000));
        CHECK(roots.size() == 2 && roots[1] == QFileInfo(shared).canonicalFilePath());

        rf.list(shared);
        CHECK(waitFor([&] { return listedPaths.size() >= 2; }, 5000));
        CHECK(entries.size() == 3 && entries[0].name == "sub" && entries[0].isDir);  // directories first
        bool sawBig = false;
        for (const auto& e : entries) sawBig |= (e.name == "big.bin" && e.size == big.size());
        CHECK(sawBig);

        rf.download(shared + "/big.bin", work.path() + "/dl/big.bin");
        CHECK(waitFor([&] { return done || !dlError.isEmpty(); }, 30000));
        CHECK(dlError.isEmpty() && done);
        QFile f(got);
        CHECK(f.open(QIODevice::ReadOnly) && f.readAll() == big);
        CHECK(lastProgress == big.size());
        CHECK(!QFile::exists(work.path() + "/dl/big.bin.part"));

        // refused: outside the shared directories, symlink escape, directory as file
        QFile::link(work.path() + "/sessions", shared + "/escape");
        dlError.clear();
        rf.download("/etc/passwd", work.path() + "/dl/passwd");
        CHECK(waitFor([&] { return !dlError.isEmpty(); }, 5000));
        CHECK(!QFile::exists(work.path() + "/dl/passwd") && !QFile::exists(work.path() + "/dl/passwd.part"));
        dlError.clear();
        rf.download(shared + "/escape/x", work.path() + "/dl/x");
        CHECK(waitFor([&] { return !dlError.isEmpty(); }, 5000));
        dlError.clear();
        rf.download(shared + "/sub", work.path() + "/dl/sub");
        CHECK(waitFor([&] { return !dlError.isEmpty(); }, 5000));
        rf.list("/etc");
        CHECK(waitFor([&] { return !failures2.isEmpty(); }, 5000));
        // a download works again after failures
        done = false;
        rf.download(shared + "/small.xyz", work.path() + "/dl/small.xyz");
        CHECK(waitFor([&] { return done; }, 5000));
    }

    // ---- full trajectory written on the server, fetched afterwards ---------------------
    {
        SimulationConfig cfg;
        cfg.mode = SimulationConfig::Mode::MolecularDynamics;
        cfg.method = "gfnff";
        cfg.steps = 30;
        cfg.fpsLimit = 0;
        QVector<MolAtom> atoms(3);
        atoms[0].element = "O"; atoms[0].position = QVector3D(0.0f, 0.0f, 0.0f);
        atoms[1].element = "H"; atoms[1].position = QVector3D(0.96f, 0.0f, 0.0f);
        atoms[2].element = "H"; atoms[2].position = QVector3D(-0.24f, 0.93f, 0.0f);

        remote::RemoteBackend rb(url, token);
        rb.setMolecule(atoms);
        rb.setConfig(cfg);
        rb.setServerTrajectory(true);
        QStringList status, errs;
        bool done = false;
        QObject::connect(&rb, &remote::RemoteBackend::statusText, [&](const QString& t) { status << t; });
        QObject::connect(&rb, &SimulationBackend::errorOccurred, [&](const QString& e) { errs << e; });
        QObject::connect(&rb, &SimulationBackend::finished, [&] { done = true; });
        rb.start();
        CHECK(waitFor([&] { return done || !errs.isEmpty(); }, 120000));
        CHECK(errs.isEmpty() && done);
        QString filesLine;
        for (const QString& s : status) if (s.startsWith("Files on")) filesLine = s;
        CHECK(filesLine.contains(".trj.xyz"));
        std::cout << filesLine.toStdString() << std::endl;
    }
    // the session of the run has ended (its client left); a browsing client finds the files
    {
        CHECK(waitFor([&] { return server.activeSession() == nullptr; }, 10000));
        remote::RemoteFiles rf(url, token);
        QVector<remote::RemoteFiles::Entry> entries;
        QString path;
        QObject::connect(&rf, &remote::RemoteFiles::listing, [&](const QString& p, const QVector<remote::RemoteFiles::Entry>&, bool) { path = p; });
        rf.connectToHost();
        CHECK(waitFor([&] { return !path.isEmpty(); }, 10000));
        // find the session directory with the trajectory below the session root
        // (curcuma names it after the molecule's basename; the worker gives none, so it is the
        // hidden file .snapshots/.trj.xyz inside the session directory)
        QString trajectory;
        QDirIterator it(sessions, { "*.trj.xyz" }, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        if (it.hasNext())
            trajectory = it.next();
        CHECK(!trajectory.isEmpty() && QFileInfo(trajectory).size() > 0);
        if (trajectory.isEmpty()) { QDirIterator all(sessions, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories); while (all.hasNext()) std::cout << "  found: " << all.next().toStdString() << std::endl; }
        std::cout << "trajectory on the server: " << trajectory.toStdString() << std::endl;

        // and the client can fetch it through the shared session root
        QString local;
        bool fetched = false;
        QObject::connect(&rf, &remote::RemoteFiles::downloadFinished, [&](quint32, const QString& p) { fetched = true; local = p; });
        rf.download(trajectory, work.path() + "/dl/trajectory.xyz");
        CHECK(waitFor([&] { return fetched; }, 10000));
        QFile a(trajectory), b(local);
        CHECK(a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly) && a.readAll() == b.readAll());
    }

    std::cout << (failures == 0 ? "All checks passed." : "FAILED") << " (" << failures << " failed)" << std::endl;
    return failures == 0 ? 0 : 1;
}
