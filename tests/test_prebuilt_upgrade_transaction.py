#!/usr/bin/env python3
"""Exercise the prebuilt installer's staged swap and rollback boundary."""

from __future__ import annotations

import hashlib
import os
import pathlib
import shutil
import socket
import stat
import subprocess
import tempfile
from contextlib import contextmanager


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
INSTALLER = REPO_ROOT / "install-prebuilt.sh"
MANAGED_SERVICES = (
    "openauto-prodigy.service",
    "openauto-prodigy-web.service",
    "openauto-system.service",
)

FAILURE_POINTS = (
    "after-services-stopped",
    "after-live-retired",
    "after-new-payload",
    "after-preflight",
    "after-application-unit",
    "after-web-unit",
    "after-system-unit",
    "after-daemon-reload",
    "after-services-restored",
    "after-readiness",
)


def extract_function(name: str) -> str:
    lines = INSTALLER.read_text(encoding="utf-8").splitlines()
    start = next((i for i, line in enumerate(lines) if line == f"{name}() {{"), None)
    if start is None:
        raise AssertionError(f"installer does not define {name}()")
    depth = 0
    for end in range(start, len(lines)):
        depth += lines[end].count("{") - lines[end].count("}")
        if depth == 0:
            return "\n".join(lines[start : end + 1])
    raise AssertionError(f"unterminated installer function: {name}")


def test_production_transaction_wiring() -> None:
    main = extract_function("main")
    ordered = (
        "require_payload",
        "stage_prebuilt_payload",
        "begin_prebuilt_transaction",
        "deploy_payload",
        "create_preflight_script",
        "create_service",
        "create_web_service",
        "create_system_service",
        "commit_prebuilt_transaction",
    )
    positions = [main.index(name) for name in ordered]
    if positions != sorted(positions):
        raise AssertionError("production prebuilt flow crosses its transaction boundaries")
    if "enable --now" in extract_function("create_web_service"):
        raise AssertionError("web unit creation still starts an initially inactive service")


def write_file(path: pathlib.Path, data: str, mode: int = 0o644) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(data, encoding="utf-8")
    path.chmod(mode)


def make_payload(root: pathlib.Path) -> pathlib.Path:
    payload = root / "payload"
    write_file(payload / "build/src/openauto-prodigy", "new-binary\n", 0o755)
    write_file(payload / "config/themes/default/theme.yaml", "name: new\n")
    for relative in (
        "config/systemd/bluetooth-compat.conf",
        "config/systemd/openauto-prodigy-hostapd.conf",
        "config/systemd/hostapd-openauto.conf",
        "config/systemd/openauto-prodigy.service.in",
        "config/installer/hardware-contracts.sh",
        "config/installer/openauto-preflight",
    ):
        source = REPO_ROOT / relative
        destination = payload / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    write_file(payload / "system-service/openauto_system.py", "# new system\n")
    write_file(payload / "web-config/server.py", "# new web\n")
    write_file(payload / "restart.sh", "#!/bin/sh\necho new\n", 0o755)
    return payload


def make_old_install(root: pathlib.Path) -> pathlib.Path:
    install = root / "live" / "openauto-prodigy"
    write_file(install / "build/src/openauto-prodigy", "old-binary\n", 0o755)
    write_file(install / "config/old.conf", "old-config\n")
    write_file(install / "system-service/openauto_system.py", "# old system\n")
    write_file(install / "web-config/server.py", "# old web\n")
    write_file(install / "restart.sh", "#!/bin/sh\necho old\n", 0o755)
    write_file(install / "preserved-user-file", "preserve me\n")
    return install


def make_external_assets(root: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path, pathlib.Path]:
    unit_dir = root / "units"
    preflight = root / "bin/openauto-preflight"
    for name in (
        "openauto-prodigy.service",
        "openauto-prodigy-web.service",
        "openauto-system.service",
    ):
        write_file(unit_dir / name, f"old:{name}\n")
    write_file(preflight, "#!/bin/sh\necho old-preflight\n", 0o755)

    new_assets = root / "new-assets"
    write_file(new_assets / "application-unit", "new:application\n")
    write_file(new_assets / "web-unit", "new:web\n")
    write_file(new_assets / "system-unit", "new:system\n")
    write_file(new_assets / "preflight", "#!/bin/sh\necho new-preflight\n", 0o755)
    return unit_dir, preflight, new_assets


