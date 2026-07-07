import io
import json
import os
import unittest
from unittest import mock

import server


def multipart(manifest, wallpaper=None):
    data = {"manifest": json.dumps(manifest)}
    if wallpaper is not None:
        data["wallpaper"] = (io.BytesIO(wallpaper), "wp.jpg", "image/jpeg")
    return data


VALID = {"name": "Sunset Vibes", "light": {"primary": "#111111"}, "dark": {"primary": "#222222"}}


class InstallThemeTest(unittest.TestCase):
    def setUp(self):
        server.app.config["TESTING"] = True
        self.client = server.app.test_client()

    @mock.patch("server.ipc_request")
    def test_happy_path(self, ipc):
        ipc.return_value = {"ok": True, "slug": "sunset-vibes"}
        r = self.client.post("/api/theme/install", data=multipart(VALID),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200)
        self.assertEqual(r.get_json(), {"installed": True, "slug": "sunset-vibes", "applied": True})
        self.assertEqual(ipc.call_args[0][0], "install_theme")

    @mock.patch("server.ipc_request")
    def test_missing_name_is_400(self, ipc):
        r = self.client.post("/api/theme/install",
                             data=multipart({"light": {"primary": "#111"}, "dark": {"primary": "#222"}}),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 400)
        ipc.assert_not_called()

    @mock.patch("server.ipc_request")
    def test_app_down_is_503(self, ipc):
        ipc.return_value = {"error": "Qt app not running (IPC socket not found)"}
        r = self.client.post("/api/theme/install", data=multipart(VALID),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 503)

    @mock.patch("server.ipc_request")
    def test_import_failure_is_500(self, ipc):
        ipc.return_value = {"ok": False, "error": "theme import failed"}
        r = self.client.post("/api/theme/install", data=multipart(VALID),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 500)

    @mock.patch("server.ipc_request")
    def test_temp_file_created_and_cleaned(self, ipc):
        captured = {}

        def fake(cmd, data=None):
            captured["path"] = data.get("wallpaper_path")
            captured["existed_during_call"] = os.path.exists(data.get("wallpaper_path", ""))
            return {"ok": True, "slug": "x"}

        ipc.side_effect = fake
        r = self.client.post("/api/theme/install",
                             data=multipart(VALID, wallpaper=b"\xff\xd8\xff" + b"\x00" * 16),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 200)
        self.assertTrue(captured["existed_during_call"])          # present while Qt reads it
        self.assertFalse(os.path.exists(captured["path"]))         # unlinked in finally

    def test_oversize_is_413(self):
        big = b"\xff\xd8\xff" + b"\x00" * (7 * 1024 * 1024)        # > MAX_CONTENT_LENGTH
        r = self.client.post("/api/theme/install", data=multipart(VALID, wallpaper=big),
                             content_type="multipart/form-data")
        self.assertEqual(r.status_code, 413)


if __name__ == "__main__":
    unittest.main()
