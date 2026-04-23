# Visualizer Roadmap

Sanierung und Ausbau von `src/view.*` und zugehörigen Rendering-Modulen. Dieses Dokument beschreibt den aktuellen Zustand nach Cleanup (Phase 0), die Zielarchitektur und einen mehrstufigen Umbau.

## Kontext

Der 3D-Viewer war über mehrere Phasen mit parallelen Rendering-Pfaden gewachsen:

- `QPhongMaterial` (Qt3D-Builtin) für kleine Moleküle,
- `PBRMaterial` mit Cook-Torrance-Shader (Phase 4A),
- `AtomInstancingSystem` + `BondInstancingSystem` mit eigenen Shadern ab Threshold ≥500 Atome (Phase 3.1/3.2).

Jede Render-Einstellung musste mehrfach verdrahtet werden. Post-Processing (SSAO, Bloom, HDR) existierte nur als totes Skelett (`CustomFrameGraph`), Fog war explizit nicht implementiert. `m_atomEntities`/`m_atomMaterials` blieben im Instancing-Pfad leer — alle `update*`-Funktionen wurden still no-op. `view.cpp` erreichte 3056 Zeilen mit gemischten Verantwortlichkeiten (Scene-Graph, Input, Trajectory, Simulation, Screenshot).

## Phase 0 — Cleanup (abgeschlossen)

Tote Module entfernt, auskommentierte Blöcke raus, redundante Wrapper weg.

- Gelöscht: `customframegraph.cpp/.h`, `fullscreenquad.cpp/.h`, `ssaorenderer.cpp/.h`, `frustumculler.cpp/.h`, `fileloadingworker.cpp/.h`.
- `QOrbitCameraController` komplett entfernt — Rotation läuft nur noch über `eventFilter`.
- `m_frameGraph`-Member + alle `if (m_frameGraph) …`-Zugriffe weg.
- Dupliziert-tote Methoden entfernt: `resetCamera()`, `showTrajectoryFrame()`, `setVTFTrajectoryData()`.
- SSAO/Bloom/HDR/Exposure-Setter zu 1-Zeilern reduziert (speichern Wert, keine Render-Wirkung bis Phase 2).
- Auskommentierte Framegraph-Init in `setupViewer` + `resizeEvent` raus.
- Shader-Ressourcen (`src/shaders/*.frag`, `.vert`) bleiben im `resources.qrc` erhalten für Wiederverwendung in Phase 2/3.

Resultat: `view.cpp` 3056 → 2928 Zeilen, `view.h` 443 → 433 Zeilen, CMakeLists um 8 Einträge kürzer.

## Phase 1 — Fog (teilweise abgeschlossen)

Exponential-Squared-Distance-Fog in bestehende Custom-Shader eingebaut.

**Fertig:**
- `pbr.frag`, `atom_instanced.frag`, `bond_instanced.frag`: `fogEnabled/fogColor/fogDensity`-Uniforms + `mix(fogColor, color, exp(-(d·density)²))`.
- `PBRMaterial`, `AtomInstancingSystem`, `BondInstancingSystem`: `QParameter`-Felder + `setFogEnabled/Color/Density`-Setter.
- `MoleculeViewer::propagateFogToMaterials()` pusht `m_fogEnabled/m_fogIntensity → density·0.05, fogColor=background` an alle aktiven Materials. Aufruf in `setFogEnabled`, `setFogIntensity` und am Ende von `addMolecule`.
- Shader-Bug in `atom_instanced.frag` (doppelte `diff`/`spec`-Deklarationen) gefixt.

**Offen (Phase 1b):**
- `QPhongMaterial` hat keine Shader-Hooks. Default-Material-Mode (Phong) zeigt daher keinen Fog. Ersatz durch ein einheitliches `MolMaterial` (siehe Phase 2).

## Phase 2 — Material-Unifizierung (nächste)

Ziel: **ein** Material mit Shader-Varianten statt dreier paralleler Pfade. Dadurch greifen alle Render-Settings einheitlich.

**Plan:**

1. Neues `MolMaterial` (abgeleitet von `Qt3DRender::QMaterial`) mit:
   - Technik `geometry-pass` (Filter-Key `pass=geometry`) — Input für späteren Framegraph.
   - Zwei Render-Passes pro Technik: non-instanced (für kleine Moleküle) + instanced (für große Moleküle).
   - Uniforms: `baseColor`, `metallic`, `roughness`, `ao`, `shininess`, `fog*`, `viewMatrix`, `cameraPosition`.
   - Zwei Shader-Varianten intern: "phong-like" und "PBR" per Preprocessor-Define oder separate Shader-Dateien.

2. `createMoleculeEntity` in `view.cpp` refaktorieren: nur noch ein Pfad, entscheidet anhand Atomcount ob instanced oder per-atom, aber nutzt dasselbe Material.

3. `AtomInstancingSystem` + `BondInstancingSystem` anpassen, damit sie `MolMaterial` wiederverwenden statt eigene `QMaterial` zu bauen.

4. `QPhongMaterial`-Referenzen entfernen. `MaterialMode::Phong` bleibt als Uniform-Flag, nicht als Material-Klasse.

5. `updateMaterials`, `updateAtomGeometry`, `updateBondGeometry` vereinfachen — sie müssen nur noch `QParameter`-Werte ändern, kein `refreshVisualization()`-Fallback mehr nötig.

