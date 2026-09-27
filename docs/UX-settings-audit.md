# Prüfung: Geht beim UX-Umbau eine Einstellung verloren?

> Claude Generated 2026-09-27. Gehört zu `docs/WP-ux-restructure.md` (Verifikation, Etappe 4).
> Vorher = `4d4c433` (Stand direkt vor dem Umbau), nachher = `f353475`.

## Methode

Die im Plan genannte Tabelle „Einstellung × Ort“ aus der Bestandsaufnahme wurde nie
festgehalten. Statt sie aus dem Gedächtnis nachzubauen, vergleicht ein Skript, was die
Oberfläche vorher und nachher erreichen kann:

`python3 docs/development/audit_ui_reach.py 4d4c433 HEAD`

- **viewer:** jede Viewer-Methode, die UI-Code außerhalb von `view.cpp` aufruft
  (`m_viewer->…`, `m_moleculeView->…`, `viewer->…`).
- **cfg:** jedes Feld, das `SimulationControlWidget::buildConfig()` setzt.
- **nci:** jedes Feld von `nci::Options`, das Display-Panel, NCI-Widget oder MainWindow schreiben.
- **slots:** jeder mit `&MainWindow::…` verbundene Slot.

Ergänzend von Hand: die Aufrufe in der Viewer-Leiste (`MoleculeViewer::setupControlPanel`,
die das Skript auslässt) und die Felder von `DisplaySettings`.

Jeder Name, der vorher erreichbar war und nachher fehlt, ist ein Kandidat und wurde einzeln
im Code nachgesehen.

## Ergebnis

Keine Einstellung und keine Aktion ist ersatzlos verloren. Jeder Kandidat ist entweder
an einen anderen Ort gewandert oder mit Beleg bewusst entfernt.

| Bereich | vorher | nachher | fehlt nachher |
|---|---|---|---|
| Viewer-Methoden aus UI-Code | 171 | 175 | 6, alle aufgeklärt |
| `SimulationConfig`-Felder in `buildConfig` | 50 | 53 | 2, bewusst entfernt |
| `nci::Options`-Felder aus UI-Code | 8 | 8 | keins |
| verbundene MainWindow-Slots | 54 | 58 | 2, aufgeklärt |
| Aufrufe in der Viewer-Leiste | 9 | 15 | 1, verschoben |
| `DisplaySettings`-Felder | 35 | 39 | keins |

## Kandidaten und wo sie jetzt sind

| Kandidat | Status | Beleg |
|---|---|---|
| `setBondEditMode`, `getBondEditMode` | verschoben | Viewer-Leiste Build ▾ ▸ Bond-Werkzeuge, `src/view.cpp:5079` (Etappe 2, `4fb33be`) |
| `setDockPreviewEnabled`, `dockPreviewEnabled` | verschoben | Build ▾ ▸ Live Docking Preview, `src/view.cpp:5092` (Etappe 4a-2, `56e53c5`) |
| `getMeasurementMode` | nur Abfrage entfallen | Messen über Viewer-Leiste und Structure ▸ Measure (`M`) mit `setMeasurementMode` |
| `resetViewToMolecule` | bewusst entfernt (toter Code) | einziger Aufrufer war `MainWindow::zoomToMolecule`, nie verbunden; entfernt in `710333e`. Funktion vorhanden als View ▸ Fit in View |
| `rmsdMtdPace` | bewusst entfernt | curcuma ignoriert `rmsd_mtd_pace` unter `strided`; `710333e` |
| `rmsdMtdEconv` | bewusst entfernt | Legacy-Schema, curcuma warnte bei jedem Lauf; `cbaf7ae` |
| Slot `switchEditorTab` (Ctrl+Tab) | bewusst entfernt | wechselte ein beliebiges Tab-Widget; `a9262bd` |
| Slot `updateDirectoryContent` | nur Verbindung entfallen | wurde vom Files/Lesson-Umschalter ausgelöst (entfernt in `c5ada07`); die Funktion wird weiter aufgerufen, `src/mainwindow.cpp:2270`, `:2549`, `:2744` |
| Viewer-Leiste `setColorScheme` | verschoben | Look ▾ ▸ Colour Scheme und Appearance ▸ Style ▸ Colors (Etappe 2/3) |

## Bewusst entfallene Bedienwege (ohne Verlust der Einstellung)

Diese Wege gibt es nicht mehr; was sie einstellten, ist anders erreichbar.

- Display-Presets, „Include display settings“, Save/Load Defaults → Looks (Etappe 3, `da4c87d`).
- Fünf Layout-Presets und Ctrl+Alt+1–5 → Modi Explore · Compute · Teaching (Etappe 4b, `a24a70b`).
- Display-Menü und Settings-Menü → View und Edit ▸ Preferences (Etappe 5, `ceb56c9`).
- Kopie des Display-Menüs im Viewport-Kontextmenü → Schnellschalter, Style, Look (Etappe 5).
- Handverlesene Palette-Einträge → alle als Menü-Aktionen (Etappe 5).
- Files/Lesson-Umschalter → Lesson-Abschnitt oben im Project-Panel (`c5ada07`).

## Grenzen

Die Prüfung erkennt eine verlorene Einstellung nur, wenn sie über einen der vier
verglichenen Wege lief. Eine Einstellung, die nachher zwar noch aufrufbar, aber schwer zu
finden ist, zeigt sie nicht; das klärt nur der Test in der Oberfläche.
