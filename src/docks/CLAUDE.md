# Dock Architecture (`src/docks/`)

## Ownership
- `DockManager` owns all `QDockWidget` shells, initial placement and the dock side of the app mode (Explore/Compute/Teaching).
- `MainWindow` coordinates via signals and pulls internal widgets through wrapper getters during migration.
- Each wrapper inherits `QDockWidget` and exposes its content widgets so existing logic can stay in `MainWindow` while construction moves here.

## Wrapper Classes
- `ProjectDock` — working directory chooser, breadcrumb, segmented directory list (Files / Bookmarks / Workspaces / Remote, see below), file/content browser (always files); on top a Lesson section (metadata, lesson structure list, per-structure editor), shown in Teaching mode and whenever the lesson has structures.
- `StructureDock` — right-side dock with a segmented [Structure | Atoms] area (XYZ editor + atom table).
- `AppearanceDock` (UX stage 4) — the Display panel + camera views; tabified on the right, **closed by default** (Look ▸ Details…). Old layouts/workspace dock states were reset once (`DockConfig::UiLayoutVersion` = 2).
- `SimulationDock` — right-side dock with tabs [Simulation | All parameters | Snapshots | RMSD / Align | Input]; tabs are switched by page (`setCurrentWidget`), never by position.
- `OutputDock` — output log + clear button.
- `ImageGalleryDock`: bottom dock (tabified with Output), hidden until the first image export; thumbnail grid with a "Show" filter (session / folder / resized-only / originals-only). Border-trim analysis (`imagecrop::commonContentRect`) writes metadata-preserving `<name>.resized.png`. Fed by `MoleculeViewer::imageExported`. Context menu: view with metadata table, remove from gallery, delete file (confirmed).
- `ChartDock` — bottom dock (tabified with Output, hidden by default), thin wrapper around `SimulationChartWidget`: one chart per tab (Temperature / Energy / Measurements / Histogram) with a shared control bar (sliding-window vs accumulated view, window length, normalisation, bin count, CSV export) and the tracked-measurement table below. Visibility via View ▸ Panels ▸ Charts and Simulation ▸ Charts (Ctrl+Shift+C) — both are the dock's own `toggleViewAction()`; the modes remember it like every other panel. Was a modeless dialog until Sep 2026.
- `NciDock` — right-side dock (tabified with Display, hidden by default) with the non-covalent interaction contact table (`NciWidget`): source combo, "Analyse current frame" for the calculated sources, summary line, table Typ/Atome/d/Winkel/Score/E/Notiz, TSV copy. Row click selects the contact's atoms in the viewer, double-click zooms to them; signals only, MainWindow drives the viewer (same split as `RMSDWidget`).

## Shared Config (`dockconfig.h`)
- `DockConfig::AppMode` — Explore / Compute / Teaching, persisted as int in `ui/appMode` (append only).
- Stable `objectName`s and default dock areas. Do not change names without a migration plan; they are persisted in `QSettings` via `QMainWindow::saveState()`/`restoreState()`.

