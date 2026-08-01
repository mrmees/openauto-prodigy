#!/usr/bin/env python3
"""Regression tests for source-installer ownership and lifecycle boundaries."""

from __future__ import annotations

import os
import pathlib
import shutil
import signal
import subprocess
import tempfile
import time


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
INSTALL_SCRIPT = REPO_ROOT / "install.sh"


def extract_function(script: pathlib.Path, name: str) -> str:
    lines = script.read_text(encoding="utf-8").splitlines()
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


def run(
    args: list[str],
    *,
    cwd: pathlib.Path,
    env: dict[str, str] | None = None,
    input_text: str | None = None,
) -> subprocess.CompletedProcess[str]:
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    return subprocess.run(
        args,
        cwd=cwd,
        env=merged_env,
        input=input_text,
        text=True,
        capture_output=True,
        check=False,
        timeout=20,
    )


def write_executable(path: pathlib.Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")
    path.chmod(0o755)


def make_complete_checkout(root: pathlib.Path) -> pathlib.Path:
    checkout = root / "non-home" / "checkout"
    sentinels = [
        "CMakeLists.txt",
        "src/main.cpp",
        "src/CMakeLists.txt",
        "libs/prodigy-oaa-protocol/CMakeLists.txt",
        "config/systemd/bluetooth-compat.conf",
        "web-config/server.py",
    ]
    for relative in sentinels:
        path = checkout / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("test sentinel\n", encoding="utf-8")
    shutil.copy2(INSTALL_SCRIPT, checkout / "install.sh")
    subprocess.run(["git", "init", "-q", str(checkout)], check=True)
    return checkout


def assert_source_root_discovery() -> None:
    with tempfile.TemporaryDirectory(prefix="oap-source-root-") as tmp:
        root = pathlib.Path(tmp)
        checkout = make_complete_checkout(root)
        fake_home = root / "home"
        fake_home.mkdir()
        result = run(
            ["bash", "install.sh"],
            cwd=checkout,
            env={
                "HOME": str(fake_home),
                "OAP_INSTALL_TEST_ACTION": "resolve-source",
            },
        )
        if result.returncode != 0:
            raise AssertionError(
                f"non-home checkout discovery failed:\n{result.stdout}\n{result.stderr}"
            )
        expected = f"INSTALL_DIR={checkout.resolve()}"
        if expected not in result.stdout.splitlines():
            raise AssertionError(f"wrong source root; expected {expected!r}: {result.stdout}")


def make_mutation_fakes(root: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path]:
    fake_bin = root / "bin"
    fake_bin.mkdir()
    mutation_log = root / "mutation.log"
    for name in ("sudo", "apt-get", "systemctl", "cmake"):
        write_executable(
            fake_bin / name,
            "#!/usr/bin/env bash\n"
            f"printf '%s\\n' {name!r} >> {str(mutation_log)!r}\n"
            "exit 97\n",
        )
    return fake_bin, mutation_log


def assert_unsupported_source_forms_rejected() -> None:
    script_text = INSTALL_SCRIPT.read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="oap-source-reject-") as tmp:
        root = pathlib.Path(tmp)
        fake_bin, mutation_log = make_mutation_fakes(root)
        env = {"PATH": f"{fake_bin}:{os.environ['PATH']}"}

        copied = root / "copied-install.sh"
        shutil.copy2(INSTALL_SCRIPT, copied)
        copied_result = run(
            ["bash", str(copied), "--mode", "source"], cwd=root, env=env
        )
        stdin_result = run(
            ["bash", "-s", "--", "--mode", "source"],
            cwd=root,
            env=env,
            input_text=script_text,
        )

        for label, result in (("copied", copied_result), ("stdin", stdin_result)):
            if result.returncode == 0:
                raise AssertionError(f"{label} source execution unexpectedly succeeded")
            combined = result.stdout + result.stderr
            for token in ("complete OpenAuto Prodigy git checkout", "git", "bash install.sh"):
                if token not in combined:
                    raise AssertionError(f"{label} rejection is not actionable: {combined}")
        if mutation_log.exists():
            raise AssertionError(
                f"unsupported execution reached mutation commands: {mutation_log.read_text()}"
            )


