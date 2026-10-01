# WP: Remote-Rechnen und VR

Status: Stand 2026-10-01. R0 ist umgesetzt (`SimulationBackend`/`LocalBackend` in `src/simulationbackend.*`, `src/moleculetypes.h`; baut, 7 Test-Targets laufen, GUI-Lauf vom Operator noch nicht geprueft). R1 ist umgesetzt (`src/remote/`: Protokoll, Dateirichtlinie, `qurcuma-server`, Tests `test_remote_protocol` und `test_remote_loopback`; Loopback an einer 3-Atom-GFN-FF-Optimierung geprueft, nicht ueber zwei Rechner, kein MD-Lauf). R2 ist umgesetzt (`RemoteBackend`, `SshTunnel`, "Compute on" im Simulation-Dock; getestet mit einem Python-Ersatz fuer ssh, nicht mit echtem ssh, nicht im GUI; die Capabilities von B (Methoden-/GPU-Liste) ersetzen die lokale Liste noch nicht). R3 bis R5 sind nicht implementiert; alle Aussagen über vorhandenen Code sind auf den Stand dieses Datums bezogen und am Quelltext gelesen, nicht gebaut oder gemessen.

## Ziel

Rechner A führt qurcuma mit Viewer (und später VR-Brille) aus. Rechner B rechnet. A steuert (Start/Stop, Parameter, Temperatur, Greifkraft) und zeigt die Frames an, B führt curcuma-MD/-Optimierung aus. Transport zunächst über SSH, Erweiterung auf direktes TCP offen halten.

## Ist-Zustand (gelesen)

- Die Naht ist `SimulationWorker` (`src/simulationworker.h`): Eingaben `setMolecule/setBonds/setConfig/setLiveNci`, Slots `run/stepOnce/injectForce/clearInjectedForce/setTargetTemperature/setWallTemp/setWallBeta`, atomare `requestStop/Pause/Resume`; Ausgaben `frameReady(SimulationFramePtr)`, `finished(reason, aborted)`, `paused`, `errorOccurred`, `runParameters`.
- Der Worker wird an genau einer Stelle gebaut und angeschlossen: `SimulationControlWidget::startWithConfig` (`simulationcontrolwidget.cpp`), `MainWindow::wireSimulationWorker` (`mainwindow.cpp`) verbindet Viewer, Charts, Statusleiste, Snapshots. Das Frame-Paket `SimulationFrame` (`simulationframe.h`) ist ein Wert ohne Zeiger: Positionen, Energien, Temperaturen, optional Bindungsliste (reaktives GFN-FF), Reaktionsereignisse, NCI-Kontakte.
- Die Konfiguration hat einen verlustfreien JSON-Roundtrip: `simConfigToJson/simConfigFromJson` (`lesson.h`).
- Hindernis fuer ein GUI-freies Programm: `simulationworker.h` und `moleculebridge.h` binden `view.h` (QWidget) nur wegen `MoleculeViewer::Atom/Bond`; diese Namen kommen in rund 50 Dateien vor.
- Nicht ueber die Naht laufen heute: `NciAnalysisWorker` (curcuma lokal), `RMSDWidget` (`RMSDDriver` lokal), `CalculationRunner` (QProcess lokal), Parameter-Tab (liest curcumas `ParameterRegistry` lokal).
- Vorhanden: `sshconfig.*` (Parser fuer `~/.ssh/config` inkl. ProxyJump), `sftpcache.*` (libssh, CMake-Option `USE_SFTP` ist standardmaessig AUS). Qt6WebSockets und Qt6Quick3DXr sind installiert (`/usr/lib/cmake`), Monado und ALVR nicht.

## Architektur

```
A: qurcuma (GUI, Viewer, spaeter VR)            B: qurcuma-server (Qt Core, curcuma, kein GUI)
  SimulationControlWidget                         RemoteSession
    SimulationBackend* (Schnittstelle)              SimulationWorker (unveraendert) in QThread
      LocalBackend  -> SimulationWorker             1 Client, 1 Lauf
      RemoteBackend -> WebSocket ---ssh -L---->     QWebSocketServer auf 127.0.0.1
```