## Dock Visibility
- View ▸ Panels uses each dock's `QDockWidget::toggleViewAction()`, which is Qt's safe path for tabified groups.
- The app mode sets visibility **per dock** (`DockManager::showPanels` → `setDockVisible`, hides before shows, then `raise()` the mode's front tab). Never toggle via `tabifiedDockWidgets()`: Structure/Simulation/Interactions share one tab bar, so a group toggle hid Structure with Simulation and surfaced the hidden-by-default Interactions/Images docks.

## ProjectDock File Browser Filter
- `DirectoryFilterProxyModel` and `FileFilterBar` (`directoryfilter.*`) are shared by the local and the remote file list: the proxy reads a `QFileSystemModel` directly, other models through `IsDirRole` and the display name; the rule is `remotepaths::fileFilterAccepts` (test `test_remotepaths`).
- Compact bar above each file list: search field + `Extensions` popup with all suffixes of the shown directory ("<no extension>" = empty suffix) + clear button; session-only (resets on restart). The owner reports the suffixes (`setAvailableSuffixes`).
- `MainWindow` resolves view indices back to source indices via `filePathFromContentIndex()`; the Lesson mode bypasses the proxy.

## Remote Segment (`remotebrowserpanel.*`, only with `USE_SFTP`)
- Same structure as the Files segment: remote "Choose working directory" button (ProjectDock shows it in its top row instead of the local one), breadcrumb of the remote working directory, list of its subdirectories with ".." and "+ New Directory", below the shared filter bar and the files of the directory selected above. Local breadcrumb and local file list are hidden in this segment.
- Interaction copied from the local list: click or current change on a directory shows its files below, context menu Add to Bookmarks / Set as Working Directory, breadcrumb click sets the working directory. Double click on a file downloads it into the SFTP cache (`<cache>/<sha256>/<name>`, an unchanged copy is reused) and emits `openFileRequested` → `MainWindow::loadMoleculeFile` (local files load on a single click).
- Extra compared with local: compact row with source combo (computers of `remote/hostprofiles.h`, SFTP profiles, "Other host..." with "Save as SFTP profile"), Connect/Disconnect and status line; Refresh; file context menu Open / Download to Local Working Directory / Copy Remote Path / Add This Folder to Bookmarks.
- The remote working directory is remembered per source in QSettings (`remotepaths::workDirSettingsKey`, source key = bookmark id or "host:user@host:port"), stored only after it was listed; default the remote home, a missing one falls back to home with the error shown.
- "Choose" opens a modal chooser (breadcrumb, folder list, path field, Select checks a typed path by listing it) on the open connection; without one it connects the selected source first.
- Bookmarks: the star toggles the remote working directory, choosing a bookmark makes it the working directory; stored as `Settings::RemoteMountPoint`, `profileId` = SFTP profile id or `computer:<name>`. Unsaved hosts cannot be bookmarked.
- "+ New Directory": name checked by `remotepaths::isValidNewDirectoryName` (not empty, not "."/"..", no '/'), refused if the name exists, then `SftpClient::makeDirectory` (`sftp_mkdir`, mode 0755) and the new directory is selected.
- libssh lives in `SftpClient` (`src/sftpclient.*`), one client per connection on its own `QThread`; listings carry a request id (working directory, selected directory, chooser). Disconnect abandons the client (cancel flag, results dropped, client and thread delete themselves once the blocking call returns).
- Login: host key against `~/.ssh/known_hosts` (unknown: fingerprint question, changed: refused), then agent and standard keys, then one password prompt (password or keyboard-interactive), never stored. Only address, user and port of a computer are used, `qurcuma-server` is never contacted; a `~/.ssh/config` alias supplies HostName/User/Port/ProxyJump (`remotepaths::connectionForHost`).
- File ▸ Open Remote File... (Ctrl+Shift+R) shows this segment; File ▸ Recent Remote Connections selects and connects a profile.
- Qt-only helpers in `src/remotepaths.*` (test `test_remotepaths`). 🤖 SftpClient, the panel and the chooser are only compiled (no SFTP server in the test setup), not checked in the GUI.
- `USE_SFTP` is ON by default; without libssh CMake warns and builds without it. Built with libssh 0.12.2.

## AppearanceDock Views (camera)
- `ViewPresetWidget` lives below the `DisplayPanel`; manages reproducible **camera views** only (UX stage 3; the appearance is a Look, see `src/CLAUDE.md`).
- Views are stored under `viewPresets/` in `QSettings` and survive restarts; the list starts empty.
- `ViewPreset` (`src/viewpreset.h`) holds camera only (`rootRotation`, `pan`, `fieldOfView`, `cameraDistance`, `zoomFactor`, `zoomMode`). `ZoomMode::Absolute` applies the stored distance verbatim; `ZoomMode::Relative` reconstructs distance = `zoomFactor * sceneExtent` so the molecule keeps its on-screen size across different structures.
- `MoleculeViewer::currentViewPreset(ZoomMode)` captures; `applyViewPreset()` restores via the atomic `SceneController::setCameraTransform` + `m_quickWindow->update()`, then emits `viewPresetApplied()` so `DisplayPanel::syncFromViewer()` re-syncs its controls (no dock raise).
- Quick buttons `Front`/`Top`/`Side` call `MoleculeViewer::setCameraOrientation()` (rotation only; zoom stays).

## App Modes (Explore / Compute / Teaching)
- The only layout switch (UX stage 4b; the five layout presets and Ctrl+Alt+1–5 are gone). Custom layouts are saved as workspaces.
- `MainWindow::setAppMode` updates the corner buttons, persists `ui/appMode`, shows the calculation toolbar in Compute only.
- Teaching = Explore with the lesson first: `ProjectDock::setLessonTeaching` shows and opens the Lesson section at the top of the Project panel; the file browser below keeps working.
- `DockManager::setAppMode`: **each mode keeps its own panels** (all seven docks, as objectNames in `ui/modePanels/<mode>`), stored when the mode is left and on close, restored on entry. Defaults: Explore = Project + Structure (Structure in front), Teaching adds Simulation, Compute also Output (Simulation in front).
- Without a stored layout (first run, workspace without layout, View ▸ Reset to Default Layout) the mode also sizes the docks (`reflow`); Reset also forgets the panels of every mode.

## Viewer Embedding (why docks stopped overlapping)
- `MoleculeViewer` embeds the 3D scene as a `QQuickWidget` (`src/view.cpp`, `setupViewer`). The former `QQuickView` + `createWindowContainer()` was a **native window**, which Qt stacks above all sibling widgets: it painted over dock panels/tab bars during resizes, animations and on Wayland, and swallowed the clicks meant for them.
- `QURCUMA_NATIVE_VIEWPORT=1` restores the native route for A/B comparison (threaded render loop, `frameSwapped`).

## Wayland
- ✅ **Drag-to-redock works under native Wayland** (tested on KWin): Qt 6.11 drags a dock as platform drag-and-drop (window attached via `xdg_toplevel_drag_v1`) and places the drop gap from the DragMove events that reach `QMainWindow::event()`.
- Qt delivers drag events only to the innermost `acceptDrops` widget (viewer `QQuickWidget`, line/text edits), so `MainWindow::forwardDockDragEvent` (qApp filter) redirects dock drags (MIME `application/x-qt-mainwindowdrag-window`) to the main window.
- It calls `QMainWindow::event()` directly: `sendEvent()` would recurse, because `QApplication::notify` routes DragMove/Drop/DragLeave to the widget that took the DragEnter.
- **`GroupedDragging` is off under Wayland**: re-docking a floating tab group crashes inside Qt 6.11 (`QMainWindowLayout::animationFinished` calls `setTabBarShape()` on a sub-layout pointer that reads back as nullptr after `reparentWidgets()`). A saved layout can still restore such a group via `restoreState()`.
- Fallback: View ▸ "Re-dock Floating Panels" (`DockManager::redockFloating`, plain `addDockWidget()`), e.g. for compositors without `xdg_toplevel_drag_v1`; skips docks inside a floating tab group.
- `QCursor::setPos` stays a no-op on Wayland (Cursor Lock, `src/CLAUDE.md`): clients get no absolute positions.

## Migration State
- Phase 4 complete: `ProjectDock` extracted from `MainWindow`; all docks now live under `DockManager`.
- Phase 5 complete: preset logic and app-mode dock handling moved to `DockManager`; `MainWindow` enums replaced by `DockConfig` enums.
- Phase 6 complete: `NavigationDock` removed; bookmarks/workspaces/remote are now segment pages inside `ProjectDock`. Signals are forwarded by `ProjectDock` so `MainWindow` stays decoupled from internal widgets.
- Phase 8 complete: `EditorsDock` and `AtomsSimulationDock` removed; content split into `DisplayDock` (Structure/Atoms segment + Display panel) and `SimulationDock` (Simulation/Snapshots/RMSD/Input tabs). The two right-side docks are tabified.
- Internal `QTabWidget`s are intentionally retained inside the remaining wrappers to keep the Qt drift workaround working.
