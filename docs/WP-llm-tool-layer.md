# WP — LLM-Werkzeugschicht: Tool-Registry, Logs, curcuma in-process

> **Status (Sep 2026):** geplant, nichts umgesetzt. Kein Abschnitt beschreibt vorhandenes
> Verhalten, außer wo er ausdrücklich unter „Ausgangslage" steht. Branch: `feature/llm-tools`.
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
| **0.1** | `canEditStructure()` (`view.h:363`) liefert zusätzlich `false`, solange `m_simulationActive` gilt; die mutierenden Viewer-APIs kehren früh zurück (Abschnitt 2). | GUI-verhaltensneutral, da die Mauspfade bereits über dieselbe Flagge gesperrt sind. Operator-Sichtprüfung. | offen |
| **0.2** | `MoleculeViewer::Atom`/`::Bond` nach `src/core/moleculedata.h`; in `view.h` bleiben Typaliase. `MoleculeFileLoader` zieht heute `view.h` (968 Zeilen, `QWidget`) allein wegen dieser Structs herein. | Build grün **ohne Änderung an den 51 Dateien**, die die Typen verwenden. | offen |
| **0.3** | `src/core/loghub.{h,cpp}` (QtCore-only) + `qInstallMessageHandler`; `OutputDock` wird Abonnent. | `test_loghub` grün; qurcumas `qDebug`-Meldungen erscheinen im Output-Dock statt nur im Terminal. | offen |

### Phase 1 — Die Registry, mit einem menschlichen Nutzer

| WP | Inhalt | Fertig, wenn | Status |
|---|---|---|---|
| **1.1** | `qurcuma_core`: `ToolSpec`, `ToolResult`, `ToolRegistry`, Schema-Prüfung. `qurcuma_core_init()` ruft `initialize_generated_registry()`. Den `-march`/AVX-Block aus `CMakeLists.txt:288-311` **spiegeln** (Eigen-ABI). | `test_toolregistry` grün; linkt nur `Qt6::Core`/`Qt6::Gui`. | offen |
| **1.2** | GUI-Thread-Marshalling im Dispatcher (`BlockingQueuedConnection` + Timeout); Audit-Spur in den `LogHub`. | `test_toolregistry_threading` grün. | offen |
| **1.3** | ~12 lesende Werkzeuge (`src/llm/tools_view.cpp`), Analyse über `moleculebridge` gegen `curcuma::Molecule`: `get_structure_summary`, `list_atoms`, `select_atoms` (FragString-Grammatik), `get_selection`, `get_fragments`, `measure`, `get_frame_info`, `read_log`, `list_workdir`, `get_camera`, `get_display`, `describe_tools`. | Headless aufrufbar, soweit ohne GUI möglich. | offen |
| **1.4** | **Vorführbar #1:** Command-Palette zeigt Menü-Actions **plus** parameterlose Registry-Werkzeuge (Vereinigung, kein Ersatz — die Palette bekommt heute ~120 Menüeinträge samt `enabled`-Zustand geschenkt). | Ctrl+K führt Registry-Werkzeuge aus. | offen |

### Phase 2 — Das LLM (erstes sichtbares Feature, ohne curcuma-Änderung)

| WP | Inhalt | Fertig, wenn | Status |
|---|---|---|---|
| **2.1** | `option(USE_LLM ... ON)` nach dem Vorbild von `USE_SFTP` (`CMakeLists.txt:66`); `Qt6::Network`; `LlmClient` (OpenAI-kompatibel, SSE, Tool-Calls); `LlmConfig` (Profile aus `~/.config/qurcuma/llm.json`, **einfache JSON-Datei, nie QSettings**, Key nur aus der Umgebung). | `test_llmclient` gegen einen `QTcpServer`-Stub, **kein Netz in ctest**; `-DUSE_LLM=OFF` baut wie bisher. | offen |
| **2.2** | `LlmSession`: Agentenschleife off-thread, Dispatch über die Registry, Freigabepolitik, Audit-Spur. | `test_llmsession` mit skriptbarem Fake-Client: Iterationsobergrenze greift; kein `Mutate`/`Compute` ohne Freigabe. | offen |
| **2.3** | **Vorführbar #2:** `ChatDock` als achter Dock (`dockmanager.cpp:273`, `dockconfig.h`), Streaming, Werkzeugzeilen mit Freigeben/Ablehnen. | „Was ist das für ein Molekül, miss den H-N-H-Winkel, färbe nach Fragment, zeig mir das Log" läuft durch. | offen |

### Phase 3 — curcuma-Kern
Eigenes Dokument: `external/curcuma/docs/TOOL_API_WP.md` (WP1–WP7: Registry-Ausbau,
Annotation, Logger-Senke, Zustand pro Lauf statt global, `Results()`, Mess-Fähigkeit,
Fähigkeitstabelle). Voraussetzung für Phase 4.

### Phase 4 — In-Prozess-Rechnen als Werkzeuge

| WP | Inhalt | Fertig, wenn | Status |
|---|---|---|---|
| **4.1** | `src/llm/curcumajob.{h,cpp}`: Einzellauf auf einem Worker-Thread, Log über die Scope-Senke mit `job_id` in den `LogHub`, `Results()` zurück. Mutex „nur ein In-Prozess-Rechenjob" (OpenMP). Optionaler Controller-Export als Reproduzierbarkeits-Artefakt: derselbe Lauf ist mit `curcuma -import_config run.json` nachstellbar. | `test_curcumajob`: echtes `sp` auf einem 3-Atom-XYZ, Energie aus `Results()`, Log unter der `job_id`. | offen |
| **4.2** | Schema-Erzeugung **aus curcumas Registry** (Kommando↔Modul aus `ModuleDefinition`, Auswahl über `tier=primary`, Constraints aus `allowed`/`unit`/`min`/`max`, bedingte Felder aus `requires`), dazu `describe_job(command)`. Keine handgepflegte Parametertabelle in qurcuma. | Schema für die exponierten Kommandos ohne handgepflegte Liste; Test prüft Modulzuordnung und Auflösbarkeit. | offen |
| **4.3** | Rechen-Werkzeuge (`Effect::Compute`), asynchron: `run_job → {status:"started", job_id}`, `job_status`, `job_result`. Dazu `set_temperature`, `pause_md`, `resume_md`, `stop_md`, `step_once` über die vorhandenen `SimulationWorker`-Slots per QueuedConnection. | MD per Werkzeug steuerbar, GUI bedienbar, `Mutate` während des Laufs sauber abgelehnt. | offen |

### Phase 5 — Nachgelagert
- `render_view` mit Dimensionsobergrenzen und Sperre während laufender MD (`exportImage`
  läuft synchron und hielte den MD-`QTimer` für die Dauer eines 4K-SSAA-Renders an).
- Werkzeug für externe Programme (ORCA, xtb) über den bestehenden `CalculationRunner`.
- Weitere native Analyse-Werkzeuge aus `curcuma::Molecule`.
- *Nur falls der Drift-Test regelmäßig ausschlägt:* `SimulationConfig` aufteilen.

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
