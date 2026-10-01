# AIChangelog - Qurcuma Improvements

Newest first; one line per fact. Entries from August 2026 and earlier: [docs/changelog/AIChangelog_2026-08-and-earlier.md](docs/changelog/AIChangelog_2026-08-and-earlier.md).

## September 2026 - WASD im View- und Measure-Werkzeug

- Q und E rollen jetzt in die jeweils andere Richtung als zuvor (getauscht); das Verschieben mit Shift+Q/E im Edit-Werkzeug bleibt, wie es war.
- W A S D Q E drehen die Szene jetzt auch im View- und im Measure-Werkzeug, solange die 3D-Ansicht den Fokus hat (hineinklicken); Listen anderswo behalten die Buchstaben für ihre Schnellsuche. Edit-Werkzeug und laufende Simulation wie bisher, im Build-Werkzeug bleiben die Buchstaben Elementtasten.

## September 2026 - UX-Umbau: Restpunkte

- **Moleküle ausblenden** wirkt jetzt auch auf Pi-Stacking-Linien (Ring-Zentroide; über ein Ringatom als Eigentümer) und auf RMSD-Overlays (eigene Fragmentzerlegung je Overlay, `SceneController::computeMoleculeKindMask`).
- Look ▸ Details… öffnet das Appearance-Dock direkt bei „Advanced“.
- Entfernt: Ctrl+Tab (wechselte ein beliebiges Tab-Widget), der doppelt gesetzte Fenstertitel, das nie gesendete Signal `displayOptionsRequested`.

## September 2026 - curcumas Defaults für Temperatur und Gradient

- MD-Temperatur startet mit 298.15 K (vorher 300 K), die Gradient-Toleranz der Optimierung mit 5e-4 Eh/Bohr (vorher 1e-6), beides curcumas Defaults; das Laufprotokoll führt sie ohne Eingriff nicht mehr als Abweichung.
- Die Temperaturanzeige am Regler zeigt bis zu zwei Nachkommastellen (298.15 K statt gerundet 298 K).

## September 2026 - UX-Etappe 6 S4: Parameter-Protokoll je Lauf

- **Jeder MD-Lauf und jede Optimierung** schreibt ins Output-Panel, welche gesendeten Parameter vom curcuma-Default abweichen (Wert, Default, Quelle: Simulation-Tab, All parameters, von qurcuma fest gesetzt), und hängt den Datensatz an `run-parameters.jsonl` im Datenverzeichnis von qurcuma an. Einzelschritte und das Clean-up des Builders zählen nicht.
- **Tools ▸ Parameter Usage**: Tabelle, wie oft jeder Parameter über die protokollierten Läufe abwich (je Modus und Quelle, sortierbar), mit „Clear Log…“.

## September 2026 - Teaching: Lesson oben, Dateien bleiben erreichbar

- **Lesson-Abschnitt oben im Project-Panel** („Lesson (N)“, einklappbar): Metadaten, eigene Strukturliste, Detail-Editor; Dateien per Drag&Drop hinzufügen. In Teaching offen und als erstes Element, sonst sichtbar, sobald die Lesson Strukturen hat.
- Der Datei-Browser zeigt in jedem Modus Dateien; der Files/Lesson-Umschalter entfällt. Vorher ersetzte Teaching die Dateiliste durch die Lesson, und ein Klick auf ein Verzeichnis änderte scheinbar nichts.

## September 2026 - UX-Etappe 6 S3: Tab „All parameters“

- **Simulation-Dock ▸ All parameters**: alle Parameter von curcumas MD-Modul `simplemd`, erzeugt aus der `ParameterRegistry`, nach Kategorie, filterbar nach tier, Suche und „Changed only“.
- Vom Simulation-Tab oder von qurcuma gesetzte Parameter sind read-only und zeigen den gesendeten Wert; die übrigen sind editierbar, gesendet und gespeichert werden nur Abweichungen vom curcuma-Default (`mdExtraParams`, auch in Lessons und Rezepten).
- Parameter, deren `relevantWhen`-Bedingung nicht erfüllt ist, sind ausgegraut. Nur MD; der Optimizer-Pfad liest die Defaults des gewählten Optimizers, nicht das Registry-Modul `opt`.
- Tabs des Simulation-Docks werden über ihre Seite gewählt, nicht über die Position.

## September 2026 - UX-Etappe 6 S2: Simulations-Rezepte

- **Rezepte** (`src/recipe.h`): benannte Protokolle (Modus, Temperaturführung, Zeitschritt, Laufzeit, Constraints, Bias, Wände), angewendet über die aktuelle Konfiguration; Methode, Ladung, ungepaarte Elektronen, GPU und Arbeitsvorlieben bleiben.
- Eingebaut: Quick relax, MD 300 K, Heat up, Confined; eigene speichern und löschen. Erreichbar über Simulation ▸ Recipe und den Recipe-Knopf neben dem Modus im Simulation-Dock.
- **Max iterations** im Opt-Modus sichtbar (vorher im MD-Block verborgen, obwohl es die Iterationszahl setzte).
- Neuer Test `test_recipes`: vollständiger `SimulationConfig`-JSON-Roundtrip, Lesson-Panels, Rezept-Regeln.

