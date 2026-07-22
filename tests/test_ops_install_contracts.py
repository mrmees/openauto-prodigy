#!/usr/bin/env python3
"""Regression tests for installer-owned operational contracts."""

from __future__ import annotations

import argparse
import pathlib
import shlex
import shutil
import stat
import subprocess
import tempfile


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
BLUETOOTH_ASSET = REPO_ROOT / "config/systemd/bluetooth-compat.conf"
APP_HOSTAPD_ASSET = REPO_ROOT / "config/systemd/openauto-prodigy-hostapd.conf"
HOSTAPD_ASSET = REPO_ROOT / "config/systemd/hostapd-openauto.conf"
SOURCE_INSTALLER = REPO_ROOT / "install.sh"
PREBUILT_INSTALLER = REPO_ROOT / "install-prebuilt.sh"


def extract_function(script: pathlib.Path, name: str) -> str:
    lines = script.read_text().splitlines()
    start = next(
        (index for index, line in enumerate(lines) if line == f"{name}() {{"),
        None,
    )
    if start is None:
        raise AssertionError(f"{script.name} does not define {name}()")

    for end in range(start + 1, len(lines)):
        if lines[end] == "}":
            return "\n".join(lines[start : end + 1]) + "\n"
    raise AssertionError(f"{script.name} has an unterminated {name}()")


def assert_installer_materializes_asset(
    script: pathlib.Path, source_variable: str
) -> None:
    function = extract_function(script, "configure_bluetooth")
    with tempfile.TemporaryDirectory(prefix="oap-bluetooth-install-") as tmp:
        tmp_dir = pathlib.Path(tmp)
        installed = tmp_dir / "override.conf"
        systemctl_log = tmp_dir / "systemctl.log"
        shell = f"""
set -euo pipefail
{source_variable}={shlex.quote(str(REPO_ROOT))}
TEST_DEST={shlex.quote(str(installed))}
TEST_SYSTEMCTL_LOG={shlex.quote(str(systemctl_log))}
ok() {{ :; }}
fail() {{ printf '%s\n' "$*" >&2; }}
sudo() {{
    case "$1" in
        install)
            [[ "$2" == "-D" && "$3" == "-m" && "$4" == "0644" ]]
            [[ "$6" == "/etc/systemd/system/bluetooth.service.d/override.conf" ]]
            /usr/bin/install -D -m "$4" "$5" "$TEST_DEST"
            ;;
        systemctl)
            printf '%s\n' "$*" >> "$TEST_SYSTEMCTL_LOG"
            ;;
        *)
            printf 'unexpected sudo command: %s\n' "$*" >&2
            return 1
            ;;
    esac
}}
{function}
configure_bluetooth
"""
        result = subprocess.run(
            ["bash", "-c", shell], cwd=REPO_ROOT, capture_output=True, text=True
        )
        if result.returncode != 0:
            raise AssertionError(
                f"{script.name} Bluetooth install harness failed:\n"
                f"{result.stdout}\n{result.stderr}"
            )

        if installed.read_bytes() != BLUETOOTH_ASSET.read_bytes():
            raise AssertionError(f"{script.name} did not install the canonical bytes")
        if stat.S_IMODE(installed.stat().st_mode) != 0o644:
            raise AssertionError(f"{script.name} installed the wrong file mode")
        if systemctl_log.read_text().splitlines() != [
            "systemctl daemon-reload",
            "systemctl restart bluetooth",
        ]:
            raise AssertionError(f"{script.name} did not reload and restart BlueZ")


