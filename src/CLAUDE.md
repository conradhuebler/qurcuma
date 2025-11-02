# src/CLAUDE.md - Qurcuma Project Overview

**Qt-based molecular visualization GUI for VTF/XYZ/PDB/MOL2 formats with 3D rendering, GPU instancing, and performance optimization for molecules >1000 atoms.**

## Phase Status Overview

| Phase | Status | Lines | Details |
|-------|--------|-------|---------|
| 1-3 | ✅ COMPLETE | 1,500+ | Settings, Navigation, Workspace | [phase-timeline](../docs/development/phase-timeline.md) |
| 4A/4B | ✅ COMPLETE | 1,550+ | PBR Shaders, Bond Editor | [api-ref](../docs/development/api-reference.md) |
| 5A/5B | ✅ COMPLETE | 2,000+ | Multi-pass FrameGraph, SSAO, Bloom, HDR | [rendering](../docs/architecture/rendering-pipeline.md) |
| 5C | ✅ COMPLETE | 950+ | PDB & MOL2 Format Support | [parsers](../docs/architecture/file-parsers.md) |
| 5D | 🚧 IN PROGRESS | 700+ | GPU Instancing, Frustum Culling, Async Loading | [performance](../docs/architecture/performance-optimization.md) |

**Detailed Documentation:** [docs/README.md](../docs/README.md)

## Core Modules

| Module | File | Purpose |
|--------|------|---------|
| **3D Viewer** | view.cpp/h | Qt3D molecular visualization + arcball rotation |
| **File Parsers** | *parser.cpp/h | VTF, XYZ, PDB, MOL2 format support |
| **GPU Instancing** | atominstancingsystem.cpp/h | Single draw call for >1000 atoms (Phase 5D) |
| **Frustum Culling** | frustumculler.cpp/h | Off-screen atom culling, 30-50% draw reduction |
| **Async Loading** | fileloadingworker.cpp/h | QThread background file parsing |
| **Selection** | selectionmanager.cpp/h | Atom selection & state management |
| **Measurements** | measurementoverlay.cpp/h | Distance/angle/dihedral calculations |
| **Rendering** | customframegraph.cpp/h | 4-pass pipeline: Geometry→SSAO→Blur→Composite |
| **Performance** | performanceoptimizer.cpp/h | LOD system, quality auto-detection |
| **Workspace** | workspacemanager.cpp/h | Save/restore full application state |
| **Settings** | settings.cpp/h | QSettings persistence (visualization + performance) |

## Build & Test

```bash
# Build
cmake --build debug      # Full debug symbols, no optimizations
cmake --build release    # O3 optimizations, production build

# Test
./debug/test_vtf_bonds   # Bond parsing (expects 199 bonds)
./debug/test_vtf_frames  # Frame detection (expects 3 frames)
./debug/test_vtf_full    # End-to-end validation
```

## Development Guidelines

- **Code Marking:** Add `// Claude Generated - Phase X` comment for traceability
- **Documentation:** Doxygen-ready with scientific context (equations, references)
- **Git Workflow:** Only commit source files, test before commit, no warnings/errors
- **Commit Format:** `"Verb: Brief description (Phase X)"`
- **Architecture:** Prefer flat over hierarchical, minimal abstraction layers

See [docs/](../docs/) for detailed architecture and API documentation.
