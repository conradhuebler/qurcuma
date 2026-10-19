# WP — Tech-Debt-Aufräumen: GUI, Docks, Configuration (Detailplan)

> **Status:** Phase 1–3 + 4.1 sowie **T1, T2, T3, T4** sind umgesetzt und committet (auf `main`); der
> `setupConnections()`-Split + tote-Includes-Cleanup (`6a56637`) sind auch durch. **Verbleibend: nur noch der
> Operator-Laufzeit-Check** (T5 `LessonEditorPanel` ist optional/niedrig, durch T4 subsumiert). `mainwindow.cpp`
> **4490 Z.** (Start 5285), `setupUI()` **182 Z.** T3 + T4 sind code-seitig fertig, aber **nicht per Operator-Lauf verifiziert**.
> **Zweck:** Die über viele Feature-Iterationen (Quick3D-Migration, Dock-Rewrite, Lessons, RMSD,
> interaktive MD, SFTP) angehäuften strukturellen Schulden in den Kernbereichen **GUI**, **Docks**
> und **Configuration** gezielt abbauen — **ohne Rewrite**. Der Render-Kern (`view`/`scenecontroller`)
> ist sauber und bleibt unangetastet.
> **Analyse-Grundlage:** `~/.claude/plans/technische-schulden-sollten-sich-swift-cook.md`.

## 1. Grundprinzipien
- **Inkrementell**: ein Arbeitspaket = ein baubarer, verhaltensneutraler Commit.
- **Build grün** nach jedem Schritt: `cmake --build debug 2>&1 | tail -20` (0 Warnings/Errors).
  IDE/clangd-Diagnosen sind in diesem Repo unzuverlässig (Flag `-mno-direct-extern-access`) → **cmake ist die Wahrheit**.
- **Rückwärtskompatibel**: persistierte QSettings-Keys und Dock-`objectName`s **nicht** ändern
  (Ausnahme war der bewusst autorisierte JSON-Reset der Record-Listen, bereits erfolgt).
- **Extraktionen streng verhaltensneutral**: Signal-Verdrahtung 1:1 erhalten, kein Feature-Umbau.
- Nach jedem Paket **Operator-Sicht-Check** — Qt/3D rendert im Agenten-Bash nicht.

## 2. Bereits erledigt (Kontext)
| Phase | Inhalt | Commit |
|---|---|---|
| 1 | VTFParser-Leak + `stepOnce()`-Config-Verlust (2 echte Bugs) | `d97c3e8` |
| 2a | Geteilte curcuma-Controller-Builder (keine Divergenz `startMD`/`stepOnce`/`runOptimization`) | `c90eed1` |
| 2b/2c | QSettings-Key-Konstanten; toter V1-`recentFiles`; „Quranuma"-Tippfehler | `4b8884c` |
| 2d | Record-Listen (bookmarks/workspaces/sftp/mounts/recentFilesV2) als **JSON** (einmaliger Reset, no-backward) | `b6be2dd` |
| 2e | `src/displaysettings.h` Basis für VisualizationSettings+ViewPreset; Preset-Datenverlust behoben | `1bc91bc` |
| 3.1 | Output-Log über `OutputDock`-API (kein geernteter Pointer mehr) | `69d119a` |
| 3.3/3.5 | Daten-getriebene `PresetSpec`-Tabelle; tote `navigationTabs()`/Fallbacks raus | `f0405e9` |
| 3.4 | Layout-Persistenz in `DockManager::saveLayout/restoreSavedLayout` gebündelt | `ff3f52b` |
| 3.2 | `SnapshotsWidget`-Signale über `SimulationDock` re-emittiert (bounded) | `8365e7e` |
| 4.1 | 25 `SIGNAL/SLOT` → Functor-`QShortcut`; toter Stylesheet-Block raus | `d97e41e` |
| **T2** | **`MoleculeFileLoader`** — 5 Parser-Dispatch-Stellen vereint, tote Member-Parser raus | `bb3e385` |
| **T1** | 3 größte `setupUI`-Gruppen (MD/RMSD-MTD/Wall) extrahiert | `ea76d09` |
| **T1** | restliche 7 Gruppen extrahiert → `setupUI` 954→336 Z. (alle 10 Gruppen = `createXGroup()`) | `1e2fb05` |
| **T3** | toter Code raus (`saveCalculationInfo`/`loadCalculationInfo`/`checkProgramPath`) | `3e29a96` |
| **T3** | `CalculationRunner` — Prozess-Orchestrierung aus MainWindow (`CalculationRequest` + Signale) | `600efc2` |
| **T4** | `LessonController` — OER-Lesson-Feature aus MainWindow (Kollaborateur-Injektion + Signale) | `dabd0d4` |

