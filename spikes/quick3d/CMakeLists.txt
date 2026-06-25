# WP0 Qt Quick 3D + Vulkan spike - standalone build (does NOT touch qurcuma's build).
# Configure/build independently, e.g.:
#   cmake -S spikes/quick3d -B spikes/quick3d/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build spikes/quick3d/build
# Claude Generated.
cmake_minimum_required(VERSION 3.21)
project(qurcuma_quick3d_spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Widgets Qml Quick Quick3D QuickWidgets)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(quick3d_spike
    main.cpp
    moleculedata.cpp
    atominstancing.cpp
    bondinstancing.cpp
    scenecontroller.cpp
)

qt_add_qml_module(quick3d_spike
    URI QurcumaSpike
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES Main.qml
)

target_link_libraries(quick3d_spike PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
    Qt6::QuickWidgets
)
