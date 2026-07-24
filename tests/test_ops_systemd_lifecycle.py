#!/usr/bin/env python3
"""Exercise installer-owned startup relationships in a real user systemd manager."""

from __future__ import annotations

import os
import pathlib
import shutil
import socket
import subprocess
import tempfile
import time
import uuid


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
APP_HOSTAPD_ASSET = REPO_ROOT / "config/systemd/openauto-prodigy-hostapd.conf"
HOSTAPD_ASSET = REPO_ROOT / "config/systemd/hostapd-openauto.conf"
APP_PREFLIGHT = REPO_ROOT / "config/installer/openauto-preflight"
SKIP_RETURN_CODE = 77


def systemctl(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["systemctl", "--user", *args], capture_output=True, text=True
    )
    if check and result.returncode != 0:
        raise AssertionError(
            f"systemctl --user {' '.join(args)} failed:\n"
            f"{result.stdout}\n{result.stderr}"
        )
    return result


def wait_for(predicate, description: str, timeout: float = 10.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.1)
    raise AssertionError(f"timed out waiting for {description}")


def active(unit: str) -> bool:
    return systemctl("is-active", "--quiet", unit, check=False).returncode == 0


def main_pid(unit: str) -> int:
    result = systemctl("show", "--property=MainPID", "--value", unit)
    return int(result.stdout.strip() or "0")


def wait_active_pid(unit: str, previous: int = 0) -> int:
    def probe() -> int:
        if not active(unit):
            return 0
        pid = main_pid(unit)
        return pid if pid > 0 and pid != previous else 0

    return wait_for(probe, f"{unit} to have a new active MainPID")


def write_unit(path: pathlib.Path, description: str, helper: pathlib.Path) -> None:
    path.write_text(
        "[Unit]\n"
        f"Description={description}\n"
        "[Service]\n"
        f"ExecStart={helper}\n"
    )


def test_wayland_condition(unit_dir: pathlib.Path) -> None:
    suffix = uuid.uuid4().hex[:12]
    unit = f"oap-wayland-condition-{suffix}.service"
    unit_path = unit_dir / unit
    with tempfile.TemporaryDirectory(prefix="oap-wayland-condition-") as tmp:
        root = pathlib.Path(tmp)
        runtime = root / "runtime"
        runtime.mkdir()
        wayland_socket = runtime / "wayland-test"
        marker = root / "started"
        helper = root / "notify-service.py"
        helper.write_text(
            "#!/usr/bin/env python3\n"
            "import os, pathlib, signal, socket, sys, time\n"
            "address = os.environ['NOTIFY_SOCKET']\n"
            "if address.startswith('@'):\n"
            "    address = '\\0' + address[1:]\n"
            "client = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)\n"
            "client.sendto(b'READY=1', address)\n"
            "pathlib.Path(sys.argv[1]).write_text('started\\n')\n"
            "signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))\n"
            "while True: time.sleep(1)\n"
        )
        helper.chmod(0o755)
        unit_path.write_text(
            "[Unit]\nDescription=Wayland condition test\n"
            "[Service]\n"
            "Type=notify\nNotifyAccess=main\n"
            f"Environment=XDG_RUNTIME_DIR={runtime}\n"
            "Environment=WAYLAND_DISPLAY=wayland-test\n"
            "Environment=OAP_PREFLIGHT_WAYLAND_TIMEOUT_SECONDS=1\n"
            f"ExecCondition={APP_PREFLIGHT} --wayland\n"
            f"ExecStart={helper} {marker}\n"
            "Restart=on-failure\nRestartSec=1\n"
        )

        server: socket.socket | None = None
        try:
            systemctl("daemon-reload")
            result = systemctl("start", unit, check=False)
            if result.returncode != 0:
                raise AssertionError(f"skipped Wayland start failed: {result.stderr}")
            if active(unit) or marker.exists():
                raise AssertionError("missing Wayland socket launched the application")
            if systemctl("is-failed", "--quiet", unit, check=False).returncode == 0:
                raise AssertionError("missing Wayland socket marked the unit failed")
            restarts = systemctl("show", "--property=NRestarts", "--value", unit)
            if restarts.stdout.strip() not in ("", "0"):
                raise AssertionError("missing Wayland socket entered a restart loop")

            server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            server.bind(str(wayland_socket))
            server.listen(1)
            systemctl("start", unit)
            wait_active_pid(unit)
            wait_for(marker.exists, "conditioned application helper to start")
        finally:
            systemctl("stop", unit, check=False)
            systemctl("reset-failed", unit, check=False)
            unit_path.unlink(missing_ok=True)
            systemctl("daemon-reload", check=False)
            if server is not None:
                server.close()