- `SimulationBackend`: abstrakte QObject-Schnittstelle mit genau den Signalen/Slots des Workers. `LocalBackend` ist ein duenner Wrapper um den bisherigen Worker; das lokale Verhalten bleibt gleich. `RemoteBackend` setzt dieselben Aufrufe in Nachrichten um. Damit aendern sich `wireSimulationWorker`, Charts, Snapshots und Viewer-Anbindung kaum (sie hoeren auf Signale).
- `qurcuma-server` ist ein eigenes CMake-Target ohne Widgets/Quick3D: `simulationworker.cpp`, `forceinjector.cpp`, `lesson.cpp` (Config-JSON), Protokollcode, curcuma_core. Es laeuft auf Rechnern ohne Display.
- Voraussetzung: `Atom/Bond` in einen GUI-freien Header `moleculetypes.h`; in `MoleculeViewer` bleiben `using Atom = ...; using Bond = ...;`, damit die ~50 Aufrufstellen unveraendert bleiben.

## Protokoll (Entwurf)

- Eine WebSocket-Verbindung, Text-Frames fuer Steuerung (JSON), Binaer-Frames fuer Simulationsframes.
- Steuerung A->B: `hello{protocol, token, qurcuma, curcuma_commit}`, `start{config (simConfigToJson), atoms (Element, Position, Ladung, Radius, Typ), bonds, liveNci}`, `stop`, `pause`, `resume`, `step`, `injectForce{atom, force, alpha, maxShells}`, `clearForce`, `setTemperature`, `setWallTemp`, `setWallBeta`.
- Steuerung B->A: `welcome{capabilities}` (verfuegbare Methoden, GPU-Backends, Threads, curcuma-Version), `runParameters`, `paused`, `finished{reason, aborted}`, `error`.
- Frame B->A (binaer): Schritt, Atomzahl, Flags, `float32` x,y,z je Atom, Energie/ekin/T/Ziel-T als `double`; optional Bindungen mit `topologyVersion` (nur bei Aenderung), Ereignisse, NCI-Kontakte. Elemente und Atomreihenfolge gehen nur einmal in `start`. Groessenordnung: 1000 Atome sind etwa 12 kB je Frame.
- Gegendruck: ist die Sende-Warteschlange des Sockets ueber einer Schwelle, verwirft der Server den Frame (latest-wins, wie die Frame-Koaleszenz im Viewer, `WP-performance.md` P3). Die Simulation wird dadurch nie von einem langsamen Link gebremst.
- Greifkraft: bleibt "klebrig" wie lokal (`injectForce` gilt bis `clearForce`); bricht die Verbindung ab, loescht der Server die Kraft.
- Version: Server und Client lehnen ab, wenn `protocol` nicht passt; unterschiedliche curcuma-Staende werden gewarnt (Parameter-Schema kann abweichen).

## Verbindung und Sicherheit

- A startet pro Sitzung einen Prozess zwei ssh-Prozesse: `ssh <Host> '<server> --port 0 --token-stdin --once'` (Token ueber stdin, nicht auf der Kommandozeile von B) und danach `ssh -N -L <lokal>:127.0.0.1:<port> <Host>`. Host-Auswahl aus `SshConfigParser`, Pfad zum Server pro Host einstellbar. Auth, Schluessel und ProxyJump liefert OpenSSH.
- Der Server lauscht nur auf `127.0.0.1`. Der Token ist trotzdem noetig: andere Benutzer auf B koennen localhost erreichen.
- Parameter-Durchreichung ist eine Angriffsflaeche: `mdExtraParams` und Dateiparameter greifen auf Pfade auf B zu. Der Server prueft Schluessel gegen die Registry und behandelt Dateiparameter nach den Regeln in "Dateizugriff".
- Erweiterung auf TCP: Transport hinter einer kleinen Schnittstelle (`Transport`: SSH-Tunnel, spaeter TCP+TLS mit Token/Zertifikat). Erst entscheiden, wenn SSH laeuft.