def snapshot(path: pathlib.Path) -> dict[str, tuple[int, str]]:
    result: dict[str, tuple[int, str]] = {}
    if not path.exists():
        return result
    paths = [path] if path.is_file() else sorted(path.rglob("*"))
    for item in paths:
        relative = "." if item == path else str(item.relative_to(path))
        mode = stat.S_IMODE(item.lstat().st_mode)
        if item.is_file():
            digest = hashlib.sha256(item.read_bytes()).hexdigest()
            result[relative] = (mode, digest)
        elif item.is_dir():
            result[relative + "/"] = (mode, "directory")
        elif item.is_symlink():
            result[relative] = (mode, f"symlink:{os.readlink(item)}")
    return result


def make_fake_commands(root: pathlib.Path) -> pathlib.Path:
    fake_bin = root / "fake-bin"
    fake_bin.mkdir()
    write_file(fake_bin / "sudo", "#!/usr/bin/env bash\nexec \"$@\"\n", 0o755)
    write_file(
        fake_bin / "systemctl",
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n"
        "command=${1:-}; shift || true\n"
        "printf '%s %s\\n' \"$command\" \"$*\" >> \"$TEST_SYSTEMCTL_LOG\"\n"
        "case \"$command\" in\n"
        "  is-active)\n"
        "    [[ ${1:-} == --quiet ]] && shift\n"
        "    state=$(cat \"$TEST_STATE_DIR/active/$1\")\n"
        "    if [[ ${TEST_ACTIVATE_AFTER_CAPTURE:-} == \"$1\" "
        "&& ! -e $TEST_STATE_DIR/race-consumed ]]; then\n"
        "      touch \"$TEST_STATE_DIR/race-consumed\"\n"
        "      echo active > \"$TEST_STATE_DIR/active/$1\"\n"
        "    fi\n"
        "    [[ $state == active ]]\n"
        "    ;;\n"
        "  is-enabled)\n"
        "    state=$(cat \"$TEST_STATE_DIR/enablement/$1\")\n"
        "    printf '%s\\n' \"$state\"\n"
        "    [[ $state == enabled || $state == enabled-runtime ]]\n"
        "    ;;\n"
        "  stop)\n"
        "    echo inactive > \"$TEST_STATE_DIR/active/$1\"\n"
        "    [[ $1 != openauto-prodigy.service ]] || rm -f \"$TEST_RUNNING_PAYLOAD\"\n"
        "    ;;\n"
        "  start)\n"
        "    state=$(cat \"$TEST_STATE_DIR/enablement/$1\")\n"
        "    [[ $state != masked && $state != masked-runtime ]]\n"
        "    echo active > \"$TEST_STATE_DIR/active/$1\"\n"
        "    if [[ $1 == openauto-prodigy.service ]]; then\n"
        "      cp \"$TEST_INSTALL_DIR/build/src/openauto-prodigy\" \"$TEST_RUNNING_PAYLOAD\"\n"
        "    fi\n"
        "    if [[ $1 == openauto-prodigy.service && ${TEST_PULL_SYSTEM:-false} == true ]]; then\n"
        "      echo active > \"$TEST_STATE_DIR/active/openauto-system.service\"\n"
        "    fi\n"
        "    ;;\n"
        "  enable|mask)\n"
        "    runtime=false\n"
        "    [[ ${1:-} != --runtime ]] || { runtime=true; shift; }\n"
        "    if [[ $command == enable ]]; then\n"
        "      [[ $runtime == true ]] && state=enabled-runtime || state=enabled\n"
        "    else\n"
        "      [[ $runtime == true ]] && state=masked-runtime || state=masked\n"
        "    fi\n"
        "    echo \"$state\" > \"$TEST_STATE_DIR/enablement/$1\"\n"
        "    ;;\n"
        "  disable)\n"
        "    echo disabled > \"$TEST_STATE_DIR/enablement/$1\"\n"
        "    ;;\n"
        "  unmask)\n"
        "    [[ ${1:-} != --runtime ]] || shift\n"
        "    state=$(cat \"$TEST_STATE_DIR/enablement/$1\")\n"
        "    if [[ $state == masked || $state == masked-runtime ]]; then\n"
        "      echo disabled > \"$TEST_STATE_DIR/enablement/$1\"\n"
        "    fi\n"
        "    ;;\n"
        "  daemon-reload) : ;;\n"
        "  *) printf 'unexpected systemctl command: %s %s\\n' \"$command\" \"$*\" >&2; exit 90 ;;\n"
        "esac\n",
        0o755,
    )
    return fake_bin


