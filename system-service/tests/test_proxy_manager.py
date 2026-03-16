import asyncio
import os
import sys
import unittest
from unittest.mock import AsyncMock, MagicMock, patch, call

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from proxy_manager import ProxyManager, ProxyState


def _make_flush_aware_mock(success_deletes=0):
    """Create a _run_cmd mock that handles the while-loop delete pattern.

    The flush/recreate model loop-deletes OUTPUT jumps until rc != 0.
    This mock returns success for the first `success_deletes` -D OUTPUT calls,
    then failure to break the loop. All other commands succeed.
    """
    delete_count = 0

    async def mock_run_cmd(*args):
        nonlocal delete_count
        if "-D" in args and "OUTPUT" in args:
            delete_count += 1
            if delete_count > success_deletes:
                return (1, "iptables: No chain/target/match by that name.")
        return (0, "")

    return AsyncMock(side_effect=mock_run_cmd)


def _make_verify_all_mock(listener=True, routing=True, upstream=True):
    """Create a _verify_all mock returning specified check results."""
    error_code = None
    error = None
    if not listener:
        error_code = "listener_down"
        error = "redsocks is not listening on 127.0.0.1:12345"
    elif not routing:
        error_code = "routing_missing"
        error = "iptables OPENAUTO_PROXY chain or OUTPUT jump missing"
    elif not upstream:
        error_code = "upstream_unreachable"
        error = "upstream SOCKS proxy not reachable"

    return AsyncMock(return_value={
        "listener_ok": listener,
        "routing_ok": routing,
        "upstream_ok": upstream,
        "error_code": error_code,
        "error": error,
    })


