import asyncio
import contextlib
import sys
import types
import unittest
from unittest.mock import AsyncMock, MagicMock, patch

import openauto_system
from openauto_system import parse_set_proxy_route_request


class OpenAutoSystemRouteValidationTests(unittest.TestCase):
    def test_disable_request_normalizes(self):
        result = parse_set_proxy_route_request({"active": False})
        self.assertEqual(result, {"active": False})

    def test_active_request_parses_and_defaults_user(self):
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.10",
            "port": 1080,
            "password": "deadbeef",
        })
        self.assertTrue(result["active"])
        self.assertEqual(result["host"], "10.0.0.10")
        self.assertEqual(result["port"], 1080)
        self.assertEqual(result["user"], "oap")
        self.assertEqual(result["skip_interfaces"], ["eth0", "lo"])
        self.assertEqual(result["skip_tcp_ports"], [22])
        self.assertEqual(result["skip_networks"], [])

    def test_active_request_parses_custom_route_exceptions(self):
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.10",
            "port": 1080,
            "user": "oap",
            "password": "deadbeef",
            "skip_interfaces": ["wlan0"],
            "skip_tcp_ports": [22, 443],
            "skip_networks": ["172.16.0.0/12", "192.168.0.0/16"],
        })
        self.assertEqual(result["skip_interfaces"], ["wlan0"])
        self.assertEqual(result["skip_tcp_ports"], [22, 443])
        self.assertEqual(result["skip_networks"], ["172.16.0.0/12", "192.168.0.0/16"])

    def test_missing_host_raises(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "port": 1080,
                "password": "deadbeef",
            })

    def test_missing_password_raises(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "host": "10.0.0.10",
                "port": 1080,
            })

    def test_invalid_port_raises(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "host": "10.0.0.10",
                "port": 70000,
                "password": "deadbeef",
            })

    def test_invalid_active_type_raises(self):
        with self.assertRaises(TypeError):
            parse_set_proxy_route_request({
                "active": 1,
                "host": "10.0.0.10",
                "port": 1080,
                "password": "deadbeef",
            })

    def test_invalid_skip_interfaces_rejected(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "host": "10.0.0.10",
                "port": 1080,
                "password": "deadbeef",
                "skip_interfaces": "eth0",
            })

    def test_invalid_skip_tcp_ports_rejected(self):
        with self.assertRaises(ValueError):
            parse_set_proxy_route_request({
                "active": True,
                "host": "10.0.0.10",
                "port": 1080,
                "password": "deadbeef",
                "skip_tcp_ports": [0],
            })


class _ImmediateStopEvent:
    def set(self):
        return None

    async def wait(self):
        return None


class _LoopStub:
    def add_signal_handler(self, signum, callback):
        _ = signum
        _ = callback
        return None