def make_case(
    initial_states: dict[str, bool],
    initial_enablement: dict[str, str] | None = None,
):
    temporary = tempfile.TemporaryDirectory(prefix="oap-prebuilt-transaction-")
    root = pathlib.Path(temporary.name)
    payload = make_payload(root)
    install = make_old_install(root)
    unit_dir, preflight, assets = make_external_assets(root)
    fake_bin = make_fake_commands(root)
    state_dir = root / "states"
    active_dir = state_dir / "active"
    enablement_dir = state_dir / "enablement"
    active_dir.mkdir(parents=True)
    enablement_dir.mkdir()
    initial_enablement = initial_enablement or {}
    for service in MANAGED_SERVICES:
        write_file(
            active_dir / service,
            "active\n" if initial_states.get(service, False) else "inactive\n",
        )
        write_file(
            enablement_dir / service,
            initial_enablement.get(service, "disabled") + "\n",
        )
    log = root / "systemctl.log"
    env = os.environ.copy()
    env.update(
        {
            "PATH": f"{fake_bin}:/usr/bin:/bin",
            "OAP_PREBUILT_TEST_ACTION": "transaction",
            "OAP_PREBUILT_TEST_ASSETS_DIR": str(assets),
            "OAP_PAYLOAD_DIR": str(payload),
            "OAP_INSTALL_DIR": str(install),
            "OAP_CONFIG_DIR": str(root / "config-home"),
            "OAP_SYSTEMD_UNIT_DIR": str(unit_dir),
            "OAP_PREFLIGHT_DEST": str(preflight),
            "TEST_STATE_DIR": str(state_dir),
            "TEST_SYSTEMCTL_LOG": str(log),
            "TEST_PULL_SYSTEM": "true",
            "TEST_INSTALL_DIR": str(install),
            "TEST_RUNNING_PAYLOAD": str(root / "running-payload"),
            "OAP_PREBUILT_TEST_AUTOSTART": "true",
            "OAP_PREBUILT_WEB_READY_TIMEOUT_SECONDS": "1",
            "USER": "test-user",
        }
    )
    return temporary, root, install, unit_dir, preflight, state_dir, log, env


def service_states(state_dir: pathlib.Path) -> dict[str, bool]:
    return {
        service: (state_dir / "active" / service).read_text(encoding="utf-8").strip()
        == "active"
        for service in MANAGED_SERVICES
    }


def service_enablement_states(state_dir: pathlib.Path) -> dict[str, str]:
    return {
        service: (state_dir / "enablement" / service)
        .read_text(encoding="utf-8")
        .strip()
        for service in MANAGED_SERVICES
    }


def external_snapshot(unit_dir: pathlib.Path, preflight: pathlib.Path):
    return {"units": snapshot(unit_dir), "preflight": snapshot(preflight)}


def assert_no_transaction_debris(install: pathlib.Path) -> None:
    debris = list(install.parent.glob(".oap-upgrade.*"))
    if debris:
        raise AssertionError(f"transaction debris remains: {debris}")


