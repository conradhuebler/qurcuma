# Arbeitspaket (WP): UX-Entschlackung und Neuordnung

> **Status:** in Arbeit, Etappen 0 bis 5 und 6 S1–S4 committet. Erstellt 2026-09-25, Claude Generated.
> **Branch:** `claude/dockwidgets-overlap-click-issue-th1rwk` (nicht gepusht).
> **Test:** Build und Unit-Tests je Etappe grün. In der GUI hat der Operator bisher nur
> Etappe 3 kurz angesehen („sieht erstmal gut aus“); 0, 1, 2, 2b, 4a, 4b, 5 und 6 S1–S4 sind dort ungetestet.
> Die GUI rendert aus der Agenten-Shell nicht, Sichtprüfung also nur durch den Operator.

## Ziel und Ordnungsprinzip

Die Oberfläche war mit curcuma gewachsen:
- rund 195 Bedienelemente (grob gezählt),
- die NCI-Quelle an 6 Stellen einstellbar,
- 4 sich überlappende Preset-Konzepte,
- ein versteckter Bindungs-Edit-Modus.

Ordnung in drei Zugriffsstufen:

1. **Schnellschalter**, ein Klick oder eine Taste in der Viewer-Leiste: NCI ▾, H-Brücken
   (`Shift+N`), H-Anzeige (`H`: All/Polar/None), Moleküle ausblenden, Stil ▾ (`1`–`4`).
2. **Looks** (`src/look.h`): Farbschema, Material, Fog, SSAO, Bloom, HDR/Belichtung,
   Eckenlichter, Hintergrund. Ein Look setzt **nie** einen Schnellschalter;
   `MoleculeViewer::applyLook` ruft nur die Setter dieser Felder auf.
3. **Details** im Appearance-Dock, das standardmäßig geschlossen ist
   (Look ▾ ▸ Details…).

Die Operator-Entscheidungen vom 2026-09-25 stehen jeweils bei der Etappe, zu der sie gehören.

## Erledigt