## Etappen

1. **R0 Entkopplung** (lokal, kein Netz): `moleculetypes.h`, `SimulationBackend`, `LocalBackend`, `SimulationControlWidget` baut ueber eine Fabrik. Erwartung: verhaltensgleich; `test_recipes`, `test_nci*`, `test_fragments` bauen und laufen, ein MD-Lauf und ein Opt-Lauf im GUI werden vom Operator geprueft.
2. **R1 Protokoll und Server**: `src/remote/` mit Serialisierung (Config, Atome, Frame, `putFile`), Sitzungsverzeichnis und Pfadpruefung (Test: `..`, absolute Pfade, Symlinks werden abgelehnt) und `qurcuma-server`; Test `tests/test_remote_protocol.cpp` (Roundtrip aller Felder) und ein Loopback-Test ohne GUI: Server + Testclient auf einem Rechner, ein deterministischer Lauf (Optimierung oder NVE bei fester Threadzahl) liefert dieselben Positionen wie der lokale Worker.
3. **R2 Client und Verbindung**: `RemoteBackend`, `SshTunnel`, Dock-Auswahl "Rechnen auf: dieser Rechner / <Host>", Capabilities von B ersetzen die lokale GPU-/Methodenliste, Statuszeile mit Frame-Rate, verworfenen Frames und Round-Trip-Zeit. Erster Test auf zwei Rechnern durch den Operator.
4. **R3 Robustheit**: Wiederverbinden und Wiederanbinden an einen laufenden Lauf (`attach`), Verhalten bei Abbruch (Lauf weiter bis Timeout, danach Stop), Arbeitsverzeichnis je Sitzung auf B (stale-`stop`-Datei, siehe `src/CLAUDE.md`), Trajektorie-Schreiber auf A aus den Frames; Dateien auf B bleiben ein spaeterer Schritt (siehe "Dateizugriff").
5. **R4 Dateien auf B** (Datei-Dock fuer B, `getFile`, vollstaendige Trajektorie von B; siehe "Dateizugriff").
6. **R5 VR** (eigene Stufe, siehe unten; unabhaengig von R4).

## VR-Stufe (R5)

- Qt Quick 3D XR (`Qt6Quick3DXr`) liest dasselbe Szenenmodell; die VR-Szene laeuft auf A, entkoppelt von B: gerendert wird mit 72 bis 90 Hz aus dem zuletzt empfangenen Frame, B liefert unabhaengig davon Frames. Die bestehende latest-wins-Kette (`onWorkerFrameReady`) ist dafuer die Vorlage.
- Controller-Ray zum Picken und Greifen speist denselben `atomForceRequested`-Pfad wie die Maus; ueber `RemoteBackend` gelangt die Kraft zu B. Die Schleife Controller -> B -> Frame -> A hat eine Verzoegerung, die gemessen und im Statusfeld angezeigt werden muss, bevor VR-Greifen beurteilt wird.
- Offen und ungeprueft: ob `XrView` mit dem bestehenden `QQuickWidget`-Viewer und `viewer3d.qml` zusammen betrieben werden kann oder ein eigenes Fenster braucht; ob die Szene (Instancing, Shader, Effekte) in Stereo funktioniert. Dafuer zuerst ein Spike unter `spikes/xr/` (Monado als Runtime ohne Headset), danach Hardware. Brille und Runtime (Pico 4 ueber ALVR/Pico Connect oder anderes Headset) sind noch festzulegen; Monado und ALVR sind auf A nicht installiert.
- Rendern auf A (PC-VR). Standalone auf dem Headset ist wegen curcuma ausgeschlossen, weil B rechnet.

## Dateizugriff

