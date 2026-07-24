#!/usr/bin/env python3
"""Production-seam tests for scripts/validate-resolutions.sh."""

from __future__ import annotations

import os
from pathlib import Path
import signal
import subprocess
import tempfile
import textwrap
import time
import unittest


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT = REPO_ROOT / "scripts" / "validate-resolutions.sh"


class ValidateResolutionsTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory(prefix="oap-resolution-test-")
        self.root = Path(self.temp_dir.name)
        self.bin_dir = self.root / "bin"
        self.bin_dir.mkdir()
        self.output_dir = self.root / "output"
        self.lock_file = self.root / ".X99-lock"
        self.xvfb_log = self.root / "xvfb.log"
        self.binary_log = self.root / "binary.log"
        self.renderer_log = self.root / "renderer.log"
        self._processes: list[subprocess.Popen[str]] = []

        self._write_executable(
            "Xvfb",
            """#!/usr/bin/env python3
import os
from pathlib import Path
import signal
import sys
import time

lock = Path(os.environ["OAP_VALIDATE_XVFB_LOCK_FILE"])
log = Path(os.environ["OAP_TEST_XVFB_LOG"])
pid = os.getpid()
lock.write_text(f"{pid}\\n", encoding="utf-8")
with log.open("a", encoding="utf-8") as stream:
    stream.write(f"start {pid} {' '.join(sys.argv[1:])}\\n")

def stop(signum, _frame):
    with log.open("a", encoding="utf-8") as stream:
        stream.write(f"signal {pid} {signum}\\n")
    if os.environ.get("OAP_TEST_XVFB_KEEP_LOCK") != "1":
        try:
            if lock.read_text(encoding="utf-8").strip() == str(pid):
                lock.unlink()
        except FileNotFoundError:
            pass
    raise SystemExit(128 + signum)

for caught in (signal.SIGHUP, signal.SIGINT, signal.SIGTERM):
    signal.signal(caught, stop)
while True:
    time.sleep(1)
""",
        )
        self._write_executable(
            "openauto-prodigy",
            """#!/usr/bin/env python3
import os
from pathlib import Path
import sys
import time

with Path(os.environ["OAP_TEST_BINARY_LOG"]).open("a", encoding="utf-8") as stream:
    stream.write(" ".join(sys.argv[1:]) + "\\n")
while True:
    time.sleep(1)
""",
        )
        self._write_executable(
            "xwd",
            """#!/usr/bin/env python3
import os
from pathlib import Path
import sys

with Path(os.environ["OAP_TEST_RENDERER_LOG"]).open("a", encoding="utf-8") as stream:
    stream.write("xwd " + " ".join(sys.argv[1:]) + "\\n")
sys.stdout.buffer.write(b"fake-xwd")
""",
        )
        self._write_executable(
            "convert",
            """#!/usr/bin/env python3
import os
from pathlib import Path
import sys

with Path(os.environ["OAP_TEST_RENDERER_LOG"]).open("a", encoding="utf-8") as stream:
    stream.write("convert " + " ".join(sys.argv[1:]) + "\\n")
sys.stdin.buffer.read()
if os.environ.get("OAP_TEST_LOCK_DIR_READ_ONLY") == "1":
    Path(os.environ["OAP_VALIDATE_XVFB_LOCK_FILE"]).parent.chmod(0o555)
if os.environ.get("OAP_TEST_CONVERT_FAIL") == "1":
    raise SystemExit(9)
Path(sys.argv[-1]).touch()
""",
        )
        self._write_executable("x11vnc", "#!/bin/sh\nexec /bin/sleep 60\n")
        self._write_executable("sleep", "#!/bin/sh\nexec /bin/sleep 0.01\n")

        self.env = os.environ.copy()
        self.env.update(
            {
                "PATH": f"{self.bin_dir}:{self.env['PATH']}",
                "OAP_VALIDATE_XVFB_LOCK_FILE": str(self.lock_file),
                "OAP_TEST_XVFB_LOG": str(self.xvfb_log),
                "OAP_TEST_BINARY_LOG": str(self.binary_log),
                "OAP_TEST_RENDERER_LOG": str(self.renderer_log),
            }
        )

    def tearDown(self) -> None:
        for process in self._processes:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=2)
        self.temp_dir.cleanup()

    def _write_executable(self, name: str, contents: str) -> Path:
        path = self.bin_dir / name
        path.write_text(textwrap.dedent(contents), encoding="utf-8")
        path.chmod(0o755)
        return path

    def _command(self, *, single: bool = True, mode: str | None = "--screenshot") -> list[str]:
        command = [
            "bash",
            str(SCRIPT),
        ]
        if mode is not None:
            command.append(mode)
        command.extend([
            "--binary",
            str(self.bin_dir / "openauto-prodigy"),
            "--output",
            str(self.output_dir),
        ])
        if single:
            command.extend(["--resolution", "800x480"])
        return command

    def _run(self, *, single: bool = True, extra_env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
        env = self.env.copy()
        if extra_env:
            env.update(extra_env)
        return subprocess.run(
            self._command(single=single),
            cwd=REPO_ROOT,
            env=env,
            text=True,
            capture_output=True,
            timeout=8,
            check=False,
        )

    def _start_sentinel(self) -> subprocess.Popen[str]:
        process = subprocess.Popen(
            ["/bin/sleep", "60"],
            text=True,
            start_new_session=True,
        )
        self._processes.append(process)
        return process

    def _wait_for_owned_xvfb(self, process: subprocess.Popen[str]) -> int:
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if process.poll() is not None:
                self.fail(f"validator exited before Xvfb started: {process.returncode}")
            if self.xvfb_log.exists():
                for line in self.xvfb_log.read_text(encoding="utf-8").splitlines():
                    if line.startswith("start "):
                        return int(line.split()[1])
            time.sleep(0.01)
        self.fail("timed out waiting for the owned Xvfb child")

    def assert_pid_gone(self, pid: int) -> None:
        self.assertFalse(Path(f"/proc/{pid}").exists(), f"PID {pid} still exists")

    def test_absent_lock_preserves_resolution_matrix_and_renderer_contract(self) -> None:
        result = self._run(single=False)
        self.assertEqual(result.returncode, 0, result.stderr)

        geometries = self.binary_log.read_text(encoding="utf-8").splitlines()
        self.assertEqual(
            geometries,
            [
                "--geometry 800x480",
                "--geometry 1024x600",
                "--geometry 1280x720",
                "--geometry 1920x480",
                "--geometry 480x800",
                "--geometry 480x272",
            ],
        )
        renderer = self.renderer_log.read_text(encoding="utf-8")
        self.assertIn("xwd -display :99 -root -silent", renderer)
        self.assertIn("convert xwd:-", renderer)
        self.assertEqual(len(list(self.output_dir.glob("test-*.png"))), 6)
        self.assertFalse(self.lock_file.exists())
        for line in self.xvfb_log.read_text(encoding="utf-8").splitlines():
            if line.startswith("start "):
                self.assert_pid_gone(int(line.split()[1]))

    def test_malformed_and_stale_locks_are_removed_without_touching_sentinel(self) -> None:
        sentinel = self._start_sentinel()
        for contents in ("not-a-pid\n", "2147483647\n"):
            with self.subTest(contents=contents.strip()):
                self.lock_file.write_text(contents, encoding="utf-8")
                result = self._run()
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIsNone(sentinel.poll())
                self.assertFalse(self.lock_file.exists())

    def test_live_unrelated_pid_is_refused_without_signal_or_lock_change(self) -> None:
        sentinel = self._start_sentinel()
        contents = f"  {sentinel.pid}\n"
        self.lock_file.write_text(contents, encoding="utf-8")

        started = time.monotonic()
        result = self._run()

        self.assertNotEqual(result.returncode, 0)
        self.assertLess(time.monotonic() - started, 2)
        self.assertIn("occupied by live PID", result.stderr)
        self.assertIsNone(sentinel.poll())
        self.assertEqual(self.lock_file.read_text(encoding="utf-8"), contents)
        self.assertFalse(self.xvfb_log.exists())

    def test_live_xvfb_pid_is_also_refused_without_signal(self) -> None:
        existing = subprocess.Popen(
            [str(self.bin_dir / "Xvfb"), ":99", "-screen", "0", "800x480x24"],
            env=self.env,
            text=True,
            start_new_session=True,
        )
        self._processes.append(existing)
        deadline = time.monotonic() + 2
        while not self.lock_file.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertTrue(self.lock_file.exists())
        contents = self.lock_file.read_text(encoding="utf-8")

        result = self._run()

        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(existing.poll())
        self.assertEqual(self.lock_file.read_text(encoding="utf-8"), contents)
        self.assertNotIn("signal", self.xvfb_log.read_text(encoding="utf-8"))

    def test_owned_xvfb_is_reaped_on_renderer_failure(self) -> None:
        sentinel = self._start_sentinel()
        result = self._run(extra_env={"OAP_TEST_CONVERT_FAIL": "1"})

        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(sentinel.poll())
        started_pid = int(self.xvfb_log.read_text(encoding="utf-8").split()[1])
        self.assert_pid_gone(started_pid)
        self.assertFalse(self.lock_file.exists())

    def test_unremovable_owned_lock_does_not_override_failure_status(self) -> None:
        try:
            result = self._run(
                extra_env={
                    "OAP_TEST_CONVERT_FAIL": "1",
                    "OAP_TEST_LOCK_DIR_READ_ONLY": "1",
                    "OAP_TEST_XVFB_KEEP_LOCK": "1",
                }
            )
        finally:
            self.root.chmod(0o755)

        self.assertEqual(result.returncode, 9, result.stderr)
        self.assertTrue(self.lock_file.exists())

    def test_signals_reap_only_the_owned_xvfb_child(self) -> None:
        for caught_signal in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            with self.subTest(signal=caught_signal):
                self.xvfb_log.unlink(missing_ok=True)
                self.lock_file.unlink(missing_ok=True)
                sentinel = self._start_sentinel()
                process = subprocess.Popen(
                    self._command(mode=None),
                    cwd=REPO_ROOT,
                    env=self.env,
                    text=True,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    start_new_session=True,
                )
                owned_pid = self._wait_for_owned_xvfb(process)

                os.kill(process.pid, caught_signal)
                stdout, stderr = process.communicate(timeout=5)

                self.assertEqual(process.returncode, -caught_signal, stdout + stderr)
                self.assert_pid_gone(owned_pid)
                self.assertIsNone(sentinel.poll())
                self.assertFalse(self.lock_file.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
