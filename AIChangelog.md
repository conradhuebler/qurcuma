# AIChangelog - Qurcuma Improvements

Newest first; one line per fact. Entries from August 2026 and earlier: [docs/changelog/AIChangelog_2026-08-and-earlier.md](docs/changelog/AIChangelog_2026-08-and-earlier.md).

## Oktober 2026 - remote-control mit main zusammengeführt

- **Ein Atomtyp**: `MolAtom`/`MolBond` (`moleculetypes.h`) sind Aliase auf `moldata::Atom/Bond` (`core/moleculedata.h`); beide Linien hatten den Datensatz getrennt herausgelöst.
- **`SimulationBackend` trägt die neuen Worker-Aufrufe**: Kraft je Quelle (`injectForce(source, ...)`, `clearInjectedForce(source)`), `injectForces`, `setExternalPotentials`; das Remote-Protokoll führt sie als optionales Feld `source` und die Nachrichten `injectForces`/`setPotentials` (`kProtocolVersion` bleibt 1).
- **`LocalBackend` wartet 15 s** auf den Worker-Thread und löst ihn sonst vom Elternobjekt (vorher 2 s, SIGABRT beim Schließen mitten im Lauf).
- Geprüft: `qurcuma` baut (debug); `test_remote_protocol`, `test_remote_loopback`, `test_remote_reconnect`, `test_remotepaths`, `test_fragments`, `test_tooldispatcher`, `test_scriptinterpreter` laufen durch. Nicht geprüft: GUI, VR-Headset, ein echter Remote-Lauf mit Kräften.

## Oktober 2026 - Remote Files entfernt

- **SFTP benutzt die Schlüsseldatei des Computers**: ein gesetzter Schlüssel wird vor Agent und Standardschlüsseln probiert (ein `.pub`-Pfad führt auf den privaten Schlüssel daneben, sonst der `IdentityFile` des ssh-Alias); ist er passwortgeschützt, gibt es eine Rückfrage nach der Passphrase (nicht gespeichert, Abbrechen geht ohne Schlüssel weiter). ⚙️ `test_remotepaths` für Pfad und Weitergabe; die Anmeldung mit libssh ist gebaut, nicht gegen einen Server ausprobiert.
- **Zweiter Remote-Dateibrowser entfernt**: File > "Remote Files (qurcuma-server)...", `RemoteFilesDialog`, der Client `remote::RemoteFiles` (eigener Tunnel und eigener Serverprozess) und `test_remote_files` sind gelöscht, auch aus der Quellliste von `qurcuma` und `qurcuma_remote`.
- **Server ohne Dateibrowsen**: `list`, `getFile`, `cancelFile` mit `handleList`, `handleGetFile`, `pumpTransfer`, `finishTransfer` und dem Übertragungszustand, `resolveBrowsePath`, `encodeChunk`/`decodeChunk`, `kChunkBytes`, `MsgKind::FileChunk` (Wert 3 bleibt gesperrt), der Konstruktorparameter `browseRoots` von `ServerSession`/`RemoteServer` und die Option `--browse` von qurcuma-server sind entfernt. Behalten: Uploads (`encodeFile`, `applyFilePolicy`, `uploadParamKeys`/`blockedParamKeys`) und `sessionDir`/`sessionFiles` in `finished`.
- **`kProtocolVersion` bleibt 1**: Läufe nutzen keine der entfernten Nachrichten; ein alter Client, der noch `list` schickt, bekommt eine Fehlermeldung statt einer Liste. Ein von Hand mit `--browse` gestarteter Server startet nicht mehr (unbekannte Option).
- **"Show results in Remote"**: nach einem Remote-Lauf meldet `RemoteBackend::sessionResultsAvailable(Computer, Sitzungsverzeichnis)`; mit SFTP-Build zeigt das Simulation-Panel darunter einen flachen Knopf, der den Project-Dock im Remote-Segment öffnet, den Computer (`computer:<Name>`) wählt, bei Bedarf verbindet und das Sitzungsverzeichnis zum Remote-Arbeitsverzeichnis macht (`RemoteBrowserPanel::showLocation`). Ist der Computer nicht mehr in der Liste, sagt das die Statuszeile des Panels. Ohne SFTP bleibt nur der Statustext. 🤖 gebaut, GUI nicht geprüft, nicht gegen einen SFTP-Server geprüft.
- **Tooltip** von "Also write the full trajectory on the server": die Datei liegt im Sitzungsverzeichnis und lässt sich mit SFTP-Build über "Show results in Remote" im Remote-Segment öffnen oder herunterladen.
- **Testabdeckung umgezogen**: `test_remote_loopback` prüft jetzt, dass die vollständige Trajektorie im Sitzungsverzeichnis liegt und in `finished` steht (vorher in `test_remote_files`); `test_remote_ssh` prüft, dass `sessionResultsAvailable` Computername und Sitzungsverzeichnis liefert.
- **Prüfung**: `qurcuma`, `qurcuma-server`, `test_remotepaths` (109 Prüfungen), `test_remote_protocol`, `test_remote_loopback`, `test_remote_reconnect`, `test_remote_ssh`, `test_remote_hosts`, `test_sshconfig`, `test_programpaths` (78) bauen und laufen grün; die Remote-Tests geben keine Anzahl aus. Nicht geprüft: GUI, ein SFTP-Server, ein Build ohne libssh.

