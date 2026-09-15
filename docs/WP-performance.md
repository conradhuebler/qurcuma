# Arbeitspaket (WP): Performance-Roadmap — Viewer, Live-Simulation, GUI-Thread

> **Status:** Roadmap (kein Code). Erstellt 2026-09, Claude Generated.
> **Anlass:** Der 3D-Viewer ist seit dem Dock-Fix ein `QQuickWidget` und rendert damit
> auf dem GUI-Thread (siehe `src/docks/CLAUDE.md` ▸ Viewer Embedding). Die Frage
> „können wir das mit Threads optimieren?" wird hier in messbare Schritte zerlegt.
> **Leitlinie:** Erst messen, dann die CPU-Arbeit pro Frame vom GUI-Thread nehmen,
> dann GPU-Qualität adaptiv machen. Ein eigener Render-Thread ist der *letzte*
> Schritt und nur bei nachgewiesenem Bedarf.

---

## 1. Ist-Zustand: was pro Live-Frame auf dem GUI-Thread läuft

Pfad `SimulationWorker::frameReady` → `MoleculeViewer::updateSimulationFrame` (`src/view.cpp`):

| Schritt | Ort | Kosten | Bemerkung |
|---|---|---|---|
| Positionen in `m_trajectoryAtoms[0]` kopieren | `updateSimulationFrame` | O(N) | unkritisch |
| **Bindungserkennung** `detectBondsHysteresis` | `view.cpp` | **O(N²)** pro Frame | Kovalenzradien × Toleranz, Vollpaar-Schleife; bei 10k Atomen 5·10⁷ Distanzen pro Frame |
| `bondSetEqual` | `view.cpp` | O(B) + QSet-Aufbau | nur zur Änderungserkennung |
| `syncSceneToController` → `SceneController::updatePositions` | `scenecontroller.cpp` | O(N + B) | ruft **`rebuildGeometry()`** |
| `rebuildGeometry` → `rebuildAtoms` | `scenecontroller.cpp` | O(N), pro Atom `atomColor()`/`schemeColorFor()` (QColor, Fragment-Tint, Selektion, Kollision) | Farben/Radien werden jedes Frame neu berechnet, obwohl sich nur Positionen ändern |
| `rebuildGeometry` Bonds | `scenecontroller.cpp` | O(B), pro Bindung Quaternion + 2 Halbzylinder | 2 Instanzen pro Bindung |
| `AtomInstancing::setItems` / `BondInstancing::setSegments` | `atominstancing.cpp`, `bondinstancing.cpp` | O(N), O(2B) | zweiter Durchlauf: `QVector<Item>` → `QByteArray` (`calculateTableEntry`) |
| `rebuildOverlays`, `rebuildNci`, `rebuildLabels` | `scenecontroller.cpp` | Early-return wenn leer | bei aktiven Labels: Projektion pro Atom in QML (`Repeater`) |
| `refreshNciOverlay` | `view.cpp` | O(N²) geometrisch, wenn Overlay an | `nci::detectGeometric` + ggf. Ringsuche; GFN-FF-Quelle nur Gate |
| `computeWallViolations` | `view.cpp` | O(N) | unkritisch |
| Scene-Graph-Sync + Render | Qt Quick (Render Control) | GPU-Kommandos aufzeichnen | seit `QQuickWidget` auf dem GUI-Thread; die GPU-Arbeit selbst bleibt asynchron |

Zusätzlich im Edit-Modus: `computeCollisions` (O(N²) über alle Paare bei jedem Move).

**Wichtig für die Einordnung:** Auch mit dem Threaded-Render-Loop des alten
`QQuickView` blockierte der GUI-Thread während der Sync-Phase. Nur das Aufzeichnen
der GPU-Kommandos lief parallel. Bei Instanced-Draws ist dieser Teil klein. Der
größte Hebel sind die O(N²)-Schleifen und der doppelte Instanzpuffer-Aufbau, und
die liegen unabhängig von der Einbettung auf dem GUI-Thread.

---

## 2. WP P0 — Messen (Voraussetzung für alles Weitere)

**Ziel:** Eine Baseline-Tabelle je Schritt aus Abschnitt 1, für drei Testfälle.

