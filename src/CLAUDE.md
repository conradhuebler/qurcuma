# VTF/XYZ Parsers, 3D Viewer, Mouse Interactions

## Parsing
- VTF format supports periodic boundary conditions
- XYZ format reads atomic coordinates and optional velocities
- Both formats handle large files efficiently

## 3D Viewer
- OpenGL-based rendering with shader support
- Interactive rotation, zoom, and pan controls
- Multiple rendering styles: ball-and-stick, space-filling, wireframe

## Interactive Simulation
- Mouse grab distributes a screen-space drag as Eh/Bohr force across bonded shells (Angstrom-to-Bohr corrected in `computeGrabForce`)
- Opt Auto-Run refreshes injected forces every iteration via the step callback (live response to user drag)
- Reset restores snapshot index 0, which is captured automatically when a molecule is loaded
- Snapshots tab provides manual take/restore/delete history plus an auto-snapshot stride (every N steps/iterations, 0 = off)

## Mouse Interactions
- Left-click: rotate view
- Right-click: context menu for atom selection
- Scroll wheel: zoom in/out
- Middle-click: pan view