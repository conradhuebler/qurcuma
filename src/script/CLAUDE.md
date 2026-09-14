# Skript-Interpreter (JavaScript über QJSEngine)

## Zweck und Aufbau
- Ein kurzes JavaScript, das qurcuma ausführt, damit Zahlen hier gerechnet werden statt im Kopf eines Modells: Differenzen, Verhältnisse, Einheiten, Mittelwert, Streuung, Steigung.
- Eigene statische Bibliothek `qurcuma_script` (`qurcuma_core` + `Qt6::Qml`), **headless** und ohne Widgets; **nicht** in `qurcuma_core`, weil `libQt6Qml` Network/ICU/libproxy/systemd mitzieht.
- **Nicht** hinter `USE_LLM`: Rechnen ist keine Endpunkt-Frage.
- `scriptinterpreter.cpp` = Mechanik (frische Engine je Lauf, Deadline, Stop, Kappungen, Fehler mit Stelle); `scriptbuiltins.cpp` = Inhalt (Tabelle als JS-Quelltext); `scriptbridge.h` = der eine Übergang nach C++.
- Ergebnis-Kontrakt: der **Wert des letzten Ausdrucks** ist das Ergebnis; ein Objekt liefert benannte Mitglieder. `print(...)` füllt die Ausgabeliste.

## Entscheidungen, die nicht neu verhandelt werden müssen
- ✅ **Qt 6.11 hat kein `QJSEngine::newFunction`** (weder Header noch Doku). Builtins stehen deshalb in JavaScript, `print` wird als Array nach dem Lauf zurückgelesen, die Werkzeug-Brücke ist ein QObject mit `Q_INVOKABLE`.
- ✅ **Kein Host im Modellpfad** — der Interpreter kann Werkzeuge aufrufen, wenn der Aufrufer eine `ScriptHost`-Brücke übergibt. Das Werkzeug des Assistenten wird ohne sie gebaut; die Trennung ist strukturell.
- ✅ **Deadline und Stop nennen keine Zeile** — gemessen: der Interrupt liefert nur `Error: Interrupted`, ohne `lineNumber`, Stack oder Frames. Bei einem Skriptfehler gibt es beides.
- ✅ **`Date` und `Math.random` sind ersetzt** (Nachvollziehbarkeit), Trigonometrie bleibt in Radiant (`radians`/`degrees` als Helfer).
- ✅ `print` und `script::formatNumber` formatieren mit derselben Regel (zehn signifikante Stellen, ohne Nullen); ein Test hält beide fest.
- Dokumentation: `docs/WP-skript-interpreter.md` (Entscheidungen, Arbeitspakete, Grenzen).

## Regeln für neue Builtins
- Jeder Faktor mit Quelle im Kommentar **und** einer Handrechnung im Test.
- Listenfunktionen nehmen Liste und Einzelwerte (`mean([1,2,3])` und `mean(1,2,3)`).
- Was ein Modell falsch erinnern würde (`log` als ln oder log10, `sd` als Stichprobe), gehört in die Beschreibung bzw. in eine klare Meldung.