## September 2026 - UX-Etappe 6 S1: Simulationsparameter Basis/Erweitert

- **Basis = curcumas primary-Parameter**: Method-Gruppe (Method, Optimizer im Opt-Modus, Charge, Unpaired electrons) und MD-Gruppe (Temperatur, Thermostat, Time step, Total steps); Thermostat-Felder nur, wenn der gewählte Thermostat sie liest.
- **Charge und ungepaarte Elektronen** neu (curcuma `charge`/`spin`), für MD und Optimierung, in Lessons gespeichert; vorher lief alles als neutrales Singulett.
- **Ein Muster für optionale Funktionen**: `CollapsibleSection` mit Schalter im Kopf für Temperature Ramp, Temperature Regions (neu schaltbar), RATTLE, RMSD Metadynamics, Confinement Walls; Rest in „Advanced“, Grab-Einstellungen in „Interactive Grab“.
- **RMSD-MTD auf curcumas `strided`-Schema**: Deposit every (`rmsd_mtd_deposit_stride`) und Hill spacing (`rmsd_mtd_r_dep`); `rmsd_econv` entfällt, damit auch curcumas Warnung bei jedem Lauf.
- **H-Massenfaktor ganzzahlig**: curcuma liest `hydrogen_mass` als Int, 1,5 wurde zu 1.

## September 2026 - Panels je Modus und je Lesson

- **Jeder Modus merkt sich seine Panels** (`ui/modePanels/<mode>`, beim Verlassen und beim Beenden gespeichert); vorher setzte jeder Moduswechsel und jeder Start einen festen Satz. Teaching öffnet standardmäßig zusätzlich Simulation.
- **Lessons tragen ihre Panels** (`layout.panels` in der `.qlesson.json`, optional): Save Lesson speichert die offenen Panels, Open Lesson öffnet sie wieder. Nur welche Panels offen sind, keine Größen.
- **Open Lesson wechselt in den Teaching-Modus**, abschaltbar unter Edit ▸ Preferences ▸ Open Lessons in Teaching Mode (Standard an).

## September 2026 - UX-Etappe 5: Menüs neu geschnitten

- **Menüleiste** File · Edit · View · Structure · Simulation · Tools · Help; Display geht in View auf, Settings wird Edit ▸ Preferences, Molecule verteilt sich auf Structure und Simulation.
- **Simulation ▸ Start MD / Start Optimization** starten einen Lauf mit den Parametern des Simulation-Docks (vorher nur Dock nach vorn); Stop, Parameters, Snapshots, Charts.
- **Structure**: Werkzeug als Radio View · Measure (`M`) · Edit (Ctrl+E) · Build (`B`), Add Molecule, Add Hydrogens, Move to Origin, RMSD / Align.
- **Tools**: Run Calculation (Ctrl+R/F5), New Calculation Directory (Ctrl+N), Clear Output (Ctrl+L), NMR Spectra (vorher nur auf der Rechen-Toolbar), Configure Programs.
- **Neu als QAction**: Select All, Deselect All (Ctrl+Shift+A), Quick Photo, View ▸ Views (Front/Top/Side + gespeicherte Ansichten), Mode als Radio.
- **Help ▸ Keyboard Shortcuts**: Tabelle aus den Menü-Aktionen; **About** mit Version aus CMake und Jahr 2026.
- **Viewport-Kontextmenü**: auf einem Atom die Atom-Aktionen, auf leerer Fläche Kamera, Schnellschalter, Style, Look, Deselect, Quick Photo; die Kopie des Display-Menüs entfällt.
- **Palette** nur noch aus der Menüleiste, ohne handverlesene Doppelungen; zeigt alle Kürzel einer Aktion.
- Tooltip des Edit-Werkzeugs korrigiert (Nudge per Shift+WASD/QE, nicht Pfeiltasten).

## September 2026 - UX-Etappe 4b: Modi statt Layout-Presets

- **Eckschalter mit drei Modi** Explore, Compute, Teaching (auch View ▸ Mode und Palette); Teaching = Explore mit dem Lesson-Browser im Project-Panel.
- Die fünf Layout-Presets (View ▸ Layout Presets, Ctrl+Alt+1–5) und `DockConfig::LayoutPreset` entfallen; eigene Layouts speichern die Workspaces.
- Ohne gespeichertes Layout (Erststart, Workspace ohne Layout, Reset to Default Layout) legt der aktuelle Modus die Docks an; vorher das Preset „Analysis“.

