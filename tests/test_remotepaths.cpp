// test_remotepaths.cpp - Helpers of the remote browser: sorting, paths, breadcrumb, bookmark ids, connections
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026: covers the Qt-only part of the Remote segment (src/remotepaths.*). The libssh
// session (SftpClient) and the widget (RemoteBrowserPanel) are not covered here.

#include "remotepaths.h"

#include "remote/hostprofiles.h"
#include "sshconfig.h"

#include <iostream>

static int failures = 0;
static int checks = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        ++checks;                                                                           \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;               \
            ++failures;                                                                     \
        }                                                                                   \
    } while (0)

static RemoteEntry entry(const QString& name, bool isDir)
{
    RemoteEntry e;
    e.name = name;
    e.isDir = isDir;
    return e;
}

static QStringList names(const QList<RemoteEntry>& entries)
{
    QStringList out;
    for (const RemoteEntry& e : entries)
        out << e.name;
    return out;
}

static void testSorting()
{
    QList<RemoteEntry> list = { entry("b.xyz", false), entry("Zeta", true), entry("a.xyz", false),
                                entry("alpha", true), entry("B.log", false), entry("ABC", false),
                                entry("abc", false), entry("Beta", true) };
    remotepaths::sortEntries(list);
    // folders first, case-insensitive; "ABC" and "abc" compare equal and keep their input order
    CHECK(names(list) == QStringList({ "alpha", "Beta", "Zeta", "a.xyz", "ABC", "abc", "B.log", "b.xyz" }));

    QList<RemoteEntry> swapped = { entry("abc", false), entry("ABC", false) };
    remotepaths::sortEntries(swapped);
    CHECK(names(swapped) == QStringList({ "abc", "ABC" }));

    QList<RemoteEntry> empty;
    remotepaths::sortEntries(empty);
    CHECK(empty.isEmpty());
}

static void testPaths()
{
    using namespace remotepaths;
    CHECK(normalise("/") == "/");
    CHECK(normalise("") == ".");
    CHECK(normalise("//home///user/") == "/home/user");
    CHECK(normalise("/home/user/./runs/../calc") == "/home/user/calc");
    CHECK(normalise("/../..") == "/");
    CHECK(normalise("/a/b/../../..") == "/");
    CHECK(normalise("a/../../b") == "../b");
    CHECK(normalise("./") == ".");

    CHECK(join("/home/user", "runs") == "/home/user/runs");
    CHECK(join("/home/user/", "runs/") == "/home/user/runs");
    CHECK(join("/", "scratch") == "/scratch");
    CHECK(join("/home/user", "/scratch/x") == "/scratch/x");
    CHECK(join("/home/user", "..") == "/home");
    CHECK(join("", "x") == "x");

    CHECK(parent("/home/user/runs") == "/home/user");
    CHECK(parent("/home/user/runs/") == "/home/user");
    CHECK(parent("/home") == "/");
    CHECK(parent("/") == "/");
    CHECK(parent("relative") == ".");

    CHECK(displayPath("/home/user", "/home/user") == "~");
    CHECK(displayPath("/home/user/", "/home/user") == "~");
    CHECK(displayPath("/home/user/runs/a", "/home/user/") == "~/runs/a");
    CHECK(displayPath("/home/username", "/home/user") == "/home/username");
    CHECK(displayPath("/scratch/user", "/home/user") == "/scratch/user");
    CHECK(displayPath("/scratch", "") == "/scratch");
    CHECK(displayPath("/scratch", "/") == "/scratch");
}

static QStringList labels(const QList<PathSegment>& segments)
{
    QStringList out;
    for (const PathSegment& s : segments)
        out << s.label;
    return out;
}

static QStringList paths(const QList<PathSegment>& segments)
{
    QStringList out;
    for (const PathSegment& s : segments)
        out << s.path;
    return out;
}

static void testBreadcrumb()
{
    using remotepaths::breadcrumbSegments;
    auto inside = breadcrumbSegments("/home/user/runs/md1", "/home/user");
    CHECK(labels(inside) == QStringList({ "Home", "runs", "md1" }));
    CHECK(paths(inside) == QStringList({ "/home/user", "/home/user/runs", "/home/user/runs/md1" }));

    CHECK(labels(breadcrumbSegments("/home/user", "/home/user")) == QStringList({ "Home" }));
    CHECK(labels(breadcrumbSegments("/home/user/runs/", "/home/user/")) == QStringList({ "Home", "runs" }));

    // outside the home: every component stays whole, with or without trailing slash
    auto outside = breadcrumbSegments("/scratch/user", "/home/user");
    CHECK(labels(outside) == QStringList({ "Root", "scratch", "user" }));
    CHECK(paths(outside) == QStringList({ "/", "/scratch", "/scratch/user" }));
    CHECK(labels(breadcrumbSegments("/scratch/user/", "/home/user")) == QStringList({ "Root", "scratch", "user" }));
    CHECK(labels(breadcrumbSegments("/home/username", "/home/user")) == QStringList({ "Root", "home", "username" }));

    CHECK(labels(breadcrumbSegments("/", "/home/user")) == QStringList({ "Root" }));
    CHECK(paths(breadcrumbSegments("/", "")) == QStringList({ "/" }));
    CHECK(breadcrumbSegments("", "/home/user").isEmpty());

    // a drive path (local Windows) keeps its drive as the first part
    auto drive = breadcrumbSegments("C:/data/calc", "C:/Users/me");
    CHECK(labels(drive) == QStringList({ "C:", "data", "calc" }));
    CHECK(paths(drive) == QStringList({ "C:", "C:/data", "C:/data/calc" }));
}

