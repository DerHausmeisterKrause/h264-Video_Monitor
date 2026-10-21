# CAM Monitor – standalone H.264 branch

**Status: Grundgerüst / noch KEINE funktionierende Video-Firmware. NICHT FLASHEN, wenn Livebild erwartet wird.**

Eigene ESP-IDF-Anwendung für **Waveshare ESP32-P4-NANO Rev. 3.x** mit separatem **ESP32-C6 WLAN-Co-Prozessor**.

### Festgelegte Architektur
- **Nur H.264** (Hardwareencoder). Keine MJPEG-Implementierung, kein Codec-Umschalter, kein JPEG-Fallback.
- HDMI über TC358743 → MIPI CSI-2 → H.264 → WebRTC → native iPhone-Safari-Wiedergabe.
- AP automatisch: `CAM-MONITOR`, ohne Passwort oder Einrichtungsassistent, unter `http://192.168.4.1/`.
- Nur ein Videoclient. Keine Benutzerverwaltung, KVM-Fernbedienung, OTA, microSD/Recording, VPN, MQTT oder Cloud.
- Offenes WLAN: Jeder in Funkreichweite kann sich verbinden. Ausschließlich in kontrollierter Umgebung verwenden.

### Stand dieses Commits
Enthalten: eigenständiger ESP-IDF-Einstiegspunkt, minimales C6-SDIO-WLAN-AP, HTTP-Server, schwarze 16:9/Seitenverhältnis-erhaltende Videoseite, Status-Endpunkt. Die frühere ESP-KVM-Weboberfläche wird **nicht** eingebunden.

**Noch nicht enthalten:** TC358743-Bildaufnahme, CSI-Frame-DMA, H.264-Hardwareencoding und WebRTC-Signalisierung/SRTP. Die Seite zeigt den fehlenden Zustand ausdrücklich an und behauptet kein funktionierendes Livebild. Der Branch veröffentlicht deshalb keine flashbare Release-Firmware.

### Build
```bash
cd standalone
. ~/esp/esp-idf/export.sh
idf.py -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
```
Die GitHub-Action `Standalone Compile` kompiliert diesen Zweig testweise und veröffentlicht nur ein Build-Artefakt; kein Release.

### Nächste Arbeitsschritte
1. Build-Fixes für ESP-IDF 6.1 / esp-hosted.
2. TC358743-Eingang / CSI-Frames aus der Apache-2.0-Quelle ESP-KVM gezielt übernehmen (mit Original-Notices), keine anderen KVM-Module.
3. Den Hardware-H.264-Encoder direkt an den CSI-Frame-Pool koppeln.
4. Minimalen WebRTC-H.264-Videoweg ergänzen; keine WebCodecs-Abhängigkeit auf HTTP.
5. Nach realem iPhone- und Nikon-Test auf den Hauptbranch übernehmen und erst dann Firmware-Releases bereitstellen.
