# Dock Architecture (`src/docks/`)

## Ownership
- `DockManager` owns all `QDockWidget` shells, initial placement and the dock side of the app mode (Explore/Compute/Teaching).
- `MainWindow` coordinates via signals and pulls internal widgets through wrapper getters during migration.
- Each wrapper inherits `QDockWidget` and exposes its content widgets so existing logic can stay in `MainWindow` while construction moves here.

## Wrapper Classes
- `ProjectDock` — working directory chooser, breadcrumb, segmented directory list (Files / Bookmarks / Workspaces / Remote), file/content browser (always files); on top a Lesson section (metadata, lesson structure list, per-structure editor), shown in Teaching mode and whenever the lesson has structures.
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
- `DirectoryFilterProxyModel` sits between `QFileSystemModel` and `QListView`; combines live name search with extension subset filtering.
- Embedded compact bar above the content list: search field + `Extensions` popup menu with all suffixes in the current directory + clear button; session-only (resets on restart).
- `MainWindow` resolves view indices back to source indices via `filePathFromContentIndex()`; Lesson/SFTP modes bypass the proxy.

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
