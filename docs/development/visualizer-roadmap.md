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

## Technische Schulden — Detaillierte Analyse (2026-04)

### HOCH — Sofort behebbar oder täuscht User

| # | Fundstelle | Problem | Fix |
|---|-----------|---------|-----|
| 1 | `pbr.frag:78` | `mix(vertColor, vertColor, 0.5)` ist No-Op, ergibt immer `vertColor`. Vermutlich `mix(vertColor, baseColor, 0.5)` gemeint. **Rendering-Bug.** | `mix`-Aufruf korrigieren |
| 2 | `resources.qrc:5-6` | Stylesheet-Einträge (`dark.qss`, `light.qss`) auskommentiert, aber `mainwindow.cpp:2677-2678` lädt sie zur Laufzeit → stilles Fallback auf kein Stylesheet | `.qrc`-Einträge reaktivieren ODER Lade-Code entfernen |
| 3 | `visualizationsettingsdialog` | 9 UI-Widgets (SSAO/Bloom/HDR) haben keine Render-Wirkung — User stellt Slider, nichts passiert | Widgets disablen + Tooltip "Kommt in Phase 4" ODER ausblenden |
| 4 | 8 Shader in `resources.qrc` | `ssao.vert/.frag`, `ssao_blur.frag`, `bloom_bright.frag`, `blur_horizontal.frag`, `blur_vertical.frag`, `bloom_composite.frag`, `tonemapping.frag` — kompiliert in Binary, nie geladen | In Phase 4 reaktivieren, bis dahin Kommentar im .qrc |

### MITTEL — Strukturelle Schulden, mit Phase 2/3 auflösen

| # | Fundstelle | Problem | Phase |
|---|-----------|---------|-------|
| 5 | QPhongMaterial — 15+ Referenzen in `view.cpp/h`, `measurementoverlay.cpp`, `atominstancingsystem.cpp` | Gesonderter Material-Pfad, kein Fog, doppelte Update-Logik. `atominstancingsystem.cpp:7` hat totes `#include` | Phase 2 (MolMaterial ersetzt alles) |
| 6 | Fog/Camera-Parameter 3× identisch verdrahtet | `PBRMaterial`, `AtomInstancingSystem`, `BondInstancingSystem` kopieren gleiche `QParameter`+Setter | Phase 2 |
| 7 | `AtomInstancingSystem` — 5 tote Methoden | `raycastAtom()`, `isSupported()`, `getLastError()`, `updateAtomColors()`, `updateAtomScales()` nie aufgerufen. `VertexData`-Struct ungenutzt. `elements`-Parameter von `setAtoms()` ignoriert | Phase 2 oder vorher |
| 8 | `view.h` — 7 tote Member | `DEFAULT_BOND_DISTANCE` (nie gelesen), `m_currentFilePath` (nie geschrieben → Auto-Save tot), `m_autoSaveEnabled`, `m_hasUnsavedChanges`, `m_firstSelectedAtomForBond` (immer -1), `m_bondRotations` (Cache geschrieben, nie gelesen), `#include <QImage>` (unbenutzt) | Phase 3 oder vorher |
| 9 | 9 No-Op-Setter in `view.cpp:2060-2069` | SSAO/Bloom/HDR/Exposure speichern nur Werte, kein Render-Effekt | Phase 4 |
| 10 | 11 Setter-Gates `!m_trajectoryAtoms.isEmpty()` | Semantisch falsch — prüft Trajectory-Daten, meint "Molekül geladen". `addMolecule()` missbraucht als 1-Frame-Trajectory | Phase 3 |
| 11 | `PerformanceOptimizer` — 11 tote API-Methoden + 3 Signale | `startMonitoring()`, `stopMonitoring()`, `recordFrame()`, `setFrustumCullingEnabled()`, `isFrustumCullingEnabled()`, `getPerformanceWarning()`, `recommendQualityMode()`, `setAdaptiveQualityEnabled()`, `isAdaptiveQualityEnabled()`, `getAverageFPS()`, `getFrameCount()` — nie extern aufgerufen | Aufräumen |
| 12 | `BondEditor` — ~10 tote Methoden/Signale | `deselectBond()`, `clearSelection()`, `getSelectedBonds()`, `getBondCount()`, `getBond()`, `canUndo()`, `saveState()` + Signale `bondAdded/Removed/Modified`, `selectionChanged`, `editModeChanged`, `validationError` — nie extern verbunden | Aufräumen |
| 13 | `MeasurementOverlay` — 3 tote Methoden | `updateLineColor()`, `setLineThickness()`, `calculateLabelPosition()` — nie aufgerufen | Aufräumen |
| 14 | `XYZParser::detectAndScaleUnits()` | Aufruf auskommentiert (cpp:101-103), Funktion existiert nutzlos | Entfernen |
| 15 | `view.cpp` — `updateMeasurementDisplay()` | Deklariert + definiert, nie aufgerufen. Messung inline in `onAtomPicked()` | Entfernen |
| 16 | `view.cpp` — redundante Methoden | `resetView()` ≈ `resetViewToMolecule()`, `clearScenePublic()` ist trivialer Wrapper, `setFrameCount()`/`setCurrentFrame()` werden sofort von `setTrajectoryData()` überschrieben | Phase 3 |
| 17 | 24/25 `qDebug`/`qWarning` ohne `#ifdef DEBUG_ON` | CLAUDE.md-Regel verletzt. Nur 1 Aufruf (Zeile 1879) korrekt gewrapped | Wrappen |

