# WP: Periodische Schrittpausen im GPU-GFN-FF-MD (Server)

Status: Stand 2026-10-13. Befund gemessen (siehe unten), Ursache **nicht** geklärt. Zu bearbeiten auf dem Rechner mit der GPU (Server `blaubaer`/`qlocal`, CUDA).

## Ziel

Klären, warum der MD-Lauf auf dem Server in regelmäßigen Abständen einen einzelnen Schritt von mehreren hundert Millisekunden braucht, und entscheiden, ob das in curcuma behoben, in qurcuma abgefedert oder hingenommen wird. Im GUI zeigt sich das als Ruckeln, obwohl Netz und Viewer es nicht verursachen.

## Befund (aus `~/.local/share/qurcuma/remote.log` des Clients)

Die `perf:`-Zeile des Clients listet pro Sekunde die ersten 12 Schritte, die mehr als das Dreifache des gleitenden Mittels dauerten (`slow steps ... [step:ms ...]`). Die Zählungen sind deshalb Untergrenzen.

| System | Atome | Topologie | Lauf | Befund |
|---|---|---|---|---|
| urea400-water1000 (`test_cases/structures/bulk/urea400-water1000.xyz`, aus der Atomzahl erschlossen, die Datei selbst wurde im GUI geladen) | 6200 | constant | 72 Messsekunden, 17:13 | 10,0 Schritte/s (Median); 68 gelistete Ausreißer bei den Schritten 10, 20, 30, 40, ... (Abstand Median 10, Minimum 1, Maximum 10); 67 von 68 dauern über 200 ms, Spanne etwa 720 bis 840 ms |
| polymer | 7320 | constant | 40 und 66 Messsekunden, 17:04/17:05 | 82 bzw. 56 Ausreißer von 400 bis 500 ms, nicht streng periodisch (in einer Stichprobe der Schritte 85, 170, 261, 336 etwa alle 80 bis 90, dazwischen kleinere um 160 ms) |
| polymer | 1410 | auto | 3 Läufe, 16:27 (2) und 17:03 | Ausreißer bei den Schritten 10, 20, 30, ... mit 160 bis 207 ms (7, 24 und 17 Ausreißer) |
| polymer | 1410 | constant | 11 und 14 Messsekunden (Limit 130) | 9 bzw. 28 Ausreißer; im ersten Lauf bei den Schritten 13, 14, 125, 314, 519 mit 28 bis 40 ms, also ohne Rhythmus; Abstand der erzeugten Frames Standardabweichung 3,8 ms (nur dieser Lauf), 139 Schritte/s |

Aus dem urea-water-Lauf folgt (abgeleitet, nicht einzeln gemessen): bei 112,8 ms mittlerem Frame-Abstand entfallen auf 10 Schritte etwa 1128 ms, davon etwa 740 ms auf den einen langen Schritt und etwa 43 ms auf jeden der neun anderen. Der lange Schritt macht also rund zwei Drittel der Zeit aus.

Ausgeschlossen durch dieselben Läufe: Leitung (kein Rückstau, keine verworfenen Frames) und Client (Viewer-Update 2 bis 15 ms).

## Hypothesen (ungeprüft)

- **H1: HB/XB-Listen-Neuaufbau alle 10 Energieauswertungen.** `hb_update_force_every`, Standard 10 (`external/curcuma/src/core/energy_calculators/ff_methods/gfnff.h:358`). Die dortige Beschreibung rechnet mit etwa 100 ms je Neuaufbau bei 7320 Atomen (Polymer, 150k Tripel). urea-water ist wasserstoffbrückenreich (die gleiche Datei nennt 1 Mio. Kandidaten bei einem 14640-Atome-Wassersystem), der Neuaufbau könnte dort um ein Vielfaches teurer sein. Der Rhythmus 10 passt zum urea-water-Befund, aber nicht zu den 85er Abständen beim Polymer.
- **H2: EEQ-Neufaktorisierung.** `eeq_refactor_force_every` (`gfnff.h:339`, Standard 0). Für N = 6200 wäre eine dichte Faktorisierung in dieser Größenordnung plausibel. Muss gemessen werden.
- **H3: Topologiemodus `auto`** (Zwei-Stufen-Cache) erklärt die Pausen bei 1410 Atomen, nicht die bei `constant`.
- **H4: etwas in `SimpleMD` selbst** (Ausgabe, Schwerpunktkorrektur, Thermostat) im Zehnerrhythmus.

## Arbeitspakete

1. **Ohne qurcuma reproduzieren.** urea400-water1000 mit `curcuma -md`, `-method gfnff -gpu cuda`, Schrittzeiten einzeln erfassen. Der GPU-Pfad hat `setForcePhaseTiming` (`gfnff_gpu_method_impl.h`), das die Phasen einer Kraftberechnung auflöst. Vor der Benutzung die genaue Optionsschreibweise mit `curcuma -md -help` und `curcuma -help gfnff` prüfen. Ergebnis: Schrittzeit pro Schritt für mindestens 100 Schritte, Ausreißer je Schritt mit Phase.
2. **H1 prüfen.** Wiederholen mit `hb_update_force_every` = 0, 10, 100. Verschwindet oder verschiebt sich die Pause? Dazu die Energie nach 200 Schritten festhalten, damit der Preis (veraltete Listen) sichtbar ist. Die Beschreibung des Parameters warnt, dass die RMSD-Auslösung bei größeren Systemen praktisch nie feuert.
3. **H2 und H3 prüfen:** `eeq_refactor_force_every` setzen und `topology_mode` zwischen `auto` und `constant` wechseln, jeweils dieselbe Tabelle.
4. **Ergebnis einordnen und entscheiden.** Je nach Befund: (a) in curcuma den Neuaufbau billiger oder asynchron machen (gehört in den Zweig `revgfnff`, nicht in diesen WP), (b) in qurcuma einen größenabhängigen Wert vorschlagen oder im Dock auf den Parameter hinweisen, (c) nichts tun und in der Doku vermerken.
5. **Messtabelle in dieses Dokument** (System, Atome, Parameter, Schritte/s, längster Schritt in ms, Anzahl Ausreißer, Länge der Messung), keine Logdateien in den Chat oder ins Repository.

## Vorgaben für den Bearbeiter

- Kein `git commit` und kein Push in `external/curcuma` ohne Rückfrage; Experimente auf einem eigenen lokalen Branch von `master`.
- Keine Änderung der Rechenergebnisse als Nebenwirkung: Parameter nur per Konfiguration setzen, nicht im Code.
- Zahlen nur mit Stichprobengröße angeben. Eine Hypothese gilt erst als bestätigt, wenn die Änderung des Parameters die Pause messbar verschiebt oder beseitigt.

## Schon heute im GUI ausprobierbar

Falls der Parameter im Reiter "All parameters" des Simulations-Docks steht (nicht geprüft), lässt sich H1 ohne den anderen Rechner testen: `hb_update_force_every` auf 0 oder 100 setzen und in `remote.log` die Zeile `slow steps` vergleichen.

## Messwerkzeuge in qurcuma (stehen)

- Client-Log `perf:`: Rate, Abstände der Frames (Mittel, Standardabweichung, Maximum) auf dem Server und beim Empfang, Ausreißerliste `step:ms`, Rückstau, RTT; `gui viewer:` für den GUI-Thread. Beschreibung in `src/remote/CLAUDE.md`.