| Etappe | Commit | Inhalt |
|---|---|---|
| Wayland | `4d4c433` | Docks per Drag zurückdocken unter Wayland (`MainWindow::forwardDockDragEvent`); `GroupedDragging` unter Wayland aus (Qt-6.11-Absturz beim Andocken schwebender Tab-Gruppen) |
| 0 | `710333e` | Ctrl+Shift+S doppelt, Load-Workspace-Stub, Frame-Spinbox, Ctrl+A, pdb/mol2 über `loadMoleculeFile`, Kontextmenü der Dateiliste, deutsche UI-Strings, totes RMSD-MTD-Feld „Pace“, toter Code |
| 1 | `c6b3ee3` | H-Anzeige All/Polar/None (`SceneController::computeHydrogenMask`, Taste `H`), H-Brücken-Schalter `Shift+N`, Signal `nciOptionsChanged` |
| Fix | `742640b` | Messung folgt Live-MD/Opt (`updateMeasurement(0)`) und der Wiedergabe |
| 2 | `4fb33be` | Viewer-Leiste: Werkzeug-Schalter View·Measure·Edit·Build, Esc stufenweise zurück, Bond-Werkzeuge im Build ▾, Schnellschalter-Buttons, Look ▾; leeres NCI-Dropdown behoben (Menü wurde vor `createMenus` übergeben) |
| 2b | `dd4dcf6` | Moleküle sortenweise ausblenden (Display ▸ Hide Molecules, nach Summenformel; gleiche Maske wie die H-Anzeige) |
| 3 | `da4c87d` | Look-System (4 eingebaute Looks + eigene), Views nur Kamera, letzte Sitzung wird beim Beenden gespeichert, alte Display-Presets einmalig verworfen |
| 4a-1 | `fad4ea4` | Structure-Dock (XYZ + Atomtabelle) und Appearance-Dock (Panel + Views, geschlossen) getrennt; Layouts und Dock-Anteil der Workspaces einmalig zurückgesetzt (`UiLayoutVersion` = 2); `test_looks` als eigenes Target |
| 4a-2 | `56e53c5` | Tools-Abschnitt aufgelöst: Sim-Anzeigen → Simulation-Dock „Show in viewer“ (`SimulationViewOptions`), Center-on-Load und Maus-Rotation → Settings-Menü, Docking-Vorschau → Build ▾ |
| 4a-3 | `bede030` | NCI-Optionen → Interactions-Dock „Options“ (`NciOptionsWidget`); NCI-Abschnitt und zweite Quellenauswahl im Panel entfernt |
| 4a-4 | `34eb95e` | Appearance geglättet: eine flache Gruppe Style (Modus, Farben, Größen, Beschriftungen, Hintergrund), dann Fragmente/Bead-Typen; Material, Lighting, Effects im eingeklappten „Advanced“ |
| 4b | `a24a70b` | Modi Explore · Compute · Teaching im Eckschalter (Teaching = Explore + Lesson-Browser); Layout-Presets, Ctrl+Alt+1–5 und `DockConfig::LayoutPreset` entfernt; ohne gespeichertes Layout legt der Modus die Docks an |
| 5 | `ceb56c9` | Menüleiste File · Edit · View · Structure · Simulation · Tools · Help; Simulation-Einträge starten wirklich; Werkzeug-Radio mit `M`/Ctrl+E/`B`; Help ▸ Keyboard Shortcuts aus den QActions; Kontextmenü nach Atom/leerer Fläche getrennt; Palette nur aus der Menüleiste |
| 4b+ | `4c19e7e`, `7fdc8ef`, `90fa070` | Jeder Modus merkt sich seine Panels (`ui/modePanels/<mode>`), Teaching öffnet standardmäßig Simulation; Lessons speichern die offenen Panels (`layout.panels`) und öffnen sie beim Laden, dabei Wechsel in Teaching (abschaltbar unter Edit ▸ Preferences) |
| 4b++ | `c5ada07` | Teaching: Lesson-Abschnitt oben im Project-Panel (Metadaten, eigene Strukturliste, Detail-Editor, Drop-Ziel), Datei-Browser darunter zeigt immer Dateien; Files/Lesson-Umschalter entfällt (Operator 2026-09-26: „Dateien erreichbar, nur nicht als erstes Bedienelement“) |
| 6 S1 | `cbaf7ae`, `3d78898`, `876bd3f` | RMSD-MTD auf `strided` (Deposit every, Hill spacing, ohne `rmsd_econv`); Charge und ungepaarte Elektronen; Simulation-Dock: Basis-Gruppen + Abschnitte mit Schalter im Kopf + „Advanced“; H-Massenfaktor ganzzahlig |
| 6 S2 | `284946b` | Rezepte (`src/recipe.h`): Protokoll ohne System/Maschine/Vorlieben, 4 eingebaute + eigene, Simulation ▸ Recipe und Knopf im Dock; Max iterations im Opt-Modus (`8820e81`); Test `test_recipes` |
| 6 S3 | `e2de967` | Tab „All parameters“: alle `simplemd`-Parameter aus der Registry; Hand-Parameter read-only mit gesendetem Wert, Rest editierbar, nur Abweichungen vom Default gesendet (`mdExtraParams`); `relevantWhen` graut aus |
| 6 S4 | `529ded8` | Parameter-Protokoll je Lauf: Abweichungen vom curcuma-Default mit Quelle ins Output-Panel und nach `run-parameters.jsonl`; Tools ▸ Parameter Usage zählt je Modus/Parameter/Quelle |

Operator-Entscheidungen, die in diesen Etappen umgesetzt sind:
- **Modus-Trennung:** Einsteiger und Forschende werden über die Modi getrennt.
- **Umfang:** Die Struktur darf neu geordnet werden.
- **H-Anzeige:** Vorbild ist die Strukturformel der Organik. Die Stufe „Polar“ blendet nur C–H aus.
- **Reset statt Migration:** Alte Presets und alte Dock-Layouts werden einmalig verworfen.
- **Letzte Sitzung:** Die Anzeige wird beim Beenden gesichert und beim Start wiederhergestellt; „Save/Load Defaults“ entfällt.
- **Eingebaute Looks:** Default, Publication, Presentation, Flat (Teaching).
- **Details-Dock:** Die Darstellungsdetails bekommen ein eigenes Dock, standardmäßig geschlossen.
- **RMSD-MTD:** Wird in Etappe 6 S1 an curcumas `strided`-Schema angeglichen (Option c).
- **Teaching und Simulation (2026-09-26):** Teaching zeigt Simulation standardmäßig, jeder Modus merkt sich seine Panels, und eine Lesson speichert ihren Panel-Stand wie ein Workspace. Gespeichert wird nur, welche Panels offen sind, nicht der binäre Dock-Zustand.

## Offen: die nächsten Etappen

### 6: Simulationsparameter, Versuchsfahrplan
Grundlage ist curcumas `ParameterRegistry` (`external/curcuma/src/core/parameter_registry.h`). Sie liefert je Parameter `tier` (primary/advanced/expert), `relevantWhen`, Einheit, Min/Max und erlaubte Werte. qurcuma füllt die Registry schon in `src/main.cpp`.

