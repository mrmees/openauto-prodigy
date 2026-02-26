"""Health monitor for system services with functional checks and auto-recovery."""
import asyncio
import logging
from dataclasses import dataclass

log = logging.getLogger(__name__)

MAX_FAST_RETRIES = 3
FAST_RETRY_INTERVAL = 5.0
SLOW_RETRY_INTERVAL = 60.0
STABILITY_WINDOW = 120.0
CMD_TIMEOUT = 5


class ServiceState:
    UNKNOWN = "unknown"
    ACTIVE = "active"
    FAILED = "failed"
    DEGRADED = "degraded"
    INACTIVE = "inactive"


@dataclass
class ServiceInfo:
    name: str
    unit: str
    state: str = ServiceState.UNKNOWN
    retries: int = 0
    last_healthy: float = 0.0
    last_check: float = 0.0


class HealthMonitor:
    def __init__(self, expected_ip: str = "10.0.0.1"):
        self._expected_ip = expected_ip
        self._services: dict[str, ServiceInfo] = {
            "hostapd": ServiceInfo(name="hostapd", unit="hostapd.service"),
            "networkd": ServiceInfo(name="networkd", unit="systemd-networkd.service"),
            "bluetooth": ServiceInfo(name="bluetooth", unit="bluetooth.service"),
        }
        self._on_bt_restart_callback = None

    def set_bt_restart_callback(self, callback):
        """Called when bluetooth.service recovers (for BT profile re-registration)."""
        self._on_bt_restart_callback = callback

    def get_health(self) -> dict:
        return {
            name: {
                "state": svc.state,
                "retries": svc.retries,
            }
            for name, svc in self._services.items()
        }

    async def check_once(self):
        """Run one round of health checks."""
        for name, svc in self._services.items():
            was_down = svc.state in (
                ServiceState.FAILED,
                ServiceState.INACTIVE,
            )

            # Check systemd unit state
            rc, output = await self.run_cmd(
                "systemctl", "is-active", svc.unit, timeout=CMD_TIMEOUT
            )
            unit_active = output.strip() == "active"

            # Functional check (only if unit reports active)
            functional = True
            if unit_active:
                functional = await self._functional_check(name)

            now = asyncio.get_event_loop().time()

            if unit_active and functional:
                svc.state = ServiceState.ACTIVE

                # Reset retries after stability window — check BEFORE updating last_healthy
                # so we compare against the previous healthy timestamp
                if svc.retries > 0 and svc.last_healthy > 0 and (now - svc.last_healthy) > STABILITY_WINDOW:
                    log.info(
                        "%s stable for >%ds, resetting retries",
                        name,
                        int(STABILITY_WINDOW),
                    )
                    svc.retries = 0

                svc.last_healthy = now

                # Detect bluetooth recovery
                if name == "bluetooth" and was_down and self._on_bt_restart_callback:
                    log.info(
                        "bluetooth recovered, triggering BT profile registration"
                    )
                    task = asyncio.create_task(self._on_bt_restart_callback())
                    task.add_done_callback(self._bt_callback_done)
            else:
                svc.state = ServiceState.FAILED if rc != 0 else ServiceState.DEGRADED
                await self._attempt_recovery(svc)

            svc.last_check = now

    @staticmethod
    def _bt_callback_done(task: asyncio.Task) -> None:
        if task.cancelled():
            return
        exc = task.exception()
        if exc:
            log.error("BT profile re-registration failed: %s", exc)

    async def _functional_check(self, name: str) -> bool:
        if name in ("hostapd", "networkd"):
            rc, output = await self.run_cmd(
                "ip", "addr", "show", "wlan0", timeout=CMD_TIMEOUT
            )
            if f"inet {self._expected_ip}/" not in output:
                log.warning("wlan0 missing expected IP %s", self._expected_ip)
                return False
        elif name == "bluetooth":
            rc, output = await self.run_cmd(
                "hciconfig", "hci0", timeout=CMD_TIMEOUT
            )
            if "UP RUNNING" not in output:
                log.warning("hci0 not UP RUNNING")
                return False
        return True

    async def _attempt_recovery(self, svc: ServiceInfo):
        if svc.retries >= MAX_FAST_RETRIES:
            # Slow retry: only try every SLOW_RETRY_INTERVAL
            now = asyncio.get_event_loop().time()
            if svc.last_check > 0 and (now - svc.last_check) < SLOW_RETRY_INTERVAL:
                return

        svc.retries += 1

        # Pre-recovery fixups (before service restart)
        await self._pre_recovery(svc)

        log.warning("Restarting %s (attempt %d)", svc.unit, svc.retries)
        rc, output = await self.run_cmd(
            "systemctl", "restart", svc.unit, timeout=CMD_TIMEOUT
        )
        if rc == 0:
            log.info("%s restart succeeded", svc.unit)
            # Post-recovery fixups (after service restart)
            await self._post_recovery(svc)
        else:
            log.error("%s restart failed: %s", svc.unit, output.strip())

    async def _pre_recovery(self, svc: ServiceInfo):
        """Fix underlying issues before restarting a service."""
        if svc.name == "hostapd":
            await self._unblock_wifi_rfkill()

    async def _post_recovery(self, svc: ServiceInfo):
        """Fix issues that require the service to be running first."""
        if svc.name == "bluetooth":
            # bluetooth.service may start but leave hci0 DOWN
            await asyncio.sleep(0.5)  # let bluetoothd settle
            rc, output = await self.run_cmd(
                "hciconfig", "hci0", timeout=CMD_TIMEOUT
            )
            if "DOWN" in output and "UP" not in output:
                log.info("hci0 is DOWN after bluetooth restart, bringing up")
                await self.run_cmd(
                    "hciconfig", "hci0", "up", timeout=CMD_TIMEOUT
                )

    async def _unblock_wifi_rfkill(self):
        """Unblock WiFi if rfkill soft-blocked (common at boot on Pi)."""
        import glob
        for rfkill_dir in sorted(glob.glob("/sys/class/rfkill/rfkill*")):
            try:
                with open(f"{rfkill_dir}/type") as f:
                    if f.read().strip() != "wlan":
                        continue
                with open(f"{rfkill_dir}/soft") as f:
                    if f.read().strip() == "1":
                        log.info("WiFi rfkill soft-blocked (%s), unblocking", rfkill_dir)
                        with open(f"{rfkill_dir}/soft", "w") as fw:
                            fw.write("0")
                        await asyncio.sleep(1)  # let driver re-init
                        return
            except OSError as e:
                log.warning("rfkill check failed for %s: %s", rfkill_dir, e)

    async def run_cmd(self, *args: str, timeout: int = CMD_TIMEOUT) -> tuple[int, str]:
        try:
            proc = await asyncio.create_subprocess_exec(
                *args,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.STDOUT,
            )
            stdout, _ = await asyncio.wait_for(proc.communicate(), timeout=timeout)
            return (proc.returncode, stdout.decode())
        except asyncio.TimeoutError:
            log.warning("Command timed out: %s", args)
            try:
                proc.kill()
            except ProcessLookupError:
                pass
            return (-1, "timeout")
        except Exception as e:
            return (-1, str(e))

    async def run(self, interval: float = 5.0):
        """Run health checks in a loop."""
        log.info("Health monitor starting (interval=%ds)", interval)
        # Immediate first check
        await self.check_once()
        while True:
            await asyncio.sleep(interval)
            await self.check_once()