def assert_initialized_submodule_uses_gitlink() -> None:
    with tempfile.TemporaryDirectory(prefix="oap-initialized-submodule-") as tmp:
        checkout = pathlib.Path(tmp) / "checkout"
        proto = checkout / "libs/prodigy-oaa-protocol/proto"
        proto.mkdir(parents=True)
        (proto / ".git").write_text(
            "gitdir: ../../../../.git/modules/libs/prodigy-oaa-protocol/proto\n",
            encoding="utf-8",
        )
        (proto / "README.md").write_text("initialized submodule\n", encoding="utf-8")
        command_log = pathlib.Path(tmp) / "submodule-command"
        build_project = extract_function(INSTALL_SCRIPT, "build_project")
        shell = f"""
set -u
INSTALL_DIR={checkout!s}
update_step() {{ :; }}
run_with_spinner() {{
    shift
    printf '%s\\0' "$@" > {command_log!s}
    exit 91
}}
{build_project}
build_project
"""
        result = run(["bash", "-c", shell], cwd=checkout)
        if result.returncode != 91:
            raise AssertionError(
                f"build_project did not reach submodule initialization:\n"
                f"{result.stdout}\n{result.stderr}"
            )
        actual = command_log.read_bytes().rstrip(b"\0").split(b"\0")
        expected = [
            b"git",
            b"submodule",
            b"update",
            b"--init",
            b"--recursive",
            b"--depth",
            b"1",
        ]
        if actual != expected:
            raise AssertionError(
                "initialized submodule must be updated through the pinned gitlink; "
                f"got: {[item.decode() for item in actual]}"
            )


def pid_is_running(pid: int) -> bool:
    try:
        stat = pathlib.Path(f"/proc/{pid}/stat").read_text(encoding="utf-8")
    except FileNotFoundError:
        return False
    return stat.split()[2] != "Z"


def wait_for_path(path: pathlib.Path, proc: subprocess.Popen[str]) -> None:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if path.exists():
            return
        if proc.poll() is not None:
            stdout, stderr = proc.communicate()
            raise AssertionError(
                f"installer exited before writing {path.name}: {proc.returncode}\n"
                f"{stdout}\n{stderr}"
            )
        time.sleep(0.02)
    raise AssertionError(f"timed out waiting for {path}")


def wait_for_pid_exit(pid: int) -> None:
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if not pid_is_running(pid):
            return
        time.sleep(0.02)
    raise AssertionError(f"owned process {pid} survived installer cleanup")


def assert_owned_command_statuses() -> None:
    env = os.environ.copy()
    env["OAP_INSTALL_TEST_ACTION"] = "owned-command"
    cases = [(0, 0), (23, 23)]
    for command_status, expected in cases:
        with tempfile.TemporaryDirectory(prefix="oap-owned-status-") as tmp:
            pid_file = pathlib.Path(tmp) / "leader.pid"
            command = f"echo $$ > {pid_file}; exit {command_status}"
            result = run(
                ["bash", str(INSTALL_SCRIPT), "bash", "-c", command],
                cwd=REPO_ROOT,
                env=env,
            )
            if result.returncode != expected:
                raise AssertionError(
                    f"owned status {command_status} became {result.returncode}:\n"
                    f"{result.stdout}\n{result.stderr}"
                )
            wait_for_pid_exit(int(pid_file.read_text().strip()))

    with tempfile.TemporaryDirectory(prefix="oap-owned-normal-cleanup-") as tmp:
        child_file = pathlib.Path(tmp) / "child.pid"
        sentinel = subprocess.Popen(["sleep", "300"], start_new_session=True)
        try:
            command = f"sleep 300 & echo $! > {child_file}; exit 0"
            result = run(
                ["bash", str(INSTALL_SCRIPT), "bash", "-c", command],
                cwd=REPO_ROOT,
                env=env,
            )
            if result.returncode != 0:
                raise AssertionError(
                    f"normal owned-group cleanup failed:\n{result.stdout}\n{result.stderr}"
                )
            wait_for_pid_exit(int(child_file.read_text().strip()))
            if sentinel.poll() is not None:
                raise AssertionError("normal cleanup killed an unrelated process")
        finally:
            sentinel.terminate()
            sentinel.wait(timeout=5)


