# Arbeitspaket (WP): Qurcuma — schicke & performante Visualisierung + VR-Schnittstelle

> **Status:** Planung / Roadmap (kein Code). Erstellt 2026-06.
> **Vision:** Qurcuma wird (a) eine *schicke und performante* Molekül-Visualisierung
> und (b) eine *VR-Schnittstelle* für OpenXR-Hardware (Workhorse: **Pico 4**),
> mit maximaler Ausnutzung von Qt6, Vulkan (als Render-Backend) und CUDA (Compute).
> Open-Source-Frameworks werden bevorzugt.

---

## 1. Ist-Zustand (fundiert)

- **Rendering:** Qt3D (`Qt6::3DCore/3DRender/3DExtras`) mit Standard-`QForwardRenderer`
  und eigenen Instancing-Shadern (`atom_/bond_instanced.*`). Umschalten per-Atom ↔
  GPU-Instancing bei 500 Atomen (`view.h:kAtomInstancingThresholdDefault`).
- **Perf-Gerüst:** `PerformanceOptimizer` (LOD Fast/Balanced/HighQuality, Frustum-Culling-Flag,
  FPS-Monitor, adaptive Quality) — teils nur Gerüst.
- **Post-Processing:** SSAO, Bloom/HDR, Tonemapping als **Shader + Settings + UI vorhanden,
  aber Setter sind Stubs** — nie an einen Framegraph angeschlossen (`setSSAOEnabled` etc.).
- **Beleuchtung:** 4 bildschirmfeste Eck-Lichter (gerade umgesetzt). Echte geworfene
  Schatten: nicht vorhanden.
- **Compute/Simulation:** curcuma-Backend mit GPU-Dropdown (none/CUDA/ROCm/Vulkan/Auto)
  → beschleunigt **Kraftfelder** (GFN-FF/xtb), **nicht** das Rendering.
- **VR:** nichts. Qt3D hat **keinen** OpenXR/VR-Pfad.

## 2. Zentrale Weiche & Entscheidung

**Qt3D** ist in Qt6 im Maintenance-Modus, hat begrenzten RHI/Vulkan-Zugriff und **keinen
VR-Weg**. Der moderne Qt6-Pfad ist **Qt Quick 3D**:

- baut auf **Qt RHI** (Backends: **Vulkan**/Metal/D3D12/OpenGL) → Vulkan kommt "gratis",
- liefert **SSAO, Bloom, Tonemapping/HDR, geworfene Schatten eingebaut**,
- hat das Modul **Qt Quick 3D XR** (= OpenXR, seit Qt 6.8) → **VR-Pfad vorhanden**.

**Konsequenz:** Die "2 Schuhe" (Desktop-Schick + VR) **konvergieren** auf Qt Quick 3D.
Ein custom **Qt3D-Framegraph** für SSAO/Bloom/Schatten wäre bei späterer Migration
**Wegwerf-Arbeit** — Quick 3D bringt das fertig mit *und* öffnet VR.

> **Entscheidung (vom Operator bestätigt):** Richtung **Qt Quick 3D**, Spike-first
> validieren. Wenn der Spike trägt → migrieren; sonst Fallback custom Qt3D-Framegraph.

### QWidget-Einbettung (geklärt)
qurcuma bleibt QWidget-basiert. Zwei Einbettungs-Routen für Qt Quick 3D:
- **`QQuickWidget`** — echtes Widget, andere Widgets *darüber* möglich; Off-Screen-Render
  + Composite → leichter Overhead/~1 Frame Latenz. Für ≤10k Atome unkritisch.
- **`createWindowContainer(QQuickView)`** — natives Direkt-Rendering (beste Perf, wie heute
  mit `Qt3DWindow`); natives Kindfenster → eingeschränktes Z-Stacking.

Heute nutzt qurcuma bereits das Window-Container-Muster (Overlays laufen *in* der 3D-Szene),
daher ist `createWindowContainer(QQuickView)` faktisch ein Drop-in. **VR wird nicht ins
Widget eingebettet** — OpenXR rendert an den Headset-Compositor; das Widget zeigt optional
eine Spectator-Ansicht und *startet* die XR-Session.

## 3. Ziel-Architektur (Skizze)

```
            +-----------------------------+
            |  Parser / Datenmodell        |   (bestehend: vtf/xyz/pdb/mol2,
            |  Atom/Bond/SimulationFrame   |    SimulationFrame, MoleculeBridge)
            +--------------+--------------+
                           |  renderer-agnostisches Szenenmodell
              +------------+-------------+
              |                          |
   +----------v---------+    +-----------v-----------+
   | Desktop-Viewer     |    | VR-Viewer (XR)        |
   | Qt Quick 3D        |    | Qt Quick 3D XR        |
   | (QQuickWidget/      |    | -> OpenXR -> Headset  |
   |  WindowContainer)   |    |   (Monado / ALVR /    |
   |  in QWidget-Shell   |    |    SteamVR)           |
   +----------+---------+    +-----------+-----------+
              |  RHI (Vulkan)             |  RHI (Vulkan) + OpenXR
   +----------v--------------------------v-----------+
   |  curcuma-Compute (CPU / CUDA / ROCm / Vulkan)    |  Kraftfelder, MD/Opt
   +--------------------------------------------------+
```