class OpenAutoSystemShutdownTests(unittest.IsolatedAsyncioTestCase):
    async def _run_main(
        self,
        *,
        proxy_state: str,
        wait_for_side_effect=None,
        health_run_side_effect=None,
    ):
        ipc = MagicMock()
        ipc.register_method = MagicMock()
        ipc.start = AsyncMock()
        ipc.stop = AsyncMock()

        health = MagicMock()
        health.set_bt_restart_callback = MagicMock()
        health.check_once = AsyncMock()
        health.get_health = MagicMock(return_value={})
        if health_run_side_effect is None:
            async def health_run():
                await asyncio.sleep(3600)
            health.run = health_run
        else:
            health.run = health_run_side_effect

        config = MagicMock()
        config.apply_section = MagicMock(return_value={"ok": True, "restarted": []})

        proxy = MagicMock()
        proxy.state = types.SimpleNamespace(value=proxy_state)
        proxy.enable = AsyncMock()
        proxy.disable = AsyncMock()

        bt = MagicMock()
        bt.register_all = AsyncMock()
        bt.close = AsyncMock()

        fake_bt_module = types.ModuleType("bt_profiles")

        class _FakeBtProfileManager:
            async def register_all(self):
                await bt.register_all()

            async def close(self):
                await bt.close()

        fake_bt_module.BtProfileManager = _FakeBtProfileManager

        created_tasks = []
        real_create_task = asyncio.create_task

        def tracked_create_task(coro, *args, **kwargs):
            task = real_create_task(coro, *args, **kwargs)
            created_tasks.append(task)
            return task

        with contextlib.ExitStack() as stack:
            stack.enter_context(
                patch.dict(sys.modules, {"bt_profiles": fake_bt_module})
            )
            stack.enter_context(
                patch.object(openauto_system, "_notify_systemd_ready", return_value=None)
            )
            stack.enter_context(patch.object(openauto_system, "IpcServer", return_value=ipc))
            stack.enter_context(
                patch.object(openauto_system, "HealthMonitor", return_value=health)
            )
            stack.enter_context(
                patch.object(openauto_system, "ConfigApplier", return_value=config)
            )
            stack.enter_context(
                patch.object(openauto_system, "ProxyManager", return_value=proxy)
            )
            stack.enter_context(patch("openauto_system.asyncio.Event", _ImmediateStopEvent))
            stack.enter_context(
                patch("openauto_system.asyncio.get_running_loop", return_value=_LoopStub())
            )
            stack.enter_context(
                patch(
                    "openauto_system.asyncio.create_task",
                    side_effect=tracked_create_task,
                )
            )
            if wait_for_side_effect is not None:
                stack.enter_context(
                    patch(
                        "openauto_system.asyncio.wait_for",
                        new=AsyncMock(side_effect=wait_for_side_effect),
                    )
                )
            await openauto_system.main()

        if created_tasks:
            for task in created_tasks:
                if not task.done():
                    task.cancel()
            await asyncio.gather(*created_tasks, return_exceptions=True)

        return ipc, proxy, bt

    async def test_shutdown_proxy_disable_timeout_logs_warning_and_continues(self):
        async def wait_for_with_proxy_timeout(awaitable, timeout):
            if timeout == 5.0:
                if asyncio.iscoroutine(awaitable):
                    awaitable.close()
                raise asyncio.TimeoutError()
            return await awaitable

        with self.assertLogs("openauto-system", level="WARNING") as captured:
            ipc, proxy, bt = await self._run_main(
                proxy_state="active",
                wait_for_side_effect=wait_for_with_proxy_timeout,
            )

        proxy.disable.assert_called_once()
        bt.close.assert_awaited_once()
        ipc.stop.assert_awaited_once()
        self.assertTrue(
            any("proxy disable timed out" in message.lower() for message in captured.output)
        )

    async def test_shutdown_ipc_stop_timeout_logs_warning(self):
        async def wait_for_with_ipc_timeout(awaitable, timeout):
            if timeout == 3.0:
                if asyncio.iscoroutine(awaitable):
                    awaitable.close()
                raise asyncio.TimeoutError()
            return await awaitable

        with self.assertLogs("openauto-system", level="WARNING") as captured:
            ipc, proxy, bt = await self._run_main(
                proxy_state="disabled",
                wait_for_side_effect=wait_for_with_ipc_timeout,
            )

        proxy.disable.assert_not_called()
        bt.close.assert_awaited_once()
        ipc.stop.assert_called_once()
        self.assertTrue(
            any("ipc stop timed out" in message.lower() for message in captured.output)
        )

    async def test_shutdown_overall_timeout_logs_forced_shutdown(self):
        async def wait_for_with_forced_overall_timeout(awaitable, timeout):
            if timeout == 10.0:
                if asyncio.iscoroutine(awaitable):
                    awaitable.close()
                raise asyncio.TimeoutError()
            return await awaitable

        with self.assertLogs("openauto-system", level="ERROR") as captured:
            await self._run_main(
                proxy_state="disabled",
                wait_for_side_effect=wait_for_with_forced_overall_timeout,
            )

        self.assertTrue(
            any("forced shutdown" in message.lower() for message in captured.output)
        )
