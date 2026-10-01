# src/: parsers, 3D viewer, simulation GUI, editing

Detail per topic lives in `docs/architecture/`; this file keeps rules and entry points only. Sub-directories: `docks/`, `dialogs/`, `widgets/`, `qml/` (each with its own CLAUDE.md where present).

## Parsing
- VTF (periodic boundary conditions, coarse-grained beads) and XYZ parsers; the VTF `atom` record is parsed keyword-based, not by column. [file-parsers.md](../docs/architecture/file-parsers.md)

## 3D viewer
- Renderer is Qt Quick 3D on the Vulkan RHI (OpenGL fallback). `MoleculeViewer` (`view.*`) is a `QWidget` wrapping a `QQuickWidget` + `SceneController` (`scenecontroller.*`) + `qml/viewer3d.qml`.
- Quick3D `Viewport`/`Camera` are private headers: the camera is axis-aligned, the molecule rotates under `moleculeRoot`, and C++ replicates the projection for picking. [[quick3d-private-headers]]
- `SceneController::schemeColor` is the single colour source for atoms, bonds and overlays; the viewer is the single source of truth for display state, panels only read (`syncFromViewer()`).
- Looks (`look.h`) set colour/material/fog/effects only, never quick toggles.
- Performance plan: [WP-performance.md](../docs/WP-performance.md). Details: [viewer-scenecontroller.md](../docs/architecture/viewer-scenecontroller.md).
- Open (to clarify): source of charges for "By Charge"; today only a side product of the NCI analysis.

## Analysis overlays
- NCI overlay: free functions in `ncianalysis.*` (`namespace nci`), worker on its own thread, contacts drawn as dashed segments. Tests `test_nci`, `test_nci_gfnff`. [nci-overlay.md](../docs/architecture/nci-overlay.md)
- RMSD workspace (`rmsdwidget.*`): reference plus tinted overlays, curcuma `RMSDDriver`. [rmsd-workspace.md](../docs/architecture/rmsd-workspace.md)
- Tracked quantities in the chart (`measurements.*`) are a Qt adapter over curcuma geometry functions; no second implementation here. Test `test_measurements`.

## Interactive simulation
- `SimulationWorker` runs curcuma `SimpleMD`/optimizers on its own thread; the GUI sends only parameters that differ from curcuma defaults (run log, `runlog.h`). Recipes (`recipe.h`) never change the system. Test `test_recipes`.
- A stale `stop` file in the CWD aborts curcuma runs at step 0; the worker removes it at run start.
- Grab force is sticky (held until mouse release, re-read every MD step / Opt iteration).
- Reactive GFN-FF (`topology_mode=react`): the viewer draws curcuma's own bond list, the geometric hysteresis is only the fallback.
- Details, walls, thermostat, temperature ramps, charts: [simulation-gui.md](../docs/architecture/simulation-gui.md).

## Lessons
- `*.qlesson.json` bundles structures plus one `SimulationConfig` each; `simConfigToJson/FromJson` is the only lossless round trip. [lessons.md](../docs/architecture/lessons.md)
- Not implemented: pressure as a barostat (Haber-Bosch scenarios use wall volume only).

## Interaction and editing
- Left-drag rotate, right-drag pan, wheel zoom, middle-click reset, click picks. Interaction modes are one exclusive enum (`None/Edit/Measure/BondEdit/Build`) switched in `setInteractionMode()`; Esc steps back one level.
- WASD/QE rotate the scene (filter in `MainWindow::eventFilter`); active in Edit, in a running simulation, and in View/Measure while the viewport has focus.
- Count-changing edits are single-frame only (`canEditStructure`); undo goes through snapshots.
- [interaction-editing.md](../docs/architecture/interaction-editing.md), [builder.md](../docs/architecture/builder.md) (`buildtools.*`, `fragmentlibrary.*`, `scenefiller.*`; tests `test_buildtools`, `test_scenefiller`).

## Structure sync and UI
- Viewer is the canonical store; atom table and structure editor mirror it, `m_structSyncing` breaks feedback loops, the text mirror pauses during MD.
- One shared `QAction` set serves menu, bar, context menu and command palette. Dock layout and app modes: `docks/CLAUDE.md`. [viewer-ui.md](../docs/architecture/viewer-ui.md)
- Release builds match curcuma's `-march=native` on the qurcuma target (Eigen alignment mismatch caused a double free).

## Status
- All entries in `docs/architecture/` were moved unchanged from this file and are 🤖 AI-generated and ⚙️ machine-tested where a test is named; none is marked operator-tested here.
