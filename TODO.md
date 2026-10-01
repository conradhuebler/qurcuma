# TODO

Open code tasks only, at most 3 lines each. Feature wishes: `docs/ROADMAP.md`. Plans: `docs/WP-*.md`.

## Viewer / performance
- Performance WP P1/P2/P5/P4 open: bond detection still O(N^2) on the GUI thread every MD step; then position-only instancing, adaptive GPU quality, NCI/collisions off-thread. `docs/WP-performance.md`
- "By Charge" colouring: charges only arrive as a side product of the NCI analysis (current frame). Decide source: GFN-FF EEQ directly, or from calculation/files.

## Simulation
- Periodic container (`wallPotential` pbc) is a stopgap: no minimum image in the non-bonded terms, occupied target is reflected elastically. Curcuma side: `external/curcuma/docs/WP-PERIODIC-NONBONDED.md`.
- Lessons: result/target fields reserved in the schema, not implemented; no barostat, "pressure" is wall volume only.

## Remote
- Remote compute (A controls/visualizes, B computes) and VR: plan in `docs/WP-remote-compute-vr.md`; R0 to R2 are in, next is R3 (reconnect, abort behaviour). Open: server capabilities (methods, GPUs) are not yet shown in the dock; "Compute on" is untested with real ssh and in the GUI.

## Docs / repository
- Qt3D-era docs in `docs/architecture/` (rendering-pipeline, performance-optimization, file-parsers) describe removed code; rewrite or archive.
- 14 test sources lie in the repository root; move to `tests/` together with the CMake paths (one configure, curcuma rebuilds ~10 min).
- `.claude/plan.md` (view presets) is untracked; decide: move to `docs/WP-view-presets.md` or drop.
