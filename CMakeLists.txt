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
QuickWidgets  # Claude Generated 2026 - QQuickWidget viewport (no native-window stacking over docks)
REQUIRED
)

# Claude Generated 2026 - OpenXR output (Qt Quick 3D XR, Qt >= 6.8). Optional: without the
# module the VR menu entry is not built.
find_package(Qt6 6.8 QUIET OPTIONAL_COMPONENTS Quick3DXr)

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

# Claude Generated 2026 - SFTP (File > Open Remote File...) is on by default; without libssh it is
# switched off with a notice instead of stopping the configuration (CI machines, Windows).
option(USE_SFTP "Enable SFTP remote file access (needs libssh; switched off when libssh is not found)" ON)
option(USE_LLM "Enable LLM support: OpenAI-compatible endpoints (needs Qt6::Network)" ON)
if(USE_SFTP)
    set(_libssh_found FALSE)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBSSH libssh)
        if(LIBSSH_FOUND)
            set(_libssh_found TRUE)
        endif()
    else()
        find_library(LIBSSH_LIBRARIES NAMES ssh)
        find_path(LIBSSH_INCLUDE_DIRS libssh/libssh.h)
        if(LIBSSH_LIBRARIES AND LIBSSH_INCLUDE_DIRS)
            set(_libssh_found TRUE)
        endif()
    endif()
    if(NOT _libssh_found)
        message(WARNING "libssh not found: SFTP (Open Remote File) is disabled. Install libssh-dev (Debian/Ubuntu), libssh-devel (Fedora/RHEL) or libssh (Arch) to enable it.")
        set(USE_SFTP OFF)
    endif()
    unset(_libssh_found)
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

# Claude Generated 2026 - qurcuma follows curcuma's master (reactive GFN-FF API, ParameterRegistry
# tiers and relevantWhen for the All parameters tab). Only a fresh clone reads this line: with the
# local checkout present FetchContent is disconnected below.
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
    src/remote/hostprofiles.cpp  # Claude Generated 2026 - the computer list (Qt core only), read by the remote dock and by SFTP
    src/remotepaths.cpp  # Claude Generated 2026 - path/listing helpers of the remote browser, breadcrumb split (Qt core only)
    src/vtfparser.cpp
    src/xyzparser.cpp
    src/moleculefileloader.cpp  # Claude Generated 2026 - unified structure-file reader
    src/llm/atomselection.cpp  # Claude Generated 2026 - shared selection grammar
    src/llm/tools_view.cpp  # Claude Generated 2026 - read-only tools over the viewer
    src/llm/curcumajob.cpp  # Claude Generated 2026 - in-process curcuma calculation
    src/llm/tools_compute.cpp  # Claude Generated 2026
    src/llm/tools_edit.cpp  # Claude Generated 2026 - structure editing
    src/llm/tools_files.cpp  # Claude Generated 2026 - merge from / save to disk
    src/llm/tools_simulation.cpp  # Claude Generated 2026 - interactive MD / opt
    src/llm/curcuma_schemas.cpp  # Claude Generated 2026
    src/llm/tools_palette.cpp  # Claude Generated 2026 - tools in the command palette
    $<$<BOOL:${USE_LLM}>:${CMAKE_CURRENT_SOURCE_DIR}/src/docks/qurcumachathost.cpp>  # Claude Generated 2026 - the ChatHost of CuteChat's ChatDock
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
    src/simulationbackend.cpp  # Claude Generated 2026 - LocalBackend (WP remote compute R0)
    src/simulationcontrolwidget.cpp  # Claude Generated - Interactive Simulation Integration
    src/simulationparameterswidget.cpp  # Claude Generated 2026 - All parameters tab (registry-generated)
    src/snapshotswidget.cpp  # Claude Generated 2026 - Snapshot history foundation
    src/rmsdwidget.cpp  # Claude Generated 2026 - RMSD / align tool (Analysis dock)
    src/docks/dockmanager.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/outputdock.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/simulationdock.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/structuredock.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/appearancedock.cpp  # Claude Generated 2026 - UX stage 4
    src/docks/bookmarkwidget.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/workspacepanel.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/projectdock.cpp  # Claude Generated 2026 - Dock system restructuring
    src/docks/directoryfilter.cpp  # Claude Generated 2026 - file filter shared by the local and the remote file list
    src/docks/imagegallerydock.cpp  # Claude Generated 2026 - batch border-trim gallery
    src/docks/chartdock.cpp  # Claude Generated 2026 - live charts dock
    src/docks/scriptdock.cpp  # Claude Generated 2026 - script editor + run (not USE_LLM-gated)
    src/docks/ncidock.cpp  # Claude Generated 2026 - non-covalent interaction dock
    src/docks/celldock.cpp  # Claude Generated 2026 - cif unit cell + repeats dock
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
    src/tableinstancing.cpp  # Claude Generated 2026 - instance-table base + mirror for the headset scene
    src/atominstancing.cpp  # Claude Generated 2026 - Quick3D renderer: atom instancing
    src/bondinstancing.cpp  # Claude Generated 2026 - Quick3D renderer: bond instancing
    src/scenecontroller.cpp  # Claude Generated 2026 - Quick3D renderer: scene view-model
    src/lesson.cpp  # Claude Generated 2026 - OER teaching scenarios (Lesson model + JSON)
    src/lessonstructuremodel.cpp  # Claude Generated 2026 - in-memory lesson structure list model
    src/lessoncontroller.cpp  # Claude Generated 2026 - WP T4 lesson feature controller
    src/dialogs/lessonmetadatadialog.cpp  # Claude Generated 2026 - lesson metadata editor
    src/programpaths.cpp  # Claude Generated 2026 - external program table, path checks, ORCID check
    src/dialogs/settingsdialog.cpp  # Claude Generated 2026 - Settings dialog (programs + operator)
)

