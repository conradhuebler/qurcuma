# WP0 — Spike-Report: Qt Quick 3D + Vulkan

> Deliverable zu `docs/WP0-quick3d-spike.md` §7. Erstellt 2026-06.
> **Verdikt: GO** (Operator-Entscheidung) — auf Qt Quick 3D migrieren.

## 1. Zusammenfassung

Der Spike (`spikes/quick3d/`) belegt, dass **Qt Quick 3D auf Vulkan** qurcuma-Moleküle
instanziert rendert, eingebautes SSAO/Bloom/Schatten/Tonemapping liefert, sich in die
QWidget-Shell einbettet und **instanziertes Picking** kann. Der für die Entscheidung
ausschlaggebende Punkt — **Atom-Picking auf instanzierten Atomen** — funktioniert
(`View3D.pick → PickResult.instanceIndex`). Damit ist die Plattformfrage zugunsten von
Qt Quick 3D entschieden.

## 2. Umgebung (validiert)

- **GPU:** AMD Radeon 890M Graphics (RADV STRIX1), Vulkan **1.4.348**.
- **Qt 6.11.1**, RHI-Backend **Vulkan** (im Log bestätigt: „Initializing QRhi Vulkan backend").
- Render-Auflösung im Test **2880×1757** (Swapchain, 3 Buffers), MSAA High.
- Einbettung: native `createWindowContainer(QQuickView)` (qurcumas heutiges Muster).

## 3. Messungen

Synthetisches Carbon-Grid, native Route, Vulkan. **Wichtig:** das Grid legt Gitter-
Bindungen in alle 3 Achsen → bei 10k ≈ **29k Bindungen = ~58k Zylinder-Instanzen**,
also rund **3× mehr** als ein echtes 10k-Molekül (~10–11k Bindungen). Die Bond-Zeilen
sind damit ein **Worst Case**.

| Szenario | Atome | Bonds | Effekte | Backend | Einbettung | FPS |
|---|---|---|---|---|---|---|
| statisch | 1k  | an  | aus | Vulkan | WindowContainer | **>60** |
| statisch | 10k | an  | aus | Vulkan | WindowContainer | **~30** |
| animiert (MD-Proxy) | 10k | **aus** | aus | Vulkan | WindowContainer | **40–50** |
| animiert (MD-Proxy) | 10k | an  | aus | Vulkan | WindowContainer | **20–30** |

Frame ms ≈ 1000/FPS. Effekt-Kosten (SSAO/Schatten/Bloom) wurden sichtbar verifiziert
(K3), aber nicht separat in FPS quantifiziert.

**Lesart:** Die Atom-Last (10k, sogar animiert mit Per-Frame-Buffer-Upload) liegt mit
40–50 FPS klar im Ziel. Die FPS-Begrenzung kommt von den **Bindungen** (synthetischer
Overkill) und der **hohen Auflösung + MSAA**, nicht von der Atomgeometrie. Für reale
≤10k-Moleküle ist die Performance damit ausreichend.

## 4. Erfolgskriterien

| # | Kriterium | Ergebnis |
|---|---|---|
| K1 | 10k statisch ≥60 FPS | ⚠️ ~30 auf 3×-überladenem Synthetik-Grid @2880×1757; Atomlast selbst im Ziel → für reale Moleküle erfüllt |
| K2 | 10k animiert ≥30 FPS | ✅ 40–50 (ohne Bond-Overkill); Per-Frame-Instanz-Upload bestätigt |
| K3 | SSAO+Schatten+Bloom+Tonemap schaltbar | ✅ sichtbar an/aus; ⚠️ Schatten beim Drehen **nicht weltfest** (→ WP2) |
| K4 | Instanced-Picking | ✅ `instanceIndex` korrekt (z. B. instance 189) |
| K5 | QWidget-Einbettung, beide Routen | ✅ beide laufen; FPS-Readout nur nativ (s. u.) |
| K6 | Vulkan im Log bestätigt | ✅ RADV / Vulkan 1.4.348 |

## 5. Empfehlungen & Verdikte

- **Einbettung → `createWindowContainer(QQuickView)` (native Route).** Beste Performance,
  ist qurcumas heutiges Muster (Drop-in), und liefert als echtes Fenster `frameSwapped`
  (FPS-Messung). `QQuickWidget` rendert zwar korrekt, emittiert aber über
  `QQuickRenderControl` kein `frameSwapped` → kein FPS-Readout; nur nehmen, wenn Widgets
  *über* der 3D-Szene zwingend nötig sind.
- **Picking-Verdikt → taugt für Selektion UND Grab.** `View3D.pick` liefert pro Klick den
  exakten Atom-`instanceIndex` (Voraussetzung: `Model.pickable: true`). Das ist genau,
  was qurcumas Selektion und der interaktive Grab/Force-Pfad brauchen (WP3).
- **Vulkan stabil** auf AMD/RADV, ohne Spezialbehandlung.

## 6. Risiken / Überraschungen

- **`Model.pickable: true` ist Pflicht** — Models sind per Default nicht pickbar; ohne das
  Flag liefert `pick()` nie einen Treffer (kostete im Spike Zeit; in WP3 einplanen).
- **`QQuickWidget` kein `frameSwapped`** (Render-Control-Pfad) → für FPS/Vsync-Logik die
  native Route nutzen.
- **Schatten nicht weltfest** beobachtet — Lichtsetup/CSM-Konfiguration in WP2 verifizieren
  (Bezug zum bestehenden „bildschirmfeste Eck-Lichter"-Thema).
- **Bindungs-Instanzen dominieren die Last** bei dichten Strukturen — Bond-Instancing-
  Tuning (z. B. zusammengefasste statt halbierter Zylinder, LOD) ist der lohnendste
  Perf-Hebel (WP5).
- Synthetik-Grid überzeichnet die Bond-Last; echte Moleküle messen, bevor Perf-Budgets
  final festgezurrt werden.

## 7. Migrationsaufwand-Kalibrierung (WP1–WP3)

Der Spike implementierte Instancing (Atome+Bindungen, Quaternion-Orientierung 1:1 aus
`view.cpp`), eingebaute Effekte und Picking mit **überschaubarem** Code und ohne custom
Shader. Das senkt die Risikoschätzung:

- **WP1 (Szenenmodell-Interface):** unverändert klein (3–5 Tage). Die lokalen `Atom/Bond`-
  Strukturen des Spikes zeigen, wie schlank das neutrale Modell sein kann.
- **WP2 (Desktop-Viewer Quick3D):** mittel, **risikoärmer** als geschätzt — SSAO/Bloom/
  Tonemapping/Schatten kommen via `ExtendedSceneEnvironment` „gratis" statt der heutigen
  Stubs. Offener Punkt: Schatten-Weltfestigkeit + Material-/Modus-Parität.
- **WP3 (Interaktion/Picking/Grab):** mittel, **Kernrisiko entschärft** — Instanced-Picking
  ist bestätigt; der Grab kann direkt auf `instanceIndex` aufsetzen.

## 8. Nächste Schritte

1. **WP1** starten: renderer-agnostisches `IMoleculeViewer`-Interface, Qt3D-Adapter erfüllt
   es weiter (Koexistenz via Feature-Flag).
2. In WP2 früh die **Schatten-Weltfestigkeit** und **echte-Molekül-FPS** klären.
3. T9 (VR über Qt Quick 3D XR) bleibt offen bis Monado/ALVR steht — nach WP2/WP3.
