import asyncio
import os
import sys
import unittest
from unittest.mock import AsyncMock, MagicMock, patch

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from proxy_manager import ProxyManager, ProxyState


class ProxyManagerTests(unittest.IsolatedAsyncioTestCase):

    def _mock_healthy_writer(self):
        reader = MagicMock()
        writer = MagicMock()
        writer.wait_closed = AsyncMock()
        return reader, writer

    def _mock_proc(self):
        proc = MagicMock()
        proc.wait = AsyncMock(return_value=0)
        proc.terminate = MagicMock()
        proc.kill = MagicMock()
        return proc

    async def _run_enable_with_mocks(self, pm: ProxyManager, host="127.0.0.2", port=1080):
        proc = self._mock_proc()
        pm._run_cmd = AsyncMock(return_value=(0, ""))
        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
            await pm.enable(host, port, "user", "pass")
        return proc

    async def test_initial_state_is_disabled(self):
        pm = ProxyManager()
        self.assertEqual(pm.state, ProxyState.DISABLED)

    async def test_enable_sets_active_state(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)

        self.assertEqual(pm.state, ProxyState.ACTIVE)

        if pm._health_task is not None:
            pm._health_task.cancel()
            try:
                await pm._health_task
            except asyncio.CancelledError:
                pass

    async def test_disable_sets_disabled_state(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._set_state(ProxyState.ACTIVE)
        pm._run_cmd = AsyncMock(return_value=(0, ""))
        pm._health_task = asyncio.create_task(asyncio.sleep(999))

        await pm.disable()
        self.assertEqual(pm.state, ProxyState.DISABLED)

    async def test_enable_writes_redsocks_config(self):
        config_path = "/tmp/test_redsocks.conf"
        try:
            os.remove(config_path)
        except FileNotFoundError:
            pass

        pm = ProxyManager(config_path=config_path)
        await self._run_enable_with_mocks(pm, host="192.168.10.1", port=1088)

        with open(config_path, "r", encoding="utf-8") as f:
            data = f.read()

        self.assertIn("192.168.10.1", data)
        self.assertIn("1088", data)
        self.assertIn("user", data)
        self.assertIn("pass", data)
        self.assertIn("12345", data)

        if pm._health_task is not None:
            pm._health_task.cancel()
            try:
                await pm._health_task
            except asyncio.CancelledError:
                pass

    async def test_enable_applies_iptables(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)

        args = [call.args for call in pm._run_cmd.await_args_list]
        self.assertTrue(any("REDIRECT" in cmd for call in args for cmd in call))
        self.assertTrue(any("10.0.0.0/24" in cmd for call in args for cmd in call))

        if pm._health_task is not None:
            pm._health_task.cancel()
            try:
                await pm._health_task
            except asyncio.CancelledError:
                pass

    async def test_disable_flushes_iptables(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)
        await pm.disable()

        args = [call.args for call in pm._run_cmd.await_args_list]
        self.assertTrue(
            any("-D" in cmd for call in args for cmd in call)
            or any("-F" in cmd for call in args for cmd in call)
        )

    async def test_double_enable_stops_old_process(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._run_cmd = AsyncMock(return_value=(0, ""))
        old_proc = self._mock_proc()
        new_proc = self._mock_proc()

        with patch("asyncio.create_subprocess_exec", AsyncMock(side_effect=[old_proc, new_proc])):
            await pm.enable("10.0.0.5", 1080, "oap", "deadbeef")
            await pm.enable("10.0.0.6", 1080, "oap", "cafebabe")

        old_proc.terminate.assert_called_once()

    async def test_disable_clears_redsocks_proc(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._set_state(ProxyState.ACTIVE)
        proc = self._mock_proc()
        pm._redsocks_proc = proc
        pm._run_cmd = AsyncMock(return_value=(0, ""))

        await pm.disable()
        self.assertIsNone(pm._redsocks_proc)

    async def test_disable_on_fresh_instance_is_safe(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._run_cmd = AsyncMock(return_value=(0, ""))

        await pm.disable()
        self.assertEqual(pm.state, ProxyState.DISABLED)

    async def test_disable_kills_redsocks(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._set_state(ProxyState.ACTIVE)
        proc = self._mock_proc()
        pm._redsocks_proc = proc
        pm._run_cmd = AsyncMock(return_value=(0, ""))

        await pm.disable()

        proc.terminate.assert_called_once()

    async def test_health_check_marks_degraded_after_failures(self):
        pm = ProxyManager()
        pm._set_state(ProxyState.ACTIVE)
        pm._proxy_host = "1.2.3.4"
        pm._proxy_port = 1080

        with patch("asyncio.open_connection", AsyncMock(side_effect=OSError("fail"))):
            await pm._probe_once()
            await pm._probe_once()
            await pm._probe_once()

        self.assertEqual(pm.state, ProxyState.DEGRADED)

    async def test_health_check_restores_active_after_success(self):
        pm = ProxyManager()
        pm._set_state(ProxyState.DEGRADED)
        pm._fail_count = 3
        pm._proxy_host = "1.2.3.4"
        pm._proxy_port = 1080

        reader, writer = self._mock_healthy_writer()
        with patch("asyncio.open_connection", AsyncMock(return_value=(reader, writer))):
            await pm._probe_once()

        self.assertEqual(pm.state, ProxyState.ACTIVE)
        self.assertEqual(pm._fail_count, 0)
