// main.cpp
#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>

#include "mainwindow.h"

// Claude Generated - Populate curcuma's ParameterRegistry with all module
// defaults (thermostat, simplemd params, etc.). The generated header lives in
// curcuma's build dir and is normally included only by curcuma's own main.cpp;
// qurcuma links curcuma_core as a library so we must trigger registration here
// or ConfigManager::get<T>() throws "Parameter '...' not found in module ...".
#include "generated/parameter_registry.h"

int main(int argc, char *argv[])
{
    initialize_generated_registry();

    // Claude Generated - Request 4x MSAA + 24-bit depth for smoother atom
    // silhouettes and better depth resolution. Must be set before QApplication
    // so the Qt3D GL context picks it up.
    {
        QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
        fmt.setSamples(4);
        fmt.setDepthBufferSize(24);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    QApplication app(argc, argv);
    MainWindow window;
    window.show();

    // Claude Generated (Apr 2026): Load file from command-line argument.
    // Delayed via QTimer::singleShot so Qt3D is fully initialized before
    // the scene is populated.  200 ms is sufficient for all tested hardware.
    // Usage: ./qurcuma molecule.xyz
    QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        const QString filePath = args[1];
        QTimer::singleShot(200, &window, [&window, filePath]() {
            window.loadFileFromArg(filePath);
        });
    }

    return app.exec();
}