static void testSourceIds()
{
    using namespace remotepaths;
    CHECK(computerSourceId("cluster") == "computer:cluster");
    CHECK(isComputerSourceId("computer:cluster"));
    CHECK(!isComputerSourceId("computer:"));
    CHECK(!isComputerSourceId("{1b4e28ba-2fa1-11d2-883f-0016d3cca427}"));
    CHECK(computerNameFromSourceId("computer:my box") == "my box");
    CHECK(computerNameFromSourceId("{1b4e28ba-2fa1-11d2-883f-0016d3cca427}").isEmpty());
    CHECK(computerNameFromSourceId(computerSourceId("a:b")) == "a:b");
}

static void testConnections()
{
    using namespace remotepaths;

    remote::HostProfile plain;
    plain.name = "Workstation";
    plain.target = "alice@ws.example.org";
    plain.port = 0;
    RemoteConnection c = connectionForComputer(plain, nullptr);
    CHECK(c.sourceId == "computer:Workstation");
    CHECK(c.label == "Workstation");
    CHECK(c.host == "ws.example.org");
    CHECK(c.user == "alice");
    CHECK(c.port == 22);
    CHECK(c.proxyCommand.isEmpty());
    CHECK(c.account() == "alice@ws.example.org");
    CHECK(c.cacheKey() == "alice@ws.example.org:22");

    // the computer's address is a ~/.ssh/config alias: HostName, User, Port and ProxyJump of the alias apply
    SshConfigEntry alias;
    alias.host = "hpc";
    alias.hostName = "login.hpc.example.org";
    alias.user = "bob";
    alias.port = 2222;
    alias.proxyCommand = "ssh -W %h:%p gateway";
    remote::HostProfile viaAlias;
    viaAlias.name = "Cluster";
    viaAlias.target = "hpc";
    c = connectionForComputer(viaAlias, &alias);
    CHECK(c.host == "login.hpc.example.org");
    CHECK(c.user == "bob");
    CHECK(c.port == 2222);
    CHECK(c.proxyCommand == "ssh -W %h:%p gateway");

    // what the computer states itself comes on top of the alias
    viaAlias.target = "carol@hpc";
    viaAlias.port = 2200;
    c = connectionForComputer(viaAlias, &alias);
    CHECK(c.user == "carol");
    CHECK(c.port == 2200);
    CHECK(c.host == "login.hpc.example.org");

    // an invalid alias entry is ignored
    SshConfigEntry invalid;
    c = connectionForComputer(plain, &invalid);
    CHECK(c.host == "ws.example.org");

    // SFTP profile / typed host: port 0 and an empty user mean "not given"
    c = connectionForHost("{id}", "Profile", "hpc", QString(), 0, &alias);
    CHECK(c.sourceId == "{id}");
    CHECK(c.host == "login.hpc.example.org");
    CHECK(c.user == "bob");
    CHECK(c.port == 2222);
    c = connectionForHost("{id}", "Profile", "hpc", "dave", 22, &alias);
    CHECK(c.user == "dave");
    CHECK(c.port == 22);
    c = connectionForHost(QString(), "x", "  10.0.0.5 ", QString(), 0, nullptr);
    CHECK(c.host == "10.0.0.5");
    CHECK(c.user.isEmpty());
    CHECK(c.port == 22);
    CHECK(c.account() == "10.0.0.5");
    CHECK(c.isValid());
    CHECK(!connectionForHost(QString(), "x", "  ", QString(), 0, nullptr).isValid());
}

int main()
{
    testSorting();
    testPaths();
    testBreadcrumb();
    testSourceIds();
    testConnections();
    if (failures == 0)
        std::cout << "test_remotepaths: all " << checks << " checks passed" << std::endl;
    else
        std::cerr << "test_remotepaths: " << failures << " of " << checks << " checks failed" << std::endl;
    return failures == 0 ? 0 : 1;
}
