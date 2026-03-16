import asyncio
import os
from asyncio.subprocess import PIPE, STDOUT
from datetime import datetime, timezone
import logging
from enum import Enum

LOG = logging.getLogger("proxy-manager")


class ProxyState(str, Enum):
    """State for the local redsocks/proxy pipeline."""

    DISABLED = "disabled"
    ACTIVE = "active"
    DEGRADED = "degraded"
    FAILED = "failed"


class ProxyManager:
    """Manage the local SOCKS redirection service used by OpenAuto Prodigy."""

    CONFIG_TEMPLATE = """base {{
    log_debug = off;
    log_info = on;
    daemon = off;
    redirector = iptables;
}}

redsocks {{
    local_ip = 127.0.0.1;
    local_port = {redirect_port};
    ip = {host};
    port = {port};
    type = socks5;
    login = "{user}";
    password = "{password}";
}}
"""
    DEFAULT_REDIRECT_PORT = 12345
    _LOCK_TIMEOUT = 10.0

    def __init__(
        self,
        config_path: str = "/run/openauto/redsocks.conf",
        redsocks_user: str = "redsocks",
        default_skip_interfaces=None,
        default_skip_networks=None,
    ):
        self._config_path = config_path
        self._redsocks_user = redsocks_user
        self._default_skip_interfaces = list(default_skip_interfaces or ["lo", "eth0"])
        self._default_skip_networks = list(
            default_skip_networks
            or ["127.0.0.0/8", "169.254.0.0/16", "10.0.0.0/24", "192.168.0.0/16"]
        )
        self._state = ProxyState.DISABLED
        self._redsocks_proc = None
        self._proxy_host = ""
        self._proxy_port = 0
        self._health_task = None
        self._state_callback = None
        self._skip_interfaces = []
        self._skip_networks = []
        self._redirect_port = self.DEFAULT_REDIRECT_PORT
        self._op_lock = asyncio.Lock()

        # Cached status fields — initialized so get_status() is safe before enable()
        self._cached_checks = {"listener": False, "iptables": False, "upstream": False}
        self._cached_error_code = None
        self._cached_error = None
        self._cached_verified_at = None
        # Track last reported failure signature to suppress repeated warnings
        self._last_failure_sig = None

    @property
    def state(self) -> ProxyState:
        return self._state

    def set_state_callback(self, cb):
        self._state_callback = cb

    def _set_state(self, new_state: ProxyState) -> None:
        if self._state != new_state:
            old = self._state
            self._state = new_state
            if self._state_callback is not None:
                self._state_callback(new_state)
            # Log transitions
            if new_state in (ProxyState.FAILED, ProxyState.DEGRADED):
                LOG.warning("State transition: %s -> %s (error_code=%s)",
                            old.value, new_state.value, self._cached_error_code)
            elif new_state == ProxyState.ACTIVE and old in (ProxyState.FAILED, ProxyState.DEGRADED):
                LOG.info("Recovery: %s -> %s", old.value, new_state.value)

    async def _run_cmd(self, *args: str) -> tuple[int, str]:
        proc = await asyncio.create_subprocess_exec(*args, stdout=PIPE, stderr=STDOUT)
        out, _ = await proc.communicate()
        return proc.returncode, (out.decode() if isinstance(out, (bytes, bytearray)) else str(out))

    # -------------------------------------------------------
    # Verification methods (injectable seam for tests)
    # -------------------------------------------------------

    async def _verify_listener(self) -> bool:
        """TCP connect to local redsocks listener. Returns True if accepting."""
        try:
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection("127.0.0.1", self._redirect_port),
                timeout=2.0,
            )
            writer.close()
            try:
                await asyncio.wait_for(writer.wait_closed(), timeout=1.0)
            except Exception:
                pass
            return True
        except (OSError, TimeoutError, asyncio.TimeoutError):
            return False

    async def _verify_routing(self) -> bool:
        """Check iptables chain exists with REDIRECT rule and OUTPUT jump."""
        # Check OPENAUTO_PROXY chain has REDIRECT to correct port
        rc, out = await self._run_cmd(
            "iptables", "-t", "nat", "-L", "OPENAUTO_PROXY", "-n",
        )
        if rc != 0:
            return False
        # Must contain REDIRECT with --to-ports {redirect_port}
        if f"redir ports {self._redirect_port}" not in out and f"to:{self._redirect_port}" not in out:
            # Also check the standard iptables output format
            has_redirect = False
            for line in out.splitlines():
                if "REDIRECT" in line and str(self._redirect_port) in line:
                    has_redirect = True
                    break
            if not has_redirect:
                return False

        # Check OUTPUT chain has jump to OPENAUTO_PROXY
        rc, out = await self._run_cmd(
            "iptables", "-t", "nat", "-L", "OUTPUT", "-n",
        )
        if rc != 0:
            return False
        if "OPENAUTO_PROXY" not in out:
            return False

        return True

    async def _verify_upstream(self) -> bool:
        """TCP connect to upstream SOCKS proxy. Returns True if reachable."""
        if not self._proxy_host or not self._proxy_port:
            return False
        try:
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(self._proxy_host, self._proxy_port),
                timeout=5.0,
            )
            writer.close()
            try:
                await asyncio.wait_for(writer.wait_closed(), timeout=1.0)
            except Exception:
                pass
            return True
        except (OSError, TimeoutError, asyncio.TimeoutError):
            return False

    async def _verify_all(self) -> dict:
        """Run all verification checks. Returns structured result."""
        listener_ok = await self._verify_listener()
        routing_ok = await self._verify_routing()
        upstream_ok = await self._verify_upstream()

        error_code = None
        error = None

        # Error priority: listener_down > routing_missing > upstream_unreachable
        if not listener_ok:
            error_code = "listener_down"
            error = f"redsocks is not listening on 127.0.0.1:{self._redirect_port}"
        elif not routing_ok:
            error_code = "routing_missing"
            error = "iptables OPENAUTO_PROXY chain or OUTPUT jump missing"
        elif not upstream_ok:
            error_code = "upstream_unreachable"
            error = f"upstream SOCKS proxy not reachable at {self._proxy_host}:{self._proxy_port}"

        return {
            "listener_ok": listener_ok,
            "routing_ok": routing_ok,
            "upstream_ok": upstream_ok,
            "error_code": error_code,
            "error": error,
        }

    def _update_cached_status(self, result: dict) -> None:
        """Update cached check results from a _verify_all() result."""
        self._cached_checks = {
            "listener": result["listener_ok"],
            "iptables": result["routing_ok"],
            "upstream": result["upstream_ok"],
        }
        self._cached_error_code = result["error_code"]
        self._cached_error = result["error"]
        self._cached_verified_at = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    # -------------------------------------------------------
    # Public API
    # -------------------------------------------------------

    async def get_status(self, *, verify: bool = False) -> dict:
        """Return full status response shape.

        If verify=True, runs live checks before responding.
        Acquires _op_lock to guarantee settled state.
        """
        try:
            async with asyncio.timeout(self._LOCK_TIMEOUT):
                async with self._op_lock:
                    if verify and self._state != ProxyState.DISABLED:
                        result = await self._verify_all()
                        self._update_cached_status(result)
                        # Update state based on verification
                        self._apply_verification_state(result)
                    return {
                        "state": self._state.value,
                        "checks": dict(self._cached_checks),
                        "error_code": self._cached_error_code,
                        "error": self._cached_error,
                        "verified_at": self._cached_verified_at,
                        "live_check": verify,
                    }
        except TimeoutError:
            return {
                "state": self._state.value,
                "checks": dict(self._cached_checks),
                "error_code": "operation_timeout",
                "error": "lock acquisition timed out",
                "verified_at": self._cached_verified_at,
                "live_check": False,
            }

    def _apply_verification_state(self, result: dict) -> None:
        """Apply state transition based on verification result."""
        if not result["listener_ok"] or not result["routing_ok"]:
            self._set_state(ProxyState.FAILED)
        elif not result["upstream_ok"]:
            self._set_state(ProxyState.DEGRADED)
        else:
            self._set_state(ProxyState.ACTIVE)

    async def enable(
        self,
        host: str,
        port: int,
        user: str,
        password: str,
        *,
        skip_interfaces=None,
        skip_networks=None,
        redirect_port: int = DEFAULT_REDIRECT_PORT,
    ) -> None:
        async with self._op_lock:
            await self._enable_locked(
                host, port, user, password,
                skip_interfaces=skip_interfaces,
                skip_networks=skip_networks,
                redirect_port=redirect_port,
            )

    async def _enable_locked(
        self,
        host: str,
        port: int,
        user: str,
        password: str,
        *,
        skip_interfaces=None,
        skip_networks=None,
        redirect_port: int = DEFAULT_REDIRECT_PORT,
    ) -> None:
        """Internal enable without lock acquisition — called by enable() and rollback."""
        if self._state != ProxyState.DISABLED:
            await self._disable_locked()

        self._proxy_host = host
        self._proxy_port = port
        self._skip_interfaces = list(self._default_skip_interfaces if skip_interfaces is None else skip_interfaces)
        self._skip_networks = list(self._default_skip_networks if skip_networks is None else skip_networks)
        self._redirect_port = redirect_port

        config_data = self.CONFIG_TEMPLATE.format(
            host=host,
            port=port,
            user=user,
            password=password,
            redirect_port=self._redirect_port,
        )

        with open(self._config_path, "w", encoding="utf-8") as conf:
            conf.write(config_data)
        os.chmod(self._config_path, 0o600)

        # Spawn redsocks under the dedicated system user for owner-based exemption
        self._redsocks_proc = await asyncio.create_subprocess_exec(
            "sudo", "-u", self._redsocks_user, "redsocks", "-c", self._config_path,
        )

        try:
            await asyncio.sleep(0.5)
            await self._iptables_apply()
            await self._dns_enable()

            # Verify local pipeline before declaring ACTIVE
            result = await self._verify_all()
            self._update_cached_status(result)

            if not result["listener_ok"] or not result["routing_ok"]:
                self._set_state(ProxyState.FAILED)
            elif not result["upstream_ok"]:
                self._set_state(ProxyState.DEGRADED)
            else:
                self._set_state(ProxyState.ACTIVE)

            if self._health_task is None or self._health_task.done():
                self._health_task = asyncio.create_task(self._health_loop())
        except Exception:
            await self._disable_locked()
            raise

    async def disable(self) -> None:
        async with self._op_lock:
            await self._disable_locked()

    async def _disable_locked(self) -> None:
        """Internal disable without lock acquisition."""
        if self._health_task is not None and not self._health_task.done():
            self._health_task.cancel()
            try:
                await self._health_task
            except asyncio.CancelledError:
                pass
            self._health_task = None
        elif self._health_task is not None:
            self._health_task = None

        errors = []

        try:
            await self._iptables_flush()
        except Exception as e:
            errors.append(f"iptables flush: {e}")

        try:
            await self._dns_restore()
        except Exception as e:
            errors.append(f"dns restore: {e}")

        try:
            await self._stop_redsocks()
        except Exception as e:
            errors.append(f"redsocks stop: {e}")

        try:
            os.remove(self._config_path)
        except FileNotFoundError:
            pass
        except Exception as e:
            errors.append(f"config cleanup: {e}")

        if errors:
            LOG.error("Proxy cleanup failures: %s", "; ".join(errors))
            self._cached_error_code = "internal_error"
            self._cached_error = "; ".join(errors)
            self._set_state(ProxyState.FAILED)
        else:
            self._cached_checks = {"listener": False, "iptables": False, "upstream": False}
            self._cached_error_code = None
            self._cached_error = None
            self._set_state(ProxyState.DISABLED)

    async def cleanup_stale_state(self) -> None:
        """Clean any stale proxy routing state from prior crash/restart."""
        LOG.info("Cleaning stale proxy routing state...")

        # Kill any orphaned redsocks from prior abnormal exit.
        try:
            await self._run_cmd("pkill", "-u", self._redsocks_user, "redsocks")
        except Exception as e:
            LOG.warning("Stale redsocks cleanup error (non-fatal): %s", e)

        try:
            await self._iptables_flush()
        except Exception as e:
            LOG.warning("Stale iptables cleanup error (non-fatal): %s", e)

        try:
            await self._dns_restore()
        except Exception as e:
            LOG.warning("Stale DNS cleanup error (non-fatal): %s", e)

        # Clean stale config file
        try:
            os.remove(self._config_path)
        except FileNotFoundError:
            pass
        except Exception as e:
            LOG.warning("Stale config cleanup error (non-fatal): %s", e)

        LOG.info("Stale proxy state cleanup complete")

    async def _stop_redsocks(self) -> None:
        if self._redsocks_proc is None:
            return

        proc = self._redsocks_proc

        if proc.returncode is not None:
            self._redsocks_proc = None
            return

        try:
            proc.terminate()
        except ProcessLookupError:
            LOG.warning("redsocks process already exited before terminate")
            self._redsocks_proc = None
            return

        try:
            await asyncio.wait_for(proc.wait(), timeout=3)
        except ProcessLookupError:
            LOG.warning("redsocks process exited before wait")
            self._redsocks_proc = None
            return
        except asyncio.TimeoutError:
            try:
                proc.kill()
            except ProcessLookupError:
                LOG.warning("redsocks process already exited before kill")
                self._redsocks_proc = None
                return
            try:
                await proc.wait()
            except ProcessLookupError:
                LOG.warning("redsocks process exited before wait during kill")
        finally:
            self._redsocks_proc = None

    async def _iptables_apply(self) -> None:
        # Step 1: Loop-delete all OUTPUT -> OPENAUTO_PROXY jumps until none remain
        while True:
            rc, _ = await self._run_cmd(
                "iptables", "-t", "nat", "-D", "OUTPUT", "-p", "tcp", "-j", "OPENAUTO_PROXY"
            )
            if rc != 0:
                break

        # Step 2: Flush and delete existing chain (ignore errors - may not exist)
        await self._run_cmd("iptables", "-t", "nat", "-F", "OPENAUTO_PROXY")
        await self._run_cmd("iptables", "-t", "nat", "-X", "OPENAUTO_PROXY")

        # Step 3: Create fresh chain
        rc, out = await self._run_cmd("iptables", "-t", "nat", "-N", "OPENAUTO_PROXY")
        if rc != 0:
            raise RuntimeError(f"iptables -N failed: {out}")

        # Step 4: Populate exemption rules in correct order

        # 4a. Owner exemption (redsocks user) - prevents redsocks traffic from being redirected
        rc, out = await self._run_cmd(
            "iptables", "-t", "nat", "-A", "OPENAUTO_PROXY",
            "-m", "owner", "--uid-owner", self._redsocks_user, "-j", "RETURN",
        )
        if rc != 0:
            raise RuntimeError(f"iptables owner exemption failed: {out}")

        # 4b. Upstream SOCKS destination exemption (host+port)
        rc, out = await self._run_cmd(
            "iptables", "-t", "nat", "-A", "OPENAUTO_PROXY",
            "-d", self._proxy_host, "-p", "tcp",
            "--dport", str(self._proxy_port), "-j", "RETURN",
        )
        if rc != 0:
            raise RuntimeError(f"iptables upstream destination exemption failed: {out}")

        # 4c. Network exemptions
        for network in self._skip_networks:
            rc, out = await self._run_cmd(
                "iptables", "-t", "nat", "-A", "OPENAUTO_PROXY",
                "-d", network, "-j", "RETURN",
            )
            if rc != 0:
                raise RuntimeError(f"iptables RETURN network {network} failed: {out}")

        # 4d. Interface exemptions
        for iface in self._skip_interfaces:
            rc, out = await self._run_cmd(
                "iptables", "-t", "nat", "-A", "OPENAUTO_PROXY",
                "-o", iface, "-j", "RETURN",
            )
            if rc != 0:
                raise RuntimeError(f"iptables RETURN interface {iface} failed: {out}")

        # 4e. Final REDIRECT rule
        rc, out = await self._run_cmd(
            "iptables", "-t", "nat", "-A", "OPENAUTO_PROXY",
            "-p", "tcp", "-j", "REDIRECT", "--to-ports", str(self._redirect_port),
        )
        if rc != 0:
            raise RuntimeError(f"iptables REDIRECT failed: {out}")

        # Step 5: Insert exactly one fresh OUTPUT jump
        rc, out = await self._run_cmd(
            "iptables", "-t", "nat", "-I", "OUTPUT", "-p", "tcp", "-j", "OPENAUTO_PROXY"
        )
        if rc != 0:
            raise RuntimeError(f"iptables output jump failed: {out}")

    async def _iptables_flush(self) -> None:
        # Loop-delete all OUTPUT -> OPENAUTO_PROXY jumps until none remain
        while True:
            rc, _ = await self._run_cmd(
                "iptables", "-t", "nat", "-D", "OUTPUT", "-p", "tcp", "-j", "OPENAUTO_PROXY"
            )
            if rc != 0:
                break

        # Flush chain (ignore error - may not exist)
        await self._run_cmd("iptables", "-t", "nat", "-F", "OPENAUTO_PROXY")
        # Delete chain (ignore error - may not exist)
        await self._run_cmd("iptables", "-t", "nat", "-X", "OPENAUTO_PROXY")

    async def _dns_enable(self) -> None:
        # Prefer systemd-resolved if available; LAN DNS still works via eth0 exclusion otherwise.
        rc, out = await self._run_cmd("which", "resolvectl")
        if rc != 0:
            LOG.info("resolvectl not available — skipping DNS override (LAN DNS still reachable via eth0)")
            return
        await self._run_cmd("resolvectl", "dns", "wlan0", "1.1.1.1")
        await self._run_cmd("resolvectl", "dnssec", "wlan0", "no")

    async def _dns_restore(self) -> None:
        rc, out = await self._run_cmd("which", "resolvectl")
        if rc != 0:
            return
        await self._run_cmd("resolvectl", "revert", "wlan0")

    async def _health_loop(self) -> None:
        while self._state in (ProxyState.ACTIVE, ProxyState.DEGRADED, ProxyState.FAILED):
            await asyncio.sleep(10)
            if self._state not in (ProxyState.ACTIVE, ProxyState.DEGRADED, ProxyState.FAILED):
                return

            # Skip tick if an operation is in flight
            if self._op_lock.locked():
                return

            try:
                async with self._op_lock:
                    result = await self._verify_all()
                    self._update_cached_status(result)

                    failure_sig = (result["listener_ok"], result["routing_ok"], result["upstream_ok"])

                    if not result["listener_ok"] or not result["routing_ok"]:
                        # Local failure -> immediate FAILED
                        if failure_sig != self._last_failure_sig:
                            LOG.warning("Health check: local pipeline failure — %s", result)
                            self._last_failure_sig = failure_sig
                        self._set_state(ProxyState.FAILED)
                    elif not result["upstream_ok"]:
                        # Local OK, upstream bad -> DEGRADED
                        if failure_sig != self._last_failure_sig:
                            LOG.warning("Health check: upstream unreachable — %s", result)
                            self._last_failure_sig = failure_sig
                        self._set_state(ProxyState.DEGRADED)
                    else:
                        # All OK -> ACTIVE (including recovery)
                        self._last_failure_sig = None
                        self._set_state(ProxyState.ACTIVE)
            except asyncio.CancelledError:
                raise
            except Exception:
                LOG.debug("Health loop tick error", exc_info=True)
