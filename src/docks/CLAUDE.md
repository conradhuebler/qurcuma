# Dock Architecture (`src/docks/`)

## Ownership
- `DockManager` owns all `QDockWidget` shells, initial placement, layout presets, and Explore/Compute mode.
- `MainWindow` coordinates via signals and pulls internal widgets through wrapper getters during migration.
- Each wrapper inherits `QDockWidget` and exposes its content widgets so existing logic can stay in `MainWindow` while construction moves here.

## Wrapper Classes
- `ProjectDock` — working directory chooser, breadcrumb, segmented directory list (Files / Bookmarks / Workspaces / Remote), file/content browser with Files/Lesson toggle, lesson metadata + per-structure editor.
- `EditorsDock` — tabbed Structure editor, Input editor, RMSD/Align widget.
- `AtomsSimulationDock` — tabbed atom list, simulation controls, snapshots.
- `DisplayDock` — viewer display options panel.
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

## Explore / Compute Mode
- `MainWindow::setAppMode` updates mode buttons, persists to `ui/appMode`, and toggles the calculation toolbar.
- Dock visibility and reflow are delegated to `DockManager::setAppMode`.
- Tab-bar stability: `DockManager` never toggles a single dock inside a tabified group; it uses `QMainWindow::tabifiedDockWidgets()` and changes visibility for the whole group at once.

## Migration State
- Phase 4 complete: `ProjectDock` extracted from `MainWindow`; all docks now live under `DockManager`.
- Phase 5 complete: preset logic and app-mode dock handling moved to `DockManager`; `MainWindow` enums replaced by `DockConfig` enums.
- Phase 6 complete: `NavigationDock` removed; bookmarks/workspaces/remote are now segment pages inside `ProjectDock`. Signals are forwarded by `ProjectDock` so `MainWindow` stays decoupled from internal widgets.
- Internal `QTabWidget`s are intentionally retained inside the remaining wrappers to keep the Qt drift workaround working.
