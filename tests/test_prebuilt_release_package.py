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


FAKE_DEB_NAME = "libspa-0.2-bluetooth_1.4.2-1+rpt3+prodigy1_arm64.deb"


def run_packager(build_dir, output_dir, *extra) -> subprocess.CompletedProcess:
    return subprocess.run(
        [
            str(PACKAGE_SCRIPT),
            "--build-dir",
            str(build_dir),
            "--output-dir",
            str(output_dir),
            "--version-tag",
            "test",
            *extra,
        ],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
    )


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="oap-prebuilt-test-") as tmp:
        tmp_dir = pathlib.Path(tmp)
        build_dir = tmp_dir / "build-pi"
        fake_binary = build_dir / "src" / "openauto-prodigy"
        make_fake_binary(fake_binary)

        fake_deb = tmp_dir / FAKE_DEB_NAME
        fake_deb.write_bytes(b"!<arch>fake-deb")

        # Case 1: releases must carry the HFP mSBC codec fix — a supplied
        # deb lands in the payload alongside every other required entry.
        output_dir = tmp_dir / "dist"
        result = run_packager(build_dir, output_dir, "--msbc-deb", str(fake_deb))
        if result.returncode != 0:
            raise AssertionError(
                f"packaging with --msbc-deb failed:\n{result.stdout}\n{result.stderr}"
            )

        package_root = "openauto-prodigy-prebuilt-test-pi4-aarch64"
        archive = output_dir / f"{package_root}.tar.gz"
        if not archive.is_file():
            raise AssertionError(f"missing archive: {archive}")

        required = {
            f"{package_root}/install-prebuilt.sh",
            f"{package_root}/RELEASE.json",
            f"{package_root}/payload/build/src/openauto-prodigy",
            f"{package_root}/payload/config/themes/default/theme.yaml",
            f"{package_root}/payload/system-service/openauto_system.py",
            f"{package_root}/payload/system-service/requirements.txt",
            f"{package_root}/payload/web-config/server.py",
            f"{package_root}/payload/web-config/requirements.txt",
            f"{package_root}/payload/restart.sh",
            f"{package_root}/payload/pipewire-msbc/{FAKE_DEB_NAME}",
        }
        with tarfile.open(archive, mode="r:gz") as tar:
            names = set(tar.getnames())
            missing = sorted(required - names)
            if missing:
                raise AssertionError(f"archive missing entries: {missing}")

        # Case 2: without a deb (and without the explicit override) the
        # packager must REFUSE — a release with silent-mic HFP is not a
        # release. (The repo's own tools/pipewire-msbc/out may hold a real
        # deb on a dev machine, so isolation comes from --msbc-deb pointing
        # at a nonexistent path being an error too.)
        output_dir2 = tmp_dir / "dist2"
        result = run_packager(
            build_dir, output_dir2, "--msbc-deb", str(tmp_dir / "no-such.deb")
        )
        if result.returncode == 0:
            raise AssertionError("packaging succeeded with a missing --msbc-deb")

        # Case 3: the development override packages without the deb.
        output_dir3 = tmp_dir / "dist3"
        result = run_packager(build_dir, output_dir3, "--allow-missing-msbc-deb")
        if result.returncode != 0:
            raise AssertionError(
                f"--allow-missing-msbc-deb failed:\n{result.stdout}\n{result.stderr}"
            )
        archive3 = output_dir3 / f"{package_root}.tar.gz"
        if not archive3.is_file():
            raise AssertionError(f"missing archive: {archive3}")

        # Case 4: a deb whose basename the installer's glob would never match
        # must be refused — it would package "successfully" and then be
        # silently skipped at install time (silent-mic HFP release).
        misnamed_deb = tmp_dir / "codec-fix.deb"
        misnamed_deb.write_bytes(b"!<arch>fake-deb")
        output_dir4 = tmp_dir / "dist4"
        result = run_packager(build_dir, output_dir4, "--msbc-deb", str(misnamed_deb))
        if result.returncode == 0:
            raise AssertionError("packaging accepted a misnamed --msbc-deb")
        if "basename must match" not in (result.stdout + result.stderr):
            raise AssertionError(
                f"misnamed --msbc-deb rejected for the wrong reason:\n{result.stderr}"
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