## Oktober 2026 - Remote-Segment wie lokal

- **Aufbau wie das Files-Segment**: Remote-Knopf "Choose working directory" an der Stelle des lokalen in der oberen Zeile, Breadcrumb des Remote-Arbeitsverzeichnisses, Liste seiner Unterordner mit ".." und "+ New Directory", darunter Filterzeile und die Dateien des oben gewählten Ordners. Darüber bleibt eine kompakte Zeile mit Quelle, Connect/Disconnect und Statuszeile. 🤖 gebaut, GUI nicht geprüft.
- **Bedienung von der lokalen Liste übernommen**: Klick (oder Wechsel des aktuellen Eintrags) auf einen Ordner zeigt unten seine Dateien, ".." die des übergeordneten; Kontextmenü Add to Bookmarks / Set as Working Directory; Klick im Breadcrumb setzt das Arbeitsverzeichnis. Dateien öffnen weiter per Doppelklick (lokal: Einfachklick), weil dabei heruntergeladen wird. 🤖 GUI nicht geprüft.
- **Remote-Arbeitsverzeichnis pro Quelle**: gespeichert unter `remote/workdir/<prozentkodierter Quellschlüssel>` (Lesezeichen-ID oder `host:user@host:port`), erst nachdem es gelistet werden konnte; beim nächsten Verbinden wiederhergestellt, sonst das Remote-Home; ein verschwundenes fällt mit Fehlermeldung auf Home zurück. ⚙️ Schlüssel in `test_remotepaths`, nicht gegen einen Server geprüft.
- **Auswahldialog**: "Choose" öffnet einen modalen Dialog (Breadcrumb, Ordnerliste, Pfadfeld mit `/`, `~` oder relativem Namen, Select/Cancel) auf der bestehenden Verbindung; Select prüft einen getippten Pfad erst durch Auflisten. Ohne Verbindung wird die gewählte Quelle zuerst verbunden. 🤖 gebaut, nicht gegen einen Server geprüft.
- **Lesezeichen**: der Stern schaltet jetzt das Remote-Arbeitsverzeichnis, ein gewähltes Lesezeichen wird zum Arbeitsverzeichnis; Speicherung unverändert (`Settings::RemoteMountPoint`).
- **"+ New Directory"**: Name nicht leer, nicht "." oder "..", ohne '/' (`remotepaths::isValidNewDirectoryName`), bestehende Namen werden abgelehnt; `SftpClient::makeDirectory` prüft mit `sftp_lstat` und legt mit `sftp_mkdir` (0755) an, danach ist der neue Ordner gewählt. Namensvorschlag wie lokal (`<gewählter Ordner>_N`). ⚙️ Namensregeln in `test_remotepaths`, `sftp_mkdir` nicht gegen einen Server geprüft.
- **Filter geteilt statt kopiert**: `DirectoryFilterProxyModel` und die neue `FileFilterBar` (Suche, Extensions-Menü, Löschen) liegen in `src/docks/directoryfilter.*` und dienen lokaler und Remote-Liste; der Proxy liest `QFileSystemModel` direkt, andere Modelle über `IsDirRole`, die Regel ist `remotepaths::fileFilterAccepts`. ⚙️ Regel in `test_remotepaths`; lokale Liste im GUI nicht geprüft.
- **Lokaler Filter, eine Änderung**: Dateien ohne Endung blieben bei angehaktem "<no extension>" unsichtbar (der Menüeintrag trug den Text statt der leeren Endung); jetzt werden sie gezeigt. Sonst gleiche Texte, Reihenfolge und Merkverhalten. ⚙️ `test_remotepaths`, GUI nicht geprüft.
- **SftpClient**: Listenanfragen tragen eine Request-ID (Arbeitsverzeichnis, gewählter Ordner, Auswahldialog); "Download to Working Directory" heißt jetzt "Download to Local Working Directory".
- **Prüfung**: `qurcuma`, `test_remotepaths` (109 Prüfungen, neu: Endungen, Dateifilter über eine Remote-Liste, Settings-Schlüssel, Ordnernamen, Namensvorschlag), `test_sshconfig`, `test_remote_hosts`, `test_programpaths` bauen und laufen grün; `check_docs.py` 0 Fehler. Ohne `USE_SFTP` nur Syntaxprüfung von `mainwindow.cpp`, `projectdock.cpp`, `directoryfilter.cpp`, `remotepaths.cpp`. Kein SFTP-Server, GUI nicht geprüft.

