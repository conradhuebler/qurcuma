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

### SFTP Remote File Dialog (sftpdialog.*)
- **Purpose**: Connect to HPC clusters and open remote molecular structure files via SFTP
- **Status**: ✅ Production-ready with full authentication & caching (Phase SFTP Integration complete)
- **Features**:
  - **Authentication**: Password + SSH key auto-detection (id_rsa, id_ed25519)
  - **SSH Config Integration**: Parses ~/.ssh/config for host aliases, ports, users
  - **Connection Profiles**: Save/load connection settings (like bookmarks)
  - **Recent Connections**: Menu with last 5 connections + timestamps
  - **Intelligent Caching**: SHA-256 hash-based file cache with hit detection
  - **Lazy Loading**: Remote directories load on-demand (QAbstractItemModel::fetchMore)
  - **Progress Feedback**: QProgressDialog for connect/download operations
  - **Error Reporting**: Detailed SSH error messages from libssh
  - Keyboard shortcut: Ctrl+Shift+R
- **Dependencies**: libssh 0.11.3+ (system-wide installation required)
- **Architecture**:
  - `sftpdialog.cpp/h` - Main dialog with profile/SSH config dropdowns
  - `sftpmodel.hpp` - QAbstractItemModel with lazy loading + key auth
  - `sshconfig.cpp/h` - Parser for ~/.ssh/config files
  - `sftpcache.cpp/h` - Cache manager (/tmp/qurcuma_sftp/)
  - `Settings` - SftpConnectionProfile persistence (QSettings)

### Add Computer Dialog (addcomputerdialog.*)
- **Purpose**: add a computer for remote runs (address, port, key), test ssh and `qurcuma-server`; also holds the computer list shared by the Simulation dock and the Remote Files dialog (`fillHostCombo`; ~/.ssh/config is not read). Details: `src/remote/CLAUDE.md`. 🤖 built, not checked in the GUI.

### Settings Dialog (settingsdialog.*)
- **Purpose**: one dialog (Edit ▸ Preferences ▸ Settings..., Ctrl+,) with tabs Programs (path, Browse, Detect, live status per program), General (working directory and known ones, invocation directory, dark mode, clear recent files), Display (`centerOnLoad`), Computers (`QURCUMA_REMOTE` only) and Operator (name, ORCID with check digit, institution, license); writes `Settings` only on OK.
- Computers: Add/Edit/Remove through `AddComputerDialog` act at once, outside OK/Cancel. The dialog does not know MainWindow: what the running window must apply leaves through signals (`darkModeChanged`, `workingDirectoryChosen`, `computersChanged`, ...), connected in `MainWindow::openSettingsDialog`. 🤖 not checked in the GUI.
- GUI-free logic in `src/programpaths.*` (program table, `detect`, `validate`, `checkOrcid`, ORCA key migration), test `test_programpaths`. Paths live under `programs/<name>`; ORCA is a folder that must hold `orca` plus an `orca_*` helper. 🤖 built, ⚙️ logic tested, not checked in the GUI.

## Architecture

```
dialogs/
├── nmrspectrumdialog.cpp/h          - Main NMR spectrum dialog window
├── nmrcontroller.cpp/h              - NMR data controller and logic
├── nmrdatastore.cpp/h               - Data storage and persistence (JSON-based)
├── nmrstructureproxymodel.cpp/h     - Qt proxy model for filtering/organization
├── sftpdialog.cpp/h                 - SFTP remote file browser dialog
└── frequencydialog.h                - Frequency analysis dialog (header-only)
```

## Known Issues

- **NMR_LOG macro conflict**: Defined in both nmrdatastore.h and nmrspectrumdialog.h (causes compiler warnings)
  - **Fix**: Consolidate NMR_LOG to single location or rename one instance

## Development Guidelines

- Maintain JSON format compatibility for NMR data storage
- Use Qt's model/view architecture for data organization
- Keep UI responsive during file loading operations