Grundsatz: **Eingaben kommen von A, Ergebnisse gehen nach A.** B hat nur ein Sitzungsverzeichnis. Dateien von B (Browser, Auswahl, Abholen) kommen spaeter.

Was Dateien in einem Lauf bedeutet (am Quelltext gelesen):
- **Struktur**: A liest die Datei (`moleculefileloader`) und schickt Atome und Bindungen in `start`. B braucht keinen Dateipfad der Struktur. Das gilt auch fuer Lessons (`*.qlesson.json`, A-seitig entpackt).
- **Dateiparameter der Konfiguration**: curcuma liest sie als einfache Strings im Dateisystem des Prozesses (also auf B). Vollstaendige Liste siehe unten.
- **Ausgaben von curcuma**: `write_xyz`, Restart-Dateien, `.param.json`/`.topo.json`-Caches, Diagnose-CSV, `.diag.jsonl` landen im Arbeitsverzeichnis des Prozesses (auf B).

Regeln (Default):
1. **Upload statt Pfad.** Verweist ein Dateiparameter auf eine Datei auf A, liest A sie, schickt sie als `putFile{name, bytes, sha256}` und der Server ersetzt den Parameter durch den Pfad im Sitzungsverzeichnis. Groessenlimit (Vorschlag 64 MB je Datei, 256 MB je Sitzung); gleiche Hashes werden nicht erneut gesendet.
2. **Kein Zugriff auf andere Pfade auf B.** Der Server nimmt nur Namen ohne Verzeichnisanteil (`..`, absolute Pfade, Symlinks abgelehnt) und schreibt ausschliesslich in `<Sitzungsverzeichnis>/in/`. Ein Dateiparameter mit einem Pfad, der nicht aus einem Upload stammt, wird abgelehnt (kein stilles Durchreichen).
3. **Trajektorie auf A.** A schreibt die XYZ-Trajektorie aus den Frames, die bei A ankommen, ins Arbeitsverzeichnis von A (wie lokal). `write_xyz` auf B bleibt aus. Bei langsamer Leitung laesst der Server Frames weg (Gegendruck); dann hat die Datei auf A Luecken in den Schrittnummern, der Lauf selbst nicht. Die Statuszeile zeigt, wie viele Frames fehlen.
4. **Ausgabedateien auf B** (Restart, Diagnose, Caches) bleiben im Sitzungsverzeichnis `<server-root>/<sitzung>/out/` und werden in dieser Stufe nicht abgeholt. Das Dock zeigt Pfad und Host an; die Dateien sind ueber ssh/scp erreichbar.
5. **Sitzungsverzeichnis** je Verbindung, vom Server angelegt und zugleich Arbeitsverzeichnis des Workers (loest die stale-`stop`-Falle: dort liegt nie eine fremde `stop`-Datei). Aufraeumen nach Sitzungsende nur auf Wunsch (Einstellung), Standard: behalten.

### Dateiparameter (vollstaendig, Stand 2026-10-01)

Durchsucht: alle `PARAM(..., String, ...)` in den Headern von `~/src/curcuma/src`, eingegrenzt auf Name/Hilfetext mit file/path/dir/json/restart/cache/input/output, danach auf die Module, die ein qurcuma-Lauf erreicht. Die Registry kennzeichnet Pfadparameter nicht; die Zuordnung "Datei" folgt aus Name und Hilfetext.

