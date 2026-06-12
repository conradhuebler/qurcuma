// main.cpp
#include <QApplication>
#include <QDir>
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

    // Claude Generated 2026 - "Use Invocation Directory" preference.
    // Capture the directory qurcuma was launched from BEFORE MainWindow
    // exists. QApplication's constructor does not change QDir::currentPath(),
    // so the order is robust; the variable is what makes features like
    // "cd ~/work && qurcuma" pick up the right Working Directory.
    const QString invocationDir = QDir::currentPath();

    MainWindow window(invocationDir);
    window.show();

    // Claude Generated (Apr 2026): Load file or directory from command-line
    // argument. Delayed via QTimer::singleShot so Qt3D is fully initialized
    // before the scene is populated (200 ms is sufficient for all tested
    // hardware).
    //
    // Supported invocations:
    //   qurcuma                          - no arg, use defaults
    //   qurcuma .                        - switch working directory to CWD
    //   qurcuma <dir>                    - switch working directory to <dir>
    //   qurcuma <file.xyz|file.vtf|...>  - load the file; the working
    //                                      directory is auto-switched to the
    //                                      file's parent directory on success
    QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        const QString cliArg = args[1];
        QFileInfo cliInfo(cliArg);
        if (cliArg == "." || cliInfo.isDir()) {
            QTimer::singleShot(200, &window, [&window, cliArg]() {
                window.loadDirFromArg(cliArg);
            });
        } else {
            QTimer::singleShot(200, &window, [&window, cliArg]() {
                window.loadFileFromArg(cliArg);
            });
        }
    }

    return app.exec();
}