---

## 3. Verbleibende Arbeitspakete

### T1 — `SimulationControlWidget::setupUI()` splitten — **erledigt** (`ea76d09`, `1e2fb05`, `6a56637`)
- Alle 10 QGroupBox-Sektionen in `createXGroup()`-Methoden ausgelagert; danach der ~160-Z.-„Connections"-Block
  in `setupConnections()` (`6a56637`, verhaltensneutral 1:1 umschlossen). `setupUI` 954→336→**182 Z.** (nur
  Komposition: Header-Controls + 10 Create-Calls + `setupConnections()`). Zusätzlich in `6a56637`: zwei durch
  T4 verwaiste tote Includes raus (`dialogs/lessonmetadatadialog.h`, `lessonstructuremodel.h` + der
  `LessonStructureModel`-Forward-Decl in `mainwindow.h`).
- **Vorgehen:** Pro `QGroupBox` eine private `createXGroup()`-Methode, die die Section baut, die
  `m_*`-Member weiterhin setzt (sie werden in `buildConfig`/`applyConfig` genutzt) und das Widget
  zurückgibt; `setupUI()` ruft sie nur noch der Reihe nach auf. Natürliche Nähte: MD-Parameter,
  Thermostat, Confinement-Walls, RMSD-MTD, Temperatur-Rampe/Regionen, Snapshots-Stride.
- **Muster im Repo:** `DisplayPanel::createAppearanceGroup/createToolsGroup/createLightingGroup`
  (`displaypanel.cpp`) ist die Vorlage.
- **Risiko:** Reihenfolge-/Layout-Abhängigkeiten zwischen Sections; ohne Sicht-Test heikel →
  Section für Section extrahieren, nach jedem Schritt bauen.
- **Fertig wenn:** `setupUI()` < ~80 Zeilen (nur Komposition), jede Gruppe eine Methode,
  Build grün, UI identisch (Operator-Check).

### T2 — `MoleculeFileLoader` extrahieren *(hoch; höchster Nutzen)* — **erledigt** (`bb3e385`)
- **Ergebnis:** `src/moleculefileloader.{h,cpp}` mit `static Result load(path)`; **5** Dispatch-Stellen vereint (parseFirstFrame, loadMoleculeFile, downloadAndLoadRemoteFile, PDB- + MOL2-Kontextmenü); tote `m_xyzParser`/`m_vtfParser`-Member entfernt (VTF-Leak-Fläche vollständig entfernt). `mainwindow.cpp` −183 Z.
- **Problem (Ausgangslage):** Das `suffix == "xyz"/"vtf"/"pdb"/"mol2"`-Dispatch war **mehrfach kopiert**:
  `parseFirstFrame` (`mainwindow.cpp:3367`), `loadMoleculeFile` (`:4185`), `downloadAndLoadRemoteFile`
  (`:4587`), Kontextmenü-Gating (`setupContextMenu`, `:373`). Innerhalb `loadMoleculeFile` sind XYZ-
  und VTF-Zweig strukturell identisch.