| Modul | Parameter | Bedeutung | Erreichbar aus qurcuma | Behandlung |
|---|---|---|---|---|
| `simplemd` | `rmsd_mtd_ref_file` | Referenzstrukturen fuer RMSD-MTD (Eingabe) | ja: Feld im Simulation-Dock, `SimulationConfig::rmsdMtdRefFile`, in Lessons gespeichert | Upload |
| `simplemd` | `restart_file` | Restart-Zustand laden (Eingabe) | nur ueber den Tab "All parameters" (`mdExtraParams`); qurcuma setzt `no_restart=true` | Upload |
| `simplemd` | `plumed_file` | PLUMED-Eingabe (Eingabe) | nur ueber "All parameters" | Upload |
| `forcefieldgenerator` | `load_ff_json` | Kraftfeld-JSON (Eingabe) | nein: qurcuma reicht nur das Modul `simplemd` weiter (Route nicht vollstaendig geprueft) | abgelehnt |
| `orcainterface` | `orca_executable`, `orca_basename` | Programmpfad und Dateiname auf der Rechenmaschine | nein | nie aus Client-Daten, immer abgelehnt |
| `opt` und Optimierer | keine String-Parameter mit Dateibedeutung (nur `method`, `optimizer`, `convergence_preset`, `lbfgs_line_search`, `rfo_solver`) | | | nichts zu tun |

Nicht dateiwertig, obwohl String: `method`, `thermostat`, `wall_type`, `wall_potential`, `rmsd_mtd_atoms`, `rmsd_mtd_scheme`, `temp_schedule`.

Parameter anderer Capabilities (`hessian`, `docking`, `confscan`, `rmsdtraj`, `qmdfffit`, `casino`, ...) nehmen ebenfalls Dateipfade, sind aber nicht Teil eines qurcuma-Laufs: ueber den Worker laufen nur MD und Optimierung.

Implizite Dateien ohne Parameter: der Parameter-Cache des Kraftfelds heisst nach der Geometriedatei (`<name>.param.json`, `forcefield.cpp:791`) und die `stop`-Datei im Arbeitsverzeichnis (`CheckStop`). Ob der Worker (Molekuel aus dem Speicher) eine Geometriedatei setzt und wohin der Cache dann geht, ist nicht geprueft; das Sitzungsverzeichnis als Arbeitsverzeichnis haelt beides in jedem Fall innerhalb einer Sitzung.

Der Server fuehrt `kFileParams = {rmsd_mtd_ref_file, restart_file, plumed_file}` (Eingabe, Upload) und eine Sperrliste (`load_ff_json`, `orca_*`). Ein anderer String-Parameter, der wie ein Pfad aussieht (enthaelt `/` oder einen Backslash, endet auf `.xyz`, `.json`, `.dat`), wird abgelehnt. Ein Test vergleicht die Registry (String-Parameter mit "file"/"path" im Hilfetext) mit diesen Listen und schlaegt bei einem unbekannten Treffer fehl, damit neue curcuma-Parameter nicht unbemerkt durchrutschen.

### Lessons

Eine Lesson ist ein Paket aus Strukturen (inline XYZ) und je einer vollstaendigen `SimulationConfig` (`lesson.cpp`). Sie wird auf A geoeffnet und entpackt (`extractLesson`); Atome und Konfiguration gehen beim Start des Laufs an B.

- Strukturen und Konfiguration brauchen auf B nichts vorab. Lessons laufen remote ohne Aenderung am Lessonformat, solange der Lauf ueber das Backend startet (`startWithConfig`).
- Dateiparameter einer Lesson (`rmsdMtdRefFile`, Inhalt von `mdExtraParams`; beides wird gespeichert, `lesson.cpp:98`, `lesson.cpp:132`) werden beim Start auf A aufgeloest (relativ zum Lesson-Verzeichnis, dann zum Arbeitsverzeichnis) und hochgeladen. Fehlt die Datei auf A, bricht der Start mit klarer Meldung ab, nicht erst auf B.
- Vorhandene Luecke, unabhaengig von Remote: `rmsdMtdRefFile` steht als Pfad in der Lesson und wird beim Speichern nicht eingepackt. Eine auf einen anderen Rechner kopierte Lesson findet die Datei auch lokal nicht. Vorschlag: beim Speichern den Dateiinhalt in die Lesson aufnehmen und beim Entpacken neben die Strukturen legen. Nicht Teil von R0 bis R3, braucht eine Entscheidung.
- Verbindungsdaten (Host, Port, Token) gehoeren nie in eine Lesson. "Rechnen auf" ist eine Einstellung des Benutzers auf A; dieselbe Lesson laeuft lokal und remote.
- Rezepte (`recipe.h`): enthalten Protokollparameter, keine Dateiparameter erkennbar (`recipes::keptKeys` und der Rezeptinhalt wurden nicht Schluessel fuer Schluessel durchgesehen).
- Ist "Rechnen auf B" gewaehlt und B nicht erreichbar, startet nichts; es gibt keinen stillen Rueckfall auf lokal. Das Dock zeigt immer, wo gerechnet wird.

