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
- `ChartDock` — bottom dock (tabified with Output, hidden by default), thin wrapper around `SimulationChartWidget`: one chart per tab (Temperature / Energy / Measurements / Histogram) with a shared control bar (sliding-window vs accumulated view, window length, normalisation, bin count, CSV export) and the tracked-measurement table below. Visibility via View ▸ Dock Panels ▸ Charts and Molecule ▸ Simulation Charts (Ctrl+Shift+C) — both are the dock's own `toggleViewAction()`. Was a modeless dialog until Sep 2026.
- `CellDock` — right-side dock (tabified with Display, hidden until a cif is loaded; View ▸ Dock Panels ▸ Unit Cell): Show = asymmetric unit (default, like Avogadro/Mercury) | unit cell (+ "Complete molecules": molecules cut by the faces reassembled); Disorder = one SHELX PART group (major preselected) | all (overlapping); cell (a/b/c, α/β/γ, volume) + "Show cell in the viewer" (persisted `structure/showCell`); contents (space group, formula (Z), sites, symmetry operations, atoms in asym. unit / unit cell); a/b/c repeats (unit cell only); "Thermal ellipsoids" (shown only with ADPs in the file): toggle + probability %, persisted `structure/ellipsoids`, `structure/ellipsoidProbability`, emits `ellipsoidsChanged`. Every change emits `optionsRequested(CifOptions)` → MainWindow re-reads via `MoleculeFileLoader::load(path, options)`; content/disorder apply at once, repeats on **Apply**. Options reset to the defaults when another file is opened. Drops the file (placeholder page) once the scene's atom count no longer matches the cif.
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

## ProjectDock File Browser Filter
- `DirectoryFilterProxyModel` sits between `QFileSystemModel` and `QListView`; combines live name search with extension subset filtering.
- Embedded compact bar above the content list: search field + `Extensions` popup menu with all suffixes in the current directory + clear button; session-only (resets on restart).
- `MainWindow` resolves view indices back to source indices via `filePathFromContentIndex()`; Lesson/SFTP modes bypass the proxy.

## DisplayDock View Presets
- `ViewPresetWidget` lives below the `DisplayPanel`; manages reproducible camera + display presets (one preset = camera + display together).
- Presets are stored under `viewPresets/` in `QSettings` and survive restarts; the list starts empty.
- `ViewPreset` (`src/viewpreset.h`) holds camera (`rootRotation`, `pan`, `fieldOfView`, `cameraDistance`, `zoomFactor`, `zoomMode`) + display state. `ZoomMode::Absolute` applies the stored distance verbatim; `ZoomMode::Relative` reconstructs distance = `zoomFactor * sceneExtent` so the molecule keeps its on-screen size across different structures.
- `MoleculeViewer::currentViewPreset(ZoomMode)` captures; `applyViewPreset()` restores via the atomic `SceneController::setCameraTransform` + `m_quickView->update()`, then emits `viewPresetApplied()` so `DisplayPanel::syncFromViewer()` re-syncs its controls (no dock raise).
- Quick buttons `Front`/`Top`/`Side` call `MoleculeViewer::setCameraOrientation()` — only rotation, zoom and display stay.

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
