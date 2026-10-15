// test_sshconfig.cpp - SshConfigParser reads every Host entry of a config file
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026: parseFile kept only the last entry of a file (the previous entry was
// checked after parseLine had already replaced it with the new, still empty one). The host
// lists of the Simulation dock, Remote Files and SFTP all depend on it.

#include "sshconfig.h"

#include <QFile>
#include <QTemporaryDir>

#include <iostream>

static int failures = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << std::endl;               \
            ++failures;                                                                     \
        }                                                                                   \
    } while (0)

static const SshConfigEntry* find(const QList<SshConfigEntry>& all, const QString& host)
{
    for (const SshConfigEntry& e : all)
        if (e.host == host)
            return &e;
    return nullptr;
}

int main()
{
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString path = dir.path() + "/config";
    QFile f(path);
    CHECK(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(
        "# a comment\n"
        "Host gateway\n"
        "    HostName gw.example.org\n"
        "    User alice\n"
        "    Port 2222\n"
        "\n"
        "Host nohostname\n"
        "    User bob\n"
        "\n"
        "Host first second\n"          // two aliases of one block
        "\tHostName shared.example.org\n"
        "\tIdentityFile /keys/id\n"
        "\n"
        "Host *.hpc\n"                 // a pattern: parsed, the lists leave it out
        "    User pattern\n"
        "\n"
        "host lowercase\n"             // keywords are case-insensitive
        "    hostname low.example.org\n"
        "\n"
        "Host last\n"
        "    HostName last.example.org\n");
    f.close();

    const QList<SshConfigEntry> all = SshConfigParser::parseFile(path);

    // every block, not only the last one
    CHECK(find(all, "gateway") && find(all, "gateway")->hostName == "gw.example.org");
    CHECK(find(all, "gateway") && find(all, "gateway")->user == "alice" && find(all, "gateway")->port == 2222);
    CHECK(find(all, "nohostname") && find(all, "nohostname")->hostName == "nohostname");  // defaults to the alias
    CHECK(find(all, "nohostname") && find(all, "nohostname")->user == "bob");
    CHECK(find(all, "first") && find(all, "second"));
    CHECK(find(all, "first") && find(all, "first")->hostName == "shared.example.org" && find(all, "first")->identityFile == "/keys/id");
    CHECK(find(all, "second") && find(all, "second")->hostName == "shared.example.org");
    CHECK(find(all, "*.hpc"));
    CHECK(find(all, "lowercase") && find(all, "lowercase")->hostName == "low.example.org");
    CHECK(find(all, "last") && find(all, "last")->hostName == "last.example.org");
    CHECK(all.size() == 7);

    // findHost goes through the same list
    CHECK(SshConfigParser::findHost("gateway", path).isValid());
    CHECK(!SshConfigParser::findHost("missing", path).isValid());

    if (failures)
        std::cerr << failures << " check(s) failed" << std::endl;
    else
        std::cout << "All checks passed. (0 failed)" << std::endl;
    return failures ? 1 : 0;
}
