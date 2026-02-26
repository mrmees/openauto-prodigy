import asyncio
import os
from asyncio.subprocess import PIPE, STDOUT
import logging
from enum import Enum


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
    local_port = 12345;
    ip = {host};
    port = {port};
    type = socks5;
    login = "{user}";
    password = "{password}";
}}
"""

    def __init__(self, config_path: str = "/run/openauto/redsocks.conf"):
        self._config_path = config_path
        self._state = ProxyState.DISABLED
        self._redsocks_proc = None
        self._proxy_host = ""
        self._proxy_port = 0
        self._fail_count = 0
        self._health_task = None
        self._state_callback = None

    @property
    def state(self) -> ProxyState:
        return self._state

    def set_state_callback(self, cb):
        self._state_callback = cb

    def _set_state(self, new_state: ProxyState) -> None:
        if self._state != new_state:
            self._state = new_state
            if self._state_callback is not None:
                self._state_callback(new_state)

    async def _run_cmd(self, *args: str) -> tuple[int, str]:
        proc = await asyncio.create_subprocess_exec(*args, stdout=PIPE, stderr=STDOUT)
        out, _ = await proc.communicate()
        return proc.returncode, (out.decode() if isinstance(out, (bytes, bytearray)) else str(out))

    async def enable(self, host: str, port: int, user: str, password: str) -> None:
        if self._state != ProxyState.DISABLED:
            await self.disable()

        self._proxy_host = host
        self._proxy_port = port
        self._fail_count = 0

        config_data = self.CONFIG_TEMPLATE.format(
            host=host,
            port=port,
            user=user,
            password=password,
        )

        with open(self._config_path, "w", encoding="utf-8") as conf:
            conf.write(config_data)
        os.chmod(self._config_path, 0o600)

        self._redsocks_proc = await asyncio.create_subprocess_exec(
            "redsocks",
            "-c",
            self._config_path,
        )

        try:
            await asyncio.sleep(0.5)
            await self._iptables_apply()
            await self._dns_enable()
            self._set_state(ProxyState.ACTIVE)

            if self._health_task is None or self._health_task.done():
                self._health_task = asyncio.create_task(self._health_loop())
        except Exception:
            await self.disable()
            raise

    async def disable(self) -> None:
        if self._health_task is not None and not self._health_task.done():
            self._health_task.cancel()
            try:
                await self._health_task
            except asyncio.CancelledError:
                pass
            self._health_task = None
        elif self._health_task is not None:
            self._health_task = None

        await self._iptables_flush()
        await self._dns_restore()
        await self._stop_redsocks()

        try:
            os.remove(self._config_path)
        except FileNotFoundError:
            pass

        self._set_state(ProxyState.DISABLED)

    async def _stop_redsocks(self) -> None:
        if self._redsocks_proc is None:
            return

        proc = self._redsocks_proc
        proc.terminate()
        try:
            await asyncio.wait_for(proc.wait(), timeout=3)
        except asyncio.TimeoutError:
            proc.kill()
            await proc.wait()
        finally:
            self._redsocks_proc = None

    async def _iptables_apply(self) -> None:
        rc, out = await self._run_cmd("iptables", "-t", "nat", "-N", "OPENAUTO_PROXY")
        if rc != 0 and "Chain already exists" not in out:
            raise RuntimeError(f"iptables -N failed: {out}")
        rc, out = await self._run_cmd(
            "iptables",
            "-t",
            "nat",
            "-A",
            "OPENAUTO_PROXY",
            "-d",
            "127.0.0.0/8",
            "-j",
            "RETURN",
        )
        if rc != 0:
            raise RuntimeError(f"iptables RETURN local failed: {out}")
        rc, out = await self._run_cmd(
            "iptables",
            "-t",
            "nat",
            "-A",
            "OPENAUTO_PROXY",
            "-d",
            "10.0.0.0/24",
            "-j",
            "RETURN",
        )
        if rc != 0:
            raise RuntimeError(f"iptables RETURN private failed: {out}")
        rc, out = await self._run_cmd(
            "iptables",
            "-t",
            "nat",
            "-A",
            "OPENAUTO_PROXY",
            "-d",
            "192.168.0.0/16",
            "-j",
            "RETURN",
        )
        if rc != 0:
            raise RuntimeError(f"iptables RETURN vpn failed: {out}")
        rc, out = await self._run_cmd(
            "iptables",
            "-t",
            "nat",
            "-A",
            "OPENAUTO_PROXY",
            "-p",
            "tcp",
            "-j",
            "REDIRECT",
            "--to-ports",
            "12345",
        )
        if rc != 0:
            raise RuntimeError(f"iptables REDIRECT failed: {out}")
        rc, out = await self._run_cmd("iptables", "-t", "nat", "-I", "OUTPUT", "-p", "tcp", "-j", "OPENAUTO_PROXY")
        if rc != 0:
            raise RuntimeError(f"iptables output jump failed: {out}")

    async def _iptables_flush(self) -> None:
        commands = [
            ("iptables", "-t", "nat", "-D", "OUTPUT", "-p", "tcp", "-j", "OPENAUTO_PROXY"),
            ("iptables", "-t", "nat", "-F", "OPENAUTO_PROXY"),
            ("iptables", "-t", "nat", "-X", "OPENAUTO_PROXY"),
        ]
        for cmd in commands:
            try:
                rc, out = await self._run_cmd(*cmd)
            except Exception as exc:  # pragma: no cover - depends on command execution environment
                logging.warning("iptables flush failed: %s", exc)
                continue
            if rc != 0:
                logging.warning("iptables flush %s failed with rc=%d: %s", " ".join(cmd), rc, out)

    async def _dns_enable(self) -> None:
        await self._run_cmd("resolvectl", "dns", "wlan0", "1.1.1.1")
        await self._run_cmd("resolvectl", "dnssec", "wlan0", "no")

    async def _dns_restore(self) -> None:
        await self._run_cmd("resolvectl", "revert", "wlan0")

    async def _health_loop(self) -> None:
        while self._state in (ProxyState.ACTIVE, ProxyState.DEGRADED):
            await asyncio.sleep(30)
            if self._state not in (ProxyState.ACTIVE, ProxyState.DEGRADED):
                return
            await self._probe_once()

    async def _probe_once(self) -> None:
        try:
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(self._proxy_host, self._proxy_port),
                timeout=5.0,
            )
            writer.close()
            try:
                await asyncio.wait_for(writer.wait_closed(), timeout=2.0)
            except Exception:
                pass
            self._fail_count = 0
            if self._state == ProxyState.DEGRADED:
                self._set_state(ProxyState.ACTIVE)
        except (OSError, TimeoutError, asyncio.TimeoutError):
            self._fail_count += 1
            if self._fail_count >= 3:
                self._set_state(ProxyState.DEGRADED)
