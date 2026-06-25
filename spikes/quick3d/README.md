# WP0 — Qt Quick 3D + Vulkan Spike

Standalone proof-of-concept for migrating qurcuma's renderer from Qt3D to **Qt Quick 3D**
(Vulkan RHI, built-in SSAO/shadows/bloom/tonemapping, instanced atoms/bonds, picking).
See `docs/WP0-quick3d-spike.md` for the full task list and success criteria (K1–K6).

This tree is **independent** of the qurcuma build — it has its own `CMakeLists.txt`,
its own self-contained data layer (no qurcuma/Qt3D headers), and is configured/built
separately.

## Build

```bash
cmake -S spikes/quick3d -B spikes/quick3d/build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/opt/cuda
cmake --build spikes/quick3d/build -j
```

The binary is `spikes/quick3d/build/quick3d_spike`.

## Run

```bash
# Vulkan (default), confirm the backend in the log:
QSG_INFO=1 ./spikes/quick3d/build/quick3d_spike

# OpenGL fallback for comparison (T5):
./spikes/quick3d/build/quick3d_spike --gl

# Embedding route (T6): QQuickWidget (default) vs native window container:
./spikes/quick3d/build/quick3d_spike --embed=quickwidget
./spikes/quick3d/build/quick3d_spike --embed=container

# Load a real molecule:
./spikes/quick3d/build/quick3d_spike some_molecule.xyz
```

The active RHI backend is printed (`Quick3D spike: RHI backend = …`) and shown in both
the side panel and the in-scene HUD (confirms K6).

## Controls

- **Side panel:** dataset (1k/5k/10k synthetic carbon grid), load XYZ, effect toggles
  (SSAO / Shadows / Bloom / Tonemap / Bonds), Animate (MD-proxy), live FPS + backend.
- **Mouse:** drag = orbit, wheel = zoom (Qt's `OrbitCameraController`).
- **Click an atom** = pick → element/index/coords in the HUD + side panel (T7/K4).
- **HUD buttons:** Reset view, Screenshot (writes `spike_<ts>.png` to the CWD).

## Measurement protocol (T8)

Toggle a dataset + effects + animation, read the FPS from the HUD/side panel, and fill in:

| Szenario | Atome | Effekte | Backend | Einbettung | FPS | Frame ms | Notizen |
|---|---|---|---|---|---|---|---|
| statisch | 1k  | aus                  | Vulkan | QQuickWidget    |   |   |   |
| statisch | 10k | aus                  | Vulkan | QQuickWidget    |   |   |   |
| statisch | 10k | SSAO+Schatten+Bloom  | Vulkan | QQuickWidget    |   |   |   |
| statisch | 10k | SSAO+Schatten+Bloom  | Vulkan | WindowContainer |   |   |   |
| animiert | 10k | aus                  | Vulkan | (Empfehlung)    |   |   |   |
| animiert | 10k | SSAO+Schatten        | Vulkan | (Empfehlung)    |   |   |   |
| statisch | 10k | SSAO+Schatten+Bloom  | OpenGL | (Vergleich)     |   |   |   |

`Frame ms ≈ 1000 / FPS`. Take screenshots (HUD button) with/without effects and for both
embedding routes for the report.

## Notes / known caveats

- Quick3D built-in `#Sphere`/`#Cylinder` are 100-unit primitives; the instance scales
  divide by the base radius/height (see `atominstancing.cpp`, `bondinstancing.cpp`).
  Calibrate visually if proportions look off.
- `QQuickWidget` + Vulkan has historically been finicky; if you see artifacts, prefer
  `--embed=container` and note it in the spike report.
- **Out of scope here:** VR/XR (T9 — needs Monado/ALVR), migration, feature parity.

## Files

| File | Role |
|---|---|
| `main.cpp` | shell, Vulkan/GL selection, both embedding routes, FPS meter |
| `Main.qml` | `View3D` + `ExtendedSceneEnvironment` + instanced models + picking + HUD |
| `scenecontroller.*` | shared model: data, instancing, effect toggles, MD-proxy, selection |
| `atominstancing.*` | `QQuick3DInstancing` for atom spheres |
| `bondinstancing.*` | `QQuick3DInstancing` for bond half-cylinders (quaternion orient) |
| `moleculedata.*` | grid generator, XYZ loader, CPK colours/radii, bond detection |
