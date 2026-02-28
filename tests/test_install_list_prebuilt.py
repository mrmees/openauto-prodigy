#!/usr/bin/env python3
"""Installer prebuilt-release listing smoke test."""

from __future__ import annotations

import json
import os
import pathlib
import subprocess
import tempfile


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
INSTALL_SCRIPT = REPO_ROOT / "install.sh"


def main() -> int:
    releases = [
        {
            "tag_name": "v0.3.0",
            "name": "v0.3.0",
            "draft": False,
            "published_at": "2026-02-27T12:00:00Z",
            "assets": [
                {
                    "name": "openauto-prodigy-prebuilt-v0.3.0-pi4-aarch64.tar.gz",
                    "browser_download_url": "https://example.invalid/v0.3.0-pi4-aarch64.tar.gz",
                }
            ],
        },
        {
            "tag_name": "v0.2.9",
            "name": "v0.2.9",
            "draft": False,
            "published_at": "2026-02-20T12:00:00Z",
            "assets": [
                {
                    "name": "openauto-prodigy-prebuilt-v0.2.9-pi4-aarch64.tar.gz",
                    "browser_download_url": "https://example.invalid/v0.2.9-pi4-aarch64.tar.gz",
                }
            ],
        },
    ]

    with tempfile.TemporaryDirectory(prefix="oap-install-list-") as tmp:
        tmp_dir = pathlib.Path(tmp)
        json_path = tmp_dir / "releases.json"
        json_path.write_text(json.dumps(releases), encoding="utf-8")

        env = dict(**{"OAP_GITHUB_API_URL": f"file://{json_path}"})
        # Keep parent environment for shell/path resolution.
        full_env = dict(**os.environ, **env)
        # Ensure list mode works in minimal/non-interactive environments.
        full_env.pop("TERM", None)

        proc = subprocess.run(
            ["bash", str(INSTALL_SCRIPT), "--list-prebuilt"],
            cwd=REPO_ROOT,
            env=full_env,
            check=False,
            capture_output=True,
            text=True,
        )

    if proc.returncode != 0:
        raise AssertionError(
            f"install.sh --list-prebuilt failed (code={proc.returncode})\n"
            f"stdout:\n{proc.stdout}\n\nstderr:\n{proc.stderr}"
        )

    out = proc.stdout
    expected = [
        "1) v0.3.0",
        "openauto-prodigy-prebuilt-v0.3.0-pi4-aarch64.tar.gz",
        "2) v0.2.9",
        "openauto-prodigy-prebuilt-v0.2.9-pi4-aarch64.tar.gz",
    ]
    missing = [token for token in expected if token not in out]
    if missing:
        raise AssertionError(f"missing expected output tokens: {missing}\nstdout:\n{out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
