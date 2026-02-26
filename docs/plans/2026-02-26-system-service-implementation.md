# System Service Daemon Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build `openauto-system`, a Python asyncio daemon that monitors system service health, applies system config from the app's config.yaml, and registers BT profiles — all exposed via JSON-over-Unix-socket IPC.

**Architecture:** Single Python asyncio process running as root via systemd. Three internal components: IPC server (Unix socket), health monitor (5s polling + auto-recovery), config applier (template-based config file rewriting). The Qt app communicates via JSON request/response over the socket.

**Tech Stack:** Python 3.11+, asyncio, dbus-next (async D-Bus for BlueZ), PyYAML, systemd sd_notify

---

## Reference

- **Design doc:** `docs/plans/2026-02-26-system-service-design.md`
- **Existing IPC pattern:** `src/core/services/IpcServer.cpp` — JSON + newline over Unix socket
- **Existing config:** `src/core/YamlConfig.cpp` — YAML with dot-path access
- **Install script templates:** `install.sh` lines 210-243 (hostapd, networkd configs)
- **BT profile details:** `../../openauto-pro-community/docs/bluetooth-wireless-aa-setup.md`

## File Layout

All daemon code lives in `system-service/` at the repo root:

```
system-service/
├── openauto_system.py          # Entry point, asyncio main loop
├── ipc_server.py               # Unix socket server, JSON protocol
├── health_monitor.py           # 5s polling, functional checks, auto-recovery
├── config_applier.py           # Read config.yaml, validate, rewrite system files
├── bt_profiles.py              # BT profile registration via dbus-next
├── templates/
│   ├── hostapd.conf.tmpl       # hostapd config template
│   └── 10-openauto-ap.network.tmpl  # systemd-networkd template
├── tests/
│   ├── test_ipc_server.py
│   ├── test_health_monitor.py
│   ├── test_config_applier.py
│   └── conftest.py             # Shared fixtures
├── requirements.txt            # dbus-next, PyYAML
└── openauto-system.service     # systemd unit file
```

---

### Task 1: Project Skeleton + IPC Server

**Files:**
- Create: `system-service/openauto_system.py`
- Create: `system-service/ipc_server.py`
- Create: `system-service/requirements.txt`
- Create: `system-service/tests/conftest.py`
- Create: `system-service/tests/test_ipc_server.py`

**Context:** The IPC server is the foundation — everything else plugs into it. It listens on a Unix socket, accepts newline-delimited JSON, dispatches to method handlers, and returns JSON responses. The pattern mirrors the existing `IpcServer.cpp` but in Python asyncio.

**Step 1: Write the failing tests**

```python
# system-service/tests/test_ipc_server.py
import asyncio
import json
import os
import tempfile
import pytest
from ipc_server import IpcServer

@pytest.fixture
def socket_path(tmp_path):
    return str(tmp_path / "test.sock")

@pytest.fixture
async def server(socket_path):
    srv = IpcServer(socket_path)
    await srv.start()
    yield srv
    await srv.stop()

@pytest.mark.asyncio
async def test_server_creates_socket(server, socket_path):
    assert os.path.exists(socket_path)

@pytest.mark.asyncio
async def test_ping_returns_pong(server, socket_path):
    reader, writer = await asyncio.open_unix_connection(socket_path)
    request = json.dumps({"id": "1", "method": "ping"}) + "\n"
    writer.write(request.encode())
    await writer.drain()
    line = await asyncio.wait_for(reader.readline(), timeout=2.0)
    response = json.loads(line)
    assert response["id"] == "1"
    assert response["result"] == "pong"
    writer.close()

@pytest.mark.asyncio
async def test_unknown_method_returns_error(server, socket_path):
    reader, writer = await asyncio.open_unix_connection(socket_path)
    request = json.dumps({"id": "2", "method": "nonexistent"}) + "\n"
    writer.write(request.encode())
    await writer.drain()
    line = await asyncio.wait_for(reader.readline(), timeout=2.0)
    response = json.loads(line)
    assert response["id"] == "2"
    assert "error" in response
    writer.close()

@pytest.mark.asyncio
async def test_malformed_json_returns_error(server, socket_path):
    reader, writer = await asyncio.open_unix_connection(socket_path)
    writer.write(b"not json\n")
    await writer.drain()
    line = await asyncio.wait_for(reader.readline(), timeout=2.0)
    response = json.loads(line)
    assert "error" in response
    writer.close()

@pytest.mark.asyncio
async def test_multiple_clients(server, socket_path):
    """Multiple clients can connect and get responses."""
    async def make_request(client_id):
        reader, writer = await asyncio.open_unix_connection(socket_path)
        request = json.dumps({"id": client_id, "method": "ping"}) + "\n"
        writer.write(request.encode())
        await writer.drain()
        line = await asyncio.wait_for(reader.readline(), timeout=2.0)
        writer.close()
        return json.loads(line)

    results = await asyncio.gather(make_request("a"), make_request("b"))
    assert results[0]["result"] == "pong"
    assert results[1]["result"] == "pong"
```

**Step 2: Run tests to verify they fail**

