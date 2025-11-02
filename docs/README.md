# Qurcuma Documentation

Comprehensive documentation for the Qurcuma molecular visualization project, organized by architecture, features, and development phases.

## Quick Navigation

- **[Project Status](../src/CLAUDE.md)** - Current phase overview and status
- **[Architecture](./architecture/)** - Technical design and system details
- **[Features](./features/)** - User-facing functionality guides
- **[Development](./development/)** - Developer guides and API reference

---

## Architecture & Design

Technical documentation for core systems:

- **[Rendering Pipeline](./architecture/rendering-pipeline.md)** - CustomFrameGraph, multi-pass rendering, SSAO, Bloom, HDR tone mapping
- **[Performance Optimization](./architecture/performance-optimization.md)** - GPU instancing, frustum culling, LOD system, async file loading
- **[File Parsers](./architecture/file-parsers.md)** - VTF, XYZ, PDB, MOL2 format parsing and integration

---

## Features & Functionality

User-facing documentation for major features:

- **[Visualization Settings](./features/visualization-settings.md)** - Rendering modes, materials, presets, fog control
- **[Selection & Measurements](./features/selection-measurements.md)** - Atom selection, distance/angle/dihedral measurements
- **[Workspace & Bookmarks](./features/workspace-bookmarks.md)** - Workspace save/restore, bookmarks, directory navigation
- **[Bond Editor](./features/bond-editor.md)** - Adding, removing, and modifying bonds with validation

---

## Development & API

Information for developers extending Qurcuma:

- **[Phase Timeline](./development/phase-timeline.md)** - Chronological history of development phases (1-5D)
- **[API Reference](./development/api-reference.md)** - Quick reference for important classes and methods
- **[Testing Guide](./development/testing.md)** - Building, testing, and debugging

---

## By Phase

Detailed information organized by development phase:

| Phase | Title | Status |
|-------|-------|--------|
| 1-3 | Settings, Navigation, Workspace | [Complete](./development/phase-timeline.md#phases-1-3) |
| 4A/4B | PBR & Bond Editor | [Complete](./development/phase-timeline.md#phase-4ab) |
| 5A/5B | Advanced Rendering | [Complete](./development/phase-timeline.md#phases-5a5b) |
| 5C | File Format Support | [Complete](./development/phase-timeline.md#phase-5c) |
| 5D | Performance Optimization | [In Progress](./development/phase-timeline.md#phase-5d) |

---

## Key Statistics

- **Total Lines of Code:** 8,000+
- **Phases Completed:** 5A, 5B, 5C
- **Phases In Progress:** 5D (GPU Instancing, Frustum Culling, Async Loading)
- **File Formats Supported:** VTF, XYZ, PDB, MOL2
- **Performance Target:** >60 FPS for 5000+ atoms with GPU instancing

---

## File Structure

```
qurcuma/
├── src/                              - Source code
│   ├── view.cpp/h                   - 3D molecule viewer
│   ├── mainwindow.cpp/h             - Main application window
│   ├── customframegraph.cpp/h       - Multi-pass rendering
│   ├── atominstancingsystem.cpp/h   - GPU instancing
│   ├── frustumculler.cpp/h          - Frustum culling
│   ├── fileloadingworker.cpp/h      - Async file loading
│   ├── *parser.cpp/h                - File format parsers
│   └── CLAUDE.md                    - Quick project overview
├── docs/                            - Detailed documentation (this folder)
├── debug/                           - Debug build artifacts
├── release/                         - Release build artifacts
└── CMakeLists.txt                  - Build configuration
```

---

## Development Workflow

### Build Commands
```bash
# Debug build (full symbols, no optimizations)
cmake --build debug

# Release build (O3 optimizations, stripped symbols)
cmake --build release

# Run tests
./debug/test_vtf_bonds
./debug/test_vtf_frames
./debug/test_vtf_full
```

### Commit Guidelines
1. Only commit source files (`.cpp`, `.h`, `.md`, `.glsl`)
2. Never commit build artifacts or executables
3. Format: `"Verb: Brief description"` (e.g., "Add:", "Fix:", "Improve:")
4. Include phase marker: `Phase 5D: GPU Instancing System`

### Code Marking
- New functions: Add `// Claude Generated - Phase X` comment
- Include Doxygen-ready documentation with scientific context
- Reference equations, papers, or physics principles where applicable

---

## Links & Resources

- **Main Documentation:** [src/CLAUDE.md](../src/CLAUDE.md)
- **Issue Tracking:** GitHub Issues
- **Build Guide:** See `docs/development/testing.md`
- **API Documentation:** Auto-generated Doxygen (not included in repo)

---

**Last Updated:** November 2025 | **Phase:** 5D (In Progress)