## September 2026 - UX-Etappe 4a-4: Appearance geglättet

- **Appearance-Panel** ohne verschachtelte Abschnitte: oben die Gruppe „Style“ (Modus, Farben, Atomgröße, Bindungsdicke, Beschriftungen, Hintergrund als Farbfeld), darunter Fragmente und Bead-Typen, sofern die Struktur welche hat.
- **„Advanced“** (eingeklappt, Zustand in `ui/displayPanel/advancedExpanded`): Material, Lighting (Eckenlichter) und Effects (Fog, SSAO, Bloom, HDR).

## September 2026 - UX-Etappe 4a-3: NCI-Optionen im Interactions-Dock

- **Interactions-Dock** = einklappbare „Options“ (`NciOptionsWidget`: Kontaktarten, H-Brücken-Grenzen, Farben, Distanz-Labels, Live-GFN-FF während MD) über der Kontakttabelle; vorher der NCI-Abschnitt im Display-Panel mit einer zweiten Quellenauswahl.
- Display ▸ NCI Options… öffnet das Interactions-Dock mit aufgeklappten Optionen.
- Farb-Swatch-Helfer gemeinsam in `widgets/colorswatch.h`.

## September 2026 - UX-Etappe 4a-2: Tools-Abschnitt aufgelöst

- **Measure** und **Bond Edit** gibt es nur noch in der Viewer-Leiste (Werkzeug-Schalter, Build ▾).
- **Simulation dock ▸ „Show in viewer“** (einklappbar, `SimulationViewOptions`): Confinement walls, Wall opacity, Wall potential shells, Wall force field, Force vectors while grabbing, Dynamic bonds; vorher im Tools-Abschnitt des Display-Panels.
- **Settings ▸ Center Molecule on Load** und **Settings ▸ Mouse Rotation** (Molekül drehen / Kamera umkreisen).
- **Build ▾ ▸ Live Docking Preview**.

## September 2026 - UX-Etappe 4a-1: Structure und Appearance getrennt

- **Structure-Dock** (`docks/structuredock.*`, vorher `DisplayDock`): nur noch [Structure | Atoms], also XYZ-Editor und Atomtabelle.
- **Appearance-Dock** (`docks/appearancedock.*`, neu): Display-Panel und Kamera-Views, rechts als Tab, **standardmäßig geschlossen** (Look ▸ Details… bzw. View ▸ Dock Panels öffnet es).
- View ▸ Dock Panels: Structure, Appearance und jetzt auch Images.
- Gespeicherte Dock-Layouts und der Dock-Anteil gespeicherter Workspaces werden einmalig verworfen (`DockConfig::UiLayoutVersion` = 2; Operator-Entscheidung). Workspaces behalten Namen und Arbeitsverzeichnis.
- `test_looks` als eigenes Test-Target (vorher Teil von `test_fragments`).

## September 2026 - UX-Etappe 3: ein Look-System, Views nur Kamera, letzte Sitzung

- **Looks** (`src/look.h`): Farbschema, Material, Fog, SSAO, Bloom, HDR/Belichtung, Eckenlichter, Hintergrund und sonst nichts. `MoleculeViewer::applyLook` setzt nur diese Felder; ein Look kann also keinen Schnellschalter umstellen. Eingebaut: Default, Publication, Presentation, Flat (Teaching); eigene Looks speichern/löschen im Look-Menü (Leiste, Display ▸ Look, Kontextmenü, Palette).
- **Ersetzt** die Display-Presets (Quick/Custom), „Include display settings“ bei den View-Presets und Save/Load Defaults. Alte Display-Presets einmalig verworfen (`dropLegacyDisplayPresetsOnce`, Operator-Entscheidung, keine Migration).
- **Views** (`ViewPreset`) tragen nur noch die Kamera; `applyViewPreset` setzt keine Anzeige mehr.
- **Letzte Sitzung**: die Anzeige wird beim Beenden gespeichert (`MainWindow::closeEvent`) und beim Start wiederhergestellt, jetzt inklusive Hintergrund, Eckenlichtern und Fog-Distanz. Im Panel bleibt nur „Reset to Factory Settings“.
- **Export-Dialog**: View (Kamera) und Look getrennt wählbar; beide Namen stehen in den PNG-Metadaten (`ViewPreset`, `Look`).
- `applyDisplayPreset` aus Etappe 1 entfernt (ohne Aufrufer, durch `applyLook` überflüssig).

## September 2026 - UX-Etappe 2b: Moleküle sortenweise ausblenden