- **S1, Basis/Erweitert von Hand:** erledigt (siehe Tabelle). Nicht in der Oberfläche: `external_potentials` (auch primary) und `max_time` (qurcuma zeigt Schritte).
- **S2, Rezepte:** erledigt (siehe Tabelle). Die vier eingebauten Rezepte sind Vorschläge, ihre Werte stehen in `src/recipe.h` und sind vom Operator noch nicht geprüft.
- **S3, generierte Expertenansicht:** umgesetzt als Tab „All parameters“ (siehe Tabelle), nur `simplemd`. **Offen:** Vergleich an 2–3 echten Läufen durch den Operator, danach Entscheidung, ob die Handgruppen auf Basis plus generierten Rest schrumpfen.
- **S4, Erfahrung sammeln:** Protokoll umgesetzt (siehe Tabelle): `run-parameters.jsonl`, Auswertung unter Tools ▸ Parameter Usage. **Offen:** die Zahl N der Läufe festlegen, nach der über Basis und Expert entschieden wird.
  - Erster Befund aus je einem automatisch gestarteten Lauf mit qurcumas Standardeinstellungen (1 MD-Lauf, 1 Optimierung, `conf_28.xyz`, 114 Atome, `qurcuma <datei> -md|-opt` mit eigenem `XDG_DATA_HOME`): Schon ohne Eingriff weichen qurcumas Vorgaben von curcumas ab, MD `temperature` 300 statt 298.15 K, `max_time` 10000 statt 1000 fs, `write_xyz` aus; Optimierung `gradient_threshold` 1e-6 statt 5e-4 Eh/Bohr, `max_iterations` 10000 statt 5000. Operator 2026-09-26: Temperatur und Gradient folgen jetzt curcumas Default (298.15 K, 5e-4 Eh/Bohr); danach weichen in derselben Messung nur noch `max_time`, `write_xyz`, `max_iterations`, `write_trajectory` und die von qurcuma fest gesetzten Werte ab.

## Offene TODOs außerhalb der Etappen
- **Ladungsquelle für „Color by Charge“ (zu klären):**
  - Heute kommen Ladungen nur als Nebenprodukt der NCI-Analyse mit GFN-FF/GFN2: `NciAnalysisWorker` → `chargesReady` → `setAtomCharges`, und nur für den aktuellen Frame.
  - Kandidaten: GFN-FF-EEQ direkt anfordern oder Partialladungen aus Rechnungen bzw. Dateien übernehmen. Ob sie im Live-MD mitlaufen sollen, ist ebenfalls offen.
- **Nicht abgedeckt beim Ausblenden von Molekülen:** Pi-Stacking-Linien (Ring-Zentroide ohne Atomindex) und RMSD-Overlays.
- **Restrisiko Wayland:** Ein Layout, das eine schwebende Tab-Gruppe enthält, würde sie wiederherstellen. Nach dem Layout-Reset in 4a-1 ist das nur für neu entstehende Layouts relevant, und `GroupedDragging` ist unter Wayland aus.

## Praktisches für die nächste Session
- **Plan-Datei:** `~/.claude/plans/gibt-es-eine-m-glichkeit-wondrous-hippo.md`. Sie enthält die Bestandsaufnahme und die Etappen im Detail, dazu „Nachträge aus dem Betrieb“.
- **Build:** `cmake --build debug > log 2>&1; echo $?`, den Exit-Code prüfen und den Log nicht in den Kontext ziehen.
- **CMake-Kosten:** Ein CMake-Configure nach neuen Commits baut ganz curcuma neu, etwa 10 Minuten. CMake-Änderungen daher bündeln.
- **Tests:** `test_looks`, `test_fragments` (H-Maske, Molekülsorten), `test_buildtools`, `test_nci`, `test_measurements`, jeweils mit `QT_QPA_PLATFORM=offscreen`.
- **clangd:** Die Diagnosen in diesem Projekt sind unzuverlässig, maßgeblich ist der cmake-Build.
- **Architektur-Regeln:**
  - Der Viewer ist die einzige Quelle des Anzeigezustands, Panels lesen nur (`syncFromViewer`).
  - Neue Display-Felder gehören in `DisplaySettings` plus das Paar `currentDisplaySettings`/`applyDisplaySettings` und in `writeVizSettings`/`readVizSettings`.
  - Geteilte QActions liegen in `MainWindow::createMenus`. Die Leistenmenüs werden am Ende von `createMenus` übergeben (`setNciQuickMenu`, `setQuickAccess`).