def main() -> int:
    if shutil.which("systemctl") is None:
        print("SKIP: systemctl is unavailable")
        return SKIP_RETURN_CODE
    manager_probe = systemctl("show-environment", check=False)
    if manager_probe.returncode != 0:
        print("SKIP: a running user systemd manager is unavailable")
        return SKIP_RETURN_CODE

    runtime_dir = pathlib.Path(
        os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")
    )
    unit_dir = runtime_dir / "systemd/user"
    unit_dir.mkdir(parents=True, exist_ok=True)
    test_wayland_condition(unit_dir)

    suffix = uuid.uuid4().hex[:12]
    app_unit = f"oap-lifecycle-app-{suffix}.service"
    ap_unit = f"oap-lifecycle-ap-{suffix}.service"
    no_ap_unit = f"oap-lifecycle-no-ap-{suffix}.service"
    app_path = unit_dir / app_unit
    ap_path = unit_dir / ap_unit
    no_ap_path = unit_dir / no_ap_unit
    app_dropin_dir = unit_dir / f"{app_unit}.d"
    ap_dropin_dir = unit_dir / f"{ap_unit}.d"

    with tempfile.TemporaryDirectory(prefix="oap-lifecycle-helper-") as tmp:
        helper = pathlib.Path(tmp) / "hold-service"
        helper.write_text(
            "#!/bin/sh\n"
            "trap 'exit 0' TERM INT\n"
            "while :; do\n"
            "    sleep 3600 &\n"
            "    wait $!\n"
            "done\n"
        )
        helper.chmod(0o755)

        try:
            write_unit(app_path, "Lifecycle application", helper)
            write_unit(ap_path, "Lifecycle access point", helper)
            write_unit(no_ap_path, "Lifecycle no-AP application", helper)
            app_dropin_dir.mkdir()
            ap_dropin_dir.mkdir()
            (app_dropin_dir / "hostapd.conf").write_text(
                APP_HOSTAPD_ASSET.read_text().replace("hostapd.service", ap_unit)
            )
            (ap_dropin_dir / "openauto.conf").write_text(
                HOSTAPD_ASSET.read_text().replace(
                    "/usr/sbin/rfkill unblock wlan", "/bin/true"
                )
            )

            systemctl("daemon-reload")
            systemctl("start", app_unit)
            app_pid = wait_active_pid(app_unit)
            ap_pid = wait_active_pid(ap_unit)

            systemctl("kill", "--kill-whom=all", "--signal=KILL", ap_unit)
            recovered_ap_pid = wait_active_pid(ap_unit, ap_pid)
            if main_pid(app_unit) != app_pid or not active(app_unit):
                raise AssertionError("application changed when the AP process crashed")

            systemctl("restart", app_unit)
            restarted_app_pid = wait_active_pid(app_unit, app_pid)
            if restarted_app_pid == app_pid:
                raise AssertionError("application restart did not replace its process")
            if main_pid(ap_unit) != recovered_ap_pid or not active(ap_unit):
                raise AssertionError("application restart replaced the AP process")

            systemctl("stop", app_unit, ap_unit)
            systemctl("start", no_ap_unit)
            wait_active_pid(no_ap_unit)
            if active(ap_unit):
                raise AssertionError("no-AP application unexpectedly started the AP")
        finally:
            systemctl("stop", app_unit, ap_unit, no_ap_unit, check=False)
            systemctl("reset-failed", app_unit, ap_unit, no_ap_unit, check=False)
            for path in (app_path, ap_path, no_ap_path):
                path.unlink(missing_ok=True)
            shutil.rmtree(app_dropin_dir, ignore_errors=True)
            shutil.rmtree(ap_dropin_dir, ignore_errors=True)
            systemctl("daemon-reload", check=False)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