Run: `cd system-service && python -m pytest tests/test_ipc_server.py -v`
Expected: FAIL (ImportError — modules don't exist yet)

**Step 3: Write minimal implementation**

```python
# system-service/requirements.txt
dbus-next>=0.2.3
PyYAML>=6.0
pytest>=7.0
pytest-asyncio>=0.21
```

```python
# system-service/tests/conftest.py
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
```

```python
# system-service/ipc_server.py
import asyncio
import json
import logging
import os

log = logging.getLogger(__name__)


class IpcServer:
    def __init__(self, socket_path: str):
        self._socket_path = socket_path
        self._server = None
        self._methods: dict[str, callable] = {}
        self.register_method("ping", self._handle_ping)

    def register_method(self, name: str, handler):
        self._methods[name] = handler

    async def start(self):
        # Remove stale socket
        if os.path.exists(self._socket_path):
            os.unlink(self._socket_path)
        os.makedirs(os.path.dirname(self._socket_path), exist_ok=True)
        self._server = await asyncio.start_unix_server(
            self._handle_client, path=self._socket_path
        )
        # World-readable so the app (non-root) can connect
        os.chmod(self._socket_path, 0o666)
        log.info("IPC server listening on %s", self._socket_path)

    async def stop(self):
        if self._server:
            self._server.close()
            await self._server.wait_closed()
            self._server = None
        if os.path.exists(self._socket_path):
            os.unlink(self._socket_path)

    async def _handle_client(self, reader: asyncio.StreamReader,
                              writer: asyncio.StreamWriter):
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break
                response = await self._dispatch(line.decode().strip())
                writer.write(json.dumps(response).encode() + b"\n")
                await writer.drain()
        except Exception as e:
            log.warning("Client error: %s", e)
        finally:
            writer.close()

    async def _dispatch(self, raw: str) -> dict:
        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            return {"error": {"code": "PARSE_ERROR", "message": "Invalid JSON"}}

        msg_id = msg.get("id")
        method = msg.get("method")
        params = msg.get("params", {})

        if method not in self._methods:
            return {"id": msg_id, "error": {"code": "UNKNOWN_METHOD",
                    "message": f"Unknown method: {method}"}}

        try:
            result = await self._methods[method](params)
            return {"id": msg_id, "result": result}
        except Exception as e:
            log.exception("Method %s failed", method)
            return {"id": msg_id, "error": {"code": "INTERNAL_ERROR",
                    "message": str(e)}}

    async def _handle_ping(self, params: dict) -> str:
        return "pong"
```

```python
# system-service/openauto_system.py
#!/usr/bin/env python3
"""OpenAuto Prodigy System Service Daemon."""
import asyncio
import logging
import signal
import sys

from ipc_server import IpcServer

SOCKET_PATH = "/run/openauto/system.sock"

log = logging.getLogger("openauto-system")


async def main():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    )
    log.info("Starting openauto-system daemon")

    ipc = IpcServer(SOCKET_PATH)
    await ipc.start()

    # sd_notify READY=1 (if running under systemd)
    notify_socket = os.environ.get("NOTIFY_SOCKET")
    if notify_socket:
        import socket as sock
        s = sock.socket(sock.AF_UNIX, sock.SOCK_DGRAM)
        s.connect(notify_socket)
        s.send(b"READY=1")
        s.close()

    stop_event = asyncio.Event()

    def handle_signal():
        log.info("Received shutdown signal")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, handle_signal)

    await stop_event.wait()
    await ipc.stop()
    log.info("Shutdown complete")


if __name__ == "__main__":
    import os
    asyncio.run(main())
```

**Step 4: Run tests to verify they pass**

Run: `cd system-service && pip install -r requirements.txt && python -m pytest tests/test_ipc_server.py -v`
Expected: All 5 tests PASS

**Step 5: Commit**

```bash
git add system-service/
git commit -m "feat(system-service): IPC server with JSON-over-Unix-socket protocol"
```

---

### Task 2: Health Monitor

**Files:**
- Create: `system-service/health_monitor.py`
- Create: `system-service/tests/test_health_monitor.py`
- Modify: `system-service/openauto_system.py` — wire health monitor into main loop

**Context:** The health monitor polls 3 systemd units every 5s with functional checks (not just `is-active`). It auto-restarts failed services with backoff. The `get_health` IPC method exposes current state.

**Step 1: Write the failing tests**

```python
# system-service/tests/test_health_monitor.py
import asyncio
import pytest
from unittest.mock import AsyncMock, patch
from health_monitor import HealthMonitor, ServiceState


@pytest.mark.asyncio
async def test_initial_state_is_unknown():
    mon = HealthMonitor()
    health = mon.get_health()
    assert health["hostapd"]["state"] == "unknown"
    assert health["bluetooth"]["state"] == "unknown"
    assert health["networkd"]["state"] == "unknown"


@pytest.mark.asyncio
async def test_check_healthy_services():
    mon = HealthMonitor()
    with patch.object(mon, "_run_cmd", new_callable=AsyncMock) as mock_cmd:
        # systemctl is-active returns "active" for all
        # ip addr shows correct IP, hciconfig shows UP RUNNING
        async def fake_cmd(cmd, timeout=5):
            if "is-active" in cmd:
                return (0, "active\n")
            if "ip" in cmd and "addr" in cmd:
                return (0, "inet 10.0.0.1/24")
            if "hciconfig" in cmd:
                return (0, "UP RUNNING")
            return (0, "")
        mock_cmd.side_effect = fake_cmd

        await mon.check_once()
        health = mon.get_health()
        assert health["hostapd"]["state"] == "active"
        assert health["bluetooth"]["state"] == "active"
        assert health["networkd"]["state"] == "active"


@pytest.mark.asyncio
async def test_failed_service_triggers_restart():
    mon = HealthMonitor()
    restart_calls = []

    with patch.object(mon, "_run_cmd", new_callable=AsyncMock) as mock_cmd:
        async def fake_cmd(cmd, timeout=5):
            if "is-active" in cmd and "hostapd" in cmd:
                return (1, "inactive\n")
            if "is-active" in cmd:
                return (0, "active\n")
            if "restart" in cmd:
                restart_calls.append(cmd)
                return (0, "")
            if "ip" in cmd:
                return (0, "inet 10.0.0.1/24")
            if "hciconfig" in cmd:
                return (0, "UP RUNNING")
            return (0, "")
        mock_cmd.side_effect = fake_cmd

        await mon.check_once()
        assert len(restart_calls) == 1
        assert "hostapd" in restart_calls[0]


@pytest.mark.asyncio
async def test_retry_count_increments():
    mon = HealthMonitor()
    with patch.object(mon, "_run_cmd", new_callable=AsyncMock) as mock_cmd:
        async def fake_cmd(cmd, timeout=5):
            if "is-active" in cmd and "hostapd" in cmd:
                return (1, "failed\n")
            if "is-active" in cmd:
                return (0, "active\n")
            if "restart" in cmd:
                return (1, "failed")  # restart also fails
            if "ip" in cmd:
                return (0, "inet 10.0.0.1/24")
            if "hciconfig" in cmd:
                return (0, "UP RUNNING")
            return (0, "")
        mock_cmd.side_effect = fake_cmd

        await mon.check_once()
        await mon.check_once()
        health = mon.get_health()
        assert health["hostapd"]["retries"] >= 2
        assert health["hostapd"]["state"] == "failed"


@pytest.mark.asyncio
async def test_stability_resets_retries():
    mon = HealthMonitor()
    # Simulate: service was retried, now healthy for > stability window
    mon._services["hostapd"].retries = 2
    mon._services["hostapd"].last_healthy = asyncio.get_event_loop().time() - 200

    with patch.object(mon, "_run_cmd", new_callable=AsyncMock) as mock_cmd:
        async def fake_cmd(cmd, timeout=5):
            if "is-active" in cmd:
                return (0, "active\n")
            if "ip" in cmd:
                return (0, "inet 10.0.0.1/24")
            if "hciconfig" in cmd:
                return (0, "UP RUNNING")
            return (0, "")
        mock_cmd.side_effect = fake_cmd

        await mon.check_once()
        assert mon._services["hostapd"].retries == 0
```

**Step 2: Run tests to verify they fail**

Run: `cd system-service && python -m pytest tests/test_health_monitor.py -v`
Expected: FAIL (ImportError)

**Step 3: Write minimal implementation**

```python
# system-service/health_monitor.py
import asyncio
import logging
import time
from dataclasses import dataclass, field

log = logging.getLogger(__name__)

MAX_FAST_RETRIES = 3
FAST_RETRY_INTERVAL = 5.0
SLOW_RETRY_INTERVAL = 60.0
STABILITY_WINDOW = 120.0
CMD_TIMEOUT = 5


@dataclass
class ServiceInfo:
    name: str
    unit: str
    state: str = "unknown"
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
        """Called when bluetooth.service restarts (for BT profile re-registration)."""
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
            was_down = svc.state in ("failed", "inactive", "unknown")

            # Check systemd unit state
            rc, output = await self._run_cmd(
                f"systemctl is-active {svc.unit}", timeout=CMD_TIMEOUT
            )
            unit_active = output.strip() == "active"

            # Functional check
            functional = True
            if unit_active:
                functional = await self._functional_check(name)

            now = asyncio.get_event_loop().time()

            if unit_active and functional:
                svc.state = "active"
                svc.last_healthy = now
                # Reset retries after stability window
                if svc.retries > 0 and (now - svc.last_healthy) > STABILITY_WINDOW:
                    log.info("%s stable for %ds, resetting retries", name, STABILITY_WINDOW)
                    svc.retries = 0

                # Detect bluetooth recovery
                if name == "bluetooth" and was_down and self._on_bt_restart_callback:
                    log.info("bluetooth recovered, triggering BT profile registration")
                    asyncio.create_task(self._on_bt_restart_callback())
            else:
                svc.state = "failed" if rc != 0 else "degraded"
                await self._attempt_recovery(svc)

            svc.last_check = now

    async def _functional_check(self, name: str) -> bool:
        if name == "hostapd" or name == "networkd":
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
            if now - svc.last_check < SLOW_RETRY_INTERVAL:
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
```

Wire into main loop:

```python
# Add to openauto_system.py main():
from health_monitor import HealthMonitor

health = HealthMonitor()
ipc.register_method("get_health", lambda p: health.get_health())

health_task = asyncio.create_task(health.run())

# Before shutdown:
health_task.cancel()
```

**Step 4: Run tests to verify they pass**

Run: `cd system-service && python -m pytest tests/test_health_monitor.py -v`
Expected: All 5 tests PASS

**Step 5: Commit**

```bash
git add system-service/health_monitor.py system-service/tests/test_health_monitor.py
git commit -m "feat(system-service): health monitor with functional checks and auto-recovery"
```

---

### Task 3: Config Applier + Templates

**Files:**
- Create: `system-service/config_applier.py`
- Create: `system-service/templates/hostapd.conf.tmpl`
- Create: `system-service/templates/10-openauto-ap.network.tmpl`
- Create: `system-service/tests/test_config_applier.py`

**Context:** The config applier reads `~/.openauto/config.yaml`, validates values, rewrites system config files from templates, and restarts affected services. The existing YAML schema uses `connection.wifi_ap.ssid`, `connection.wifi_ap.password`, etc.

**Step 1: Write the failing tests**

```python
# system-service/tests/test_config_applier.py
import os
import tempfile
import pytest
import yaml
from unittest.mock import AsyncMock, patch
from config_applier import ConfigApplier, ConfigValidationError


@pytest.fixture
def config_dir(tmp_path):
    """Create a temp config.yaml."""
    config = {
        "connection": {
            "wifi_ap": {
                "ssid": "TestAP",
                "password": "testpass123",
                "channel": 36,
                "band": "a",
                "interface": "wlan0",
            },
            "tcp_port": 5277,
        },
        "network": {
            "ap_ip": "10.0.0.1",
            "dhcp_range_start": 10,
            "dhcp_range_end": 50,
        },
    }
    config_path = tmp_path / "config.yaml"
    config_path.write_text(yaml.dump(config))
    return tmp_path, str(config_path)


@pytest.fixture
def applier(config_dir, tmp_path):
    config_tmp, config_path = config_dir
    output_dir = tmp_path / "output"
    output_dir.mkdir()
    return ConfigApplier(
        config_path=config_path,
        hostapd_path=str(output_dir / "hostapd.conf"),
        networkd_path=str(output_dir / "10-openauto-ap.network"),
    )


def test_apply_wifi_writes_hostapd(applier):
    result = applier.apply_section("wifi")
    assert result["ok"] is True
    content = open(applier._hostapd_path).read()
    assert "ssid=TestAP" in content
    assert "wpa_passphrase=testpass123" in content
    assert "channel=36" in content
    assert "hw_mode=a" in content


def test_apply_network_writes_networkd(applier):
    result = applier.apply_section("network")
    assert result["ok"] is True
    content = open(applier._networkd_path).read()
    assert "Address=10.0.0.1/24" in content
    assert "PoolOffset=10" in content
    assert "PoolSize=40" in content


def test_validate_ssid_length():
    with pytest.raises(ConfigValidationError, match="SSID"):
        ConfigApplier._validate_wifi({"ssid": "", "password": "testpass1", "channel": 36, "band": "a"})
    with pytest.raises(ConfigValidationError, match="SSID"):
        ConfigApplier._validate_wifi({"ssid": "x" * 33, "password": "testpass1", "channel": 36, "band": "a"})


def test_validate_password_length():
    with pytest.raises(ConfigValidationError, match="password"):
        ConfigApplier._validate_wifi({"ssid": "Test", "password": "short", "channel": 36, "band": "a"})


def test_validate_ip_address():
    with pytest.raises(ConfigValidationError, match="IP"):
        ConfigApplier._validate_network({"ap_ip": "not.an.ip", "dhcp_range_start": 10, "dhcp_range_end": 50})


def test_unknown_section_returns_error(applier):
    result = applier.apply_section("nonexistent")
    assert result["ok"] is False
    assert "Unknown" in result["error"]
```

**Step 2: Run tests to verify they fail**

Run: `cd system-service && python -m pytest tests/test_config_applier.py -v`
Expected: FAIL (ImportError)

**Step 3: Write minimal implementation**

```
# system-service/templates/hostapd.conf.tmpl
interface={interface}
driver=nl80211
ssid={ssid}
hw_mode={band}
channel={channel}
ieee80211n=1
ieee80211ac=1
wmm_enabled=1
country_code={country_code}
ieee80211d=1
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase={password}
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
```

```
# system-service/templates/10-openauto-ap.network.tmpl
[Match]
Name={interface}

[Network]
Address={ap_ip}/24
DHCPServer=yes

[DHCPServer]
PoolOffset={dhcp_range_start}
PoolSize={dhcp_pool_size}
EmitDNS=no
```

```python
# system-service/config_applier.py
import ipaddress
import logging
import os
import tempfile
import yaml

log = logging.getLogger(__name__)

TEMPLATE_DIR = os.path.join(os.path.dirname(__file__), "templates")


class ConfigValidationError(Exception):
    pass


class ConfigApplier:
    def __init__(
        self,
        config_path: str,
        hostapd_path: str = "/etc/hostapd/hostapd.conf",
        networkd_path: str = "/etc/systemd/network/10-openauto-ap.network",
        country_code: str = "US",
    ):
        self._config_path = config_path
        self._hostapd_path = hostapd_path
        self._networkd_path = networkd_path
        self._country_code = country_code

    def _read_config(self) -> dict:
        with open(self._config_path, "r") as f:
            return yaml.safe_load(f) or {}

    def apply_section(self, section: str) -> dict:
        try:
            config = self._read_config()
            if section == "wifi":
                return self._apply_wifi(config)
            elif section == "network":
                return self._apply_network(config)
            else:
                return {"ok": False, "error": f"Unknown section: {section}"}
        except ConfigValidationError as e:
            return {"ok": False, "error": str(e)}
        except Exception as e:
            log.exception("Failed to apply config section %s", section)
            return {"ok": False, "error": str(e)}

    def _apply_wifi(self, config: dict) -> dict:
        wifi = config.get("connection", {}).get("wifi_ap", {})
        self._validate_wifi(wifi)

        template = self._read_template("hostapd.conf.tmpl")
        content = template.format(
            interface=wifi.get("interface", "wlan0"),
            ssid=wifi["ssid"],
            password=wifi["password"],
            channel=wifi.get("channel", 36),
            band=wifi.get("band", "a"),
            country_code=self._country_code,
        )
        self._atomic_write(self._hostapd_path, content)
        return {"ok": True, "restarted": ["hostapd"]}

    def _apply_network(self, config: dict) -> dict:
        net = config.get("network", {})
        wifi = config.get("connection", {}).get("wifi_ap", {})
        self._validate_network(net)

        pool_start = net.get("dhcp_range_start", 10)
        pool_end = net.get("dhcp_range_end", 50)

        template = self._read_template("10-openauto-ap.network.tmpl")
        content = template.format(
            interface=wifi.get("interface", "wlan0"),
            ap_ip=net.get("ap_ip", "10.0.0.1"),
            dhcp_range_start=pool_start,
            dhcp_pool_size=pool_end - pool_start,
        )
        self._atomic_write(self._networkd_path, content)
        return {"ok": True, "restarted": ["systemd-networkd"]}

    @staticmethod
    def _validate_wifi(wifi: dict):
        ssid = wifi.get("ssid", "")
        if not ssid or len(ssid) > 32:
            raise ConfigValidationError("SSID must be 1-32 characters")
        password = wifi.get("password", "")
        if len(password) < 8:
            raise ConfigValidationError("WPA2 password must be at least 8 characters")

    @staticmethod
    def _validate_network(net: dict):
        try:
            ipaddress.IPv4Address(net.get("ap_ip", ""))
        except (ipaddress.AddressValueError, ValueError):
            raise ConfigValidationError(f"Invalid IP address: {net.get('ap_ip')}")

    def _read_template(self, name: str) -> str:
        path = os.path.join(TEMPLATE_DIR, name)
        with open(path, "r") as f:
            return f.read()

    @staticmethod
    def _atomic_write(path: str, content: str):
        dir_name = os.path.dirname(path)
        fd, tmp_path = tempfile.mkstemp(dir=dir_name, suffix=".tmp")
        try:
            os.write(fd, content.encode())
            os.close(fd)
            os.rename(tmp_path, path)
        except Exception:
            os.close(fd)
            os.unlink(tmp_path)
            raise
```

**Step 4: Run tests to verify they pass**

Run: `cd system-service && python -m pytest tests/test_config_applier.py -v`
Expected: All 7 tests PASS

**Step 5: Commit**

```bash
git add system-service/config_applier.py system-service/templates/ system-service/tests/test_config_applier.py
git commit -m "feat(system-service): config applier with templates and validation"
```

---

### Task 4: BT Profile Registration

**Files:**
- Create: `system-service/bt_profiles.py`
- Create: `system-service/tests/test_bt_profiles.py`

**Context:** BT profile registration currently happens in the app's C++ init. We need to move it to the daemon. The daemon registers three profiles with BlueZ via D-Bus: HFP AG, HSP HS, and AA RFCOMM (channel 8). It also cleans stale SDP records. This uses `dbus-next` to call `org.bluez.ProfileManager1.RegisterProfile()`.

**UUIDs:**
- HFP AG: `0000111f-0000-1000-8000-00805f9b34fb`
- HSP HS: `00001108-0000-1000-8000-00805f9b34fb`
- AA RFCOMM: `4de17a00-52cb-11e6-bdf4-0800200c9a66`

**Step 1: Write the failing tests**

```python
# system-service/tests/test_bt_profiles.py
import pytest
from unittest.mock import AsyncMock, MagicMock, patch
from bt_profiles import BtProfileManager, HFP_AG_UUID, HSP_HS_UUID, AA_UUID


@pytest.mark.asyncio
async def test_profile_uuids_are_correct():
    assert HFP_AG_UUID == "0000111f-0000-1000-8000-00805f9b34fb"
    assert HSP_HS_UUID == "00001108-0000-1000-8000-00805f9b34fb"
    assert AA_UUID == "4de17a00-52cb-11e6-bdf4-0800200c9a66"


@pytest.mark.asyncio
async def test_register_profiles_calls_dbus():
    mgr = BtProfileManager()
    mock_proxy = AsyncMock()

    with patch.object(mgr, "_get_profile_manager", return_value=mock_proxy):
        await mgr.register_all()
        # Should register 3 profiles
        assert mock_proxy.call_register_profile.call_count == 3


@pytest.mark.asyncio
async def test_register_handles_already_registered():
    """If profiles are already registered, AlreadyExists error is ignored."""
    from dbus_next.errors import DBusError

    mgr = BtProfileManager()
    mock_proxy = AsyncMock()
    mock_proxy.call_register_profile.side_effect = DBusError(
        "org.bluez.Error.AlreadyExists", "Already exists"
    )

    with patch.object(mgr, "_get_profile_manager", return_value=mock_proxy):
        # Should not raise
        await mgr.register_all()


@pytest.mark.asyncio
async def test_register_propagates_other_errors():
    from dbus_next.errors import DBusError

    mgr = BtProfileManager()
    mock_proxy = AsyncMock()
    mock_proxy.call_register_profile.side_effect = DBusError(
        "org.bluez.Error.Failed", "Something went wrong"
    )

    with patch.object(mgr, "_get_profile_manager", return_value=mock_proxy):
        with pytest.raises(DBusError):
            await mgr.register_all()
```

**Step 2: Run tests to verify they fail**

Run: `cd system-service && python -m pytest tests/test_bt_profiles.py -v`
Expected: FAIL (ImportError)

**Step 3: Write minimal implementation**

```python
# system-service/bt_profiles.py
import asyncio
import logging

log = logging.getLogger(__name__)

HFP_AG_UUID = "0000111f-0000-1000-8000-00805f9b34fb"
HSP_HS_UUID = "00001108-0000-1000-8000-00805f9b34fb"
AA_UUID = "4de17a00-52cb-11e6-bdf4-0800200c9a66"

PROFILES = [
    {
        "uuid": HFP_AG_UUID,
        "path": "/org/openauto/hfp_ag",
        "options": {
            "Name": "Hands-Free AG",
            "Role": "server",
            "RequireAuthentication": False,
            "RequireAuthorization": False,
        },
    },
    {
        "uuid": HSP_HS_UUID,
        "path": "/org/openauto/hsp_hs",
        "options": {
            "Name": "Headset",
            "Role": "server",
            "RequireAuthentication": False,
            "RequireAuthorization": False,
        },
    },
    {
        "uuid": AA_UUID,
        "path": "/org/openauto/aa_wireless",
        "options": {
            "Name": "Android Auto Wireless",
            "Role": "server",
            "Channel": 8,
            "RequireAuthentication": False,
            "RequireAuthorization": False,
        },
    },
]


class BtProfileManager:
    def __init__(self):
        self._bus = None

    async def _get_profile_manager(self):
        """Get BlueZ ProfileManager1 D-Bus proxy."""
        from dbus_next.aio import MessageBus

        if self._bus is None:
            self._bus = await MessageBus(bus_type=BusType.SYSTEM).connect()

        introspection = await self._bus.introspect("org.bluez", "/org/bluez")
        proxy = self._bus.get_proxy_object("org.bluez", "/org/bluez", introspection)
        return proxy.get_interface("org.bluez.ProfileManager1")

    async def register_all(self):
        """Register all BT profiles with BlueZ."""
        from dbus_next.errors import DBusError
        from dbus_next import Variant

        profile_mgr = await self._get_profile_manager()

        for profile in PROFILES:
            # Convert options to D-Bus Variant dict
            options = {}
            for key, value in profile["options"].items():
                if isinstance(value, bool):
                    options[key] = Variant("b", value)
                elif isinstance(value, int):
                    options[key] = Variant("q", value)  # uint16
                elif isinstance(value, str):
                    options[key] = Variant("s", value)

            try:
                await profile_mgr.call_register_profile(
                    profile["path"], profile["uuid"], options
                )
                log.info("Registered BT profile: %s (%s)", profile["options"]["Name"], profile["uuid"])
            except DBusError as e:
                if "AlreadyExists" in str(e):
                    log.info("BT profile already registered: %s", profile["options"]["Name"])
                else:
                    log.error("Failed to register %s: %s", profile["options"]["Name"], e)
                    raise

    async def close(self):
        if self._bus:
            self._bus.disconnect()
            self._bus = None
```

**Note:** The `_get_profile_manager` import of `BusType` needs fixing: add `from dbus_next import BusType` at usage. The tests mock this method entirely so they pass without a real D-Bus connection.

**Step 4: Run tests to verify they pass**

Run: `cd system-service && python -m pytest tests/test_bt_profiles.py -v`
Expected: All 4 tests PASS

**Step 5: Commit**

```bash
git add system-service/bt_profiles.py system-service/tests/test_bt_profiles.py
git commit -m "feat(system-service): BT profile registration via dbus-next"
```

---

### Task 5: Wire Everything Together + Systemd Unit

**Files:**
- Modify: `system-service/openauto_system.py` — integrate health monitor, config applier, BT profiles
- Create: `system-service/openauto-system.service` — systemd unit file
- Modify: `system-service/tests/test_ipc_server.py` — add integration test for `get_health` and `apply_config`

**Context:** The main entry point needs to wire all components: start IPC server, register method handlers for health/config/restart, start health monitor loop, trigger BT profile registration on boot, and handle graceful shutdown.

**Step 1: Write the failing test**

```python
# Add to system-service/tests/test_ipc_server.py

@pytest.mark.asyncio
async def test_get_health_method(socket_path):
    """Integration: get_health returns health data via IPC."""
    from health_monitor import HealthMonitor
    from unittest.mock import AsyncMock, patch

    srv = IpcServer(socket_path)
    health = HealthMonitor()
    srv.register_method("get_health", lambda p: health.get_health())
    await srv.start()

    try:
        reader, writer = await asyncio.open_unix_connection(socket_path)
        request = json.dumps({"id": "h1", "method": "get_health"}) + "\n"
        writer.write(request.encode())
        await writer.drain()
        line = await asyncio.wait_for(reader.readline(), timeout=2.0)
        response = json.loads(line)
        assert response["id"] == "h1"
        assert "hostapd" in response["result"]
        assert "bluetooth" in response["result"]
        assert "networkd" in response["result"]
        writer.close()
    finally:
        await srv.stop()
```

**Step 2: Run test to verify it fails**

Run: `cd system-service && python -m pytest tests/test_ipc_server.py::test_get_health_method -v`
Expected: FAIL (test doesn't exist yet or wiring issue)

**Step 3: Update openauto_system.py with full wiring**

```python
# system-service/openauto_system.py
#!/usr/bin/env python3
"""OpenAuto Prodigy System Service Daemon."""
import asyncio
import logging
import os
import signal
import sys

from ipc_server import IpcServer
from health_monitor import HealthMonitor
from config_applier import ConfigApplier

SOCKET_PATH = "/run/openauto/system.sock"
CONFIG_PATH = os.path.expanduser("~matt/.openauto/config.yaml")

log = logging.getLogger("openauto-system")


async def main():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    )
    log.info("Starting openauto-system daemon")

    # --- Components ---
    ipc = IpcServer(SOCKET_PATH)
    health = HealthMonitor()
    config = ConfigApplier(config_path=CONFIG_PATH)

    # BT profiles (optional — only if dbus-next available)
    bt = None
    try:
        from bt_profiles import BtProfileManager
        bt = BtProfileManager()
        health.set_bt_restart_callback(bt.register_all)
    except ImportError:
        log.warning("dbus-next not available, BT profile registration disabled")

    # --- IPC method handlers ---
    async def handle_get_health(params):
        return health.get_health()

    async def handle_get_status(params):
        import time
        return {
            "health": health.get_health(),
            "uptime": time.monotonic(),
            "version": "0.1.0",
        }

    async def handle_apply_config(params):
        section = params.get("section", "")
        result = config.apply_section(section)
        # If config wrote files that need a service restart, do it
        if result.get("ok") and result.get("restarted"):
            for svc in result["restarted"]:
                rc, out = await health._run_cmd(f"systemctl restart {svc}")
                if rc != 0:
                    result["ok"] = False
                    result["error"] = f"Failed to restart {svc}: {out}"
                    break
        return result

    async def handle_restart_service(params):
        name = params.get("name", "")
        allowed = {"hostapd", "bluetooth", "systemd-networkd"}
        if name not in allowed:
            return {"ok": False, "error": f"Unknown service: {name}"}
        rc, out = await health._run_cmd(f"systemctl restart {name}")
        return {"ok": rc == 0, "output": out.strip()}

    ipc.register_method("get_health", handle_get_health)
    ipc.register_method("get_status", handle_get_status)
    ipc.register_method("apply_config", handle_apply_config)
    ipc.register_method("restart_service", handle_restart_service)

    # --- Start ---
    await ipc.start()

    # Initial health check
    log.info("Running initial health check...")
    await health.check_once()
    log.info("Initial health: %s", health.get_health())

    # Register BT profiles on boot
    if bt:
        try:
            await bt.register_all()
            log.info("BT profiles registered")
        except Exception as e:
            log.error("BT profile registration failed: %s", e)

    # sd_notify READY=1
    notify_socket = os.environ.get("NOTIFY_SOCKET")
    if notify_socket:
        import socket as sock
        s = sock.socket(sock.AF_UNIX, sock.SOCK_DGRAM)
        s.connect(notify_socket)
        s.send(b"READY=1")
        s.close()
        log.info("Notified systemd: READY")

    # --- Run ---
    stop_event = asyncio.Event()

    def handle_signal():
        log.info("Received shutdown signal")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, handle_signal)

    health_task = asyncio.create_task(health.run())

    await stop_event.wait()

    # --- Shutdown ---
    health_task.cancel()
    try:
        await health_task
    except asyncio.CancelledError:
        pass

    if bt:
        await bt.close()
    await ipc.stop()
    log.info("Shutdown complete")


if __name__ == "__main__":
    asyncio.run(main())
```

Create systemd unit:

```ini
# system-service/openauto-system.service
[Unit]
Description=OpenAuto Prodigy System Manager
Before=openauto-prodigy.service
After=network.target bluetooth.target

[Service]
Type=notify
ExecStart=/usr/bin/python3 /home/matt/openauto-prodigy/system-service/openauto_system.py
RuntimeDirectory=openauto
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

**Step 4: Run all tests**

Run: `cd system-service && python -m pytest tests/ -v`
Expected: All tests PASS

**Step 5: Commit**

```bash
git add system-service/openauto_system.py system-service/openauto-system.service system-service/tests/
git commit -m "feat(system-service): wire all components, add systemd unit"
```

---

### Task 6: SystemServiceClient (Qt App Side)

**Files:**
- Create: `src/core/services/SystemServiceClient.hpp`
- Create: `src/core/services/SystemServiceClient.cpp`
- Modify: `src/main.cpp` — instantiate and expose to QML
- Modify: `src/CMakeLists.txt` — add new source files

**Context:** Lightweight Qt class that connects to `/run/openauto/system.sock`, sends JSON requests, and emits signals with responses. Mirrors the pattern in `web-config/server.py` but in C++ with `QLocalSocket`.

**Step 1: Write the header**

```cpp
// src/core/services/SystemServiceClient.hpp
#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>
#include <QMap>
#include <functional>

namespace oap {

class SystemServiceClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QJsonObject health READ health NOTIFY healthChanged)

public:
    explicit SystemServiceClient(QObject* parent = nullptr);

    bool isConnected() const;
    QJsonObject health() const;

    Q_INVOKABLE void getHealth();
    Q_INVOKABLE void applyConfig(const QString& section);
    Q_INVOKABLE void restartService(const QString& name);
    Q_INVOKABLE void getStatus();

signals:
    void connectedChanged();
    void healthChanged();
    void configApplied(const QString& section, bool ok, const QString& error);
    void serviceRestarted(const QString& name, bool ok);
    void statusReceived(const QJsonObject& status);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError error);

private:
    void connectToService();
    void sendRequest(const QString& method, const QJsonObject& params = {});
    void handleResponse(const QJsonObject& response);

    QLocalSocket* socket_ = nullptr;
    QByteArray readBuffer_;
    QJsonObject health_;
    int nextId_ = 1;
    QMap<QString, QString> pendingMethods_;  // id -> method name
};

} // namespace oap
```

**Step 2: Write the implementation**

```cpp
// src/core/services/SystemServiceClient.cpp
#include "SystemServiceClient.hpp"
#include <QJsonDocument>
#include <QTimer>

