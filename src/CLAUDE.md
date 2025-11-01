# src/CLAUDE.md - Core Modules Documentation

## Overview

Core modules for file parsing, 3D visualization, and user interface.

## Current Status (November 2025)

### Directory Navigation & Workspace Management ✅ FOUNDATION COMPLETE
- ✅ **Phase 1: Breadcrumb Navigation** - Clickable path segments in sidebar (src/widgets/breadcrumbbar.h/cpp)
  - Click segments to jump to parent directories, Home shown as ~
  - Replaces plain text label for interactive navigation
- ✅ **Phase 2: Enhanced Recent Files** - Dated groups + full path context
  - RecentFileEntry struct with QDateTime timestamps
  - Menu grouped: Today, Yesterday, This Week, Older
  - Shows filename (parent directory) for better context
- ✅ **Phase 3.1: BookmarkItem Structure** - Hierarchical bookmark system with metadata
  - BookmarkItem struct: id, name, path, tags, color, parentId, isFolder, created
  - Methods: bookmarks(), addBookmark(), removeBookmark(), updateBookmark()
  - Auto-migration from legacy workingDirectories format
- ✅ **Phase 4.1-4.2: Workspace System** - Foundation for complete state management
  - Workspace struct: id, name, description, workingDirectory, geometry, splitterStates
  - WorkspaceManager class for save/restore/list operations
  - Timestamp tracking and persistence
- ⏳ **Phase 3.2-3.5: Bookmark UI** - Deferred (Tree widget, drag & drop, tag system)
- ⏳ **Phase 4.3-4.5: Workspace UI** - Deferred (Sidebar widget, menu integration, auto-save)

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

## Next Iteration Work (Directory Navigation Phase 3.2-3.5 & 4.3-4.5)

**Phase 3.2-3.5 - Bookmark UI & Features** (estimated 4-5h):
- Replace QListWidget with QTreeWidget for hierarchical bookmarks
- Implement folder creation/management in tree
- Drag & drop to reorganize bookmarks between folders
- Context menu: New Folder, Add Bookmark, Edit Tags, Change Color, Delete, Move
- Tag system UI: Input dialog, tag display as badges
- Color selector for bookmark categorization
- Bookmark search/filter by tag

**Phase 4.3-4.5 - Workspace UI & Integration** (estimated 4-5h):
- Add workspace list widget to sidebar (after bookmarks)
- Workspace context menu: Rename, Delete, Duplicate, Edit description
- File menu integration: "Save Workspace...", "Load Workspace..."
- Manage Workspaces dialog with table view
- MainWindow integration: captureCurrentState() using real window/splitter state
- Auto-save on quit with user confirmation
- Restore last workspace on app startup (with preference toggle)
- Keyboard shortcuts: Ctrl+Shift+S (save), Ctrl+Shift+O (load), Ctrl+1-9 (quick switch)

**Implementation Foundation (Complete ✅):**
- All data structures in place (BookmarkItem, Workspace)
- All Settings methods implemented with serialization
- WorkspaceManager logic ready for UI integration
- Persistence layer functional

**3D Visualization - Phase 2 (Deferred)**:
- 3D Atom picking with Qt3D ObjectPicker (High priority, 2-3h)
- Visual selection highlighting with emissive materials (Medium, 1h)
- AtomListPanel with QTableView for structure browsing (High, 2-2.5h)
- Measurement overlay text and lines in 3D view (Medium, 1-1.5h)