class ProxyManagerTests(unittest.IsolatedAsyncioTestCase):

    def _mock_healthy_writer(self):
        reader = MagicMock()
        writer = MagicMock()
        writer.wait_closed = AsyncMock()
        return reader, writer

    def _mock_proc(self):
        proc = MagicMock()
        proc.wait = AsyncMock(return_value=0)
        proc.returncode = None
        proc.terminate = MagicMock()
        proc.kill = MagicMock()
        return proc

    async def _run_enable_with_mocks(
        self,
        pm: ProxyManager,
        host="127.0.0.2",
        port=1080,
        verify_all_mock=None,
        **kwargs,
    ):
        proc = self._mock_proc()
        pm._run_cmd = _make_flush_aware_mock()
        if verify_all_mock is None:
            pm._verify_all = _make_verify_all_mock()
        else:
            pm._verify_all = verify_all_mock
        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
            await pm.enable(host, port, "user", "pass", **kwargs)
        return proc

    async def _cleanup_health(self, pm):
        if pm._health_task is not None:
            pm._health_task.cancel()
            try:
                await pm._health_task
            except asyncio.CancelledError:
                pass

    # -------------------------------------------------------
    # Basic state tests
    # -------------------------------------------------------

    async def test_initial_state_is_disabled(self):
        pm = ProxyManager()
        self.assertEqual(pm.state, ProxyState.DISABLED)

    async def test_enable_sets_active_state(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)
        self.assertEqual(pm.state, ProxyState.ACTIVE)
        await self._cleanup_health(pm)

    async def test_disable_sets_disabled_state(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._run_cmd = _make_flush_aware_mock()
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
        await self._cleanup_health(pm)

    async def test_enable_applies_iptables(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)

        args = [call.args for call in pm._run_cmd.await_args_list]
        # REDIRECT rule present
        self.assertTrue(any("REDIRECT" in token for call_args in args for token in call_args))
        # Interface exemptions (defaults: lo, eth0)
        self.assertTrue(any("-o" in call_args and "eth0" in call_args for call_args in args))
        self.assertTrue(any("-o" in call_args and "lo" in call_args for call_args in args))
        # Network exemptions (defaults)
        self.assertTrue(any("127.0.0.0/8" in token for call_args in args for token in call_args))
        self.assertTrue(any("192.168.0.0/16" in token for call_args in args for token in call_args))
        await self._cleanup_health(pm)

    async def test_enable_applies_custom_exceptions(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(
            pm,
            skip_interfaces=["wlan0"],
            skip_networks=["172.16.0.0/12"],
        )

        args = [call.args for call in pm._run_cmd.await_args_list]
        self.assertTrue(any("-o" in call_args and "wlan0" in call_args for call_args in args))
        self.assertTrue(any("-d" in call_args and "172.16.0.0/12" in call_args for call_args in args))
        await self._cleanup_health(pm)

    async def test_disable_flushes_iptables(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)
        # Reset mock to track disable's calls
        pm._run_cmd = _make_flush_aware_mock()
        await pm.disable()

        args = [call.args for call in pm._run_cmd.await_args_list]
        self.assertTrue(
            any("-D" in token for call_args in args for token in call_args)
            or any("-F" in token for call_args in args for token in call_args)
        )

    async def test_double_enable_stops_old_process(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._run_cmd = _make_flush_aware_mock()
        pm._verify_all = _make_verify_all_mock()
        old_proc = self._mock_proc()
        new_proc = self._mock_proc()

        with patch("asyncio.create_subprocess_exec", AsyncMock(side_effect=[old_proc, new_proc])):
            await pm.enable("10.0.0.5", 1080, "oap", "deadbeef")
            await pm.enable("10.0.0.6", 1080, "oap", "cafebabe")

        old_proc.terminate.assert_called_once()

    async def test_disable_clears_redsocks_proc(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        proc = self._mock_proc()
        pm._redsocks_proc = proc
        pm._run_cmd = _make_flush_aware_mock()

        await pm.disable()
        self.assertIsNone(pm._redsocks_proc)

    async def test_disable_on_fresh_instance_is_safe(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._run_cmd = _make_flush_aware_mock()

        await pm.disable()
        self.assertEqual(pm.state, ProxyState.DISABLED)

    async def test_disable_kills_redsocks(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        proc = self._mock_proc()
        pm._redsocks_proc = proc
        pm._run_cmd = _make_flush_aware_mock()

        await pm.disable()
        proc.terminate.assert_called_once()

    async def test_disable_handles_stale_redsocks_process(self):
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        proc = self._mock_proc()
        proc.terminate.side_effect = ProcessLookupError(3, "No such process")
        pm._redsocks_proc = proc
        pm._run_cmd = _make_flush_aware_mock()

        await pm.disable()

        self.assertEqual(pm.state, ProxyState.DISABLED)
        self.assertIsNone(pm._redsocks_proc)
        proc.terminate.assert_called_once()

    # -------------------------------------------------------
    # TS-01: Permission-denied proxy-route setup failure
    # -------------------------------------------------------

    async def test_permission_denied_iptables_raises_and_cleans_up(self):
        """TS-01: iptables -N permission denied -> RuntimeError, cleanup succeeds -> DISABLED."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")

        async def mock_run_cmd(*args):
            if "-N" in args:
                return (1, "iptables: Permission denied.")
            if "-D" in args and "OUTPUT" in args:
                return (1, "iptables: No chain/target/match by that name.")
            return (0, "")

        proc = self._mock_proc()
        pm._run_cmd = AsyncMock(side_effect=mock_run_cmd)
        pm._verify_all = _make_verify_all_mock()

        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
            with self.assertRaises(RuntimeError) as ctx:
                await pm.enable("10.0.0.5", 1080, "user", "pass")

        self.assertIn("Permission denied", str(ctx.exception))
        self.assertEqual(pm.state, ProxyState.DISABLED)

    async def test_permission_denied_blanket_results_in_failed_state(self):
        """TS-01: All iptables calls fail -> cleanup also fails -> FAILED state."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")

        async def mock_run_cmd_all_fail(*args):
            if "which" in args:
                return (1, "")
            return (1, "iptables: Permission denied.")

        proc = self._mock_proc()
        pm._run_cmd = AsyncMock(side_effect=mock_run_cmd_all_fail)
        pm._verify_all = _make_verify_all_mock()

        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
            with self.assertRaises(RuntimeError):
                await pm.enable("10.0.0.5", 1080, "user", "pass")

        self.assertIn(pm.state, (ProxyState.DISABLED, ProxyState.FAILED))

    # -------------------------------------------------------
    # TS-02: Flush/recreate idempotency
    # -------------------------------------------------------

    async def test_flush_recreate_idempotency_with_preexisting_chain(self):
        """TS-02: Pre-existing jumps are deleted before fresh chain creation."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")

        delete_count = 0

        async def mock_run_cmd(*args):
            nonlocal delete_count
            if "-D" in args and "OUTPUT" in args:
                delete_count += 1
                if delete_count <= 2:
                    return (0, "")
                return (1, "iptables: No chain/target/match by that name.")
            return (0, "")

        proc = self._mock_proc()
        pm._run_cmd = AsyncMock(side_effect=mock_run_cmd)
        pm._verify_all = _make_verify_all_mock()

        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
            await pm.enable("10.0.0.5", 1080, "user", "pass")

        self.assertEqual(pm.state, ProxyState.ACTIVE)

        args_list = [c.args for c in pm._run_cmd.await_args_list]

        delete_calls = [a for a in args_list if "-D" in a and "OUTPUT" in a]
        self.assertEqual(len(delete_calls), 3)

        flush_calls = [a for a in args_list if "-F" in a and "OPENAUTO_PROXY" in a]
        self.assertTrue(len(flush_calls) >= 1)

        destroy_calls = [a for a in args_list if "-X" in a and "OPENAUTO_PROXY" in a]
        self.assertTrue(len(destroy_calls) >= 1)

        create_calls = [a for a in args_list if "-N" in a and "OPENAUTO_PROXY" in a]
        self.assertEqual(len(create_calls), 1)

        insert_calls = [a for a in args_list if "-I" in a and "OUTPUT" in a]
        self.assertEqual(len(insert_calls), 1)

        await self._cleanup_health(pm)

    async def test_repeated_enable_5x_only_one_jump_per_cycle(self):
        """TS-02: 5 consecutive enables, each cleans previous state first."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")

        for i in range(5):
            proc = self._mock_proc()
            existing_jumps = 1 if i > 0 else 0
            pm._run_cmd = _make_flush_aware_mock(success_deletes=existing_jumps)
            pm._verify_all = _make_verify_all_mock()
            with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
                await pm.enable(f"10.0.0.{i}", 1080, "user", "pass")

            self.assertEqual(pm.state, ProxyState.ACTIVE)

            args_list = [c.args for c in pm._run_cmd.await_args_list]
            insert_calls = [a for a in args_list if "-I" in a and "OUTPUT" in a]
            self.assertEqual(len(insert_calls), 1, f"Cycle {i}: expected 1 -I OUTPUT, got {len(insert_calls)}")

        await self._cleanup_health(pm)

    # -------------------------------------------------------
    # TS-03: Exemption rules
    # -------------------------------------------------------

    async def test_upstream_destination_exemption(self):
        """TS-03: Upstream SOCKS destination (host+port) gets a RETURN rule."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm, host="10.0.0.5", port=1080)

        args_list = [c.args for c in pm._run_cmd.await_args_list]
        dest_exempt = [
            a for a in args_list
            if "-d" in a and "10.0.0.5" in a and "--dport" in a and "1080" in a and "RETURN" in a
        ]
        self.assertEqual(len(dest_exempt), 1, "Expected exactly one upstream destination exemption rule")
        await self._cleanup_health(pm)

    async def test_redsocks_owner_self_exemption(self):
        """TS-03: Redsocks user gets owner-based RETURN rule."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)

        args_list = [c.args for c in pm._run_cmd.await_args_list]
        owner_exempt = [
            a for a in args_list
            if "--uid-owner" in a and "redsocks" in a and "RETURN" in a
        ]
        self.assertEqual(len(owner_exempt), 1, "Expected exactly one owner exemption rule")
        await self._cleanup_health(pm)

    async def test_exemption_rule_ordering(self):
        """TS-03: owner RETURN < destination RETURN < network RETURNs < interface RETURNs < REDIRECT."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm, host="10.0.0.5", port=1080)

        args_list = [c.args for c in pm._run_cmd.await_args_list]

        append_rules = [(i, a) for i, a in enumerate(args_list) if "-A" in a and "OPENAUTO_PROXY" in a]

        owner_idx = None
        dest_idx = None
        network_idx = None
        iface_idx = None
        redirect_idx = None

        for idx, (orig_idx, a) in enumerate(append_rules):
            if "--uid-owner" in a:
                owner_idx = idx
            elif "--dport" in a and "RETURN" in a:
                dest_idx = idx
            elif "-d" in a and "RETURN" in a and network_idx is None:
                network_idx = idx
            elif "-o" in a and "RETURN" in a and iface_idx is None:
                iface_idx = idx
            elif "REDIRECT" in a:
                redirect_idx = idx

        self.assertIsNotNone(owner_idx, "Owner exemption not found")
        self.assertIsNotNone(dest_idx, "Destination exemption not found")
        self.assertIsNotNone(network_idx, "Network exemption not found")
        self.assertIsNotNone(iface_idx, "Interface exemption not found")
        self.assertIsNotNone(redirect_idx, "REDIRECT not found")

        self.assertLess(owner_idx, dest_idx, "Owner must come before destination")
        self.assertLess(dest_idx, network_idx, "Destination must come before networks")
        self.assertLess(network_idx, iface_idx, "Networks must come before interfaces")
        self.assertLess(iface_idx, redirect_idx, "Interfaces must come before REDIRECT")
        await self._cleanup_health(pm)

    async def test_default_network_exemptions(self):
        """TS-03: Default network exemptions include the four expected subnets."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)

        args_list = [c.args for c in pm._run_cmd.await_args_list]
        expected_networks = ["127.0.0.0/8", "169.254.0.0/16", "10.0.0.0/24", "192.168.0.0/16"]

        for net in expected_networks:
            matching = [a for a in args_list if "-d" in a and net in a and "RETURN" in a]
            self.assertTrue(len(matching) >= 1, f"Expected network exemption for {net}")

        await self._cleanup_health(pm)

    async def test_default_interface_exemptions(self):
        """TS-03: Default interface exemptions include lo and eth0."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)

        args_list = [c.args for c in pm._run_cmd.await_args_list]
        for iface in ["lo", "eth0"]:
            matching = [a for a in args_list if "-o" in a and iface in a and "RETURN" in a]
            self.assertTrue(len(matching) >= 1, f"Expected interface exemption for {iface}")

        await self._cleanup_health(pm)

    # -------------------------------------------------------
    # TS-04: Cleanup failure honesty
    # -------------------------------------------------------

    async def test_cleanup_failure_sets_failed_state(self):
        """TS-04: disable() sets FAILED when iptables_flush raises."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._run_cmd = _make_flush_aware_mock()

        pm._iptables_flush = AsyncMock(side_effect=RuntimeError("iptables flush exploded"))

        await pm.disable()
        self.assertEqual(pm.state, ProxyState.FAILED)

    async def test_cleanup_full_success_sets_disabled(self):
        """TS-04: disable() sets DISABLED when all cleanup steps succeed."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        proc = self._mock_proc()
        pm._redsocks_proc = proc
        pm._run_cmd = _make_flush_aware_mock()

        await pm.disable()
        self.assertEqual(pm.state, ProxyState.DISABLED)

    # -------------------------------------------------------
    # Startup self-heal: cleanup_stale_state()
    # -------------------------------------------------------

    async def test_cleanup_stale_state_calls_flush_and_dns_restore(self):
        """cleanup_stale_state() calls iptables flush and dns restore."""
        pm = ProxyManager(config_path="/tmp/test_redsocks_stale.conf")
        pm._run_cmd = _make_flush_aware_mock()

        await pm.cleanup_stale_state()

        args_list = [c.args for c in pm._run_cmd.await_args_list]

        pkill_calls = [a for a in args_list if "pkill" in a]
        self.assertTrue(len(pkill_calls) >= 1, "Should call pkill for orphaned redsocks")

        delete_calls = [a for a in args_list if "-D" in a and "OUTPUT" in a]
        self.assertTrue(len(delete_calls) >= 1, "Should attempt iptables delete loop")

        which_calls = [a for a in args_list if "which" in a and "resolvectl" in a]
        self.assertTrue(len(which_calls) >= 1, "Should check for resolvectl")

    async def test_cleanup_stale_state_swallows_errors(self):
        """cleanup_stale_state() logs errors but does not raise."""
        pm = ProxyManager(config_path="/tmp/test_redsocks_stale.conf")

        async def mock_all_fail(*args):
            raise OSError("everything is broken")

        pm._run_cmd = AsyncMock(side_effect=mock_all_fail)

        await pm.cleanup_stale_state()

    async def test_state_callback_invoked(self):
        """State callback is invoked on state transitions."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        states_received = []
        pm.set_state_callback(lambda s: states_received.append(s))

        pm._set_state(ProxyState.ACTIVE)
        pm._set_state(ProxyState.ACTIVE)  # duplicate, should not trigger
        pm._set_state(ProxyState.DEGRADED)

        self.assertEqual(states_received, [ProxyState.ACTIVE, ProxyState.DEGRADED])

    # -------------------------------------------------------
    # SR-01: Verification methods and honest enable
    # -------------------------------------------------------

    async def test_verify_listener_success(self):
        """_verify_listener returns True when TCP connect succeeds."""
        pm = ProxyManager()
        pm._redirect_port = 12345

        reader, writer = self._mock_healthy_writer()
        with patch("asyncio.open_connection", AsyncMock(return_value=(reader, writer))):
            result = await pm._verify_listener()

        self.assertTrue(result)

    async def test_verify_listener_failure(self):
        """_verify_listener returns False when TCP connect fails."""
        pm = ProxyManager()
        pm._redirect_port = 12345

        with patch("asyncio.open_connection", AsyncMock(side_effect=OSError("refused"))):
            result = await pm._verify_listener()

        self.assertFalse(result)

    async def test_verify_upstream_success(self):
        """_verify_upstream returns True when upstream SOCKS is reachable."""
        pm = ProxyManager()
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080

        reader, writer = self._mock_healthy_writer()
        with patch("asyncio.open_connection", AsyncMock(return_value=(reader, writer))):
            result = await pm._verify_upstream()

        self.assertTrue(result)

    async def test_verify_upstream_failure(self):
        """_verify_upstream returns False when upstream is unreachable."""
        pm = ProxyManager()
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080

        with patch("asyncio.open_connection", AsyncMock(side_effect=OSError("timeout"))):
            result = await pm._verify_upstream()

        self.assertFalse(result)

    async def test_verify_upstream_no_host(self):
        """_verify_upstream returns False when no host is configured."""
        pm = ProxyManager()
        result = await pm._verify_upstream()
        self.assertFalse(result)

    async def test_verify_routing_success(self):
        """_verify_routing returns True when chain and jump both present."""
        pm = ProxyManager()
        pm._redirect_port = 12345

        call_count = 0

        async def mock_run_cmd(*args):
            nonlocal call_count
            call_count += 1
            if "-L" in args and "OPENAUTO_PROXY" in args and "OUTPUT" not in args:
                return (0, "Chain OPENAUTO_PROXY\ntarget     prot opt source    destination\nREDIRECT   tcp  -- 0.0.0.0/0  0.0.0.0/0  redir ports 12345\n")
            if "-L" in args and "OUTPUT" in args:
                return (0, "Chain OUTPUT\ntarget     prot opt source    destination\nOPENAUTO_PROXY  tcp  -- 0.0.0.0/0  0.0.0.0/0\n")
            return (0, "")

        pm._run_cmd = AsyncMock(side_effect=mock_run_cmd)
        result = await pm._verify_routing()
        self.assertTrue(result)

    async def test_verify_routing_no_chain(self):
        """_verify_routing returns False when chain doesn't exist."""
        pm = ProxyManager()
        pm._redirect_port = 12345

        async def mock_run_cmd(*args):
            if "-L" in args and "OPENAUTO_PROXY" in args and "OUTPUT" not in args:
                return (1, "iptables: No chain/target/match by that name.")
            return (0, "")

        pm._run_cmd = AsyncMock(side_effect=mock_run_cmd)
        result = await pm._verify_routing()
        self.assertFalse(result)

    async def test_verify_routing_no_jump(self):
        """_verify_routing returns False when OUTPUT has no OPENAUTO_PROXY jump."""
        pm = ProxyManager()
        pm._redirect_port = 12345

        async def mock_run_cmd(*args):
            if "-L" in args and "OPENAUTO_PROXY" in args and "OUTPUT" not in args:
                return (0, "Chain OPENAUTO_PROXY\nREDIRECT tcp redir ports 12345\n")
            if "-L" in args and "OUTPUT" in args:
                return (0, "Chain OUTPUT\ntarget prot opt source destination\n")
            return (0, "")

        pm._run_cmd = AsyncMock(side_effect=mock_run_cmd)
        result = await pm._verify_routing()
        self.assertFalse(result)

    async def test_verify_all_all_ok(self):
        """_verify_all returns all ok when everything passes."""
        pm = ProxyManager()
        pm._verify_listener = AsyncMock(return_value=True)
        pm._verify_routing = AsyncMock(return_value=True)
        pm._verify_upstream = AsyncMock(return_value=True)

        result = await pm._verify_all()
        self.assertTrue(result["listener_ok"])
        self.assertTrue(result["routing_ok"])
        self.assertTrue(result["upstream_ok"])
        self.assertIsNone(result["error_code"])

    async def test_verify_all_listener_down(self):
        """_verify_all returns listener_down with priority over other failures."""
        pm = ProxyManager()
        pm._redirect_port = 12345
        pm._verify_listener = AsyncMock(return_value=False)
        pm._verify_routing = AsyncMock(return_value=False)
        pm._verify_upstream = AsyncMock(return_value=False)

        result = await pm._verify_all()
        self.assertEqual(result["error_code"], "listener_down")

    async def test_verify_all_routing_missing(self):
        """_verify_all returns routing_missing when listener ok but routing fails."""
        pm = ProxyManager()
        pm._verify_listener = AsyncMock(return_value=True)
        pm._verify_routing = AsyncMock(return_value=False)
        pm._verify_upstream = AsyncMock(return_value=True)

        result = await pm._verify_all()
        self.assertEqual(result["error_code"], "routing_missing")

    async def test_verify_all_upstream_unreachable(self):
        """_verify_all returns upstream_unreachable when local ok but upstream fails."""
        pm = ProxyManager()
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080
        pm._verify_listener = AsyncMock(return_value=True)
        pm._verify_routing = AsyncMock(return_value=True)
        pm._verify_upstream = AsyncMock(return_value=False)

        result = await pm._verify_all()
        self.assertEqual(result["error_code"], "upstream_unreachable")

    async def test_enable_verifies_before_active(self):
        """enable() calls _verify_all and sets ACTIVE only if local pipeline ok."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)
        # _verify_all was called (mocked to return all ok)
        pm._verify_all.assert_awaited()
        self.assertEqual(pm.state, ProxyState.ACTIVE)
        await self._cleanup_health(pm)

    async def test_enable_sets_failed_on_listener_down(self):
        """enable() sets FAILED when _verify_all reports listener_down."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(
            pm,
            verify_all_mock=_make_verify_all_mock(listener=False),
        )
        self.assertEqual(pm.state, ProxyState.FAILED)
        await self._cleanup_health(pm)

    async def test_enable_sets_degraded_on_upstream_failure(self):
        """enable() sets DEGRADED when local ok but upstream unreachable."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(
            pm,
            verify_all_mock=_make_verify_all_mock(upstream=False),
        )
        self.assertEqual(pm.state, ProxyState.DEGRADED)
        await self._cleanup_health(pm)

    # -------------------------------------------------------
    # SR-02: get_status() response shape
    # -------------------------------------------------------

    async def test_get_status_returns_full_shape(self):
        """get_status() returns dict with all required keys."""
        pm = ProxyManager()
        status = await pm.get_status()

        self.assertIn("state", status)
        self.assertIn("checks", status)
        self.assertIn("error_code", status)
        self.assertIn("error", status)
        self.assertIn("verified_at", status)
        self.assertIn("live_check", status)
        self.assertEqual(status["state"], "disabled")
        self.assertFalse(status["live_check"])

    async def test_get_status_disabled_safe(self):
        """get_status() on fresh DISABLED proxy returns well-formed response without crash."""
        pm = ProxyManager()
        status = await pm.get_status()

        self.assertEqual(status["state"], "disabled")
        self.assertEqual(status["checks"], {"listener": False, "iptables": False, "upstream": False})
        self.assertIsNone(status["error_code"])
        self.assertIsNone(status["error"])
        self.assertIsNone(status["verified_at"])

    async def test_get_status_with_verify(self):
        """get_status(verify=True) runs live checks and returns results."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._verify_all = _make_verify_all_mock()

        status = await pm.get_status(verify=True)

        self.assertTrue(status["live_check"])
        self.assertTrue(status["checks"]["listener"])
        self.assertTrue(status["checks"]["iptables"])
        self.assertTrue(status["checks"]["upstream"])
        self.assertIsNotNone(status["verified_at"])
        pm._verify_all.assert_awaited_once()

    async def test_get_status_verify_skipped_when_disabled(self):
        """get_status(verify=True) on DISABLED skips live checks."""
        pm = ProxyManager()
        pm._verify_all = _make_verify_all_mock()

        status = await pm.get_status(verify=True)

        pm._verify_all.assert_not_awaited()
        self.assertEqual(status["state"], "disabled")

    async def test_get_status_cached_after_enable(self):
        """get_status() without verify returns cached results after enable."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(pm)

        status = await pm.get_status()

        self.assertEqual(status["state"], "active")
        self.assertTrue(status["checks"]["listener"])
        self.assertFalse(status["live_check"])
        self.assertIsNotNone(status["verified_at"])
        await self._cleanup_health(pm)

    # -------------------------------------------------------
    # SR-03: Health loop redesigned
    # -------------------------------------------------------

    async def test_health_loop_detects_local_failure(self):
        """Health loop sets FAILED immediately on local pipeline failure."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._redirect_port = 12345
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080

        # Mock _verify_all to report listener down
        pm._verify_all = _make_verify_all_mock(listener=False)

        # Run one health tick manually (simulate what the loop would do)
        result = await pm._verify_all()
        pm._update_cached_status(result)
        pm._apply_verification_state(result)

        self.assertEqual(pm.state, ProxyState.FAILED)
        self.assertEqual(pm._cached_error_code, "listener_down")

    async def test_health_loop_detects_upstream_failure(self):
        """Health loop sets DEGRADED when local ok but upstream fails."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080

        pm._verify_all = _make_verify_all_mock(upstream=False)

        result = await pm._verify_all()
        pm._update_cached_status(result)
        pm._apply_verification_state(result)

        self.assertEqual(pm.state, ProxyState.DEGRADED)
        self.assertEqual(pm._cached_error_code, "upstream_unreachable")

    async def test_health_loop_detects_recovery(self):
        """Health loop transitions FAILED -> ACTIVE when checks pass again."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.FAILED
        pm._cached_error_code = "listener_down"

        pm._verify_all = _make_verify_all_mock()

        result = await pm._verify_all()
        pm._update_cached_status(result)
        pm._apply_verification_state(result)

        self.assertEqual(pm.state, ProxyState.ACTIVE)
        self.assertIsNone(pm._cached_error_code)

    # -------------------------------------------------------
    # Lock behavior
    # -------------------------------------------------------

    async def test_op_lock_exists(self):
        """ProxyManager has an asyncio.Lock for operation guarding."""
        pm = ProxyManager()
        self.assertIsInstance(pm._op_lock, asyncio.Lock)

    async def test_enable_acquires_lock(self):
        """enable() acquires _op_lock during execution."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        lock_was_held = False

        original_enable_locked = pm._enable_locked

        async def spy_enable_locked(*args, **kwargs):
            nonlocal lock_was_held
            lock_was_held = pm._op_lock.locked()
            return await original_enable_locked(*args, **kwargs)

        pm._enable_locked = spy_enable_locked
        pm._run_cmd = _make_flush_aware_mock()
        pm._verify_all = _make_verify_all_mock()
        proc = self._mock_proc()

        with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
            await pm.enable("10.0.0.5", 1080, "user", "pass")

        self.assertTrue(lock_was_held)
        await self._cleanup_health(pm)

    # -------------------------------------------------------
    # SR-01 extended: enable() routing verification
    # -------------------------------------------------------

    async def test_enable_sets_failed_on_routing_missing(self):
        """enable() sets FAILED when _verify_all reports routing_missing."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        await self._run_enable_with_mocks(
            pm,
            verify_all_mock=_make_verify_all_mock(routing=False),
        )
        self.assertEqual(pm.state, ProxyState.FAILED)
        self.assertEqual(pm._cached_error_code, "routing_missing")
        await self._cleanup_health(pm)

    # -------------------------------------------------------
    # SR-02 extended: error code priority
    # -------------------------------------------------------

    async def test_error_code_priority_listener_over_routing(self):
        """listener_down has priority over routing_missing when both fail."""
        pm = ProxyManager()
        pm._redirect_port = 12345
        pm._verify_listener = AsyncMock(return_value=False)
        pm._verify_routing = AsyncMock(return_value=False)
        pm._verify_upstream = AsyncMock(return_value=True)

        result = await pm._verify_all()
        self.assertEqual(result["error_code"], "listener_down")

    async def test_error_code_priority_routing_over_upstream(self):
        """routing_missing has priority over upstream_unreachable when both fail."""
        pm = ProxyManager()
        pm._verify_listener = AsyncMock(return_value=True)
        pm._verify_routing = AsyncMock(return_value=False)
        pm._verify_upstream = AsyncMock(return_value=False)

        result = await pm._verify_all()
        self.assertEqual(result["error_code"], "routing_missing")

    # -------------------------------------------------------
    # SR-03 extended: health loop routing + no corrective action + recovery
    # -------------------------------------------------------

    async def test_health_loop_detects_routing_failure(self):
        """Health loop sets FAILED immediately on routing failure."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._redirect_port = 12345
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080

        pm._verify_all = _make_verify_all_mock(routing=False)

        result = await pm._verify_all()
        pm._update_cached_status(result)
        pm._apply_verification_state(result)

        self.assertEqual(pm.state, ProxyState.FAILED)
        self.assertEqual(pm._cached_error_code, "routing_missing")

    async def test_health_loop_no_corrective_action(self):
        """Health loop only detects and reports — does not call enable/disable/restart."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._redirect_port = 12345
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080
        pm._verify_all = _make_verify_all_mock(listener=False)

        # Spy on _run_cmd to ensure health loop tick doesn't invoke subprocess
        pm._run_cmd = AsyncMock()

        result = await pm._verify_all()
        pm._update_cached_status(result)
        pm._apply_verification_state(result)

        # _run_cmd should NOT have been called — no corrective action
        pm._run_cmd.assert_not_awaited()
        self.assertEqual(pm.state, ProxyState.FAILED)

    async def test_health_loop_degraded_to_active_recovery(self):
        """Health loop transitions DEGRADED -> ACTIVE when upstream recovers."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.DEGRADED
        pm._cached_error_code = "upstream_unreachable"

        pm._verify_all = _make_verify_all_mock()  # all OK

        result = await pm._verify_all()
        pm._update_cached_status(result)
        pm._apply_verification_state(result)

        self.assertEqual(pm.state, ProxyState.ACTIVE)
        self.assertIsNone(pm._cached_error_code)

    async def test_health_loop_failed_to_active_external_recovery(self):
        """Health loop detects external recovery: FAILED -> ACTIVE when pipeline becomes healthy."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.FAILED
        pm._cached_error_code = "routing_missing"

        # Simulate external fix — someone manually restored iptables + redsocks
        pm._verify_all = _make_verify_all_mock()  # all OK now

        result = await pm._verify_all()
        pm._update_cached_status(result)
        pm._apply_verification_state(result)

        self.assertEqual(pm.state, ProxyState.ACTIVE)
        self.assertIsNone(pm._cached_error_code)

    # -------------------------------------------------------
    # Response shape extended
    # -------------------------------------------------------

    async def test_get_status_checks_has_correct_keys(self):
        """get_status checks dict has exactly listener, iptables, upstream booleans."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._verify_all = _make_verify_all_mock()

        status = await pm.get_status(verify=True)

        checks = status["checks"]
        self.assertEqual(set(checks.keys()), {"listener", "iptables", "upstream"})
        for key, val in checks.items():
            self.assertIsInstance(val, bool, f"checks[{key}] should be bool, got {type(val)}")

    async def test_get_status_verified_at_iso8601(self):
        """verified_at is ISO 8601 formatted string after verification."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._state = ProxyState.ACTIVE
        pm._verify_all = _make_verify_all_mock()

        status = await pm.get_status(verify=True)

        verified_at = status["verified_at"]
        self.assertIsNotNone(verified_at)
        # Must end with Z (UTC) and parse as datetime
        self.assertTrue(verified_at.endswith("Z"))
        from datetime import datetime
        parsed = datetime.fromisoformat(verified_at.replace("Z", "+00:00"))
        self.assertIsNotNone(parsed)

    # -------------------------------------------------------
    # Lock behavior extended
    # -------------------------------------------------------

    async def test_get_status_blocks_while_enable_in_flight(self):
        """get_status waits for enable to complete before returning (settled state)."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")

        enable_started = asyncio.Event()
        enable_continue = asyncio.Event()
        status_result = {}

        original_enable_locked = pm._enable_locked

        async def slow_enable_locked(*args, **kwargs):
            enable_started.set()
            await enable_continue.wait()
            return await original_enable_locked(*args, **kwargs)

        pm._enable_locked = slow_enable_locked
        pm._run_cmd = _make_flush_aware_mock()
        pm._verify_all = _make_verify_all_mock()
        proc = self._mock_proc()

        async def do_enable():
            with patch("asyncio.create_subprocess_exec", AsyncMock(return_value=proc)):
                await pm.enable("10.0.0.5", 1080, "user", "pass")

        async def do_get_status():
            await enable_started.wait()
            # enable is holding the lock — get_status should block
            result = await pm.get_status()
            status_result.update(result)

        # Start enable, then get_status. Release enable after a moment.
        enable_task = asyncio.create_task(do_enable())
        status_task = asyncio.create_task(do_get_status())

        await asyncio.sleep(0.1)
        enable_continue.set()

        await asyncio.gather(enable_task, status_task)

        # get_status returned AFTER enable completed, so state should be settled (active)
        self.assertEqual(status_result["state"], "active")
        await self._cleanup_health(pm)

    async def test_lock_timeout_returns_operation_timeout(self):
        """get_status returns operation_timeout error_code when lock times out."""
        pm = ProxyManager(config_path="/tmp/test_redsocks.conf")
        pm._LOCK_TIMEOUT = 0.1  # very short timeout

        # Acquire lock externally to force timeout
        await pm._op_lock.acquire()

        try:
            status = await pm.get_status()
            self.assertEqual(status["error_code"], "operation_timeout")
            self.assertIn("timed out", status["error"])
            self.assertFalse(status["live_check"])
        finally:
            pm._op_lock.release()


if __name__ == "__main__":
    unittest.main()