6. `m_atomMaterials` Typ von `QVector<QMaterial*>` → `QVector<MolMaterial*>`.

**Ergebnis:** Fog greift auch im Phong-Mode. Transparency/Shininess/Color-Scheme wirken in jedem Szenario. Setter-Gates auf `m_trajectoryAtoms` können entfallen.

## Phase 3 — Storage + Struktur

`view.cpp` strukturell entflechten.

- Single-Molecule vs. Trajectory trennen: `m_currentAtoms`/`m_currentBonds` für Einzelmolekül, `m_trajectoryAtoms/Bonds` nur bei Multi-Frame. `addMolecule` nicht länger abusive als 1-Frame-Trajectory.
- Setter-Gates `if (!m_trajectoryAtoms.isEmpty())` durch `if (hasMolecule())` ersetzen.
- `view.cpp` auf Module splitten:
  - `view.cpp` (Widget-Setup, Scene-Root, Lifecycle)
  - `view_input.cpp` (Maus, Keyboard, Event-Filter, Rotation/Pan/Zoom)
  - `view_scene.cpp` (createMoleculeEntity, clearScene, updateMaterials)
  - `view_trajectory.cpp` (Frames, Animation, Simulation-Integration)
  - `view_screenshot.cpp` (Save-Funktionen)

Ziel: jede Datei unter ~600 Zeilen, klare Verantwortlichkeiten.

## Phase 4 — Framegraph + Post-Processing

Multi-Pass-Rendering reaktivieren für SSAO, Bloom, HDR.

**Plan:**

1. Neues `PostProcessingFramegraph` von Grund auf, keine Wiederverwendung des alten Skelett-Codes.
   - G-Buffer-Pass: Scene rendert in `color`-Texture (RGBA16F) + `depth`-Texture (D24S8) + `normal`-Texture (RGB16F). Filter-Key `pass=geometry`.
   - SSAO-Pass: Fullscreen-Quad-Entity mit `ssao.frag`, schreibt in `ao`-Texture (R16F).
   - SSAO-Blur-Pass: Quad mit `ssao_blur.frag`.
   - Bloom-Bright-Pass: Quad mit `bloom_bright.frag` → Bright-Texture.
   - Bloom-Blur-H + Bloom-Blur-V: zwei Quads mit `blur_horizontal.frag`/`blur_vertical.frag`.
   - Composite-Pass: Quad mit `bloom_composite.frag` + `tonemapping.frag`, kombiniert Color·AO + Bloom, Exposure, HDR-Tonemap, schreibt Bildschirm.

2. `FullscreenQuad`-Helper neu bauen (kleines Utility-Entity, keine eigene Datei wenn < 50 Zeilen).

3. `PostProcessingFramegraph` als `QRenderSettings`-Ersatz in `setupViewer` einhängen, bei Resize Texturen neu skalieren.

4. SSAO/Bloom/HDR-Setter in `view.cpp` pushen an Framegraph-`QParameter`.

5. GPU-Kompat-Test: bei RHI-Backend-Fehler auf Standard-`QForwardRenderer` zurückfallen, UI-Widgets ausgrauen + Message.

**Abhängigkeit:** Phase 2 muss vorher fertig sein — Framegraph braucht `MolMaterial` mit korrektem Filter-Key.

## Phase 5 — Optional

- Schatten (Shadow-Mapping in zusätzlicher Pass).
- Environment-Map-Reflexionen für metallic Atoms.
- Outline/Toon-Shading als Alternative zu PBR.
- Screen-Space-Reflexions für Bond-Zylinder.
- `qDebug`/`qWarning` (~25× in view.cpp) in `#ifdef DEBUG_ON` wrappen (CLAUDE.md-Regel).

## Non-Goals

- Keine eigene Ray-Tracing- oder Pathtracing-Pipeline.
- Keine Volumen-Renderings (Electron-Density o. ä. separat, nicht im Viewer).
- Keine Integration eines externen Rendering-Engines (OSPRay, Blender-Cycles). Reiner Qt3D.

## Abhängigkeiten

```
Phase 0 (Cleanup, done)
  ↓
Phase 1 (Fog in Shadern, done)
  ↓
Phase 2 (MolMaterial) ── ermöglicht ──→ Phase 4 (Framegraph)
  ↓                                        ↓
Phase 3 (Storage-Split)            Phase 5 (Optional-Effekte)
```

Phase 2 ist Voraussetzung für Phase 4, weil ein einheitliches Material mit Pass-Filter-Keys nötig ist. Phase 3 (Storage/Struktur) kann parallel zu Phase 4 laufen — betrifft andere Codepfade.

## Kritische Dateien

- `src/view.cpp/.h` — Hauptwork.
- `src/pbrmaterial.cpp/.h` — wird in Phase 2 durch `MolMaterial` abgelöst oder untergeordnet.
- `src/atominstancingsystem.cpp/.h`, `src/bondinstancingsystem.cpp/.h` — Material-Teil durch `MolMaterial` ersetzen.
- `src/shaders/*.frag`, `.vert` — bestehende SSAO/Bloom/Blur/Tonemap-Shader warten auf Phase 4.
- `src/visualizationsettingsdialog.cpp/.h` — Dialog wartet passiv auf Phase-2-Wirksamkeit.

## Referenzen

- `docs/architecture/rendering-pipeline.md` — Stand vor Cleanup (veraltet, zu aktualisieren nach Phase 2).
- `AIChangelog.md` — historische Phasen 1–5D.
