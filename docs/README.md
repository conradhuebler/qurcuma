# Qurcuma Documentation

Index of `docs/`. Rules and entry points per source directory are in the `CLAUDE.md` files (root, `src/`, `src/docks/`, `src/dialogs/`, `src/widgets/`, `src/qml/`). Open tasks: `../TODO.md`. Feature wishes: [ROADMAP.md](./ROADMAP.md).

## Architecture (`architecture/`)

Current (Qt Quick 3D renderer), moved from `src/CLAUDE.md`:

- [viewer-scenecontroller.md](./architecture/viewer-scenecontroller.md): viewer, `SceneController`, display, looks, image export
- [nci-overlay.md](./architecture/nci-overlay.md): non-covalent interaction detection and drawing
- [rmsd-workspace.md](./architecture/rmsd-workspace.md): RMSD/align workspace
- [simulation-gui.md](./architecture/simulation-gui.md): simulation dock, walls, thermostat, charts, reactive topology
- [lessons.md](./architecture/lessons.md): OER lesson files
- [interaction-editing.md](./architecture/interaction-editing.md): mouse, hotkeys, edit mode, collisions
- [builder.md](./architecture/builder.md): Build mode, fragments, container fill
- [viewer-ui.md](./architecture/viewer-ui.md): structure sync, appearance dock, menus, palette

Older documents written for the Qt3D renderer (since removed); statements about `CustomFrameGraph`, frustum culling and GPU instancing systems describe that design and are not checked against the current code:

- [rendering-pipeline.md](./architecture/rendering-pipeline.md), [performance-optimization.md](./architecture/performance-optimization.md), [file-parsers.md](./architecture/file-parsers.md)

## Work packages

- [WP-performance.md](./WP-performance.md): bond detection and NCI off the GUI thread, position-only instancing, adaptive quality
- [WP-ux-restructure.md](./WP-ux-restructure.md): UX slimming (modes, looks, menus); status per stage
- [WP-visualization-and-vr.md](./WP-visualization-and-vr.md): older plan (written for Qt3D; the renderer decision is done)
- [WP-remote-compute-vr.md](./WP-remote-compute-vr.md): A visualizes, B computes (SSH), then VR
- [WP0-quick3d-spike.md](./WP0-quick3d-spike.md): renderer migration spike (`../spikes/quick3d/`)
- [UX-settings-audit.md](./UX-settings-audit.md)

## Development (`development/`)

- [phase-timeline.md](./development/phase-timeline.md): history of phases 1-5D (Nov 2025)
- [api-reference.md](./development/api-reference.md), [visualizer-roadmap.md](./development/visualizer-roadmap.md)
- `audit_ui_reach.py`, `count_controls.py`: UI audit scripts

## Build and test

```bash
cmake --build debug; echo CMAKE_EXIT=$?     # development build, ./debug/qurcuma
cmake --build release; echo CMAKE_EXIT=$?   # optimized build
python3 scripts/check_docs.py               # documentation checks; hook: git config core.hooksPath scripts/git-hooks
```

Test executables (`test_vtf_bonds`, `test_nci`, `test_fragments`, `test_recipes`, ...) are CMake targets of the root `CMakeLists.txt`; the sources currently lie in the repository root.