# Header-Dateien
set(HEADERS
    src/mainwindow.h
    src/settings.h
    src/remote/hostprofiles.h
    src/remotepaths.h  # Claude Generated 2026
    src/view.h
    src/vtfparser.h
    src/xyzparser.h
    src/moleculefileloader.h  # Claude Generated 2026 - unified structure-file reader
    src/llm/atomselection.h  # Claude Generated 2026 - shared selection grammar
    src/llm/tools_view.h  # Claude Generated 2026 - read-only tools over the viewer
    src/llm/curcumajob.h  # Claude Generated 2026 - in-process curcuma calculation
    src/llm/tools_compute.h  # Claude Generated 2026
    src/llm/tools_edit.h  # Claude Generated 2026 - structure editing
    src/llm/tools_files.h  # Claude Generated 2026 - merge from / save to disk
    src/llm/tools_simulation.h  # Claude Generated 2026 - interactive MD / opt
    src/llm/curcuma_schemas.h  # Claude Generated 2026
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
    src/simulationbackend.h
    src/moleculetypes.h
    src/simulationcontrolwidget.h  # Claude Generated - Interactive Simulation Integration
    src/simulationparameterswidget.h  # Claude Generated 2026 - All parameters tab (Q_OBJECT)
    src/snapshotswidget.h  # Claude Generated 2026 - Snapshot history foundation
    src/rmsdwidget.h  # Claude Generated 2026 - RMSD / align tool (Analysis dock)
    src/forceinjector.h  # Claude Generated 2026 - Topological force distribution (Phase 4)
    src/elementdata.h  # Claude Generated 2026 - Quick3D renderer: shared element tables
    src/tableinstancing.h  # Claude Generated 2026 - instance-table base + mirror for the headset scene
    src/atominstancing.h  # Claude Generated 2026 - Quick3D renderer: atom instancing
    src/bondinstancing.h  # Claude Generated 2026 - Quick3D renderer: bond instancing
    src/scenecontroller.h  # Claude Generated 2026 - Quick3D renderer: scene view-model
    src/lesson.h  # Claude Generated 2026 - OER teaching scenarios (Lesson model + JSON)
    src/lessonstructuremodel.h  # Claude Generated 2026 - in-memory lesson structure list model (Q_OBJECT)
    src/lessoncontroller.h  # Claude Generated 2026 - WP T4 lesson feature controller (Q_OBJECT)
    src/dialogs/lessonmetadatadialog.h  # Claude Generated 2026 - lesson metadata editor (Q_OBJECT)
    src/dialogs/fillcontainerdialog.h  # Claude Generated 2026 - fill-container dialog (Q_OBJECT)
    src/programpaths.h  # Claude Generated 2026 - external program table, path checks, ORCID check
    src/dialogs/settingsdialog.h  # Claude Generated 2026 - Settings dialog (Q_OBJECT)
    #src/dialogs/nmrstructuremodel.h

)

