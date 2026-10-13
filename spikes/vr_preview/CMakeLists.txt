# Offline preview of the qurcuma headset scene (src/qml/VrScene.qml) - standalone build, does NOT
# touch qurcuma's build. The QML is read from the source tree at run time, so edits to src/qml/*.qml
# need no rebuild. See README.md. Claude Generated 2026.
cmake_minimum_required(VERSION 3.21)
project(qurcuma_vr_preview LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Quick3D/Quick3DXr are only needed as QML modules at run time (VrScene uses Node/Model/XrItem).
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(vr_preview main.cpp)
target_compile_definitions(vr_preview PRIVATE
    VR_PREVIEW_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vr_preview PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
