import asyncio
import contextlib
import os
import sys
import types
import unittest
from unittest.mock import AsyncMock, MagicMock, patch

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import openauto_system
from openauto_system import parse_set_proxy_route_request
from proxy_manager import ProxyManager, ProxyState


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
        self.assertEqual(result["skip_interfaces"], ["lo"])
        self.assertNotIn("skip_tcp_ports", result)
        self.assertEqual(
            result["skip_networks"],
            ["127.0.0.0/8", "169.254.0.0/16", "10.0.0.0/24", "192.168.0.0/16"],
        )

    def test_active_request_parses_custom_route_exceptions(self):
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.10",
            "port": 1080,
            "user": "oap",
            "password": "deadbeef",
            "skip_interfaces": ["wlan0"],
            "skip_networks": ["172.16.0.0/12", "192.168.0.0/16"],
        })
        self.assertEqual(result["skip_interfaces"], ["wlan0"])
        self.assertNotIn("skip_tcp_ports", result)
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

    def test_skip_tcp_ports_not_in_output(self):
        """skip_tcp_ports is no longer a valid field in parsed output."""
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.10",
            "port": 1080,
            "password": "deadbeef",
            "skip_tcp_ports": [22, 443],  # ignored by parser
        })
        self.assertNotIn("skip_tcp_ports", result)

    def test_default_skip_networks_are_four_subnets(self):
        """Default skip_networks includes exactly four subnets."""
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.10",
            "port": 1080,
            "password": "deadbeef",
        })
        self.assertEqual(
            result["skip_networks"],
            ["127.0.0.0/8", "169.254.0.0/16", "10.0.0.0/24", "192.168.0.0/16"],
        )

    def test_default_skip_interfaces_are_lo_only(self):
        """Default skip_interfaces is lo only — eth0 exemption bypasses all REDIRECT."""
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.10",
            "port": 1080,
            "password": "deadbeef",
        })
        self.assertEqual(result["skip_interfaces"], ["lo"])

    def test_ipv6_mapped_host_prefix_stripped(self):
        """Qt dual-stack sockets send ::ffff:x.x.x.x — must strip to plain IPv4."""
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "::ffff:10.0.0.31",
            "port": 1080,
            "password": "deadbeef",
        })
        self.assertEqual(result["host"], "10.0.0.31")

    def test_plain_ipv4_host_unchanged(self):
        """Plain IPv4 host passes through unmodified."""
        result = parse_set_proxy_route_request({
            "active": True,
            "host": "10.0.0.31",
            "port": 1080,
            "password": "deadbeef",
        })
        self.assertEqual(result["host"], "10.0.0.31")


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
        proxy.cleanup_stale_state = AsyncMock()

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

    async def test_startup_cleanup_stale_state_before_ipc_start(self):
        """IC-03: cleanup_stale_state() is called BEFORE ipc.start()."""
        call_order = []

        ipc = MagicMock()
        ipc.register_method = MagicMock()

        async def track_ipc_start():
            call_order.append("ipc.start")

        ipc.start = track_ipc_start
        ipc.stop = AsyncMock()

        health = MagicMock()
        health.set_bt_restart_callback = MagicMock()
        health.check_once = AsyncMock()
        health.get_health = MagicMock(return_value={})

        async def health_run():
            await asyncio.sleep(3600)

        health.run = health_run

        config = MagicMock()
        config.apply_section = MagicMock(return_value={"ok": True, "restarted": []})

        proxy = MagicMock()
        proxy.state = types.SimpleNamespace(value="disabled")
        proxy.enable = AsyncMock()
        proxy.disable = AsyncMock()

        async def track_cleanup():
            call_order.append("cleanup_stale_state")

        proxy.cleanup_stale_state = track_cleanup

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
            await openauto_system.main()

        if created_tasks:
            for task in created_tasks:
                if not task.done():
                    task.cancel()
            await asyncio.gather(*created_tasks, return_exceptions=True)

        # Verify ordering: cleanup_stale_state BEFORE ipc.start
        self.assertIn("cleanup_stale_state", call_order)
        self.assertIn("ipc.start", call_order)
        cleanup_idx = call_order.index("cleanup_stale_state")
        start_idx = call_order.index("ipc.start")
        self.assertLess(
            cleanup_idx,
            start_idx,
            f"cleanup_stale_state must be called before ipc.start, got order: {call_order}",
        )


