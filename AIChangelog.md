# AIChangelog - Qurcuma Improvements

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

## September 2026 - Makro-Aufzeichnung: Werkzeugaufrufe werden zum Skript

- **`ToolDispatcher::callRecorded`** (neues Signal, nach jedem Aufruf samt Ausgang) und **`callOrigin`/`setCallOrigin`** (thread-lokal): die Herkunft reist **mit dem Signal**, weil eine queued Zustellung auf einem anderen Thread läuft und ein Nachlesen dort die Herkunft des Empfängers wäre. Die Audit-Zeile trägt sie als `[assistant]` bzw. `[script]`.
- **Record-Knopf im Skript-Dock**: was der Bediener selbst über Palette oder Menüs an Werkzeugen startet, wird zu `var rN = tool("name", {...});` und landet beim Anhalten im Editor. Aufrufe des Assistenten (Herkunft `assistant`) und die Wiedergabe eines Skripts selbst (`script`) bleiben draußen, sonst zeichnete eine Aufzeichnung ihre eigene Wiedergabe auf.
- `test_tooldispatcher` prüft Signal, Argumente, beide Ausgänge und die Herkunft, dazu die Herkunft in der Audit-Zeile.
- **Grenze, die bleibt**: aufgezeichnet wird nur, was durch den Dispatcher läuft. Menü-Aktionen ohne Werkzeug (Datei öffnen, Layout wechseln) erscheinen in einem Makro nicht.

## September 2026 - Skript-Dock: dasselbe Rechnen für den Bediener

- **`src/docks/scriptdock.{h,cpp}`** (DockConfig `ScriptDock`, unten neben dem Output, anfangs versteckt, **nicht** `USE_LLM`-gated): Editor, Run (Strg+Return), Stop, vier Beispiele (Energiedifferenz in kJ/mol, Mittelwert und Streuung, Gerade durch Messpunkte, Zugriff auf die geladene Struktur über ein Werkzeug), Ausgabebereich, Editorinhalt unter `script/editor` gespeichert. Der Lauf läuft auf einem eigenen Thread, Stop greift über `setStopPoll` in die laufende Engine.
- **Mit Brücke, aber nicht ohne Politik**: das Dock gibt dem Interpreter eine Werkzeug-Brücke (der Bediener hat Run gedrückt), und jeder Aufruf geht durch `ToolDispatcher` **und** `MainWindow::approveToolCall`. Dafür ist die Politik aus dem `USE_LLM`-Block herausgezogen worden (samt `m_autonomy` und `m_toolsAllowedForSession`), sonst wäre sie im `USE_LLM=OFF`-Build nicht vorhanden — der Assistent und das Skript-Dock werden jetzt von einer Leiter benotet und teilen sich die Freigaben der Sitzung.
- **Vorschau vor dem Lauf** (`script::namedToolsIn`, getestet): nennt ein Skript Werkzeuge, die mehr als Lesen und Anzeigen tun, listet das Dock sie vorher mit ihrem Effekt auf. Nur ein vollständiges String-Literal zählt; bei `tool("mea" + "sure", {})` wird nichts gelistet, weil der erste Bruchteil der falsche Name wäre.
- **`scriptRan`** spiegelt jede Ausführung in die Statusleiste, damit ein Lauf auch dann auffällt, wenn das Dock hinter einem anderen Reiter liegt.

## September 2026 - Das Werkzeug `calculate` und die Prompt-Zeile

