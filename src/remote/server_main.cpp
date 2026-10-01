// server_main.cpp - qurcuma-server: runs curcuma simulations for a remote qurcuma
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R1, docs/WP-remote-compute-vr.md)
//
//   qurcuma-server --port 40123 (--token <secret> | --token-stdin) [--root <dir>] [--once] [--grace <s>]
//
// Listens on 127.0.0.1 only; the client reaches it through `ssh -L`. Prints
// "listening <port>" on stdout once ready (port 0 picks a free one).

#include "serversession.h"
#include "generated/parameter_registry.h"  // initialize_generated_registry()

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>

#include <csignal>
#include <cstdio>

int main(int argc, char* argv[])
{
    initialize_generated_registry();  // curcuma's parameter defaults, as qurcuma's main.cpp does
    // The ssh session that started the server may vanish; the run must survive that.
#ifdef SIGHUP
    std::signal(SIGHUP, SIG_IGN);
#endif
#ifdef SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
#endif
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({ "port", "TCP port on 127.0.0.1 (0 = any).", "port", "0" });
    parser.addOption({ "token", "Shared secret the client must present (visible in the process list; prefer --token-stdin).", "token" });
    parser.addOption({ "token-stdin", "Read the shared secret from the first line of standard input." });
    parser.addOption({ "root", "Directory for session directories.", "dir",
        QDir::homePath() + QStringLiteral("/qurcuma-sessions") });
    parser.addOption({ "browse", "Additional directory clients may list and download from (repeatable); the session root is always shared.", "dir" });
    parser.addOption({ "once", "Exit after the first session ends." });
    parser.addOption({ "grace", "Seconds a run keeps going after the client connection is lost.", "seconds", "60" });
    parser.process(app);

    QString token = parser.value("token");
    if (parser.isSet("token-stdin")) {
        char line[512] = {};
        if (std::fgets(line, sizeof line, stdin))
            token = QString::fromUtf8(line).trimmed();
    }
    if (token.size() < 16) {
        std::fprintf(stderr, "qurcuma-server: a token of at least 16 characters is required (--token or --token-stdin)\n");
        return 2;
    }
    const QString root = parser.value("root");
    if (!QDir().mkpath(root)) {
        std::fprintf(stderr, "qurcuma-server: cannot create %s\n", qPrintable(root));
        return 2;
    }

    remote::RemoteServer server(token, root, parser.value("grace").toInt(), parser.values("browse"), &app);
    if (!server.listen(quint16(parser.value("port").toUInt()))) {
        std::fprintf(stderr, "qurcuma-server: cannot listen: %s\n", qPrintable(server.errorString()));
        return 2;
    }
    if (parser.isSet("once"))
        QObject::connect(&server, &remote::RemoteServer::sessionEnded, &app, &QCoreApplication::quit);

    std::printf("listening %u\n", unsigned(server.port()));
    std::fflush(stdout);
    return app.exec();
}