namespace oap {

static const QString SOCKET_PATH = QStringLiteral("/run/openauto/system.sock");

SystemServiceClient::SystemServiceClient(QObject* parent)
    : QObject(parent)
{
    socket_ = new QLocalSocket(this);
    connect(socket_, &QLocalSocket::connected, this, &SystemServiceClient::onConnected);
    connect(socket_, &QLocalSocket::disconnected, this, &SystemServiceClient::onDisconnected);
    connect(socket_, &QLocalSocket::readyRead, this, &SystemServiceClient::onReadyRead);
    connect(socket_, &QLocalSocket::errorOccurred, this, &SystemServiceClient::onError);

    connectToService();
}

bool SystemServiceClient::isConnected() const
{
    return socket_->state() == QLocalSocket::ConnectedState;
}

QJsonObject SystemServiceClient::health() const { return health_; }

void SystemServiceClient::getHealth() { sendRequest("get_health"); }
void SystemServiceClient::getStatus() { sendRequest("get_status"); }

void SystemServiceClient::applyConfig(const QString& section)
{
    QJsonObject params;
    params["section"] = section;
    sendRequest("apply_config", params);
}

void SystemServiceClient::restartService(const QString& name)
{
    QJsonObject params;
    params["name"] = name;
    sendRequest("restart_service", params);
}

void SystemServiceClient::connectToService()
{
    if (socket_->state() != QLocalSocket::UnconnectedState)
        return;
    socket_->connectToServer(SOCKET_PATH);
}

void SystemServiceClient::onConnected()
{
    qInfo() << "SystemServiceClient: connected to daemon";
    emit connectedChanged();
    getHealth();
}

void SystemServiceClient::onDisconnected()
{
    qInfo() << "SystemServiceClient: disconnected from daemon";
    emit connectedChanged();
    // Retry after 5 seconds
    QTimer::singleShot(5000, this, &SystemServiceClient::connectToService);
}

void SystemServiceClient::onError(QLocalSocket::LocalSocketError error)
{
    if (error == QLocalSocket::ServerNotFoundError ||
        error == QLocalSocket::ConnectionRefusedError) {
        // Daemon not running yet, retry
        QTimer::singleShot(5000, this, &SystemServiceClient::connectToService);
    }
}

void SystemServiceClient::onReadyRead()
{
    readBuffer_ += socket_->readAll();
    while (true) {
        int idx = readBuffer_.indexOf('\n');
        if (idx < 0) break;
        QByteArray line = readBuffer_.left(idx);
        readBuffer_ = readBuffer_.mid(idx + 1);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error == QJsonParseError::NoError)
            handleResponse(doc.object());
    }
}