## Oktober 2026 - Remote-Browser im Project-Dock

- **Remote-Segment als Browser**: `RemoteBrowserPanel` ersetzt die Liste der Remote-Verzeichnisse und den modalen SFTP-Dialog. Oben Quelle (Computer der Liste, dann SFTP-Profile, dann "Other host..." mit "Save as SFTP profile"), Connect/Disconnect und eine Statuszeile; darunter Breadcrumb (Home = Remote-Home), Up, Refresh und die Liste des Ordners (Ordner zuerst, ohne Beachtung der Groß-/Kleinschreibung). Im Remote-Segment sind lokaler Breadcrumb und lokale Dateiliste ausgeblendet. 🤖 gebaut, GUI nicht geprüft.
- **Öffnen und Herunterladen**: Doppelklick betritt einen Ordner oder lädt die Datei in den SFTP-Cache (`<Cache>/<sha256>/<Dateiname>`, damit der Viewer den echten Namen zeigt) und öffnet sie über `loadMoleculeFile`; eine Kopie mit gleicher Größe und nicht älterer Änderungszeit wird ohne Download geöffnet. Kontextmenü: Open, Download to Working Directory, Copy Remote Path, Add to Bookmarks. 🤖 gebaut, nicht gegen einen Server geprüft.
- **Remote-Lesezeichen**: Stern schaltet den aktuellen Ordner, ein Menü öffnet oder entfernt Lesezeichen und verbindet bei Bedarf zuerst; gespeichert als `Settings::RemoteMountPoint`, `profileId` = SFTP-Profil-ID oder `computer:<name>`. Nicht gespeicherte Hosts können keine Lesezeichen haben. ⚙️ ID-Abbildung in `test_remotepaths`, GUI nicht geprüft.
- **libssh im eigenen Thread**: `SftpClient` (`src/sftpclient.*`) übernimmt Verbindung, ProxyCommand, Schlüssel- und Passwortanmeldung, Auflisten und Download aus dem früheren `SftpItemModel`; ein Client pro Verbindung in einem eigenen `QThread`, Disconnect gibt ihn auf, statt den GUI-Thread zu blockieren (libssh-Zeitlimit 20 s pro Wartevorgang). ⚙️ gebaut und gelinkt mit libssh 0.12.2; nicht gegen einen SFTP-Server ausgeführt, keiner im Testaufbau.
- **Anmeldung**: Host-Key gegen `~/.ssh/known_hosts` (unbekannt: Rückfrage mit SHA256-Fingerabdruck, bei Ja Eintrag in known_hosts; geändert: Abbruch mit Erklärung), dann ssh-Agent und Standardschlüssel, dann einmal ein Passwort (Methode password oder keyboard-interactive), das nicht gespeichert wird. Fehlertexte nennen die Ursache (nicht erreichbar, Anmeldung abgelehnt, Zugriff verweigert, Ordner fehlt, Verbindung verloren). Neu gegenüber dem alten Dialog: Host-Key-Prüfung, keyboard-interactive, symbolische Links auf Ordner werden als Ordner gezeigt. Nicht gegen einen Server geprüft.
- **Computerliste**: wie bisher nur Adresse, Benutzer und Port, `qurcuma-server` wird nie kontaktiert; ist die Adresse ein Alias der `~/.ssh/config`, gelten zuerst dessen HostName/User/Port/ProxyJump, darüber die Angaben des Computers (`remotepaths::connectionForHost`). Bei SFTP-Profilen und "Other host..." bedeutet Port 0 "nicht angegeben". ⚙️ `test_remotepaths`.
- **File ▸ Open Remote File... (Ctrl+Shift+R)** zeigt den Project-Dock im Remote-Segment und setzt den Fokus auf die Quellenauswahl; File ▸ Recent Remote Connections wählt das Profil dort und verbindet. Der Eintrag "Clear Recent Connections" löschte schon bisher alle SFTP-Profile und heißt jetzt "Delete Saved SFTP Profiles..." mit Rückfrage. 🤖 gebaut, GUI nicht geprüft.
- **Entfernt**: `SftpDialog`, `SftpItemModel` (`sftpmodel.hpp`), `RemoteDirectoriesPanel`, in MainWindow `onAddRemoteDirectoryClicked`, `updateRemoteDirectoriesView`, `onRemoteDirectoryClicked`, `onRemoteFileDoubleClicked`, `downloadAndLoadRemoteFile`, `m_remoteDirectoriesView`, `m_remoteSftpModels`, `m_currentRemoteMountId`. Behalten: `sftpcache.*`, `sshconfig.*`, SFTP-Profile und Remote-Lesezeichen in `Settings`.
- **BreadcrumbBar**: Pfade außerhalb des Home-Ordners verloren ohne abschließenden Schrägstrich das letzte Zeichen, und `/home/ab` galt bei Home `/home/a` als darin; die Zerlegung ist jetzt `remotepaths::breadcrumbSegments`, gemeinsam für lokalen und Remote-Breadcrumb. ⚙️ `test_remotepaths`, lokale Anzeige im GUI nicht geprüft.
- **Prüfung**: `qurcuma`, `test_remotepaths` (77 Prüfungen: Sortierung, Pfadfunktionen, Breadcrumb, Lesezeichen-IDs, Abbildung Computer/Profil auf Verbindungsdaten), `test_sshconfig`, `test_remote_hosts`, `test_programpaths` bauen und laufen grün. Ohne `USE_SFTP` nur Syntaxprüfung von `mainwindow.cpp`, `projectdock.cpp`, `breadcrumbbar.cpp`, `remotepaths.cpp` (ohne Fehler); ein vollständiger Build ohne libssh wurde nicht ausgeführt.

