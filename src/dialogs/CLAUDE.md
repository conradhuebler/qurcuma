# src/dialogs/CLAUDE.md - Dialog Modules Documentation

## Overview

Specialized dialog windows and analysis tools for molecular visualization.

## Current Modules

### NMR Spectrum Dialog (nmrspectrumdialog.*, nmrcontroller.*, nmrdatastore.*, nmrstructureproxymodel.*)
- **Purpose**: Manage and visualize NMR spectral data for multiple structures
- **Status**: Functional with structure loading and spectrum management
- **Features**:
  - Load NMR data from computational chemistry output files
  - Display spectral analysis in organized structure groups
  - Proxy model for filtering and organization
  - Persistent data storage

### Frequency Analysis Dialog (frequencydialog.h)
- **Purpose**: Analyze vibrational frequencies from quantum chemistry calculations
- **Status**: Integrated with main window for frequency selection and visualization

## Architecture

```
dialogs/
├── nmrspectrumdialog.cpp/h          - Main NMR spectrum dialog window
├── nmrcontroller.cpp/h              - NMR data controller and logic
├── nmrdatastore.cpp/h               - Data storage and persistence (JSON-based)
├── nmrstructureproxymodel.cpp/h     - Qt proxy model for filtering/organization
└── frequencydialog.h                - Frequency analysis dialog (header-only)
```

## Known Issues

- **NMR_LOG macro conflict**: Defined in both nmrdatastore.h and nmrspectrumdialog.h (causes compiler warnings)
  - **Fix**: Consolidate NMR_LOG to single location or rename one instance

## Development Guidelines

- Maintain JSON format compatibility for NMR data storage
- Use Qt's model/view architecture for data organization
- Keep UI responsive during file loading operations
