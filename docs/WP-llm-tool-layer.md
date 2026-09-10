# WP — LLM-Werkzeugschicht: Tool-Registry, Logs, curcuma in-process

> **Status (09.09.2026):** Phase 0 und Phase 1 sind umgesetzt (WP0.1–0.3, WP1.1–1.4), Phase 2
> ist in Arbeit. Branch: `feature/llm-tools`. Abschnitt 2 („Ausgangslage") beschreibt den Stand
> **vor** dieser Arbeit und bleibt als Begründung stehen; was inzwischen existiert, steht in den
> Arbeitspaket-Tabellen. Alles ohne Erledigt-Vermerk ist geplant, nicht vorhanden.
> Die curcuma-Seite hat ein eigenes Dokument: `external/curcuma/docs/TOOL_API_WP.md`
> (Branch `llm-core`, Integration über `reactff2-llm`).
> **Zweck:** LLM-Unterstützung bei Analyse und Darstellung über OpenAI-kompatible Endpunkte,
> und zwar so, dass praktisch alles, was qurcuma und curcuma können, als Werkzeug aufrufbar
> wird — einschließlich der Logs.

## 1. Warum das ein Umbau ist und kein Chatfenster

Der Aufwand liegt nicht beim LLM-Client. Er liegt darin, dass es **keine programmatische
Aufrufebene** gibt: qurcumas Funktionalität hängt an Widget-Slots in `MainWindow`, curcumas
Fähigkeiten an einer Dispatch-Tabelle in dessen `main.cpp`. Ein LLM-Werkzeug ist nichts
anderes als „benannte Operation mit typisierten Parametern und strukturiertem Ergebnis".
Genau das fehlt an beiden Enden.

Deshalb entsteht **eine** Registry mit mehreren Konsumenten: dem Chat-Dock, der bestehenden
Command-Palette und headless-Tests. Derselbe Umbau trägt Skriptbarkeit und Makro-Aufzeichnung.

Operator-Entscheidungen:
- qurcuma ↔ curcuma läuft künftig über die **direkte In-Prozess-Integration**. Der
  QProcess-Weg bleibt für ORCA und xtb, wird für curcuma aber nicht ausgebaut.
- Freigaben: Lesen und Darstellen ohne Rückfrage, Rechnen/Schreiben/Strukturänderung mit.
- API-Key ausschließlich aus einer Umgebungsvariablen; Endpunkt und Modell in einer Datei
  ohne Key.

## 2. Ausgangslage (in dieser Sitzung an der Quelle gemessen)

### Was fehlt
- **Keine parametrisierte Aufrufebene.** `CommandPalette::Command`
  (`src/widgets/commandpalette.h:19-25`) ist ein `std::function<void()>` *ohne* Parameter, aus
  den Menüs gescrapt (`mainwindow.cpp:649-710`). ~109 `addAction`-Aufrufe. Kein
  `Q_INVOKABLE`, keine Skript-Engine, kein `QUndoStack` (Undo läuft über
  Ganzgeometrie-Snapshots, `mainwindow.cpp:3078`).
- **Kein zentrales Dokumentobjekt.** Molekül, Trajektorie, Frame und Auswahl liegen im
  `MoleculeViewer` (`view.h:736-770`), die Kamera im `SceneController`, Dateipfade, Snapshots
  und Sim-Konfiguration in `MainWindow` (`mainwindow.h:350-473`). `MainWindow` (5205 Zeilen)
  ist das einzige Objekt, das alles sieht.
- **Kein Netzwerk-Stack.** `Qt6::Network` ist nicht eingebunden (`CMakeLists.txt:33-42`).
- **Zwei ctest-Ziele** (`measurements`, `scenefiller`). Die Testinfrastruktur wächst mit.
- **curcumas In-Prozess-Ausgabe wird verworfen.** qurcuma setzt in jedem selbst gebauten
  Controller `verbosity = 0` (`simulationworker.cpp:164`, `:192`, `:206`), weil es die Ausgabe
  nirgends hinlegen kann. Deshalb musste `SimpleMD::stopReason()` nachgerüstet und durch
  `finished(reason, aborted)` geschleust werden; `mainwindow.cpp:4534-4536` sagt es offen.

### Was schon werkzeugförmig ist und wiederverwendet wird
`MoleculeFileLoader::load()` (`moleculefileloader.h:16`), `atomsToXyz()`/`xyzToAtoms()`
(`lesson.h:94`, `:99`), `atomsToMolecule()`/`moleculeToAtoms()` (`moleculebridge.h`),
`measure::` (`measurements.h`), `DisplaySettings` (`displaysettings.h:19`), `ViewPreset`
(`viewpreset.h:37`), `MoleculeViewer::exportImage()` (`view.h:257`), die ~120
parametrisierten Setter der Viewer-Slot-Fläche (`view.h:236-652`) und die thread-sicheren
`SimulationWorker`-Slots (`simulationworker.h:202-236`).

### Ein erreichbarer Datenverlust
`canEditStructure()` ist `{ return m_frameCount <= 1; }` (`view.h:363`) und prüft den
Simulationszustand **nicht**. `m_simulationActive` sperrt nur die Maus-Handler (18 Stellen in
`view.cpp`). `MoleculeViewer::updateSimulationFrame` (`view.cpp:1858`) behandelt eine
abweichende Atomzahl als Topologieänderung und baut das Molekül aus dem Worker-Frame neu auf:
Atome jenseits des Caches bekommen Element `"C"`, und `addMolecule(atoms, {})` verwirft
**alle** Bindungen. Ein Lösch- oder Einfüge-Aufruf während laufender MD lässt die Atome also
als bindungslosen Kohlenstoff wiederauftauchen. Heute unerreichbar, weil nur die Maus
mutieren kann. Die Registry macht es erreichbar, deshalb ist WP0.1 das erste Paket.

## 3. Was von curcuma wiederverwendbar ist — und was nicht

Die naheliegende Idee, qurcumas `Atom`/`Bond` durch `curcuma::Molecule` zu ersetzen, trägt
nicht: **curcumas Typen sind für die Darstellung ärmer.**

| | qurcuma | curcuma |
|---|---|---|
| Bindung | `Bond { atom1, atom2, bondOrder }` (`view.h:96`) | `vector<pair<int,int>>` (`molecule.h:444`), **keine Bindungsordnung** |
| Atom | `Atom { position, element, charge, radius, type }` (`view.h:87`) | `Mol { m_geometry, m_atoms, m_partial_charges }` (`global.h:69`), **kein radius/type/name** |
| VTF | keyword-basiert, trägt Bead-`radius`/`type`/`name` (`vtfparser.h:16`) | `Files::VTF2Mol` liefert nur Elemente und Koordinaten |

Die Bindungsordnung trägt den Builder (aromatische Ringe, Kekulé-Abgleich) und das
Rendering, `radius`/`type` die grobkörnigen VTF-Beads. **Render-Typen und Parser bleiben bei
qurcuma.**

Der Gewinn liegt in der **Analyse**, also genau dort, wo die Werkzeugschicht ihn braucht.
Schon genutzt: `Elements::`, `GeometryTools`/`RMSDFunctions` (`measurements.cpp`, laut Header
bewusst „keine zweite Implementierung"), `Topology::FindRings`. Noch ungenutzt und lohnend:
- `Molecule::FragString2Indicies()` (`molecule.h:246`): curcumas Auswahl-Grammatik
  (`"1:10,15"`, `"F2"`, `"-1"`). Als Auswahlwerkzeug für ein Modell deutlich besser als ein
  200-elementiges Indexfeld.
- `getConnectivtiy()` / `GetFragments()`, `GyrationRadius`, `EndToEndDistance`,
  `CalculateDipoleMoment`, Distanzmatrizen, `MapHydrogenBonds`, jeweils mit PBC-Varianten.

`moleculebridge.h` wird damit zur **schmalen Taille**: Render-Typen bleiben qurcumas, jede
Analyse läuft über eine konvertierte `curcuma::Molecule`. Das Muster verfolgt
`measurements.cpp` bereits.

## 4. Der Werkzeug-Kontrakt

```cpp
enum class Effect   { Read, Display, Mutate, Compute, FileWrite, Process };
enum class Affinity { Gui, Any };

struct ToolSpec {
    QString name, description, category;
    QJsonObject paramSchema;   // type/properties/required/enum; unbekannte Schlüssel abgelehnt
    Effect   effect;           // steuert die Rückfragepolitik
    Affinity affinity;         // Gui = muss auf dem GUI-Thread laufen
    std::function<ToolResult(const QJsonObject&)> handler;
};

struct ToolResult { bool ok; QJsonObject data; QString text;
                    QByteArray image; bool truncated; QString error; };
```

Drei Felder müssen **im ersten Paket** stehen, weil Nachrüsten jede Registrierungsstelle
anfasst:
- **`Affinity`** — die Agentenschleife läuft nicht auf dem GUI-Thread, Viewer-, Auswahl-,
  Kamera- und Render-Werkzeuge müssen es. Ohne Affinität ist der erste `select_atoms`-Aufruf
  ein Datenrennen auf `MoleculeViewer::m_trajectoryAtoms`.
- **`image` / `truncated` in `ToolResult`** — für das PNG aus `render_view` und die harten
  Obergrenzen von `read_log`.
- **`Effect`** — setzt die Freigabepolitik um, mit „für diese Sitzung merken" pro Werkzeug.

Die Schema-Prüfung sind ~80 Zeilen gegen `QJsonObject` (Typen, `required`, `enum`, Ablehnung
unbekannter Schlüssel). Keine JSON-Schema-Bibliothek: der Build kämpft schon mit FetchContent
und `Qt6GuiPrivate`.

**„GUI-frei" gilt für die Registry, nicht für jedes Werkzeug.** Registry, Dispatcher, Prüfung
und `LogHub` sind QtCore-only und headless testbar. Die Anzeige-Werkzeuge fangen
`MainWindow*` und `MoleculeViewer*` und liegen in `src/llm/tools_gui.cpp`. Das ist **keine
Entkopplung** — die rohen Widget-Zeiger in `MainWindow` bleiben unangetastet (T6 aus
`WP-tech-debt-cleanup.md` bleibt zurückgestellt). Der Gewinn ist die einheitliche, testbare
Aufruf- und Freigabeebene.

**Logs und Audit-Spur.** `LogHub` ist ein strukturierter Ringpuffer (Zeitstempel, Quelle,
Level, Text, optional `job_id`), gespeist aus `qInstallMessageHandler`, curcumas Logger-Senke
und den QProcess-Pipes der externen Programme. Der `OutputDock` wird eine Ansicht darauf.
`read_log(...)` hat harte Obergrenzen; eine MD-Logdatei darf nie ungefiltert in den Kontext.
**Jeder Werkzeugaufruf wird protokolliert** (Name, Argumente, Effekt, freigegeben, Ergebnis).
In einem wissenschaftlichen Werkzeug, in dem ein Modell Strukturen verändern kann, ist „was
hat es mit meinem Molekül gemacht" eine Reproduzierbarkeitsfrage.

## 5. Arbeitspakete

Ein Paket = ein baubarer Commit, Build grün nach jedem Schritt.
**Phase 0 bis 2 kommen ohne jede curcuma-Änderung aus.**

### Phase 0 — Sicherheit und Verkabelung (auch ohne LLM nützlich)

| WP | Inhalt | Fertig, wenn | Status |
|---|---|---|---|
| **0.1** | `canEditStructure()` (`view.h:363`) liefert zusätzlich `false`, solange `m_simulationActive` gilt; die mutierenden Viewer-APIs kehren früh zurück (Abschnitt 2). | GUI-verhaltensneutral, da die Mauspfade bereits über dieselbe Flagge gesperrt sind. Operator-Sichtprüfung. | **erledigt** (`1ea0317`) |
| **0.2** | `MoleculeViewer::Atom`/`::Bond` nach `src/core/moleculedata.h`; in `view.h` bleiben Typaliase. `MoleculeFileLoader` zieht heute `view.h` (968 Zeilen, `QWidget`) allein wegen dieser Structs herein. | Build grün; 42 Dateien behalten `MoleculeViewer::Atom` unverändert (der Alias fängt sie ab). Der Parse-Pfad (4 Parser + Loader, 10 Dateien) ist echt umgestellt: eine TU mit nur `moleculefileloader.h` übersetzt mit Qt6Core+Qt6Gui allein und zieht **0** QtWidgets-Header, wo `view.h` **10** einzieht. | **erledigt** (`b71baf5`) |
| **0.3** | `src/core/loghub.{h,cpp}` (QtCore-only) + `qInstallMessageHandler`; `OutputDock` wird Abonnent. | `test_loghub` grün; qurcumas `qDebug`-Meldungen erscheinen im Output-Dock statt nur im Terminal. | **erledigt** (`2c24a2c`) |

Beim Umsetzen von 0.1 kam zweierlei dazu. Der Spiegel in den Struktur-Texteditor ist ein
*Lesen* und hing trotzdem an `canEditStructure()`; er gatet jetzt auf die Frame-Zahl, weil die
Auffrischung nach Laufende aus `setupConnections()` **vor** dem Löschen der Simulationsflagge
in `createDockWidgets()` zugestellt wird — die gemeinsame Bedingung hätte den Editor sonst auf
der Geometrie von vor dem Lauf stehen lassen. Und `enable_testing()` fehlte im obersten
CMake-File, die beiden `add_test`-Aufrufe waren also wirkungslos (`639370e`); qurcumas eigene
Tests tragen jetzt das Label `qurcuma`, weil curcuma über `add_subdirectory(test_cases)` 312
weitere registriert, deren Binaries hier nicht gebaut werden.

**Noch offen als kleine Folgearbeit:** `setSimulationActive()` sendet kein Signal, die
Build-Leiste kann also nicht ausgrauen und lehnt nur ab; `pasteClipboard` und `deleteSelection`
lehnen stumm ab (das taten sie für Trajektorien schon vorher).

#### WP0.3 im Einzelnen — **erledigt** (`2c24a2c`)

| # | Schritt | Status |
|---|---|---|
| a | `LogHub`-Header: `LogRecord {seq, ts, source, level, text, jobId}`, Ringpuffer mit fester Kapazität, `Query {source, minLevel, sinceSeq, grep, limit}`, harte Obergrenze | erledigt |
| b | Implementierung, **thread-sicher** (`qDebug` kommt auch aus dem `SimulationWorker`-Thread), Sequenznummer bleibt über den Umlauf hinweg monoton, verworfene Datensätze werden gezählt | erledigt |
| c | `test_loghub` (ctest, Label `qurcuma`): Umlauf, Filter nach Quelle/Level/`sinceSeq`/Muster, Obergrenze greift, nebenläufiges Anhängen | erledigt |
| d | `qInstallMessageHandler`-Brücke in `main.cpp`, **mit Weitergabe an den vorherigen Handler**, damit die Terminalausgabe erhalten bleibt | erledigt |
| e | `OutputDock` wird Abonnent; die vorhandene `appendOutput`-API bleibt für den `CalculationRunner`-Pfad | erledigt |
| f | `CMakeLists.txt`: Quellen + Testziel | erledigt |
| g | Build grün, Tests grün, Commit, Doku/Changelog | erledigt |

Zwei Punkte aus der Umsetzung, die im Kopf bleiben sollten. Der `OutputDock` sammelt
Anhänge über 100 ms, weil `simulationworker.cpp:920` im Optimierer-Callback pro Iteration
loggt — ein `QTextEdit::append` je Zeile lässt die GUI beim Greifen stehen. Und die
Weitergabe an Qts eigenen Handler *sieht* kaputt aus, wenn man sie prüft: auf einem
systemd-System geht Qts Standard-Handler an journald, sobald stderr kein Terminal ist. Mit
`QT_LOGGING_TO_CONSOLE=1` sind die Zeilen da. Nachgemessen am 09.09.2026.

**Was die neue Sichtbarkeit sofort zutage gefördert hat** (Operator-Lauf 09.09.2026): beim
Start stehen **zehnmal** `QLayout::addChildLayout: layout QHBoxLayout "" already has a parent`
im Dock — acht vor dem NMR-Dialog, zwei währenddessen. Ein echter Layout-Fehler, der bisher nur
im Terminal stand. Nicht eingegrenzt: ein Textmuster über die Quellen liefert nur einen
Fehltreffer (`simulationchart.cpp` erzeugt sein `row` ohne Elternteil), die Stelle braucht einen
Lauf unter dem Debugger. Ebenfalls sichtbar: `This plugin supports grabbing the mouse only for
popup windows` beim Ziehen — die bekannte Wayland-Grenze des Cursor-Pinnings (`view.h`).

**Offene Frage zur Lautstärke:** der Dock zeigt derzeit auch `Debug`-Datensätze, weshalb interne
Zeilen wie `[NMRDataStore] DataStore created` beim Benutzer landen. Ein Vorgabefilter ab `Info`
(mit Umschalter) würde das aufräumen, ist aber eine Bedienentscheidung.

**Noch offen:** `MainWindow::updateOutputView()` ersetzt den gesamten Dock-Inhalt durch eine
Logdatei (`setText`), überschreibt also die LogHub-Zeilen. Das ist Altverhalten des
Rechnungs-Pfads und wird erst dann unangenehm, wenn der Dock zwei Quellen gleichzeitig zeigen
soll — Kandidat für eine getrennte Ansicht oder Filterung nach `source`.

### Phase 1 — Die Registry, mit einem menschlichen Nutzer

| WP | Inhalt | Fertig, wenn | Status |
|---|---|---|---|
| **1.1** *(erledigt `a15a581`)* | `qurcuma_core`: `ToolSpec`, `ToolResult`, `ToolRegistry`, Schema-Prüfung. `qurcuma_core_init()` ruft `initialize_generated_registry()`. Den `-march`/AVX-Block aus `CMakeLists.txt:288-311` **spiegeln** (Eigen-ABI). | `test_toolregistry` grün (37 Prüfungen); über `compile_commands.json` belegt, dass beide Kern-TUs weder QtWidgets noch Quick3D, QtCharts oder curcuma im Include-Pfad haben. | **erledigt** |
| **1.2** | GUI-Thread-Marshalling im Dispatcher; Audit-Spur in den `LogHub`. | `test_tooldispatcher` grün (18 Prüfungen), inkl. Nachweis des Threadwechsels und des Zeitlimits. | **erledigt** |
| **1.3** | ~12 lesende Werkzeuge (`src/llm/tools_view.cpp`), Analyse über `moleculebridge` gegen `curcuma::Molecule`: `get_structure_summary`, `list_atoms`, `select_atoms` (FragString-Grammatik), `get_selection`, `get_fragments`, `measure`, `get_frame_info`, `read_log`, `list_workdir`, `get_camera`, `get_display`, `describe_tools`. | Kern-Werkzeuge headless geprüft (`test_toolscore`, 25 Prüfungen); die zehn Viewer-Werkzeuge brauchen ein Widget und bleiben beim Operator-Check. | **erledigt** |
| **1.4** | **Vorführbar #1:** Command-Palette zeigt Menü-Actions **plus** parameterlose Registry-Werkzeuge (Vereinigung, kein Ersatz — die Palette bekommt heute ~120 Menüeinträge samt `enabled`-Zustand geschenkt). | Ctrl+K führt Registry-Werkzeuge aus; das Ergebnis landet im Output-Dock. **Operator-Check offen.** | **erledigt** (`7bf965d`) |

#### WP1.1 im Einzelnen — **erledigt** (`a15a581`)

| # | Schritt | Status |
|---|---|---|
| a | `src/core/tool.h`: `ToolEffect`, `ToolAffinity`, `ToolResult` (inkl. `image`, `truncated`), `ToolSpec` — reine Wertetypen, ohne Registry | erledigt |
| b | `src/core/toolregistry.{h,cpp}`: Registrierung (lehnt Doppelnamen und fehlerhafte Schemata **beim Anmelden** ab), Nachschlagen, Auflisten, `validate()`, `invoke()` | erledigt |
| c | Schema-Prüfung: `type`/`required`/`enum`/`minimum`/`maximum` und **Ablehnung unbekannter Schlüssel** | erledigt |
| d | `test_toolregistry`: Wohlgeformtheit, fehlende Pflichtfelder, Typfehler, `enum`-Verstoß, unbekannter Schlüssel, Doppelanmeldung, `invoke`-Durchlauf | erledigt |
| e | CMake: statische Bibliothek `qurcuma_core` (nur `Qt6::Core`/`Qt6::Gui`), `loghub` und `moleculedata` ziehen um, `qurcuma` linkt sie, Testziel | erledigt |
| f | Build grün, Tests grün, Commit, Doku/Changelog | erledigt |

**Abweichung vom Plantext, bewusst:** der Plan sah ein `qurcuma_core_init()` vor, das curcumas
`initialize_generated_registry()` ruft. Das erzwänge eine curcuma-Abhängigkeit für eine
Bibliothek, die keine braucht — die Registry selbst kennt curcuma nicht. `main.cpp:25` ruft es
bereits; der Helfer entsteht dort, wo der erste curcuma-berührende Kern-Code liegt (Phase 4).

#### WP1.2 im Einzelnen — **erledigt**

| # | Schritt | Status |
|---|---|---|
| a | `src/core/tooldispatcher.{h,cpp}`: Direktaufruf bei `Affinity::Any` **und** wenn schon auf dem Zielthread (sonst verklemmt sich `BlockingQueuedConnection` selbst) | erledigt |
| b | Marshalling mit **Zeitlimit**: Qt kennt für `BlockingQueuedConnection` keines, also `QueuedConnection` + `QSemaphore::tryAcquire`, Zustand über `shared_ptr`, damit ein Zeitüberlauf keinen freigegebenen Speicher beschreibt | erledigt |
| c | Audit-Spur in den `LogHub`: Name, Effekt, gekappte Argumente, Ergebnis, Dauer, ob marshallt wurde | erledigt |
| d | `test_tooldispatcher`: `Gui`-Handler aus einem Arbeitsthread landet auf dem Zielthread; `Any` läuft an Ort und Stelle; Zeitüberlauf ergibt einen sauberen Fehler statt einer Verklemmung; Audit-Datensätze erscheinen | erledigt |
| e | CMake: Quelle in `qurcuma_core`, Testziel | erledigt |
| f | Build grün, Tests grün, Commit, Doku/Changelog | erledigt |

**Nicht in diesem Paket:** die Freigabepolitik. Sie hängt an `ToolEffect`, gehört aber zur
Sitzung (WP2.2) und bekommt ihren Einhängepunkt dort, wo bekannt ist, was sie braucht.

#### WP1.3 im Einzelnen — **erledigt** (`ee4969f`, `b2da381`)

Zweigeteilt, weil nur die erste Hälfte headless prüfbar ist: Werkzeuge am `MoleculeViewer`
brauchen ein Widget und Quick3D, die bleiben beim Operator-Check.

| # | Schritt | Status |
|---|---|---|
| a | `src/core/tools_core.{h,cpp}`: `read_log` und `describe_tools` — brauchen nur `LogHub` und die Registry, bleiben also in `qurcuma_core` | erledigt |
| b | `test_toolscore`: Schemata wohlgeformt, `read_log`-Filter und Kappung, `next_seq` fürs Blättern, `describe_tools` mit und ohne Schema | erledigt |
| c | `src/llm/tools_view.{h,cpp}` mit `ViewToolContext`: `get_structure_summary`, `list_atoms` (gekappt), `get_selection`, `select_atoms`, `get_fragments`, `measure`, `get_distance_matrix`, `get_contacts`, `get_frame_info`, `get_camera`, `get_display`, `list_workdir` | erledigt |
| d | Verdrahtung in `MainWindow`: Registry und Dispatcher anlegen, Kern- und Viewer-Werkzeuge anmelden | erledigt |
| e | CMake: Quellen, Testziel | erledigt |
| f | Build grün, Tests grün, Commit, Doku/Changelog | erledigt |

**Zwei Festlegungen, die hier zum ersten Mal greifen.** `describe_tools` liefert per Vorgabe
**kein** Schema, sondern nur Name, Beschreibung, Kategorie und Effekt; das vollständige Schema
gibt es auf Nachfrage für ein einzelnes Werkzeug. Das ist die Zweistufigkeit aus Abschnitt 4.3
im Kleinen, und sie greift, bevor der Katalog groß wird. Und `tools_view` bindet **nicht**
`mainwindow.h` ein: was es von `MainWindow` braucht (das Arbeitsverzeichnis) kommt als
`std::function`, sodass die Kopplung auf den Viewer beschränkt bleibt.

#### WP1.4 im Einzelnen — **erledigt** (`7bf965d`)

| # | Schritt | Status |
|---|---|---|
| a | `src/llm/tools_palette.{h,cpp}`: Registry-Werkzeuge zu `CommandPalette::Command` — **nur die, die ohne Argumente laufen**, geprüft über `validateAgainst(schema, {})`, was auch Werkzeuge mit rein optionalen Parametern einschließt | erledigt |
| b | Ausführung über den **Dispatcher**, nicht über die Registry: so gelten Threadwechsel und Audit-Spur auch für den Palettenweg | erledigt |
| c | Ergebnis sichtbar machen: `text` bzw. eingerücktes JSON in den Output-Dock, gekappt; Fehler auf `Warning` | erledigt |
| d | Einhängen in `showCommandPalette()` als **Vereinigung** — Menü-Actions und die zehn kuratierten Einträge bleiben unverändert | erledigt |
| e | Build grün, Tests grün, Commit, Doku/Changelog | erledigt |

**Warum Vereinigung und nicht Ersatz:** die Palette bekommt heute rund 120 Menüeinträge samt
ihrem `enabled`-Zustand geschenkt (`collectMenuCommands`, `mainwindow.cpp:649`). Sie durch die
Registry zu *ersetzen* hieße, all das von Hand nachzubauen und dabei auf die parameterlosen
Werkzeuge zu schrumpfen. Der Gewinn liegt woanders: ab hier benutzt ein Mensch die Registry
täglich, und ein kaputtes Werkzeug fällt sofort auf statt erst, wenn ein Modell darüber stolpert.

### Phase 2 — Das LLM (erstes sichtbares Feature, ohne curcuma-Änderung)

| WP | Inhalt | Fertig, wenn | Status |
|---|---|---|---|
| **2.1** | `option(USE_LLM ... ON)` nach dem Vorbild von `USE_SFTP` (`CMakeLists.txt:66`); `Qt6::Network`; `LlmClient` (OpenAI-kompatibel, SSE, Tool-Calls); `LlmConfig` (Profile aus `~/.config/qurcuma/llm.json`, **einfache JSON-Datei, nie QSettings**, Key nur aus der Umgebung). | `test_llmclient` gegen einen `QTcpServer`-Stub, **kein Netz in ctest**; `-DUSE_LLM=OFF` baut wie bisher. | **erledigt** (`52ddc3a`, Streaming `5274252`) |
| **2.2** | `LlmSession`: Agentenschleife off-thread, Dispatch über die Registry, Freigabepolitik, Audit-Spur. | `test_llmsession` (24 Prüfungen) gegen den echten Client am Stub-Endpunkt: Iterationsgrenze, Freigabe, erfundene Werkzeugnamen, kaputte Argumente. | **erledigt** |
| **2.3** | **Vorführbar #2:** `ChatDock` als achter Dock (`dockmanager.cpp:273`, `dockconfig.h`), Streaming, Werkzeugzeilen mit Freigeben/Ablehnen. | Code steht, `USE_LLM=ON` und `OFF` beide vollständig gebaut. **Operator-Check offen.** | **erledigt** |

#### WP2.1 im Einzelnen — **a–f erledigt** (`52ddc3a`, Streaming `5274252`)

| # | Schritt | Status |
|---|---|---|
| a | `option(USE_LLM ... ON)`; neue Bibliothek `qurcuma_llm` (Core + Gui + **Network**), damit `qurcuma_core` seine geprüfte Eigenschaft „nur Core/Gui" behält | erledigt |
| b | `src/llm/llmconfig.{h,cpp}`: Profile aus `~/.config/qurcuma/llm.json` (einfache JSON-Datei, **nie QSettings**); das Profil nennt nur den **Namen der Umgebungsvariablen**, nie den Schlüssel selbst | erledigt |
| c | `src/llm/llmclient.{h,cpp}`: `POST /v1/chat/completions`, Tool-Calls in beide Richtungen, Abbruch, Fehler mit Klartext statt stiller Leere | erledigt |
| d | `test_llmclient` gegen einen `QTcpServer`-Stub: Anfrageform, `Authorization`-Kopfzeile nur bei gesetztem Schlüssel, Antwort mit `content` und mit `tool_calls`, HTTP-Fehler, kaputtes JSON. **Kein Netzzugriff.** | erledigt |
| e | Build grün, Tests grün, `-DUSE_LLM=OFF` baut wie bisher, Commit | erledigt |
| f | SSE-Streaming — **erledigt**, siehe WP2.1f weiter unten | erledigt |

**Warum Streaming abgetrennt ist:** die Zeile zu WP2.1 versprach es zusammen mit dem Rest. Der
tragende Teil ist die Anfrage/Antwort mit Tool-Calls — sie entscheidet, ob die Werkzeugschicht
überhaupt erreichbar ist. Streaming setzt darauf auf, verlangt aber das schrittweise
Zusammensetzen von `tool_calls` aus Bruchstücken und ist reine Bedienqualität. Getrennt gebaut
ist beides prüfbar; zusammen wäre der erste Commit ein Klumpen.

#### WP2.2 im Einzelnen — **erledigt**

| # | Schritt | Status |
|---|---|---|
| a | `src/llm/llmsession.{h,cpp}`: Gesprächsverlauf, Werkzeugkatalog im OpenAI-Format aus der Registry, Schleife *senden → Werkzeug ausführen → Ergebnis zurückschicken* | erledigt |
| b | Freigabepolitik über `ToolEffect`: `Read`/`Display` laufen durch, alles andere fragt. **Ohne gesetzten Rückfrage-Haken wird abgelehnt**, nicht durchgewinkt | erledigt |
| c | Iterationsobergrenze, Abbruch, und eine Kappung der Werkzeugergebnisse, bevor sie in den Verlauf wandern | erledigt |
| d | `test_llmsession` gegen den `QTcpServer`-Stub mit einer **Folge** vorbereiteter Antworten: schlichte Antwort, Werkzeugaufruf mit Rückgabe, verweigerte Freigabe, Iterationsgrenze, Transportfehler | erledigt |
| e | Build grün, Tests grün, Commit, Doku | erledigt |

**Warum der Test gegen den echten Client läuft** und nicht gegen eine Attrappe: die interessante
Frage ist, ob Client und Sitzung *zusammen* das Richtige tun — ob ein `tool_calls`-Block wirklich
zu einem Dispatch führt und das Ergebnis in der richtigen Nachrichtenform zurückgeht. Eine
Attrappe würde genau diese Naht überspringen.

#### WP2.3 im Einzelnen — **erledigt**

| # | Schritt | Status |
|---|---|---|
| a | `src/docks/chatdock.{h,cpp}`: Gesprächsansicht, Eingabe, Profilauswahl, Abbruch-Knopf, Werkzeugzeilen | erledigt |
| b | Registrierung als achter Dock (`dockconfig.h`, `dockmanager.cpp:273`) | erledigt |
| c | Verdrahtung in `MainWindow`: `LlmClient` + `LlmSession`, Systemprompt, Profil aus `LlmConfig` (Beispieldatei anlegen, wenn keine da ist) | erledigt |
| d | Freigabedialog mit **„einmal / für diese Sitzung / ablehnen"**, pro Werkzeug gemerkt | erledigt |
| e | `-DUSE_LLM=OFF` baut **vollständig** — hier wird die Option zum ersten Mal tragend, vorher war sie folgenlos | erledigt |
| f | Build grün, Tests grün, Commit, Doku | erledigt |

#### Gegen ein echtes Ollama geprüft (09.09.2026)

Der Stub-Server beweist nur, dass der Client tut, was ich erwarte. Deshalb einmal gegen den
laufenden Dienst auf `127.0.0.1:11434` gemessen, mit `glm-5.3-flash:cloud` und exakt dem Body,
den `LlmClient` schickt:

| Was | Ergebnis |
|---|---|
| Schlichte Anfrage an `/v1/chat/completions` ohne `Authorization` | funktioniert |
| `tools` + `tool_choice: "auto"` | angenommen, kein Fehler |
| Antwort | ein `tool_calls`-Eintrag mit `id`, `function.name`, `function.arguments` |
| Rückgabe als `{role:"tool", tool_call_id, name, content}` | angenommen, Modell antwortet inhaltlich korrekt |
| Modell existiert nicht | `{"error":{"message":"model '…' not found"}}` → der Client zeigt daraus Klartext |

**Zwei Formen, die vom Erwarteten abweichen** und jetzt als Test festgenagelt sind: neben
`tool_calls` ist `content` ein **leerer String**, nicht `null`; und `arguments` ist ein
JSON-**String**, kein Objekt. Beides behandelt der Code richtig, aber ohne Test könnte ein
späterer Umbau `""` für eine Antwort halten und den Zug beenden, ohne etwas auszuführen.

**Was das nicht beweist:** dass ein *lokales* Modell Werkzeuge zuverlässig aufruft. Das hängt am
Modell, nicht am Endpunkt. Das Beispielprofil sagt das jetzt auch.

#### WP2.1f — Streaming und sichtbares Nachdenken — **erledigt**

Vorgezogen vor Phase 3, weil ohne Streaming bei einem nachdenkenden Modell minutenlang nichts
passiert und niemand sieht, ob es hängt.

**Am laufenden Endpunkt gemessen** (Ollama, `glm-5.3-flash:cloud`, 09.09.2026), nicht aus
Dokumentation abgeleitet:

| Beobachtung | Bedeutung für die Umsetzung |
|---|---|
| SSE-Zeilen `data: {json}`, Abschluss `data: [DONE]` | Zeilenweise zerlegen, `[DONE]` gesondert |
| `choices[0].delta` trägt `role`, `content`, **`reasoning`** | Das Denken heißt `reasoning`, **nicht** `reasoning_content` |
| Tool-Call kam **vollständig in einem Chunk** | Trotzdem über `index` zusammensetzen — OpenAI selbst fragmentiert `arguments` |
| `finish_reason: "tool_calls"` bzw. `"stop"` | Ende des Zuges |

| # | Schritt | Status |
|---|---|---|
| a | `LlmClient`: `"stream": true`, SSE zeilenweise, Sammeln von `content`/`reasoning`/`tool_calls` (nach `index`), Signale `contentChunk`/`reasoningChunk` | erledigt |
| b | Am Ende dieselbe Assistenten-Nachricht wie bisher zusammensetzen, damit `LlmSession` unverändert bleibt. `reasoning` geht **nicht** in den Verlauf zurück | erledigt |
| c | `LlmSession` reicht die Bruchstücke durch | erledigt |
| d | `ChatDock`: Antwort läuft live ein; das Denken in einem einklappbaren Abschnitt (`CollapsibleSection`), der beim Streamen offen ist | erledigt |
| e | Test gegen den Stub: SSE-Zerlegung, `[DONE]`, zusammengesetzte Tool-Calls, `reasoning` landet nicht im Verlauf | erledigt |
| f | Build, Tests, Commit, Doku | erledigt |

### Phase 3 — curcuma-Kern
Eigenes Dokument: `external/curcuma/docs/TOOL_API_WP.md` (WP1–WP7: Registry-Ausbau,
Annotation, Logger-Senke, Zustand pro Lauf statt global, `Results()`, Mess-Fähigkeit,
Fähigkeitstabelle). Voraussetzung für Phase 4.

### Phase 4 — In-Prozess-Rechnen als Werkzeuge

| WP | Inhalt | Fertig, wenn | Status |
|---|---|---|---|
| **4.1** | `src/llm/curcumajob.{h,cpp}`: Einzellauf auf einem Worker-Thread, Log über die Scope-Senke mit `job_id` in den `LogHub`, `Results()` zurück. Mutex „nur ein In-Prozess-Rechenjob" (OpenMP). Optionaler Controller-Export als Reproduzierbarkeits-Artefakt: derselbe Lauf ist mit `curcuma -import_config run.json` nachstellbar. | `test_curcumajob` grün: echtes `sp` auf einem 3-Atom-XYZ, Energie aus `Results()`, Log unter der `job_id`. | **erledigt** (`1c93ecc`) |
| **4.2** | Schema-Erzeugung **aus curcumas Registry** (Kommando↔Modul aus `ModuleDefinition`, Auswahl über `tier=primary`, Constraints aus `allowed`/`unit`/`min`/`max`, bedingte Felder aus `requires`), dazu `describe_job(command)`. Keine handgepflegte Parametertabelle in qurcuma. | `test_curcumaschemas` grün: Schema für die exponierten Kommandos ohne handgepflegte Liste, Modulzuordnung und Auflösbarkeit geprüft. | **erledigt** (`b6eec1e`) |
| **4.3** | Rechen-Werkzeuge (`Effect::Compute`), asynchron: `run_job → {status:"started", job_id}`, `job_status`, `job_result`. Dazu `set_temperature`, `pause_md`, `resume_md`, `stop_md`, `step_once` über die vorhandenen `SimulationWorker`-Slots per QueuedConnection. | MD per Werkzeug steuerbar, GUI bedienbar, `Mutate` während des Laufs sauber abgelehnt. | **erledigt** — Einzelläufe `b6eec1e`, Warteschlange und `wait_seconds` `9249313`, Laufzeitsteuerung `95c2185` |

Zur Laufzeitsteuerung: die Werkzeuge gehen **über `SimulationControlWidget`**, nicht direkt auf
die Worker-Slots wie im Plan angenommen. Das Dock hält den Thread-Lebenszyklus (`startWithConfig`,
Teardown, Wiederverdrahtung des Viewers über `workerStarted`); ein zweiter Einstieg daneben hätte
ihn verdoppelt. Der Nebeneffekt ist der eigentliche Gewinn: der Bediener sieht den Lauf im Dock
mit genau den Parametern stehen, die das Modell angefragt hat.

| Werkzeug | Wirkung |
|---|---|
| `run_simulation` | MD oder Optimierung starten (`mode`, `method`, `steps`, `temperature`, `timestep`, `thermostat`, `optimizer`, `convergence`) |
| `simulation_status` | Schritt, Energie, Temperatur; mit `wait_seconds` wartet es auf das Laufende statt zu pollen |
| `pause_simulation` / `resume_simulation` | idempotent, anders als der Knopf, der umschaltet |
| `stop_simulation` / `step_simulation` | beenden bzw. genau einen Schritt |
| `set_temperature` | Sollwert des Thermostaten im laufenden MD |

Die `enum`-Listen für `method`, `optimizer` und `thermostat` liest das Schema aus den Comboboxen
des Docks (`methodValues()` und die beiden Geschwister). Von Hand abgeschrieben standen dort
schon `diis` und `rfo`, wo das Dock `native_diis` und `native_rfo` sagt — genau der Drift, den
Abschnitt 7.2 für die Thermostatliste beschreibt.

### Leitszenario — agentisches Docking

Der Operator hat als Zielbild genannt: **zwei Moleküle laden, dann interaktiv-agentisch docken**
— ein Rezeptor mit Kavität, ein Gast, der hineinmuss. Und ausdrücklich so, dass **das Modell alle
nötigen Befehle und Analysen selbst ausführt und selbst prüft**.

Das ist als Prüfstein wertvoller denn als Feature: es misst den Werkzeugsatz an einer Aufgabe
statt an einer Liste. Daraus folgt ein Entwurfsgrundsatz, der über die Werkzeugauswahl
entscheidet: **zu jeder Handlung muss es eine Beobachtung geben.** Ein Modell, das platzieren
darf, aber nicht messen kann, ob es getroffen hat, rät. Die Schleife lautet
*platzieren → messen → bewerten → wiederholen*, und jeder Pfeil braucht ein Werkzeug.

**Was es dafür schon gibt:**

| Baustein | Stand |
|---|---|
| Fragmenterkennung (welches ist Rezeptor, welches Gast) | `get_fragments`, über curcumas `GetFragments()` — **fertig** (WP1.3) |
| Fragment auswählen | `select_atoms` mit `"F0"`/`"F1"` — **fertig** (WP1.3) |
| Geometrie prüfen | `measure` (Abstand, Winkel, Diederwinkel, Gyrationsradius) — **fertig** (WP1.3) |
| Kontakte Rezeptor↔Gast | `get_contacts` mit zwei Auswahlen — **fertig** |
| Überlappung zählen | `MoleculeViewer::getCollisionCount()` (`view.h:360`) und `resolveClashes()` (`:353`), beide öffentlich — **Werkzeug fehlt noch** |
| Zwei Strukturen in eine Szene | `MainWindow::mergeFileIntoScene()` — vorhanden, aber modal und ohne Werkzeug |
| Docking als Rechnung | curcumas `Docking` (25 PARAMs, `capabilities/docking.h:103`) |
| Wechselwirkungsenergie | curcumas `interaction` — aber `main.cpp`-resident, ohne Klasse und ohne Schema (eines der 22 modul-losen Kommandos) |

**Was konkret fehlt, mit Fundstelle:**

1. **Ein Fragment bewegen.** `MoleculeViewer::moveSelection()` ist **privat** (`view.h:863`). Für
   ein `transform_fragment`-Werkzeug (Translation + Rotation um den Fragmentschwerpunkt, `Mutate`)
   muss es eine öffentliche, snapshot-fähige Entsprechung geben. Rotation gibt es noch gar nicht.
2. ~~**Laden und Zusammenführen als Werkzeuge.**~~ **`merge_structure`** und
   **`save_structure`** stehen (`3646ac0`), beide ohne `QMessageBox` und ohne `mainwindow.h`.
   Offen bleibt bewusst das *Ersetzen* der ganzen Szene: `MainWindow` hält Trajektorie,
   Framezahl und Fensterzustand, und ein Werkzeug, das die Geometrie darunter austauscht,
   ließe alle drei stehen. Das erste Laden bleibt Sache des Bedieners.
3. **Die Kavität finden.** Nichts davon existiert bisher — weder in qurcuma noch als
   curcuma-Fähigkeit. Für den Anfang reicht wahrscheinlich Billigeres als echte Kavitätssuche:
   Schwerpunkt und Trägheitsachsen des Rezeptors, plus `render_view` aus mehreren Richtungen,
   und das Modell schließt daraus. Ob das trägt, ist eine offene Frage, keine Zusage.
4. ~~**`render_view`.**~~ Vorgezogen und erledigt (`b17935b`): eine Kavität aus
   Koordinatenlisten zu erschließen ist deutlich schwerer, als sie anzusehen.
5. ~~**Bewerten.**~~ `run_single_point` nimmt seit `25fd722` eine Auswahl, die
   Wechselwirkungsenergie ist damit E(ganz) − E(`F1`) − E(`F2`) aus drei Aufrufen. Ein
   Ausschnitt, der kovalente Bindungen schneidet, wird als solcher gemeldet, damit keine
   Radikalenergie subtrahiert wird. curcumas `interaction` als eigene Fähigkeit bleibt offen
   (WP3.5/WP6 auf curcuma-Seite).
6. **Freie Energie.** Nach einer Bindungs*energie* gefragt liefert das Modell heute eine
   Differenz von Potentialenergien und sagt das auch dazu. Für ΔG fehlt die ganze Kette:
   die Arbeit entlang eines gesteuerten Zugs, eine restrainte Kollektivvariable samt
   Histogrammen für ein PMF, und λ-Kopplung mit Soft-Core für FEP/TI. Gestaffelt und mit
   dem Vorhandenen abgeglichen (curcumas RMSD-Metadynamik ist bereits eine
   gradientenexakte Biasing-Form) als **WP9** in `external/curcuma/docs/TOOL_API_WP.md`.
   Stufe 1 hängt an WP8; Stufe 3 reicht in die Energiemethoden und ist bewusst nicht
   terminiert.

7. **Ziehen statt setzen.** Der Operator will, dass das Modell an Atomen *zieht*, nicht nur
   Koordinaten setzt. `SimpleMD::applyExternalForces()` sieht danach aus, ist aber transient
   („cleared after use", `simplemd.h:503`) und nur zwischen zwei `step()`-Aufrufen wirksam — eine
   Injektion, kein Potential. Deklarative, im Controller stehende und zur Laufzeit änderbare
   externe Potentiale sind als **WP8** in `external/curcuma/docs/TOOL_API_WP.md` beschrieben.

8. **Solvatation.** `fill_container` packt jetzt Kopien einer Bibliotheksmolekel um die
   Struktur — brauchbar für eine **Mikrosolvatationsschale** (ein paar Dutzend explizite
   Wasser um eine Bindungstasche) und für Gasphasen-Szenen. Eine **Wasserbox im Sinne einer
   Solvatationsrechnung ist es nicht**, und das ist keine Beschriftungsfrage:

   - Die Zufallspackung mit Mindestabstand erreicht die Flüssigkeitsdichte nicht. Eine echte
     Box entsteht aus einer vorequilibrierten Wasserzelle, die repliziert und um die
     Solut-Überlappungen bereinigt wird.
   - **Es gibt keine periodischen Randbedingungen in den Nichtbindungstermen.** curcumas
     `wall_potential=pbc` setzt ein austretendes Molekül auf der Gegenseite wieder ein, aber
     ohne Minimum-Image werden Wechselwirkungen über die Grenze nicht fortgesetzt
     (`external/curcuma/docs/WP-PERIODIC-NONBONDED.md`). Eine endliche Box im Vakuum hat eine
     Oberfläche und zieht sich zum Tropfen zusammen.
   - Nichts equilibriert das Ergebnis; eine Zufallspackung braucht Minimierung und NVT/NPT,
     bevor irgendeine Zahl daraus etwas bedeutet.
   - Größenordnung: 175 Atome Solut plus 10 Å Wasserhülle sind einige tausend Moleküle. GFN-FF
     bei interaktiven Bildraten trägt das nicht.

   Der tragende Teil davon liegt in curcuma (PBC in den Nichtbindungstermen) und ist dort als
   Arbeitspaket beschrieben. Bis dahin ist Mikrosolvatation das, was wissenschaftlich
   verteidigbar bleibt.

9. **Schleifenkosten.** Eine agentische Schleife ruft Werkzeuge dutzendfach. Das ist genau der
   Fall, für den der TODO zum effizienten Tooling geschrieben wurde: Katalog klein halten,
   Ergebnisse kappen, Bilder nur auf Anforderung.

Kein eigenes Arbeitspaket. Der Eintrag steht hier, damit die Reihenfolge der nächsten Pakete an
dieser Aufgabe geprüft werden kann statt an einer Featureliste.

### Phase 5 — Nachgelagert

| WP | Inhalt | Status |
|---|---|---|
| **5.1** | `render_view` mit Dimensionsobergrenze (1600 px) und Sperre während laufender MD (`exportImage` läuft synchron und hielte den MD-`QTimer` für die Dauer eines 4K-SSAA-Renders an). | **erledigt** (`b17935b`) |
| **5.2** | `--serve`: JSON-RPC/MCP über stdio, vollständige Anwendung offscreen. | offen |
| **5.3** | Werkzeug für externe Programme (ORCA, xtb) über den bestehenden `CalculationRunner`. | offen |
| **5.4** | Weitere native Analyse-Werkzeuge aus `curcuma::Molecule` (Dipolmoment, H-Brücken-Karte, PBC-Varianten). | offen |
| **5.5** | *Nur falls der Drift-Test regelmäßig ausschlägt:* `SimulationConfig` aufteilen. | offen |

Dazwischen kamen fünf Werkzeuge aus der Bearbeitung (`b17935b`) und drei aus dem Leitszenario
dazu, die im Plan keine eigene Nummer hatten:

| Werkzeug | Wofür | Commit |
|---|---|---|
| `add_atoms`, `add_fragment`, `add_hydrogens`, `delete_atoms`, `list_fragments` | Struktur ändern; alle `Mutate`, alle über den Snapshot-Stapel rücknehmbar | `b17935b` |
| `run_single_point` **mit Auswahl** | Wechselwirkungsenergie als drei Aufrufe (ganz, `F1`, `F2`); `severedBondCount()` meldet geschnittene Bindungen | `25fd722` |
| `merge_structure`, `save_structure` | zweites Molekül dazuladen, Struktur oder Auswahl als xyz schreiben | `3646ac0` |

### TODO — effizientes Tooling (offen, spannt über alle Phasen)

Der Werkzeugkatalog geht bei **jedem** Zug vollständig mit: Name, Beschreibung und
Parameter-Schema jedes registrierten Werkzeugs stehen im `tools:`-Feld jedes Requests. Das ist
der wiederkehrende Kostenposten der ganzen Funktion, und er wächst mit jedem neuen Werkzeug.
Zu klären, bevor der Katalog groß wird:

- **Stufen auch für Werkzeuge, nicht nur für Parameter.** Ein kleiner Kern, der immer
  mitgeht, der Rest über `describe_tools`/`search_tools` auf Nachfrage nachladbar.
- **Kontextabhängige Teilmengen.** Ohne geladene Struktur braucht es keine Mess- und
  Auswahlwerkzeuge; im Explore-Modus keine Builder-Werkzeuge. Die Registry kennt Kategorie und
  Effekt, die Auswahl ist also ableitbar.
- **Beschreibungen kurz halten.** Das Schema ist der Kontrakt; die Beschreibung soll ihn nicht
  in Prosa wiederholen.
- **Messen statt schätzen.** Ein ctest, der die Katalogröße in Token gegen ein Budget prüft und
  fehlschlägt, wenn eine Erweiterung darüber hinausschießt. Ohne diesen Test merkt man das
  Wachstum erst an der Rechnung.
- **Prompt-Caching prüfen.** Der Werkzeugblock ist über die Züge einer Sitzung stabil und
  gehört deshalb an den Anfang des Requests, wo ein Anbieter-Cache ihn abdecken kann. Ob und
  wie das greift, ist pro Endpunkt zu prüfen — OpenAI-kompatibel heißt nicht gleich
  cache-kompatibel.
- **Ergebnisgrößen.** Nicht nur der Katalog, auch die Rückgaben füllen den Kontext. `truncated`
  in `ToolResult` ist der Anfang; Werkzeuge mit potenziell großer Ausgabe (`read_log`,
  `list_atoms`, `job_result`) brauchen sinnvolle Vorgaben und Zusammenfassungen statt roher
  Vollausgabe.

Nichts davon blockiert Phase 0 bis 2 — bei einem Dutzend lesender Werkzeuge ist der Katalog
klein. Es blockiert den Zeitpunkt, an dem curcumas 36 Kommandos dazukommen.

### Bewusst nicht gemacht
- **Ablösung von `SimulationConfig`.** Der Struct (`simulationworker.h:42-140`, 60+ Felder)
  spiegelt curcumas PARAM-Blöcke von Hand; gemessen ~316 Feldzugriffe (65 in
  `simulationworker.cpp`, 122 in `simulationcontrolwidget.cpp`, 129 in `lesson.cpp`), und
  `simConfigToJson` ist das persistierte `*.qlesson.json`-Format. Der 80-Prozent-Nutzen ist
  **ein ctest**, der prüft, dass jeder von `build*Params()` erzeugte Schlüssel in curcumas
  `ParameterRegistry` auflösbar ist.
- **T6 (vollständige Dock-Entkopplung)** aus `WP-tech-debt-cleanup.md`.
- **Nachgebaute Kernel entfernen.** `ncianalysisworker.cpp:34-51` und `:59` bauen curcumas
  Coulomb- und Dispersionspaar-Kernel nach. Entfernbar erst, wenn curcuma die
  **Paarbeiträge einzeln** herausgibt; heute summiert der Kernel intern, qurcuma braucht die
  Einzelwerte für die Kontaktliste.

## 6. Risiken

- **Der In-Prozess-Weg verlagert Arbeit nach vorn.** Was ein Subprozess geschenkt hätte
  (stdout an der Pipe, eigenes Arbeitsverzeichnis, Isolation gegen Abstürze, zuverlässiger
  Abbruch), muss in curcuma gebaut werden. Ein Absturz dort reißt künftig die GUI mit. Für
  lange, unbeaufsichtigte Läufe bleibt das ein Argument für einen Kindprozess.
- **Erste Netzabhängigkeit.** `Qt6::Network` ist Teil von qtbase, also kein zusätzliches
  CI-Modul. Heikel ist die Auslieferung: HTTPS braucht Qts TLS-Plugin. Für AppImage
  (`.github/workflows/build.yml:87`), Windows und macOS ist zu prüfen, dass das
  OpenSSL-Backend mitgepackt wird. Ein Build, der lokal läuft und im AppImage stumm keine
  Verbindung aufbaut, ist der wahrscheinlichste Ausrutscher.
- **Eigen-ABI.** `CMakeLists.txt:288-311` dokumentiert ein reales `double free`, wenn die
  `-march`-Flags eines Ziels von curcumas abweichen. **Jedes** neue Ziel muss den Flag-Block
  wiederholen.
- **`initialize_generated_registry()`** wird bisher nur aus `src/main.cpp:25` gerufen. Jede
  neue Testbinary muss es tun, sonst wirft `ConfigManager::get<T>()`.
- **Modale Einstiegspunkte.** `loadMoleculeFile` fragt bei ungesicherten Änderungen per
  `QMessageBox` (`mainwindow.cpp:3916`). Ein Werkzeug darf keinen Dialog öffnen, den niemand
  sieht: nicht-interaktive Variante mit explizitem `force`-Parameter.
- **Kontextkosten.** Der Werkzeugkatalog geht bei *jeder* Anfrage mit. Nach jeder Erweiterung
  auf Token-Größe prüfen; deshalb `tier` plus `describe_job` statt vollständiger Schemata.
- **Prompt-Injektion.** Logzeilen, Kommentarzeilen in XYZ-Dateien und Molekülnamen landen als
  Werkzeugergebnisse im Modellkontext. Sie sind Daten, keine Anweisungen. Die Absicherung
  bleibt die Freigabepolitik.

## 7. Verifikation

Der Build ist die Wahrheit; IDE-Diagnosen sind hier unzuverlässig (Flag
`-mno-direct-extern-access`). Exit-Code separat prüfen, nie über eine Pipe:

```fish
cmake --build debug > build.log 2>&1; echo "CMAKE_EXIT=$status"
grep -iE "error:|Fehler|undefined reference" build.log | grep -viE "external/curcuma|ulysses"
```

`| tail` verschluckt den Rückgabewert und hat schon zu falsch gemeldeten grünen Builds geführt.

**Automatisiert:** `test_loghub`, `test_toolregistry`, `test_toolregistry_threading`,
`test_llmclient` (gegen einen `QTcpServer`-Stub, kein Netz), `test_llmsession` (Fake-Client),
`test_curcumajob`, `test_curcuma_schemas` (Drift-Alarm), `test_simconfig_keys`. Bestehende
Suites bleiben grün.

**Operator-Prüfpunkte** (Qt und 3D rendern im Agenten-Bash nicht):
1. Nach 0.1: MD starten, dann Struktur bearbeiten wollen — Ablehnung, sonst unverändert.
2. Nach 1.4: Ctrl+K, ein Registry-Werkzeug ausführen.
3. Nach 2.3: „Was ist das für ein Molekül, miss den H-N-H-Winkel, färbe nach Fragment, zeig
   mir das Log." Anzeige-Werkzeuge ohne Rückfrage.
4. Nach 4.3: Rechenjob per Werkzeug starten — Bestätigungsdialog, „für diese Sitzung merken",
   GUI bleibt bedienbar.
5. Während laufender MD ein `Mutate`-Werkzeug anfordern — saubere Ablehnung mit Begründung.
6. `read_log` nach einem fehlgeschlagenen In-Prozess-Job: die curcuma-Meldung ist sichtbar
   (heute geht sie verloren).
7. Nach einem LLM-Lauf: keine neuen `Basename.Keyword.ZEITSTEMPEL/`-Verzeichnisse und keine
   `curcuma_restart.json` im Projektordner.
8. Audit-Spur ansehen: was hat das Modell aufgerufen, was wurde freigegeben.
9. Ein altes `*.qlesson.json` laden: Simulationskonfiguration unverändert.
10. AppImage bauen und darin eine echte Verbindung zum Endpunkt herstellen (TLS-Plugin).
