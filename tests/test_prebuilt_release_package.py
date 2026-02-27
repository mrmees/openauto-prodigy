#!/usr/bin/env python3
"""Verify prebuilt release packaging layout and required payload files."""

from __future__ import annotations

import pathlib
import stat
import subprocess
import tarfile
import tempfile


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
PACKAGE_SCRIPT = REPO_ROOT / "tools" / "package-prebuilt-release.sh"


def make_fake_binary(path: pathlib.Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x7fELFfake")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="oap-prebuilt-test-") as tmp:
        tmp_dir = pathlib.Path(tmp)
        build_dir = tmp_dir / "build-pi"
        output_dir = tmp_dir / "dist"
        fake_binary = build_dir / "src" / "openauto-prodigy"
        make_fake_binary(fake_binary)

        subprocess.check_call(
            [
                str(PACKAGE_SCRIPT),
                "--build-dir",
                str(build_dir),
                "--output-dir",
                str(output_dir),
                "--version-tag",
                "test",
            ],
            cwd=REPO_ROOT,
        )

        archive = output_dir / "openauto-prodigy-prebuilt-test.tar.gz"
        if not archive.is_file():
            raise AssertionError(f"missing archive: {archive}")

        required = {
            "openauto-prodigy-prebuilt-test/install-prebuilt.sh",
            "openauto-prodigy-prebuilt-test/payload/build/src/openauto-prodigy",
            "openauto-prodigy-prebuilt-test/payload/config/themes/default/theme.yaml",
            "openauto-prodigy-prebuilt-test/payload/system-service/openauto_system.py",
            "openauto-prodigy-prebuilt-test/payload/system-service/requirements.txt",
            "openauto-prodigy-prebuilt-test/payload/web-config/server.py",
            "openauto-prodigy-prebuilt-test/payload/web-config/requirements.txt",
            "openauto-prodigy-prebuilt-test/payload/restart.sh",
        }
        with tarfile.open(archive, mode="r:gz") as tar:
            names = set(tar.getnames())
            missing = sorted(required - names)
            if missing:
                raise AssertionError(f"archive missing entries: {missing}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