## Oktober 2026 - SFTP standardmäßig an

- **SFTP liest die Computerliste mit**: der SFTP-Dialog hat ein Feld "Computers" aus der Liste der Remote-Rechner und übernimmt Adresse, Benutzer und Port (ein ssh-Alias als Adresse wählt den Eintrag der `~/.ssh/config` mit); nichts davon braucht oder kontaktiert `qurcuma-server`. Die SFTP-Profile bleiben eine eigene Liste, die Schlüsseldatei eines Computers wird nicht benutzt (Agent und Standardschlüssel). ⚙️ `test_remote_hosts` für die Adressaufteilung; der Dialog ist gebaut, nicht im GUI geprüft.
- **SFTP (Open Remote File) ist jetzt standardmäßig gebaut**: `USE_SFTP` steht auf ON; fehlt libssh, meldet CMake eine Warnung und baut ohne SFTP, statt abzubrechen (CI, Windows). ⚙️ mit libssh 0.12.2 konfiguriert, gebaut und gelinkt; nicht gegen einen echten Server ausgeführt, der Dialog nicht im GUI geprüft.

## Oktober 2026 - Einstellungsdialog erweitert

- **Tote Instancing-Schwelle entfernt**: `instancingThreshold` wurde gesetzt und gespeichert, aber nirgends ausgewertet (Quick3D instanziert immer); Feld, Setter/Getter im Viewer und der gespeicherte Schlüssel sind weg, der alte Schlüssel wird beim Start gelöscht. ⚙️ gebaut.
- **Reiter "General"** im Einstellungsdialog: Arbeitsverzeichnis (aktuelles, Browse, Liste der bekannten mit Use/Remove), "Use the directory qurcuma was started from", Dark mode, Leeren von File ▸ Recent Files mit Anzahl und Rückfrage. Alles gilt erst mit OK. 🤖 gebaut, GUI nicht geprüft.
- **Reiter "Display"**: "Move the structure's center of mass to the origin after loading" (`VisualizationSettings::centerOnLoad`, die übrigen Felder bleiben unverändert); der Menüpunkt Preferences ▸ Center Molecule on Load bleibt und wird nachgeführt. 🤖 gebaut, GUI nicht geprüft.
- **Kein Regler für die Instancing-Schwelle**: `instancingThreshold` wird gespeichert, aber vom Viewer nicht gelesen (Quick3D instanziert immer, `view.h`); ein Regler hätte keine Wirkung.
- **Reiter "Computers"** (nur mit `QURCUMA_REMOTE`): Liste der gespeicherten Rechner mit Add/Edit/Remove über `AddComputerDialog`, wirkt sofort (nicht über OK/Cancel); "Compute on" im Simulation-Panel wird über `SimulationControlWidget::refreshComputers()` neu gefüllt. 🤖 gebaut, GUI nicht geprüft, Build ohne `QURCUMA_REMOTE` nicht geprüft.
- **Dialog kennt MainWindow nicht**: Signale `darkModeChanged`, `useInvocationDirectoryChanged`, `workingDirectoryChosen`, `centerOnLoadChanged`, `recentFilesCleared`, `computersChanged`; MainWindow wendet sie an und hält die Menü-Schalter mit blockierten Signalen synchron. ⚙️ `qurcuma`, `test_programpaths`, `test_sshconfig`, `test_remote_hosts` bauen und laufen grün; keine neue GUI-freie Logik, daher kein neuer Test.

