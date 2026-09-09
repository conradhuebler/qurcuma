cmake_minimum_required(VERSION 3.20)
project(qurcuma VERSION 1.0 LANGUAGES CXX)

# Claude Generated 2026 - Without this, CMake writes no CTestTestfile.cmake and the
# add_test() calls further down are inert: the test binaries are built but
# `ctest` reports "No tests were found". Must be in the top-level list file.
#
# It also activates the 312 tests that curcuma's own CMakeLists registers through
# add_subdirectory(test_cases) -- their executables are EXCLUDE_FROM_ALL here (see
# the FetchContent block below), so a bare `ctest` would try to run binaries that
# were never built. qurcuma's own tests therefore carry the label `qurcuma`:
#     ctest --test-dir debug -L qurcuma
enable_testing()

# Claude Generated 2026 - Qt/QtCharts's exported CMake targets still list
# AGL.framework (legacy pre-Metal OpenGL entry points) as a link dependency on
# macOS, even though nothing in the RHI-based rendering path ever calls into
# it. macOS SDKs from Xcode 26 onward dropped AGL.framework entirely, so the
# final link of the qurcuma executable fails with "framework 'AGL' not found"
# despite every translation unit compiling cleanly. Since no AGL symbol is
# ever referenced, satisfy the linker with an empty stub dylib built at
# configure time instead of patching Qt's own CMake exports.
if(APPLE)
    set(AGL_STUB_DIR "${CMAKE_BINARY_DIR}/agl_stub")
    if(NOT EXISTS "${AGL_STUB_DIR}/AGL.framework/AGL")
        file(MAKE_DIRECTORY "${AGL_STUB_DIR}/AGL.framework")
        file(WRITE "${CMAKE_BINARY_DIR}/agl_stub.cpp" "")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -dynamiclib -o "${AGL_STUB_DIR}/AGL.framework/AGL" "${CMAKE_BINARY_DIR}/agl_stub.cpp"
            RESULT_VARIABLE AGL_STUB_RESULT
        )
        if(NOT AGL_STUB_RESULT EQUAL 0)
            message(WARNING "qurcuma: failed to build AGL.framework stub; link may fail on macOS SDKs without AGL")
        endif()
    endif()
    add_link_options("-F${AGL_STUB_DIR}")
endif()

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

# Claude Generated 2026 - GuiPrivate (needed for the QRhi-based high-res image export,
# src/view.cpp) is requested separately and non-fatally: official Qt installer builds
# (aqt/install-qt-action, used by CI) don't ship a standalone Qt6GuiPrivateConfig.cmake,
# only distro packages (e.g. Arch, used locally) do. The private headers themselves are
# present in both cases -- only the small CMake package descriptor is sometimes missing.
# When find_package can't locate it, build the INTERFACE IMPORTED target by hand from
# Qt6Gui's own include dir, using the same <QtGui include>/<version>/QtGui layout that
# Qt6GuiPrivateConfig.cmake would have pointed at.
find_package(Qt6GuiPrivate QUIET)
if(NOT TARGET Qt6::GuiPrivate)
    message(STATUS "qurcuma: Qt6GuiPrivateConfig.cmake not found - constructing Qt6::GuiPrivate manually")
    # Qt6Gui_INCLUDE_DIRS is a list (QtGui dir, QtCore dir, ...) -- only the first
    # entry is the QtGui-specific include dir the private headers live under.
    list(GET Qt6Gui_INCLUDE_DIRS 0 _qt6gui_include_dir)
    add_library(Qt6::GuiPrivate INTERFACE IMPORTED)
    target_include_directories(Qt6::GuiPrivate INTERFACE
        ${_qt6gui_include_dir}/${Qt6Gui_VERSION}
        ${_qt6gui_include_dir}/${Qt6Gui_VERSION}/QtGui
    )
    unset(_qt6gui_include_dir)
endif()

option(USE_SFTP "Enable SFTP remote file access (requires libssh)" OFF)
option(USE_LLM "Enable LLM support: OpenAI-compatible endpoints (needs Qt6::Network)" ON)
if(USE_SFTP)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBSSH REQUIRED libssh)
    else()
        find_library(LIBSSH_LIBRARIES NAMES ssh)
        find_path(LIBSSH_INCLUDE_DIRS libssh/libssh.h)
        if(NOT LIBSSH_LIBRARIES OR NOT LIBSSH_INCLUDE_DIRS)
            message(FATAL_ERROR "libssh not found. Install libssh-dev (Debian/Ubuntu) or libssh-devel (Fedora/RHEL)")
        endif()
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
    GIT_TAG        master
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

