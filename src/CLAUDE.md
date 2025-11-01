# src/CLAUDE.md - Core Modules Documentation

## Overview

Core modules for file parsing, 3D visualization, and user interface.

## Current Status (November 2025)

### Directory Navigation & Workspace Management ✅ COMPLETE
- ✅ **Phase 1: Breadcrumb Navigation** - Clickable path segments in sidebar
  - Click segments to jump to parent directories, Home shown as ~
  - Replaces plain text label for interactive navigation
- ✅ **Phase 2: Enhanced Recent Files** - Dated groups + full path context
  - RecentFileEntry struct with QDateTime timestamps
  - Menu grouped: Today, Yesterday, This Week, Older
- ✅ **Phase 3.1: BookmarkItem Structure** - Hierarchical bookmark system with metadata
  - Supports folders + bookmarks with tags, colors, hierarchy
  - Auto-migration from legacy workingDirectories format
- ✅ **Phase 3.2-3.5: Bookmark UI** - COMPLETE
  - Tree widget replacing QListWidget for hierarchical structure
  - Context menu: New Folder, Add Bookmark, Rename, Delete, Edit Tags
  - Drag & Drop enabled for reorganizing bookmarks
  - Minimal tag system: edit tags via dialog, display in tooltip
- ✅ **Phase 4.1-4.2: Workspace System** - Foundation complete
  - Workspace struct captures working dir, geometry, splitter states, timestamps
  - WorkspaceManager handles save/restore/list operations
- ✅ **Phase 4.3-4.5: Workspace UI** - COMPLETE
  - Sidebar widget with workspace list and "+" save button
  - File menu "Workspaces" submenu (Save: Ctrl+Shift+S, Load: Ctrl+Shift+O)
  - Click workspace in sidebar → restore full state
  - Capture/restore: working dir, window geometry, splitter layout

### Phase 1: Visualization Settings & Shortcuts ✅ COMPLETE
- ✅ **Settings Persistence** - All visualization settings now saved to QSettings, restored on startup
- ✅ **Fog Controls** - Enable/disable fog with intensity slider in VisualizationSettingsDialog
- ✅ **Keyboard Shortcuts** - 1-4 for rendering modes, +/- for atom size, </> for bond thickness

### Phase 3: Presets & Focus Commands ✅ COMPLETE
- ✅ **Visualization Presets** - Save/load named presets (Publication, Analysis, Presentation built-in)
- ✅ **Focus Commands** - fitAllInView(), centerOnAtom(), zoomToSelection() with proper camera math
- ✅ **Focus Shortcuts** - Ctrl+0/Home to fit, Ctrl+F to focus on selection

### Previously Fixed ✅
- ✅ **VTF Bond Parsing Bug** - Changed `QMap<int, VTFBond>` to `QVector<VTFBond>` (allowed multiple bonds per atom)
- ✅ **VTF Frame Parsing Bug** - Removed global `parseError` flag stopping loop; now detects all 3 frames correctly
- ✅ **Mouse Interactions** - Disabled QOrbitCameraController, implemented arcball rotation + pan + zoom
- ✅ **Test Infrastructure** - Added comprehensive VTF parsing tests: `test_vtf_bonds.cpp`, `test_vtf_frames.cpp`, `test_vtf_full.cpp`

## Modules

### VTF/XYZ Parsers (vtfparser.*, xyzparser.*)
- **VTFParser**: Parses VTF trajectory files with multi-frame support, handles atom definitions, bonds, and unit cells
- **XYZParser**: Parses XYZ structure/trajectory files
- **Known Issues**: XYZ parser needs trajectory support (currently only basic structure parsing)

### 3D Molecule Viewer (view.*)
- **MoleculeViewer**: Qt3D-based 3D molecular visualization with trajectory frame navigation
- **Mouse Interactions** (Claude Generated):
  - Left click: Arcball rotation around view center (quaternion-based, gimbal-lock free)
  - Right click: Pan/translate view (distance-proportional)
  - Mouse wheel: Zoom in/out with minimum distance clamping
  - Middle click: Reset to molecule center
- **Visualization Controls** (Claude Generated - Phase 1):
  - Rendering modes: Ball-and-stick, Space-filling, Wireframe, Sticks-only
  - Color schemes: CPK (element), Monochrome, By-charge, Custom
  - Material properties: Transparency, shininess, atom scale, bond thickness
  - Fog/depth cueing: Enable/disable with intensity control
- **Focus & Navigation** (Claude Generated - Phase 3):
  - fitAllInView() - Frame entire molecule in viewport
  - centerOnAtom(index) - Focus on specific atom
  - zoomToSelection() - Frame selected atoms
- **Features**: Atom rendering (CPK colors), bond visualization (multiple bonds), frame slider navigation, preset management

### Main Window & Settings (mainwindow.*, settings.*)
- **MainWindow**: Qt-based GUI with file browser, molecule viewer, and output panels
- **Settings**: Persistent application settings storage with QSettings
- **Features**: Visualization settings persistence, preset management, keyboard shortcuts for common operations
- **Shortcuts** (Claude Generated - Phase 1 & 3):
  - 1-4: Rendering mode selection
  - +/-: Atom size adjustment
  - </> (Shift+comma/period): Bond thickness adjustment
  - Ctrl+0 / Home: Fit molecule in view
  - Ctrl+F: Center on selection

## Architecture