## Oktober 2026 - Einstellungsdialog

- **Alte Platzhalter-Pfade werden aufgeräumt**: die Migration entfernt für jedes Programm die Werte des früheren Erststart-Standards (`/usr/local/bin/<programm>`, `C:/Program Files/<programm>`), wenn sie nicht gültig sind, damit "nicht gesetzt" und Detect erscheinen statt eines rot markierten Fantasiepfads. ⚙️ `test_programpaths`.
- **Ein Icon-Helfer**: `themeIcon()` in `src/widgets/themeicon.h` ersetzt die zwei Kopien (Einstellungsdialog, `AddComputerDialog::icon`).
- **Ein Einstellungsdialog** (`src/dialogs/settingsdialog.*`, Edit ▸ Preferences ▸ Settings..., Strg+,): Reiter "Programs" (Pfad, Browse, Detect, Statuszeile je Programm) und "Operator" (Name, ORCID, Institution, Lizenz) ersetzen "Configure Programs" und "Operator Metadata". 🤖 gebaut, GUI nicht geprüft.
- **Speichern erst bei OK**: Cancel verwirft alle Änderungen beider Reiter; vorher schrieb "..." einen Programmpfad sofort, auch wenn der Dialog danach abgebrochen wurde. 🤖 im Code so umgesetzt, GUI nicht geprüft.
- **Programmpfade ohne GUI prüfbar** (`src/programpaths.*`): Programmtabelle, `detect()` (PATH, übliche Ordner, `/opt/<name>*`, `~/<name>*`; Avogadro auch als `avogadro2`) und `validate()` (nicht gesetzt, ok, fehlt, nicht ausführbar, falsche Art). ⚙️ `test_programpaths` (72 Prüfungen) grün.
- **ORCA-Ordner braucht `orca_*`**: ein Ordner gilt nur als ORCA-Ordner, wenn neben `orca` mindestens ein ausführbares `orca_*` liegt; so wird der Screenreader Orca (`/usr/bin/orca` unter GNOME) nicht als ORCA erkannt. ⚙️ `test_programpaths`.
- **ORCID-Prüfung**: Format `0000-0000-0000-000X` und Prüfziffer nach ISO 7064 MOD 11-2, live im Reiter "Operator"; ob die iD registriert ist, wird nicht geprüft. ⚙️ `test_programpaths`.
- **ORCA unter `programs/orca`**: wie die anderen Programme; `orca/binaryPath` wird beim Start einmal übernommen und gelöscht, der Platzhalter `/usr/local/bin/orca` der alten Startwerte verworfen (außer dort liegt ein gültiger ORCA-Ordner); `orcaBinaryPath()` bleibt als Hülle, `setOrcaBinaryPath()` entfällt (letzter Aufrufer war der alte Dialog). ⚙️ 6 Migrationsfälle in `test_programpaths`, an einer echten Einstellungsdatei nicht geprüft.
- **Settings ohne tote Zugriffe**: `loadDefaults()` (schrieb beim ersten Start feste Pfade `/usr/local/bin/<programm>`), `saveSettings()` und die drei Workspace-Einstellungen `lastActiveWorkspaceId`, `autoSaveWorkspace`, `restoreLastWorkspace` samt Settern entfernt; keiner hatte einen Aufrufer.