# Claude Generated 2026 - CuteChart's own demo executable ("example") is not needed
# by qurcuma (only the cutechart library is linked, see further below) and fails to
# link on current macOS CI runners: `ld: framework 'AGL' not found` -- AGL is a
# pre-Metal OpenGL framework Apple finally dropped from recent macOS SDKs (Xcode 26),
# and CuteChart's Qt Charts dependency chain still probes for it. Exclude it from the
# default build the same way curcuma's own demo executables are excluded above.
if(TARGET example)
    set_target_properties(example PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

# Claude Generated 2026 - curcuma's own CMakeLists.txt unconditionally adds its full
# test suite (test_cases/, incl. sqm_reference). qurcuma only links curcuma_core/
# curcuma_cap and never needs those test executables; building them wastes CI time
# and at least one of them (test_xtb_coulomb.cpp) compiles pathologically slowly
# with Apple Clang, which is what stalls the macOS CI job. Excluding the whole
# directory from the default `all` target is supported retroactively by CMake
# (>=3.18) even though add_subdirectory(test_cases) already ran inside curcuma's
# own CMakeLists.txt.
set_property(DIRECTORY ${curcuma_SOURCE_DIR}/test_cases PROPERTY EXCLUDE_FROM_ALL TRUE)

# Quelldateien
set(SOURCES
    src/view.cpp
    src/viewpresetwidget.cpp  # Claude Generated 2026 - view preset UI
    src/main.cpp
    src/mainwindow.cpp
    src/settings.cpp
    src/vtfparser.cpp
    src/xyzparser.cpp
    src/moleculefileloader.cpp  # Claude Generated 2026 - unified structure-file reader
    src/llm/tools_view.cpp  # Claude Generated 2026 - read-only tools over the viewer
    src/llm/tools_palette.cpp  # Claude Generated 2026 - tools in the command palette
    $<$<BOOL:${USE_LLM}>:${CMAKE_CURRENT_SOURCE_DIR}/src/docks/chatdock.cpp>  # Claude Generated 2026 (see the SKIP_AUTOMOC note below)
    src/calculationhistory.cpp  # Claude Generated 2026 - calculations.json persistence
    src/calculationrunner.cpp  # Claude Generated 2026 - WP T3 external-process orchestration
    src/modifiabletextedit.cpp
    src/displaypanel.cpp  # Claude Generated 2026 - docked viewer display options
    src/widgets/collapsiblesection.cpp  # Claude Generated 2026 - accordion section
    src/widgets/elementpicker.cpp  # Claude Generated 2026 - builder element strip + periodic table
    src/widgets/commandpalette.cpp  # Claude Generated 2026 - P3 Ctrl+K command palette
    src/widgets/temperatureslider.cpp  # Claude Generated 2026 - vertical temperature-colored slider
    src/widgets/simulationchart.cpp  # Claude Generated 2026 - live MD temperature/energy charts
    src/dialogs/nmrspectrumdialog.cpp
    src/dialogs/nmrcontroller.cpp
    src/dialogs/nmrdatastore.cpp
    src/dialogs/nmrstructureproxymodel.cpp
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
    src/docks/dockmanager.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/outputdock.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/simulationdock.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/displaydock.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/bookmarkwidget.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/workspacepanel.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/remotedirectoriespanel.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/projectdock.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/imagegallerydock.cpp  # Claude Generated 2026 - batch border-trim gallery
    src/docks/chartdock.cpp  # Claude Generated 2026 - live charts dock
    src/docks/ncidock.cpp  # Claude Generated 2026 - non-covalent interaction dock
    src/nciwidget.cpp  # Claude Generated 2026 - non-covalent interaction contact table
    src/ncianalysisworker.cpp  # Claude Generated 2026 - off-thread NCI analysis (GFN-FF/GFN2)
    src/imagecrop.cpp  # Claude Generated 2026 - image border-trim analysis (metadata-preserving)
    src/forceinjector.cpp  # Claude Generated 2026 - Topological force distribution (Phase 4)
    src/ncianalysis.cpp  # Claude Generated 2026 - non-covalent interaction detection
    src/elementdata.cpp  # Claude Generated 2026 - Quick3D renderer: shared element tables
    src/buildtools.cpp  # Claude Generated 2026 - builder valence accounting + auto-H placement
    src/fragmentlibrary.cpp  # Claude Generated 2026 - built-in builder fragments
    src/scenefiller.cpp  # Claude Generated 2026 - pack fragment copies into a container
    src/measurements.cpp  # Claude Generated 2026 - distance/angle/dihedral + histogram binning
    src/dialogs/fillcontainerdialog.cpp  # Claude Generated 2026 - fill-container dialog
    src/atominstancing.cpp  # Claude Generated 2026 - Quick3D renderer: atom instancing
    src/bondinstancing.cpp  # Claude Generated 2026 - Quick3D renderer: bond instancing
    src/scenecontroller.cpp  # Claude Generated 2026 - Quick3D renderer: scene view-model
    src/lesson.cpp  # Claude Generated 2026 - OER teaching scenarios (Lesson model + JSON)
    src/lessonstructuremodel.cpp  # Claude Generated 2026 - in-memory lesson structure list model
    src/lessoncontroller.cpp  # Claude Generated 2026 - WP T4 lesson feature controller
    src/dialogs/lessonmetadatadialog.cpp  # Claude Generated 2026 - lesson metadata editor
)

# Header-Dateien
set(HEADERS
    src/mainwindow.h
    src/settings.h
    src/view.h
    src/vtfparser.h
    src/xyzparser.h
    src/moleculefileloader.h  # Claude Generated 2026 - unified structure-file reader
    src/llm/tools_view.h  # Claude Generated 2026 - read-only tools over the viewer
    src/llm/tools_palette.h  # Claude Generated 2026 - tools in the command palette
    src/calculationhistory.h  # Claude Generated 2026 - calculations.json persistence
    src/calculationrunner.h  # Claude Generated 2026 - WP T3 external-process orchestration
    src/modifiabletextedit.h
    src/displaypanel.h  # Claude Generated 2026 - docked viewer display options
    src/widgets/collapsiblesection.h  # Claude Generated 2026 - accordion section
    src/widgets/elementpicker.h  # Claude Generated 2026 - builder element strip + periodic table
    src/widgets/commandpalette.h  # Claude Generated 2026 - P3 Ctrl+K command palette
    src/widgets/temperatureslider.h  # Claude Generated 2026 - vertical temperature-colored slider
    src/widgets/simulationchart.h  # Claude Generated 2026 - live MD temperature/energy charts
    src/dialogs/nmrspectrumdialog.h
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
    src/lesson.h  # Claude Generated 2026 - OER teaching scenarios (Lesson model + JSON)
    src/lessonstructuremodel.h  # Claude Generated 2026 - in-memory lesson structure list model (Q_OBJECT)
    src/lessoncontroller.h  # Claude Generated 2026 - WP T4 lesson feature controller (Q_OBJECT)
    src/dialogs/lessonmetadatadialog.h  # Claude Generated 2026 - lesson metadata editor (Q_OBJECT)
    src/dialogs/fillcontainerdialog.h  # Claude Generated 2026 - fill-container dialog (Q_OBJECT)
    #src/dialogs/nmrstructuremodel.h

)

if(USE_SFTP)
    list(APPEND SOURCES
        src/dialogs/sftpdialog.cpp
        src/sshconfig.cpp
        src/sftpcache.cpp
    )
    list(APPEND HEADERS
        src/dialogs/sftpdialog.h
        src/sftpmodel.hpp
        src/sshconfig.h
        src/sftpcache.h
    )
endif()

# --- qurcuma_core: the widget-free core -------------------------------------
# Claude Generated 2026 - The tool registry, the log hub and the molecule records.
# Linking ONLY Qt6::Core and Qt6::Gui is the point of this target, not an accident:
# it is what lets ctest drive the registry headlessly and turns "GUI-free" into a
# property the build checks rather than a claim in a comment. Qt6::Gui is needed
# for QVector3D and QColor in the data records; Widgets and Quick3D must stay out.
#
# No curcuma here yet, so qurcuma_match_curcuma_simd() is deliberately NOT applied:
# there is no Eigen type crossing this boundary. The moment a core TU includes a
# curcuma header, add the include dirs AND that call together.
add_library(qurcuma_core STATIC
    src/core/loghub.cpp
    src/core/loghub.h
    src/core/toolregistry.cpp
    src/core/toolregistry.h
    src/core/tooldispatcher.cpp
    src/core/tooldispatcher.h
    src/core/tools_core.cpp
    src/core/tools_core.h
    src/core/tool.h
    src/core/moleculedata.h
)
target_link_libraries(qurcuma_core PUBLIC Qt6::Core Qt6::Gui)
target_include_directories(qurcuma_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- qurcuma_llm: the endpoint client ---------------------------------------
# Claude Generated 2026 - Separate from qurcuma_core on purpose. The core's
# verified property is that it links only Qt6::Core and Qt6::Gui; putting the
# network stack in there would quietly end that. This target adds Qt6::Network and
# nothing else, so it stays testable headlessly against a local stub server.
if(USE_LLM)
    find_package(Qt6 COMPONENTS Network REQUIRED)
    add_library(qurcuma_llm STATIC
        src/llm/llmconfig.cpp
        src/llm/llmconfig.h
        src/llm/llmclient.cpp
        src/llm/llmclient.h
        src/llm/llmsession.cpp
        src/llm/llmsession.h
    )
    target_link_libraries(qurcuma_llm PUBLIC qurcuma_core Qt6::Network)
    target_include_directories(qurcuma_llm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

add_executable(qurcuma
    ${SOURCES}
    ${HEADERS}
    resources.qrc
)

# Claude Generated 2026 - AUTOMOC does not evaluate preprocessor conditionals: it
# finds chatdock.h through the #ifdef USE_LLM include in dockmanager.cpp, generates
# moc_chatdock.cpp for it, and the link then fails on a ChatDock constructor whose
# .cpp was excluded from the build. Tell it to leave the header alone in that case.
if(NOT USE_LLM)
    set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/docks/chatdock.h PROPERTY SKIP_AUTOMOC ON)
endif()
# Switching USE_LLM in an existing build tree needs the autogen directory cleared
# (rm -rf <build>/qurcuma_autogen): AUTOMOC keeps its previous verdict about the
# header and the link then fails the other way round, on a missing staticMetaObject.

target_compile_definitions(qurcuma PRIVATE
    $<$<BOOL:${USE_SFTP}>:USE_SFTP>
    $<$<BOOL:${USE_LLM}>:USE_LLM>
    QURCUMA_VERSION="${PROJECT_VERSION}")

# Claude Generated 2026 - Match curcuma's CPU/SIMD flags on a target.
#
# curcuma (FetchContent, above) compiles its TUs with -march=native (or AVX-512)
# when USE_MARCH_NATIVE/USE_AVX512 is set, but that set(CMAKE_CXX_FLAGS ...) lives
# in curcuma's subdirectory scope and does NOT reach parent-scope targets.
# The mismatch changes Eigen's EIGEN_MAX_ALIGN_BYTES (16 for SSE2 here vs 64 for
# AVX-512), so a curcuma-allocated Eigen Geometry copied/freed inside a qurcuma TU
# (e.g. moleculeToFrame: `Geometry geo = mol.getGeometry();`) hits mismatched
# aligned-alloc bookkeeping -> "double free or corruption" the instant the
# interactive MD/Opt starts (Release-only; Debug builds both sides SSE2 and works).
#
# A function rather than a copied block: EVERY parent-scope target that ever sees a
# curcuma header has to carry these flags, and a copied block is the kind of thing
# that gets added to three targets and forgotten on the fourth.
include(CheckCXXCompilerFlag)
function(qurcuma_match_curcuma_simd target)
    if(USE_MARCH_NATIVE)
        check_cxx_compiler_flag("-march=native" QURCUMA_SUPPORTS_MARCH_NATIVE)
        if(QURCUMA_SUPPORTS_MARCH_NATIVE)
            target_compile_options(${target} PRIVATE -march=native)
            message(STATUS "${target}: matching curcuma -march=native (Eigen ABI alignment)")
        endif()
    elseif(USE_AVX512)
        target_compile_options(${target} PRIVATE -mavx512f -mavx512vl -mavx512dq -mfma)
        message(STATUS "${target}: matching curcuma AVX-512 flags (Eigen ABI alignment)")
    elseif(USE_AVX2)
        target_compile_options(${target} PRIVATE -mavx2 -mfma)
        message(STATUS "${target}: matching curcuma AVX2 flags (Eigen ABI alignment)")
    endif()
endfunction()

qurcuma_match_curcuma_simd(qurcuma)

# Claude Generated - OpenMP needed by curcuma_core
find_package(OpenMP)
target_link_libraries(qurcuma PRIVATE
qurcuma_core  # Claude Generated 2026 - tool registry + log hub
$<$<BOOL:${USE_LLM}>:qurcuma_llm>  # Claude Generated 2026 - endpoint client
Qt6::Core
Qt6::Gui
Qt6::GuiPrivate
Qt6::Widgets
Qt6::Charts
Qt6::Qml
Qt6::Quick
Qt6::Quick3D
cutechart
$<$<BOOL:${USE_SFTP}>:${LIBSSH_LIBRARIES}>
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
    $<$<BOOL:${USE_SFTP}>:${LIBSSH_INCLUDE_DIRS}>
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

# NCI Detection Test - Claude Generated 2026
add_executable(test_nci test_nci.cpp src/ncianalysis.cpp src/forceinjector.cpp)
target_link_libraries(test_nci PRIVATE
Qt6::Core
Qt6::Widgets
)
target_include_directories(test_nci PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
)

# Fragment Decomposition Test - Claude Generated 2026
# Needs Quick3D only because SceneController owns the instancing objects; no window
# is created (offscreen platform is enough).
add_executable(test_fragments test_fragments.cpp src/scenecontroller.cpp
    src/atominstancing.cpp src/bondinstancing.cpp src/elementdata.cpp src/ncianalysis.cpp
    src/forceinjector.cpp)
target_link_libraries(test_fragments PRIVATE
Qt6::Core
Qt6::Gui
Qt6::Widgets
Qt6::Quick3D
)
target_include_directories(test_fragments PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)

# Builder auto-hydrogen test - Claude Generated 2026
# Header-only use of MoleculeViewer's Atom/Bond types; no GUI, no Quick3D.
# measurements.cpp is a thin adapter over curcuma's GeometryTools / RMSDFunctions,
# so the test needs curcuma's headers and library.
add_executable(test_measurements test_measurements.cpp src/measurements.cpp)
target_link_libraries(test_measurements PRIVATE
Qt6::Core
Qt6::Gui
curcuma_core
)
target_include_directories(test_measurements PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)
add_test(NAME measurements COMMAND test_measurements)
set_tests_properties(measurements PROPERTIES LABELS qurcuma)

# LogHub test - Claude Generated 2026
# LogHub is QtCore-only by design (it has to link into a widget-free core), so this
# target needs neither Gui nor curcuma. Threads::Threads is explicit because the
# test appends from several std::threads to pin the locking.
find_package(Threads REQUIRED)
add_executable(test_loghub test_loghub.cpp)
target_link_libraries(test_loghub PRIVATE
qurcuma_core
Threads::Threads
)
target_include_directories(test_loghub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME loghub COMMAND test_loghub)
set_tests_properties(loghub PROPERTIES LABELS qurcuma)

# ToolRegistry test - Claude Generated 2026
add_executable(test_toolregistry test_toolregistry.cpp)
target_link_libraries(test_toolregistry PRIVATE
qurcuma_core
)
add_test(NAME toolregistry COMMAND test_toolregistry)
set_tests_properties(toolregistry PROPERTIES LABELS qurcuma)

# ToolDispatcher test - Claude Generated 2026
# Needs Threads because it dispatches from a real second thread; that is the whole
# point of the target.
add_executable(test_tooldispatcher test_tooldispatcher.cpp)
target_link_libraries(test_tooldispatcher PRIVATE
qurcuma_core
Threads::Threads
)
add_test(NAME tooldispatcher COMMAND test_tooldispatcher)
set_tests_properties(tooldispatcher PROPERTIES LABELS qurcuma)

# Core tools test - Claude Generated 2026
add_executable(test_toolscore test_toolscore.cpp)
target_link_libraries(test_toolscore PRIVATE
qurcuma_core
)
add_test(NAME toolscore COMMAND test_toolscore)
set_tests_properties(toolscore PROPERTIES LABELS qurcuma)

# LLM client/config test - Claude Generated 2026
# Talks to a QTcpServer stub inside the test process. ctest never touches a network.
if(USE_LLM)
    add_executable(test_llmclient test_llmclient.cpp)
    target_link_libraries(test_llmclient PRIVATE
    qurcuma_llm
    )
    add_test(NAME llmclient COMMAND test_llmclient)
    set_tests_properties(llmclient PROPERTIES LABELS qurcuma)

    add_executable(test_llmsession test_llmsession.cpp)
    target_link_libraries(test_llmsession PRIVATE
    qurcuma_llm
    )
    add_test(NAME llmsession COMMAND test_llmsession)
    set_tests_properties(llmsession PROPERTIES LABELS qurcuma)
endif()

add_executable(test_scenefiller test_scenefiller.cpp src/scenefiller.cpp src/buildtools.cpp
    src/elementdata.cpp src/fragmentlibrary.cpp)
target_link_libraries(test_scenefiller PRIVATE
Qt6::Core
Qt6::Gui
Qt6::Widgets
)
target_include_directories(test_scenefiller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)
add_test(NAME scenefiller COMMAND test_scenefiller)
set_tests_properties(scenefiller PROPERTIES LABELS qurcuma)

add_executable(test_buildtools test_buildtools.cpp src/buildtools.cpp src/elementdata.cpp
    src/fragmentlibrary.cpp)
target_link_libraries(test_buildtools PRIVATE
Qt6::Core
Qt6::Gui
Qt6::Widgets
)
target_include_directories(test_buildtools PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)

# NCI GFN-FF Integration Test - Claude Generated 2026
# Links curcuma so the analysis worker's real curcuma path is exercised.
add_executable(test_nci_gfnff test_nci_gfnff.cpp src/ncianalysisworker.cpp src/ncianalysis.cpp
    src/forceinjector.cpp src/xyzparser.cpp src/elementdata.cpp)
target_link_libraries(test_nci_gfnff PRIVATE
Qt6::Core
Qt6::Widgets
curcuma_core
curcuma_cap
)
# Mirrors the qurcuma target's include list: the analysis worker pulls in the
# same curcuma headers, which reach for curcuma's own dependencies.
target_include_directories(test_nci_gfnff PRIVATE
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
    ${CMAKE_CURRENT_SOURCE_DIR}/external/CxxThreadPool/include
)
if(USE_MARCH_NATIVE)
    target_compile_options(test_nci_gfnff PRIVATE -march=native)
elseif(USE_AVX512)
    target_compile_options(test_nci_gfnff PRIVATE -mavx512f -mavx512vl -mavx512dq -mfma)
elseif(USE_AVX2)
    target_compile_options(test_nci_gfnff PRIVATE -mavx2 -mfma)
endif()

# Claude Generated 2026 - install()/CPack were never configured: the CI workflow's
# "Package with CPack" step (windows/macos, `cpack -G ZIP`) always failed with
# "Cannot find CPack config file" because include(CPack) was simply never called.
# This wires up a minimal package: the qurcuma binary plus its Qt runtime
# dependencies (DLLs/frameworks and, critically, the QML/platform plugins that a
# plain link-dependency walk misses -- windeployqt/macdeployqt understand Qt's
# plugin system, generic tools don't).
if(APPLE)
    # MACOSX_BUNDLE is the idiomatic packaging for a Qt GUI app on macOS and is
    # what macdeployqt expects to deploy frameworks/plugins into.
    set_target_properties(qurcuma PROPERTIES MACOSX_BUNDLE TRUE)
endif()

install(TARGETS qurcuma
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION .
)

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --qmldir \"${CMAKE_SOURCE_DIR}/src/qml\" \"\${CMAKE_INSTALL_PREFIX}/qurcuma.exe\")
        ")
    else()
        message(WARNING "qurcuma: windeployqt not found - packaged ZIP will be missing Qt runtime DLLs")
    endif()
elseif(APPLE)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BIN_DIR}")
    if(MACDEPLOYQT_EXECUTABLE)
        install(CODE "
            execute_process(COMMAND \"${MACDEPLOYQT_EXECUTABLE}\" \"\${CMAKE_INSTALL_PREFIX}/qurcuma.app\" -qmldir=\"${CMAKE_SOURCE_DIR}/src/qml\")
        ")
    else()
        message(WARNING "qurcuma: macdeployqt not found - packaged ZIP will be missing the Qt runtime")
    endif()
endif()

set(CPACK_PACKAGE_NAME "qurcuma")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "qurcuma-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
include(CPack)
