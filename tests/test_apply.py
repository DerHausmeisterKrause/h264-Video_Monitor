import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from apply import apply


class ApplyTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "main").mkdir()
        (self.root / "main/main.c").write_text("int main(void){return 0;}")
        self.base = self.root / "components/kvm_web"
        (self.base / "assets").mkdir(parents=True)
        (self.base / "CMakeLists.txt").write_text('    EMBED_FILES\n        "assets/index.html.gz"\n')
        self.server = self.base / "http_server.c"
        self.server.write_text(
            'extern const char index_html_gz_start[] asm("_binary_index_html_gz_start");\n'
            'static esp_err_t favicon_get(httpd_req_t *req) { return ESP_OK; }\n'
            '    httpd_uri_t u_favicon = {};\n'
        )

    def tearDown(self):
        self.tmp.cleanup()

    def test_patch_is_idempotent(self):
        apply(self.root)
        first = self.server.read_text()
        apply(self.root)
        self.assertEqual(first, self.server.read_text())
        self.assertEqual(first.count('register_route(h, &u_cam);'), 1)
        self.assertTrue((self.base / "assets/cam.html.gz").is_file())

    def test_unknown_layout_refuses_without_modifications(self):
        self.server.write_text("unrelated")
        before = self.server.read_text()
        with self.assertRaises(RuntimeError):
            apply(self.root)
        self.assertEqual(self.server.read_text(), before)


if __name__ == "__main__":
    unittest.main()
