cmake_minimum_required(VERSION 3.16)
project(qurcuma VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)  # Wichtig für Q_OBJECT
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt6 COMPONENTS
Core
Gui
Widgets
3DCore
3DRender
3DInput
3DExtras
Charts
REQUIRED
)

add_subdirectory(external/CuteChart)
# Quelldateien
set(SOURCES
    src/view.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/settings.cpp
    src/vtfparser.cpp
    src/xyzparser.cpp
    src/modifiabletextedit.cpp
    src/visualizationsettingsdialog.cpp
    src/dialogs/nmrspectrumdialog.cpp
    src/dialogs/nmrcontroller.cpp
    src/dialogs/nmrdatastore.cpp
    src/dialogs/nmrstructureproxymodel.cpp
    src/widgets/breadcrumbbar.cpp  # Claude Generated Phase 1
    src/workspacemanager.cpp  # Claude Generated Phase 4.2
    src/selectionmanager.cpp  # Claude Generated Phase 2A
    src/measurementoverlay.cpp  # Claude Generated Phase 2B
    src/atomlistpanel.cpp  # Claude Generated Phase 2C

    #src/dialogs/nmrstructuremodel.cpp
)

# Header-Dateien
set(HEADERS
    src/mainwindow.h
    src/settings.h
    src/view.h
    src/vtfparser.h
    src/xyzparser.h
    src/modifiabletextedit.h
    src/visualizationsettingsdialog.h
    src/dialogs/nmrspectrumdialog.h
    src/widgets/breadcrumbbar.h  # Claude Generated Phase 1
    src/workspacemanager.h  # Claude Generated Phase 4.2
    src/selectionmanager.h  # Claude Generated Phase 2A
    src/measurementoverlay.h  # Claude Generated Phase 2B
    src/atomlistpanel.h  # Claude Generated Phase 2C
    #src/dialogs/nmrstructuremodel.h

)

add_executable(qurcuma
    ${SOURCES}
    ${HEADERS}
    resources.qrc
)

target_link_libraries(qurcuma PRIVATE
Qt6::Core
Qt6::Gui
Qt6::Widgets
Qt6::3DCore
Qt6::3DRender
Qt6::3DInput
Qt6::3DExtras
Qt6::Charts
cutechart
)

# Include-Verzeichnisse
target_include_directories(qurcuma PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
)

# VTF Parser Test
add_executable(test_vtf_minimal test_vtf_minimal.cpp)
target_link_libraries(test_vtf_minimal PRIVATE
Qt6::Core
)
target_include_directories(test_vtf_minimal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
)

# VTF Bond Test - Claude Generated
add_executable(test_vtf_bonds test_vtf_bonds.cpp)
target_link_libraries(test_vtf_bonds PRIVATE
Qt6::Core
)
target_include_directories(test_vtf_bonds PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
)

# VTF Frame Test - Claude Generated
add_executable(test_vtf_frames test_vtf_frames.cpp)
target_link_libraries(test_vtf_frames PRIVATE
Qt6::Core
)
target_include_directories(test_vtf_frames PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
)

# VTF Full Test - Claude Generated
add_executable(test_vtf_full test_vtf_full.cpp)
target_link_libraries(test_vtf_full PRIVATE
Qt6::Core
)
target_include_directories(test_vtf_full PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
)
