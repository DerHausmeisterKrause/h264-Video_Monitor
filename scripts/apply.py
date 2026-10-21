#!/usr/bin/env python3
"""Add a low-latency /cam WebRTC viewer to pinned ESP-KVM firmware."""
import argparse
import gzip
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]

def apply(root):
    root = Path(root).resolve()
    if not (root / "main/main.c").is_file():
        raise RuntimeError("Not an ESP-KVM checkout: " + str(root))
    base = root / "components/kvm_web"
    cmake = base / "CMakeLists.txt"
    server = base / "http_server.c"
    specs = [
        (cmake, '        "assets/index.html.gz"', '        "assets/cam.html.gz"\n'),
        (server, 'extern const char index_html_gz_start[]',
         'extern const char cam_html_gz_start[] asm("_binary_cam_html_gz_start");\n'
         'extern const char cam_html_gz_end[] asm("_binary_cam_html_gz_end");\n'),
        (server, 'static esp_err_t favicon_get(httpd_req_t *req)',
         'static esp_err_t cam_get(httpd_req_t *req)\n'
         '{\n'
         '    httpd_resp_set_type(req, "text/html");\n'
         '    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");\n'
         '    httpd_resp_set_hdr(req, "Cache-Control", "no-store");\n'
         '    return httpd_resp_send(req, cam_html_gz_start,\n'
         '        (size_t)(cam_html_gz_end - cam_html_gz_start));\n'
         '}\n\n'),
        (server, '    httpd_uri_t u_favicon =',
         '    httpd_uri_t u_cam = {.uri = "/cam", .method = HTTP_GET, .handler = cam_get};\n'
         '    register_route(h, &u_cam);\n'),
    ]
    for file, marker, insertion in specs:
        content = file.read_text()
        if insertion not in content and content.count(marker) != 1:
            raise RuntimeError(f"Upstream changed: {file}, marker {marker!r}")
    asset = base / "assets/cam.html.gz"
    asset.write_bytes(gzip.compress((HERE / "src/cam.html").read_bytes(), compresslevel=9, mtime=0))
    for file, marker, insertion in specs:
        content = file.read_text()
        if insertion not in content:
            file.write_text(content.replace(marker, insertion + marker, 1))
    print("CAM Monitor embedded. Existing ESP-KVM at / ; video view at /cam")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("upstream")
    args = parser.parse_args()
    apply(args.upstream)
