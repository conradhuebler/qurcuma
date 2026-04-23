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

# Claude Generated - SFTP Support with libssh
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBSSH REQUIRED libssh)
else()
    # Fallback: Try to find libssh manually
    find_library(LIBSSH_LIBRARIES NAMES ssh)
    find_path(LIBSSH_INCLUDE_DIRS libssh/libssh.h)
    if(NOT LIBSSH_LIBRARIES OR NOT LIBSSH_INCLUDE_DIRS)
        message(FATAL_ERROR "libssh not found. Install libssh-dev (Debian/Ubuntu) or libssh-devel (Fedora/RHEL)")
    endif()
endif()

add_subdirectory(external/CuteChart)

# Claude Generated - Curcuma simulation backend (Interactive Simulation Integration)
# Build curcuma_core and curcuma_cap as libraries, no standalone executable needed here
set(USE_XTB OFF CACHE BOOL "" FORCE)
set(USE_TBLITE OFF CACHE BOOL "" FORCE)
set(USE_GFNFF OFF CACHE BOOL "" FORCE)
set(USE_ULYSSES OFF CACHE BOOL "" FORCE)
set(USE_D3 OFF CACHE BOOL "" FORCE)
set(USE_D4 OFF CACHE BOOL "" FORCE)
# Claude Generated - CUDA: user-overridable via -DUSE_CUDA=ON; not forced so GPU builds work
option(USE_CUDA "Enable CUDA-accelerated GFN-FF (requires CUDA toolkit, gfnff -gpu cuda)" OFF)
set(USE_PCH OFF CACHE BOOL "" FORCE)
set(Helper OFF CACHE BOOL "" FORCE)
add_subdirectory(external/curcuma EXCLUDE_FROM_ALL)

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
    src/dialogs/sftpdialog.cpp  # Claude Generated - SFTP remote file access
    src/sshconfig.cpp  # Claude Generated - SSH config parser for SFTP integration
    src/sftpcache.cpp  # Claude Generated - SFTP cache management for SFTP integration
    src/widgets/breadcrumbbar.cpp  # Claude Generated Phase 1
    src/workspacemanager.cpp  # Claude Generated Phase 4.2
    src/selectionmanager.cpp  # Claude Generated Phase 2A
    src/measurementoverlay.cpp  # Claude Generated Phase 2B
    src/atomlistpanel.cpp  # Claude Generated Phase 2C
    src/performanceoptimizer.cpp  # Claude Generated Phase 3B
    src/bondeditor.cpp  # Claude Generated Phase 4B
    src/pbrmaterial.cpp  # Claude Generated Phase 4A
    src/pdbparser.cpp  # Claude Generated Phase 5C
    src/mol2parser.cpp  # Claude Generated Phase 5C

    #src/dialogs/nmrstructuremodel.cpp
    src/atominstancingsystem.cpp  # Claude Generated Phase 5D (GPU instancing implementation)
    src/bondinstancingsystem.cpp  # Claude Generated Phase 3.2 - Interactive Sim: bond GPU instancing
    src/simulationworker.cpp  # Claude Generated - Interactive Simulation Integration
    src/simulationcontrolwidget.cpp  # Claude Generated - Interactive Simulation Integration
    src/forceinjector.cpp  # Claude Generated 2026 - Topological force distribution (Phase 4)
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
    src/dialogs/sftpdialog.h  # Claude Generated - SFTP remote file access
    src/sftpmodel.hpp  # Claude Generated - SFTP model (header-only with Q_OBJECT)
    src/sshconfig.h  # Claude Generated - SSH config parser for SFTP integration
    src/sftpcache.h  # Claude Generated - SFTP cache management for SFTP integration
    src/widgets/breadcrumbbar.h  # Claude Generated Phase 1
    src/workspacemanager.h  # Claude Generated Phase 4.2
    src/selectionmanager.h  # Claude Generated Phase 2A
    src/measurementoverlay.h  # Claude Generated Phase 2B
    src/atomlistpanel.h  # Claude Generated Phase 2C
    src/performanceoptimizer.h  # Claude Generated Phase 3B
    src/bondeditor.h  # Claude Generated Phase 4B
    src/pbrmaterial.h  # Claude Generated Phase 4A
    src/pdbparser.h  # Claude Generated Phase 5C
    src/mol2parser.h  # Claude Generated Phase 5C
    src/atominstancingsystem.h  # Claude Generated Phase 5D (GPU instancing implementation)
    src/bondinstancingsystem.h  # Claude Generated Phase 3.2 - Interactive Sim: bond GPU instancing
    src/simulationworker.h  # Claude Generated - Interactive Simulation Integration
    src/simulationcontrolwidget.h  # Claude Generated - Interactive Simulation Integration
    src/forceinjector.h  # Claude Generated 2026 - Topological force distribution (Phase 4)
    #src/dialogs/nmrstructuremodel.h

)

add_executable(qurcuma
    ${SOURCES}
    ${HEADERS}
    resources.qrc
)

# Claude Generated - OpenMP needed by curcuma_core
find_package(OpenMP)
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
${LIBSSH_LIBRARIES}  # Claude Generated - SFTP support
curcuma_core  # Claude Generated - Curcuma simulation backend
curcuma_cap   # Claude Generated - Curcuma capabilities (MD, Opt)
$<$<BOOL:${OpenMP_CXX_FOUND}>:OpenMP::OpenMP_CXX>  # Claude Generated - OpenMP for curcuma
)

# Include-Verzeichnisse
target_include_directories(qurcuma PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
    ${CMAKE_CURRENT_SOURCE_DIR}/external/curcuma  # Claude Generated - Curcuma headers
    ${CMAKE_CURRENT_SOURCE_DIR}/external/curcuma/src  # Claude Generated - Curcuma src headers
    ${CMAKE_CURRENT_SOURCE_DIR}/external/curcuma/external/eigen-3.4.0  # Claude Generated - Eigen for curcuma types
    ${CMAKE_CURRENT_SOURCE_DIR}/external/curcuma/external  # Claude Generated - json.hpp and other curcuma externals
    ${CMAKE_CURRENT_BINARY_DIR}/external/curcuma  # Claude Generated - generated global_config.h at src/global_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/external/LBFGSpp/include  # Claude Generated - LBFGSpp for curcuma
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fmt/include  # Claude Generated - fmt for curcuma
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ripser  # Claude Generated - ripser for curcuma
    ${CMAKE_CURRENT_SOURCE_DIR}/external/cpp-d4/include  # Claude Generated - cpp-d4 for curcuma
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ulysses-main/src  # Claude Generated - ulysses for curcuma
    ${LIBSSH_INCLUDE_DIRS}  # Claude Generated - SFTP support
)

# VTF Parser Test
#add_executable(test_vtf_minimal test_vtf_minimal.cpp)
#target_link_libraries(test_vtf_minimal PRIVATE
#Qt6::Core
#)
#target_include_directories(test_vtf_minimal PRIVATE
#    ${CMAKE_CURRENT_SOURCE_DIR}/src
#    ${CMAKE_CURRENT_SOURCE_DIR}/external/
#)

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