def verify_systemd_fragment() -> None:
    expected_lines = [
        "[Service]",
        "ExecStart=",
        "ExecStart=/usr/libexec/bluetooth/bluetoothd --compat",
    ]
    asset_text = BLUETOOTH_ASSET.read_text()
    for line in expected_lines:
        if line not in asset_text.splitlines():
            raise AssertionError(f"Bluetooth asset is missing: {line}")
    if "[ -e /var/run/sdp ]" not in asset_text:
        raise AssertionError("Bluetooth asset does not wait for an existing SDP socket")
    if "chgrp bluetooth /var/run/sdp" not in asset_text:
        raise AssertionError("Bluetooth asset does not assign the SDP socket group")
    if "chmod g+rw /var/run/sdp" not in asset_text:
        raise AssertionError("Bluetooth asset does not make the SDP socket group-writable")

    systemd_analyze = shutil.which("systemd-analyze")
    if systemd_analyze is None:
        raise AssertionError("systemd-analyze is required for installer contract tests")

    with tempfile.TemporaryDirectory(prefix="oap-systemd-verify-") as tmp:
        root = pathlib.Path(tmp)
        unit_dir = root / "usr/lib/systemd/system"
        dropin_dir = root / "etc/systemd/system/bluetooth.service.d"
        bluetoothd = root / "usr/libexec/bluetooth/bluetoothd"
        shell = root / "bin/sh"
        unit_dir.mkdir(parents=True)
        dropin_dir.mkdir(parents=True)
        bluetoothd.parent.mkdir(parents=True)
        shell.parent.mkdir(parents=True)

        (unit_dir / "bluetooth.service").write_text(
            "[Unit]\nDescription=Test Bluetooth\n"
            "[Service]\nType=simple\n"
            "ExecStart=/usr/libexec/bluetooth/bluetoothd\n"
        )
        (unit_dir / "sysinit.target").write_text("[Unit]\nDescription=Sysinit\n")
        shutil.copyfile(BLUETOOTH_ASSET, dropin_dir / "override.conf")
        (dropin_dir / "override.conf").chmod(0o644)
        bluetoothd.touch()
        shell.touch()
        bluetoothd.chmod(0o755)
        shell.chmod(0o755)

        result = subprocess.run(
            [systemd_analyze, "verify", f"--root={root}", "bluetooth.service"],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            raise AssertionError(
                f"systemd-analyze rejected bluetooth-compat.conf:\n{result.stderr}"
            )


def test_bluetooth() -> None:
    source_text = SOURCE_INSTALLER.read_text()
    source_main = extract_function(SOURCE_INSTALLER, "main")
    prebuilt_main = extract_function(PREBUILT_INSTALLER, "main")
    prebuilt_require_payload = extract_function(PREBUILT_INSTALLER, "require_payload")

    if source_main.index("configure_bluetooth") > source_main.index("configure_network"):
        raise AssertionError("source Bluetooth setup must precede optional WiFi setup")
    if prebuilt_main.index("configure_bluetooth") > prebuilt_main.index(
        "configure_network"
    ):
        raise AssertionError("prebuilt Bluetooth setup must precede optional WiFi setup")

    source_network = extract_function(SOURCE_INSTALLER, "configure_network")
    prebuilt_network = extract_function(PREBUILT_INSTALLER, "configure_network")
    if "bluetooth.service" in source_network or "bluetooth.service" in prebuilt_network:
        raise AssertionError("Bluetooth setup is still coupled to WiFi configuration")

    required_payload = '"$PAYLOAD_DIR/config/systemd/bluetooth-compat.conf"'
    if required_payload not in prebuilt_require_payload:
        raise AssertionError("prebuilt installer does not require the compatibility asset")
    if "ExecStart=/usr/libexec/bluetooth/bluetoothd --compat" in source_text:
        raise AssertionError("source installer still carries an inline BlueZ fragment")
    if (
        "ExecStart=/usr/libexec/bluetooth/bluetoothd --compat"
        in PREBUILT_INSTALLER.read_text()
    ):
        raise AssertionError("prebuilt installer carries an inline BlueZ fragment")

    assert_installer_materializes_asset(SOURCE_INSTALLER, "INSTALL_DIR")
    assert_installer_materializes_asset(PREBUILT_INSTALLER, "PAYLOAD_DIR")
    verify_systemd_fragment()


def assert_installer_materializes_hostapd_assets(
    script: pathlib.Path, source_variable: str
) -> None:
    function = extract_function(script, "configure_hostapd_lifecycle")
    with tempfile.TemporaryDirectory(prefix="oap-hostapd-install-") as tmp:
        tmp_dir = pathlib.Path(tmp)
        installed_app = tmp_dir / "app-hostapd.conf"
        installed_hostapd = tmp_dir / "hostapd-openauto.conf"
        systemctl_log = tmp_dir / "systemctl.log"
        shell = f"""
set -euo pipefail
{source_variable}={shlex.quote(str(REPO_ROOT))}
SERVICE_NAME=openauto-prodigy
TEST_APP={shlex.quote(str(installed_app))}
TEST_HOSTAPD={shlex.quote(str(installed_hostapd))}
TEST_SYSTEMCTL_LOG={shlex.quote(str(systemctl_log))}
ok() {{ :; }}
warn() {{ :; }}
fail() {{ printf '%s\n' "$*" >&2; }}
sudo() {{
    case "$1" in
        install)
            [[ "$2" == "-D" && "$3" == "-m" && "$4" == "0644" ]]
            case "$6" in
                /etc/systemd/system/openauto-prodigy.service.d/hostapd.conf)
                    /usr/bin/install -D -m "$4" "$5" "$TEST_APP"
                    ;;
                /etc/systemd/system/hostapd.service.d/openauto.conf)
                    /usr/bin/install -D -m "$4" "$5" "$TEST_HOSTAPD"
                    ;;
                *)
                    printf 'unexpected install destination: %s\n' "$6" >&2
                    return 1
                    ;;
            esac
            ;;
        rm)
            [[ "$2" == "-f" ]]
            /usr/bin/rm -f "$TEST_APP" "$TEST_HOSTAPD"
            ;;
        systemctl)
            printf '%s\n' "$*" >> "$TEST_SYSTEMCTL_LOG"
            ;;
        *)
            printf 'unexpected sudo command: %s\n' "$*" >&2
            return 1
            ;;
    esac
}}
{function}
WIFI_IFACE=wlan0
configure_hostapd_lifecycle
cmp "$TEST_APP" {shlex.quote(str(APP_HOSTAPD_ASSET))}
cmp "$TEST_HOSTAPD" {shlex.quote(str(HOSTAPD_ASSET))}
[[ "$(stat -c %a "$TEST_APP")" == 644 ]]
[[ "$(stat -c %a "$TEST_HOSTAPD")" == 644 ]]
WIFI_IFACE=""
configure_hostapd_lifecycle
[[ ! -e "$TEST_APP" && ! -e "$TEST_HOSTAPD" ]]
"""
        result = subprocess.run(
            ["bash", "-c", shell], cwd=REPO_ROOT, capture_output=True, text=True
        )
        if result.returncode != 0:
            raise AssertionError(
                f"{script.name} hostapd install harness failed:\n"
                f"{result.stdout}\n{result.stderr}"
            )

        if systemctl_log.read_text().splitlines() != [
            "systemctl daemon-reload",
            "systemctl daemon-reload",
        ]:
            raise AssertionError(
                f"{script.name} did not reload units after install and cleanup"
            )


def verify_hostapd_fragments() -> None:
    app_lines = APP_HOSTAPD_ASSET.read_text().splitlines()
    if app_lines != ["[Unit]", "Wants=hostapd.service", "After=hostapd.service"]:
        raise AssertionError("application hostapd asset is not optional Wants/After")

    hostapd_text = HOSTAPD_ASSET.read_text()
    for line in (
        "StartLimitIntervalSec=60",
        "StartLimitBurst=5",
        "ExecStartPre=/usr/sbin/rfkill unblock wlan",
        "Restart=on-failure",
        "RestartSec=3",
    ):
        if line not in hostapd_text.splitlines():
            raise AssertionError(f"hostapd recovery asset is missing: {line}")

    systemd_analyze = shutil.which("systemd-analyze")
    if systemd_analyze is None:
        raise AssertionError("systemd-analyze is required for installer contract tests")

    with tempfile.TemporaryDirectory(prefix="oap-hostapd-verify-") as tmp:
        root = pathlib.Path(tmp)
        unit_dir = root / "usr/lib/systemd/system"
        app_dropin = root / "etc/systemd/system/openauto-prodigy.service.d"
        hostapd_dropin = root / "etc/systemd/system/hostapd.service.d"
        unit_dir.mkdir(parents=True)
        app_dropin.mkdir(parents=True)
        hostapd_dropin.mkdir(parents=True)
        (root / "bin").mkdir()
        (root / "usr/sbin").mkdir(parents=True)

        (unit_dir / "openauto-prodigy.service").write_text(
            "[Unit]\nDescription=Test application\n"
            "[Service]\nExecStart=/bin/true\n"
        )
        (unit_dir / "hostapd.service").write_text(
            "[Unit]\nDescription=Test access point\n"
            "[Service]\nExecStart=/bin/true\n"
        )
        (unit_dir / "sysinit.target").write_text("[Unit]\nDescription=Sysinit\n")
        shutil.copyfile(APP_HOSTAPD_ASSET, app_dropin / "hostapd.conf")
        shutil.copyfile(HOSTAPD_ASSET, hostapd_dropin / "openauto.conf")
        for executable in (root / "bin/true", root / "usr/sbin/rfkill"):
            executable.touch()
            executable.chmod(0o755)

        result = subprocess.run(
            [
                systemd_analyze,
                "verify",
                f"--root={root}",
                "openauto-prodigy.service",
                "hostapd.service",
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            raise AssertionError(
                f"systemd-analyze rejected hostapd lifecycle assets:\n{result.stderr}"
            )


def test_hostapd() -> None:
    for script in (SOURCE_INSTALLER, PREBUILT_INSTALLER):
        script_text = script.read_text()
        if "BindsTo=hostapd.service" in script_text:
            raise AssertionError(f"{script.name} still hard-binds the app to hostapd")
        if "PartOf=openauto-prodigy.service" in script_text:
            raise AssertionError(f"{script.name} still propagates app stops to hostapd")

        network = extract_function(script, "configure_network")
        if network.index("configure_hostapd_lifecycle") > network.index(
            'if [[ -z "$WIFI_IFACE" ]]'
        ):
            raise AssertionError(
                f"{script.name} does not clean stale drop-ins on a no-AP install"
            )

    prebuilt_require_payload = extract_function(PREBUILT_INSTALLER, "require_payload")
    for required in (
        '"$PAYLOAD_DIR/config/systemd/openauto-prodigy-hostapd.conf"',
        '"$PAYLOAD_DIR/config/systemd/hostapd-openauto.conf"',
    ):
        if required not in prebuilt_require_payload:
            raise AssertionError(f"prebuilt installer does not require {required}")

    assert_installer_materializes_hostapd_assets(SOURCE_INSTALLER, "INSTALL_DIR")
    assert_installer_materializes_hostapd_assets(PREBUILT_INSTALLER, "PAYLOAD_DIR")
    verify_hostapd_fragments()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--case", choices=("all", "bluetooth", "hostapd"), default="all"
    )
    args = parser.parse_args()

    if args.case in ("all", "bluetooth"):
        test_bluetooth()
    if args.case in ("all", "hostapd"):
        test_hostapd()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