- **Vorgehen:** Neue `src/moleculefileloader.{h,cpp}` mit `parse(path) -> Trajectory` (Atoms/Bonds
  pro Frame + FrameCount + optionaler Datei-Text). Besitzt die Parser-Instanzen (XYZ/VTF/PDB/MOL2);
  ersetzt die 4 Dispatch-Stellen. Natürlicher Ort auch für den VTF-Leak-Fix aus Phase 1.
- **Fertig wenn:** Genau **eine** Format-Weiche im Code; `loadMoleculeFile`/`parseFirstFrame`/
  `downloadAndLoadRemoteFile` rufen den Loader; Laden aller vier Formate funktioniert (Operator-Check);
  `CMakeLists.txt` um die neue Datei ergänzt.

### T3 — `CalculationRunner` extrahieren *(hoch)* — **erledigt** (Model `4787f58`; Orchestrierung `3e29a96` + `600efc2`)
- **Model-Layer** (`4787f58`): `CalculationEntry` + `calculations.json`-Persistenz → `src/calculationhistory.{h,cpp}`.
- **Toter Code entfernt** (`3e29a96`): `saveCalculationInfo`/`loadCalculationInfo` (schrieben ein nie gelesenes
  `calculation.json`-Sidecar) + `checkProgramPath` — keine Aufrufer.
- **Orchestrierung** (`600efc2`): neue `src/calculationrunner.{h,cpp}`. `MainWindow` validiert die Eingaben und
  baut eine **`CalculationRequest`** (Widgets + Settings); `CalculationRunner` **besitzt den `QProcess`** (+ die
  Completer-Kommandos), schreibt Struktur-/Input-Datei, baut die Args pro Programm (curcuma/orca/xtb, inkl.
  xtb-`xtbopt`→trj-Rename + ORCA-`.xyz`-Copy) und meldet per Signalen zurück: `outputReceived/errorReceived`
  → `OutputDock`; `finished(entry,exitCode)` → `MainWindow::onCalculationFinished` (History-Status, Output-View,
  Workflow-State, Progress-Dialog, Timer, Cursor). `start()` liefert den „started"-Entry synchron zurück.
  `orcaPlotVib`/`openWithVisualizer` nutzen jetzt ein **lokales** `QProcess` (borgten vorher `m_currentProcess`).
  `runSimulation` 217→~75 Z.; `mainwindow.cpp` 5039→**4830** Z.
- **Offen: Operator-Laufzeit-Check** (Qt rendert nicht im Agenten-Bash) — echter curcuma/xtb/orca-Lauf,
  Cancel, Live-Log, Calculation-History. Siehe §5.

### T4 — `LessonController` extrahieren *(hoch)* — **erledigt** (`dabd0d4`)
- **Ergebnis:** OER-Lesson-Workflow (13 Methoden + ~15 `m_lesson*`-Member + die Inline-Metadaten-/Detail-
  Editor-Lambdas) → neue `src/lessoncontroller.{h,cpp}`. Der Controller **besitzt** `Lesson`/`m_lessonFilePath`/
  `LessonStructureModel`/Browse-Modus und verdrahtet die Metadaten-/Struktur-Editoren selbst.
- **Kollaborateur-Injektion** (kein echtes Entkoppeln, aber gebündelt): Viewer / Sim-Widget / Content-View +
  FS-Model / ProjectDock-Lesson-Widgets werden nach dem Dock-Aufbau einmal injiziert; MainWindow-globale
  Aktionen laufen über **Signale** (`workingDirectoryChangeRequested`→`switchWorkingDirectory`,
  `windowTitleChangeRequested`→`setWindowTitle`, `directoryContentRefreshRequested`→`updateDirectoryContent`,
  `statusMessage`→Statusbar, `inMemoryStructureLoaded`→`onLessonStructureLoaded`). Datei-Parsing direkt über
  `MoleculeFileLoader` (keine `MainWindow::parseFirstFrame`-Abhängigkeit). Alle Call-Sites (File▸Lesson-Menü,
  Kontextmenü + Klick, Drag&Drop, `applyConditions`-Hook) delegieren. MainWindow behält nur die zwei
  Files|Lesson-Buttons (eventFilter-Drop-Target). `mainwindow.cpp` 4830→**4490** Z.
