cmake_minimum_required(VERSION 3.20)
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
Charts
Qml
Quick
Quick3D
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

# External dependencies via FetchContent (replaces git submodules)
include(FetchContent)

FetchContent_Declare(CuteChart
    GIT_REPOSITORY https://github.com/conradhuebler/CuteChart
    GIT_TAG        master
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/external/CuteChart
)

# Curcuma simulation backend — options must be set BEFORE FetchContent_MakeAvailable
set(USE_XTB OFF CACHE BOOL "" FORCE)
set(USE_TBLITE OFF CACHE BOOL "" FORCE)
set(USE_GFNFF OFF CACHE BOOL "" FORCE)
set(USE_ULYSSES OFF CACHE BOOL "" FORCE)
set(USE_D3 OFF CACHE BOOL "" FORCE)
set(USE_D4 OFF CACHE BOOL "" FORCE)
option(USE_CUDA "Enable CUDA-accelerated GFN-FF (requires CUDA toolkit, gfnff -gpu cuda)" OFF)
option(USE_ROCM "Enable ROCm/HIP GPU backend (requires ROCm, gfn1/gfn2 -gpu rocm)" OFF)
option(USE_VULKAN "Enable Vulkan Compute GPU backend (requires Vulkan SDK, gfn1/gfn2 -gpu vulkan)" OFF)
set(USE_ROCM_XTB   ${USE_ROCM} CACHE BOOL "" FORCE)
set(USE_VULKAN_XTB ${USE_VULKAN} CACHE BOOL "" FORCE)
set(USE_PCH OFF CACHE BOOL "" FORCE)
set(Helper OFF CACHE BOOL "" FORCE)

FetchContent_Declare(curcuma
    GIT_REPOSITORY https://github.com/conradhuebler/curcuma
    GIT_TAG        feature/vulkan_rocm
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/external/curcuma
)

# Claude Generated 2026 - external/curcuma and external/CuteChart are active local
# working trees (curcuma has local commits ahead of origin + uncommitted work).
# FetchContent's populate/update step runs `git checkout/rebase <GIT_TAG>` and
# aborts with merge conflicts on every (re)configure, breaking debug AND release
# builds. When the local checkouts are already present, fully disconnect
# FetchContent so it uses them as-is and never touches their git state. On a fresh
# machine (checkouts absent) the normal clone path runs. Override per build dir
# with -DFETCHCONTENT_FULLY_DISCONNECTED=OFF to re-enable git sync.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/curcuma/CMakeLists.txt
    AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/CuteChart/CMakeLists.txt)
    set(FETCHCONTENT_FULLY_DISCONNECTED ON)
    message(STATUS "qurcuma: local external/curcuma + external/CuteChart present — FetchContent fully disconnected (no git update)")
else()
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
endif()

FetchContent_MakeAvailable(CuteChart curcuma)