## Oktober 2026 - Remote: Messpunkte

- **Remote-Link messbar**: der Client schreibt einmal pro Sekunde eine `perf:`-Zeile ins Remote-Log (Frames/s, kB/s, Anteil des GUI-Threads, Round-Trip) samt `linkStats` des Servers (erzeugt/gesendet/verworfen, Pending-Bytes, Kodierzeit); `ping`/`pong` ist neu im Protokoll. ⚙️ Loopback-Test, nicht über echtes ssh.
- **Rechner hinzufügen**: Dock "Compute on" hat `+`/`-`; der Dialog nimmt Adresse, Name, Port, Schlüssel und prüft per ssh (ohne Passwortabfrage) und Suche nach `qurcuma-server`, mit Hinweis bei den üblichen Fehlern (`ssh-copy-id`, Host-Key, Name nicht auflösbar). Eigene Liste in den qurcuma-Einstellungen, `~/.ssh/config` bleibt unangetastet; die Liste des Remote-Files-Dialogs ist dieselbe. ⚙️ `test_remote_hosts` (Validierung, ssh-Argumente, Verbindungstest gegen ssh-Stand-in); der Dialog selbst ist gebaut, aber nicht im GUI geprüft.
- **Ein Dialog für Test und Bearbeiten**: der Test-Knopf im Dock und der Testmodus des Dialogs entfallen; "Edit" öffnet den Dialog mit "Test connection" für jeden gespeicherten Rechner.
- **Rechnerlisten nur aus qurcuma**: die Liste im Dock und im Remote-Files-Dialog kommt nur aus den in qurcuma gespeicherten Rechnern; `~/.ssh/config` wird dafür nicht mehr gelesen. Eingetragene Adressen dürfen weiter ssh-Aliase sein (ssh wendet die Konfiguration an). Test, Bearbeiten und Entfernen gelten damit für jeden Eintrag außer "This computer".
- **`~/.ssh/config`-Parser liefert alle Einträge**: `SshConfigParser::parseFile` gab nur den letzten `Host` der Datei zurück (der vorherige wurde erst geprüft, nachdem `parseLine` ihn durch den neuen, leeren ersetzt hatte), `Host a b` ergibt jetzt beide Namen. Betrifft auch den SFTP-Dialog. ⚙️ `test_sshconfig` (vor der Korrektur 11 Prüfungen rot).
- **Verzeichnis als Server-Befehl wird ergänzt**: liegt `qurcuma-server` in dem getippten Verzeichnis (auch `~/...`), ergänzt der Test den Dateinamen, behält die Argumente und trägt den korrigierten Befehl ins Feld ein; ein bereits gespeichertes Verzeichnis meldet der Tunnel beim Lauf mit Ausweg (exit 126). ⚙️ `test_remote_hosts`.
- **Server-Befehl wird geprüft, Test für jeden Rechner**: ein eingetippter Server-Befehl wird auf dem Rechner geprüft (fehlt oder ist ein Verzeichnis: Schritt "Server" scheitert und nennt den gefundenen Server); exit 127 beim Start meldet den Befehl und verweist auf Test; der Dock-Knopf Test prüft auch Einträge aus `~/.ssh/config`. Dialog größer (660x480), Schritte und Knöpfe mit Icons. ⚙️ `test_remote_hosts`; Aussehen im GUI nicht geprüft.
- **Rechner bearbeiten, Test in drei Schritten**: gespeicherte Rechner lassen sich im Dock ändern (die für jeden Rechner gespeicherten Einstellungen ziehen bei Umbenennung mit); `~/.ssh/config`-Einträge sind ausgegraut mit Begründung. Der Test zeigt Login, Server und Sitzung einzeln mit Zustand, öffnet wirklich eine Sitzung bis `welcome` (Threads, GPU-Backends, Round-Trip) und sperrt während des Laufs alle Eingaben. ⚙️ `test_remote_hosts` (echte Sitzung gegen den gebauten Server über einen ssh-Stand-in); der Dialog selbst ist gebaut, aber nicht im GUI geprüft.
- **Remote: Sendebegrenzung**: der Server sendet höchstens 60 Frames/s (neuester Frame gewinnt; nicht mit Client-Trajektorie; nie Frames mit Reaktionsereignissen). ⚙️ Loopback-Test (Quelle 642 -> 60/s, Client-Trajektorie alle Frames), echte Leitung nicht geprüft.
- **Remote: Verwerfgrenze nach Linkrate**: ein Frame wird verworfen, wenn mehr ungesendet ist, als die Leitung in 0,25 s abführt (statt fester 4 MiB; RTT 5 s bei 7320 Atomen und 2,3 MB/s). ⚙️ nur gebaut und im Loopback gelaufen, auf der echten Leitung nicht gemessen.
- **MD-Takt nach Restzeit**: der Worker wartet nach jedem Schritt nur die Zeit bis zum Zielabstand (Single-Shot statt periodischem Timer, der bei Überlauf Takte ausließ: Limit 30 ergab 22 Schritte/s). ⚙️ Loopback-Test (Limit 30 -> 30/s), GUI nicht geprüft.
- **Bindungserkennung im Viewer per Zellliste** (`src/bonddetect.h`): `detectBonds`/`detectBondsHysteresis` prüften alle Paare mit zwei String-Nachschlagungen pro Paar (O(N²)); gemessen im `remote.log` 24 ms pro Frame bei 1410 Atomen und 610 bis 714 ms bei 7320 (bis zu 1,5 Durchläufe pro Sekunde). Jetzt gleiche Paare in gleicher Reihenfolge (Test `test_bonddetect` gegen die Schleife über alle Paare, 7320 Atome 25 ms im Debug-Build). ⚙️ GUI nicht geprüft.
- **Remote-Log: Einstellungen und GUI-Seite**: `run:`-Zeile mit Methode, GPU, fps-Limit, Thermostat, Topologie, Threads; `gui remote|local rate:` (gleiches Format für lokale und entfernte Läufe) und `gui viewer` (Frames ein/verarbeitet, Zeit in `updateSimulationFrame`, Verspätung eines 10-ms-Herzschlags im GUI-Event-Loop). 🤖 gebaut, GUI-Teil nicht ausgeführt.
## October 2026 - VR: Auswahl, Thermostat-Schieber, verschiebbare Panels

