# WP0 — Qt Quick 3D + Vulkan Spike (Detailplan)

> **Teil von** [WP-visualization-and-vr.md](WP-visualization-and-vr.md). **Status:** Planung (kein Produktiv-Code).
> **Zweck:** Migrations-Entscheidung de-risken. Belegen (oder widerlegen), dass **Qt Quick 3D**
> auf **Vulkan/RHI** qurcuma-Moleküle (≤10k Atome) flüssig rendert, sich brauchbar in die
> **QWidget**-Shell einbetten lässt, **instanziertes Picking** kann und **SSAO/Schatten/Bloom
> eingebaut** liefert. Ergebnis: **Go/No-Go** + Messzahlen + Empfehlungen.

## 1. Entscheidungsfrage
> *„Trägt Qt Quick 3D (Vulkan, eingebaute Effekte, QWidget-Einbettung, Instanced-Picking)
> als zukünftiger Renderer für qurcuma — schick **und** performant — und als Basis für VR?"*

## 2. Umgebung (verifiziert 2026-06)
- **Qt 6.11.1**; Module `Quick`, `Quick3D`, `QuickWidgets`, `Quick3DXr`, `ShaderTools` **alle vorhanden**.
- **Vulkan 1.4** (AMD Radeon 890M, `radv`), `libvulkan.so` vorhanden.
- **OpenXR-Loader** vorhanden; **Monado/ALVR nicht** installiert → nur für T9/VR nötig.
- Dev-GPU **AMD** → CUDA (NVIDIA) hier n/a; Rendering-Spike nutzt **Vulkan-RHI** (unberührt).

## 3. Erfolgskriterien (quantitativ)
| # | Kriterium |
|---|---|
| K1 | 10k Atome (Ball-Stick), **statisch** ≥ **60 FPS** @1080p, Vulkan-Backend |
| K2 | 10k Atome **animiert** (Instanz-Buffer pro Frame neu = MD-Proxy) ≥ **30 FPS** |
| K3 | **SSAO + geworfene Schatten + Tonemapping** sichtbar an/aus schaltbar |
| K4 | **Atom-Picking** auf instanzierten Atomen funktioniert (`PickResult.instanceIndex`) |
| K5 | In **QWidget**-Shell eingebettet (beide Routen getestet), keine groben Artefakte |
| K6 | Build reproduzierbar via CMake, Vulkan-Backend im Log bestätigt |

## 4. Scope
- **In:** eigenständige Mini-App (kein Umbau von qurcuma); Rendering + Instancing + eingebaute
  Effekte + Einbettung + Picking + Messung. Wiederverwendung: bestehender `xyzparser` zum Laden
  *oder* synthetischer Grid-Generator (1k/5k/10k).
- **Out:** Feature-Parität, Migration, VR-Parität (nur optionaler Smoke-Test T9), curcuma-Compute,
  Produktionsqualität.

## 5. Aufgaben

> Spike lebt unter **`spikes/quick3d/`** (eigene CMakeLists, eigener Tree) — beeinträchtigt den
> qurcuma-Build **nicht**.

### T1 — Projekt-Setup
- `spikes/quick3d/` mit eigener `CMakeLists.txt`:
  `find_package(Qt6 COMPONENTS Core Gui Widgets Quick Quick3D QuickWidgets ShaderTools)`,
  `qt_add_qml_module(...)`.
- `main.cpp`: `QApplication` + `QQuickWidget` (oder `QQuickView`) lädt eine `Main.qml` mit `View3D`.
- **Fertig wenn:** leeres `View3D`-Fenster startet; **Vulkan aktiv** (per `QSG_RHI_BACKEND=vulkan`,
  verifiziert via `QSG_INFO=1`-Log).

### T2 — Basis-Szene
- `View3D` + `PerspectiveCamera` + `DirectionalLight` + `SceneEnvironment` (clearColor).
- `OrbitCameraController` für Maus-Rotation; ein paar statische Kugeln als Sanity-Check.
- **Fertig wenn:** rotierbare Szene, Kugeln sichtbar.

### T3 — Instancing (Atome + Bindungen)
- C++-Subklasse von **`QQuick3DInstancing`**: `getInstanceBuffer()` füllt per-Instance
  Position/Scale/Color (Layout via `InstanceTableEntry`/`calculateTableEntry`).
- `Model { source: "#Sphere"; instancing: atomInstancing }`; Bindungen = Zylinder-Instancing
  mit Rotation (Quaternion Y→Bindungsrichtung) — analog zum bestehenden `BondInstancingSystem`.
- Daten: **`xyzparser`** einbinden **oder** Grid-Generator (1k/5k/10k C-Atome).
- **Fertig wenn:** 10k instanzierte Kugeln in **einer** Instancing-Klasse gerendert.

### T4 — Eingebaute Effekte
- **`ExtendedSceneEnvironment`**: `aoEnabled/aoStrength/aoDistance` (SSAO), `tonemapMode`,
  `glowEnabled/glowStrength` (Bloom), `antialiasingMode` (MSAA/SSAA/progressive).
- **`DirectionalLight { castsShadow: true; shadowMapQuality; shadowFactor }`** → geworfene Schatten.
- Kleine Toggle-Leiste (QWidget-Panel oder QML-Buttons).
- **Fertig wenn:** SSAO + Schatten + Bloom + Tonemapping sichtbar schaltbar; **Schatten beim
  Drehen prüfen** (bildschirm-/weltfest — Bezug zum bisherigen Beleuchtungs-Thema).