def run_case(env: dict[str, str], fail_at: str | None = None):
    case_env = env.copy()
    if fail_at:
        case_env["OAP_PREBUILT_FAIL_AT"] = fail_at
    return subprocess.run(
        ["bash", str(INSTALLER)],
        cwd=REPO_ROOT,
        env=case_env,
        capture_output=True,
        text=True,
        timeout=30,
    )


@contextmanager
def listening_web_socket(env: dict[str, str]):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(4)
    env["OAP_PREBUILT_WEB_READY_PORT"] = str(listener.getsockname()[1])
    try:
        yield
    finally:
        listener.close()


def select_unbound_web_port(env: dict[str, str]) -> None:
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.bind(("127.0.0.1", 0))
    env["OAP_PREBUILT_WEB_READY_PORT"] = str(probe.getsockname()[1])
    probe.close()


def test_failure_rollbacks() -> None:
    initial = {
        "openauto-prodigy.service": True,
        "openauto-prodigy-web.service": True,
        "openauto-system.service": True,
    }
    initial_enablement = {
        "openauto-prodigy.service": "enabled",
        "openauto-prodigy-web.service": "disabled",
        "openauto-system.service": "masked",
    }
    for failure in FAILURE_POINTS:
        temporary, _, install, units, preflight, states, _, env = make_case(
            initial, initial_enablement
        )
        try:
            old_payload = snapshot(install)
            old_external = external_snapshot(units, preflight)
            with listening_web_socket(env):
                result = run_case(env, failure)
            if result.returncode == 0:
                raise AssertionError(f"injected failure unexpectedly succeeded: {failure}")
            if snapshot(install) != old_payload:
                raise AssertionError(f"{failure} did not restore the prior managed payload")
            if external_snapshot(units, preflight) != old_external:
                raise AssertionError(f"{failure} did not restore prior unit/preflight bytes")
            if service_states(states) != initial:
                raise AssertionError(f"{failure} changed the prior active service set")
            if service_enablement_states(states) != initial_enablement:
                raise AssertionError(
                    f"{failure} changed prior enablement/mask state: "
                    f"{service_enablement_states(states)}"
                )
            running = pathlib.Path(env["TEST_RUNNING_PAYLOAD"])
            if initial["openauto-prodigy.service"] and running.read_text() != "old-binary\n":
                raise AssertionError(f"{failure} left the application on a retired payload")
            assert_no_transaction_debris(install)
        finally:
            temporary.cleanup()


def test_success_and_inactive_preservation() -> None:
    for initial in (
        {
            "openauto-prodigy.service": True,
            "openauto-prodigy-web.service": True,
            "openauto-system.service": False,
        },
        {
            "openauto-prodigy.service": False,
            "openauto-prodigy-web.service": False,
            "openauto-system.service": False,
        },
    ):
        temporary, root, install, units, preflight, states, log, env = make_case(initial)
        try:
            if initial["openauto-prodigy-web.service"]:
                with listening_web_socket(env):
                    result = run_case(env)
            else:
                select_unbound_web_port(env)
                result = run_case(env)
            if result.returncode != 0:
                raise AssertionError(f"successful transaction failed:\n{result.stdout}\n{result.stderr}")
            if (install / "build/src/openauto-prodigy").read_text() != "new-binary\n":
                raise AssertionError("successful transaction did not activate the new payload")
            if (install / "preserved-user-file").read_text() != "preserve me\n":
                raise AssertionError("transaction changed an unmanaged install-root file")
            if (units / "openauto-prodigy.service").read_text() != "new:application\n":
                raise AssertionError("successful transaction did not install the new app unit")
            if preflight.read_text() != "#!/bin/sh\necho new-preflight\n":
                raise AssertionError("successful transaction did not install the new preflight")
            if service_states(states) != initial:
                raise AssertionError("successful transaction changed the active service set")
            expected_enablement = {service: "enabled" for service in MANAGED_SERVICES}
            if service_enablement_states(states) != expected_enablement:
                raise AssertionError(
                    "successful transaction did not exercise production enable mutations: "
                    f"{service_enablement_states(states)}"
                )
            running = pathlib.Path(env["TEST_RUNNING_PAYLOAD"])
            if initial["openauto-prodigy.service"]:
                if running.read_text() != "new-binary\n":
                    raise AssertionError("application did not start from the new payload")
            elif running.exists():
                raise AssertionError("inactive application was started during upgrade")
            starts = {
                line.split(" ", 1)[1]
                for line in log.read_text().splitlines()
                if line.startswith("start ")
            }
            expected_starts = {service for service, active in initial.items() if active}
            if starts != expected_starts:
                raise AssertionError(
                    f"transaction explicitly started the wrong services: {starts}"
                )
            assert_no_transaction_debris(install)
        finally:
            temporary.cleanup()