if(USE_SFTP)
    # Claude Generated 2026 - remote browser of the Project dock (Remote segment): the libssh session
    # runs in SftpClient on its own thread, RemoteBrowserPanel is the widget.
    list(APPEND SOURCES
        src/sftpclient.cpp
        src/docks/remotebrowserpanel.cpp
        src/sshconfig.cpp
        src/sftpcache.cpp
    )
    list(APPEND HEADERS
        src/sftpclient.h
        src/docks/remotebrowserpanel.h
        src/sshconfig.h
        src/sftpcache.h
    )
endif()

# Claude Generated 2026 - Remote compute (docs/WP-remote-compute-vr.md): the GUI side
# (RemoteBackend, SshTunnel, "Compute on" in the Simulation dock) needs Qt6::WebSockets.
option(USE_REMOTE "Build remote compute support (needs Qt6::WebSockets)" ON)
find_package(Qt6 COMPONENTS WebSockets QUIET)
# remotelog is Qt Core only and the viewer writes its timings through it, so it is always built.
list(APPEND SOURCES src/remote/remotelog.cpp)
list(APPEND HEADERS src/remote/remotelog.h)
if(USE_REMOTE AND TARGET Qt6::WebSockets)
    set(QURCUMA_REMOTE ON)
    list(APPEND SOURCES
        src/remote/protocol.cpp
        src/remote/remotebackend.cpp
        src/remote/hostprobe.cpp
        src/remote/sshtunnel.cpp
        src/dialogs/addcomputerdialog.cpp)
    list(APPEND HEADERS
        src/remote/protocol.h
        src/remote/remotebackend.h
        src/remote/sshtunnel.h
        src/remote/hostprobe.h
        src/dialogs/addcomputerdialog.h)
endif()

# --- CuteChat: the tool layer, the endpoint client, the agent loop and the chat log ----------------
# Claude Generated 2026 - Shared with SupraFit (docs/DESIGN.md in the CuteChat repo). A git submodule at
# external/CuteChat (git submodule update --init); -DCUTECHAT_DIR=<checkout> or a sibling checkout ../CuteChat
# override it for working on the library. Only the Qt-Core-only tool layer (CuteChat::tools) goes into
# qurcuma_core; the client, session and config (CuteChat::core, needs Qt6::Network) into qurcuma_llm.
set(CUTECHAT_DIR "" CACHE PATH "CuteChat checkout (default: external/CuteChat, then ../CuteChat)")
if(NOT CUTECHAT_DIR)
    foreach(_candidate ${CMAKE_CURRENT_SOURCE_DIR}/external/CuteChat ${CMAKE_CURRENT_SOURCE_DIR}/../CuteChat)
        if(EXISTS ${_candidate}/CMakeLists.txt)
            set(CUTECHAT_DIR ${_candidate})
            break()
        endif()
    endforeach()
endif()
if(NOT CUTECHAT_DIR OR NOT EXISTS ${CUTECHAT_DIR}/CMakeLists.txt)
    message(FATAL_ERROR "qurcuma needs the CuteChat library: run `git submodule update --init` "
                        "(external/CuteChat), or set -DCUTECHAT_DIR=<path to CuteChat> (../CuteChat is tried as well)")
endif()
message(STATUS "qurcuma: CuteChat from ${CUTECHAT_DIR}")
set(CUTECHAT_BUILD_WIDGETS ON)    # the chat dock and the endpoint dialog: qurcuma's assistant is CuteChat's ChatDock
set(CUTECHAT_BUILD_TESTS OFF)     # the library's own tests run in its repo; qurcuma's test_* cover its use of it
set(CUTECHAT_INSTALL OFF)
add_subdirectory(${CUTECHAT_DIR} ${CMAKE_BINARY_DIR}/external/CuteChat)