### T5 — Vulkan-Backend
- `QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan)` **oder** `QSG_RHI_BACKEND=vulkan`;
  zusätzlich **OpenGL-Fallback** testen (Vergleich/Treiber-Quirks radv).
- **Fertig wenn:** stabil auf Vulkan **und** GL-Fallback; Backend im Log verifiziert.

### T6 — QWidget-Einbettung (beide Routen)
- **Variante A:** `QQuickWidget` (echtes Widget, Overlays darüber möglich).
- **Variante B:** `QWidget::createWindowContainer(QQuickView)` (nativ, beste Perf — wie heute `Qt3DWindow`).
- Beide in eine Mini-Shell mit Seitenpanel (wie qurcuma) einbetten.
- **Achtung:** `QQuickWidget` + Vulkan explizit testen (historisch zickig); falls Probleme →
  WindowContainer als Primärweg dokumentieren.
- **Fertig wenn:** beide laufen; FPS/Latenz/Z-Stacking notiert; **Empfehlung** formuliert.

### T7 — Picking
- `View3D.pick(x, y)` → `PickResult.instanceIndex` auf instanzierten Atomen; Klick markiert Atom.
- **Fertig wenn:** Atom per Klick eindeutig identifiziert → bestätigt, dass Instanced-Picking für
  qurcumas Selektion **und** den interaktiven Grab taugt.

### T8 — Messprotokoll
- Datensätze: **1k / 5k / 10k** (synth. Grid + 1 echtes Molekül).
- Szenarien: **statisch**; **animiert** (Instanz-Buffer pro Frame neu = MD-Proxy).
- Metriken: **FPS** (`QQuickWindow::frameSwapped` + `QElapsedTimer`), Frame-Zeit, grob GPU/CPU.
- Effekte **an/aus** vergleichen (Kosten SSAO/Schatten/Bloom).
- Messvorlage ausfüllen (§6).

### T9 — (Stretch) VR-Smoke-Test
- Nur bei Zeit: `XrView` statt `View3D`; **Monado-Simulator** (ohne Headset) *oder* **ALVR→Pico 4**.
- **Voraussetzung:** Monado oder ALVR installieren (aktuell nicht vorhanden).
- **Ziel:** nur Aufwand/Reife einschätzen — keine Parität.

## 6. Messvorlage
| Szenario | Atome | Effekte | Backend | Einbettung | FPS | Frame ms | Notizen |
|---|---|---|---|---|---|---|---|
| statisch | 1k | aus | Vulkan | QQuickWidget | | | |
| statisch | 10k | aus | Vulkan | QQuickWidget | | | |
| statisch | 10k | SSAO+Schatten+Bloom | Vulkan | QQuickWidget | | | |
| statisch | 10k | SSAO+Schatten+Bloom | Vulkan | WindowContainer | | | |
| animiert | 10k | aus | Vulkan | (Empfehlung) | | | |
| animiert | 10k | SSAO+Schatten | Vulkan | (Empfehlung) | | | |
| statisch | 10k | SSAO+Schatten+Bloom | OpenGL | (Vergleich) | | | |

## 7. Deliverable: Spike-Report
- Ausgefüllte Messtabelle + Screenshots (mit/ohne Effekte, beide Einbettungen).
- **Go/No-Go**-Empfehlung mit Begründung.
- **Einbettungs-Empfehlung** (QQuickWidget vs WindowContainer).
- **Picking-Verdikt** (taugt für Selektion + Grab?).
- Risiken/Überraschungen.
- **Migrationsaufwand-Schätzung** (kalibriert WP2/WP3).

## 8. Entscheidungskriterien
- **GO (migrieren):** K1–K6 erfüllt (10k ≥60 FPS statisch & ≥30 animiert, Picking ok,
  Einbettung ok, Effekte ok, Vulkan stabil).
- **Teil-GO:** Funktioniert, aber nur via WindowContainer **oder** mit reduzierten Effekten in VR
  → migrieren mit dokumentierten Einschränkungen.
- **NO-GO:** Vulkan/QQuickWidget instabil **oder** Picking unbrauchbar **oder** Perf < Ziel
  → Fallback **custom Qt3D-Framegraph** (SSAO/Bloom/Schatten) und VR später neu bewerten.

## 9. Risiken (WP0-spezifisch) & Gegenmaßnahmen
| Risiko | Gegenmaßnahme |
|---|---|
| `QQuickWidget` + Vulkan zickig | WindowContainer-Route als Fallback (T6) |
| Instanced-Picking-Eigenheiten | früh in T7 isoliert testen |
| radv/AMD-Treiber-Quirks | GL-Fallback zum Vergleich (T5) |
| Instancing-Buffer-Layout (`InstanceTableEntry`) fummelig | erst 1k debuggen, dann auf 10k skalieren |

## 10. Aufwand & Reihenfolge
- **T1–T5** Kern (~3–5 Tage) → **T6–T8** Einbettung/Messung (~3–4 Tage) → **T9** optional.
- **Gesamt ~1.5–2 Wochen.** Sequenziell; T9 nur bei Pufferzeit.

## 11. Bewusst NICHT in WP0
Migration, Feature-Parität, VR-Parität, curcuma-Compute, Produktionsqualität. Diese folgen erst
nach **GO** in WP1–WP6.