- **Display ▸ Hide Molecules** (auch Leisten-Button neben H und Kontextmenü): listet die Molekülsorten der geladenen Struktur nach Summenformel mit Anzahl (z. B. „H2O ×120“), jede einzeln ausblendbar, dazu „Show All“. Nur Anzeige; verborgene Moleküle werden nicht gezeichnet, beschriftet oder gepickt, NCI-Kontakte zu ihnen entfallen.
- Gleiche Maske wie die H-Anzeige (`SceneController::isAtomHidden`); Auswahl gehört zur Struktur, wird beim Laden einer neuen zurückgesetzt und im Build-Modus ignoriert. Geprüft in `test_fragments`.

## September 2026 - UX-Etappe 2: Viewer-Leiste

- **Werkzeug-Schalter** View · Measure · Edit · Build (exklusiv, gespiegelt aus `interactionModeChanged`) statt drei einzelner Toggles; **Esc** geht eine Ebene zurück (Fragment fallen lassen → Auswahl/Marken löschen → zurück zu View).
- **Bond-Werkzeuge** (Bindung hinzufügen/löschen/Ordnung durchschalten per Klick auf zwei Atome) im Build-Dropdown, mit Hinweis im Viewport; vorher nur über eine Combobox im eingeklappten Tools-Abschnitt erreichbar.
- **Schnellschalter** H-Brücken · H ▾ · Stil ▾ neben NCI ▾, dazu **Look ▾** (Farbschema, Details…); Farb-Combo und „Display ⚙“-Button entfallen.
- Frame-Navigation und Play/Pause mit gezeichneten Symbolen; die Trenner verschwinden mit ihren Gruppen bei Einzelstrukturen.
- Clash-Anzeige auch im Build-Modus (rote Atome werden dort schon berechnet).
- **NCI-Dropdown der Leiste** war leer: `setNciQuickMenu` lief aus `setupUI` vor `createMenus` und übergab einen Nullzeiger; die Leistenmenüs werden jetzt am Ende von `createMenus` übergeben.
- Render-Style- und Farbschema-Aktionen starten mit dem Häkchen auf dem gespeicherten Zustand statt auf Modus 0.

## September 2026 - Messung folgt Live-MD/Opt und Wiedergabe

- **`MoleculeViewer::updateMeasurement(frame)`** läuft jetzt auch im Live-Pfad (`updateSimulationFrame`, misst Frame 0, in den der Live-Pfad schreibt) und bei der Wiedergabe (`updateFramePositions`). Vorher blieben Messlinien und HUD-Werte auf der Geometrie stehen, bei der gemessen wurde.

## September 2026 - UX-Etappe 1: Schnellschalter H-Anzeige und H-Brücken

- **H-Anzeige** (Display ▸ Hydrogens, Taste `H` schaltet durch): All / Polar (wie in Strukturformeln: H nur an C wird ausgeblendet) / None. Nur Anzeige; verborgene Atome werden nicht gezeichnet, beschriftet oder gepickt, NCI-Linien setzen am gebundenen Atom an. Im Build-Modus immer alle H. Regel in `SceneController::computeHydrogenMask`, geprüft in `test_fragments`.
- **Hydrogen Bonds** (Display-Menü, `Shift+N`): schaltet H-Brücken im NCI-Overlay; beim Einschalten geht das Overlay mit an. Panel-Checkbox und Menü spiegeln sich über `nciOptionsChanged`.
- **Presets setzen keine Schnellschalter mehr** (`MoleculeViewer::applyDisplayPreset`): Laden eines View- oder Display-Presets schaltete bisher das NCI-Overlay aus, weil Presets diese Felder mit Defaults trugen (View-Presets speichern sie gar nicht).
- RMSD-MTD „Conv. threshold“ als „(legacy)“ gekennzeichnet: curcumas Standardschema `strided` ignoriert `rmsd_econv`.

## September 2026 - UX-Etappe 0: Fehler und Stubs

- **Ctrl+Shift+S** war doppelt belegt (Save As und Save Workspace) und löste deshalb keins von beiden aus; Save Workspace hat jetzt kein Kürzel.
- **File ▸ Workspaces ▸ Load Workspace…** öffnet eine Auswahlliste (vorher nur eine Statusmeldung).
- **Frame-Sprung-Spinbox** springt jetzt (`showFrame`) und zählt wie das Label ab 1.
- **Ctrl+A** wählt alle Atome des aktuellen Frames (vorher nur 0…max. bereits gewählter Index, bei leerer Auswahl nichts).
- **pdb/mol2** laden über `loadMoleculeFile` wie xyz/vtf (vorher Stub „coming soon“ beim Öffnen, im Kontextmenü ein Nebenweg ohne Snapshots/Recent Files).
- **Kontextmenü der Dateiliste**: molden/hess/out werden am Dateinamen erkannt statt am ganzen Pfad; kein `QAction`-Leck mehr je Popup.
- **Englische Oberfläche**: NMR-Dialog und restliche deutsche Meldungen übersetzt.
- **RMSD-MTD**: das tote Feld „Pace (unused)“ entfernt, `rmsd_mtd_pace` wird nicht mehr an curcuma geschrieben (unter `rmsd_mtd_scheme=strided` ignoriert, löste eine Warnung aus).
- Toter Code entfernt: `startNewCalculation`, `saveCurrentEditor`, `reloadCurrentFile`, `zoomToMolecule`.