```
src/
├── vtfparser.cpp/h          - VTF file parsing (3 frames, 200 atoms, 199 bonds)
├── xyzparser.cpp/h          - XYZ file parsing
├── view.cpp/h               - 3D molecular viewer (Qt3D + custom camera control)
├── orbittransformcontroller.cpp/h - Additional camera controls
├── mainwindow.cpp/h         - Main GUI window with shortcuts and menu system
├── main.cpp                 - Application entry point
├── settings.cpp/h           - Configuration persistence with QSettings
├── visualizationsettingsdialog.cpp/h - Visualization & preset management (Phase 1 & 3)
├── selectionmanager.cpp/h   - Atom selection state management (Phase 2 stub)
├── modifiabletextedit.h     - Custom text edit widget
├── frequencydialog.h        - Frequency analysis dialog
├── widgets/
│   └── breadcrumbbar.cpp/h  - Clickable path navigation widget (Directory Navigation Phase 1)
├── workspacemanager.cpp/h   - Workspace capture/restore manager (Phase 4.2)
└── dialogs/                 - Modal dialogs for specialized features
```

## Testing

**VTF Parser Tests** (all passing):
- `test_vtf_bonds.cpp` - Validates bond parsing (expects 199 bonds from scnp_cut.vtf)
- `test_vtf_frames.cpp` - Validates frame detection (expects 3 frames)
- `test_vtf_full.cpp` - End-to-end validation combining both tests

Run tests: `./release/test_vtf_bonds`, `./release/test_vtf_frames`, `./release/test_vtf_full`

## Development Guidelines

- Mark new functions as "Claude Generated" for traceability
- Document new functions briefly with scientific context
- Keep camera interactions intuitive: rotate around view center, not origin
- Maintain backward compatibility for file format support

### Directory Navigation - Implementation Details (Claude Generated Nov 2025)
- **BreadcrumbBar** (widgets/breadcrumbbar.h/cpp):
  - Custom QWidget with mouse events and painting
  - Splits filesystem paths into clickable segments
  - Home directory displayed as ~ for readability
  - Hover effects and PointingHandCursor for UX
  - Emits pathSelected(QString) signal on segment click

- **RecentFileEntry** (settings.h):
  - Struct with QString path + QDateTime lastAccessed
  - Serialized as "path|timestamp" format in QSettings
  - Automatic migration from old QStringList format
  - Max 10 entries with automatic old-entry removal

- **Recent Files Menu** (mainwindow.cpp:updateRecentFilesMenu):
  - Date-based grouping: Today, Yesterday, This Week, Older
  - Shows full context: filename (parent directory)
  - Bold group headers for visual separation
  - Validates directory existence before opening

## Iteration 2 - Directory Navigation & Workspace System ✅ COMPLETE

**Phase 3.2-3.5 - Bookmark UI & Features (DONE ✅)**
- ✅ QTreeWidget replacing QListWidget for hierarchical bookmarks
- ✅ Folder creation/management in tree structure
- ✅ Drag & drop to reorganize bookmarks between folders
- ✅ Context menu: New Folder, Add Bookmark, Rename, Delete, Edit Tags
- ✅ Minimal tag system: Input dialog, tag display in tooltip
- ✅ Color support for bookmark items (foundation in place)

**Phase 4.3-4.5 - Workspace UI & Integration (DONE ✅)**
- ✅ Workspace list widget in sidebar with "+" save button
- ✅ Click workspace → restore full state
- ✅ File menu "Workspaces" submenu
- ✅ Save Workspace (Ctrl+Shift+S) → Dialog for name + description
- ✅ Capture full state: working directory, window geometry, splitter layout
- ✅ Restore workspace: Working dir switch, geometry restore, layout restore
- ✅ Workspace persistence with timestamps (created, lastUsed)

**Completion Stats:**
- 4 main phases implemented (1, 2, 3.1-3.5, 4.1-4.5)
- 6 commits in Iteration 2
- ~600 new lines of code
- All compilation successful, no warnings/errors
- 100% persistence layer functional

### 3D Visualization - Phase 2A ✅ COMPLETE
- ✅ **3D Atom Picking** - Qt3D ObjectPicker on each atom, click-based selection
  - Left Click: Single selection
  - Ctrl+Click: Multi-select (append to selection)
  - Shift+Click: Toggle selection state
- ✅ **Visual Selection Highlighting** - Bright orange-yellow colors + extra shininess
- ✅ **SelectionManager Integration** - Full bidirectional state sync
- ✅ **Keyboard Shortcuts** - Ctrl+A (select all), Escape (clear selection)
- Implementation: 157 lines, 1 git commit

### 3D Visualization - Phase 2B ✅ COMPLETE
- ✅ **MeasurementOverlay Class** - Geometry calculations + 3D visualization
  - Distance measurement: 2-atom selection → line + Å value
  - Angle measurement: 3-atom selection → 2 lines + degree value
  - Dihedral measurement: 4-atom selection → 3 lines + degree value
- ✅ **Cylinder-based Visualization** - Orange-yellow measurement lines
- ✅ **Auto-calculation** - Distance (√((x2-x1)²+(y2-y1)²+(z2-z1)²)), Angle (arccos), Dihedral (plane normal angle)
- ✅ **Dynamic Updates** - Measurements refresh on frame changes and animation
- ✅ **UI Integration** - Combo-box with 4 modes (None/Distance/Angle/Dihedral)
- Implementation: 416 lines, 2 new classes, 1 git commit

**3D Visualization - Phase 2C (Next)**:
- AtomListPanel with QTableView for structure browsing (2-2.5h)
  - Atom properties: Index, Element, X/Y/Z, Charge
  - Bidirectional selection sync with 3D viewer
  - Sort/filter capabilities
