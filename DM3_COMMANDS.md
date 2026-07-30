# DM3 RCP-Kommandos

Übersicht der Yamaha DM3 RCP-Befehle (Remote Control Protocol), die von diesem Projekt aktuell verwendet werden. Dient der Nachvollziehbarkeit, falls neue Funktionen ergänzt werden oder das Protokollverhalten mit einem echten DM3 abgeglichen werden muss.

## Protokoll-Basics

- Reines TCP-Textprotokoll, Port `49280`
- Ein Befehl pro Zeile, abgeschlossen mit `\n`
- Kanalindizes sind **0-indexiert** (im Unterschied zu OSC, das 1-indexiert ist) – Slot-Nummer 1 in der UI entspricht Index 0 im Kommando
- Format: `get MIXER:Current/<Kanaltyp>/<Parameter> <Index> 0` bzw. `set MIXER:Current/<Kanaltyp>/<Parameter> <Index> 0 <Wert>`
- Kanaltyp-Kürzel: `St` (ST Master, immer Index 0), `InCh` (Input, 0–15), `Mix` (Mix, 0–5), `Mtrx` (Matrix, 0–1)

## Verwendete Kommandos

| Zweck | Kommando | Quelle |
|---|---|---|
| Fader-Level abfragen | `get MIXER:Current/<Typ>/Fader/Level <idx> 0` | `Network.ino::pollDM3()` |
| Fader-Level setzen | `set MIXER:Current/<Typ>/Fader/Level <idx> 0 <Wert>` | `Input.ino::handleEncoder()` (MENU_MASTER) |
| Mute-Status abfragen | `get MIXER:Current/<Typ>/Fader/On <idx> 0` | `Network.ino::pollDM3()` |
| Mute-Status setzen | `set MIXER:Current/<Typ>/Fader/On <idx> 0 <0\|1>` | `Input.ino::handleButton()` (kurzer Druck, MENU_MASTER) |
| Kanalname abfragen | `get MIXER:Current/<Typ>/Label/Name <idx> 0` | `Network.ino::requestChannelNames()` |

`<Wert>` bei `Fader/Level` ist in Hundertstel-dB (z.B. `0` = 0,0 dB, `-600` = -6,0 dB), Bereich in der Firmware auf `-13800`…`1000` begrenzt (-138,0 dB…+10,0 dB). `Fader/On` ist invertiert zu "Mute": `0` = gemutet, `1` = aktiv.

`Label/Name` liefert als Antwort einen gequoteten String, z.B. `OK get MIXER:Current/InCh/Label/Name 3 0 "Vocal 1"` – Parsing in `Network.ino::extractLabelName()` / `parseChannelName()`.

## Bekannt nicht verfügbar

- **PEQ/EQ**: Laut offizieller Yamaha DM3 OSC Specifications V1.0.0 ist die maximale Band-Anzahl für PEQ auf InCh/Mix/Matrix jeweils `0` – EQ ist über kein Remote-Protokoll (weder OSC noch RCP) steuerbar. Kein Firmware-Bug, sondern eine harte Protokoll-Grenze des DM3.

## Ergänzen

Neue Kommandos hier eintragen, sobald sie in der Firmware verwendet werden (Datei, Zweck, Wertebereich/-format).
