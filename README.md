# CAM Monitor — HDMI → WLAN → iPhone

**Status: v0.1 developer preview; unflashed, not yet hardware-validated.**

Minimal, native H.264/WebRTC full-screen viewer for an existing ESP-KVM firmware on
**Waveshare ESP32-P4-NANO rev 3.x** with **TC358743 HDMI-to-CSI adapter**. Intended
for Nikon D3200 and similar HDMI cameras. Landscape and portrait use `object-fit:
contain` to show the entire image, with black bars where the aspect ratios differ.

## What works in this first iteration (in code)

- The `/cam` route serves one compressed HTML file with no JS runtime, CDNs or fonts.
- WebRTC negotiates directly against ESP-KVM's own API. The iPhone decodes H.264
  natively in a `<video>` element; the webpage does not copy frames via canvas.
- Camera preview uses no input controls or KVM-specific video chrome.
- Overlay hides after a few seconds. Tap to reveal settings.
- Bitrate, FPS cap, and EDID mode use the existing ESP-KVM settings API.
- Reload/reconnect on network errors and on returning from background.
- Existing ESP-KVM admin/login at `/` stays intact as a recovery path.

## What this is NOT yet

This version **does not delete ESP-KVM's KVM, recording or other subsystems**.
It builds upstream firmware with a minimal **additional** `/cam` route. That
is deliberate: first establish actual camera latency and stability, then trim
backend modules individually and measure whether it improves anything.

Not yet compiled against ESP-IDF in this delivery environment (not installed);
not yet flashed or tested on your hardware. Your original ESP-KVM installation
is the safe fallback using https://espkvm.io/flash/.

## Prerequisites

- VS Code, Linux or WSL2 (Debian/Ubuntu recommended).
- `git`, `python3`, compiler tools and Espressif **ESP-IDF v6.1**.
- USB-C data cable and known board revision **3.x** (confirm from `esptool`).
- Your camera HDMI cable and CSI cable correctly connected.

For ESP-IDF v6.1 setup, follow the official instructions:
https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/get-started/

The upstream project also provides `tools/install-idf.sh`.

## Build

```bash
git clone https://github.com/DerHausmeisterKrause/h264-Video_Monitor.git
cd h264-Video_Monitor
. ~/esp/esp-idf/export.sh
bash scripts/build-linux.sh
```

The helper clones ESP-KVM at pinned revision
`1983a0adf2474e96026a7f46357f7812eb5e6960`, checks out its upstream web
submodule, injects `/cam`, and compiles for `nano_p4_rev3`.

```bash
cd .upstream/espkvm
idf.py -B build.cam-nano-rev3 -p /dev/ttyACM0 flash monitor
```

**Windows:** use the actual COM port instead; on WSL2 you may need USB forwarding
via usbipd-win. No firmware binary is supplied or verified yet.

## Open on iPhone

1. Complete first-run setup in original ESP-KVM at `http://192.168.4.1/` (or
   your device's HTTPS address), and log in.
2. Configure the hotspot password; change the admin password.
3. Select **H.264** and **Allow WebRTC** in original video settings.
4. Open `http://192.168.4.1/cam` in Safari. The session cookie applies to `/cam`.
5. Rotate the iPhone; use Safari **Add to Home Screen** for less browser chrome.

The viewer is scoped to the same origin; it never sends the admin password or
stream to a third-party service. Safari/WebRTC support depends on OS version.

## Settings

- **Bitrate:** `h264_kbps` (500–20000, hardware supported).
- **FPS:** `vid_fps_max`; actual frame rate depends on encoder, CSI and source.
- **EDID:** `edid_prof` with `full`, `1080p30`, `720p`; switching requires restart.

The EDID is only a list of available output modes; it does not guarantee that a
Nikon's HDMI timing will change, nor does any codec guarantee a particular lag.

## Engineering roadmap

1. Validate WebRTC on real iPhone via AP; measure end-to-end latency with LED/timecode.
2. Add optional browser live diagnostics (jitter-buffer, frame drops) **off by default**.
3. Separate only the proven working HDMI/CSI, encoder, networking, web and AP
   components into a genuinely smaller ESP-IDF application.
4. Profile encoder vs Wi-Fi vs iOS decoder latency; optimize based on measurements.

## License, provenance and security

Custom overlay and build scripts: MIT (see `LICENSE`). The source repository
https://github.com/espkvm/espkvm is **Apache-2.0** and remains an explicit
external dependency, not silently relicensed. Respect upstream LICENSE and NOTICE.
The project is pinned to ESP-KVM v0.68.0 source snapshot, but no reproducible
binary/sha256 has yet been published or audited here.

Use **WPA2 password** and a nondefault admin password. Keep this device on an
isolated/trusted LAN. No public internet exposure.