Kerngedanke: **ein** renderer-agnostisches Szenenmodell speist sowohl Desktop- als auch
VR-Viewer. Die curcuma-Compute-Schicht bleibt unverändert darunter.

## 4. Arbeitspakete

Format je WP: **Ziel · Scope · Deliverable · Abhängigkeit · Risiko · Aufwand (grob)**.

### WP0 — Spike: Qt Quick 3D + Vulkan (Proof of Concept) ⭐ zuerst
- **Ziel:** Tragfähigkeit & Performance von Qt Quick 3D belegen, bevor migriert wird.
- **Scope:** Standalone-Mini-App. Molekül laden (bestehende Parser wiederverwenden),
  Atome/Bindungen via Quick-3D-**Instancing** (`QQuick3DInstancing`/`InstanceList`),
  eingebautes **SSAO + Schatten + Tonemapping** an, **Vulkan-RHI** erzwingen
  (`QSG_RHI_BACKEND=vulkan`). Einbettung **beide** Routen testen (QQuickWidget vs
  WindowContainer).
- **Deliverable:** Go/No-Go + FPS-Zahlen (1k/10k Atome, statisch + animierte Trajektorie)
  + Einbettungs-Empfehlung + Notiz zu Picking-Machbarkeit.
- **Abhängigkeit:** —  · **Risiko:** mittel (Quick3D/RHI-Reifegrad) · **Aufwand:** 1–2 Wochen.

### WP1 — Renderer-agnostisches Szenenmodell
- **Ziel:** Viewer hinter eine Schnittstelle legen, damit Qt3D- und Quick3D-Viewer
  während der Migration koexistieren.
- **Scope:** `Atom/Bond/SimulationFrame` als neutrales Modell; `IMoleculeViewer`-Interface
  (addMolecule, setFrame, rendering/material modes, selection, picking-Signale).
- **Deliverable:** Interface + Qt3D-Adapter (bestehend) erfüllt es weiter.
- **Abhängigkeit:** —  · **Risiko:** niedrig · **Aufwand:** 3–5 Tage.

### WP2 — Desktop-Viewer in Qt Quick 3D (Feature-Parität Rendering)
- **Ziel:** Den 3D-Viewer auf Quick 3D portieren, in der QWidget-Shell.
- **Scope:** Rendering-Modi (Ball-Stick/Spacefill/Wireframe/Sticks), Farbschemata,
  Transparenz/Material; **eingebaute SSAO/Bloom/Schatten/Tonemapping statt der Stubs**;
  Instancing; Hintergrund/Fog. Einbettung gemäß WP0.
- **Deliverable:** Quick3D-Viewer als wählbare Alternative (Feature-Flag) zum Qt3D-Viewer.
- **Abhängigkeit:** WP0, WP1 · **Risiko:** mittel · **Aufwand:** 3–4 Wochen.

### WP3 — Interaktion & Overlays (Parität)
- **Ziel:** Maus-Interaktion und Overlays auf Quick 3D nachbauen.
- **Scope:** Rotate/Zoom/Pan, **Atom-Picking** (Quick3D `pickResult`/Ray), Selektion,
  Mess-Overlay, **Grab-Kraft** (interaktive Sim — bestehende `forceinjector`-Kette
  weiterverwenden), Snapshots.
- **Deliverable:** Desktop-Parität zur heutigen Qt3D-Version.
- **Abhängigkeit:** WP2 · **Risiko:** mittel (Picking-API) · **Aufwand:** 2–3 Wochen.

### WP4 — VR über Qt Quick 3D XR / OpenXR (Pico 4)
- **Ziel:** Szene aus WP2/3 in VR betreten.
- **Scope:** `XrView`/`XrController` auf das gemeinsame Szenenmodell; **VR-Interaktion**:
  Controller-Ray zum Atom-Picken, **Greifen → Kraft anlegen** (mappt direkt auf die
  bestehende Grab-/Force-Logik!), Skalieren/Greifen der Molekül-Welt, Komfort-Optionen.
  **Spectator-View** im Desktop-Widget. Open-Source-Runtime-Pfad: **Monado**; Pico-4-Pfad:
  **ALVR** (Open-Source-Streaming) oder Pico Connect/SteamVR.
- **Deliverable:** Molekül in VR betrachten + interaktiv greifen, lauffähig auf Pico 4.
- **Abhängigkeit:** WP2, WP3 · **Risiko:** hoch (XR-Reifegrad, Streaming-Latenz) · **Aufwand:** 3–5 Wochen.

### WP5 — Performance-Härtung
- **Ziel:** Flüssig auf Desktop **und** VR (VR braucht 72–90 Hz, niedrige Latenz, Stereo = 2×).
- **Scope:** Instancing-Tuning, Frustum-/Occlusion-Culling, LOD, **Frame-Pacing/Reprojection**
  für VR; effizienter **Per-Frame-Update-Pfad** für Trajektorie/Echtzeit-MD (Instanz-Buffer);
  Impostor/Billboard-Spheres nur falls >10k nötig (bei ≤10k meist unnötig).
