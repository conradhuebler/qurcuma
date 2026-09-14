# WP — Skript-Interpreter: rechnen lassen statt selbst rechnen

> **Status (14.09.2026):** S0 bis S4 sind umgesetzt (S0 bis S2 in einem Commit, weil S1
> ohne die Tabelle nichts zu rechnen hätte), S5 bis S7 sind geplant. Offen aus S4: die
> Bedienerprüfung an einem laufenden Endpunkt. Branch `feature/llm-tools`.
> **Zweck:** Das Modell soll Zahlen, die es bereits hat, nicht im Kopf verrechnen, sondern
> qurcuma rechnen lassen: Differenzen, Verhältnisse, Einheitenwechsel, Mittelwert,
> Streuung, Steigung. SupraFit hat dafür seine Skript-Engine (`ScriptingEngine` mit den
> Backends ExprTk, ChaiScript, Duktape, Python, QJS, Vorgabe ExprTk; `ScriptModel`
> kompiliert eine Gleichung einmal und wertet sie je Datenpunkt aus). qurcuma hatte bis
> hierher Rechenwerkzeuge für curcuma-Läufe (`run_single_point`, `job_status`), aber
> nichts für Arithmetik auf Zahlen, die schon vorliegen.

## 1. Gemessene Grundlage (14.09.2026, an dieser Maschine)