def _make_status_dict(state="disabled", listener=False, iptables=False, upstream=False,
                       error_code=None, error=None, verified_at=None, live_check=False):
    """Helper to build a full status response dict."""
    return {
        "state": state,
        "checks": {"listener": listener, "iptables": iptables, "upstream": upstream},
        "error_code": error_code,
        "error": error,
        "verified_at": verified_at,
        "live_check": live_check,
    }


class OpenAutoSystemIPCHandlerTests(unittest.IsolatedAsyncioTestCase):
    """Test the IPC handlers defined inside main() by extracting them via register_method spy."""

    async def _extract_handlers(self):
        """Run main() with spied register_method to capture handler closures."""
        handlers = {}

        ipc = MagicMock()
        ipc.start = AsyncMock()
        ipc.stop = AsyncMock()

        def capture_register(name, handler):
            handlers[name] = handler

        ipc.register_method = capture_register

        health = MagicMock()
        health.set_bt_restart_callback = MagicMock()
        health.check_once = AsyncMock()
        health.get_health = MagicMock(return_value={})

        async def health_run():
            await asyncio.sleep(3600)

        health.run = health_run

        config = MagicMock()
        config.apply_section = MagicMock(return_value={"ok": True, "restarted": []})

        self._proxy = MagicMock()
        self._proxy.state = ProxyState.DISABLED
        self._proxy.enable = AsyncMock()
        self._proxy.disable = AsyncMock()
        self._proxy.cleanup_stale_state = AsyncMock()
        self._proxy.get_status = AsyncMock(return_value=_make_status_dict())

        fake_bt_module = types.ModuleType("bt_profiles")

        class _FakeBtProfileManager:
            async def register_all(self):
                pass

            async def close(self):
                pass

        fake_bt_module.BtProfileManager = _FakeBtProfileManager

        created_tasks = []
        real_create_task = asyncio.create_task

        def tracked_create_task(coro, *args, **kwargs):
            task = real_create_task(coro, *args, **kwargs)
            created_tasks.append(task)
            return task

        with contextlib.ExitStack() as stack:
            stack.enter_context(patch.dict(sys.modules, {"bt_profiles": fake_bt_module}))
            stack.enter_context(patch.object(openauto_system, "_notify_systemd_ready", return_value=None))
            stack.enter_context(patch.object(openauto_system, "IpcServer", return_value=ipc))
            stack.enter_context(patch.object(openauto_system, "HealthMonitor", return_value=health))
            stack.enter_context(patch.object(openauto_system, "ConfigApplier", return_value=config))
            stack.enter_context(patch.object(openauto_system, "ProxyManager", return_value=self._proxy))
            stack.enter_context(patch("openauto_system.asyncio.Event", _ImmediateStopEvent))
            stack.enter_context(patch("openauto_system.asyncio.get_running_loop", return_value=_LoopStub()))
            stack.enter_context(patch("openauto_system.asyncio.create_task", side_effect=tracked_create_task))
            await openauto_system.main()

        if created_tasks:
            for task in created_tasks:
                if not task.done():
                    task.cancel()
            await asyncio.gather(*created_tasks, return_exceptions=True)

        return handlers

    # -------------------------------------------------------
    # get_proxy_status handler
    # -------------------------------------------------------

    async def test_get_proxy_status_returns_full_shape(self):
        """get_proxy_status returns the full status response shape."""
        handlers = await self._extract_handlers()
        handler = handlers["get_proxy_status"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(
            state="active", listener=True, iptables=True, upstream=True,
            verified_at="2026-03-16T04:00:00Z",
        ))

        result = await handler(None)

        self.assertEqual(result["state"], "active")
        self.assertIn("checks", result)
        self.assertIn("error_code", result)
        self.assertIn("verified_at", result)
        self.assertIn("live_check", result)

    async def test_get_proxy_status_passes_verify_true(self):
        """get_proxy_status passes verify=True when requested."""
        handlers = await self._extract_handlers()
        handler = handlers["get_proxy_status"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(live_check=True))

        result = await handler({"verify": True})

        self._proxy.get_status.assert_awaited_once_with(verify=True)

    async def test_get_proxy_status_rejects_non_bool_verify(self):
        """get_proxy_status returns error when verify is not a boolean."""
        handlers = await self._extract_handlers()
        handler = handlers["get_proxy_status"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict())

        result = await handler({"verify": "true"})

        self.assertEqual(result["error_code"], "invalid_request")
        self.assertIn("boolean", result["error"])

    async def test_get_proxy_status_no_params(self):
        """get_proxy_status works with None params (no verify)."""
        handlers = await self._extract_handlers()
        handler = handlers["get_proxy_status"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict())

        result = await handler(None)

        self._proxy.get_status.assert_awaited_once_with(verify=False)

    # -------------------------------------------------------
    # set_proxy_route handler — full response shape
    # -------------------------------------------------------

    async def test_set_proxy_route_enable_returns_full_shape(self):
        """set_proxy_route returns full status shape after enable."""
        handlers = await self._extract_handlers()
        handler = handlers["set_proxy_route"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(
            state="active", listener=True, iptables=True, upstream=True,
            verified_at="2026-03-16T04:00:00Z",
        ))

        result = await handler({
            "active": True,
            "host": "10.0.0.5",
            "port": 1080,
            "password": "deadbeef",
        })

        self.assertTrue(result["ok"])
        self.assertEqual(result["state"], "active")
        self.assertIn("checks", result)
        self.assertIn("verified_at", result)

    async def test_set_proxy_route_disable_returns_full_shape(self):
        """set_proxy_route returns full status shape after disable."""
        handlers = await self._extract_handlers()
        handler = handlers["set_proxy_route"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict())

        result = await handler({"active": False})

        self.assertTrue(result["ok"])
        self.assertEqual(result["state"], "disabled")
        self.assertIn("checks", result)

    async def test_set_proxy_route_failed_returns_full_shape(self):
        """set_proxy_route returns full status shape even when state is FAILED."""
        handlers = await self._extract_handlers()
        handler = handlers["set_proxy_route"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(
            state="failed", error_code="listener_down",
            error="redsocks is not listening",
        ))

        result = await handler({"active": False})

        self.assertFalse(result["ok"])
        self.assertEqual(result["state"], "failed")
        self.assertIn("checks", result)

    async def test_set_proxy_route_exception_returns_full_shape(self):
        """set_proxy_route returns full status shape even on exception."""
        handlers = await self._extract_handlers()
        handler = handlers["set_proxy_route"]

        self._proxy.enable = AsyncMock(side_effect=RuntimeError("permission denied"))
        self._proxy.state = ProxyState.FAILED
        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(
            state="failed", error_code="internal_error",
        ))

        result = await handler({
            "active": True,
            "host": "10.0.0.5",
            "port": 1080,
            "password": "deadbeef",
        })

        self.assertFalse(result["ok"])
        self.assertIn("permission denied", result["error"])
        self.assertIn("checks", result)

    async def test_set_proxy_route_exception_fallback_when_get_status_fails(self):
        """set_proxy_route returns synthetic fallback when both enable and get_status fail."""
        handlers = await self._extract_handlers()
        handler = handlers["set_proxy_route"]

        self._proxy.enable = AsyncMock(side_effect=RuntimeError("kaboom"))
        self._proxy.state = ProxyState.FAILED
        self._proxy.get_status = AsyncMock(side_effect=RuntimeError("double fault"))

        result = await handler({
            "active": True,
            "host": "10.0.0.5",
            "port": 1080,
            "password": "deadbeef",
        })

        self.assertFalse(result["ok"])
        self.assertEqual(result["state"], "failed")
        self.assertEqual(result["error_code"], "internal_error")
        self.assertIsNone(result["checks"])  # synthetic fallback

    # -------------------------------------------------------
    # get_proxy_status extended tests
    # -------------------------------------------------------

    async def test_get_proxy_status_verify_true_returns_live_check(self):
        """get_proxy_status with verify=True returns live_check=True in response."""
        handlers = await self._extract_handlers()
        handler = handlers["get_proxy_status"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(
            state="active", listener=True, iptables=True, upstream=True,
            live_check=True, verified_at="2026-03-16T04:00:00Z",
        ))

        result = await handler({"verify": True})

        self.assertTrue(result["live_check"])
        self._proxy.get_status.assert_awaited_once_with(verify=True)

    async def test_get_proxy_status_rejects_integer_verify(self):
        """get_proxy_status returns invalid_request when verify is integer 1."""
        handlers = await self._extract_handlers()
        handler = handlers["get_proxy_status"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict())

        result = await handler({"verify": 1})

        self.assertEqual(result["error_code"], "invalid_request")
        self.assertIn("boolean", result["error"])
        # Full response shape must still be present
        self.assertIn("state", result)
        self.assertIn("checks", result)
        self.assertIn("verified_at", result)

    async def test_get_proxy_status_full_response_shape_all_keys(self):
        """get_proxy_status response has every required key from the contract."""
        handlers = await self._extract_handlers()
        handler = handlers["get_proxy_status"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(
            state="degraded", listener=True, iptables=True, upstream=False,
            error_code="upstream_unreachable", error="upstream not reachable",
            verified_at="2026-03-16T04:30:00Z",
        ))

        result = await handler(None)

        required_keys = {"state", "checks", "error_code", "error", "verified_at", "live_check"}
        self.assertTrue(required_keys.issubset(set(result.keys())),
                        f"Missing keys: {required_keys - set(result.keys())}")
        self.assertEqual(result["state"], "degraded")
        self.assertEqual(result["error_code"], "upstream_unreachable")

    # -------------------------------------------------------
    # set_proxy_route extended tests
    # -------------------------------------------------------

    async def test_set_proxy_route_success_full_shape_all_keys(self):
        """set_proxy_route success returns ok + all status shape keys."""
        handlers = await self._extract_handlers()
        handler = handlers["set_proxy_route"]

        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(
            state="active", listener=True, iptables=True, upstream=True,
            verified_at="2026-03-16T04:00:00Z",
        ))

        result = await handler({
            "active": True,
            "host": "10.0.0.5",
            "port": 1080,
            "password": "deadbeef",
        })

        required_keys = {"ok", "state", "checks", "error_code", "error", "verified_at", "live_check"}
        self.assertTrue(required_keys.issubset(set(result.keys())),
                        f"Missing keys: {required_keys - set(result.keys())}")
        self.assertTrue(result["ok"])

    async def test_set_proxy_route_failure_full_shape_all_keys(self):
        """set_proxy_route failure returns ok=false + all status shape keys."""
        handlers = await self._extract_handlers()
        handler = handlers["set_proxy_route"]

        self._proxy.enable = AsyncMock(side_effect=RuntimeError("iptables failed"))
        self._proxy.state = ProxyState.FAILED
        self._proxy.get_status = AsyncMock(return_value=_make_status_dict(
            state="failed", error_code="internal_error",
            error="iptables failed",
        ))

        result = await handler({
            "active": True,
            "host": "10.0.0.5",
            "port": 1080,
            "password": "deadbeef",
        })

        required_keys = {"ok", "state", "checks", "error_code", "error", "verified_at", "live_check"}
        self.assertTrue(required_keys.issubset(set(result.keys())),
                        f"Missing keys: {required_keys - set(result.keys())}")
        self.assertFalse(result["ok"])