void SystemServiceClient::sendRequest(const QString& method, const QJsonObject& params)
{
    if (!isConnected()) return;

    QString id = QString::number(nextId_++);
    pendingMethods_[id] = method;

    QJsonObject msg;
    msg["id"] = id;
    msg["method"] = method;
    if (!params.isEmpty())
        msg["params"] = params;

    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
    socket_->write(data);
    socket_->flush();
}

void SystemServiceClient::handleResponse(const QJsonObject& response)
{
    QString id = response["id"].toString();
    QString method = pendingMethods_.take(id);

    if (method == "get_health") {
        health_ = response["result"].toObject();
        emit healthChanged();
    } else if (method == "apply_config") {
        QJsonObject result = response.contains("error")
            ? response["error"].toObject()
            : response["result"].toObject();
        bool ok = result["ok"].toBool();
        QString error = result.value("message").toString(result.value("error").toString());
        emit configApplied(method, ok, error);
    } else if (method == "restart_service") {
        QJsonObject result = response["result"].toObject();
        emit serviceRestarted(result["name"].toString(), result["ok"].toBool());
    } else if (method == "get_status") {
        emit statusReceived(response["result"].toObject());
    }
}

} // namespace oap
```

**Step 3: Wire into main.cpp**

Add to `src/main.cpp` after the companion listener setup:

```cpp
#include "core/services/SystemServiceClient.hpp"
// ...
auto* systemClient = new oap::SystemServiceClient(&app);
engine.rootContext()->setContextProperty("SystemService", systemClient);
```

Add to `src/CMakeLists.txt` source list:

```
core/services/SystemServiceClient.cpp
```

**Step 4: Build and verify**

Run: `cd build && cmake .. && cmake --build . -j$(nproc)`
Expected: Compiles with no errors

**Step 5: Commit**

```bash
git add src/core/services/SystemServiceClient.hpp src/core/services/SystemServiceClient.cpp src/CMakeLists.txt src/main.cpp
git commit -m "feat: SystemServiceClient for IPC with system daemon"
```

---

### Task 7: Deploy and Test on Pi

**Files:**
- Modify: `install.sh` — add system service installation section

**Context:** Deploy the daemon to the Pi, install the systemd unit, verify health monitoring works, verify IPC from the app works.

**Step 1: Install Python dependencies on Pi**

```bash
ssh matt@192.168.1.152 "pip3 install --break-system-packages dbus-next pyyaml"
```

**Step 2: Copy daemon files to Pi**

```bash
rsync -av system-service/ matt@192.168.1.152:~/openauto-prodigy/system-service/
```

**Step 3: Install systemd unit**

```bash
ssh matt@192.168.1.152 "sudo cp ~/openauto-prodigy/system-service/openauto-system.service /etc/systemd/system/ && sudo systemctl daemon-reload && sudo systemctl enable openauto-system && sudo systemctl start openauto-system"
```

**Step 4: Verify daemon is running**

```bash
ssh matt@192.168.1.152 "systemctl status openauto-system"
```
Expected: Active (running), "Notified systemd: READY"

**Step 5: Test IPC manually**

```bash
ssh matt@192.168.1.152 'echo "{\"id\":\"1\",\"method\":\"get_health\"}" | socat - UNIX-CONNECT:/run/openauto/system.sock'
```
Expected: JSON response with hostapd/bluetooth/networkd states

**Step 6: Cross-compile and deploy updated app**

```bash
cd build-pi && cmake --build . -j$(nproc)
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
```

**Step 7: Restart app and check logs**

```bash
ssh matt@192.168.1.152 "sudo systemctl restart openauto-prodigy && journalctl -u openauto-prodigy -n 20 --no-pager"
```
Expected: "SystemServiceClient: connected to daemon" in logs

**Step 8: Add install.sh section for system service**

Add to `install.sh` after the web config service section:

```bash
# System service daemon
echo "Installing system service daemon..."
sudo cp "$INSTALL_DIR/system-service/openauto-system.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable openauto-system
sudo systemctl start openauto-system
```

**Step 9: Commit**

```bash
git add install.sh
git commit -m "feat: deploy system service daemon in install script"
```
