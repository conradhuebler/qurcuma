// main.cpp
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
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

    // Claude Generated 2026 - CLI option parsing for auto-starting the
    // interactive simulation from bash (`qurcuma <file> -md` / `-opt`). This is
    // a diagnostic lever: the interactive sim crashes immediately on start in
    // the RELEASE build (debug works), so launching it from the command line
    // makes the crash reproducible under gdb/valgrind for a backtrace.
    //
    // Parsed BEFORE MainWindow is constructed so --help exits cleanly without
    // flashing the window or building the (expensive) main window.
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Qurcuma - molecular visualization and interactive simulation"));
    parser.addHelpOption();
    // Treat single-dash multi-char words as long options so `-md`/`-opt` work
    // alongside `--md`/`--opt` (matches the requested invocation style).
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    QCommandLineOption mdOpt("md",
        QStringLiteral("Auto-start molecular dynamics after loading the file"));
    QCommandLineOption optOpt("opt",
        QStringLiteral("Auto-start geometry optimization after loading the file"));
    parser.addOption(mdOpt);
    parser.addOption(optOpt);
    // Allow `qurcuma file.xyz -md` (flag after the positional file argument):
    // ParseAsOptions parses options even when they appear after a positional.
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    parser.process(app);

    const bool doOpt = parser.isSet(optOpt);
    const bool doMd = parser.isSet(mdOpt);
    const QStringList posArgs = parser.positionalArguments();

    QString cliArg;
    bool autoStart = false;
    SimulationConfig::Mode autoMode = SimulationConfig::Mode::MolecularDynamics;
    if (!posArgs.isEmpty()) {
        cliArg = posArgs.first();
        if (doOpt && doMd) {
            qWarning() << "Both -md and -opt given; using -opt";
            autoStart = true;
            autoMode = SimulationConfig::Mode::GeometryOptimization;
        } else if (doOpt) {
            autoStart = true;
            autoMode = SimulationConfig::Mode::GeometryOptimization;
        } else if (doMd) {
            autoStart = true;
            autoMode = SimulationConfig::Mode::MolecularDynamics;
        }
    }

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
    //   qurcuma <file> -md               - load the file and auto-start MD
    //   qurcuma <file> -opt              - load the file and auto-start Opt
    if (!cliArg.isEmpty()) {
        const QFileInfo cliInfo(cliArg);
        const bool isDir = (cliArg == "." || cliInfo.isDir());
        QTimer::singleShot(200, &window,
            [&window, cliArg, isDir, autoStart, autoMode]() {
                if (isDir) {
                    window.loadDirFromArg(cliArg);
                } else {
                    window.loadFileFromArg(cliArg);
                    if (autoStart)
                        window.autoStartSimulation(autoMode);
                }
            });
    }

    return app.exec();
}