# --- qurcuma_core: the widget-free core -------------------------------------
# Claude Generated 2026 - The log hub, the core tools and the molecule records; the tool registry and
# dispatcher come from CuteChat::tools (Qt Core only).
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
    src/core/tools_core.cpp
    src/core/tools_core.h
    src/core/moleculedata.h
)
target_link_libraries(qurcuma_core PUBLIC Qt6::Core Qt6::Gui CuteChat::tools)
target_include_directories(qurcuma_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- qurcuma_llm: the endpoint client (CuteChat::core) ----------------------
# Claude Generated 2026 - Separate from qurcuma_core on purpose. The core's
# verified property is that it links only Qt6::Core and Qt6::Gui; putting the
# network stack in there would quietly end that. This target adds Qt6::Network and
# nothing else, so it stays testable headlessly against a local stub server.
if(USE_LLM)
    find_package(Qt6 COMPONENTS Network REQUIRED)
    # nothing of its own any more: the client, config and session are CuteChat::core
    add_library(qurcuma_llm INTERFACE)
    target_link_libraries(qurcuma_llm INTERFACE qurcuma_core CuteChat::core)
endif()

# --- qurcuma_script: the script interpreter ----------------------------------
# Claude Generated 2026 - SupraFit's scripting engine, in qurcuma's shape: a short
# JavaScript calculation run by the application, so that numbers are computed here
# instead of in a model's head. The engine is QJSEngine, which arrives with Qt6Qml
# (already in the build for the 3D viewer), hence no new package.
#
# Deliberately NOT in qurcuma_core: that target's verified property is "links only
# Qt6::Core and Qt6::Gui", and libQt6Qml pulls Qt6Network, ICU, libproxy and libsystemd
# along with it (measured with ldd on this machine, 14.09.2026). Deliberately not
# USE_LLM-gated either: a script is a calculation, not a way to reach an endpoint, so
# a -DUSE_LLM=OFF build still gets the whole feature.
#
# Headless: Qt6::Widgets and Quick3D stay out, so ctest drives it without a display.
add_library(qurcuma_script STATIC
    src/script/scriptinterpreter.cpp
    src/script/scriptinterpreter.h
    src/script/scriptbuiltins.cpp
    src/script/scriptbuiltins.h
    src/script/scriptbridge.h
    src/script/scriptsyntax.cpp
    src/script/scriptsyntax.h
    src/script/tools_script.cpp
    src/script/tools_script.h
)
target_link_libraries(qurcuma_script PUBLIC qurcuma_core Qt6::Qml)
target_include_directories(qurcuma_script PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(qurcuma
    ${SOURCES}
    ${HEADERS}
    resources.qrc
)

if(TARGET Qt6::Quick3DXr)
    target_sources(qurcuma PRIVATE src/vrsession.cpp src/vrsession.h)  # Claude Generated 2026 - OpenXR session
    target_sources(qurcuma PRIVATE src/xrscenebridge.cpp src/xrscenebridge.h)  # Claude Generated 2026 - mirrored tables for the headset
    target_link_libraries(qurcuma PRIVATE Qt6::Quick3DXr)
    target_compile_definitions(qurcuma PRIVATE QURCUMA_HAS_XR)
endif()

target_compile_definitions(qurcuma PRIVATE
    $<$<BOOL:${USE_SFTP}>:USE_SFTP>
    $<$<BOOL:${QURCUMA_REMOTE}>:QURCUMA_REMOTE>
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
$<$<BOOL:${USE_LLM}>:CuteChat::cutechat>  # Claude Generated 2026 - ChatDock, ChatEngine, EndpointDialog
qurcuma_script  # Claude Generated 2026 - script interpreter (QJSEngine)
Qt6::Core
Qt6::Gui
Qt6::GuiPrivate
Qt6::Widgets
Qt6::Charts
Qt6::Qml
Qt6::Quick
Qt6::Quick3D
Qt6::QuickWidgets
$<$<BOOL:${QURCUMA_REMOTE}>:Qt6::WebSockets>
cutechart
$<$<BOOL:${USE_SFTP}>:${LIBSSH_LIBRARIES}>
curcuma_core  # Claude Generated - Curcuma simulation backend
curcuma_cap   # Claude Generated - Curcuma capabilities (MD, Opt)
$<$<BOOL:${OpenMP_CXX_FOUND}>:OpenMP::OpenMP_CXX>  # Claude Generated - OpenMP for curcuma
)

# Claude Generated 2026 - curcuma's GPU backends are dlopen plugins
# (libcurcuma_<backend>.so) whose core symbols (NativeXtbMethod, GFNFF,
# CurcumaLogger, ...) resolve against the host executable. curcuma adds
# -rdynamic only for its own binary, so export them from qurcuma as well.
set_target_properties(qurcuma PROPERTIES ENABLE_EXPORTS ON)

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
#add_executable(test_vtf_minimal tests/test_vtf_minimal.cpp)
#target_link_libraries(test_vtf_minimal PRIVATE
#Qt6::Core
#)
#target_include_directories(test_vtf_minimal PRIVATE
#    ${CMAKE_CURRENT_SOURCE_DIR}/src
#    ${CMAKE_CURRENT_SOURCE_DIR}/external/
#)

# VTF Bond Test - Claude Generated
add_executable(test_vtf_bonds tests/test_vtf_bonds.cpp)
target_link_libraries(test_vtf_bonds PRIVATE
Qt6::Core
)
target_include_directories(test_vtf_bonds PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
)

# VTF Frame Test - Claude Generated
add_executable(test_vtf_frames tests/test_vtf_frames.cpp)
target_link_libraries(test_vtf_frames PRIVATE
Qt6::Core
)
target_include_directories(test_vtf_frames PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
)

# VTF Full Test - Claude Generated
add_executable(test_vtf_full tests/test_vtf_full.cpp)
target_link_libraries(test_vtf_full PRIVATE
Qt6::Core
)
target_include_directories(test_vtf_full PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
)

# NCI Detection Test - Claude Generated 2026
add_executable(test_nci tests/test_nci.cpp src/ncianalysis.cpp src/forceinjector.cpp)
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
add_executable(test_fragments tests/test_fragments.cpp src/scenecontroller.cpp
    src/tableinstancing.cpp src/atominstancing.cpp src/bondinstancing.cpp src/elementdata.cpp
    src/ncianalysis.cpp src/forceinjector.cpp)
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

# Instance-table mirror test - Claude Generated 2026. Pins how the headset scene follows the
# desktop SceneController (src/tableinstancing.h, src/xrscenebridge.h): mirror count/bytes after a
# rebuild, one mirror per controller table, instance -> atom index mapping, and (offscreen OpenGL)
# that a mirror in a second window re-renders after a source rebuild and only then, and that
# src/qml/MoleculeModels.qml shows overlays switched on later with their parent's rotation.
add_executable(test_instancing_mirror test_instancing_mirror.cpp src/scenecontroller.cpp
    src/tableinstancing.cpp src/atominstancing.cpp src/bondinstancing.cpp src/xrscenebridge.cpp
    src/elementdata.cpp src/ncianalysis.cpp src/forceinjector.cpp)
target_link_libraries(test_instancing_mirror PRIVATE
Qt6::Core
Qt6::Gui
Qt6::Widgets
Qt6::Qml
Qt6::Quick
Qt6::Quick3D
)
target_include_directories(test_instancing_mirror PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)
target_compile_definitions(test_instancing_mirror PRIVATE
    QURCUMA_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/qml")
add_test(NAME instancing_mirror COMMAND test_instancing_mirror)
set_tests_properties(instancing_mirror PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Look round-trip test - Claude Generated 2026 (look.h is header-only).
# ~/.ssh/config parser (SFTP: aliases of the remote browser) - Claude Generated 2026.
add_executable(test_sshconfig tests/test_sshconfig.cpp src/sshconfig.cpp)
target_link_libraries(test_sshconfig PRIVATE Qt6::Core)
target_include_directories(test_sshconfig PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Remote browser helpers: sorting, paths, breadcrumb parts, bookmark ids, connection mapping - Claude Generated 2026.
add_executable(test_remotepaths tests/test_remotepaths.cpp src/remotepaths.cpp src/remote/hostprofiles.cpp)
target_link_libraries(test_remotepaths PRIVATE Qt6::Core)
target_include_directories(test_remotepaths PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME remotepaths COMMAND test_remotepaths)

# Program path detection/validation, ORCID check, ORCA settings-key migration - Claude Generated 2026.
add_executable(test_programpaths tests/test_programpaths.cpp src/programpaths.cpp)
target_link_libraries(test_programpaths PRIVATE Qt6::Core)
target_include_directories(test_programpaths PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME programpaths COMMAND test_programpaths)

# Cell-list bond detection (src/bonddetect.h) against the all-pairs loop - Claude Generated 2026.
add_executable(test_bonddetect tests/test_bonddetect.cpp)
target_link_libraries(test_bonddetect PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(test_bonddetect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(test_looks tests/test_looks.cpp)
target_link_libraries(test_looks PRIVATE
Qt6::Core
Qt6::Gui
)
target_include_directories(test_looks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Simulation settings JSON + recipes test - Claude Generated 2026 (UX stage 6).
# lesson.cpp needs only Qt; the worker/viewer types it names come from headers.
add_executable(test_recipes tests/test_recipes.cpp src/lesson.cpp)
target_link_libraries(test_recipes PRIVATE
Qt6::Core
Qt6::Gui
Qt6::Widgets
)
target_include_directories(test_recipes PRIVATE
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
add_executable(test_measurements tests/test_measurements.cpp src/measurements.cpp)
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

# Selection-grammar test - Claude Generated 2026
# Pins curcuma's FragString2Indicies: one-based, and Fn needs GetFragments() first.
add_executable(test_selectiongrammar test_selectiongrammar.cpp src/llm/atomselection.cpp)
target_link_libraries(test_selectiongrammar PRIVATE
Qt6::Core
Qt6::Gui
curcuma_core
)
qurcuma_match_curcuma_simd(test_selectiongrammar)
target_include_directories(test_selectiongrammar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)
add_test(NAME selectiongrammar COMMAND test_selectiongrammar)
set_tests_properties(selectiongrammar PROPERTIES LABELS qurcuma)

# CurcumaLogger sink test - Claude Generated 2026
# The property under test is thread-locality, so this one needs Threads.
add_executable(test_curcumalogsink test_curcumalogsink.cpp)
target_link_libraries(test_curcumalogsink PRIVATE
Qt6::Core
curcuma_core
Threads::Threads
)
qurcuma_match_curcuma_simd(test_curcumalogsink)
target_include_directories(test_curcumalogsink PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)
add_test(NAME curcumalogsink COMMAND test_curcumalogsink)
set_tests_properties(curcumalogsink PROPERTIES LABELS qurcuma)

# In-process job test - Claude Generated 2026
# Runs a real single point and a real RMSD, so the four properties the in-process
# path had to acquire are checked together: captured output, per-run state, a
# result without a file, and nothing left in the working directory.
add_executable(test_curcumajob test_curcumajob.cpp src/llm/curcumajob.cpp)
target_link_libraries(test_curcumajob PRIVATE
qurcuma_core
curcuma_core
curcuma_cap
$<$<BOOL:${OpenMP_CXX_FOUND}>:OpenMP::OpenMP_CXX>
Threads::Threads
)
qurcuma_match_curcuma_simd(test_curcumajob)
target_include_directories(test_curcumajob PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/LBFGSpp/include  # rmsd.h pulls LBFGS.h
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)
add_test(NAME curcumajob COMMAND test_curcumajob)
set_tests_properties(curcumajob PROPERTIES LABELS qurcuma)

# Schema-generation test - Claude Generated 2026
add_executable(test_curcumaschemas test_curcumaschemas.cpp src/llm/curcuma_schemas.cpp)
target_link_libraries(test_curcumaschemas PRIVATE
qurcuma_core
curcuma_core
$<$<BOOL:${OpenMP_CXX_FOUND}>:OpenMP::OpenMP_CXX>
)
qurcuma_match_curcuma_simd(test_curcumaschemas)
target_include_directories(test_curcumaschemas PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}
    ${curcuma_SOURCE_DIR}/src
    ${curcuma_SOURCE_DIR}/external/eigen-3.4.0
    ${curcuma_SOURCE_DIR}/external
    ${curcuma_BINARY_DIR}
)
add_test(NAME curcumaschemas COMMAND test_curcumaschemas)
set_tests_properties(curcumaschemas PROPERTIES LABELS qurcuma)

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

# Script interpreter test - Claude Generated 2026
# Threads because stopping a runaway script is done from a second thread, which is
# the whole point of the check.
add_executable(test_scriptinterpreter test_scriptinterpreter.cpp)
target_link_libraries(test_scriptinterpreter PRIVATE
qurcuma_script
Threads::Threads
)
add_test(NAME scriptinterpreter COMMAND test_scriptinterpreter)
set_tests_properties(scriptinterpreter PROPERTIES LABELS qurcuma)

# Script tool test - Claude Generated 2026
# Threads because the dispatcher check starts a script and interrupts it, which is the
# path the assistant's Stop button takes.
add_executable(test_scripttool test_scripttool.cpp)
target_link_libraries(test_scripttool PRIVATE
qurcuma_script
Threads::Threads
)
add_test(NAME scripttool COMMAND test_scripttool)
set_tests_properties(scripttool PROPERTIES LABELS qurcuma)

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

add_executable(test_scenefiller tests/test_scenefiller.cpp src/scenefiller.cpp src/buildtools.cpp
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

add_executable(test_buildtools tests/test_buildtools.cpp src/buildtools.cpp src/elementdata.cpp
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
add_executable(test_nci_gfnff tests/test_nci_gfnff.cpp src/ncianalysisworker.cpp src/ncianalysis.cpp
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


# ---------------------------------------------------------------------------
# Remote compute (WP remote compute R1, docs/WP-remote-compute-vr.md):
# qurcuma-server runs SimulationWorker headless for a remote qurcuma. Needs Qt6::WebSockets;
# skipped when it is not installed.
# ---------------------------------------------------------------------------
if(TARGET Qt6::WebSockets)
    add_library(qurcuma_remote STATIC
        src/remote/protocol.cpp
        src/remote/remotebackend.cpp
        src/remote/remotelog.cpp
        src/remote/hostprofiles.cpp
        src/remote/hostprobe.cpp
        src/remote/sshtunnel.cpp
        src/remote/serversession.cpp
        src/simulationbackend.cpp
        src/simulationworker.cpp
        src/forceinjector.cpp
        src/lesson.cpp)
    target_link_libraries(qurcuma_remote PUBLIC Qt6::Core Qt6::Gui Qt6::WebSockets curcuma_core curcuma_cap
        $<$<BOOL:${OpenMP_CXX_FOUND}>:OpenMP::OpenMP_CXX>)
    target_include_directories(qurcuma_remote PUBLIC
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
        ${CMAKE_CURRENT_SOURCE_DIR}/external/CxxThreadPool/include)
    # Same Eigen alignment as curcuma (see the qurcuma target above).
    if(USE_MARCH_NATIVE)
        target_compile_options(qurcuma_remote PUBLIC -march=native)
    elseif(USE_AVX512)
        target_compile_options(qurcuma_remote PUBLIC -mavx512f -mavx512vl -mavx512dq -mfma)
    elseif(USE_AVX2)
        target_compile_options(qurcuma_remote PUBLIC -mavx2 -mfma)
    endif()
    set_target_properties(qurcuma_remote PROPERTIES AUTOMOC ON)

    add_executable(qurcuma-server src/remote/server_main.cpp)
    target_link_libraries(qurcuma-server PRIVATE qurcuma_remote)

    add_executable(test_remote_protocol tests/test_remote_protocol.cpp)
    target_link_libraries(test_remote_protocol PRIVATE qurcuma_remote)
    add_executable(test_remote_loopback tests/test_remote_loopback.cpp)
    target_link_libraries(test_remote_loopback PRIVATE qurcuma_remote)
    add_executable(test_remote_reconnect tests/test_remote_reconnect.cpp)
    target_link_libraries(test_remote_reconnect PRIVATE qurcuma_remote)
    add_executable(test_remote_hosts tests/test_remote_hosts.cpp)
    target_link_libraries(test_remote_hosts PRIVATE qurcuma_remote)
    add_executable(test_remote_ssh tests/test_remote_ssh.cpp)
    target_link_libraries(test_remote_ssh PRIVATE qurcuma_remote)
    add_dependencies(test_remote_ssh qurcuma-server)
    add_dependencies(test_remote_hosts qurcuma-server)
else()
    message(STATUS "qurcuma: Qt6::WebSockets not found - qurcuma-server and the remote tests are not built")
endif()

set(CPACK_PACKAGE_NAME "qurcuma")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "qurcuma-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
include(CPack)

# Claude Generated 2026 - The GPU plugins (libcurcuma_<backend>.so, dlopen'd by curcuma's
# gpu_plugin) resolve their undefined core symbols (NativeXtbMethod vtable, XTB, ...) against
# the executable. Curcuma's -rdynamic lives in its own directory scope and does not reach
# these targets, so export the symbols explicitly.
set_target_properties(qurcuma PROPERTIES ENABLE_EXPORTS ON)
if(TARGET qurcuma-server)
    set_target_properties(qurcuma-server PROPERTIES ENABLE_EXPORTS ON)
endif()
