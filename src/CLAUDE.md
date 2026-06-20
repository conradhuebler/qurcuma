# VTF/XYZ Parsers, 3D Viewer, Mouse Interactions

## Parsing
- VTF format supports periodic boundary conditions
- XYZ format reads atomic coordinates and optional velocities
- Both formats handle large files efficiently

## 3D Viewer
- OpenGL-based rendering with shader support
- Interactive rotation, zoom, and pan controls
- Multiple rendering styles: ball-and-stick, space-filling, wireframe
- ✅ **Screen-fixed corner lights** - 4 toggleable lights (◤◥◣◢). Phong path: `QPointLight`s parented to the camera; instancing path: view-space lights via `cornerLightEnabled` vec4 uniform. Lit zone stays fixed to the view, does NOT rotate with the molecule. (Real cast shadows = future shadow-mapping work.)

## Editors Dock (RMSD-Tab)
- ✅ **RMSD / Align** (`src/rmsdwidget.*`, `RMSDWidget : QWidget`) — dritter Tab im Editors-Dock (`m_editorsTabs`: Structure/Input/RMSD), statt eigenem Dock. Backed by curcuma `RMSDDriver`; reference = Viewer-Struktur, target = Datei. Referenz-Saat per Auto (`showRMSDTool` → Tab-Index 2) + Button (`seedReferenceRequested` → `MainWindow::seedRMSDReference`); Overlay via `overlayRequested` → `MoleculeViewer::showOverlay`.

## Interactive Simulation
- ✅ **RMSD-MTD bias** — `rmsd_mtd` ist kein eigener Treiber, sondern ein Bias-Modus *innerhalb* von curcuma `SimpleMD` (aktiviert via `rmsd_mtd=true` + `rmsd_mtd_*` im `simplemd`-Controller). Im Simulation-Widget als QGroupBox "RMSD Metadynamics" (MD-only, Enable→Details): k/α/atoms/ref-file/max-gaussians/max-height/econv/pace/wtmtd+ΔT/freeze-inherited. `SimulationConfig` hält die Felder; `SimulationWorker::applyRmsdMtdParams()` (file-local, simulationworker.cpp) schreibt sie nur bei `rmsdMtd=true` in den Controller. Defaults aus `external/curcuma/src/capabilities/simplemd.h` "RMSD-MTD"-PARAM-Kategorie (`rmsd_econv` default 1e8, funktional — steuert Bias-Ablagerung via `setEnergyConv`).
- Mouse grab distributes a screen-space drag as Eh/Bohr force across bonded shells (Angstrom-to-Bohr corrected in `computeGrabForce`)
- Grab force is **sticky**: `injectForce` holds it until `clearInjectedForce` (mouse release), and the worker *re-reads* (`currentInjectedForces`, peek not drain) it every MD step / Opt iteration. The viewer only emits `atomForceRequested` on mouse-*move* + once per frame, so the old one-shot drain dropped the bias whenever the cursor held still — fatal for Opt (no momentum: it relaxes straight back). Sticky = "force acts as long as the button is held"
- Opt grab needs all three: (1) sticky force (above); (2) `runOptimization` pumps `QCoreApplication::processEvents()` in the step callback so queued `injectForce`/`clearInjectedForce` reach the worker (its event loop is blocked in synchronous `Optimize()`; the dispatcher exists — MD's `QTimer` fires — so the pump delivers; MD never blocks so it needs no pump); (3) curcuma applies the bias at each optimizer's own gradient eval (LBFGSpp objective, native `LBFGS::getEnergyGradient`, ANCOpt) — the old base-loop bias was inert
- Reset restores snapshot index 0, which is captured automatically when a molecule is loaded
- Snapshots tab provides manual take/restore/delete history plus an auto-snapshot stride in Simulation tab (every N steps/iterations, 0 = off)
- GPU dropdown shows only compiled backends (CUDA/ROCm/Vulkan via `USE_*` CMake options); curcuma `feature/vulkan_rocm` branch
- CLI `qurcuma <file> -md|-opt` loads the file and auto-starts the interactive simulation from bash
- Release/AVX-512 start crash fixed: `CMakeLists.txt` matches curcuma's `-march=native` on the qurcuma target so both share Eigen's `EIGEN_MAX_ALIGN_BYTES` (mismatch caused `double free` in `moleculeToFrame`)

## Mouse Interactions
- Left-click: rotate view
- Right-click: context menu for atom selection
- Scroll wheel: zoom in/out
- Middle-click: pan view