- Ausgewählte Atome tragen im Headset eine pulsierende Hülle (eigene Instanztabelle, gespiegelt, erst bei Auswahl sichtbar geschaltet); das Atom unter Controller-Strahl oder Fingerspitze eine helle Hülle passend zur Darstellung (rot beim Ziehen).
- Min/Max des Temperatur-Schiebers sind im Headset per Ziffernblock eintippbar (Eingabefeld antippen).
- Simulation-Panel: Temperatur-Schieber wie im Dock (linear, blau→rot, Min/Max in 50-K-Schritten), teilt Bereich und Wert mit dem Dock-Schieber; per Strahl oder Pinch ziehbar.
- Jedes Panel (auch die Menüleiste) hat einen Griff wie eine Fenster-Titelleiste; Trigger oder Pinch darauf verschiebt das Panel, es bleibt dem Kopf zugewandt; Recenter stellt das Layout wieder her.

## October 2026 - VR: Hand-Tracking als Eingabe

- View ▸ VR Input (und Input-Zeile im View-Panel im Headset): Controller (Strahl + Trigger) oder getrackte Hände (`VrHandInput.qml`); gespeichert in `vr/handInput`. Pinch (Daumen–Zeigefinger aus den Gelenken, mit Hysterese) wirkt nur auf das, was er beim Schließen berührt (Knopf oder gezeichnete Atomkugel, keine Strahlen): Knopf klicken; im Simulationslauf Atom ziehen (Molekül bleibt stehen), sonst Molekül halten/drehen/tragen, mit zwei Händen skalieren; kurzer Pinch auf ein Atom wählt es. Panels und Molekül rücken in Armreichweite; B/Y am Controller schaltet zurück.

