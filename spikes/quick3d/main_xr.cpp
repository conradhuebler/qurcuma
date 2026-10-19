// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// T9 VR spike - entry point. Shows the spike molecule scene through Qt Quick 3D XR
// (OpenXR). Runtime is whatever the OpenXR loader selects (WiVRn -> Pico, or Monado).
// Claude Generated.
#include "scenecontroller.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption sizeOpt({ QStringLiteral("n") }, QStringLiteral("Atom count of the synthetic dataset."),
        QStringLiteral("atoms"), QStringLiteral("1000"));
    QCommandLineOption fileOpt({ QStringLiteral("f") }, QStringLiteral("XYZ file to load."),
        QStringLiteral("file"));
    parser.addOption(sizeOpt);
    parser.addOption(fileOpt);
    parser.addHelpOption();
    parser.process(app);

    // OpenXR via Qt Quick 3D XR needs the Vulkan RHI on this stack.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    SceneController controller;
    controller.setBackendName(QStringLiteral("Vulkan + OpenXR"));
    controller.setEmbedMode(QStringLiteral("XrView"));
    if (parser.isSet(fileOpt))
        controller.loadFile(parser.value(fileOpt));
    else
        controller.loadDataset(parser.value(sizeOpt).toInt());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/QurcumaSpikeXr/MainXr.qml")));
    return app.exec();
}