- **`calculate`** (`src/script/tools_script.cpp`, registriert in `MainWindow::createDockWidgets`): das Modell übergibt ein kurzes JavaScript und bekommt das Ergebnis des letzten Ausdrucks zurück, benannte Ergebnisse über ein Objekt, `print(...)` als beschriftete Zeile. Effekt `Read` (es rechnet auf Zahlen, die es bekommt), Affinität `Any`, **immer verfügbar** — ein Taschenrechner muss ohne geladene Struktur arbeiten. Katalogkosten gemessen **1296 Byte** gegen die Grenze 1500 in `test_scripttool`.
- **Kein Host im Modellpfad**: `calculate` wird ohne `ScriptHost` registriert, also kann ein Skript aus dem Assistenten keine Werkzeuge aufrufen. `test_scripttool` nagelt das fest („line 1: this script is a calculation only"), denn ein Interpreter mit Brücke wäre ein Weg um die Freigabepolitik herum.
- **Der Stop-Knopf erreicht ein laufendes Skript**: `ScriptInterpreter::setStopPoll` fragt die Abbruchflagge des `ToolDispatcher` im Wächter-Thread ab. Gemessen: `while (true) {}` endet nach **21 ms** statt nach der 5-s-Deadline.
- **Prompt-Zeile** in `MainWindow::applySystemPrompt`: Zahlen nicht selbst ausrechnen, sondern `calculate` aufrufen und den zurückgegebenen Wert berichten. Ohne diesen Satz wird ein neu hinzugekommenes Werkzeug erfahrungsgemäß nicht gefunden.
- **Zwei Fehler, die das Messen zutage brachte**: ein von einem Builtin geworfener Fehler meldete die Zeile *im Builtin-Programm* („line 57" für ein einzeiliges Skript); jetzt gewinnt der Stack-Frame des Skripts. Und `throw "boom"` setzt **kein** `isError()` — ohne die Stack-Frames wäre ein geworfener Wert als normales Ergebnis durchgegangen.
- **Umgebungsbefund**: `external/curcuma/CMakeLists.txt:465` schreibt curcumas `version.h` mit dem **qurcuma**-Commit-Hash. Ein Configure nach einem neuen Commit erneuert ihn, und weil ihn viele curcuma-Quellen einbinden, baut danach ganz curcuma neu (gemessen 106 Objekte, rund zehn Minuten in `debug`).

## September 2026 - Skript-Interpreter: rechnen lassen statt selbst rechnen

- **`src/script/`** (neues Ziel `qurcuma_script`, `qurcuma_core` + `Qt6::Qml`, headless): ein kurzes JavaScript, das qurcuma ausführt, damit Zahlen hier gerechnet werden statt im Kopf des Modells — Differenzen, Verhältnisse, Einheiten, Mittelwert, Streuung, Steigung. Ergebnis ist der Wert des letzten Ausdrucks, ein Objekt liefert benannte Mitglieder; `print(...)` füllt die Ausgabeliste. Grenzen: 5 s, 64 Werte je Liste, 32 Ausgabezeilen, 2000 Zeichen für den Wert. Nicht in `qurcuma_core`, weil `libQt6Qml` Network/ICU/libproxy/systemd mitzieht (mit `ldd` gemessen); nicht hinter `USE_LLM`, weil Rechnen keine Endpunkt-Frage ist.
- **Qt 6.11 hat kein `QJSEngine::newFunction`** (weder im installierten Header noch in der Dokumentation). Deshalb stehen die Builtins in **JavaScript** (`scriptbuiltins.cpp`), `print` wird als Array nach dem Lauf zurückgelesen, und die Werkzeug-Brücke ist ein QObject mit einem `Q_INVOKABLE tool(...)` (`scriptbridge.h`) — installiert nur, wenn der Aufrufer eine `ScriptHost`-Brücke übergibt. Der Modellpfad bekommt keine, damit ist ein Skript aus dem Assistenten strukturell ein Taschenrechner.
- **Abbruch ist gemessen, nicht zugesagt** (`test_scriptinterpreter`, 45 Prüfungen): `while (true) {}` endet bei 300 ms Deadline nach 301 ms, ein Stop aus einem zweiten Thread beendet die Schleife und meldet „stopped at the operator's request". Beim Interrupt liefert die Engine nur `Error: Interrupted` **ohne Zeile, Stack oder Frames**; bei einem Skriptfehler dagegen `line 1, column 4: SyntaxError: …`. Deadline und Stop nennen deshalb den Grund, nicht die Stelle.
- **Nachvollziehbarkeit statt Sandkasten**: `Date` und `Math.random` sind ersetzt (gleicher Quelltext, gleiche Eingaben, gleiches Ergebnis), Trigonometrie bleibt in Radiant (`radians`/`degrees` als Helfer). `print` und `script::formatNumber` formatieren mit derselben Regel (zehn signifikante Stellen, ohne Nullen), ein Test hält beide fest.
- **Entscheidungsrecord** in `docs/WP-skript-interpreter.md`: Engine-Vergleich, sechs Entscheidungen mit ihren Gegenargumenten, Arbeitspakete S0–S7 (S0 und S1 erledigt).

## September 2026 - GPU-Auswahl zur Laufzeit, MD-Einstellungen für das LLM

- **Atome festhalten bei der Optimierung** — Simulation-Dock „Hold atoms“ (keine / Schweratome / Wasserstoffe / Auswahlausdruck), fürs LLM `run_simulation hold_atoms`. Umgesetzt als Gradient 0 über curcumas `OptimizerDriver::setConstraints`. Bei ANCOpt reichte das nicht: es schreitet entlang genäherter Normalkoordinaten aus der Modell-Hesse-Matrix, jeder Schritt mischte gehaltene und freie Atome (Schweratome wanderten 0,04 Å); jetzt werden die gehaltenen Freiheitsgrade vor der Diagonalisierung herausprojiziert. Gemessen an Ethanol mit Röntgen-Reiter-H (C–H 0,95, O–H 0,85 Å): Schweratome 0,00000 Å bei ancopt, lbfgs und rfo, danach C–H 1,092, O–H 0,985 Å.
- **Grundwerkzeuge für die LLM-CIF-Analyse** — `describe_cif` liest eine CIF, ohne die Szene anzufassen: Raumgruppe, Z, Z′, Summen- und Moiety-Formel, Disorder-Gruppen mit Assemblies und Besetzung, je Gruppe die Atomzahl von asymmetrischer Einheit und Zelle samt Formel und ein Abgleich mit `_chemical_formula_sum`. `open_structure` ersetzt die Szene (über `MainWindow`, das Unit-Cell-Dock zieht mit), `run_single_point` rechnet mit `cif_path` + `disorder_group` direkt eine Konformation aus der Datei — zwei Aufrufe, eine Differenz. Anlass: das Modell hatte beide Konformationen mühsam über Szenenbearbeitung gerechnet und ΔE = 28,4 kJ/mol aus der Röntgengeometrie mit Reiter-H erhalten; die Werkzeuge schlagen jetzt vor, erst die H mit festgehaltenem Gerüst zu relaxieren. Geprüft über die Registry in der laufenden App an einer synthetischen P-1-Ethanol-CIF mit OH-Fehlordnung.
- **CIF: vollständige Moleküle, Raumgruppe, Formel** — Unit-Cell-Dock „Complete molecules“ (und `complete_molecules` in den Werkzeugen) setzt an den Zellflächen zerschnittene Moleküle wieder zusammen, Schwerpunkt in der Zelle (curcuma `CifBuildOptions::complete_molecules`); im Bild: 4 Bruchstücke → 2 ganze Ethanole. Das Dock zeigt Raumgruppe und Formel (Z).
- **Ellipsoide in jedem Darstellungsmodus, Zelle immer im Bild** — Ellipsoide erscheinen jetzt auch in Wireframe/Sticks (dort war das ganze Atom-Modell per `atomsVisible` ausgeblendet; atomlose Modi bekommen Instanzen der Größe 0 für Atome ohne Ellipsoid, damit Index = Atom bleibt), und solange sie gezeigt werden, sind Bindungen höchstens 0,07 Å dick, sonst verschluckt ein 0,15-Å-Zylinder ein 0,15-Å-Ellipsoid. Das Zellgitter „verschwand“ beim Umschalten nicht wirklich: die Kamera rahmte nach jedem Neuladen nur die Atome ein, die in einer Zelle deren untere Hälfte füllen (NaCl 0–2,8 Å von 5,6 Å) — die Zellecken zählen jetzt zu den Szenengrenzen. Geprüft mit echten Renderings aus einer unsichtbaren `kwin_wayland --virtual`-Sitzung.
- **Schwingungsellipsoide (ORTEP)** — curcuma liest `_atom_site_aniso_U_ij`/`B_ij` und `U_iso`/`B_iso` (B = 8π²U), rechnet U auf kartesische Achsen um (U_cart = M N U N Mᵀ, N = diag(a*, b*, c*)) und dreht es mit jedem Symmetriebild (U* → R U* Rᵀ); getestet an einer 4-zähligen Achse (U₁₁↔U₂₂) und an einer monoklinen Zelle gegen die Lehrbuchformel für U_eq inkl. cos β. qurcuma zieht Hauptachsen und Drehung heraus (`moldata::Ellipsoid`) und zeichnet Atome als gestreckte, gedrehte Kugeln; Größe über die Aufenthaltswahrscheinlichkeit (χ²₃: 50 % ↔ 1,538 σ). Unit-Cell-Dock: „Thermal ellipsoids“ mit Wahrscheinlichkeit (Prozent) und Zahl anisotroper/isotroper Lagen. In der laufenden App nachgemessen: Halbachsen 0,154/0,218/0,266 Å für U = diag(0,01; 0,03; 0,02), längste Achse entlang y, im gedrehten Bild entlang x.
- **Umschalten im Unit-Cell-Dock ging nur einmal, Leeren der Szene hakte** — `loadMoleculeFile(const QString&)` bekam das Member `m_cifPath` als Referenz, und der `moleculeUpdated`-Handler leerte es mitten im Laden: die Datei war schon gelesen, danach aber `m_cifPath` (und `m_currentMoleculeFilePath`) leer, jede weitere Anfrage des Docks lief ins Leere und nach New Scene zeigte das Dock noch die alte CIF. Jetzt Übergabe per Wert. Zweitens markierte jedes `moleculeUpdated` die Struktur als ungespeichert geändert — auch das Laden selbst und die leere Szene —, sodass Laden und New Scene nach Änderungen fragten, die niemand gemacht hatte; ausgenommen sind jetzt Emissionen während eines Ladevorgangs und die leere Szene. Geprüft in der laufenden App über eine `LD_PRELOAD`-Testbibliothek, die das Dock durchklickt (5 ↔ 20, Part 2, alle, asym. Einheit, Leeren — dreimal identisch).
- **CIF: asymmetrische Einheit / volle Zelle und Fehlordnung umschaltbar** — Standard ist jetzt die asymmetrische Einheit wie in Avogadro und Mercury (Koordinaten wie geschrieben, Moleküle nicht zerschnitten), die volle Zelle schaltet man im Unit-Cell-Dock zu. Disorder-Gruppen (SHELX PART) sind wählbar, vorausgewählt die mit der höchsten mittleren Besetzung, „alle“ zeigt die Alternativen überlagert. Anlass: eine Pbca-Struktur mit zwei Konformationen (102 Lagen, Z = 8) kam als 768 Atome an — beide Konformationen übereinander (8 × 102 = 816) minus 48, die die alte Duplikatprüfung verschluckt hatte. Richtig: 70 (asym. Einheit) bzw. 560 (Zelle). Einstellungen gelten pro Datei, eine neue CIF startet mit den Vorgaben (vorher übertrugen sich gespeicherte Wiederholungen still auf die nächste Datei). `merge_structure` nutzt denselben Weg (`cif_content`, `disorder_group`).
- **Einheitszelle im Viewer**: das Parallelepiped der Gittervektoren (auch trikline Zellen), Kanten a/b/c vom Ursprung rot/grün/blau, der Rest grau, bei Wiederholungen zusätzlich der dünnere Umriss des ganzen Blocks. Dreht mit dem Kristall, erscheint im Bildexport, abschaltbar im Unit-Cell-Dock. Der Ursprung folgt der Zentrierung beim Laden (geprüft: 2×2×1-NaCl → Ursprung (−4,23 | −4,23 | −1,41) = −Schwerpunkt).
- **Zellwiederholungen beim Laden einer CIF** — Dock „Unit Cell“ (`src/docks/celldock.*`, rechts mit Display gruppiert, erscheint beim Laden einer CIF): Zelle (a/b/c, α/β/γ, Volumen), Lagen, Symmetrieoperationen, Atome pro Zelle, Reader-Hinweise und Spinboxen a/b/c mit Vorschau der Atomzahl. **Apply** liest die Datei neu — die Superzelle entsteht in curcuma beim Einlesen (`MoleculeFileLoader::load(path, na, nb, nc)`), weil die Atome im Viewer keine Zelle tragen. Die Werte bleiben gespeichert (`structure/cifRepeats`). Das Dock lässt die Datei los (Platzhalter), sobald die Szene nicht mehr die gelesene CIF ist (Atomzahl weicht ab: neue Szene, Lesson-Struktur, Snapshot, Atome gelöscht). Geprüft mit NaCl: 1×1×1 → 8, 2×2×1 → 32, 3×3×3 → 216 Atome.
- **CIF-Dateien öffnen sich per Klick im Datei-Browser**: der Klick lud nur xyz/vtf, obwohl `MoleculeFileLoader` auch pdb/mol2/cif liest; `loadMoleculeFile` hielt pdb/mol2 zusätzlich mit „coming soon“ auf. Jetzt eine Liste, `MoleculeFileLoader::isSupported()`, genutzt von Klick, Lesson-Import und Kontextmenü (cif teilt das vtf-Menü); `RMSDWidget::loadStructureFile` liest über den Loader statt über eine eigene Parserleiter ohne cif. Lesefehler erscheinen in der Statusleiste.
- **Zahlen unter deutscher Locale falsch gelesen**: `QApplication` übernimmt unter Unix die System-Locale in die C-Laufzeit, und curcuma liest Zahlen mit `std::stod` — unter de_DE wurde „0.5957“ zu 0, eine CIF kam mit abgeschnittenen Koordinaten an (Cl auf Na, Zelle 5 statt 5,64 Å). `main.cpp` setzt direkt nach der `QApplication` `setlocale(LC_NUMERIC, "C")` (Qts eigene Empfehlung); betraf jede Zahlenumwandlung von curcuma in qurcuma, die GUI-Formatierung (`QLocale`) bleibt.
- **`select_atoms` lief bei großen Szenen in den 15-s-Timeout** — Ursache in curcuma: `Molecule::addPair()` prüfte bei jedem neuen Atom *alle* Paare auf Deckungsgleichheit, der atomweise Aufbau eines Moleküls war O(N³). Gemessen 3400 Atome: 16,4 s → 14 ms. Betraf jede Umwandlung `atomsToMolecule` (Auswahlausdrücke, Fragmente, Messungen, Simulationsstart). Behoben in `external/curcuma/src/core/molecule.cpp` (curcuma `ca71d8c0`).
- **`fill_container` mit `keep_container`**: packt in den gesetzten Container, der seine Größe behält — vorher legte jeder Aufruf eine neue Box um alles Vorhandene plus 6 Å, die Dichte blieb gleich. Die Antwort nennt Volumen und Dichte der aktuellen Szene (curcumas `Molecule::Density`).
- **`watch_simulation` misst ohne laufende Simulation die aktuelle Szene**: vorher den letzten Frame des letzten Laufs und dessen Containervolumen — nach Füllen oder Löschen blieb die Dichte deshalb auf dem alten Wert (0,818 g/cm³ nach 200 zusätzlichen Wassern). Szene und Dock werden vor dem Cache-Lock im GUI-Thread gelesen; das Volumen kommt aus `SimulationConfig::containerVolume()`, das jetzt auch das Dock nutzt.
- **`clear_scene`** leert die Szene samt Container-Wand in einem Aufruf; `delete_atoms` nimmt auch einen Auswahlausdruck und nennt die verbleibende Atomzahl. `get_structure_summary` griff bei leerer Szene auf `atoms.first()` zu. Die Atomtabelle wählt große Selektionen in einem `select()` über zusammenhängende Bereiche statt zeilenweise (quadratisch, 0,26 s bei 2100 Zeilen).
- **GPU-Combo prüft die Plugins zur Laufzeit**: seit curcuma die GPU-Backends als dlopen-Plugins lädt, ist `USE_CUDA` außerhalb des Plugin-Targets nicht mehr definiert — die `#if`-Blöcke im Simulation-Dock waren immer falsch, CUDA erschien nie. Jetzt `gpu_plugin::available()` je Backend. Zusätzlich `ENABLE_EXPORTS ON` auf dem qurcuma-Target: das Plugin löst `NativeXtbMethod`, `GFNFF`, `CurcumaLogger` … gegen die Programmdatei auf und scheiterte ohne `-rdynamic` an `undefined symbol: _ZTV15NativeXtbMethod`.
- **`run_simulation` kennt RATTLE, Wasserstoffmasse, GFN-FF-Topologie und GPU** (`rattle` off/all/h_only, `rattle_angles`, `hydrogen_mass`, `topology`, `gpu`); vorher blieben sie auf dem Stand des Docks. RATTLE mit `topology: react` wird mit einer Fehlermeldung abgelehnt statt still verworfen; die Antwort meldet die tatsächlich wirksamen Werte zurück.

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

## August 2026 - Reaktives GFN-FF: Topologiemodus "react"

- **Topologie-Combo erweitert** (`src/simulationcontrolwidget.cpp`): dritter Eintrag "Reactive (bonds form and break)" (userData `react`) neben Default (adaptive, `auto`) und Constant; Tooltip beschreibt den Modus sachlich (Bindungen werden während der MD neu erkannt, Bonded-Terme bei Änderung neu aufgebaut, NVT-only). Der String fließt unverändert über `SimulationConfig::topologyMode` und `buildMdController` an curcuma; Lesson-Roundtrip generisch, keine weiteren Änderungen nötig.
- **curcuma-Seite** (external/curcuma, Branch `reactff`): ereignisgesteuerter Hysterese-Scan (Bildung optimistisch 1.6, Erhalt konservativ 2.6), vollständige Regeneration aller Bonded-Terme + Repulsions-Partition, dE_jump-Protokoll, GPU/ROCm-Workspace-Rekonstruktion — Details in `external/curcuma/docs/GFNFF_REACT_TOPOLOGY.md`.

## August 2026 - NCI-Overlay: nichtkovalente Wechselwirkungen anzeigen

- **Geometrische Erkennung** (`src/ncianalysis.{h,cpp}`, `namespace nci`, freie Funktionen): Wasserstoffbrücken (D-H...A, D/A aus N,O,F,S bzw. N,O,F,S,Cl,Br,I; 2.50 Å / 130° nach Jeffrey + IUPAC), Halogenbrücken (C-X...A, X aus Cl,Br,I,At — F ohne σ-Loch ausgeschlossen; 0.95·Σr_vdW / 150°), π-Stacking (planare 5-/6-Ringe, Zentroidabstand ≤ 5.5 Å, parallel ≤ 30° mit Versatz ≤ 2.0 Å oder T-förmig ≥ 60°) und generische vdW-Nahkontakte (0.90·Σr_vdW, Vorgabe aus). 1-2/1-3-Ausschluss über `forceinjector::buildAdjacency`; Ringerkennung über curcumas `Topology::FindRings` (nur bei Topologieänderung, Cache im Viewer).
- **Radien aus curcuma** (`Elements::VanDerWaalsRadius`, Cramer/Truhlar 2009) statt `elem::vdwRadius()` — letzteres ist ein Zeichenradius (H = 0.5 Å) über 16 Elemente und wäre als Σr_vdW-Kriterium um Faktor zwei falsch.
- **3D-Overlay**: neue `BondInstancing`-Ebene in `SceneController` (`setNciContacts`/`rebuildNci`), gestrichelte Linien aus kurzen `#Cylinder`-Segmenten (Qt Quick 3D hat kein Dash-Primitive), farbcodiert pro Typ, Stärke doppelt kodiert über Linienradius und Alpha; Endpunkte auf die Kugeloberflächen gekürzt, damit es auch in Space-Filling stimmt. Model unter `moleculeRoot` (intrinsische Koordinaten → dreht mit). Zweite `QVariantList nciLabels` nutzt das vorhandene projizierende `Text`-Delegate für die Abstandsbeschriftung.
- **NCI-Dock „Interactions"** (`src/docks/ncidock.*`, `src/nciwidget.*`, rechts, mit Display tabifiziert, anfangs versteckt): Kontakttabelle Typ/Atome/d/Winkel/Score/E/Notiz, Zeilenklick selektiert die Atome im Viewer, Doppelklick zoomt darauf, „Copy table" als TSV.
- **GFN-FF-Quelle** (`src/ncianalysisworker.*`, eigener Thread): `EnergyCalculator` → `GFNFFComputationalMethod::getGFNFF()` → `generateGFNFFParameterSet()`; HB/XB-Terme, optional Coulomb- und D4-Dispersionspaare mit **echter Paarenergie** (Formeln spiegeln `ff_workspace_gfnff.cpp`, Bohr → kJ/mol). GFN-FF liefert eine Kandidaten-Enumeration (>5000 Tripel bei 114 Atomen) — sie wird mit demselben Abstands-/Winkelgatter auf die im Frame tatsächlich eingegangenen Terme reduziert. Pro-Kontakt-Energien für HB/XB gibt es in GFN-FF nicht; angezeigt werden Geometrie, Score und `case_type`, die Systemsummen E_HB/E_XB stehen in der Kopfzeile aus `getEnergyDecomposition()`.
- **Live während MD** (Opt-in, „Live from GFN-FF during MD"): `SimulationWorker` setzt `hb_update_force_every=1` und liest die HB/XB-Listen des laufenden Kraftfelds pro Schritt in ein neues Feld von `SimulationFrame`. Ohne das Opt-in ändert sich am MD-Pfad nichts.
- **Ladungen**: `EnergyCalculator::Charges()` (EEQ bei GFN-FF, Mulliken bei GFN2) → `MoleculeViewer::setAtomCharges()` speist das vorhandene „By Charge"-Farbschema.
- **Bedienung**: Display ▸ Tools ▸ „Non-covalent interactions" — Quellen-Combo (Aus/Geometrie/GFN-FF/GFN2), Typfilter H/X/π/vdW/q/disp, H...A-Abstand und D-H...A-Winkel, Beschriftung, Live-MD. Alles in `DisplaySettings` persistiert und in den View-Presets mitgeführt.
- **Deckelung sichtbar**: Kontaktlisten werden pro Typ und global begrenzt; was wegfällt, steht in der Kopfzeile statt still zu verschwinden.
- **Fragment-Einfärbung für Wirt-Gast-Systeme** (Display ▸ Style ▸ „Fragments (host-guest)"): Fragmente = Zusammenhangskomponenten des Bindungsgraphen, **nach Atomzahl absteigend** sortiert. Das größte bleibt unverändert (im Komplex der Wirt), alle anderen werden per Hue-Rotation zu einem eigenen Farbton verschoben — Stärke über Schieberegler (Vorgabe 60 %), Farbton pro Fragment wählbar, „Auto" setzt zurück. `tintFragmentColor()` verschiebt nur den Farbton und hebt bei Grautönen die Sättigung an, ohne abzudunkeln: Sauerstoff bleibt im getönten Fragment als Sauerstoff erkennbar. Wirkt über `schemeColorFor()` auf Atome **und** Bindungshälften. Das Auswahlfeld zeigt Summenformel (Hill) und Atomzahl je Fragment; die Gruppe blendet sich bei einkomponentigen Strukturen aus. Zerlegung wird faul neu berechnet und bei jeder Struktur-/Bindungsänderung verworfen (auch beim Bindungsbruch in laufender MD).
- **Fragmentweise Radienskalierung**: Größe 20-200 % pro Fragment, wirkt auch auf das Referenzfragment, damit man den Wirt schrumpfen und in seinen Hohlraum sehen kann. Trifft Atomradien **und** Bindungsdicke (eine Bindung liegt immer innerhalb eines Fragments, die Zuordnung ist also eindeutig) sowie die Endpunktkürzung der NCI-Striche.
- **Ein Bedienmodell für die Fragmentgruppe**: die Combobox wählt das Fragment, ein Kasten darunter trägt dessen Namen im Titel und enthält Farbe, Tönungsstärke und Größe - alle drei ausschließlich für dieses Fragment. Neben dem Farbfeld ein eigener Zurücksetzer auf den automatischen Farbton (ein gewählter Farbton lässt sich über den Farbdialog nicht wieder abwählen, und der Sammelreset würde auch Stärke und Größe verwerfen); er ist nur aktiv, wenn dieses Fragment überhaupt einen eigenen Farbton trägt. Dazu ein Knopf, der Stärke und Größe auf alle Nicht-Referenzfragmente überträgt, und einer, der alle Überschreibungen zurücknimmt. Vorher standen zwei global wirkende Regler über einer Combobox, die zusätzlich eine eigene Skalen-Spinbox hatte - zwei Wege zur selben Größe ohne sichtbare Zuständigkeit. Test: `test_fragments`.
- **Frei wählbare Farben**: pro NCI-Typ (Auswahlfeld mit Farbfeld im Display-Panel; die elektrostatische Klasse ist nach Vorzeichen in „attractive"/„repulsive" getrennt, damit die Information beim Überschreiben nicht verloren geht) und pro **CG-Bead-Typ**. Die Bead-Liste wird aus der geladenen Struktur gebaut — Typen mit Bead-Zahl im Combo, Farbkachel je Eintrag, „Auto" setzt zurück; die Gruppe blendet sich bei All-Atom-Strukturen aus. Beide Abbildungen sind inhaltsbasiert (Typ-Label bzw. Wechselwirkungsklasse) und liegen deshalb in eigenen QSettings-Gruppen (`visualization/beadColors`, `visualization/nciColors`) statt in `VisualizationSettings`. Kontakttabelle und Bildexport (`cloneStateFrom`) folgen denselben Farben.
- **Tests**: `test_nci` (Kriterien an handgebauten Geometrien: Wasserdimer 1.95 Å/175°, 120°-Ablehnung, 1-3-Ausschluss am Methan, C-Cl...N vs. C-F...N, paralleles Benzol-Stacking) und `test_nci_gfnff` (curcuma-Pfad auf `conf_28.xyz`: Ladungssumme, Gatter, Deckel, Ringerkennung, Index-Konvention beider Quellen).


## Juli 2026 - Arbeitsverzeichnis bleibt beim Öffnen einer Struktur stabil

- **`loadMoleculeFile()` wechselt das Arbeitsverzeichnis nicht mehr**: Das frühere „Open file follows its own directory"-Verhalten (Auto-Wechsel ins Elternverzeichnis der geladenen Datei) ist entfernt. Struktur-Laden ist eine reine Viewer-Operation; das Arbeitsverzeichnis ist ein stabiler, bewusst gesetzter Anker. Gilt für alle Ladepfade (Datei-Browser-Klick, File▸Open, Drag&Drop, Recent Files, Remote, CLI `qurcuma <file>`). Wechsel nur noch explizit: Choose Directory, „Set as Working Directory", Breadcrumb, Recent-Dirs, Workspace-Load, CLI `qurcuma <dir>`.

## Juli 2026 - Batch-Randbeschnitt exportierter Bilder (Image-Gallery-Dock)

- **Image-Gallery-Dock** (unten, ausblendbar, erscheint automatisch beim ersten Bildexport): Thumbnail-Raster der in der Session exportierten Bilder; Checkbox „Show all images in folder" schaltet auf alle `*.png` im Arbeitsverzeichnis um.
- **Gemeinsamer Randbeschnitt** (FlipBooQ-Vorbild, „Daumenkino"): alle Frames werden auf eine gemeinsame Leinwand (max. Quellbreite × -höhe) zentriert, der Rand über die Eckpixel-Hintergrundfarbe erkannt und die Inhalts-Rechtecke auf der Leinwand vereinigt (`united`) → **ein** Crop-Rechteck an identischer Position. Ergebnis: alle Ausgaben haben dieselbe X×Y, kein Molekül wird beschnitten, die Bewegung bleibt registriert, minimaler gemeinsamer Rand — auch bei unterschiedlich großen Quellbildern. Toleranz-Slider (0–32).
- **Metadaten-erhaltender Export**: `imagecrop::saveResized` arbeitet mit `QImage` (nicht `QPixmap`), überträgt alle PNG-Text-Chunks der Quelle in die zugeschnittene Kopie und ergänzt Crop-Provenienz (`ResizeSourceSize/CropRect/Tolerance/Background/BatchTimestamp/Software`); Ausgabe als `<name>.resized.png` (nicht-destruktiv).
- **Schnell-Export „Photo"-Button** in der Viewer-Leiste (neben Measure/Edit): dialogfreier Export (`MoleculeViewer::quickExportImage` — 2× Viewport, SSAA, Metadaten) mit Auto-Dateiname `<stem>_<timestamp>.png` ins Arbeitsverzeichnis; landet direkt in der Galerie. Daneben eine **Transparent-Checkbox** + **Hintergrund-Farbpreset-Combo** (Scene/White/Black/Grautöne, via `exportImage`-`background=3`+`QColor`). Metadaten-Aufbau in `buildImageMetadata` extrahiert (geteilt mit dem Export-Dialog).
- **Galerie-Kontextmenü + Bild-Viewer**: Rechtsklick/Doppelklick auf ein Thumbnail → Bild-Viewer (Fit-to-Window + Zoom-Slider 10–400 %, Tabelle aller eingebetteten PNG-Text-Chunks), „Remove from gallery", „Delete file from disk…" (mit Bestätigung).
- **Crop-Vorschau + Quell-Filter**: nach „Analyze borders" wird das gemeinsame Crop-Rechteck (gestrichelt rot) in die Thumbnails eingezeichnet. Quell-Combo „Show:" wählt Session / Ordner: alle PNG / nur `*.resized.png` / nur Originale — die exportierten Bilder sind so filterbar sichtbar.
- **Neu**: `src/imagecrop.*` (reine, testbare Analyse), `src/docks/imagegallerydock.*` (Dock); Signal `MoleculeViewer::imageExported` verdrahtet den Export mit dem Dock.

## Juli 2026 - Reproduzierbare Metadaten in exportierten Abbildungen

- **Operator-Metadaten** (Settings ▸ „Operator Metadata…"): Name, ORCID, Institution, Lizenz einmal konfigurierbar; gespeichert unter `operator/` in QSettings. Werden als Default-Autorenschaft für Bildexport (und künftig Lessons) verwendet.
- **PNG-Text-Chunks** im Bildexport (`Molecule ▸ Export Image…`): `QImage::setText` schreibt Software/Quranuma-Version, Export-Zeitstempel, Quelldatei, Kamera-Parameter (Rotation als Quaternion, Distanz, Pan, ZoomMode/ZoomFactor), Display-Einstellungen (Modus, Farbschema, Größen, Transparenz, Hintergrund, Effekte), Operator (Name/ORCID/Institution/Lizenz) und optional den angewandten View-Preset-Namen.
- **Export-Dialog erweitert**: Checkbox „Embed metadata (PNG)" (default an) + Combo „View preset (optional)" — wendet das Preset vor dem Export an und schreibt dessen Namen in die Metadaten.
- **Application-Version**: `QURCUMA_VERSION` via CMake compile definition + `QCoreApplication::setApplicationVersion` in `main.cpp` (vorher leer).
- JPEG/TIFF-Export ohne Metadaten (Qt6 kann kein EXIF schreiben); PNG ist der empfohlene Format.

## Juli 2026 - View-Presets für einheitliche Moleküldarstellungen (Redesign)

- **Ein Preset = Kamera + Display** im `DisplayDock` (`ViewPresetWidget`): Save-Dialog mit Namen + Zoom-Modus, Load per Klick/Doppelklick, Delete. Presets unter `viewPresets/` in `QSettings` persistiert; Liste startet leer.
- **Zoom in zwei Modi** (`ZoomMode` in `src/viewpreset.h`): **Absolute** wendet die gespeicherte `cameraDistance` direkt an (identisch nur bei gleich großen Strukturen); **Relative** rekonstruiert `cameraDistance = zoomFactor * sceneExtent` des aktuell geladenen Moleküls → gleiche Bildschirmgröße über unterschiedlich große Moleküle hinweg.
- **`SceneController::setCameraTransform`** (atomar, einzelnes `transformChanged`) + `m_quickView->update()` sorgen für sofort konsistente Kamera-Updates ohne Maus-Bewegung.
- **Quick-Buttons** `Front`/`Top`/`Side` rufen `MoleculeViewer::setCameraOrientation()` auf — nur Rotation, Zoom/Display bleiben unverändert.
- **Display-Sync ohne Dock-Raise**: `MoleculeViewer::viewPresetApplied()` → `DisplayPanel::loadCurrentSettings()` (statt Umweg über `displayOptionsRequested`/`openVisualizationSettings`).
- Checkbox **„Include display settings“** (im Save-Dialog und beim Load) erlaubt reine Kamera-Presets.

## Juli 2026 - ProjectDock Datei-Filter

- **Filter-/Suchpanel im Datei-Browser** (`ProjectDock`): über der Dateiliste eingebettet — Live-Suche nach Dateinamen plus Endungs-Filter als **popup-Menü mit allen Suffixen des aktuellen Verzeichnisses** (Include-Filter: angehakt = sichtbar); „Select all“/„Select none“ + Reset-Button. Knopf zeigt `Extensions (x/y)` an.
- **`DirectoryFilterProxyModel`** (`src/docks/projectdock.cpp`) sitzt zwischen `QFileSystemModel` und `QListView`; `MainWindow` löst View-Indizes über `filePathFromContentIndex()` zurück in Source-Indizes auf. Nur für den lokalen Files-Modus aktiv, Lesson/SFTP bleiben unberührt.
- Filtereinstellungen sind **session-only** (kein `QSettings`-Persistieren); Startverhalten des Browsers bleibt unverändert.

## Juni 2026 - Bild-Export (hi-res)

- **File ▸ Export Image…** (Ctrl+Shift+E): echter Offscreen-Render in beliebiger Auflösung statt des alten Fake-Upscales. Via **`QQuickRenderControl` + `QRhi`** (`Qt6::GuiPrivate`, `<rhi/qrhi.h>`): QML-Szene in eine `QRhiTexture` rendern + zurücklesen (`grabWindow()` auf verstecktem Fenster liefert leer bei threaded render loop). Separate `SceneController` (`cloneStateFrom` — kein Teilen von Szenengraph-Knoten), Wiederverwendung der `QVulkanInstance` der Live-View; OpenGL braucht `mirrored()`. Dialog: Breite/Höhe (default 2× Viewport), Hintergrund (transparent default/weiß/Szene), SSAA-Schalter, Default-Ordner = Workspace. Export-Props `highQualityAA` (SSAA VeryHigh) + `transparentBackground` in `viewer3d.qml` gebunden; transiente Overlays werden nicht mitkopiert → sauberes Bild.

## Juni 2026 - RMSD-Overlay-Workspace

- RMSD/Align komplett auf einen **`QTableWidget`-Workspace** umgestellt: Tabelle aller Strukturen mit Referenz-**Radiobutton** (welche ist Referenz/Primary), Show-Checkbox (einzelnes Ausblenden, auch der Referenz via `setPrimaryVisible`), **plain + permutation RMSD**, Farb-Tint-Swatch und Größen-Spinbox pro Struktur sowie Entfernen. Referenzwechsel richtet alle anderen neu aus.
- **plain RMSD wird jetzt korrekt angezeigt** (`RMSDWidget`): statt des nie gesetzten `RMSDDriver::RMSDRaw()` wird der Kabsch best-fit-RMSD in der ursprünglichen Atomreihenfolge direkt in Qurcuma berechnet; der permutierte RMSD kommt weiterhin aus `RMSDDriver::RMSD()`.
- **Overlay-Geometrie im Reorder-Modus korrigiert** (`RMSDWidget::alignToReference`): das getönte Overlay nutzt jetzt `RMSDDriver::TargetReorderd()` (reordered + Kabsch-aligned, = der angezeigte perm.-RMSD) statt `TargetAligned()` (un-reordered = `rmsd_raw`). Vorher zeigte die `perm.`-Spalte den korrigierten Wert (z. B. 0.16), gezeichnet wurde aber die unkorrigierte Struktur (rmsd_raw 3.75). Fallback auf `TargetAligned()` ohne Reorder.
- **RMSD-Icons ergänzt** (systemunabhängig über `QIcon::fromTheme`): Tab `RMSD / Align` im SimulationDock, Kontextmenü-Eintrag „Overlay onto current (RMSD/Align)…" im Datei-Browser, sowie die Widget-Buttons „Add structure…", „Use current view as reference", „Re-align all" und „Remove".
- Overlays sind eine **Liste** ausgerichteter Strukturen; jede erbt die globalen Display-Styles (Modus/Größe/Bindungen/Transparenz/Farbschema) und hat einen editierbaren Farb-**Tint** (Hue/Sat-Shift über CPK, Element-Identität bleibt) + **Größe**. `SceneController::rebuildOverlays()` (in `rebuildGeometry()` eingehängt) sorgt dafür, dass Display-Änderungen auf die Overlays durchschlagen.
- Entkopplung Viewer↔Widget über `overlayWorkspaceChanged` (Voll-Rebuild, `resetView` nur bei Referenzwechsel → kein Kamerasprung beim Hinzufügen) + günstige Tint/Size/Visibility-Index-Signale; Zeilen-Controls über stabile Struktur-`id` statt Pointer/Index (behebt die früheren Manual-Row-Design-Debts). Kontextmenü „Overlay onto current" + `addStructureFromFile` richten direkt aus und geben Statusbar-Feedback (`structureAligned`).
- **Dubletten-Check** (`isDuplicateName`, case-insensitiv): dieselbe Datei/derselbe Name wird nicht doppelt in den Workspace aufgenommen.
- Reorder-Logik korrigiert: die zwei verwirrenden Checkboxen (Force/Disable reorder) ersetzt durch **eine** „Reorder atoms" (default **aus** — Permutation ist teuer; an → `force_reorder`, sonst würde die Methodenwahl Konformere in gleicher Atomreihenfolge nicht permutieren). Default-Methode = **inertia** (template-free, schnell). Buttons logisch in **eine** Aktionszeile unter der Tabelle konsolidiert; der Reorder-Haken sitzt direkt vor „Re-align all". perm.-RMSD-Spalte nur befüllt, wenn reordert wurde.

## Juni 2026 - Dock-Architektur-Refactor

- **`src/docks/`** zentrales Dock-Verzeichnis: `DockManager` besitzt alle `QDockWidget`-Shells, Platzierung, Layout-Presets und Explore/Compute-Modus; `MainWindow` koordiniert über Signale und holt interne Widgets per Getter aus den Wrappern.
- **Wrapper-Klassen**: `ProjectDock`, `DisplayDock`, `SimulationDock`, `OutputDock`; jede erbt `QDockWidget` und kapselt ihren Inhalt. `EditorsDock` und `AtomsSimulationDock` entfernt.
- **ProjectDock-Segmente**: `NavigationDock` entfernt; Bookmarks, Workspaces und Remote sind jetzt direkte Segmente im `ProjectDock` (neben Files). Slim-Icon-Button für „Choose Working Directory“; Breadcrumb-Bar zeigt immer den aktuellen Pfad. Klick auf Bookmark/Workspace schaltet automatisch zurück auf die Files-Ansicht und wechselt das Arbeitsverzeichnis.
- **Display-Dock**: Rechtes Dock mit segmentiertem Umschalter `[Structure | Atoms]` oben und dem Display-Panel unten. Struktur-Editor und Atom-Tabelle leben hier.
- **Simulation-Dock**: Rechtes Dock mit Tabs `[Simulation | Snapshots | RMSD / Align | Input]`. Es ist mit dem Display-Dock tabifiziert.
- **Trennung**: Structure/Atoms und Display zusammen; Simulation/Snapshots/RMSD/Input zusammen.
- **`dockconfig.h`** hält stabile `objectName`s, Dock-Bereiche, `LayoutPreset` (Visualization/Editing/Calculation/Analysis/Teaching) und `AppMode` (Explore/Compute). Namen dürfen nicht ohne Migrationsplan geändert werden, weil sie in `QSettings` via `saveState()`/`restoreState()` persistiert werden.
- **Presets in `DockManager`**: Lazy-Caching mit `saveState()`/`restoreState()` vermeidet Qt-Drift bei wiederholtem `tabifyDockWidget`/`splitDockWidget`; Tastenkürzel Ctrl+Alt+1..4 bleiben erhalten; Teaching-Layout für Lesson-/Demo-Workflow ergänzt.
- **Explore/Compute-Modus**: `MainWindow::setAppMode` aktualisiert Buttons, persistiert `ui/appMode` und schaltet die Calculation-Toolbar; Sichtbarkeit/Reflow der Docks delegiert an `DockManager::setAppMode`.
- **Tab-Bar-Kollaps-Fix**: `DockManager` schaltet tabifizierte Dock-Gruppen (via `QMainWindow::tabifiedDockWidgets()`) immer gemeinsam ein/aus; das View-Menü verwendet jetzt `QDockWidget::toggleViewAction()` statt direktem `setVisible()`. Damit bleibt die Tab-Bar stabil, auch wenn der Benutzer Docks manuell tabifiziert (z. B. Simulation auf Display zieht).

## Juni 2026 - Struktur-Synchronisation (Viewer ↔ Tabelle ↔ Texteditor)

- **Bidirektionale Sync**: Viewer ist kanonischer Speicher; `moleculeUpdated` spiegelt Geometrieänderungen in Atom-Tabelle + Struktur-Editor (XYZ via `atomsToXyz`). `MainWindow::m_structSyncing` verhindert Feedback-Loops; Text-Spiegel ausgesetzt während MD (`simulationActive()`) und beim Laden (qScopeGuard, erhält den Datei-Text z. B. VTF), Refresh nach `simulationRunningChanged(false)`.
- **Tabelle editierbar**: `AtomTableModel::flags()`/`setData()` für Element + X/Y/Z (validiert), Signal `AtomListPanel::atomEdited` → `MoleculeViewer::setAtomInCurrentFrame` (keepView, kein Kamerasprung; Element-Wechsel = atom-rebuild).
- **Text „Apply → Viewer"**: Button am Struktur-Editor → `xyzToAtoms` → `MoleculeViewer::applyStructureFromAtoms` (Single-Frame, `detectBonds` neu). Auswahl bleibt bidirektional (Phase 2C).

## Juni 2026 - Lehrszenarien (Lessons, OER) v1

- **Lessons** (`src/lesson.{h,cpp}`): self-contained `*.qlesson.json` Lehrszenario — mehrere Strukturen, je mit voller `SimulationConfig` (verlustfreier `simConfigToJson`/`simConfigFromJson`-Roundtrip, eigene Feldnamen) + Lehr-Metadaten (Titel/Beschreibung/Lizenz/Sprache/Keywords + Autoren mit Name/ORCID/Einrichtung/E-Mail). Strukturen sind inline als XYZ eingebettet. `extractLesson()` entpackt beim Laden in ein Arbeitsverzeichnis (`<slug>.xyz` + Sidecar `lesson.json` mit `file`-Verweisen) → Strukturen erscheinen im bestehenden Datei-Browser.
- **File ▸ Lesson-Menü** (`MainWindow::openLesson`/`saveLesson`/`addCurrentStructureToLesson`/`editLessonMetadata`): Open lädt+entpackt+wechselt Arbeitsverzeichnis; „Add Current Structure to Lesson…" erfasst aktuelle Geometrie + Dock-Bedingungen + Name/Beschreibung/Rolle; „Lesson Metadata…" via `LessonMetadataDialog` (`src/dialogs/`); „Save as Lesson…" schreibt inline-JSON.
- **Bedingungs-Restore**: `SimulationControlWidget::applyConfig()` (Umkehrung von `buildConfig`, signal-blockiert) treibt alle MD/Opt/Wall/Thermostat/Ramp-Widgets aus einer `SimulationConfig`. `MainWindow::applyLessonConditions()` (Hook in `loadMoleculeFile`) stellt beim Klick auf eine Lesson-Struktur deren Bedingungen wieder her, wenn ein `lesson.json`-Sidecar sie referenziert.
- **Überspeichern**: `Save Lesson` überschreibt die gemerkte `m_lessonFilePath` direkt (gesetzt von `openLesson`/`saveLesson`), `Save Lesson As…` fragt nach — beide über `saveLessonInteractive(forceDialog)`.
- **In-Memory-Strukturen sichtbar**: `LessonStructureModel` (`src/lessonstructuremodel.*`) zeigt die noch nicht gespeicherten Lesson-Strukturen im **bestehenden** Datei-Browser via segmentiertem `[ Files | Lesson (N) ]`-Umschalter (Modell-Swap, kein zweiter View, Stil wie Explore/Compute); Klick lädt Inline-XYZ (`xyzToAtoms`) + `applyConfig`, Kontextmenü Load/Remove.
- **Dialogfreies Authoring**: `addCurrentStructureToLesson` fügt mit Default-Namen hinzu (keine 3 QInputDialogs mehr) und fokussiert einen **Inline-Detail-Editor** (Name/Notes/Role) unter der Liste; darüber ein **Metadaten-Widget** (Titel/Beschreibung inline + Authors/License-Dialog), beide nur im Lesson-Modus sichtbar.
- **Aus dem Datei-Browser hinzufügen**: Kontextmenü „Add to Lesson" (xyz/vtf/pdb/mol2) **und** Drag&Drop von Dateien auf den `Lesson`-Umschalter → `addFileToLesson` (`parseFirstFrame` + gemeinsamer `appendLessonStructureFromAtoms`); lädt die Datei nicht in den Viewer, fügt Mehrfachauswahl in einem Rutsch hinzu.
- **Haber-Bosch-Kontext**: „Druck" wird über Box-/Wandvolumen (`wall*`-Felder) + Zusammensetzung kodiert; kein Barostat/NPT (separates Feature). Ergebnis-Felder im Schema reserviert, in v1 nicht implementiert.

## Juni 2026 - Wall Potential Parameters + Visual Potential Field

- **Wall potential parameters** (`wall_temp` / `wall_beta`) in Simulation dock Confinement Walls group: two `TemperatureSlider` widgets ("Strength (K)" / "Steepness β") for energy/force scale and LogFermi steepness. Live-adjustable during an MD run via mutex-buffered `SimulationWorker::setWallTemp`/`setWallBeta` (same pattern as thermostat). `SimulationConfig` extended; `applyWallParams()` writes them into the curcuma `simplemd` controller.
- **Iso-potential shell overlay** (Display panel ▸ "Show potential gradient"): 3 concentric wireframe shells around the confinement wall visualising the force gradient — cyan (far/weak) → amber → red (near/strong). Harmonic walls: fixed distances 4/2/0.8 Å inward; LogFermi walls: distances scale with 1/β (4/β, 2/β, 0.5/β) so the steepness is immediately visible. New `BondInstancing` (`m_potShells`) with `Q_PROPERTY wallPotShellsInstancing/wallPotShellsVisible` in `SceneController`; driven by `MoleculeViewer::setWallPotentialViz`/`setWallPotentialParams`; live-updated when wall sliders move during a run.

## Juni 2026 - Center at Origin

- **Center at Origin** (`Molecule ▸ Center at Origin`, Ctrl+Backspace): `MoleculeViewer::centerAtOrigin()` verschiebt jedes Trajektorie-Frame so, dass der massengewichtete Schwerpunkt im Koordinatenursprung liegt; danach wird die Kamera zurückgesetzt. Massen aus curcuma's `Elements::AtomicMass` + `Elements::String2Element` (kein eigener Massentabellen-Duplikat). `SceneController::centerAtOrigin()` für den aktuellen Frame. Menüeintrag im Molecule-Menü + Command-Palette-Eintrag.

## Juni 2026 - Thermostat-Auswahl (CSVR/Berendsen/Andersen/Nosé-Hoover/None)

- **Thermostat wählbar** im Simulation-Widget (MD Parameters): Combo CSVR/Berendsen/**Andersen**/Nosé-Hoover/None. `SimulationConfig` um `thermostat`/`thermostatCoupling`/`andersenProbability`/`noseChainLength` erweitert; `SimulationWorker::startMD` schreibt `thermostat`/`coupling`/`andersen_probability`/`chain_length` in den `simplemd`-Controller (curcuma liest nur die zum Typ passenden Felder). UI aktiviert Coupling (alle außer None), Andersen-p (nur Andersen) und NH-Kettenlänge (nur Nosé-Hoover) kontextabhängig; im Lauf gesperrt. Andersen thermalisiert Einzelatome/Gasphase besser als CSVR. Engine: `external/curcuma/src/capabilities/simplemd.h` "Thermostat"-PARAM-Kategorie (`thermostat`/`coupling`/`andersen_probability`/`chain_length`).
- **curcuma-Typo `anderson`→`andersen` bereinigt** (ohne Alias, breaking): `ThermostatType::Andersen`, `SimpleMD::Andersen()`, `m_andersen`, Thermostat-String `andersen` und PARAM `andersen_probability` in `external/curcuma/src/capabilities/simplemd.{h,cpp}`; Param-Registry regeneriert. Alte Configs/CLI mit `anderson`/`anderson_probability` funktionieren nicht mehr (so gewünscht).

## Juni 2026 - Struktur-Editing: Markieren/Kopieren/Verschieben + Moleküle einladen + Kollisionsfeedback

- **Edit-Modus** (Viewer-Bar-Toggle „Edit" in Explore, Sibling von „Measure"; `MoleculeViewer::setEditMode`, exklusiv zu Measure/Bond-Edit): direkte Koordinaten-Bearbeitung, getrennt von der Sim-Grab-Force (die injiziert Kräfte in laufendes MD/Opt). `eventFilter`-Zweig: Klick = Atom selektieren, **Doppelklick = ganzes Molekül** (`selectFragment`, BFS über den aktuellen Bindungsgraphen via `forceinjector::buildAdjacency`), Drag = verschieben (Shift = Tiefe, Pfeiltasten = Nudge), Klick ins Leere = Auswahl löschen/rotieren.
- **Verschieben**: `SceneController::screenDragToModelDelta` (Translations-Zwilling von `computeGrabForce`: gleiche Pixel→Welt-Skala in der Atom-Tiefe, aber Ångström statt Bohr, kein Force-Faktor) → `moveSelection` addiert das Delta auf die Atompositionen und nutzt den günstigen `updatePositions`-Pfad (keine Kamerasprünge).
- **Kollisionen**: `MoleculeViewer::computeCollisions` (O(N²), Clash wenn Abstand < `kClashFactor`=0.6 × (vdW_i+vdW_j); überspringt gebundene Paare und Paare *innerhalb* der Auswahl) → `SceneController::setCollisionAtoms` färbt sie **rot** (Priorität über die magenta Auswahl in `atomColor`), `collisionCountChanged` speist „⚠ N clashes / ✓ no clashes" in die Viewer-Bar.
- **Resolve clashes** (Button, sichtbar bei Clashes): `resolveClashes` verschiebt die Auswahl rigide entlang der überlappgewichteten Netto-Abstoßrichtung, bis kollisionsfrei (Iter-Cap).
- **Kopieren/Einfügen/Löschen** (Edit-Menü, kontextabhängig zu Text-Copy/Paste): `copySelection`/`pasteClipboard` (Clipboard mit intern re-indizierten Bindungen, Offset beim Einfügen) und `deleteSelection` (entfernt Atome + inzidente Bindungen, re-indiziert) — nur Einzelstruktur (`canEditStructure`, `frameCount<=1`); Ctrl+C/Ctrl+V wirken im Edit-Modus auf die Auswahl, sonst auf den Struktur-Text.
- **Molekül einladen** (`Edit ▸ Add Molecule to Scene…` + `MainWindow::parseFirstFrame` für xyz/vtf/pdb/mol2; **auch per Rechtsklick im Datei-Browser** „Add to current scene" → `mergeFileIntoScene`): `MoleculeViewer::appendMolecule` hängt Atome/Bindungen an, selektiert sie und startet die Platzierung über einen **bounds-/kamera-erhaltenden** Rebuild (`SceneController::setStructure(..., keepView=true)` — kein `recomputeBounds`/`resetView`, sonst würde ein rotiertes Molekül um (I−R)·Δcenter springen).
- **WASD/QE-Rotation + Rubber-Band**: **W/S Pitch, A/D Yaw, Q/E Roll** drehen die Szene über einen **App-weiten Key-Filter** (`MainWindow::eventFilter` auf `qApp`), **nur im Edit-Modus aktiv** (sonst sind die Tasten überall frei), unterdrückt bei fokussiertem Text-Widget oder Ctrl/Alt/Meta. `MoleculeViewer::rotateSceneByKey` → `applyModelRotation` (3-Achsen, gemeinsam mit Maus-Rotation). **Shift+WASDQE** nudgt die Auswahl (Q/E = Tiefe). **Ctrl/Shift+Ziehen auf leerer Fläche** = Rubber-Band-Box-Auswahl (`SceneController::atomsInScreenRect` + 2D-QML-Overlay via `setRubberBand`/`rubberBandRect`); normales Ziehen auf leerer Fläche rotiert weiterhin.
- **Deselektion per Rechtsklick**: Rechtsklick (ohne Pan-Drag) löscht die Auswahl (und Mess-Marken); Rechts-Ziehen pant weiterhin. `m_rightDragged`-Flag unterscheidet Klick von Drag.
- **Edit-Hint-HUD**: 2D-Overlay unten-mittig (`SceneController::editHint` → `viewer3d.qml`) mit der Tasten-/Maus-Belegung, eingeblendet solange der Edit-Modus an ist.
- **Undo**: vor jeder mutierenden Aktion (move/paste/merge/delete) emittiert der Viewer `editSnapshotRequested(label)` → `MainWindow::takeSnapshot`, sodass der Snapshots-Tab als Undo dient.
- **Cursor-Lock beim Ziehen** (`m_dragCursorLock`, default an; Edit ▸ „Lock Cursor While Dragging"): warpt den Cursor pro Move zurück zum Anfasspunkt → relatives/unendliches Ziehen ohne Bildschirmrand (synthetisches Warp-Move via Null-Delta-Guard absorbiert). Braucht X11-Cursor-Warping; unter Wayland ggf. No-op (Ziehen funktioniert weiter, Cursor nur nicht fixiert).

## Juni 2026 - Laufzeit-Temperatur: Slider + Rampen + Regionen + Live-Charts + dynamische Bindungen

- **Dynamische Bindungen** (`MoleculeViewer::updateSimulationFrame`, default an): pro Live-Frame (MD **und** Opt) wird der Bindungsgraph aus der Geometrie neu erkannt (`detectBondsHysteresis`, Kovalenzradien × Toleranz mit Hysterese: form 1.25, break 1.45 → kein Flackern bei thermischer Vibration nahe der Schwelle), sodass Bindungsbruch/-bildung in Reaktionen gezeichnet werden. Nur bei tatsächlicher Topologieänderung (`bondSetEqual`) wird `SceneController::updateBonds()` gerufen (neue Bond-Instancing-Geometrie ohne Bounds-/Kamera-Reset → kein Ruckeln); stabile Frames bleiben auf dem schnellen Positions-Pfad. Toggle im Display-Dock (Tools: "Dynamic bonds"). O(N²)/Frame (für interaktive Größen ok).
- **Live-Charts** (`src/widgets/simulationchart.*`, CuteChart `ListChart`): zwei gestapelte Zeitreihen-Charts in einem **modeless Dialog** "Simulation Charts" (geöffnet über Molecule ▸ Simulation Charts, `m_simulationChartDialog`; modeless statt `exec()`, damit die Sim-Steuerung während des Laufs bedienbar bleibt) — **Temperatur** (instantan + Sollwert/Rampe) und **Energie** (E_pot/E_kin/E_tot). `SimulationFrame` um `temperature`/`targetTemperature` erweitert (aus `SimpleMD::currentTemperature()`/`targetTemperature()` in `moleculeToFrame`). `MainWindow::wireSimulationWorker` verbindet `frameReady` → `SimulationChartWidget::appendFrame` (QueuedConnection) + `reset()` pro Run; rollendes Punkt-Limit (2000) + gedrosseltes `formatAxis()` (~8 Hz). QtCharts + CuteChart sind bereits gelinkt (NMR-Dialog).
- **Vertikaler temperatur-farbiger Slider** (`src/widgets/temperatureslider.*`): Thermometer (blau→rot) mit editierbarem Min/Max + numerischer Anzeige, ersetzt die Temperatur-Spinbox im Simulation-Dock und bleibt **während des Laufs aktiv** (`setRunning` sperrt ihn nicht mehr). Drag → `temperatureChanged` → `MainWindow::wireSimulationWorker` (QueuedConnection) → `SimulationWorker::setTargetTemperature` → setzt `SimpleMD::setTargetTemperature` vor dem nächsten Step (Muster wie Grab-Force); überschreibt eine laufende globale Rampe („ramp overridden"-Badge).
- **Temperatur-Rampe** (QGroupBox „Temperature Ramp", MD-only): Enable + Tabelle (Target K | Mode steps/reach | Value) → baut `temp_schedule`-String; **Temperatur-Regionen** (QGroupBox „Temperature Regions"): Tabelle (Atoms | Start T | Schedule) → `temp_regions`-JSON-Array. `SimulationConfig` hält `tempRamp`/`tempSchedule`/`tempRegions`; `applyTempRampParams()` (file-local, simulationworker.cpp) schreibt sie in den `simplemd`-Controller. Engine-Seite: siehe curcuma AIChangelog + `external/curcuma/docs/TEMPERATURE_RAMP.md`.

## Juni 2026 - UI P3+P4: Command-Palette + Menü-Konsolidierung

- **P3 Command-Palette** (`src/widgets/commandpalette.*`, `Ctrl+K`): durchsuchbares Popup; sammelt automatisch alle Menü-Actions (rekursiver `menuBar()`-Walk: Titel, Menü-Pfad als Kontext, Shortcut, `QAction::trigger`) + kuratierte Shortcut-only-Befehle (Explore/Compute, Render-Modi, Fit, Select-All/Clear). Tippen filtert (Prefix>Wortanfang>enthält>Kontext), ↑/↓, Enter, Esc; deaktivierte Actions grau. `MainWindow::showCommandPalette`.
- **P4 Menü-Konsolidierung** (7→6): „Analysis" + „Simulation" → **„Molecule"** (MD/Opt + RMSD; redundantes „Show Simulation Panel" raus). **View** erweitert: Command Palette, **Mode ▸ Explore/Compute**, **Display-Dock-Toggle** (fehlte), „Display Options…" (von Settings hierher). Settings schlanker. Icons ergänzt (About/Recent/Workspaces/Dark Mode/…). `Ctrl+K` nur noch auf der Menü-Action (kein doppelter `QShortcut`).
- **Fix Mode-Switch-Verschiebung**: Explore/Compute-Buttons aus der Toolbar-Area in die **Menüleisten-Ecke** (`menuBar()->setCornerWidget(…, Qt::TopRightCorner)`) → fester Platz, kein Reflow mehr beim Ein-/Ausblenden der Rechen-Toolbar. `createModeBar` läuft jetzt nach `createMenus`.

## Juni 2026 - Harmonische Confinement-Wände (aktivieren + visualisieren)

- Curcuma `SimpleMD` kennt harmonische Wände (`wall_type` none/spheric/rect, `wall_potential` harmonic/logfermi, `wall_x|y|z_min/max`, `wall_radius`), aber qurcuma stellte sie nie ein, aktivierte sie nicht und zeichnete sie nicht. Jetzt: QGroupBox „Confinement Walls" im Simulation-Dock (MD-only, Enable→Details: Geometry/Potential-Combo + 6 rect bounds + sphere radius, manuell einstellbar) → `SimulationConfig`-Felder → `SimulationWorker::applyWallParams()` schreibt sie bei `wallEnabled` in den curcuma-Controller (Muster wie `applyRmsdMtdParams`).
- **Live-Visualisierung**: `SceneController::setWallBox`/`setWallSphere` bauen 12 Kanten bzw. Lat/Long-Ringe als `BondInstancing`-Segmente (Muster wie `setMeasurement`), `#Cylinder`-Model unter `moleculeRoot` (rotiert mit dem Molekül, intrinsische Koordinaten). `MainWindow::onSimulationConfigChanged` → `MoleculeViewer::setConfinementBox` zeichnet die Box schon beim Tippen der Bounds (auto-show when enabled); Display-Panel „Show confinement walls" (`setWallVisibleOverride`, `wallVisible` in `VisualizationSettings`) blendet sie unabhängig aus. Auto-Size (Bounds/Radius = 0) nicht vorab zeichenbar — nur explizite Werte werden gezeichnet.
- **Grenzverletzungs-Feedback**: `MoleculeViewer::computeWallViolations()` zählt pro Frame/Live-MD die Atome außerhalb der Wand, rekolloriert das Wireframe **rot** bei Verletzung (`SceneController::setWallColor`+`rebuildWall`, Material baseColor→white damit die Per-Segment-Farbe voll zeigt), emit `wallViolationChanged` → Status-Label im Sim-Dock („⚠ N atoms outside / ✓ all atoms inside").

## Juni 2026 - UI P2 + Viewer-UX (Mode-Switch, Messen-Rework, Hover)

- **P2 Mode-Switch Explore/Compute** (`MainWindow::createModeBar`/`setAppMode`): segmentierte Top-Leiste [🔬 Explore | ⚙ Compute]. Explore = Viewer groß, Display/Atoms-Docks, **Rechen-Toolbar aus**; Compute = Rechen-Toolbar an + Project/Output/Editors. Setzt Dock-/Toolbar-Sichtbarkeit explizit (deterministisch), persistiert `ui/appMode` (Default Explore). Die 4 Layout-Presets bleiben unverändert.
- **Messen neu**: Bar-`QToolButton` „Measure" (Icon, checkable) statt Combo; Typ wird aus der **Atomanzahl** erkannt (2=Distanz, 3=Winkel, 4=Dieder). Klick markiert, Klick auf markiertes Atom **demarkiert**, Esc/Leerklick löscht. HUD zeigt Live-Fortschritt UND **alle** Größen: alle paarweisen Abstände + Ketten-Winkel + Dieder (mehrzeilig, monospace, wrap). Sync Bar↔Dock via `measurementModeChanged`; Dock-Combo → Checkbox.
- **Hover-Feedback**: Maus über Atom hellt es auf (`SceneController::setHoverAtom`, günstiger Atoms-only-Rebuild via `rebuildAtoms`, nur bei Wechsel) + Pointing-Hand-Cursor; löscht beim Verlassen.
- **Player nur bei Trajektorien**: Playback (Play/Pause/FPS/Loop) in `m_playbackWidget` gruppiert, ausgeblendet bei Einzelstruktur (sichtbar ab >1 Frame, wie der Frame-Slider).

## Juni 2026 - UI P1: "Display"-Dock (Viewer-Leiste entrümpelt, Dialog konsolidiert)

- Neues **Display-Dock** (`src/displaypanel.*`, rechts, tabifiziert mit Editors) mit einklappbaren Sektionen (`src/widgets/collapsiblesection.*`): **Style / Effects / Lighting / Tools / Presets** — die EINE Heimat aller 3D-Anzeige-Optionen, live an die `MoleculeViewer`-Setter gebunden.
- **Viewer-Leiste (`setupControlPanel`) entrümpelt**: nur noch Frame-Nav + Playback + Quick-Combos (Render-Mode/Color) + „Display ⚙"-Button. Material/Glow/Measure/Bond-Edit/Force/Fog/Eck-Lichter/BG sind ins Dock gewandert (~20 → ~6 Controls).
- **Modaler `VisualizationSettingsDialog` entfernt** (Logik/Persistenz/Presets ins Dock portiert); Menü „Visualization Settings" + Button raisen jetzt das Dock; `syncVisualizationDialog`→`DisplayPanel::loadCurrentSettings`.
- Bar↔Dock-Sync via neue Signals `MoleculeViewer::renderingModeChanged/colorSchemeChanged` (+ `displayOptionsRequested`); Shortcuts 1–4 halten beide aktuell. Nächste UI-Schritte: P2 (Mode-Switch Explore/Compute), P3 (Command-Palette).

## Juni 2026 - Quick3D-Overlays portiert (M2: Messen, Bond-Edit, RMSD)

- **Messen** (Distanz/Winkel/Dieder): Mode-Combo + Klicks sammeln 2/3/4 Atome → cyanfarbene Linien (instanzierte Zylinder, Weltraum) + Ergebnis-Label (2D-HUD); Werte folgen Trajektorien-Frames. `updateMeasurement()` in `view.cpp`, `SceneController::setMeasurement()`.
- **Bond-Editing** über Atompaar (kein Bond-Picking nötig): in Add/Delete/Cycle-Mode zwei Atome klicken → Bindung hinzufügen/löschen/Ordnung 1→2→3 zyklen; Live-Rebuild + XYZ-Auto-Save (`performBondEdit()`).
- **RMSD-Overlay** (`showOverlay`): zweite Struktur als eigener Instanz-Satz unter `moleculeRoot` (rotiert mit). Statt „doofem Gelb" jetzt **HSV-verschobene CPK-Farben** (Hue +30°, leicht dunkler, Sättigungs-Floor → auch C/H getönt) + leicht kleinere Kugeln → element-erkennbar und klar als „andere" Struktur unterscheidbar (`SceneController::setOverlayStructure` + `shiftOverlayColor`).
- Klick-Logik in `eventFilter` jetzt modusabhängig (Selektion / Messen / Bond-Edit); `clearSelection`/`setMeasurementMode` aktualisieren das Mess-Overlay.

## Juni 2026 - Qt3D entfernt, Vulkan-Backend + Tiefen-Nebel (Schritt 2b)

- **Qt3D vollständig entfernt** (`find_package`/Link ohne `Qt6::3D*`, orphane Qt3D-Quellen gelöscht: atom/bondinstancingsystem, force/measurementoverlay, pbrmaterial, orbittransformcontroller). Binary linkt keine `Qt6::3D*`-Lib mehr.
- **RHI-Backend Vulkan** als Default, aber mit **Probe + Fallback**: `main.cpp` testet `QVulkanInstance::create()` und schaltet bei fehlendem Loader/ICD automatisch auf OpenGL → läuft cross-vendor (NVIDIA proprietär / AMD-RADV / Intel-ANV) und auch ohne Vulkan. Override via `QSG_RHI_BACKEND=vulkan|opengl`.
- **Optionaler Tiefen-Nebel** (`≋`-Toggle + zwei Slider: **Stärke** und **Distanz** im Control-Panel): `ExtendedSceneEnvironment.Fog` (depth-fog); Stärke = `fogDensity`, Distanz = `fogDistance` (0..1 verschiebt den Nebel-Start von der Molekül-Vorderseite zur Rückseite). near/far-Band folgt der Molekül-Tiefe (zoom-abhängig), entfernte Atome verblassen in die Hintergrundfarbe. Über `setFogEnabled`/`setFogIntensity`/`setFogDistance`.

## Juni 2026 - Renderer-Migration Qt3D → Qt Quick 3D (WP2, Schritt 2a)

- `MoleculeViewer` (`src/view.*`) intern auf **Qt Quick 3D** umgebaut (eingebetteter `QQuickView` + `SceneController` + `src/qml/viewer3d.qml`), **öffentliche API unverändert** → `mainwindow.cpp` & Konsumenten unberührt. Neue Bausteine: `scenecontroller.*` (Szene-View-Model), `atominstancing.*`/`bondinstancing.*` (`QQuick3DInstancing`), `elementdata.*` (CPK/vdW/kovalent).
- Instanziertes Rendering (Atome/Bindungen), eingebaute Effekte via `ExtendedSceneEnvironment` (**SSAO/Bloom/HDR/Tonemap wirken jetzt echt** statt der alten Stubs), Schatten, Fog. Maus (Rotate/Pan/Zoom/Reset) + **Ray-Picking → Selektion** (Klick vs. Drag) in C++; Materialien opak (Blend nur bei Transparenz<1, sonst sah man Zylinder-Kanten).
- **Interaktiver Grab** portiert: `computeGrabForce` rechnet Screen→World über die selbst-replizierte Kamera-Projektion (Quick3D-Viewport/Camera sind privat in dieser Qt-Installation), FoV auf 45° wie der alte Viewer; Vorzeichen geprüft (Atom folgt Cursor, da curcuma `gradient += F`).
- **Opt-in Kraftvektoren** (`↯`-Toggle): gelber Pfeil am gegriffenen Atom + orange Schalen-Pfeile via identischem `forceinjector::distributeForce` wie der Integrator (Pfeile = exakt injizierte Kräfte). Noch offen (M2): Mess-/Bond-Edit-Overlays, RMSD-Tönung. Vulkan-Backend + Qt3D-Entfernung = Schritt 2b.

## Juni 2026 - WP0: Qt Quick 3D + Vulkan Spike (standalone)

- `spikes/quick3d/` als **eigenständige** Mini-App (eigene `CMakeLists.txt`, qurcuma-Build unberührt) zur De-Risk-Entscheidung Qt3D → Qt Quick 3D. Self-contained Datenschicht (`moleculedata.*`: Grid-Generator 1k/5k/10k + XYZ-Loader + lokale CPK-Farben/Radien/Bond-Detection), **keine** qurcuma/Qt3D-Header.
- T3-Instancing in **einer** Klasse je Geometrie: `AtomInstancing`/`BondInstancing` (`QQuick3DInstancing`), per-Instanz via `calculateTableEntry`/`calculateTableEntryFromQuaternion`; Bond-Quaternion (Y→Bindungsrichtung) 1:1 aus `view.cpp:1345`, zwei Halbzylinder je Bindung. #Sphere/#Cylinder-100-Unit-Skalierung berücksichtigt.
- T4 `ExtendedSceneEnvironment` (SSAO/Bloom/Tonemap) + `DirectionalLight castsShadow` mit Boden-Plane; T5 Vulkan-RHI Default + `--gl`-Fallback, Backend im Log/HUD bestätigt; T6 beide Einbettungsrouten (`--embed=quickwidget|container`, gleiches `Main.qml`); T7 Picking via `View3D.pick`→`instanceIndex` (Model braucht `pickable: true`!); T8 FPS-Meter (`frameSwapped`) + MD-Proxy-Animation. UX-Extras: In-Szene-HUD, Screenshot- + Reset-View-Button. Build warnungsfrei (Qt 6.11.1).
- **Operator-validiert (AMD Radeon 890M / RADV, Vulkan 1.4.348):** läuft flüssig, 1k statisch >60 FPS, 10k statisch ~30 FPS (synthetisches Grid bond-lastig: ~58k Zylinder-Instanzen), Instanced-Picking liefert korrekten `instanceIndex`. FPS-Readout nur in der nativen `createWindowContainer`-Route (QQuickWidget rendert via `QQuickRenderControl`, kein `frameSwapped`) — die native Route ist ohnehin qurcumas heutiges Muster. 10k animiert (MD-Proxy) 40–50 FPS ohne / 20–30 mit dem synthetischen Bond-Overkill (~58k Zylinder, ~3× eines echten Moleküls). **Operator-Verdikt: GO** für Qt-Quick-3D-Migration (Report: `spikes/quick3d/REPORT.md`). Offene Punkte für WP2: Schatten nicht weltfest, echte-Molekül-FPS. `QQuickWidget` emittiert kein `frameSwapped` (Render-Control) → FPS nur in nativer Route. T9-VR weiter offen.

## Juni 2026 - RMSD-MTD-Bias im interaktiven Simulation-Widget

- `rmsd_mtd` (curcuma `SimpleMD`-Bias-Modus, kein eigener Treiber) als Option ins Simulation-Widget gebaut: neue QGroupBox "RMSD Metadynamics" (nur im MD-Modus sichtbar, Enable-Checkbox → Details). Alle relevanten Parameter exponiert: k, α, RMSD-atoms, ref-file (mit Browse), max-gaussians, max-height, econv (Bias-Ablagerungs-Schwelle, default 1e8), pace, well-tempered (wtmtd) + ΔT, freeze-inherited.
- `SimulationConfig` (`simulationworker.h`) um die Felder erweitert (Defaults aus `external/curcuma/src/capabilities/simplemd.h` "RMSD-MTD"-PARAM-Kategorie). `buildConfig()`/`notifyConfig`/`setRunning`/`onModeChanged` im Widget bedacht; `SimulationWorker` schreibt die Keys nur bei `rmsdMtd=true` via file-local `applyRmsdMtdParams()` in den `simplemd`-Controller (startMD + single-step MD).

## Juni 2026 - RMSD-Tool vom Dialog in Editors-Dock-Tab umgewandelt

- `RMSDDialog` (modaler Fremdkörper) → `RMSDWidget : public QWidget`, eingebettet als **dritter Tab im Editors-Dock** (`m_editorsTabs`: Structure/Input/RMSD) statt eigenem Dock — rechte Seite bleibt bei der 5-Dock-Architektur (Project/Navigation/Editors/Atoms&Simulation/Output).
- Referenz-Saat jetzt **Auto + Button**: `showRMSDTool()` fokussiert den Editors-Dock + Tab-Index 2 und säht beim Menü-/Kontextmenü-Aufruf automatisch aus dem Viewer, plus Button "Use current as reference" im Widget (Signal `seedReferenceRequested` → `MainWindow::seedRMSDReference()`). Widget bleibt vom Viewer entkoppelt.
- Menüeintrag `Analysis ▸ RMSD / Align Structures` fokussiert Editors-Dock + RMSD-Tab (Vorbild `showSimDock`); Layout-Presets unverändert (Editors-Sichtbarkeit steuert den Tab). `src/dialogs/rmsddialog.*` entfernt, `src/rmsdwidget.*` neu.

## Juni 2026 - Bildschirmfeste 4-Eck-Beleuchtung (dreht nicht mehr mit dem Molekül)

- Eck-Lichter (`m_lightRoot`) von `m_modelEntity` an die **Kamera** gehängt → die beleuchtete Zone bleibt bildschirmfest; "Lampe links oben" leuchtet immer den aktuell links-oben sichtbaren Molekülteil aus, statt mit dem Molekül mitzudrehen (`view.cpp`).
- Instancing-Shader (`atom_instanced.frag`/`bond_instanced.frag`) nutzen jetzt 4 zuschaltbare View-Space-Eck-Lichter via Uniform `cornerLightEnabled` (vec4) statt eines fest verdrahteten Einzel-Headlights; die 4 Eck-Toggles wirken damit auch im GPU-Instancing-Pfad (≥500 Atome).
- Neuer Param/Setter `setCornerLightIntensities()` in `AtomInstancingSystem`/`BondInstancingSystem`; `updateInstancingCornerLights()` pusht die Maske nach Rebuilds und Toggles. Intensität auf gemeinsame Konstante `kCornerLightIntensity` vereinheitlicht.
- Echte geworfene Schatten (Shadow-Mapping) bewusst offen gelassen (Phase 2: Custom-Framegraph nötig).

## Juni 2026 - RMSD/Align/Reorder-Tool aus curcuma direkt in qurcuma

- Neuer Dialog (`src/dialogs/rmsddialog.*`) kapselt curcumas `RMSDDriver`: Referenz = aktuell angezeigte Struktur, Ziel = geladene Datei; richtet das Ziel aus und ordnet Atome optional um (Permutation), zeigt RMSD-Wert + Reorder-Mapping und speichert das ausgerichtete Ziel als XYZ.
- Permutationsmethode wählbar (subspace/inertia/template/dtemplate/incr/molalign/predefined) plus Schalter protons/force_reorder/no_reorder, Template-Element und Threads.
- 3D-Overlay: `MoleculeViewer::showOverlay()` zeigt beide Strukturen gleichzeitig — Referenz in CPK, ausgerichtetes Ziel einfarbig (Gold, transparent). `createMoleculeEntity` erhielt dafür einen optionalen Uniform-Color/Alpha- und `trackForUpdates`-Override (Ziel ist statisch, fasst die Inkrement-Update-/Picker-/Instancing-Bookkeeping der Primärstruktur nicht an).
- Einstieg: Menü „Analysis ▸ RMSD / Align Structures…" und Kontextmenü im Datei-Manager (xyz/vtf/pdb/mol2 → „Overlay onto current (RMSD/Align)…").
- Molekül-Brücke `src/moleculebridge.h` (atomsToMolecule/moleculeToAtoms) wiederverwendbar zwischen Viewer-Atomliste und curcuma `Molecule`.
- Build-Fix: `FETCHCONTENT_FULLY_DISCONNECTED` automatisch gesetzt, wenn lokale `external/curcuma`/`external/CuteChart` vorhanden sind — FetchContent rebased/überschreibt den lokalen, push-fähigen curcuma-Checkout nicht mehr bei jedem Reconfigure (debug + release).

## Juni 2026 - Interaktiver Opt-Grab: Kraft wirkt als Potential + Keep-alive + Crash-Fix

Per Print-Instrumentierung verifiziert, dass die Kraftkette vollständig ankommt (`injectForce` → Worker → `setExternalForces` → LBFGSpp-Gradient, |ext| bis ~0.6 Eh/Bohr). Drei verbleibende Probleme behoben:

- **„Gefühlt passiert nichts"**: Der Bias wurde nur auf den **Gradienten** addiert, nicht auf die zurückgegebene **Energie**. LBFGSpps Backtracking-Line-Search akzeptiert aber nur Schritte, die die *Energie* senken → jeder Schritt in Grab-Richtung, der die echte GFN-FF-Energie erhöht, wurde verworfen → Atom bewegte sich kaum. Fix (curcuma `lbfgspp_optimizer.cpp`): konsistentes lineares Bias-**Potential** `E_bias = Σ f_ext·x` hinzufügen (dessen Gradient genau `f_ext` ist). Jetzt sind Energie und Gradient konsistent, der Schritt wird angenommen, das Atom wandert ins verschobene Gleichgewicht (Rückstellkraft balanciert den Zug). **Noch zu replizieren für native LBFGS + ANCOpt** (gleicher Gradient-only-Bias).
- **Keep-alive bei Konvergenz**: `runOptimization` baut jetzt pro Zyklus einen **frischen** Optimizer (EnergyCalculator wird wiederverwendet) und macht von der aktuellen Geometrie weiter, statt `Optimize()` erneut aufzurufen. Beendet nur über Stop.
- **SIGSEGV behoben**: Ein zweiter `Optimize()`-Aufruf auf demselben (verbrauchten) LBFGSpp-Solver betrat toten Single-Step-Zustand → Absturz. Der frische Optimizer pro Zyklus verhindert das.

## Juni 2026 - Maus-Grab-Kraft ist jetzt "sticky" (wirkt solange geklickt gehalten)

- **Symptom**: Im Opt-Modus reagierte das Molekül nicht auf den Grab, obwohl die Kraftvektoren korrekt angezeigt wurden.
- **Ursache**: Der Viewer sendet `atomForceRequested` nur bei Maus-*Bewegung* (view.cpp:239) + einmal pro Frame (view.cpp:2094). Der Worker verbrauchte die Kraft per Einmal-Drain (`m_pendingForcesValid=false`). Bei stillgehaltener Maus kam kein neues Event → die Verzerrung wurde jeden Schritt gelöscht. MD kaschierte das über den Impuls; Opt hat keinen Impuls und relaxiert sofort zurück → keine sichtbare Bewegung.
- **Fix**: Kraft ist jetzt *sticky*. `injectForce` hält sie bis `clearInjectedForce` (Mausloslassen); der Worker liest sie per Peek (`currentInjectedForces`, kein Verbrauch) bei *jedem* MD-Schritt / Opt-Iteration neu. Damit wirkt die Kraft genau solange der Button gehalten wird.
- **Unverändert**: `processEvents()`-Pump im Opt-Callback (liefert die ge-queue-ten `injectForce`/`clearInjectedForce` an den im synchronen `Optimize()` blockierenden Worker; der Dispatcher existiert — MDs `QTimer` feuert) und die curcuma-seitige Gradient-Bias.

## Juni 2026 - Release/AVX-512-Crash der interaktiven Simulation behoben

- **Ursache**: Eigen-ABI-Mismatch zwischen qurcuma und curcuma. curcuma_core wird per FetchContent mit `-march=native` (hier AVX-512 → `EIGEN_MAX_ALIGN_BYTES=64`) gebaut, qurcumas eigene TUs aber nur mit `-O3` (SSE2 → `16`). curcumas `set(CMAKE_CXX_FLAGS ... -march=native)` liegt im Subdir-Scope und erreicht das qurcuma-Target nicht.
- **Symptom**: `moleculeToFrame` kopiert/freed eine curcuma-allokierte `Geometry` in einer qurcuma-TU → `double free or corruption` direkt beim Start von MD/Opt (nur Release; Debug baut beide Seiten SSE2 → ok). Backtrace bestätigt: `#7 moleculeToFrame → #8 SimulationWorker::runOptimization`.
- **Fix**: `CMakeLists.txt` spiegelt jetzt curcumas SIMD-Flags (`USE_MARCH_NATIVE`/`USE_AVX512`/`USE_AVX2`) per `target_compile_options(qurcuma ...)`, sodass beide Seiten dieselbe Eigen-Alignment-ABI nutzen.
- **CLI-Reproduktion**: `qurcuma <file> -md|-opt` lädt die Datei und startet die Simulation direkt aus der Bash (diagnostischer Hebel; getestet mit `complex.xyz`, 231 Atome).

## Juni 2026 - Force-Injection in Opt wirkt jetzt wirklich (alle Optimizer)

Zwei Bugs zusammen verhinderten jede Wirkung des Maus-Grabs im Opt-Modus:

- **qurcuma-Bug (eigentliche Ursache)**: `runOptimization` ruft `optimizer->Optimize()` synchron im Worker-Thread auf — dessen Qt-Event-Loop läuft währenddessen NICHT. `injectForce`/`clearInjectedForce` sind aber `QueuedConnection`-Slots an genau diesen Thread, ihre Events werden also nie zugestellt; `drainPendingForces` liefert immer leer → `clearExternalForces` → kein Bias. (MD funktioniert, weil es `QTimer`-getrieben ist und der Event-Loop zwischen Schritten läuft.) **Fix**: `QCoreApplication::processEvents()` im Opt-Step-Callback stellt die gequeueten Grab-Forces zu, bevor drainiert wird.
- **curcuma-Bug**: Der frühere Bias hing nur am `OptimizerDriver::Optimize`-Loop (`m_current_gradient += m_external_forces`) — toter Code, da JEDER Optimizer seinen Gradienten selbst auswertet und `m_current_gradient` für den Schritt nie liest. **Fix (beide Kopien)**: Bias direkt an den echten Cartesian-Gradient-Stellen addiert: `LBFGSppObjectiveFunction::operator()` (deckt `auto`/`lbfgspp`), `LBFGS::getEnergyGradient` (deckt `native_lbfgs`/`diis`/`rfo`), `ANCOptimizer::CalculateOptimizationStep` vor der ANC-Transformation. Bias wird per `const Vector*` in die Nicht-Treiber-Klassen (`bindExternalForces`) geleitet, persistiert über Line-Search-Auswertungen und wird via `clearExternalForces()` beim Loslassen genullt. Sign/Layout/Einheiten identisch zum MD-Pfad (Eh/Bohr, atom-major).
- **Verifiziert (CLI, ohne Maus)**: konstante Testkraft auf Atom 0 bewegt dessen Endposition deutlich (Baseline x=2.41 → Kraft 0.1: x=1.10 → Kraft 0.5: x=1.97), Bias erreicht also nachweislich den Optimierer-Gradienten.

## June 2026 - Interaktive Simulation: live Force-Update in Opt + korrigierte Grab-Skala

- Opt-Auto-Run reagiert jetzt live auf Mausziehen: der Step-Callback drainiert `pendingForces` nach jeder Iteration und aktualisiert `optimizer->setExternalForces()` / `clearExternalForces()`
- Grab-Force-Skala korrigiert: `computeGrabForce()` rechnet screen-space-Delta von Å nach Bohr um; Default `m_grabStrength` von 0.01 auf 0.1, Range bis 10.0
- Tote `ForceOverlay::updatePositions()` entfernt

## June 2026 - Simulation dock: Reset + Snapshot-History-Foundation

- Reset-Button im Simulation-Dock neben Save; Status-Labels (Modified/Finished) auf eine zweite Zeile ausgelagert
- Reset-Button ist jetzt index-basiert und stellt Snapshot 0 wieder her; Snapshot 0 wird automatisch beim Laden erzeugt
- `m_originalSnapshot` entfernt; Reset greift konsistent auf `m_snapshots[0]` zu
- Manuelle Snapshot-History im neuen Snapshots-Tab: Take/Restore/Delete; globaler `MoleculeSnapshot`-Typ wird von `MainWindow` und `SnapshotsWidget` geteilt
- Auto-Snapshot-Stride im Snapshots-Tab: jede N-te MD/Opt-Step erzeugt automatisch einen Snapshot (0 = aus)

## June 2026 - Simulation dock UX: kompakte Buttons + Step für MD & Opt

- Button-Reihe von 4 text+icon QPushButtons auf 5 icon-only QToolButtons (▶ Start, ⏸ Pause, ⏭ Step, ■ Stop, 💾 Save) geschrumpft — spart ~30px Vertikalraum im Dock
- Farbiger Status-Pill (`● Running` grün / `⏸ Paused` amber / `● Finished` grau / `⏭ Stepping` blau) ersetzt separaten Status-Text
- `Speed` (fpsLimit) aus der MD-Gruppe an Top-Level verschoben — jetzt in beiden Modi sichtbar
- Neuer **Step**-Button funktioniert symmetrisch für MD und Opt:
  - MD: ein `md.step()` (frischer SimpleMD, ein Schritt, fertig)
  - Opt: eine LBFGS/DIIS/RFO/ANCOpt-Iteration (`single_step_mode=true`, fertig)
  - Dock-throttlet Klicks auf `1000/fpsLimit` ms → "max XXX FPS" wird eingehalten
- `Speed`-Spinner bleibt während eines laufenden Runs editierbar (Live-Throttle)
- **Force-Injection auch in der Optimierung** (Maus-Grab Parität mit MD):
  - qurcuma: `runOptimization` und `stepOnce` drainen pending forces und reichen sie via `setExternalForces`/`clearExternalForces` an den Optimizer weiter
  - curcuma-seitige Anwendung war zunächst nur im Treiber-Loop (`m_current_gradient += bias`) und damit wirkungslos — korrekt umgesetzt in der Force-Injection-Korrektur (siehe oberste Changelog-Einträge Juni 2026)

## April 2026 - Simulation: Echtzeit-Schrittanzeige & RATTLE-UI

- MD: Jeder berechnete Schritt wird angezeigt; `fpsLimit` koppelt Step-Rate an Anzeige-Rate 1:1, throttelt nur wenn CPU schneller als Ziel (bei langsamer Rechnung läuft jede Step voll durch)
- MD-Render-Fix: Backpressure entfernt, throttle-then-emit statt emit-then-ack — deterministische Cadence, keine Jitter-induzierten Frame-Drops durch Qt3D-Coalescing mehr
- OPT: Per-Schritt-Callback via `OptimizerDriver::setStepCallback()` — jede Iterationsgeometrie wird live dargestellt, gleicher throttle-then-emit-Pfad
- RATTLE: Vollständige UI im SimulationDock (Mode off/RATTLE/H-only, 1-2/1-3 Constraints, Toleranzen, Max-Iter); wird an SimpleMD-JSON weitergereicht
- curcuma: `StepCallback`-API in `optimizer_driver.h/.cpp` ergänzt (std::function-basiert, kein API-Break)

## January 2025 - Complete SFTP Integration & HPC Workflow ✅

### Phase SFTP Integration - Production-Ready Remote File Access (~1200 lines total)

#### **Core Components**
- **SftpDialog** (440 lines): Enhanced UI with profile management + SSH config dropdowns
- **SftpItemModel** (445 lines): QAbstractItemModel with lazy loading (fetchMore/canFetchMore)
- **SshConfigParser** (200 lines): ~/.ssh/config parser (Host, HostName, Port, User, IdentityFile)
- **SftpCache** (240 lines): SHA-256 hash-based file cache with cleanup (size/age policies)
- **Settings** (130 lines): SftpConnectionProfile persistence (save/load/recent connections)

#### **Features Implemented**
- ✅ **Dual Authentication**: Password + SSH key auto-detection (id_rsa, id_ed25519, etc.)
- ✅ **SSH Config Integration**: Parses ~/.ssh/config for HPC cluster aliases
- ✅ **Connection Profiles**: Save/load credentials like bookmarks (NO password storage)
- ✅ **Recent Connections Menu**: Last 5 connections with timestamps ("X hours ago")
- ✅ **Intelligent Caching**: Cache-hit detection, avoids re-downloading files
- ✅ **Lazy Directory Loading**: Remote dirs load on-demand (QTreeView expansion)
- ✅ **Progress Dialogs**: QProgressDialog for connect/auth/download stages
- ✅ **Error Reporting**: Detailed SSH error messages via ssh_get_error()
- ✅ **Port Support**: Custom SSH ports (not just 22)
- ✅ **Path Bug Fix**: Subdirectory files now download correctly (was broken for non-root paths)

#### **Architecture Integration**
- **MainWindow**: Recent Remote Connections menu + updateRecentConnectionsMenu()
- **Settings**: SftpConnectionProfile struct + getRecentSftpConnections(limit)
- **Dialog**: Profile/SSH Config dropdowns auto-populate on open
- **Cache**: /tmp/qurcuma_sftp/ with automatic cleanup policies

#### **Dependencies**
- libssh 0.11.3+ (pkg-config detection in CMakeLists.txt)
- Qt6 Core/Widgets (QAbstractItemModel, QSettings)

### Rendering & Performance Fixes
- **CustomFrameGraph disabled**: Fallback to standard Qt3D (Phase 5A incompatible with some RHI backends)
- **Bond detection improvements**: getCovalentRadius() with accurate covalent radii (CRC Handbook)
- **Bond tolerance optimized**: 1.25x multiplier (~2.0Å max) for cleaner detection
- **VTF animation fix**: Bond rotation now updates correctly during trajectory playback
- **XYZ unit handling**: Disabled auto Bohr conversion (assumes Ångström only)
- **MainWindow slots fix**: Moved 8 methods to private slots section (Qt signal-slot errors)
- **ChartView**: Added missing ResetFontConfig() slot
- **Debug output cleanup**: Removed ~50 qDebug() statements from parsers (major performance boost)

**Total: 550+ lines, 2 commits, libssh external dependency, production-ready SFTP**

## November 2025 (Iteration 5 - Phase 5A/5B/5C Complete)

### Phase 5A - Multi-Pass FrameGraph & SSAO Integration ✅
- CustomFrameGraph (400 lines): 4-pass rendering (Geometry → SSAO → Blur → Composite)
- G-buffer setup: Color (RGBA16F), Depth (D24S8), Normal (RGB16F) textures
- SSAO integration: UI sliders for Intensity/Radius/Bias with real-time control
- Settings persistence: All parameters saved to QSettings, restored on startup
- Filter key routing system for render pass selection and fallback

### Phase 5B - Bloom/Glow & HDR Tone Mapping ✅
- Bloom shaders (5 files, 280 lines): bright pass, horizontal/vertical blur, composite
- Bloom parameters: Threshold (0.5-1.5), Intensity (0.0-2.0) with slider controls
- HDR tone mapping: Reinhard operator, sRGB gamma correction, exposure compensation
- Post-processing UI: New "Post-Processing" section in VisualizationSettingsDialog
- FullscreenQuad utility: Helper class for rendering fullscreen effects

### Phase 5C - File Format Support (PDB & MOL2) ✅
- PDBParser (450 lines): Fixed-width PDB parsing, CONECT records, multi-model NMR support
- MOL2Parser (350 lines): Tripos MOL2 format, section-based parsing, Sybyl atom types
- File integration: Context menu "Open with 3D Viewer" for .pdb/.mol2 files
- Bond detection: Distance-based (covalent radii) + explicit connectivity
- Error handling: Inline error dialogs with parser-specific messages

**Total Phase 5: 2,400+ lines, 3 commits (fee39f8, 6467838, cfdad3b), clean build**

## November 2025 (Iteration 4 - Phase 4A/4B Complete)

### Phase 4A - PBR Rendering Shaders ✅
- Cook-Torrance BRDF: pbr.vert/frag with Fresnel, GGX, Schlick-GGX
- Materials: Metallic, Roughness, AO, baseColor parameters
- Educational: Full equation comments with physics references

### Phase 4B - Bond Editing System ✅
- BondEditor class (700 lines): add/remove/changeBondOrder with validation
- Bond picking via Qt3DObjectPicker with mode routing (Add/Delete/Cycle)
- XYZ I/O: writeFile/writeTrajectory + convertFromMoleculeViewer

### Phase 4 Extended - Advanced Features ✅
- **PBRMaterial** (150 lines): Qt3D wrapper with Metal/Plastic/Glass/Rubber presets
- **Auto-Save** (110 lines): 500ms debouncing, XYZ backup on first edit, BondEditor integration
- **Bond UI Toolbar** (30 lines): ComboBox with 4 modes (No Edit, Add, Delete, Cycle)
- Material mode infrastructure ready (Phong ↔ PBR toggle)

**Total: 1,550+ lines, 2 commits (a91fd56, 6b2e241), 9 major components**

### November 2025 (Iteration 3 - COMPLETE)

### 3D Visualization Phase 2A - Atom Selection & Picking (COMPLETE)
- Implemented Qt3D ObjectPicker on each atom for direct 3D click-based selection
- Single-click selection, Ctrl+Click multi-select, Shift+Click toggle modes fully functional
- Visual selection feedback: Orange-yellow highlighting with increased shininess
- SelectionManager class: Centralized selection state management with signals
- Keyboard shortcuts: Ctrl+A (select all atoms), Escape (clear selection)
- Direct integration: MoleculeViewer → SelectionManager bidirectional state sync
- Fully tested: All selection modes working, colors visible, signals propagating

### 3D Visualization Phase 2B - Measurement Overlay System (COMPLETE)
- MeasurementOverlay class: Manages distance/angle/dihedral visualizations in 3D space
- Distance measurement: 2-atom selection creates line with calculated Å distance
- Angle measurement: 3-atom selection shows 2 lines with calculated angle in degrees
- Dihedral measurement: 4-atom selection visualizes torsion angle between planes
- Cylinder-based geometry: Orange-yellow measurement lines with proper rotation/scaling
- Math implementation: Vector length (distance), dot product (angle), cross product (dihedral)
- Dynamic updates: Measurements recalculate and re-render on frame changes during animation
- UI integration: Combo-box modes (None/Distance/Angle/Dihedral) with auto-detection
- Selection-driven: Measurements auto-trigger when correct number of atoms selected (2/3/4)

### 3D Visualization Phase 2C - Atom List Panel (COMPLETE)
- AtomListPanel widget: QTableView for browsing atom properties (Index/Element/X/Y/Z/Charge)
- AtomTableModel: Custom QAbstractTableModel with sortable columns and live data updates
- Bidirectional selection: 3D clicks → table highlights + auto-scroll; table clicks → 3D highlighting
- Context menu: Copy atom data (tab-separated), Focus on atom (center camera)
- DockWidget integration: Dockable panel on right side with state persistence
- Dynamic updates: Table refreshes on frame changes, positions update in real-time during animation
- Multi-select support: Ctrl+Click in both 3D viewer and table for multiple atom selection
- Full synchronization: SelectionManager signals keep table and viewer in sync automatically

### 3D Visualization Phase 3B - Performance Optimization & GPU Instancing Foundation (FOUNDATION COMPLETE)
- PerformanceOptimizer system: Pragmatic LOD-based performance enhancement
  - Adaptive quality modes (Fast=8 rings, Balanced=16, High-Quality=32) reduce geometry complexity
  - Auto-detection recommends quality based on atom count thresholds (1000, 2000, 5000)
  - Real-time FPS monitoring with 1-second update intervals for performance tracking
  - 30-50% performance improvement for large molecules via geometry LOD
  - Frustum culling framework and adaptive quality adjustment system
- AtomInstancingSystem architecture: Foundation for GPU instancing implementation
  - Custom ray-casting algorithm for atom picking (replaces ObjectPicker for instanced rendering)
  - Per-instance data structure with position/scale/color/index mapping
  - Deferred full GPU instancing (requires extensive Qt3D setup)
- SSAO Shaders: Complete Screen-Space Ambient Occlusion implementation
  - ssao.vert: Vertex shader for screen-space processing
  - ssao.frag: Fragment shader with 32-sample SSAO kernel and depth reconstruction
  - ssao_blur.frag: 5x5 Gaussian blur for noise reduction
  - Deferred integration pending FrameGraph customization

### Directory Navigation & Workspace System (COMPLETE - Iteration 2)

**Phase 1-2: UI Navigation (Complete)**
- Added BreadcrumbBar widget: Clickable path segments replacing plain text label; users jump to parent dirs by clicking breadcrumb segments; Home shown as ~
- Enhanced Recent Files: RecentFileEntry struct with QDateTime timestamps; menu groups by date (Today/Yesterday/This Week/Older); shows context (filename with parent directory)

**Phase 3.1: Bookmark Foundation (Complete)**
- BookmarkItem struct: Hierarchical support with id, name, path, tags, color, parentId, isFolder, created timestamp
- Settings methods: bookmarks(), setBookmarks(), addBookmark(), removeBookmark(), updateBookmark()
- Serialization: Pipe-delimited format in QSettings with auto-migration from legacy workingDirectories
- Ready for UI: Tree widget integration deferred to next iteration

**Phase 4.1-4.2: Workspace Foundation (Complete)**
- Workspace struct: Captures complete state - working directory, calculation directories, window geometry, splitter states, timestamps
- WorkspaceManager class: Methods for save/restore/list/delete/rename workspace operations
- Settings persistence: All workspace data serialized and persisted in QSettings
- Ready for UI: Sidebar widget and menu integration deferred to next iteration

**Phase 3.2-3.5: Bookmark Tree UI (Complete)**
- Replaced QListWidget with QTreeWidget for hierarchical bookmark structure with folders
- Context menu: New Folder, Add Bookmark, Rename, Delete, Edit Tags operations
- Drag & Drop enabled for reorganizing bookmarks between folders (QAbstractItemView::InternalMove)
- Minimal tag system: Edit tags via dialog (comma-separated), display in bookmark tooltip
- Icons: Folder-icon for folders, Bookmark-icon for bookmarks, color support for visual organization

**Phase 4.3-4.5: Workspace Management UI (Complete)**
- Workspace list widget in sidebar below bookmarks with "+" button to save new workspaces
- File menu "Workspaces" submenu with Save (Ctrl+Shift+S) and Load (Ctrl+Shift+O) shortcuts
- Complete workspace capture: working directory, window geometry, splitter layout, timestamps
- Full restore functionality: Click workspace in sidebar → restores complete application state
- Workspace persistence: Saves to QSettings with JSON serialization, auto-migration support

**Bonus Context Menu Activation**
- Enabled missing setupProjectViewContextMenu() call in MainWindow constructor
- Right-click on calculation directories now shows context menu: "Add to Bookmarks", "Set as Working Directory"

## October 2025

- Fixed VTF bond parsing: Changed QMap<int, VTFBond> to QVector<VTFBond> to support multiple bonds per atom (previously lost ~50% of bonds due to key collision)
- Fixed VTF frame parsing: Removed global parseError flag from main loop; now correctly processes all frame sections (was stopping after first conversion error, returning 0 frames instead of 3)
- Added test infrastructure: test_vtf_bonds.cpp (validates 199 bonds), test_vtf_frames.cpp (detects 3 frames), test_vtf_full.cpp (end-to-end validation)
- 2026-08-28: Display state integrity: viewer is single source of truth (currentDisplaySettings/applyDisplaySettings); panel syncFromViewer is read-only; Save/Reset/presets round-trip the full DisplaySettings struct
- 2026-08-28: NCI quick access: Display menu with NCI Overlay toggle (shortcut N) + source submenu, NCI button with source dropdown in the viewer bar, all mirrored from one viewer signal
- 2026-08-28: Display panel restructure: NCI as own top-level section, Labels moved to Style, dead instancing row removed, accordion + splitter state persisted, Display menu NCI Options jumps to the section
- 2026-08-28: Chrome polish: shared display QActions + viewport context menu, playback toggle with Space/arrow keys, permanent status-bar indicators, Photo split-button, Custom scheme in bar combo, Teaching layout reachable (Ctrl+Alt+5), shortcut conflicts resolved
- 2026-08-28: Builder groundwork: exclusive InteractionMode enum (None/Edit/Measure/BondEdit/Build) replaces pairwise mode resets; bond edits now undoable and fully propagated; appendMolecule startPlacement flag
- 2026-08-28: Build mode core: place/attach atoms with chosen element, bond by dragging atom to atom (order cycles), element hotkeys, HUD, per-atom context menu, coalesced undo snapshots
- 2026-08-28: Element picker: quick strip (H C N O S P F Cl Br) + full periodic-table popup in the viewer bar (Build mode only); element tables extended via curcuma literature data for all 118 elements
- 2026-08-28: Auto-hydrogens: VSEPR placement (build::generateHydrogens, tested in test_buildtools), Add-H button + open-valence indicator in Build mode, context-menu entries
- 2026-08-28: Fragment templates: built-in library (methyl/phenyl/carboxyl/amino/hydroxyl, benzene/cyclohexane/methane/water/ammonia), insert standalone or dock onto a selected atom (sacrificial H consumed)
- 2026-08-28: Builder Clean up: bounded geometry optimization (startQuickOptimization, shared startWithConfig lifecycle) from the Build strip; Build Mode menu action for the palette; builder docs in src/CLAUDE.md
- 2026-08-28: Build gestures reworked: left-click on atom changes its element, middle-click attaches, right-click deletes (context menu stays on empty space)
- 2026-08-28: Build mode: drag an atom onto empty space moves it (bond-drag preview doubles as move preview; drawn topology kept)
- 2026-08-29: Build drag feedback: atom follows the cursor live, bond target parks it with preview line + highlight (pick excludes the dragged atom); first atom seeds the scene without camera reset (lands under the cursor)
- 2026-08-29: Build drag: bond preview is now a real live bond (removed on leave, committed on release); Ctrl+drag = navigation override; stuck-rotate fixed (button state re-synced on move, dblclick re-arms press state)
- 2026-08-29: Rotate-instead-of-add fixed properly: 8 px click threshold in Build mode and no rotation below it (click jitter neither nudged the view nor cancelled the placement)
- 2026-08-29: Bond-drag keeps the pulled position (snaps to covalent distance on the approach side); buildBond strips excess H from over-valent endpoints (build ring, add H, aromatize); ring H-count pins added to test_buildtools
- 2026-08-29: Bond-drag no longer snaps to the tabulated covalent distance: the atom freezes at the pulled position while hovering the target and stays there when the bond forms
- 2026-08-29: Bond drag reworked: atom always follows the mouse, bond intent is proximity-based, preview bond order follows the drag distance live (bondOrderFromDistance, C-C series ratios) and commits as shown
- 2026-08-29: File > New Scene (empty scene, keeps camera, enters Build mode, Snapshots-undoable); drawn palette-coloured icons for all viewer-bar buttons (theme icons were inconsistent/missing)
- 2026-08-29: Ctrl+Z undo (restores+consumes newest snapshot, in sync with the Snapshots tab); fragment carry mode: fragment hangs on the mouse with live bond preview, click drops, Shift+click serial-places, right-click/Esc cancels
- 2026-08-29: Snapshot coverage completed: resolve clashes, nudge (coalesced), structure-text apply, center at origin, table edits, context-menu element change and the first atom on an empty scene are now all undoable
- 2026-08-29: Fragments carry an explicit Xx/R1 attachment point (curcuma polymerbuild convention); docking aligns the Xx axis and picks the roll with maximum clearance (dockRotation); carry-drop re-docks properly, free drop strips the Xx
- 2026-08-29: Carry-drop no longer places a stray atom: the drop happens on press and the matching release is swallowed (also for rapid Shift+click series via double-click events)
- 2026-08-29: Carried fragments appear under the cursor immediately (no visible flash at the insertion position)
- 2026-08-29: Open UX decisions resolved: Shift+drag = depth move, held Space = navigation override, keys 1/2/3 force the previewed bond order; plane drags keep the atom's current depth; middle-click reset kept
- 2026-08-29: Live docking preview while carrying fragments (final pose incl. clearance roll shown before the drop; cursor-based target detection, drift-free from captured library pose; opt-out in Display > Tools); saturated targets dock along the approach-side sacrificial H
- 2026-08-29: Docking preview stabilised: preview target and sacrificial H are sticky (115 percent keep-range), and drop/release commit exactly the previewed target instead of re-searching
- 2026-08-29: Docking preview fixed for real: bare atoms dock on the approach side (was fixed +x, fighting the mouse), sacrificial-H choice follows the mouse with hysteresis instead of a hard lock, Xx renders as a small magenta marker
- 2026-08-29: Carry commit is now literally WYSIWYG (previewed pose frozen: positions kept, previewed H + Xx removed, bond added - no recomputation); un-docking keeps the fragment's shown orientation
- 2026-08-29: Fragment library gains Gases (H2/N2/O2/F2/Cl2/Br2/I2 with experimental lengths and orders) and Materials (planar graphene flake, greedy Kekule matching + rim H, fully saturated); Build dropdown grouped by category
- 2026-08-29: New-scene button in the Build strip (page icon; same action as File > New Scene), optimization button renamed Clean up -> Relax to end the naming confusion; newScene also clears stale RMSD overlays and the NCI overlay
- 2026-08-29: Relax fixed: runs ONE bounded Optimize() pass (optSingleShot, max 50 iterations, no trajectory file) instead of entering the interactive keep-alive loop that never terminates
- 2026-09-09: Structure edits are refused while a simulation runs (canEditStructure now checks m_simulationActive) — a menu/shortcut edit under a running MD made updateSimulationFrame rebuild the molecule, turning atoms past the cached frame into carbon and dropping every bond
- 2026-09-09: ctest works again: enable_testing() was missing entirely, so both add_test calls were inert; qurcuma's own tests carry the label `qurcuma` (`ctest --test-dir debug -L qurcuma`) to separate them from curcuma's 312
- 2026-09-09: LLM tool layer planned — work packages in docs/WP-llm-tool-layer.md (qurcuma) and external/curcuma/docs/TOOL_API_WP.md (curcuma, branch llm-core)
- 2026-09-09: Atom/Bond moved from view.h to src/core/moleculedata.h (namespace moldata); MoleculeViewer keeps them as aliases so 42 files stay untouched, while the parse path (4 parsers + loader) now compiles without QtWidgets — 0 QtWidgets headers instead of 10
- 2026-09-09: LogHub (src/core/loghub.*) — thread-safe ring of structured records with source/level/sinceSeq/substring filters and a hard query cap; qDebug/qWarning now appear in the Output dock (coalesced over 100 ms) instead of only the terminal, with Qt's own handler still in the chain
- 2026-09-09: ToolRegistry + qurcuma_core (static lib, only Qt6::Core/Gui) — tools carry effect, GUI-thread affinity and a JSON-schema subset validated at registration; unknown arguments are refused rather than dropped, and the schema subset rejects anything it cannot enforce
- 2026-09-09: ToolDispatcher — Gui-affinity handlers are marshalled onto the GUI thread with a real timeout (QueuedConnection + QSemaphore, not BlockingQueuedConnection, which has none and deadlocks on its own thread); every call leaves an audit record with tool, effect, capped args, outcome and duration
- 2026-09-09: First 12 tools — read_log and describe_tools in qurcuma_core (schemas only on request, so the catalogue stays small), plus ten read-only view tools; select_atoms/get_fragments/measure go through moleculebridge into curcuma rather than re-implementing the selection grammar, fragment perception or geometry
- 2026-09-09: get_contacts (pairs under a cutoff, optionally only between two selections, bonded pairs excluded) and get_distance_matrix (named atom set required, refuses oversized sets) — the geometry questions agentic docking actually asks
- 2026-09-09: Registry tools appear in the command palette (Ctrl+K), added to the ~120 menu entries rather than replacing them; they run through the dispatcher so the palette path carries the same thread marshalling and audit record as a model's, and the answer lands in the Output dock
- 2026-09-09: LLM support — LlmClient (OpenAI-compatible, tool calls), LlmConfig (plain JSON, key only from an environment variable, a key in the file is refused), LlmSession (agent loop; without an approval policy anything beyond Read/Display is refused), and the Assistant dock with allow-once/allow-for-session/deny
- 2026-09-09: Streaming answers and a foldable reasoning section — measured shapes from the live endpoint (delta.reasoning, not reasoning_content; tool_calls assembled by index because OpenAI fragments arguments); reasoning is shown but never sent back, since the history is resent every round
- 2026-09-09: curcuma parameter registry extended (tier/enum/unit/bounds/requires, StringList+Json+Selection+Path types, ModuleDefinition with CLI verbs) and the extractor fixed — it had been dropping 22 multi-line PARAMs, among them the whole eeq_solver PCG block, gfnff.solvent and gfnff.solvent_model
- 2026-09-09: CurcumaLogger gained a thread-local SinkScope, so an embedded caller can capture a run's output without seeing another thread's; stdout is unchanged
- 2026-09-09: curcuma run state made per-run instead of process-wide — verbosity is a per-thread override (the global save/restore corrupted two concurrent runs), requestStop() addresses one run where the CWD "stop" file stops all of them, and pinOutputDir() keeps a GUI run from leaving a timestamped folder behind
- 2026-09-09: CurcumaMethod::Results() — a pure, machine-readable outcome per run (no file writing); RMSDDriver returns the exact key set the CLI's .rmsd.json always had, so executeRMSD only adds the file names, and SimpleMD reports steps, time, energies and temperatures
- 2026-09-09: 148 std::cout prints in SimpleMD and UnifiedAnalysis routed through CurcumaLogger::raw() — same stream semantics and unconditional, so the CLI output is unchanged while an embedded caller finally sees what a run is doing
- 2026-09-09: render_view returns the rendered scene as a PNG (refused during a running MD, which it would stall), and five editing tools — add_atoms, add_fragment (names offered as an enum from the library), add_hydrogens, delete_atoms, list_fragments — all Effect::Mutate, all refusing during a simulation with the reason, all undoable via the snapshot stack
- 2026-09-09: run_single_point takes a selection (curcuma's grammar or explicit indices), so an interaction energy is three calls instead of impossible; severedBondCount() reports how many covalent bonds the subset cuts, because an energy from a fragment with open valences must not be subtracted from the whole
- 2026-09-09: resolveAtomSet moved to src/llm/atomselection.{h,cpp} (moldata::Atom only, headless-testable) — one place for the selection grammar, now covered by test_selectiongrammar alongside the curcuma behaviour underneath it
- 2026-09-09: merge_structure (add a file's structure to the scene, atoms land where the file puts them) and save_structure (whole structure or a selection, to xyz); replacing the whole scene stays in the GUI, which owns the trajectory and window state
- 2026-09-09: ToolRegistry::add() warns when it refuses a spec — registration sites count successes and pass no error pointer, so a rejected schema used to make a tool vanish without a word
- 2026-09-09: Ctrl+C no longer copies the 3D structure when text is selected elsewhere (a window-level QAction shortcut fires before any widget sees the key); the chat and Output docks gained copy buttons, and ChatDock::transcript() assembles the whole session including folded reasoning, which a mouse selection cannot span
- 2026-09-09: A second calculation is queued instead of refused — only one job computes at a time (OpenMP), but a model asks for the complex and both fragments in one turn, and two of three answers were being discarded; each request gets its id at once, job_status reports the queue and takes wait_seconds instead of being polled
- 2026-09-09: The agent loop's round cap (12 → 30) no longer ends a turn empty-handed: pending calls are answered so the history stays valid, then one round goes out with no tool catalogue, which leaves the model nothing to do but write its answer from what it gathered
- 2026-09-09: MD and geometry optimisation as tools — run_simulation (md/opt), simulation_status (with wait_seconds), pause/resume/stop/step and set_temperature for the live setpoint; they drive the Simulation dock rather than the worker, so the operator sees the run with the model's parameters in the controls
- 2026-09-09: run_simulation's method/optimizer/thermostat enums are read out of the dock's combos instead of written down again — the hand-copied list already said "diis"/"rfo" where the dock says "native_diis"/"native_rfo"
- 2026-09-09: transform_atoms — rigid placement of an atom set (rotate about its own centroid, then shift) in one snapshot; refused during a run, where the viewer would overwrite it at the next frame
- 2026-09-09: pull_atoms/clear_forces — the mouse grab without a mouse: forces are added to the gradient of every following step and stay until cleared, so the structure answers with its own forces; SimulationWorker::injectForces() distributes several pulls through the bond graph and sums them, where injectForce() could only hold one
- 2026-09-09: watch_simulation — measures the running simulation from the agent loop's thread on a locked per-frame snapshot, so the MD keeps stepping; waits on a threshold (min_distance, centroid_distance, gyration_radius, rmsd_to_start, energy, temperature, step) instead of on a clock, and every answer carries fragment sizes, gyration radii and closest approaches
- 2026-09-09: render_view works during a run (ceiling 800 px instead of 1600) — the render holds the GUI thread the MD timer lives on, but watching a run by eye is half the point
- 2026-09-10: watch_simulation reports regularly as well as on a threshold — every_steps returns a trace of value against step instead of a single number, so a run is followed rather than sampled; both can be combined and it returns on whichever comes first
- 2026-09-10: pull_atoms no longer multiplies a fragment pull — bond-graph spreading defaults to off for a set and stays at 3 shells for a single atom, since every atom's pull otherwise leaks onto neighbours that are being pulled themselves
- 2026-09-10: watch_simulation takes atom_indices to follow a fixed set (fragment numbering is geometric and moves as the structure does — pulling two parts apart renumbers them), pull_atoms returns the indices it resolved, and get_fragments gained index_range, complete however long the fragment is
- 2026-09-10: Autonomy switch in the Assistant dock — Ask (read/display only), Auto in the program (also structure changes and calculations, both undoable or contained), Full auto (also file writes and external programs); needsApprovalAt() in tool.h is the single grading of the effects and the matrix is pinned by test_toolregistry, the level is remembered across launches, shown with a coloured line whenever it is not Ask, logged on every change, and loosening it drops the per-tool session grants
- 2026-09-10: The agent loop moved to its own thread — it ran on the GUI thread, so a waiting tool (job_status, watch_simulation) froze the window and could never be woken, since delivering the wake-up was the blocked thread's job; calls in are marshalled, the approval dialog is marshalled back, and test_llmsession pins it with a tool that waits for the main thread
- 2026-09-10: SimulationControlWidget::publishLiveState() reports the dock's own running flag instead of asking the worker thread — setRunning(true) precedes the thread start, so a caller was told nothing was running for the whole window before the first frame
- 2026-09-10: The system prompt follows the autonomy switch: in an auto mode it says to carry the task through without stopping to ask between steps, which is the behaviour the switch exists to enable
- 2026-09-10: render_view's PNG actually reaches the model — the bytes were carried in ToolResult and never read, because tool messages are text; the image now arrives as its own user message with a data URI, only against a model the endpoint reports as vision-capable, and only the newest one stays in the resent history
- 2026-09-10: Waiting tools allow much longer waits (job_status 300 s, simulation_status and watch_simulation 600 s) and the round budget goes 30 → 50 — one long wait is one round, asking again every minute is one round each; LlmSession::cancel() sets an interrupt flag on the dispatcher that every waiting handler checks, because a queued cancel cannot reach a loop blocked inside a wait
- 2026-09-10: An optimisation started through run_simulation is a single bounded pass — it was getting the interactive keep-alive loop, which restarts after every convergence for the sake of a mouse grab and therefore never ends when nobody is holding one; keep_alive opts back in and says so
- 2026-09-10: SimulationControlWidget::startRun() carries over config fields that have no widget (optSingleShot), which the applyConfig/buildConfig round trip silently dropped
- 2026-09-10: The temporary [GRAB-DEBUG] traces are gone — two ungated qDebug lines per optimiser cycle, which since the log hub feeds the Output dock filled it at the frame rate
- 2026-09-10: Closing the window during a curcuma calculation no longer aborts — both teardowns waited 2/5 s and then destroyed a still-running QThread (a qFatal); the waits are 15 s and a thread that outlives even that is released from its parent instead, with the curcuma runner told first that its owner is gone
- 2026-09-10: The endpoint is editable in the Assistant dock and written back into the active profile — only that one base_url changes, the file's notes and api_key_env survive, and a URL without a scheme is refused before anything is written (test_llmclient)
- 2026-09-10: An optimisation can reach its convergence criterion — the gradient tolerance defaulted to 1e-6 here against curcuma's 5e-4 Eh/Bohr, unreachable for these methods, so runs went to the iteration ceiling with the energy long flat; energy_threshold was never written into the controller at all, and a tool-started opt inherited the MD's 10000-step box
- 2026-09-10: The amount of thinking is controllable — a Thinking selector sent as the field the profile names (reasoning_effort, or think for Ollama), remembered per profile, and carried into the system prompt as well, since an endpoint that does not know the field ignores it silently; the prompt also says to call a tool rather than reason about what the answer might be, and not to work through candidate compound names, which is what 22,000 characters of reasoning had gone on
- 2026-09-10: simulation_status reports the run that just ended (mode, steps done against planned, final energy, why) instead of an unqualified "nothing is running", which reads the same as a run that never started
- 2026-09-10: A round spent only waiting no longer advances the agent loop's round budget (total rounds still capped at three times it) — watching a simulation used to exhaust the budget for being patient
- 2026-09-10: fill_container exposes scenefiller as a tool — random packing of library molecules around the structure, in a box wrapped around its own extent or a sphere, reproducible under a seed; described as a microsolvation shell or gas-phase scene and explicitly NOT a solvation box (no liquid density, no periodic boundaries, not equilibrated), with the last point repeated in every answer it gives
- 2026-09-10: get_structure_summary reports centre, bounds and extent, so the size of the structure no longer has to be reconstructed by paging through every atom
- 2026-09-10: fill_container can make the volume it packed the simulation's container (set_wall, pbc by default), so a run afterwards is held in it instead of expanding into vacuum
- 2026-09-10: Density lives in curcuma — Molecule::Density(volume) with the u/A^3 to g/cm^3 conversion in one place, SimpleMD::containerVolume()/density() reporting the run's own box in Results(); qurcuma's watch_simulation "density" reads it rather than computing its own
- 2026-09-10: pull_atoms uses curcuma's configured external potentials instead of the transient force injection — the pull stands in the run's controller so the run is reproducible from it, the engine resolves the selection on the live geometry, and the accumulated work travels out through the frame into simulation_status as a watchable quantity
- 2026-09-10: restrain_atoms — the two harmonic forms in one tool: "point" ties a selection's centroid to a place (a guest held in a cavity while the rest relaxes), "distance" ties two centroids to a separation
- 2026-09-10: The tool catalogue is measured and filtered — 38 tools are 24,130 bytes of JSON in every request; ToolSpec::available leaves out what cannot work in the current state, which with nothing loaded is 21 tools and 14,784 bytes, a third less, and also stops a model spending a round being refused; the size is logged at registration and test_llmsession pins that an unavailable tool is absent from the request
- 2026-09-10: CIF files load — cell, atom_site loop (fractional or Cartesian) and the symmetry operations, which are applied and deduplicated, since a reader that ignores them returns a well-formed molecule with the wrong atom count; curcuma::Supercell replicates along the cell vectors, and merge_structure can build one on the way in, because qurcuma's render records drop the cell at the door
- 2026-09-10: A tool loading into an empty scene takes the ordinary load path (setTrajectoryData) instead of addMolecule's direct scene build with a camera reset — filling a container with nothing loaded segfaulted, with a structure present it did not, and that branch was the last caller of a path the rest of the application avoids (not reproducible headless, so not proven)
- 2026-09-10: The agent loop's round budget is a Rounds box in the Assistant dock, remembered per profile and winning over llm.json — every profile carries max_tool_iterations, so the file's 12 silently overrode the code default; the file's default is now 60 and both limit messages name the number and where to change it