## October 2026 - VR: Bewegen und Umgebung

- Linker Stick läuft (horizontal, in Blickrichtung, bis 1,5 m/s), rechter Stick dreht in 45°-Schritten um den Kopf; beides verschiebt den `XrOrigin`, Molekül, Panels und Umgebung bleiben stehen. Ein Snap-Turn während Trigger/Grip dreht das Molekül nicht mit.
- View ▸ VR Surroundings: 360°-Panorama als Skybox (equirektangular, .hdr/.jpg/.png; beleuchtet die Szene mit) und/oder eine 3D-Szene (glTF/GLB/OBJ/FBX per `RuntimeLoader`, Einheit Meter oder Zentimeter) auf dem Boden des Tracking-Raums; Auswahl wird gespeichert.

## October 2026 - VR: Panel-Knöpfe per Controller-Strahl

- Headset-Panels reagieren jetzt zuverlässig auf beide Controller („Open structure…“, Dateiliste, Stop usw.): Qt 6.11 pickt ein XrItem gegen seine unendliche Ebene und bricht beim ersten Treffer ohne Item ab, sodass die Menüleiste und versteckte Panels Klicks auf die Panels dahinter schluckten. `XrVirtualMouse` entfällt; `VrPanelHost::rayHit`/`buttonAt` testet das echte Rechteck, `Hand` (viewerxr.qml) setzt Hover/Down am `VrButton` und löst `clicked()` beim Loslassen aus.
- Atom-Auswahl (A/X) und Grab picken nur noch das Atom-Modell (`rayPick(..., atomModel)`), dieselben Panel-Ebenen lagen vor dem Molekül.
## Oktober 2026 - Assistent-Dock aus CuteChat

- `src/docks/chatdock.*` entfällt: der Assistent ist CuteChats `ChatDock`, gebaut vom `DockManager` mit `QurcumaChatHost`. Client und Agentenschleife laufen in dessen `ChatEngine` auf einem Worker-Thread (wartende Tools dürfen den GUI-Thread nicht blockieren), die Freigabe-Rückfrage kommt im GUI-Thread. Thinking, Rounds, Autonomie (inkl. Warnhinweis) und die faltbaren Reasoning-Blöcke sind jetzt Bibliotheksfunktionen. Neu gegenüber vorher: Chat-Historie, Export/Import, Endpunkt-Manager.
- Ein Praxistest im echten Programm (kwin virtuell, Stub-Endpoint) fand einen Deadlock: `ChatEngine::conversation()` wartete blockierend auf den Worker, der gerade ein Gui-Tool auf den GUI-Thread schickte. Jetzt wartet es in einer lokalen Event-Loop (Test `conversationCanBeAskedWhileAGuiToolIsInFlight`).
- curcuma auf master; der Parameter-Parser verlor beim Merge die `MODULE_INFO`-Auswertung (0 Module, `test_curcumaschemas` rot) und liest sie wieder, auch mehrteilige Beschreibungen.

## Oktober 2026 - Gemeinsame Bibliothek CuteChat für die Werkzeugschicht

- `src/core/tool*.{h,cpp}` und `src/llm/llm{client,config,session}.*` sind durch die Bibliothek **CuteChat** ersetzt (gleiche Quelle wie in SupraFit): `qurcuma_core` linkt nur `CuteChat::tools` (Qt Core), `qurcuma_llm` ist `CuteChat::core`. Der Dispatcher schreibt seine Audit-Zeile über `setLogSink` in den `LogHub`, die Sitzung meldet über `noted`. Neu aus SupraFit: Provider-Presets, Chat-Log/Export, Audit-Aufzeichnung, Chat-Verlauf mit Speicher-Backend, Fähigkeitsabfrage. Die bestehenden Tests `toolregistry`, `tooldispatcher`, `toolscore`, `llmclient`, `llmsession` laufen unverändert gegen die Bibliothek. Der `ChatDock` bleibt vorerst eigen (Worker-Thread-Modell, siehe `todo.md`). Branch `feature/shared-chat`.

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
