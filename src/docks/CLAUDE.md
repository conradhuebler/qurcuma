# Dock Architecture (`src/docks/`)

## Ownership
- `DockManager` owns all `QDockWidget` shells, initial placement, layout presets, and Explore/Compute mode.
- `MainWindow` coordinates via signals and pulls internal widgets through wrapper getters during migration.
- Each wrapper inherits `QDockWidget` and exposes its content widgets so existing logic can stay in `MainWindow` while construction moves here.

## Wrapper Classes
- `ProjectDock` — working directory chooser, breadcrumb, segmented directory list (Files / Bookmarks / Workspaces / Remote), file/content browser with Files/Lesson toggle, lesson metadata + per-structure editor.
- `DisplayDock` — right-side dock with segmented top area [Structure | Atoms], the Display panel, and the View-Preset widget.
- `SimulationDock` — right-side dock with tabs [Simulation | Snapshots | RMSD / Align | Input].
- `OutputDock` — output log + clear button.
- `ImageGalleryDock` — bottom dock (tabified with Output), hidden until the first image export; thumbnail grid of exported images. "Show:" combo = session / folder all-PNG / resized-only / originals-only. Common border-trim analysis (flip-book: centre all frames on a max-size canvas, `imagecrop::commonContentRect` = union of content → one crop at identical position, uniform X×Y even for differently-sized sources; the crop rect is drawn dashed-red onto the analyzed thumbnails) → saves metadata-preserving `<name>.resized.png`. Fed by `MoleculeViewer::imageExported` (export dialog + viewer-bar "Photo" quick-export with transparent/colour-preset controls). Context menu / double-click: view image (fit-to-window + zoom slider) + embedded-metadata table, remove from gallery, delete file from disk (confirmed).
- `NciDock` — right-side dock (tabified with Display, hidden by default) with the non-covalent interaction contact table (`NciWidget`): source combo, "Analyse current frame" for the calculated sources, summary line, table Typ/Atome/d/Winkel/Score/E/Notiz, TSV copy. Row click selects the contact's atoms in the viewer, double-click zooms to them; signals only, MainWindow drives the viewer (same split as `RMSDWidget`).

## Shared Config (`dockconfig.h`)
- `DockConfig::LayoutPreset` — Visualization, Editing, Calculation, Analysis, Teaching.
- `DockConfig::AppMode` — Explore (viewer focus) vs. Compute (calculation workflow).
- Stable `objectName`s and default dock areas. Do not change names without a migration plan; they are persisted in `QSettings` via `QMainWindow::saveState()`/`restoreState()`.

## Layout Presets
- Implemented in `DockManager` using lazy `saveState()`/`restoreState()` caching to avoid Qt drift from repeated `tabifyDockWidget`/`splitDockWidget`.
- MainWindow only adds status-bar messages and menu/shortcut dispatch.
- Bound to Ctrl+Alt+1..4; Teaching is used by the Lesson / interactive-demo workflow.
- View ▸ Dock Panels uses each dock's `QDockWidget::toggleViewAction()`, which is Qt's safe path for tabified groups.
- Presets/app mode set visibility **per dock** (`setDockVisible`, hides before shows, then `raise()` the preset's front tab). Never toggle via `tabifiedDockWidgets()`: Display/Simulation/Interactions share one tab bar, so a group toggle hid Display with Simulation and surfaced the hidden-by-default Interactions/Images docks.

## ProjectDock File Browser Filter
- `DirectoryFilterProxyModel` sits between `QFileSystemModel` and `QListView`; combines live name search with extension subset filtering.
- Embedded compact bar above the content list: search field + `Extensions` popup menu with all suffixes in the current directory + clear button; session-only (resets on restart).
- `MainWindow` resolves view indices back to source indices via `filePathFromContentIndex()`; Lesson/SFTP modes bypass the proxy.

## DisplayDock View Presets
- `ViewPresetWidget` lives below the `DisplayPanel`; manages reproducible camera + display presets (one preset = camera + display together).
- Presets are stored under `viewPresets/` in `QSettings` and survive restarts; the list starts empty.
- `ViewPreset` (`src/viewpreset.h`) holds camera (`rootRotation`, `pan`, `fieldOfView`, `cameraDistance`, `zoomFactor`, `zoomMode`) + display state. `ZoomMode::Absolute` applies the stored distance verbatim; `ZoomMode::Relative` reconstructs distance = `zoomFactor * sceneExtent` so the molecule keeps its on-screen size across different structures.
- `MoleculeViewer::currentViewPreset(ZoomMode)` captures; `applyViewPreset()` restores via the atomic `SceneController::setCameraTransform` + `m_quickWindow->update()`, then emits `viewPresetApplied()` so `DisplayPanel::syncFromViewer()` re-syncs its controls (no dock raise).
- Quick buttons `Front`/`Top`/`Side` call `MoleculeViewer::setCameraOrientation()` — only rotation, zoom and display stay.

## Explore / Compute Mode
- `MainWindow::setAppMode` updates mode buttons, persists to `ui/appMode`, and toggles the calculation toolbar.
- Dock visibility and reflow are delegated to `DockManager::setAppMode`.
- Explore hides Simulation + Output and raises Display; Compute shows all four layout docks and raises Simulation. Interactions/Images are left as they are.

## Viewer Embedding (why docks stopped overlapping)
- `MoleculeViewer` embeds the 3D scene as a `QQuickWidget` (`src/view.cpp`, `setupViewer`). The former `QQuickView` + `createWindowContainer()` was a **native window**, which Qt stacks above all sibling widgets: it painted over dock panels/tab bars during resizes, animations and on Wayland, and swallowed the clicks meant for them.
- `QURCUMA_NATIVE_VIEWPORT=1` restores the native route for A/B comparison (threaded render loop, `frameSwapped`).

## Known Limitations (Wayland)
- **Re-docking a floated dock does not work under native Wayland.** Confirmed 2026-09: undocking is fine, dragging the floating title bar back over the main window never shows a drop indicator and never re-docks; `QT_QPA_PLATFORM=xcb` (XWayland) fixes it immediately. Cause: Qt's built-in float/redock hit-testing needs absolute screen coordinates to tell whether the floating window is over a dock area, and native Wayland does not expose those to clients. No code-side fix; `main.cpp` prints a startup hint (`QGuiApplication::platformName() == "wayland"`) instead of silently forcing `xcb`, which could keep the app from starting on a compositor without XWayland.
- Same root cause as the existing Wayland `QCursor::setPos` no-op noted under Cursor Lock (`src/CLAUDE.md`): absolute-position APIs are unavailable to Wayland clients by design.

## Migration State
- Phase 4 complete: `ProjectDock` extracted from `MainWindow`; all docks now live under `DockManager`.
- Phase 5 complete: preset logic and app-mode dock handling moved to `DockManager`; `MainWindow` enums replaced by `DockConfig` enums.
- Phase 6 complete: `NavigationDock` removed; bookmarks/workspaces/remote are now segment pages inside `ProjectDock`. Signals are forwarded by `ProjectDock` so `MainWindow` stays decoupled from internal widgets.
- Phase 8 complete: `EditorsDock` and `AtomsSimulationDock` removed; content split into `DisplayDock` (Structure/Atoms segment + Display panel) and `SimulationDock` (Simulation/Snapshots/RMSD/Input tabs). The two right-side docks are tabified.
- Internal `QTabWidget`s are intentionally retained inside the remaining wrappers to keep the Qt drift workaround working.