## September 2026 - Wayland: Panels per Drag zurückdocken

- **Ursache der fehlenden Andock-Markierung**: Qt 6.11 zieht Docks unter Wayland per Plattform-Drag-and-Drop (`xdg_toplevel_drag_v1`) und setzt die Lücke aus DragMove-Events am `QMainWindow`. Diese Events schluckte das innerste Widget mit `acceptDrops` (Viewer-`QQuickWidget`, Text- und Eingabefelder).
- **`MainWindow::forwardDockDragEvent`** (qApp-Filter): leitet Dock-Drags (MIME `application/x-qt-mainwindowdrag-window`) an `QMainWindow::event()` weiter; DragLeave wird verzögert, damit die Lücke beim Überqueren von Widgets nicht flackert.
- **`GroupedDragging` unter Wayland aus**: das Andocken einer schwebenden Tab-Gruppe stürzte in Qt 6.11 ab (`QMainWindowLayout::animationFinished` → `setTabBarShape()` auf einem Nullzeiger).
- Startup-Hinweis in `main.cpp` entfernt; „Re-dock Floating Panels" bleibt als Rückfallweg.

## September 2026 - RMSD-Overlay: Referenz behält ihre Farbe (auch als Primärstruktur)

- **`SceneController::setPrimaryTint`** (`src/scenecontroller.{h,cpp}`): die Primärstruktur konnte bisher keinen Farb-Tint tragen, sie rendert immer über das globale Farbschema — deshalb sah es nach einem Referenzwechsel so aus, als würde die neue Referenz „grau" statt in ihrer vorherigen Overlay-Farbe erscheinen. `schemeColorFor()` wendet jetzt bei gesetztem Tint dasselbe `shiftOverlayColor` an, das auch Overlays einfärbt, auf dieselbe Basisfarbe — eine Struktur zeigt also dieselbe Farbe, ob sie gerade Referenz oder Overlay ist.
- **`MoleculeViewer::setOverlayWorkspace`/`setPrimaryTint`** und **`RMSDWidget::overlayWorkspaceChanged`/`referenceTintChanged`** um den Referenz-Tint erweitert; Farb-Swatch in der Tabelle jetzt auch für die Referenzzeile aktiv und klickbar (vorher deaktiviert mit „uses the global colour scheme").

## September 2026 - RMSD-Overlay: Referenz ohne Farbe zeigte falsche Farbe nach Wechsel

- **Fix** (`src/rmsdwidget.cpp`, `addStructure`/`setReferenceStructure`): eine Struktur, die als *erste* ins RMSD-Workspace kommt oder über „Use current view as reference" neu angelegt wird, ist von Anfang an Referenz und bekam nie einen `tint` zugewiesen (`s.tint` blieb default-konstruiertes, ungültiges `QColor()`). Solange sie Referenz blieb, fiel das nicht auf — die Referenz rendert ohnehin über das globale Farbschema. Sobald aber eine andere Struktur zur Referenz wurde, tauchte genau diese als Overlay auf und zeigte die nie gesetzte Farbe: wirkte wie ein Farbwechsel beim Referenzwechsel, war aber eine fehlende Zuweisung. Jede Struktur bekommt jetzt beim Anlegen sofort eine Farbe aus derselben Palette wie jedes andere Overlay; geändert wird sie danach nur noch über den Farb-Dialog, nie automatisch durch einen Referenzwechsel.

## September 2026 - Performance-Roadmap P0/P3: GUI-Timing + Frame-Koaleszenz; Redock-Menü

- **P0 GUI-Timing** (`MoleculeViewer::setPerformanceAnalysis`, `src/view.cpp`): misst pro Live-Frame Bindungserkennung und Rest-Rebuild getrennt, `qDebug`-Summary alle `performanceInterval` Frames (Avg/Max µs + GUI-FPS) — dieselbe „Performance"-Checkbox im Simulation-Dock, die bisher nur die Worker-Step-Zeit zeigte.
- **P3 Frame-Koaleszenz** (`MoleculeViewer::onWorkerFrameReady`): `SimulationWorker::frameReady` läuft jetzt über einen Debounce (`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`) statt direkt auf `updateSimulationFrame` — bei einem GUI-gebundenen Burst wird immer nur der jüngste Frame verarbeitet, ältere werden verworfen statt nacheinander abgearbeitet zu werden. Kein Timer, keine Änderung an Charts/Atom-Tabelle (bekommen weiterhin jeden Frame).
- **Redock-Menü** (View ▸ „Re-dock Floating Panels", `DockManager::redockFloating`): docked jedes schwebende Panel über `QMainWindow::addDockWidget()` an seine Vorgabeposition zurück — umgeht die Wayland-Einschränkung beim Zurückziehen per Drag, da der Aufruf keine Bildschirmkoordinaten braucht.

## September 2026 - Wayland: abgedockte Panels lassen sich nicht zurückdocken (überholt)

- **Überholt** durch „Wayland: Panels per Drag zurückdocken" (oben): die Ursache war nicht fehlende Bildschirmkoordinaten, und es gibt eine Lösung im Code.

- **Kein Code-Fehler, Wayland-Plattformgrenze**: Qts eingebautes Andock-Hit-Testing für ein bereits schwebendes `QDockWidget` braucht beim Ziehen absolute Bildschirmkoordinaten, um zu erkennen, dass die Titelleiste über einem Dock-Bereich des Hauptfensters ist. Natives Wayland gibt Clients diese Koordinaten grundsätzlich nicht heraus. Bestätigt auf der eigenen Wayland-Session des Projekts: Abdocken funktioniert, Zurückdocken nicht; `QT_QPA_PLATFORM=xcb` (XWayland) behebt es sofort. Derselbe Mechanismus wie die bereits dokumentierte Wayland-Einschränkung bei `QCursor::setPos` (Cursor-Lock).
- **Kein automatischer Plattform-Zwang**: `main.cpp` erzwingt `xcb` nicht selbsttätig — das würde den Start auf einem Compositor ohne XWayland verhindern. Stattdessen ein einmaliger `qInfo()`-Hinweis beim Start, wenn `QGuiApplication::platformName() == "wayland"`.

## September 2026 - Dock-Widgets: Überlagerung und verschluckte Klicks

- **3D-Viewer als `QQuickWidget`** (`src/view.cpp`, `setupViewer`): der bisherige `QQuickView` im `createWindowContainer()` war ein natives Fenster, das Qt über alle Geschwister-Widgets stapelt — es überdeckte Dock-Panels/Tab-Leisten beim Resize, bei Dock-Animationen und unter Wayland und nahm die Mausklicks entgegen, die den Docks galten. `QQuickWidget` rendert per `QQuickRenderControl` in eine Textur und wird wie ein normales Widget komponiert (Render-Loop auf dem GUI-Thread, Screenshot über `grabFramebuffer()`). Escape-Hatch `QURCUMA_NATIVE_VIEWPORT=1` für die alte Route; CMake-Komponente `QuickWidgets`.
- **DockManager: Sichtbarkeit pro Dock statt pro Tab-Gruppe** (`src/docks/dockmanager.cpp`): `tabifiedDockWidgets()`-Gruppen-Toggling versteckte im Explore-Modus und in den Presets Visualization/Editing das Display-Dock mit dem Simulation-Dock (gleiche Tab-Gruppe) und holte die standardmäßig verborgenen Docks Interactions/Images bei jedem Preset-/Moduswechsel in die Tab-Leiste. Jetzt: erst verstecken, dann zeigen, dann das Preset-Frontdock `raise()`; Interactions/Images bleiben unangetastet.

## September 2026 - RMSD, Gyrationsradius und wählbare Histogramm-Quellen

- **Geometrie liegt jetzt in curcuma** (Operator-Regel: so tief wie möglich verankern): `GeometryTools::Angle/Dihedral/GyrationRadius` sind dort ergänzt, `Distance` und `RMSDFunctions` gab es schon. `src/measurements.{h,cpp}` ist nur noch der Qt-Adapter (QVector3D → `Position`/`Geometry`, Beschriftungen, Farben) — keine eigene Geometrie mehr. Beim Umstellen fiel ein echter Fehler in curcumas `BestFitRotation` auf: der Guard prüfte `det(Cov)`, was bei **jeder planaren Struktur** null ist, obwohl die Kabsch-Rotation dort wohldefiniert ist — planare Moleküle blieben unausgerichtet (gemessen: RMSD √2 statt 0 für ein um 90° gedrehtes Quadrat). Behoben und in curcuma committet.
- **Zwei neue verfolgbare Größen**: RMSD zum ersten Frame des Laufs (Schwerpunkte entfernt, dann Kabsch — Drift und Taumeln zählen also nicht als Strukturänderung) und Gyrationsradius. Beide brauchen keine Atomauswahl und haben einen eigenen Auswahlknopf neben "Add from selection".
- **Histogramm über beliebige Größen**: eine Auswahlliste im Histogramm-Reiter bestimmt, was gebinnt wird — E_pot, E_kin, E_tot, T und jede verfolgte Messgröße, mehrere gleichzeitig. Vorher gab es Histogramme nur für die Messgrößen und ohne Wahlmöglichkeit.
- **Messung und Plot zusammen**: die Tabelle der verfolgten internen Koordinaten sitzt jetzt im Reiter "Internal coordinates" bei ihrem Plot, statt unter allen Reitern zu hängen — Atome picken und die Kurve lesen ist eine Tätigkeit, und auf dem Temperatur- oder Energiereiter war die Tabelle bedeutungslos.

## September 2026 - Verfolgte Messungen in den Charts

- **`src/measurements.{h,cpp}`** (GUI-frei, `test_measurements` mit 29 Prüfungen): Abstand, Winkel und vorzeichenbehafteter Diederwinkel, eine `Tracked`-Beschreibung (2/3/4 Atome bestimmen die Art) und Histogramm-Binning. Die Mathematik lag bisher als lokale Lambdas in `MoleculeViewer::updateMeasurement()`; der Viewer-HUD nutzt jetzt dieselben Funktionen wie die Plots, damit abgelesene und geplottete Zahlen nicht auseinanderlaufen können. Die Vorzeichenkonvention des Diederwinkels ist im Test von Hand hergeleitet und damit festgenagelt.
- **Chart-Widget erweitert**: eigener Plot für beliebig viele verfolgte Messungen, **jedes Diagramm in einem eigenen Reiter** (Temperature / Energy / Measurements / Histogram) statt gestapelt — im Dock teilen sich sonst alle dieselbe knappe Höhe; Diagrammtitel entfallen, weil der Reiter sie schon nennt. Geteilte Steuerleiste mit Ansichtsmodus (gleitendes Fenster über N ps / akkumuliert), Fensterlänge, Normierungs-Umschalter, Binzahl und CSV-Export der Rohwerte. Messwerte liegen als Vektoren, die Serien werden daraus neu aufgebaut — Fenster, Normierung und Binzahl wirken dadurch rückwirkend auf alles bereits Aufgezeichnete. Zeitachse in ps (der Zeitschritt kommt aus der Config, das Frame trägt nur die Schrittzahl). Das Histogramm ist bewusst eine Stufenlinie: CuteCharts `ChartView` würde bei einer Balkenserie an deren `QBarCategoryAxis` in einen Null-Zeiger laufen.
- **Charts als Dock** (`src/docks/chartdock.*`, `DockConfig::ChartDock*`): aus dem modelosen Dialog wurde ein reguläres Dock unten neben dem Output-Log (auch rechts andockbar), anfangs ausgeblendet. Sichtbarkeit über View ▸ Dock Panels ▸ Charts und Molecule ▸ Simulation Charts (Strg+Umschalt+C) — beides dieselbe `toggleViewAction()` des Docks, also ohne eigenen Sync-Code und korrekt auch beim Schließen über die Titelleiste. Die Charts nehmen damit an den Layout-Presets und am gespeicherten Dock-Zustand teil; Mindesthöhe 300 px, damit die drei gestapelten Plots im unteren Bereich lesbar bleiben.

## September 2026 - Abbruchgrund und periodischer Container

- **Warum ein Lauf endet** (`src/simulationworker.*`, `src/simulationcontrolwidget.*`, `src/mainwindow.cpp`): `finished()` trägt jetzt Grund und Abbruch-Flag, gespeist aus curcumas neuem `SimpleMD::stopReason()`. Das Dock zeigt "Finished: Simulationszeit erreicht (nach N Schritten)" oder rot "Aborted: …", das neue Signal `runEnded` spiegelt die Zeile ins Output-Dock und in die Statusleiste. Vorher war jedes Ende identisch beschriftet — curcuma meldet Abbrüche erst ab Verbosity 1 (die GUI fährt 0) und das reguläre Zeitende gar nicht, und `m_run_aborted` hatte keinen Getter.
- **Periodischer Container** als dritte Wand-Option (`wallPotential` int statt `wallHarmonic` bool; Lessons lesen den alten Schlüssel weiter): "Periodic (wrap around)" übt keine Kraft aus und setzt ein Molekül, das den Container verlässt, auf der Gegenseite wieder ein — bei Kugel wie Box. Heizt nicht, hält den Inhalt (20 ps, 4,5-Å-Kugel: max |r| 4,91 Å gegen 11,00 harmonic und 8,76 logfermi) und liefert dadurch 601 statt 177 Bindungsereignisse. Der Viewer blendet für pbc die Iso-Potentialschalen und Kraftpfeile aus, weil es kein Potential zu zeichnen gibt. Die Engine-Seite ist bewusst unfertig (kein Minimum-Image, belegtes Ziel wird elastisch reflektiert) und in `external/curcuma/docs/WP-PERIODIC-NONBONDED.md` als Arbeitspaket festgehalten.

## September 2026 - Reaktive Parameter und Szenenfüller

- **Reaktive Parameter in der GUI** (`src/simulationcontrolwidget.*`, `src/simulationworker.*`): Gruppe "Reactive Topology" mit Form-/Break-Faktor, Scan-Intervall, Refraktärscans, Valenzschranke und Exchange-Scans; sichtbar nur bei MD + GFN-FF + react, geschrieben nach `controller["gfnff"]` nur in diesem Modus. Das Break-Minimum folgt dem Form-Wert, weil curcuma bei `break <= form` warnt und **beide** Werte auf die Defaults zurücksetzt — ein still verlorener Nutzerwert wäre schlimmer als eine Spinbox, die den Wert ablehnt. Lesson-Roundtrip in `lesson.cpp`.
- **RATTLE bei react gesperrt**: curcuma lehnt die Kombination seit `reactff2` beim Start ab (Constraints sind beim Init eingefroren); die GUI nimmt die Wahl vorher weg und `buildConfig` erzwingt es zusätzlich für aus Lessons geladene Configs.
- **`row = 2`-Hack ersetzt**: die Sichtbarkeit der Topologie-Zeile hängt jetzt an `QFormLayout::labelForField` statt an einem hartkodierten Zeilenindex.
- **Gezeichnete Topologie als curcuma-Seed**: `atomsToMolecule(atoms, &bonds)` baut im react-Modus die Topologiematrix und übergibt sie über `setTopologyMatrix`; damit startet das Kraftfeld mit genau den Bindungen, die im Builder gezeichnet wurden. Bewusst nur im react-Modus — Forced-Bonds wirken in jedem Topologiemodus und würden sonst GFN-FFs eigene Detektion für geladene Strukturen still ersetzen.
- **Szenenfüller** (`src/scenefiller.{h,cpp}`, `src/dialogs/fillcontainerdialog.*`): "Fill" im Build-Strip und Molecule ▸ Fill Container… packen N zufällig orientierte Kopien von Bibliotheks-Molekülen in Kugel oder Box — gleichverteilte Rotationen (Shoemake), Rejection-Sampling, Mindestabstand zu allem bereits Platzierten, Abbruch mit `placed < requested` statt Endlosschleife, optionaler Seed für reproduzierbare Szenen. Bindungsordnungen bleiben erhalten, alle Kopien in einem `appendMolecule`-Aufruf (ein Undo-Schritt), und die Confinement-Wand wird auf Wunsch auf den Container gesetzt. Test `test_scenefiller` (15 Prüfungen, GUI-frei).

## September 2026 - Reaktives GFN-FF: Topologie und Ereignisse in der GUI

- **Kraftfeld-Bindungen im Frame** (`src/simulationframe.h`, `src/simulationworker.{h,cpp}`): `SimulationFrame` trägt jetzt `bonds` (mit Ordnungen), `topologyVersion` und `events`. `liveGfnff()` bündelt den Zugriff auf die laufende GFNFF-Instanz (CPU/CUDA/ROCm — die GPU-Wrapper haben dafür curcuma-seitig `getGFNFF()` bekommen) und ersetzt den Inline-Cast in `collectLiveNci`. Gelesen wird im Worker-Thread direkt nach `step()`; die Bindungsliste wird nur bei geändertem Rebuild-Zähler kopiert. Nur aktiv bei Methode gfnff + `topology_mode=react`.
- **Viewer zeichnet die gerechnete Topologie** (`src/view.cpp`): trägt ein Frame Kraftfeld-Bindungen, ersetzen sie `m_trajectoryBonds[0]` samt Ordnungen; die eigene Hysterese (1.25/1.45, Ordnung 1) bleibt der Rückfall für alle anderen Läufe. Vorher widersprachen sich gezeichnete und gerechnete Topologie per Konstruktion (Kraftfeld: 1.6/2.6 auf fat-skalierten Radien).
- **Ereignis-Rückmeldung**: Gruppe "Reaction Events" im Simulation-Dock (Step/t/Ereignis/ΔE, Clear, "Snapshot on event"), Spiegel ins Output-Dock über das neue `reactionEvent`-Signal, 500-ms-Bernstein-Flash der beteiligten Atome (`SceneController::setFlashAtoms`) und eine Ereignis-Scatter-Serie auf E_pot im Energie-Chart. Bisher existierten die Ereignisse nur als curcuma-Logzeilen, die die GUI wegen `verbosity = 0` nie sah.
- **curcuma-Seite** (external/curcuma, Branch `reactff2`): öffentliche Topologie-/Ereignis-API (`reactiveBonds`, `reactiveBondOrders`, `consumeReactEvents`, `reactiveRebuildCount`, `topologyMode`), sieben Buchhaltungsfixes im Scan, `rattle`+react wird abgelehnt, zwei neue Tests — Details in `external/curcuma/AIChangelog.md`.