| Was | Befund | Folge |
|---|---|---|
| Qt | 6.11.2, `QJSEngine` in `Qt6Qml` | Für den 3D-Viewer ist `Qt6::Qml` schon im Build, also kein neues Paket |
| `QJSEngine::setInterrupted()` | laut Dokumentation thread-safe | Eine Endlosschleife ist von außen abbrechbar |
| Endlosschleife, Deadline 300 ms | endet nach **301 ms**, `stopped = true` | Die Deadline ist eine Wanduhr und greift |
| Stop aus zweitem Thread | beendet `while (true) {}`, Meldung „stopped at the operator's request" | Der Stop-Knopf funktioniert (`test_scriptinterpreter`) |
| Position beim Interrupt | `Error: Interrupted`, `lineNumber = 0`, kein Stack, keine Frames | **Eine Zeilenangabe ist bei Deadline und Stop nicht zu haben.** Der Abbruch nennt den Grund, nicht die Stelle |
| Position bei einem Skriptfehler | `line 1, column 4: SyntaxError: …`, `line 2: ReferenceError: …` | Fehler nennen die Stelle, wie geplant |
| `QJSEngine::newFunction` | existiert in 6.11 nicht (weder im installierten Header noch in der installierten Dokumentation) | Builtins stehen in **JavaScript**, `print` wird als Array zurückgelesen, die Werkzeug-Brücke ist ein QObject (`scriptbridge.h`) |
| `ldd /usr/lib/libQt6Qml.so.6` | zieht `libQt6Network`, ICU, `libproxy`, `libsystemd` | Die Skript-Bibliothek kommt **nicht** in `qurcuma_core`, dessen Eigenschaft „nur Core + Gui" bleibt |
| Wert des letzten Ausdrucks | `var a = 1; a * 2` → `2` (Test) | Der Ergebnis-Kontrakt der Qt-Doku („returns the result of the evaluation") trägt |
| `libQt6Qml`-freier Testlauf | `test_scriptinterpreter` läuft headless, ohne Anzeige | Die Bibliothek ist in ctest prüfbar |

## 2. Engine-Wahl (Entscheidung des Bedieners)

Verglichen wurden: eigene kleine Grammatik in `qurcuma_core`, **JavaScript über QJSEngine**,
ExprTk (SupraFits Vorgabe), ChaiScript (SupraFits Option). Gewählt wurde QJSEngine.

| | eigene Grammatik | **QJSEngine (gewählt)** | ExprTk | ChaiScript |
|---|---|---|---|---|
| Abhängigkeit | keine | `Qt6::Qml` (schon im Build) | 1,6 MB Header | neue Abhängigkeit für drei Plattformen |
| Modell schreibt es richtig | muss die Grammatik lernen | höchste Vertrautheit, `Math.*` gratis | C-nah, weniger vertraut | C++-nah |
| `log`-Mehrdeutigkeit | müsste definiert werden | entfällt: `Math.log` (ln) gegen `Math.log10` | entfällt | entfällt |
| Abbruch | eigener Zähler, exakt | thread-safe, **gemessen wirksam** | `loop_runtime_check`, nur Schleifen | nicht geprüft |
| Fehlerstelle | eigene Meldungen | Zeile und Spalte von der Engine | C++-gefärbt | C++-gefärbt |
| Kontrolle über die Semantik | vollständig | keine, die Engine ist eine Blackbox | teilweise | teilweise |

Das Gegenargument steht offen im Raum: eine eigene Grammatik hätte erlaubt, dem Modell in
jedem Fehlertext den Ausweg zu nennen, und `sd` als Stichprobe oder Bevölkerung wäre eine
Entscheidung von uns gewesen statt eine Konvention der Sprache. Der Ausschlag war, dass
dieselbe Zahl in zwei Sprachen verschieden heißt (`log` in Chemietexten gegen `log` in
Programmiersprachen) und dass ein Modell JavaScript ohnehin schreibt.

## 3. Entscheidungen

1. **`ToolEffect::Read` für das Rechenwerkzeug `calculate`.** Es rechnet auf Zahlen, die es
   mitbekommt, und ändert nichts. Eine Rückfrage pro Aufruf würde das Modell dazu erziehen,
   „Erlauben" reflexhaft zu klicken. Das widerspricht der Politikzeile „Rechnen und
   Schreiben mit Rückfrage" aus `WP-llm-tool-layer.md`, die aber über curcuma-Jobs von
   Minutenlänge stand. Die Änderung wäre ein Wort (`ToolEffect::Compute`).
2. **Der Modellpfad hat keine Werkzeug-Brücke.** Der Interpreter kann Werkzeuge aufrufen,
   wenn der Aufrufer eine `ScriptHost`-Brücke übergibt (Dock, Makro-Wiedergabe). Das
   Werkzeug des Assistenten wird ohne sie gebaut. Die Trennung ist damit strukturell und
   nicht eine Eigenschaft der Beschreibung.
3. **`available` bleibt ungesetzt.** Ein Taschenrechner muss ohne geladene Struktur
   arbeiten; eine Verfügbarkeitsprüfung würde ihn in genau dem Zustand entfernen, in dem
   „hier sind drei Zahlen" gefragt wird.
4. **`Date` und `Math.random` werden ersetzt.** Kein Sandkasten, sondern Nachvollziehbarkeit:
   derselbe Quelltext und dieselben Eingaben ergeben dasselbe Ergebnis. Wer sie benutzt,
   bekommt eine Meldung, die das sagt.
5. **Trigonometrie bleibt in Radiant.** `Math.sin` umzubiegen wäre eine Lüge über
   JavaScript. Stattdessen `radians(deg)` und `degrees(rad)` samt Satz in der Beschreibung.
6. **Die Builtins stehen in JavaScript**, nicht in C++. In dieser Qt-Version gibt es keinen
   Weg, aus C++ eine JS-Funktion zu definieren; jede C++-Builtin bräuchte ein eigenes
   QObject. Nur `print` kreuzt zurück, und zwar als Array, das nach dem Lauf gelesen wird.

## 4. Aufbau

`src/script/`, neues statisches Ziel `qurcuma_script` (`qurcuma_core` + `Qt6::Qml`,
headless):

| Datei | Inhalt |
|---|---|
| `scriptinterpreter.{h,cpp}` | `ScriptLimits`, `ScriptError`, `ScriptResult`, `ScriptHost`, `ScriptInterpreter::run`. Eine frische Engine je Lauf, Wächter-Thread für Deadline und Stop, Kappungen, Fehler mit Stelle |
| `scriptbuiltins.{h,cpp}` | Die Tabelle als JS-Quelltext plus `formatNumber`, das Zurücklesen von `print` und die Kappung von Ergebnissen |
| `scriptbridge.h` | `ScriptBridge : QObject` mit dem einen `Q_INVOKABLE tool(name, args)`, nur installiert, wenn der Aufrufer eine `ScriptHost` übergibt |

Der Ergebnis-Kontrakt: **der Wert des letzten Ausdrucks ist das Ergebnis**. Ist er ein Objekt,
werden seine Mitglieder die benannten Ergebnisse (`({dE: e1 - e2, kJ: ha_to_kjmol(e1 - e2)})`).
`print(...)` füllt die Ausgabeliste. Grenzen: 5 s, 64 Werte je Liste, 32 Ausgabezeilen,
2000 Zeichen für den Wert als Ganzes; was gekappt wurde, steht als `truncated` im Ergebnis.

## 5. Arbeitspakete

| WP | Inhalt | Fertig, wenn | Status |
|---|---|---|---|
| **S0** | Dieses Dokument | liest sich als Entscheidungsrecord | **erledigt** |
| **S1** | `scriptinterpreter.{h,cpp}`, `scriptbuiltins.{h,cpp}` (nur `print`/`sum`/`mean`), `scriptbridge.h`, `test_scriptinterpreter`, CMake | `test_scriptinterpreter` grün (45 Prüfungen): Ergebniskontrakt, Vorrang, Listen und Objekte, `print`, Bindungen, jede Fehlerform mit Stelle, Determinismus, Deadline bei 301 ms, Stop aus zweitem Thread, Kappungen, Brücke mit und ohne Host | **erledigt** |
| **S2** | `scriptbuiltins.cpp`: Statistik (`sum`, `min`, `max`, `mean`, `sd` als Stichprobe, `sem`, `median`, `len`), Regression (`slope`, `intercept`, `r2`), Einheiten (`ha_to_kjmol`, `kjmol_to_ha`, `ha_to_ev`, `ev_to_ha`, `ha_to_kcal`, `kcal_to_ha`, `bohr_to_ang`, `ang_to_bohr`, `cm_to_kjmol`), Winkel (`radians`, `degrees`), Konstanten (`kB`, `NA`, `h`, `c`, `R`); die Umrechnungen aus den definierenden Konstanten **abgeleitet** statt als Zitat | Jeder Faktor gegen eine Handrechnung gepinnt (Hartree→kJ/mol 2625.4996394798254, eV 27.211386245988, kcal/mol 627.5094740631, Bohr 0.529177210903, cm⁻¹ 0.011962657), jede Umrechnung hebt ihre Umkehrung auf, `sd` als n−1 gegen ein von Hand gerechnetes Beispiel | **erledigt** |
| **S3** | `tools_script.{h,cpp}`: das Werkzeug `calculate` (Kategorie `compute`, Effekt `Read`, Affinität `Any`, immer verfügbar), Schema `source` + `data`, Ergebnisförmung, Anbindung an `ToolDispatcher::isInterrupted()`; Registrierung in `MainWindow::createDockWidgets`; `test_scripttool` | `test_scripttool` grün (24 Prüfungen): Schema akzeptiert, `data` typgeprüft und benannt, Ergebnisförmung, **Katalogkosten 1296 Byte** gegen die Grenze 1500, Interrupt beendet `while (true) {}` nach **21 ms**, kein Host im Modellpfad | **erledigt** |
| **S4** | Absatz im Systemprompt (`MainWindow::applySystemPrompt`), Changelog-Zeile | Absatz und Changelog stehen. **Offen:** Bedienerprüfung am laufenden Endpunkt, ob eine Rechenfrage ohne Stichwort zu einem `calculate`-Aufruf führt | **Code erledigt, Prüfung offen** |
| **S5** | `src/docks/scriptdock.{h,cpp}`: Editor, Run, Stop, Ausgabe, Beispiele, Persistenz; `DockConfig::ScriptDock`; **nicht** `USE_LLM`-gated; Lauf auf Arbeitsthread; hier bekommt der Interpreter die Brücke, mit Sammelbestätigung und Freigabepolitik je Aufruf | Bedienerprüfung: Beispiel starten, `while (true) {}` mit Stop abbrechen (Fenster friert nie ein), Text nach Neustart noch da | offen |
| **S6** | Makro-Aufzeichnung: Signal `callRecorded` am `ToolDispatcher` plus Herkunft, Aufzeichnung als JS-Zeilen mit `tool(...)` in den Dock-Editor | Bedienerprüfung: aufzeichnen, abspielen, gleiche Wirkung; Aufrufe des Assistenten landen standardmäßig nicht darin | offen |
| **S7** | Dasselbe dem Modell geben: `run_script` (`Compute`, asynchron über `script_status`), Werkzeugname als String-Literal erzwungen, je Aufruf durch die Freigabepolitik | Umgehungstest: Freigabe verweigert → Zähler unverändert; Freigabe erteilt → Zähler steigt genau um die Zahl der Aufrufstellen | offen |

## 6. Grenzen, die benannt bleiben müssen

- **Deadline und Stop nennen keine Zeile.** Gemessen: der Interrupt liefert nur
  `Error: Interrupted`. Der Fehlertext sagt deshalb, dass der Lauf zu lange dauerte und dass
  die Schleife enden muss, aber nicht, wo sie steht.
- **Ein werfendes Builtin meldet seine eigene Zeile.** Gemessen: `tool(...)` ohne Brücke
  ergab „line 57", also eine Zeile im Builtin-Programm, für ein einzeiliges Skript. Der
  Stack enthält aber auch den Frame des Skripts, und der gewinnt jetzt: dieselbe Meldung
  lautet „line 1: this script is a calculation only". Dasselbe gilt für jede
  Builtin-Beschwerde (`mean()` ohne Argument).
- **Der Quelltext geht in den Verlauf.** Ein 300-Byte-Skript steht in jeder folgenden Runde
  erneut im Request. Das ist der eigentliche Byte-Posten dieser Funktion, weit über dem
  Katalogeintrag, und das Argument dafür, Skripte kurz zu halten.
- **Eine Rechenrunde verbraucht Budget wie jede andere.** Nachgemessen an
  `llmsession.cpp:236-238`: eine Runde wird nur dann nicht gezählt, wenn kein Aufruf
  gearbeitet hat **und** die Runde mindestens 2000 ms gedauert hat. `calculate` ist in
  Millisekunden fertig, also zählt es. Das ist die gewünschte Richtung (Rechnen soll
  nicht gratis sein), bedeutet aber: ein Skript statt vier Aufrufe spart drei Runden,
  und das ist der eigentliche Gewinn des Werkzeugs.
- **Kein Sandkasten für den Bedienerpfad.** Sobald das Dock eine Brücke hat, kann ein Skript
  dort alles aufrufen, was die Freigabepolitik erlaubt. Die Brücke ist die Grenze, und die
  Sammelbestätigung zeigt vorher, welche Werkzeuge im Quelltext stehen.
- **Molare Massen und Elementdaten fehlen.** Sie bräuchten entweder curcuma in der
  Skript-Bibliothek oder eine zweite Massentabelle; beides ist eine eigene Entscheidung.
- **Aufgezeichnet wird nur, was durch den Dispatcher läuft.** Menü-Aktionen ohne Werkzeug
  erscheinen in einem Makro nicht.
- **Die Engine ist eine Blackbox.** Ein Wechsel auf eine eigene Grammatik oder ExprTk bleibt
  hinter derselben Naht (`ScriptInterpreter::run`) ein Commit, wenn sich zeigt, dass Modelle
  damit bessere Skripte schreiben.