Trajektorie in zwei Schritten (entschieden):
1. **Variante 1, ab R2:** A schreibt die Datei aus den Frames (Regel 3), mit moeglichen Luecken.
2. **Variante 2, mit der Stufe "Dateien auf B":** B schreibt die vollstaendige Trajektorie (`write_xyz` im Sitzungsverzeichnis, optional je Lauf einschaltbar), A holt sie nach dem Lauf oder auf Wunsch ab (`getFile`, Groessenanzeige vorher). Die Datei von B ersetzt die lueckenhafte auf A nicht automatisch; beide Namen unterscheiden sich (`<name>.trj.xyz` auf A aus Frames, `<name>.full.xyz` von B).

Spaetere Stufe "Dateien auf B" (nicht Teil von R0 bis R3), Optionen:
- Dateibaum von B im Datei-Dock: Listen/Lesen ueber das Protokoll (`list`, `getFile`) mit Wurzel `<server-root>` und derselben Pfadpruefung, oder ueber `sftpcache` (libssh, `USE_SFTP`) mit eigener Anmeldung. Das Protokoll braucht keine zweite Verbindung und erbt Token und Tunnel; SFTP deckt beliebige Pfade ab, hat aber eigene Authentifizierung und die `USE_SFTP`-Abhaengigkeit.
- Strukturen von B laden: `getFile` liefert die Bytes, A parst sie lokal und ruft danach den normalen Pfad auf.
- Entscheidung zwischen Protokoll und SFTP erst dann, mit Blick auf den Bedarf (nur Sitzungsergebnisse oder beliebige Verzeichnisse auf B).

## Nicht im Umfang (zunaechst)

- Remote `NciAnalysisWorker`, `RMSDWidget`, `CalculationRunner`, Dateibrowser auf B: bleiben lokal auf A. Die NCI-Kontakte der Live-MD kommen dagegen aus dem Frame von B.
- Mehrere gleichzeitige Clients oder Laeufe pro Server.
- Eigene Authentifizierung ausser Token.

## Offene Fragen

- Parameter-Tab: liest `ParameterRegistry` von A; wenn B ein anderes curcuma hat, stimmen Defaults und Schluessel nicht. Pruefen, ob die Registry als JSON exportierbar ist und in `welcome` mitgeschickt werden kann (nicht geprueft).
- Lesson-Luecke: Referenzdatei der RMSD-MTD in die Lesson einpacken (Vorschlag oben)? Entscheidung offen.
- Trajektorie: Entschieden: erst Variante 1 (A schreibt aus den Frames, R2), dann Variante 2 (vollstaendige Datei von B, nach "Dateien auf B").
- Verhalten beim Verbindungsabbruch: Default festlegen (Vorschlag: Lauf bis 60 s weiter, dann Stop).
- GPU-Auswahl auf B (CUDA/ROCm): kommt aus `welcome`, Fehlerfall `-gpu_strict` pruefen.

## Pruefung

- Automatisch: Protokoll-Roundtrip und Loopback-Vergleich (R1), alle bestehenden Test-Targets nach R0.
- Durch den Operator (GUI/3D/VR sind aus der Agent-Shell nicht pruefbar): MD und Opt lokal wie vorher (R0), Lauf ueber SSH auf B mit Greifkraft und Temperatur-Regler (R2), Verbindungsabbruch (R3), VR-Spike (R5). Status bleibt bis dahin "ADDED"/"machine-tested", nie "TESTED".