def test_web_readiness_failure_rolls_back() -> None:
    initial = {
        "openauto-prodigy.service": True,
        "openauto-prodigy-web.service": True,
        "openauto-system.service": False,
    }
    temporary, _, install, units, preflight, states, _, env = make_case(initial)
    try:
        old_payload = snapshot(install)
        old_external = external_snapshot(units, preflight)
        select_unbound_web_port(env)
        result = run_case(env)
        if result.returncode == 0:
            raise AssertionError("started-but-never-bound web service committed the upgrade")
        if "Web config socket did not become ready" not in result.stdout:
            raise AssertionError(f"web readiness failure was not reported: {result.stdout}")
        if snapshot(install) != old_payload:
            raise AssertionError("web readiness failure did not restore the prior payload")
        if external_snapshot(units, preflight) != old_external:
            raise AssertionError("web readiness failure did not restore external assets")
        if service_states(states) != initial:
            raise AssertionError("web readiness failure changed the prior active set")
        assert_no_transaction_debris(install)
    finally:
        temporary.cleanup()


def test_inactive_capture_race_is_stopped() -> None:
    initial = {service: False for service in MANAGED_SERVICES}
    temporary, _, install, _, _, states, log, env = make_case(initial)
    try:
        env["TEST_ACTIVATE_AFTER_CAPTURE"] = "openauto-prodigy-web.service"
        select_unbound_web_port(env)
        result = run_case(env)
        if result.returncode != 0:
            raise AssertionError(f"inactive-capture race transaction failed: {result.stderr}")
        if service_states(states) != initial:
            raise AssertionError("capture/stop race changed the prior inactive set")
        stops = {
            line.split(" ", 1)[1]
            for line in log.read_text().splitlines()
            if line.startswith("stop ")
        }
        if stops != set(MANAGED_SERVICES):
            raise AssertionError(f"transaction did not stop every managed service: {stops}")
        assert_no_transaction_debris(install)
    finally:
        temporary.cleanup()


def test_incomplete_payload_precedes_stop() -> None:
    initial = {
        "openauto-prodigy.service": True,
        "openauto-prodigy-web.service": False,
        "openauto-system.service": True,
    }
    temporary, root, install, _, _, states, log, env = make_case(initial)
    try:
        (pathlib.Path(env["OAP_PAYLOAD_DIR"]) / "config/systemd/openauto-prodigy.service.in").unlink()
        old_payload = snapshot(install)
        result = run_case(env)
        if result.returncode == 0:
            raise AssertionError("incomplete payload unexpectedly passed validation")
        if log.exists() and " stop " in f" {log.read_text()} ":
            raise AssertionError("incomplete staging stopped a managed service")
        if snapshot(install) != old_payload or service_states(states) != initial:
            raise AssertionError("incomplete staging mutated live state")
        if list(root.glob(".oap-upgrade.*")):
            raise AssertionError("incomplete staging created transaction material")
    finally:
        temporary.cleanup()


def main() -> int:
    test_production_transaction_wiring()
    test_incomplete_payload_precedes_stop()
    test_failure_rollbacks()
    test_success_and_inactive_preservation()
    test_web_readiness_failure_rolls_back()
    test_inactive_capture_race_is_stopped()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
