// server_main.cpp - qurcuma-server: runs curcuma simulations for a remote qurcuma
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R1, docs/WP-remote-compute-vr.md)
//
//   qurcuma-server --port 40123 --token <secret> [--root <dir>] [--once]
//
// Listens on 127.0.0.1 only; the client reaches it through `ssh -L`. Prints
// "listening <port>" on stdout once ready (port 0 picks a free one).

#include "serversession.h"
#include "generated/parameter_registry.h"  // initialize_generated_registry()

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <QWebSocket>
#include <QWebSocketServer>

#include <cstdio>

int main(int argc, char* argv[])
{
    initialize_generated_registry();  // curcuma's parameter defaults, as qurcuma's main.cpp does
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({ "port", "TCP port on 127.0.0.1 (0 = any).", "port", "0" });
    parser.addOption({ "token", "Shared secret the client must present.", "token" });
    parser.addOption({ "root", "Directory for session directories.", "dir",
        QDir::homePath() + QStringLiteral("/qurcuma-sessions") });
    parser.addOption({ "once", "Exit after the first session ends." });
    parser.process(app);

    const QString token = parser.value("token");
    if (token.size() < 16) {
        std::fprintf(stderr, "qurcuma-server: --token with at least 16 characters is required\n");
        return 2;
    }
    const QString root = parser.value("root");
    if (!QDir().mkpath(root)) {
        std::fprintf(stderr, "qurcuma-server: cannot create %s\n", qPrintable(root));
        return 2;
    }

    QWebSocketServer server(QStringLiteral("qurcuma-server"), QWebSocketServer::NonSecureMode);
    if (!server.listen(QHostAddress::LocalHost, quint16(parser.value("port").toUInt()))) {
        std::fprintf(stderr, "qurcuma-server: cannot listen: %s\n", qPrintable(server.errorString()));
        return 2;
    }

    remote::ServerSession* active = nullptr;
    const bool once = parser.isSet("once");
    QObject::connect(&server, &QWebSocketServer::newConnection, [&]() {
        QWebSocket* socket = server.nextPendingConnection();
        if (active) {  // one session per process (the working directory is process-wide)
            socket->close(QWebSocketProtocol::CloseCodeGoingAway);
            socket->deleteLater();
            return;
        }
        active = new remote::ServerSession(socket, token, root, &server);
        QObject::connect(active, &remote::ServerSession::ended, &app, [&, once]() {
            active->deleteLater();
            active = nullptr;
            if (once)
                app.quit();
        });
    });

    std::printf("listening %u\n", unsigned(server.serverPort()));
    std::fflush(stdout);
    return app.exec();
}
