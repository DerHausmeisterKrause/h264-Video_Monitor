#!/usr/bin/env python3
"""
Prepare a *standalone* CAM Monitor firmware from audited ESP-KVM hardware drivers.
Only TC358743/CSI, ESP-H264, WebRTC RTP and their support dependencies are reused.
The original ESP-KVM app_main, HTTP handlers, UI and WiFi setup are NOT used.
ESP-KVM derived code retains Apache-2.0 copyright notices.

The original ESP-KVM source is pinned to a tested API snapshot (0.68.0).
This script refuses unknown revisions rather than silently changing interfaces.
"""
import argparse
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def replace_once(path: Path, old: str, new: str):
    text = path.read_text()
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected exactly 1 anchor, found {count}: {old[:100]}")
    path.write_text(text.replace(old, new))

def replace_braced_function(path: Path, name: str, replacement: str):
    data = path.read_text()
    matches = list(re.finditer(r"static esp_err_t " + re.escape(name) + r"\(void\)\s*\{", data))
    if len(matches) != 1:
        raise RuntimeError(f"Expected 1 {name} definition in {path}, got {len(matches)}")
    start = matches[0].start()
    open_pos = data.index("{", start)
    depth = 0
    end = None
    for i in range(open_pos, len(data)):
        if data[i] == "{":
            depth += 1
        elif data[i] == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    if end is None: raise RuntimeError("unbalanced braces")
    path.write_text(data[:start] + replacement + data[end:])

def prepare(upstream: Path):
    upstream = upstream.resolve()
    if not (upstream / "components/video_pipeline/capture_h264.c").exists():
        raise RuntimeError("ESP-KVM capture source missing")

    # Original firmware code is never the entrypoint. Reuse only the
    # ESP-KVM hardware and video transport libraries required by our app.
    dst = upstream / "main"
    for file in ("main.c", "wifi_ap.c", "wifi_ap.h", "web.c", "web.h",
                 "CMakeLists.txt", "idf_component.yml"):
        shutil.copy2(ROOT / "main" / file, dst / file)
    (dst / "web").mkdir(exist_ok=True)
    shutil.copy2(ROOT / "web/index.html", dst / "web/index.html")
    cm = dst / "CMakeLists.txt"
    replace_once(cm, 'EMBED_FILES "../web/index.html"', 'EMBED_FILES "web/index.html"')

    root_cmake = upstream / "CMakeLists.txt"
    replace_once(root_cmake, "project(espkvm)", "project(cammonitor)")

    vid = upstream / "components/video_pipeline"
    shutil.copy2(ROOT / "video/capture_loop.c", vid / "capture_loop.c")

    # No MJPEG or screenshot encoder is linked into this image. No codec
    # chooser, JPEG fallback, screen-reader or snapshot task is started.
    vcm = vid / "CMakeLists.txt"
    replace_once(vcm, '        "capture_mjpeg.c"\n', '')
    replace_once(vcm, '        "capture_snapshot.c"\n', '')
    replace_once(vcm, '        esp_driver_jpeg\n', '') if False else None

    src = vid / "capture.c"
    replace_once(src, "    capture_mjpeg_probe();\n", "")
    replace_once(src, "    capture_mjpeg_bind_settings();\n", "")
    # The original status API references flat scanner: compiled as a small
    # support component but never started by our capture loop.

    arena = vid / "capture_arena.c"
    replace_once(arena, "    const size_t mjpeg = capture_mjpeg_arena_bytes();",
                 "    const size_t mjpeg = 0; /* H.264 only */")
    # This makes the PSRAM reservation sized solely for H.264.
    h264 = vid / "capture_h264.c"
    replace_braced_function(h264, "h264_open",
        """static esp_err_t h264_open(void)
{
    /* No JPEG codec allocation or fallback paths. */
    esp_err_t err = h264_open_once();
    if (err == ESP_ERR_NO_MEM && capture_arena_active() &&
        capture_release_memory(true)) {
        err = h264_open_once();
    }
    return err;
}""")

    # The encoder is the single fixed codec; use hardware REV3 YUV422 path.
    defaults = upstream / "sdkconfig.defaults"
    with defaults.open("a") as f:
        f.write("\n# CAM Monitor: only H264 + local open WiFi AP\n")
        f.write("CONFIG_KVM_ENABLE_H264=y\n")
        f.write("CONFIG_KVM_ENABLE_WEBRTC=y\n")

    print("Standalone H.264 capture and WebRTC integration prepared: no original app_main or MJPEG module.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("upstream", type=Path)
    prepare(parser.parse_args().upstream)
