# Rendering Pipeline Architecture

## Overview

The Qurcuma rendering pipeline is built on Qt3D with a custom multi-pass frame graph implementation. The system supports advanced visual effects including screen-space ambient occlusion (SSAO), bloom/glow, and HDR tone mapping.

## Architecture Diagram

```
Input: Molecule Data (atoms, bonds, colors)
    ↓
[Pass 1: Geometry] → G-Buffer (Color, Depth, Normals)
    ↓
[Pass 2: SSAO] → SSAO Texture (ambient occlusion)
    ↓
[Pass 3: Blur] → Blurred SSAO + Bloom Bright Pass
    ↓
[Pass 4: Composite] → Final Image with HDR Tone Mapping
    ↓
Output: Display Surface
```

## Components

### CustomFrameGraph (customframegraph.cpp/h)

**Purpose:** Multi-pass rendering pipeline orchestration

**Key Features:**
- 4-pass architecture: Geometry → SSAO → Blur → Composite
- G-Buffer setup with multiple render targets
- Filter key-based pass routing
- HDR support with Float16 render targets

**Render Targets:**
- Color: RGBA16F (half-float for HDR)
- Depth: D24S8 (24-bit depth + 8-bit stencil)
- Normals: RGB16F (for SSAO calculations)

**Configuration:**
```cpp
// Viewport dimensions (typically 1920x1080)
// G-Buffer resolution matches viewport
// SSAO and Bloom intermediate targets at full resolution
```

### Pass 1: Geometry Pass

**Input:** MoleculeViewer scene graph with atom entities and bond cylinders

**Processing:**
- Standard Phong/PBR material rendering
- Outputs position, depth, normal, color, specular to G-Buffer
- Uses filter key `geometryPass` for routing

**Output:** G-Buffer with 3 render targets

### Pass 2: SSAO (Screen-Space Ambient Occlusion)

**Purpose:** Add depth-based ambient shadows for better depth perception

**Shader:** `shaders/ssao.vert` and `shaders/ssao.frag`

**Parameters:**
- **Intensity** (0.0-2.0, default 1.0): How strong the occlusion effect
- **Radius** (0.01-0.2, default 0.1): Sampling radius in screen space
- **Bias** (0.0-0.1, default 0.025): Prevents acne artifacts

**Algorithm:**
1. Sample random points in hemisphere around fragment
2. Compare depths to determine occlusion
3. Mix with base color: `finalColor = baseColor * (1.0 - ssao_intensity * occlusion)`

**Performance:** ~2-5ms on typical GPU at 1920x1080

### Pass 3: Bloom & Blur

**Purpose:** Create glow effects on bright atoms/bonds

**Components:**

1. **Bloom Bright Pass** (`shaders/bloom_bright.frag`)
   - Extract bright pixels (luminance > threshold)
   - Soft knee for smooth transitions
   - Threshold: 0.5-1.5 (default 1.0)

2. **Gaussian Blur** (horizontal + vertical)
   - 7-tap blur kernels
   - Separable blur for performance
   - Shaders: `blur_horizontal.frag`, `blur_vertical.frag`

3. **Bloom Composite** (`shaders/bloom_composite.frag`)
   - Add blurred bloom to base image
   - Intensity: 0.0-2.0 (default 0.5)
   - Additive blending: `finalColor = baseColor + bloom * intensity`

**Total Bloom Overhead:** ~3-4ms

### Pass 4: Composite & Tone Mapping

**Purpose:** Final image assembly with HDR tone mapping and gamma correction

**Shader:** `shaders/tonemapping.frag`

**Tone Mapping Operator:** Reinhard Global
- Formula: `Lout = Lfog / (1.0 + Lout)`
- Reduces HDR values to LDR display range
- Preserves local contrast

**Parameters:**
- **Exposure** (0.5-3.0, default 1.0): Exposure compensation for overall brightness

**Gamma Correction:**
- sRGB gamma: `color = pow(color, vec3(1.0/2.2))`
- Applied after tone mapping

**Final Output:** LDR image (8-bit per channel) for display

---

## Integration with GPU Instancing

**Phase 5D Enhancement:** The geometry pass supports GPU instancing via `AtomInstancingSystem`:

```cpp
// Geometry pass now routes to either:
// - Standard per-atom entities (small molecules)
// - Single instanced draw call (>1000 atoms)

// Selection:
if (atomCount > performanceOptimizer.getInstanceThreshold()) {
    useInstancingSystem();  // Single draw call
} else {
    useStandardRendering();  // Per-entity picking works
}
```

---

## Material System

### Phong Material (Default)

**Shader:** Built-in Qt3D Phong

**Properties:**
- Ambient, Diffuse, Specular
- Shininess (typically 80 for atoms)
- Per-vertex lighting

### PBR Material (Phase 4A)

**Shader:** `shaders/pbr.vert/frag`

**BRDF:** Cook-Torrance

**Parameters:**
- Metallic (0.0-1.0)
- Roughness (0.0-1.0)
- AO (Ambient Occlusion)
- Base Color (RGB)

**Components:**
- Fresnel-Schlick function
- GGX distribution
- Schlick-GGX geometry function

**Educational References:**
- LearnOpenGL: https://learnopengl.com/PBR/Theory
- "Real Shading in Unreal Engine 4" (Karis, 2013)

---

## Performance Characteristics

| Effect | Duration | Memory | Notes |
|--------|----------|--------|-------|
| Geometry Pass | 2-5ms | Per-atom | Uses LOD with rings/slices |
| SSAO | 2-5ms | G-Buffer | Configurable radius/intensity |
| Bloom | 3-4ms | Intermediate FBOs | 7-tap separable blur |
| Tone Mapping | <1ms | Final composite | Negligible overhead |
| **Total Overhead** | **7-15ms** | **~300MB** | At 1920x1080 |

**Target:** 60 FPS (16.67ms per frame) with all effects enabled on RTX 2070+

---

## Future Enhancements

- Physically-based bloom (energy conservation)
- Depth of field effects
- Screen-space reflections (SSR)
- Temporal anti-aliasing (TAA)
- Adaptive quality scaling based on GPU load

---

## References

- **Qt3D Documentation:** https://doc.qt.io/qt-6/qtenclosing3d-index.html
- **Frame Graph Concept:** Real-Time Rendering (Akenine-Möller et al., 4th ed.)
- **SSAO Algorithm:** Crysis SSAO implementation (Kajalin, 2009)
- **Bloom:** "Post-processing in the Orange Box" (Valve, 2008)
