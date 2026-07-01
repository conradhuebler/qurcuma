# Dock Architecture (`src/docks/`)

## Ownership
- `DockManager` owns all `QDockWidget` shells, initial placement, layout presets, and Explore/Compute mode.
- `MainWindow` coordinates via signals and pulls internal widgets through wrapper getters during migration.
- Each wrapper inherits `QDockWidget` and exposes its content widgets so existing logic can stay in `MainWindow` while construction moves here.

## Wrapper Classes
- `ProjectDock` — working directory chooser, breadcrumb, segmented directory list (Files / Bookmarks / Workspaces / Remote), file/content browser with Files/Lesson toggle, lesson metadata + per-structure editor.
- `DisplayDock` — right-side dock with segmented top area [Structure | Atoms] and the viewer Display panel below it.
- `SimulationDock` — right-side dock with tabs [Simulation | Snapshots | RMSD / Align | Input].
- `OutputDock` — output log + clear button.

## Shared Config (`dockconfig.h`)
- `DockConfig::LayoutPreset` — Visualization, Editing, Calculation, Analysis, Teaching.
- `DockConfig::AppMode` — Explore (viewer focus) vs. Compute (calculation workflow).
- Stable `objectName`s and default dock areas. Do not change names without a migration plan; they are persisted in `QSettings` via `QMainWindow::saveState()`/`restoreState()`.

## Layout Presets
- Implemented in `DockManager` using lazy `saveState()`/`restoreState()` caching to avoid Qt drift from repeated `tabifyDockWidget`/`splitDockWidget`.
- MainWindow only adds status-bar messages and menu/shortcut dispatch.
- Bound to Ctrl+Alt+1..4; Teaching is used by the Lesson / interactive-demo workflow.
- View ▸ Dock Panels uses each dock's `QDockWidget::toggleViewAction()`, which is Qt's safe path for tabified groups.

## ProjectDock File Browser Filter
- `DirectoryFilterProxyModel` sits between `QFileSystemModel` and `QListView`; combines live name search with extension subset filtering.
- Embedded compact bar above the content list: search field + `Extensions` popup menu with all suffixes in the current directory + clear button; session-only (resets on restart).
- `MainWindow` resolves view indices back to source indices via `filePathFromContentIndex()`; Lesson/SFTP modes bypass the proxy.

## Explore / Compute Mode
- `MainWindow::setAppMode` updates mode buttons, persists to `ui/appMode`, and toggles the calculation toolbar.
- Dock visibility and reflow are delegated to `DockManager::setAppMode`.
- Tab-bar stability: `DockManager` never toggles a single dock inside a tabified group; it uses `QMainWindow::tabifiedDockWidgets()` and changes visibility for the whole group at once.

## Migration State
- Phase 4 complete: `ProjectDock` extracted from `MainWindow`; all docks now live under `DockManager`.
- Phase 5 complete: preset logic and app-mode dock handling moved to `DockManager`; `MainWindow` enums replaced by `DockConfig` enums.
- Phase 6 complete: `NavigationDock` removed; bookmarks/workspaces/remote are now segment pages inside `ProjectDock`. Signals are forwarded by `ProjectDock` so `MainWindow` stays decoupled from internal widgets.
- Phase 8 complete: `EditorsDock` and `AtomsSimulationDock` removed; content split into `DisplayDock` (Structure/Atoms segment + Display panel) and `SimulationDock` (Simulation/Snapshots/RMSD/Input tabs). The two right-side docks are tabified.
- Internal `QTabWidget`s are intentionally retained inside the remaining wrappers to keep the Qt drift workaround working.