### NIEDRIG — Sauberkeit, kein Funktionsverlust

| # | Fundstelle | Problem |
|---|-----------|---------|
| 18 | `atom_instanced.frag:25`, `bond_instanced.frag:25` | Variable `V` deklariert, nie referenziert |
| 19 | `ssao.frag:26-28` | `random()`-Funktion deklariert, nie aufgerufen |
| 20 | `ssao.frag:10` | `uniform sampler2D normalTexture` deklariert, nie gelesen |
| 21 | `pbr.frag:10` + `pbr.vert:9` | `vertColor`-Input → Standard-Qt3D-Geometrie hat kein `vertexColor`-Attribut an Location 2 → undefined/zeros |
| 22 | `pbr.frag:117-121` | Reinhard-Tonemapping im Fragment-Shader + `tonemapping.frag` identisch → doppeltes Tonemapping-Risiko wenn Framegraph aktiviert |
| 23 | `atom_instanced.frag` ≈ `bond_instanced.frag` | Fast identisch (nur specular-Power und ambient/diffuse-Ratios verschieden) → Konsolidierungsschuld |
| 24 | Instancing-Shader hardcodiert Licht | `Lview = normalize(vec3(0.3, 0.7, 0.5))`, PBR nutzt `lightPosition`-Uniform → Inkonsistenz |
| 25 | `NMR_DEBUG` 4× definiert | In `nmrspectrumdialog.h:11`, `nmrcontroller.h:15`, `nmrdatastore.h:15`, `nmrstructureproxymodel.h:14` → Redefinition-Warnungen |
| 26 | `nmrspectrumdialog.h:99` | Tote auskommentierte Method-Deklaration |
| 27 | `nmrcontroller.cpp:164` | Totes `// emit dataExportFailed(...)` |
| 28 | `mainwindow.cpp:114` | Irreführendes `//delete m_currentProcess;` (Qt auto-deleted) |

### Empfohlene Reihenfolge für Quick-Wins (vor Phase 2)

1. **pbr.frag `mix`-Bug fixen** — 1-Zeilen-Change, Rendering-Korrektur
2. **Tote Methoden entfernen**: `updateMeasurementDisplay()`, `AtomInstancingSystem::raycastAtom/isSupported/getLastError/updateAtomColors/updateAtomScales/VertexData`, `MeasurementOverlay::updateLineColor/setLineThickness/calculateLabelPosition`, `XYZParser::detectAndScaleUnits()`
3. **Tote Member entfernen**: `DEFAULT_BOND_DISTANCE`, `m_currentFilePath`, `m_autoSaveEnabled`, `m_hasUnsavedChanges`, `m_firstSelectedAtomForBond`, `m_bondRotations`, `#include <QImage>`
4. **SSAO/Bloom/HDR-Widgets disablen** mit Hinweis bis Phase 4
5. **qDebug/qWarning in `#ifdef DEBUG_ON` wrappen**
6. **Stylesheet-.qrc-Problem klären** — reaktivieren oder Lade-Code entfernen
7. **`NMR_DEBUG`-Doppeldefinitionen** in ein gemeinsames Header oder `#ifndef`-Guard

## Referenzen

- `docs/architecture/rendering-pipeline.md` — Stand vor Cleanup (veraltet, zu aktualisieren nach Phase 2).
- `AIChangelog.md` — historische Phasen 1–5D.
