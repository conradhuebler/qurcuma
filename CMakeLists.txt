cmake_minimum_required(VERSION 3.16)
project(qurcuma VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)  # Wichtig für Q_OBJECT
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS
Core
Gui
Widgets
3DCore
3DRender
3DInput
3DExtras
REQUIRED
)
# Quelldateien
set(SOURCES
    src/view.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/settings.cpp
)

# Header-Dateien
set(HEADERS
    src/mainwindow.h
    src/settings.h
    src/view.h
)

add_executable(qurcuma
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(qurcuma PRIVATE
Qt6::Core
Qt6::Gui
Qt6::Widgets
Qt6::3DCore
Qt6::3DRender
Qt6::3DInput
Qt6::3DExtras
)

# Include-Verzeichnisse
target_include_directories(qurcuma PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
