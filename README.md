# CAM Monitor – HDMI → WLAN → iPhone

> **Aktuelles Entwicklungsziel (21.10.2026): echte Standalone-Firmware mit ausschließlich Hardware-H.264.** Die bisherige v0.1.0 ist nur ein ESP-KVM-Weboberflächen-Prototyp und entspricht noch nicht dem gewünschten schlanken Aufbau. Der neue Stand entsteht auf dem Branch [standalone-h264](https://github.com/DerHausmeisterKrause/h264-Video_Monitor/tree/standalone-h264/standalone) und wird erst nach erfolgreicher Integration von HDMI/CSI/H.264/WebRTC als Release bereitgestellt. **Nicht die alte v0.1.0 für die neue Standalone-Firmware halten.**

**Status: v0.1 Entwickler-Prototyp – noch nicht auf der Hardware kompiliert oder getestet.**

Minimalistische H.264/WebRTC-Livebildoberfläche für **Waveshare ESP32-P4-NANO (Chiprevision 3.x)** mit **TC358743 HDMI-zu-CSI-Adapter** und beispielsweise einer Nikon D3200.

## Funktionen der ersten Version

- Livebild im iPhone-Safari ohne KVM-Bedienleisten; Video mit `object-fit: contain`, also vollständig mit schwarzen Balken statt Beschnitt.
- H.264 über **WebRTC**, direkt in einem nativen HTML-`video`-Element dekodiert; keine Canvas-Kopien.
- Schlichtes Einstellungsmenü für Bitrate, FPS-Limit und HDMI-EDID-Auflösung.
- Reconnect bei Verbindungsverlust und Rückkehr aus dem Hintergrund.
- ESP-KVM-Verwaltung bleibt unter `/` erreichbar, CAM Monitor unter `/cam`.

**Wichtig:** Die Firmware besteht in v0.1 noch aus ESP-KVM mit einer zusätzlichen minimalen Weboberfläche. Die internen KVM-, Recording- und sonstigen Module sind **nicht** entfernt. Erst nach einem funktionierenden Hardwaretest folgt eine echte Firmware-Verschlankung.

## Entwicklungsumgebung

Empfohlen: Debian/Ubuntu oder Windows 11 mit WSL2, VS Code, git, Python 3 und **ESP-IDF 6.1**.

Die ESP-IDF-Installation beschreibt Espressif:
https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/get-started/

## Bauen (Linux / WSL2)

```bash
git clone https://github.com/DerHausmeisterKrause/h264-Video_Monitor.git
cd h264-Video_Monitor
. ~/esp/esp-idf/export.sh
bash scripts/build-linux.sh
```

Das Skript klont `espkvm/espkvm` einschließlich Submodule, checkt die festgelegte Revision
`1983a0adf2474e96026a7f46357f7812eb5e6960` (ESP-KVM 0.68.0) aus, fügt die separate `/cam`-Route hinzu und ruft ESP-IDF für `nano_p4_rev3` auf.

## Firmware über GitHub Actions bauen und veröffentlichen

**Manueller Build:** [Actions → Build & Release Firmware](https://github.com/DerHausmeisterKrause/h264-Video_Monitor/actions/workflows/release.yml) → **Run workflow** → Branch `main` → **Run workflow**.

Der Workflow läuft auf GitHub, installiert ESP-IDF 6.1 im offiziellen Espressif-Build-Container, kompiliert für das Waveshare ESP32-P4-NANO mit **Chiprevision 3.x**, erzeugt zwei Dateien und veröffentlicht nach erfolgreichem Build ein GitHub-Release:

- `cammonitor-v0.1.0-p4-nano-rev3-merged.bin`: vollständiges USB-Flash-Image ab Adresse `0x0`; kann bestehende Einstellungen überschreiben.
- `cammonitor-v0.1.0-p4-nano-rev3-ota.bin`: App-Image für eine *kompatible* vorhandene ESP-KVM-Partitionierung, **nicht** für Adresse `0x0`.
- `SHA256SUMS.txt`: Prüfsummen.

Die **aktuelle Projektversion** steht in der Datei [`VERSION`](VERSION). Zum Beispiel nach Änderung von `0.1.0` auf `0.1.1` erzeugt der nächste manuelle erfolgreiche Build das Release `v0.1.1` mit entsprechend benannten Firmwaredateien. Bereits veröffentlichte Versionen bleiben an ihrem ursprünglichen Git-Commit; ohne Versionswechsel kann keine neue Version aus einem anderen Commit unter demselben Tag erstellt werden.

[GitHub Releases öffnen](https://github.com/DerHausmeisterKrause/h264-Video_Monitor/releases)

**Hinweis:** Die Action ist eingerichtet, ein erfolgreicher ESP-IDF-Build und ein Flash-/Kameratest auf echter Hardware stehen noch aus. Nur ein erfolgreicher Build erstellt eine Veröffentlichung.

## Flashen

**Erst verwenden, wenn der Build erfolgreich durchgelaufen ist** und die Hardware-Revision mit `esptool` tatsächlich als 3.x bestätigt ist.

```bash
cd .upstream/espkvm
idf.py -B build.cam-nano-rev3 -p /dev/ttyACM0 flash monitor
```

Unter Windows gegebenenfalls `COM4` statt `/dev/ttyACM0` und unter WSL2 USB-Weiterleitung per `usbipd-win` nutzen. Vorhandene ESP-KVM-Konfiguration ggf. sichern; ein Flash kann sie beeinträchtigen.

## iPhone benutzen

1. Unter `http://192.168.4.1/` zunächst das ESP-KVM-Passwort und ein WPA2-geschütztes WLAN konfigurieren.
2. `H.264` sowie `Allow WebRTC` im bisherigen ESP-KVM einstellen.
3. Unter `http://192.168.4.1/cam` die schlichte Kameraansicht öffnen.
4. Im Querformat anschauen; über Safari zum Home-Bildschirm hinzufügen für weniger Browser-Bedienelemente.

Beim Wechsel der EDID kann ein Neustart erforderlich sein. Die Nikon kann ihre HDMI-Ausgabe unabhängig davon selbst bestimmen.

## Qualitätsprüfung

```bash
python3 -m unittest discover -s tests -v
python3 -m py_compile scripts/apply.py
bash -n scripts/build-linux.sh
```

Die GitHub-Action prüft zusätzlich die JavaScript-Syntax. **Diese Quellcodeprüfungen ersetzen keinen vollständigen ESP-IDF-Build und keinen iPhone-Hardwaretest.**

## Nächste Schritte

1. Firmware erfolgreich bauen, auf dem Waveshare testen, Kamera über HDMI verbinden.
2. Latenz und Stabilität bei 720p/1080p mit mehreren Bitraten messen.
3. Problematische WebRTC-Verbindungsfälle auf Safari beheben.
4. Unnötige Firmwaremodule erst anschließend gezielt entfernen.
5. Optional später einen eigenen Browser-Flasher bereitstellen.

## Lizenzen und Sicherheit

Der Code dieses Overlays steht unter MIT (siehe `LICENSE`). Die beim Build extern bezogene ESP-KVM-Firmware steht unter Apache-2.0, inklusive ihrer eigenen Urheberrechts- und NOTICE-Bestimmungen. Der Projektcode ist bislang **nicht** unabhängig auf Schwachstellen oder für den Internetbetrieb geprüft. Den Hotspot mit WPA2 absichern und ein eigenes Admin-Passwort setzen.

ESP-KVM: https://github.com/espkvm/espkvm