- Testfälle festlegen: klein (~100 Atome, z. B. `conf_28.xyz`), mittel (~1k), groß (~10k, synthetisch oder Polymer-VTF). Jeweils MD mit GFN-FF, 1000 Steps.
- Timing-Analyse gemäß CLAUDE.md-Regel („Implement timing analysis for complex functions"): `QElapsedTimer` um Bindungserkennung, `rebuildGeometry`, `setItems`/`setSegments`, `refreshNciOverlay`; Ausgabe nur unter `#ifdef DEBUG_ON` oder hinter dem vorhandenen `performanceAnalysis`-Flag des Workers (`SimulationConfig::performanceAnalysis`), das bereits Step-Zeiten sammelt.
- Render-Seite: `QSG_RENDER_TIMING=1` liefert Sync/Render/Swap pro Frame. Vergleich beider Einbettungen: Standard vs. `QURCUMA_NATIVE_VIEWPORT=1`.
- Sichtbare Bildrate: FPS-Zähler im Viewer-Bar-HUD (bei `QQuickWidget` über `QQuickWindow::afterRendering`, nicht `frameSwapped`).
- **Definition of Done:** Tabelle in `AIChangelog.md` (Step-Zeit Worker, GUI-Zeit pro Frame aufgeschlüsselt, Sync/Render-Zeit, FPS) für alle drei Fälle und beide Einbettungen. Erst danach werden P1–P5 priorisiert.

**Aufwand:** klein (1 Tag). **Risiko:** keines.

---

## 3. WP P1 — Bindungserkennung in den `SimulationWorker`

**Ziel:** `detectBondsHysteresis` verlässt den GUI-Thread; der Viewer vergleicht nur noch.

- `SimulationFrame` erhält `QVector<Bond> bonds` (oder kompakter `QVector<QPair<int,int>>`) und ein Flag `topologyChanged`. Der Worker hält den vorherigen Bindungssatz für die Hysterese selbst.
- `updateSimulationFrame` nutzt die Frame-Bindungen und ruft `updateBonds` nur bei `topologyChanged`; die O(N²)-Schleife im GUI-Thread entfällt.
- Optional im gleichen Schritt: **Zellliste** (Gitter mit Zellgröße = max. Bindungslänge ≈ 2·r_cov,max·1.45) → O(N) statt O(N²). Dieselbe Zellliste kann später NCI-Erkennung und Kollisionen bedienen (`nci::detectGeometric`, `computeCollisions`).
- Optimizer-Pfad (`runOptimization`) gleich mitziehen, dort werden Frames pro Iteration emittiert.
- **Definition of Done:** GUI-Anteil „Bindungserkennung" in der P0-Tabelle = 0; Bindungsbruch/-bildung während MD verhält sich sichtbar wie vorher (Hysterese 1.25/1.45 unverändert). `test_buildtools`-artiger Test für die Zellliste gegen die Vollpaar-Referenz.

**Aufwand:** mittel (2–3 Tage inkl. Zellliste). **Risiko:** gering; Verhalten ist durch die Referenzimplementierung festgelegt.

---

## 4. WP P2 — Instanzpuffer: Position-only-Fast-Path

**Ziel:** Bei reinen Positionsänderungen nur Positionen anfassen, keine Farben, Radien, Quaternionen zweimal neu berechnen.

- `SceneController::updatePositions` bekommt einen eigenen Pfad: Atomfarben/-radien werden **gecacht** (`m_atomColorCache`, `m_atomScaleCache`, invalidiert durch Selektion/Hover/Kollision/Schema/Fragment-Tint) → `AtomInstancing::updatePositions(const QVector<QVector3D>&)` patcht nur die Translations-Spalten der bestehenden Tabelle (`calculateTableEntry` entfällt pro Atom).
- Bonds: Segmentfarben cachen; pro Frame nur Mittelpunkt, Länge, Rotation. Prüfen, ob `bondRotation` (Quaternion aus Richtung) durch die Rotationsmatrix direkt im Tabellen-Eintrag ersetzt werden kann (spart eine Quaternion→Matrix-Konvertierung pro Segment).
- `rebuildOverlays`/`rebuildNci`/`rebuildLabels` aus dem Position-only-Pfad heraushalten, wenn nichts davon aktiv ist (heute Early-Returns, aber die Aufrufe und Prüfungen bleiben).
- Optional: den fertigen `QByteArray` schon im Worker bauen (der Worker kennt Positionen und Bindungen nach P1). Nur sinnvoll, wenn P0 zeigt, dass `setItems`/`setSegments` nach dem Cache noch dominiert; die Farb-/Stil-Abhängigkeit müsste dann als Snapshot in den Worker.
- **Definition of Done:** `rebuildGeometry` wird im MD-Pfad nicht mehr aufgerufen; GUI-Zeit pro Frame für „Instanzpuffer" sinkt messbar (Zielwert nach P0 festlegen); Darstellung pixelidentisch zu vorher (Screenshot-Vergleich über `exportImage`).

**Aufwand:** mittel (2–3 Tage). **Risiko:** mittel — viele Invalidierungspfade (Selektion, Hover, Kollision, Fragment-Tint, Bead-Farben); Tests für die Cache-Invalidierung nötig.

---

## 5. WP P3 — Frame-Koaleszenz (Latest-wins)

**Ziel:** Der GUI-Thread zeichnet mit Bildschirmrate, nicht mit Step-Rate; Frames stauen sich nie in der Queue.

- Heute emittiert der Worker pro Step `frameReady` (QueuedConnection). Ist der GUI-Thread langsamer als der Step, wächst die Event-Queue und die Anzeige hinkt nach; Eingaben (Grab, Temperatur-Slider) werden träge.
- Lösung: ein Mailbox-Puffer (`std::atomic`/Mutex + `SimulationFramePtr latest`) im Viewer; der Worker legt ab, der Viewer holt beim nächsten Tick eines 60-Hz-`QTimer` bzw. über `QQuickWindow::beforeSynchronizing` den jüngsten Frame. Snapshots/Charts/Kalorimetrie erhalten weiterhin jeden Frame (getrennter Kanal, keine Zeichenlast).
- Live-Charts (`SimulationChartWidget::appendFrame`) und Atom-Tabelle prüfen: `moleculeUpdated` ist bereits auf einmal pro Lauf gedrosselt, `appendFrame` auf ~8 Hz für die Achsen; die Punktaufnahme pro Frame bleibt O(1).
- **Definition of Done:** Bei künstlich verlangsamtem GUI (z. B. 10k Atome + SSAO) bleibt die Eingabe reaktiv, die Queue-Länge bleibt 1; Charts zeigen weiterhin alle Steps.

**Aufwand:** klein–mittel (1–2 Tage). **Risiko:** gering.

---

## 6. WP P4 — NCI, Kollisionen, Wände vom GUI-Thread nehmen

- **NCI geometrisch bei Live-MD:** `refreshNciOverlay` ruft `nci::detectGeometric` synchron. Nach P1 hat der Worker Positionen + Bindungen + Zellliste; die geometrische Erkennung wandert in den Worker (`SimulationFrame::nciContacts` gibt es schon für GFN-FF). GUI macht nur `setNciContacts`. Ringsuche (`findAromaticRings`) bleibt topologiegebunden und läuft nur bei `topologyChanged`.
- **Kollisionen im Edit-Modus:** `computeCollisions` prüft alle Paare. Da nur die Selektion bewegt wird, genügt Selektion × Rest (O(k·N)) plus Zellliste. Kein Thread nötig.
- **Wände:** bereits O(N), bleibt.
- **Definition of Done:** Mit eingeschaltetem NCI-Overlay bei 1k Atomen keine messbare GUI-Mehrzeit pro Frame; Kontakttabelle aktualisiert weiterhin live.

**Aufwand:** mittel (2 Tage). **Risiko:** gering; die Erkennung ist bereits eine freie Funktion ohne GUI-Abhängigkeiten (`ncianalysis.*`).

---

## 7. WP P5 — GPU-Seite: adaptive Qualität während der Animation

Die GPU-Kosten (SSAO, Bloom, Schatten, MSAA High) sind unabhängig vom Thread-Modell, dominieren aber bei großen Systemen die Bildrate (Spike: 10k Atome ~30 FPS statisch, 20–30 FPS animiert).

- **Adaptive Quality:** während Live-MD/Opt und während Maus-Drag SSAO/Schatten/Bloom aus, MSAA auf Medium; nach 300 ms Ruhe zurück auf die Nutzereinstellung. `PerformanceOptimizer` hat das Gerüst („adaptive quality"), es ist nur nicht an den `SceneController` angebunden.
- **Bond-Instanzen halbieren:** Bindungen zwischen gleichfarbigen Atomen als *ein* Zylinder statt zwei Halbzylinder (der Spike nennt Bond-Instancing als lohnendsten Perf-Hebel: ~58k Zylinder beim synthetischen Grid).
- **LOD:** Kugel-/Zylinder-Tesselation nach Instanzzahl (`#Sphere`-Primitive durch eigenes `QQuick3DGeometry` mit 2–3 Stufen ersetzen).
- **Definition of Done:** FPS animiert bei 10k Atomen ≥ FPS statisch × 0.8; keine sichtbare Qualitätsänderung im Ruhezustand.

**Aufwand:** mittel–groß (3–5 Tage). **Risiko:** mittel (Qualitätsumschaltung darf nicht flackern; Hysterese wie bei den Bindungen).

---

## 8. WP P6 (optional) — Laden großer Trajektorien

Nicht Teil der Thread-Frage, aber derselbe Nutzer-Schmerz bei großen Dateien:

- Frames lazy parsen (Offset-Index beim ersten Durchlauf, Frame-Inhalt bei Bedarf) statt alle Frames in `m_trajectoryAtoms` zu halten.
- Speicher: `Atom` (Element als `QString`) pro Frame und Atom → Element/Typ einmal pro Struktur halten, pro Frame nur Positionen (`QVector<QVector3D>`).
- **Definition of Done:** 100k-Frame-VTF öffnet in < 2 s bis zum ersten Bild; Speicher linear in Atomen, nicht in Frames × Atomen für Metadaten.

**Aufwand:** groß. **Risiko:** mittel (viele Konsumenten von `m_trajectoryAtoms`).

---

## 9. WP P7 (geparkt) — Eigener Render-Thread über `QQuickRenderControl`

**Warum geparkt:** `QQuickWidget` rendert per Design auf dem GUI-Thread; ein eigener
Render-Thread hieße, die Szene mit einem selbst getriebenen `QQuickRenderControl`
in eine Vulkan-Textur zu rendern und diese in den Backing-Store-`QRhi` des
Hauptfensters zu übergeben. Dieser Textur-Transfer zwischen zwei `QRhi`-Instanzen
und Threads ist keine öffentliche Qt-API (native Handles, Semaphoren, Ownership),
widerspricht dem Grundsatz „minimale Abstraktion, Algorithmus sichtbar" und
bringt die Stacking-Probleme des nativen Fensters zurück, sobald man den Umweg
über ein natives Ziel geht.

**Wiedervorlage-Kriterium:** P0-Messung zeigt nach P1–P5, dass die *Render-Phase*
(nicht Sync, nicht CPU-Vorbereitung) auf dem GUI-Thread mehr als ~4 ms pro Frame
kostet **und** die Eingabe dadurch spürbar träge ist. Bis dahin bleibt
`QURCUMA_NATIVE_VIEWPORT=1` der Vergleichspfad für Benchmarks.

---

## 10. Reihenfolge und Abhängigkeiten

```
P0 Messen ──► P1 Bindungen im Worker (+ Zellliste)
                 │
                 ├──► P2 Position-only-Fast-Path (Instanzpuffer)
                 ├──► P3 Frame-Koaleszenz (unabhängig, jederzeit)
                 └──► P4 NCI/Kollisionen (braucht Zellliste aus P1)
P5 Adaptive GPU-Qualität: unabhängig, parallel zu P2–P4 möglich
P6 Laden: unabhängig, eigenes Paket
P7 Render-Thread: nur nach Wiedervorlage-Kriterium
```

Empfohlene Reihenfolge: **P0 → P3 → P1 → P2 → P5 → P4**. P3 ist billig und macht
die Eingabe sofort reaktiv, auch wenn die Frame-Kosten noch hoch sind; P1 nimmt
den größten Einzelposten; P2 den zweitgrößten; P5 wirkt bei großen Systemen auf
die GPU-Grenze.

## 11. Risiken (übergreifend)

| Risiko | Gegenmaßnahme |
|---|---|
| Verhalten der Bindungs-Hysterese ändert sich beim Umzug in den Worker | Referenz-Test Vollpaar vs. Zellliste; Toleranzen als gemeinsame Konstanten |
| Cache-Invalidierung im Position-only-Pfad vergisst einen Fall (Hover, Selektion, Tint) | Zentrale `invalidateAtomStyle()`; Screenshot-Regressionstest über `exportImage` |
| Adaptive Qualität flackert | Hysterese/Verzögerung, Umschalten nur an Frame-Grenzen |
| Messwerte plattformabhängig (RADV vs. NVIDIA) | P0-Tabelle mit GPU/Treiber-Angabe, mindestens zwei Rechner |
