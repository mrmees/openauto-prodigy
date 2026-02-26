"""Tests for the health monitor."""
import asyncio
import unittest
from unittest.mock import AsyncMock, patch

from health_monitor import HealthMonitor, ServiceState


class HealthMonitorTests(unittest.IsolatedAsyncioTestCase):

    async def test_initial_state_is_unknown(self):
        mon = HealthMonitor()
        health = mon.get_health()
        self.assertEqual(health["hostapd"]["state"], "unknown")
        self.assertEqual(health["bluetooth"]["state"], "unknown")
        self.assertEqual(health["networkd"]["state"], "unknown")

    async def test_check_healthy_services(self):
        mon = HealthMonitor()
        with patch.object(mon, "run_cmd", new_callable=AsyncMock) as mock_cmd:

            async def fake_cmd(*args, timeout=5):
                if "is-active" in args:
                    return (0, "active\n")
                if "ip" in args:
                    return (0, "inet 10.0.0.1/24")
                if "hciconfig" in args:
                    return (0, "UP RUNNING")
                return (0, "")

            mock_cmd.side_effect = fake_cmd

            await mon.check_once()
            health = mon.get_health()
            self.assertEqual(health["hostapd"]["state"], "active")
            self.assertEqual(health["bluetooth"]["state"], "active")
            self.assertEqual(health["networkd"]["state"], "active")

    async def test_failed_service_triggers_restart(self):
        mon = HealthMonitor()
        restart_calls = []

        with patch.object(mon, "run_cmd", new_callable=AsyncMock) as mock_cmd:

            async def fake_cmd(*args, timeout=5):
                if "is-active" in args and "hostapd.service" in args:
                    return (1, "inactive\n")
                if "is-active" in args:
                    return (0, "active\n")
                if "restart" in args:
                    restart_calls.append(args)
                    return (0, "")
                if "ip" in args:
                    return (0, "inet 10.0.0.1/24")
                if "hciconfig" in args:
                    return (0, "UP RUNNING")
                return (0, "")

            mock_cmd.side_effect = fake_cmd

            await mon.check_once()
            self.assertEqual(len(restart_calls), 1)
            self.assertIn("hostapd.service", restart_calls[0])

    async def test_retry_count_increments(self):
        mon = HealthMonitor()
        with patch.object(mon, "run_cmd", new_callable=AsyncMock) as mock_cmd:

            async def fake_cmd(*args, timeout=5):
                if "is-active" in args and "hostapd.service" in args:
                    return (1, "failed\n")
                if "is-active" in args:
                    return (0, "active\n")
                if "restart" in args:
                    return (1, "failed")  # restart also fails
                if "ip" in args:
                    return (0, "inet 10.0.0.1/24")
                if "hciconfig" in args:
                    return (0, "UP RUNNING")
                return (0, "")

            mock_cmd.side_effect = fake_cmd

            await mon.check_once()
            await mon.check_once()
            health = mon.get_health()
            self.assertGreaterEqual(health["hostapd"]["retries"], 2)
            self.assertEqual(health["hostapd"]["state"], "failed")

    async def test_stability_resets_retries(self):
        mon = HealthMonitor()
        # Simulate: service was retried, now healthy for > stability window
        # Set last_healthy to 200s ago so the stability window check passes
        loop = asyncio.get_event_loop()
        mon._services["hostapd"].retries = 2
        mon._services["hostapd"].last_healthy = loop.time() - 200

        with patch.object(mon, "run_cmd", new_callable=AsyncMock) as mock_cmd:

            async def fake_cmd(*args, timeout=5):
                if "is-active" in args:
                    return (0, "active\n")
                if "ip" in args:
                    return (0, "inet 10.0.0.1/24")
                if "hciconfig" in args:
                    return (0, "UP RUNNING")
                return (0, "")

            mock_cmd.side_effect = fake_cmd

            await mon.check_once()
            self.assertEqual(mon._services["hostapd"].retries, 0)

    async def test_bt_restart_callback_on_recovery(self):
        """Bluetooth recovery triggers the BT restart callback."""
        mon = HealthMonitor()
        callback_called = False

        async def bt_callback():
            nonlocal callback_called
            callback_called = True

        mon.set_bt_restart_callback(bt_callback)
        # Start with bluetooth in failed state
        mon._services["bluetooth"].state = "failed"

        with patch.object(mon, "run_cmd", new_callable=AsyncMock) as mock_cmd:

            async def fake_cmd(*args, timeout=5):
                if "is-active" in args:
                    return (0, "active\n")
                if "ip" in args:
                    return (0, "inet 10.0.0.1/24")
                if "hciconfig" in args:
                    return (0, "UP RUNNING")
                return (0, "")

            mock_cmd.side_effect = fake_cmd

            await mon.check_once()
            # Let the created task run
            await asyncio.sleep(0)
            self.assertTrue(callback_called)

    async def test_degraded_when_functional_check_fails(self):
        """Service active but functional check fails -> degraded state."""
        mon = HealthMonitor()
        with patch.object(mon, "run_cmd", new_callable=AsyncMock) as mock_cmd:

            async def fake_cmd(*args, timeout=5):
                if "is-active" in args:
                    return (0, "active\n")
                if "ip" in args:
                    # Missing expected IP
                    return (0, "inet 192.168.1.100/24")
                if "hciconfig" in args:
                    return (0, "UP RUNNING")
                if "restart" in args:
                    return (0, "")
                return (0, "")

            mock_cmd.side_effect = fake_cmd

            await mon.check_once()
            health = mon.get_health()
            # hostapd and networkd share the wlan0 IP check
            self.assertEqual(health["hostapd"]["state"], "degraded")
            self.assertEqual(health["networkd"]["state"], "degraded")
            # bluetooth should still be active
            self.assertEqual(health["bluetooth"]["state"], "active")

    async def test_slow_retry_after_max_fast_retries(self):
        """After MAX_FAST_RETRIES, recovery should be throttled."""
        mon = HealthMonitor()
        restart_count = 0

        with patch.object(mon, "run_cmd", new_callable=AsyncMock) as mock_cmd:

            async def fake_cmd(*args, timeout=5):
                nonlocal restart_count
                if "is-active" in args and "hostapd.service" in args:
                    return (1, "failed\n")
                if "is-active" in args:
                    return (0, "active\n")
                if "restart" in args and "hostapd.service" in args:
                    restart_count += 1
                    return (1, "failed")
                if "ip" in args:
                    return (0, "inet 10.0.0.1/24")
                if "hciconfig" in args:
                    return (0, "UP RUNNING")
                return (0, "")

            mock_cmd.side_effect = fake_cmd

            # First 3 checks should each trigger a restart (fast retries)
            for _ in range(3):
                await mon.check_once()
            self.assertEqual(restart_count, 3)

            # 4th check should be throttled (last_check is recent)
            await mon.check_once()
            self.assertEqual(restart_count, 3)  # no additional restart
            self.assertEqual(mon._services["hostapd"].retries, 3)
