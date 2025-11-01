# AIChangelog - Qurcuma Improvements

## November 2025 (Iteration 3 - In Progress)

### 3D Visualization Phase 2A - Atom Selection & Picking (COMPLETE)
- Implemented Qt3D ObjectPicker on each atom for direct 3D click-based selection
- Single-click selection, Ctrl+Click multi-select, Shift+Click toggle modes fully functional
- Visual selection feedback: Orange-yellow highlighting with increased shininess
- SelectionManager class: Centralized selection state management with signals
- Keyboard shortcuts: Ctrl+A (select all atoms), Escape (clear selection)
- Direct integration: MoleculeViewer → SelectionManager bidirectional state sync
- Fully tested: All selection modes working, colors visible, signals propagating

### Directory Navigation & Workspace System (COMPLETE - Iteration 2)

**Phase 1-2: UI Navigation (Complete)**
- Added BreadcrumbBar widget: Clickable path segments replacing plain text label; users jump to parent dirs by clicking breadcrumb segments; Home shown as ~
- Enhanced Recent Files: RecentFileEntry struct with QDateTime timestamps; menu groups by date (Today/Yesterday/This Week/Older); shows context (filename with parent directory)

**Phase 3.1: Bookmark Foundation (Complete)**
- BookmarkItem struct: Hierarchical support with id, name, path, tags, color, parentId, isFolder, created timestamp
- Settings methods: bookmarks(), setBookmarks(), addBookmark(), removeBookmark(), updateBookmark()
- Serialization: Pipe-delimited format in QSettings with auto-migration from legacy workingDirectories
- Ready for UI: Tree widget integration deferred to next iteration

**Phase 4.1-4.2: Workspace Foundation (Complete)**
- Workspace struct: Captures complete state - working directory, calculation directories, window geometry, splitter states, timestamps
- WorkspaceManager class: Methods for save/restore/list/delete/rename workspace operations
- Settings persistence: All workspace data serialized and persisted in QSettings
- Ready for UI: Sidebar widget and menu integration deferred to next iteration

**Phase 3.2-3.5: Bookmark Tree UI (Complete)**
- Replaced QListWidget with QTreeWidget for hierarchical bookmark structure with folders
- Context menu: New Folder, Add Bookmark, Rename, Delete, Edit Tags operations
- Drag & Drop enabled for reorganizing bookmarks between folders (QAbstractItemView::InternalMove)
- Minimal tag system: Edit tags via dialog (comma-separated), display in bookmark tooltip
- Icons: Folder-icon for folders, Bookmark-icon for bookmarks, color support for visual organization

**Phase 4.3-4.5: Workspace Management UI (Complete)**
- Workspace list widget in sidebar below bookmarks with "+" button to save new workspaces
- File menu "Workspaces" submenu with Save (Ctrl+Shift+S) and Load (Ctrl+Shift+O) shortcuts
- Complete workspace capture: working directory, window geometry, splitter layout, timestamps
- Full restore functionality: Click workspace in sidebar → restores complete application state
- Workspace persistence: Saves to QSettings with JSON serialization, auto-migration support

**Bonus Context Menu Activation**
- Enabled missing setupProjectViewContextMenu() call in MainWindow constructor
- Right-click on calculation directories now shows context menu: "Add to Bookmarks", "Set as Working Directory"

## October 2025

- Fixed VTF bond parsing: Changed QMap<int, VTFBond> to QVector<VTFBond> to support multiple bonds per atom (previously lost ~50% of bonds due to key collision)
- Fixed VTF frame parsing: Removed global parseError flag from main loop; now correctly processes all frame sections (was stopping after first conversion error, returning 0 frames instead of 3)
- Added test infrastructure: test_vtf_bonds.cpp (validates 199 bonds), test_vtf_frames.cpp (detects 3 frames), test_vtf_full.cpp (end-to-end validation)
