// main.cpp
#include <QApplication>

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

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
