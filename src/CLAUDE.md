# src/CLAUDE.md - Core Modules Documentation

## Overview

Core modules for file parsing, 3D visualization, and user interface.

## Current Status (October 2025)

### Recently Fixed ✅
- ✅ **VTF Bond Parsing Bug** - Changed `QMap<int, VTFBond>` to `QVector<VTFBond>` (allowed multiple bonds per atom)
- ✅ **VTF Frame Parsing Bug** - Removed global `parseError` flag stopping loop; now detects all 3 frames correctly
- ✅ **Mouse Interactions** - Disabled QOrbitCameraController, implemented arcball rotation + pan + zoom
- ✅ **Test Infrastructure** - Added comprehensive VTF parsing tests: `test_vtf_bonds.cpp`, `test_vtf_frames.cpp`, `test_vtf_full.cpp`

## Modules

### VTF/XYZ Parsers (vtfparser.*, xyzparser.*)
- **VTFParser**: Parses VTF trajectory files with multi-frame support, handles atom definitions, bonds, and unit cells
- **XYZParser**: Parses XYZ structure/trajectory files
- **Known Issues**: XYZ parser needs trajectory support (currently only basic structure parsing)

### 3D Molecule Viewer (view.*)
- **MoleculeViewer**: Qt3D-based 3D molecular visualization with trajectory frame navigation
- **Mouse Interactions** (Claude Generated):
  - Left click: Arcball rotation around view center (quaternion-based, gimbal-lock free)
  - Right click: Pan/translate view (distance-proportional)
  - Mouse wheel: Zoom in/out with minimum distance clamping
  - Middle click: Reset to molecule center
- **Features**: Atom rendering (CPK colors), bond visualization (including multiple bonds), frame slider navigation

### Main Window & Settings (mainwindow.*, settings.*)
- **MainWindow**: Qt-based GUI with file browser, molecule viewer, and output panels
- **Settings**: Persistent application settings storage
- **Status**: Functional with basic file loading and navigation

## Architecture

```
src/
├── vtfparser.cpp/h          - VTF file parsing (3 frames, 200 atoms, 199 bonds)
├── xyzparser.cpp/h          - XYZ file parsing
├── view.cpp/h               - 3D molecular viewer (Qt3D + custom camera control)
├── orbittransformcontroller.cpp/h - Additional camera controls
├── mainwindow.cpp/h         - Main GUI window
├── main.cpp                 - Application entry point
├── settings.cpp/h           - Configuration persistence
├── frequencydialog.h        - Frequency analysis dialog
└── dialogs/                 - Modal dialogs for specialized features
```

## Testing

**VTF Parser Tests** (all passing):
- `test_vtf_bonds.cpp` - Validates bond parsing (expects 199 bonds from scnp_cut.vtf)
- `test_vtf_frames.cpp` - Validates frame detection (expects 3 frames)
- `test_vtf_full.cpp` - End-to-end validation combining both tests

Run tests: `./release/test_vtf_bonds`, `./release/test_vtf_frames`, `./release/test_vtf_full`

## Development Guidelines

- Mark new functions as "Claude Generated" for traceability
- Document new functions briefly with scientific context
- Keep camera interactions intuitive: rotate around view center, not origin
- Maintain backward compatibility for file format support
