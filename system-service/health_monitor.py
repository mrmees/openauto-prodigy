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
                ServiceState.UNKNOWN,
            )

            # Check systemd unit state
            rc, output = await self._run_cmd(
                f"systemctl is-active {svc.unit}", timeout=CMD_TIMEOUT
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
                    asyncio.create_task(self._on_bt_restart_callback())
            else:
                svc.state = ServiceState.FAILED if rc != 0 else ServiceState.DEGRADED
                await self._attempt_recovery(svc)

            svc.last_check = now

    async def _functional_check(self, name: str) -> bool:
        if name in ("hostapd", "networkd"):
            rc, output = await self._run_cmd(
                "ip addr show wlan0", timeout=CMD_TIMEOUT
            )
            if f"inet {self._expected_ip}/" not in output:
                log.warning("wlan0 missing expected IP %s", self._expected_ip)
                return False
        elif name == "bluetooth":
            rc, output = await self._run_cmd(
                "hciconfig hci0", timeout=CMD_TIMEOUT
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
        log.warning("Restarting %s (attempt %d)", svc.unit, svc.retries)
        rc, output = await self._run_cmd(
            f"systemctl restart {svc.unit}", timeout=CMD_TIMEOUT
        )
        if rc == 0:
            log.info("%s restart succeeded", svc.unit)
        else:
            log.error("%s restart failed: %s", svc.unit, output.strip())

    async def _run_cmd(self, cmd: str, timeout: int = CMD_TIMEOUT) -> tuple[int, str]:
        try:
            proc = await asyncio.create_subprocess_shell(
                cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.STDOUT,
            )
            stdout, _ = await asyncio.wait_for(proc.communicate(), timeout=timeout)
            return (proc.returncode, stdout.decode())
        except asyncio.TimeoutError:
            log.warning("Command timed out: %s", cmd)
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