- **Offen: Operator-Laufzeit-Check** — Lesson öffnen/speichern/Struktur hinzufügen (Menü + Drag&Drop)/laden,
  Files|Lesson-Umschalten, Metadaten-/Detail-Editor, `applyConditions` beim Laden. Siehe §5.

### T5 — *(optional, weitgehend subsumiert)* `LessonEditorPanel` *(niedrig)*
- Nach T4 konsumiert der `LessonController` die 10 ProjectDock-Lesson-Getter einmalig bei der Injektion. Ein
  eigenständiges `LessonEditorPanel`, das die Widgets **selbst besitzt** (statt ProjectDock), wäre noch etwas
  sauberer, ist aber optional — der Kopplungs-Nutzen ist mit T4 weitgehend gehoben.

### T6 — *(bewusst zurückgestellt)* Vollständige Dock-Entkopplung
- Das Signal-Re-Emit allein (3.2) ist **kosmetisch**: MainWindow ruft weiterhin Methoden auf den
  geernteten Pointern (`m_rmsdWidget`, `m_simulationControlWidget`, …) auf. Echte Entkopplung entsteht
  erst, wenn die zugehörige **Logik in die Wrapper wandert** (Teil von T3/T4-Denke). RMSD-Signale
  nicht re-emittieren: sie tragen `MoleculeViewer`-Typen → würde `view.h` in den Dock-Header ziehen
  (Kopplungs-Regression). **Nur umsetzen, wenn Logik ohnehin in den Dock verschoben wird.**

---

## 4. Empfohlene Reihenfolge
1. **T2 `MoleculeFileLoader`** — höchster Nutzen, klar abgegrenzt, behebt zugleich das Parser-Duplikat.
2. **T1 `setupUI`-Split** — mechanisch, gut zwischendurch.
3. **T3 `CalculationRunner`** — kohäsiv, `#ifdef`-arm.
4. **T5 `LessonEditorPanel`** → **T4 `LessonController`** (T5 zuerst entlastet ProjectDock).
- T2 + T3 + T4 (+ Dead-Code) haben `mainwindow.cpp` von ~5.285 auf **4490** Z. gesenkt — WP-Zeilenziel erreicht.

## 5. Verifikation (jeweils Operator, da kein Agent-Rendering)
- **T1:** Simulation-Dock optisch/funktional identisch; alle Parameter setz- und lesbar.
- **T2:** xyz/vtf/pdb/mol2 laden (lokal + SFTP); VTF mehrfach → kein Speicherwachstum.
- **T3:** Berechnung starten/abbrechen; Output-Log; Calculation-History.
- **T4:** Lesson öffnen (entpackt + Titel/Statusbar) / speichern (Save + Save As) / Struktur hinzufügen
  (Menü **und** Drag&Drop auf den Lesson-Button) / im Lesson-Modus laden + Remove; Files|Lesson-Umschalten;
  Metadaten- + Per-Struktur-Detail-Editor schreiben zurück; `applyConditions` beim Laden einer entpackten `.xyz`.
- **Regressionsbasis:** die 5 Laufzeit-Checks aus dem Session-Abschluss (VTF-Reload, MD Step vs. Run,
  Settings/Presets/Bookmarks, Layout-Presets/Output, Shortcuts).

## 6. Definition of Done (Gesamt-WP)
- `mainwindow.cpp` deutlich unter 5.000 Zeilen; keine 4-fache Format-Weiche mehr; `setupUI()` < 80 Zeilen.
- Alle Extraktionen als eigene `src/*controller`/`*loader`-Dateien, Signal-gekoppelt.
- Build grün, keine neuen Warnungen; Operator-Checks bestanden.