# Exclude curcuma standalone executables from default build
set_target_properties(curcuma PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(curcuma_helper PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Quelldateien
set(SOURCES
    src/view.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/settings.cpp
    src/vtfparser.cpp
    src/xyzparser.cpp
    src/modifiabletextedit.cpp
    src/displaypanel.cpp  # Claude Generated 2026 - docked viewer display options
    src/widgets/collapsiblesection.cpp  # Claude Generated 2026 - accordion section
    src/widgets/commandpalette.cpp  # Claude Generated 2026 - P3 Ctrl+K command palette
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
    src/atomlistpanel.cpp  # Claude Generated Phase 2C
    src/performanceoptimizer.cpp  # Claude Generated Phase 3B
    src/bondeditor.cpp  # Claude Generated Phase 4B
    src/pdbparser.cpp  # Claude Generated Phase 5C
    src/mol2parser.cpp  # Claude Generated Phase 5C

    #src/dialogs/nmrstructuremodel.cpp
    src/simulationworker.cpp  # Claude Generated - Interactive Simulation Integration
    src/simulationcontrolwidget.cpp  # Claude Generated - Interactive Simulation Integration
    src/snapshotswidget.cpp  # Claude Generated 2026 - Snapshot history foundation
    src/rmsdwidget.cpp  # Claude Generated 2026 - RMSD / align tool (Analysis dock)
    src/forceinjector.cpp  # Claude Generated 2026 - Topological force distribution (Phase 4)
    src/elementdata.cpp  # Claude Generated 2026 - Quick3D renderer: shared element tables
    src/atominstancing.cpp  # Claude Generated 2026 - Quick3D renderer: atom instancing
    src/bondinstancing.cpp  # Claude Generated 2026 - Quick3D renderer: bond instancing
    src/scenecontroller.cpp  # Claude Generated 2026 - Quick3D renderer: scene view-model
)

# Header-Dateien
set(HEADERS
    src/mainwindow.h
    src/settings.h
    src/view.h
    src/vtfparser.h
    src/xyzparser.h
    src/modifiabletextedit.h
    src/displaypanel.h  # Claude Generated 2026 - docked viewer display options
    src/widgets/collapsiblesection.h  # Claude Generated 2026 - accordion section
    src/widgets/commandpalette.h  # Claude Generated 2026 - P3 Ctrl+K command palette
    src/dialogs/nmrspectrumdialog.h
    src/dialogs/sftpdialog.h  # Claude Generated - SFTP remote file access
    src/sftpmodel.hpp  # Claude Generated - SFTP model (header-only with Q_OBJECT)
    src/sshconfig.h  # Claude Generated - SSH config parser for SFTP integration
    src/sftpcache.h  # Claude Generated - SFTP cache management for SFTP integration
    src/widgets/breadcrumbbar.h  # Claude Generated Phase 1
    src/workspacemanager.h  # Claude Generated Phase 4.2
    src/selectionmanager.h  # Claude Generated Phase 2A
    src/atomlistpanel.h  # Claude Generated Phase 2C
    src/performanceoptimizer.h  # Claude Generated Phase 3B
    src/bondeditor.h  # Claude Generated Phase 4B
    src/pdbparser.h  # Claude Generated Phase 5C
    src/mol2parser.h  # Claude Generated Phase 5C
    src/simulationworker.h  # Claude Generated - Interactive Simulation Integration
    src/simulationcontrolwidget.h  # Claude Generated - Interactive Simulation Integration
    src/snapshotswidget.h  # Claude Generated 2026 - Snapshot history foundation
    src/rmsdwidget.h  # Claude Generated 2026 - RMSD / align tool (Analysis dock)
    src/forceinjector.h  # Claude Generated 2026 - Topological force distribution (Phase 4)
    src/elementdata.h  # Claude Generated 2026 - Quick3D renderer: shared element tables
    src/atominstancing.h  # Claude Generated 2026 - Quick3D renderer: atom instancing
    src/bondinstancing.h  # Claude Generated 2026 - Quick3D renderer: bond instancing
    src/scenecontroller.h  # Claude Generated 2026 - Quick3D renderer: scene view-model
    #src/dialogs/nmrstructuremodel.h

)

add_executable(qurcuma
    ${SOURCES}
    ${HEADERS}
    resources.qrc
)

# Claude Generated 2026 - Match curcuma's CPU/SIMD flags on the qurcuma target.
#
# curcuma (FetchContent, above) compiles its TUs with -march=native (or AVX-512)
# when USE_MARCH_NATIVE/USE_AVX512 is set, but that set(CMAKE_CXX_FLAGS ...) lives
# in curcuma's subdirectory scope and does NOT reach this parent-scope target.
# The mismatch changes Eigen's EIGEN_MAX_ALIGN_BYTES (16 for SSE2 here vs 64 for
# AVX-512), so a curcuma-allocated Eigen Geometry copied/freed inside a qurcuma TU
# (e.g. moleculeToFrame: `Geometry geo = mol.getGeometry();`) hits mismatched
# aligned-alloc bookkeeping -> "double free or corruption" the instant the
# interactive MD/Opt starts (Release-only; Debug builds both sides SSE2 and works).
# Mirroring curcuma's flag selection keeps both sides on the same Eigen ABI.
include(CheckCXXCompilerFlag)
if(USE_MARCH_NATIVE)
    check_cxx_compiler_flag("-march=native" QURCUMA_SUPPORTS_MARCH_NATIVE)
    if(QURCUMA_SUPPORTS_MARCH_NATIVE)
        target_compile_options(qurcuma PRIVATE -march=native)
        message(STATUS "qurcuma: matching curcuma -march=native (Eigen ABI alignment)")
    endif()
elseif(USE_AVX512)
    target_compile_options(qurcuma PRIVATE -mavx512f -mavx512vl -mavx512dq -mfma)
    message(STATUS "qurcuma: matching curcuma AVX-512 flags (Eigen ABI alignment)")
elseif(USE_AVX2)
    target_compile_options(qurcuma PRIVATE -mavx2 -mfma)
    message(STATUS "qurcuma: matching curcuma AVX2 flags (Eigen ABI alignment)")
endif()

# Claude Generated - OpenMP needed by curcuma_core
find_package(OpenMP)
target_link_libraries(qurcuma PRIVATE
Qt6::Core
Qt6::Gui
Qt6::Widgets
Qt6::Charts
Qt6::Qml
Qt6::Quick
Qt6::Quick3D
cutechart
${LIBSSH_LIBRARIES}  # Claude Generated - SFTP support
curcuma_core  # Claude Generated - Curcuma simulation backend
curcuma_cap   # Claude Generated - Curcuma capabilities (MD, Opt)
$<$<BOOL:${OpenMP_CXX_FOUND}>:OpenMP::OpenMP_CXX>  # Claude Generated - OpenMP for curcuma
)

# Include directories — curcuma deps fetched by curcuma's FetchContent into external/
target_include_directories(qurcuma PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/LBFGSpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/fmt/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ripser
    ${CMAKE_CURRENT_SOURCE_DIR}/external/cpp-d4/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ulysses-main/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/CxxThreadPool/include
    ${LIBSSH_INCLUDE_DIRS}
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