- **Deliverable:** Stabile Framerate-Budgets dokumentiert (Desktop + VR).
- **Abhängigkeit:** WP2–WP4 · **Risiko:** mittel · **Aufwand:** laufend / 2–3 Wochen fokussiert.

### WP6 — Compute (optional, parallel)
- **Ziel:** Echtzeit-MD in VR flüssig versorgen.
- **Scope:** curcuma-GPU (CUDA/Vulkan-Compute) so ausreizen, dass der Sim-Loop interaktive
  Raten liefert; saubere Entkopplung Sim-Thread ↔ Render-Thread; (Stretch) CUDA↔Vulkan-
  Interop für Zero-Copy bei sehr großen/Echtzeit-Szenen.
- **Deliverable:** Interaktive MD-Rate gemessen, Engpässe benannt.
- **Abhängigkeit:** WP2 · **Risiko:** mittel · **Aufwand:** offen.

## 5. Roadmap / Reihenfolge

```
WP0 (Spike) ──► Entscheidung ──► WP1 ──► WP2 ──► WP3 ──► WP4 (VR)
                                          └──► WP5 (Perf, laufend) ◄──┘
                                          └──► WP6 (Compute, parallel)
```

1. **WP0** zuerst (geringes Commitment, hoher Lerneffekt).
2. Bei Go: **WP1→WP2→WP3** = Desktop-Migration mit Schick (SSAO/Schatten/Bloom gratis).
3. **WP4** = VR auf der migrierten Szene.
4. **WP5/WP6** begleitend (Perf/Compute).

## 6. Skalierungs-Hinweis (≤10k Atome)

10k Atome sind für moderne GPUs **moderat** — instanzierte Spheres/Cylinder laufen locker
in hoher Framerate. Die eigentlichen Lasten sind daher **nicht** rohe Geometrie, sondern:
- **VR-Stereo @ 72–90 Hz** mit niedriger Latenz (doppeltes Rendern + Reprojection),
- **Echtzeit-Updates** bei MD/Trajektorie (effiziente Instanz-Buffer-Updates),
- **Post-Processing-Kosten** (SSAO/Schatten/Bloom) — in VR sparsamer dosieren.

→ Priorität liegt auf **Qualität + VR-Latenz**, nicht auf Geometrie-Durchsatz. Vulkan kommt
als RHI-Backend automatisch; CUDA bleibt auf der **Compute-Seite** (curcuma); Render-Compute-
Interop ist bei ≤10k Atomen **kein** vorrangiges Thema.

## 7. Open-Source-VR-Stack (Pico 4)

- **API:** **OpenXR** (Standard; Pico, SteamVR, Monado sprechen es).
- **Qt:** **Qt Quick 3D XR** → spricht OpenXR.
- **PC-VR-Pfad zu Pico 4:** PC rendert/rechnet, streamt zum standalone Headset:
  - **ALVR** (Open-Source) — streamt OpenXR-Inhalte zu Pico/Quest über WLAN.
  - Alternativ Pico Connect / SteamVR-Bridge (nicht voll OSS).
- **Voll-OSS-Runtime auf PC:** **Monado** (Open-Source-OpenXR-Runtime) zum Entwickeln/Testen,
  ggf. ohne Headset (Simulator).
- **Standalone-on-Pico** (App läuft *auf* dem Android-Headset) ist möglich, aber separates
  Build-Target (ARM/Android) und **ungeeignet für curcumas schwere Compute** → daher
  **PC-VR + Streaming** als Hauptpfad.

## 8. Risiken & offene Punkte

- **Qt-Quick-3D-XR-Reifegrad** (relativ jung) — in WP0 früh anfassen.
- **`QQuickWidget`-Perf/Latenz** vs. Window-Container — in WP0 messen.
- **Migrationskosten/Feature-Parität** Qt3D → Quick3D (Picking, Overlays, Grab).
- **Streaming-Latenz Pico 4** (ALVR/WLAN) für interaktives Greifen.
- **Doppelpflege** während der Migration (zwei Viewer hinter einem Interface, Feature-Flag).
- **curcuma-Integration** muss über die Migration stabil bleiben.

## 9. Lernziele (explizit gewünscht: "vielleicht lernen wir noch was")

- Qt RHI / **Vulkan**-Backend in der Praxis.
- **OpenXR**-Loop & VR-Interaktionsdesign (Greifen = Kraft, Skalieren der Molekülwelt).
- Qt Quick 3D Szenen-/Instancing-/Material-System.
- (Stretch) **CUDA↔Vulkan-Interop** für Zero-Copy.

---

### Nächster Schritt
**WP0 (Spike)** ausplanen und starten — minimaler Qt-Quick-3D-Renderer mit Vulkan + SSAO/
Schatten, eingebettet in eine QWidget-Shell, gemessen bis 10k Atome. Erst danach
Migrations-Commitment.