def assert_signal_cleanup() -> None:
    with tempfile.TemporaryDirectory(prefix="oap-owned-signals-") as tmp:
        root = pathlib.Path(tmp)
        helper = root / "owned-command.sh"
        leader_file = root / "leader.pid"
        child_file = root / "child.pid"
        write_executable(
            helper,
            "#!/usr/bin/env bash\n"
            f"echo $$ > {leader_file!s}\n"
            "sleep 300 &\n"
            f"echo $! > {child_file!s}\n"
            "wait\n",
        )

        for sig, expected in (
            (signal.SIGINT, 130),
            (signal.SIGTERM, 143),
            (signal.SIGHUP, 129),
        ):
            leader_file.unlink(missing_ok=True)
            child_file.unlink(missing_ok=True)
            sentinel = subprocess.Popen(["sleep", "300"], start_new_session=True)
            env = os.environ.copy()
            env["OAP_INSTALL_TEST_ACTION"] = "owned-command"
            proc = subprocess.Popen(
                ["bash", str(INSTALL_SCRIPT), str(helper)],
                cwd=REPO_ROOT,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                start_new_session=True,
            )
            try:
                wait_for_path(child_file, proc)
                proc.send_signal(sig)
                stdout, stderr = proc.communicate(timeout=10)
                if proc.returncode != expected:
                    raise AssertionError(
                        f"signal {sig.name} returned {proc.returncode}, expected {expected}:\n"
                        f"{stdout}\n{stderr}"
                    )
                wait_for_pid_exit(int(leader_file.read_text().strip()))
                wait_for_pid_exit(int(child_file.read_text().strip()))
                if sentinel.poll() is not None:
                    raise AssertionError(f"signal {sig.name} killed an unrelated process")
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait()
                sentinel.terminate()
                sentinel.wait(timeout=5)


def assert_cleanup_idempotence() -> None:
    result = run(
        ["bash", str(INSTALL_SCRIPT), "37"],
        cwd=REPO_ROOT,
        env={"OAP_INSTALL_TEST_ACTION": "cleanup-idempotence"},
    )
    if result.returncode != 37:
        raise AssertionError(f"idempotent cleanup changed status: {result.returncode}")


def make_service_fakes(root: pathlib.Path, initially_active: bool) -> dict[str, str]:
    fake_bin = root / "bin"
    fake_bin.mkdir()
    state = root / "state"
    state.write_text("active" if initially_active else "inactive", encoding="utf-8")
    events = root / "events"
    write_executable(fake_bin / "sudo", "#!/usr/bin/env bash\nexec \"$@\"\n")
    write_executable(
        fake_bin / "systemctl",
        "#!/usr/bin/env bash\n"
        "case \"$1\" in\n"
        "  is-active) [[ $(cat \"$OAP_TEST_SERVICE_STATE\") == active ]] ; exit $? ;;\n"
        "  stop) echo stop >> \"$OAP_TEST_EVENTS\"; echo inactive > \"$OAP_TEST_SERVICE_STATE\" ;;\n"
        "  start) echo start >> \"$OAP_TEST_EVENTS\"; echo active > \"$OAP_TEST_SERVICE_STATE\" ;;\n"
        "  *) exit 98 ;;\n"
        "esac\n",
    )
    return {
        "PATH": f"{fake_bin}:{os.environ['PATH']}",
        "OAP_TEST_SERVICE_STATE": str(state),
        "OAP_TEST_EVENTS": str(events),
    }


def assert_service_state_restoration() -> None:
    for initially_active in (True, False):
        for command_status in (0, 31):
            with tempfile.TemporaryDirectory(prefix="oap-service-lifecycle-") as tmp:
                root = pathlib.Path(tmp)
                env = make_service_fakes(root, initially_active)
                env["OAP_INSTALL_TEST_ACTION"] = "service-rebuild"
                events = pathlib.Path(env["OAP_TEST_EVENTS"])
                mutation = f"echo mutate >> {events}; exit {command_status}"
                result = run(
                    ["bash", str(INSTALL_SCRIPT), "bash", "-c", mutation],
                    cwd=REPO_ROOT,
                    env=env,
                )
                if result.returncode != command_status:
                    raise AssertionError(
                        f"service case returned {result.returncode}, expected {command_status}:\n"
                        f"{result.stdout}\n{result.stderr}"
                    )
                actual = events.read_text().splitlines()
                expected = ["stop", "mutate", "start"] if initially_active else ["mutate"]
                if actual != expected:
                    raise AssertionError(
                        f"service lifecycle active={initially_active} status={command_status}: "
                        f"expected {expected}, got {actual}"
                    )

    with tempfile.TemporaryDirectory(prefix="oap-service-skip-") as tmp:
        root = pathlib.Path(tmp)
        env = make_service_fakes(root, True)
        env["OAP_INSTALL_TEST_ACTION"] = "service-skip"
        result = run(["bash", str(INSTALL_SCRIPT)], cwd=REPO_ROOT, env=env)
        if result.returncode != 0:
            raise AssertionError(f"skip lifecycle failed: {result.stderr}")
        if pathlib.Path(env["OAP_TEST_EVENTS"]).exists():
            raise AssertionError("skipped rebuild stopped or started the active service")


def main() -> int:
    assert_source_root_discovery()
    assert_unsupported_source_forms_rejected()
    assert_initialized_submodule_uses_gitlink()
    assert_owned_command_statuses()
    assert_signal_cleanup()
    assert_cleanup_idempotence()
    assert_service_state_restoration()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
