# src/CLAUDE.md - Core Modules Documentation

## Overview

Core modules for file parsing, 3D visualization, and user interface.

## Current Status (November 2025)

### Phase 1: Visualization Settings & Shortcuts ✅ COMPLETE
- ✅ **Settings Persistence** - All visualization settings now saved to QSettings, restored on startup
- ✅ **Fog Controls** - Enable/disable fog with intensity slider in VisualizationSettingsDialog
- ✅ **Keyboard Shortcuts** - 1-4 for rendering modes, +/- for atom size, </> for bond thickness

### Phase 3: Presets & Focus Commands ✅ COMPLETE
- ✅ **Visualization Presets** - Save/load named presets (Publication, Analysis, Presentation built-in)
- ✅ **Focus Commands** - fitAllInView(), centerOnAtom(), zoomToSelection() with proper camera math
- ✅ **Focus Shortcuts** - Ctrl+0/Home to fit, Ctrl+F to focus on selection

### Previously Fixed ✅
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
- **Visualization Controls** (Claude Generated - Phase 1):
  - Rendering modes: Ball-and-stick, Space-filling, Wireframe, Sticks-only
  - Color schemes: CPK (element), Monochrome, By-charge, Custom
  - Material properties: Transparency, shininess, atom scale, bond thickness
  - Fog/depth cueing: Enable/disable with intensity control
- **Focus & Navigation** (Claude Generated - Phase 3):
  - fitAllInView() - Frame entire molecule in viewport
  - centerOnAtom(index) - Focus on specific atom
  - zoomToSelection() - Frame selected atoms
- **Features**: Atom rendering (CPK colors), bond visualization (multiple bonds), frame slider navigation, preset management

### Main Window & Settings (mainwindow.*, settings.*)
- **MainWindow**: Qt-based GUI with file browser, molecule viewer, and output panels
- **Settings**: Persistent application settings storage with QSettings
- **Features**: Visualization settings persistence, preset management, keyboard shortcuts for common operations
- **Shortcuts** (Claude Generated - Phase 1 & 3):
  - 1-4: Rendering mode selection
  - +/-: Atom size adjustment
  - </> (Shift+comma/period): Bond thickness adjustment
  - Ctrl+0 / Home: Fit molecule in view
  - Ctrl+F: Center on selection

## Architecture

```
src/
├── vtfparser.cpp/h          - VTF file parsing (3 frames, 200 atoms, 199 bonds)
├── xyzparser.cpp/h          - XYZ file parsing
├── view.cpp/h               - 3D molecular viewer (Qt3D + custom camera control)
├── orbittransformcontroller.cpp/h - Additional camera controls
├── mainwindow.cpp/h         - Main GUI window with shortcuts and menu system
├── main.cpp                 - Application entry point
├── settings.cpp/h           - Configuration persistence with QSettings
├── visualizationsettingsdialog.cpp/h - Visualization & preset management (Phase 1 & 3)
├── selectionmanager.cpp/h   - Atom selection state management (Phase 2 stub)
├── modifiabletextedit.h     - Custom text edit widget
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

## Future Work (Phase 2 - Deferred)

See **[../TODO.md](../TODO.md)** for detailed Phase 2 roadmap:
- 3D Atom picking with Qt3D ObjectPicker (High priority, 2-3h)
- Visual selection highlighting with emissive materials (Medium, 1h)
- AtomListPanel with QTableView for structure browsing (High, 2-2.5h)
- Measurement overlay text and lines in 3D view (Medium, 1-1.5h)

These features require significant 3D interaction infrastructure and are documented for future implementation.
