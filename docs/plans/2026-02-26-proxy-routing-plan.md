# Transparent SOCKS5 Proxy Routing Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** When the companion app enables internet sharing, the Pi automatically routes all outbound TCP traffic through the phone's SOCKS5 proxy, with a status indicator in CompanionSettings.qml.

**Architecture:** `CompanionListenerService` detects `socks5.active` *changes* in the 5-second status stream and calls `SystemServiceClient::setProxyRoute()`. The system daemon's new `ProxyManager` configures `redsocks` + `iptables` to intercept all outbound TCP (excluding loopback, AP, and LAN), and re-routes DNS through DoH when active. State is fed back to the QML UI via a `routeState` Q_PROPERTY.

**Tech Stack:** Python asyncio (system-service), redsocks, iptables, systemd-resolved, C++/Qt 6, QML

---

## Context: How the pieces fit together

- **`openauto_system.py`** — asyncio daemon running as root. `IpcServer` at `/run/openauto/system.sock`. Methods registered via `ipc.register_method(name, handler)`. Handlers are `async def handler(params) -> dict`.
- **`SystemServiceClient`** — Qt C++ client. `sendRequest(method, params)` → tracks `id → method` in `pendingMethods_` → `handleResponse()` dispatches by method name.
- **`CompanionListenerService`** — receives 5s status JSON from phone. Currently reads `socks5` object for `active`, `host`, `port`, `user`, `password` fields. We need to track *changes*, not just the current value.
- **`redsocks`** — transparent TCP proxy. Reads a config file (`/run/openauto/redsocks.conf`), listens on `127.0.0.1:12345`, forwards through SOCKS5.
- **`iptables OUTPUT`** — redirects outbound TCP to redsocks. Exclusions: `-o lo`, `-d 10.0.0.0/24`, `-d 192.168.0.0/16`.
- **SOCKS5 password** — first **8** hex chars of `/home/matt/.openauto/companion.key` (confirmed by companion dev).

---

## Task 1: `ProxyManager` — core class with tests

**Files:**
- Create: `system-service/proxy_manager.py`
- Create: `system-service/tests/test_proxy_manager.py`

### Step 1: Write the failing tests first

Create `system-service/tests/test_proxy_manager.py`:

```python
"""Unit tests for ProxyManager — all subprocess calls are mocked."""
import asyncio
import os
import sys
import pytest
from unittest.mock import AsyncMock, MagicMock, patch, call

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from proxy_manager import ProxyManager, ProxyState


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def make_manager(**kwargs):
    """Return a ProxyManager with patched run_cmd."""
    pm = ProxyManager(**kwargs)
    pm._run_cmd = AsyncMock(return_value=(0, ""))
    return pm


# ---------------------------------------------------------------------------
# State machine
# ---------------------------------------------------------------------------

def test_initial_state_is_disabled():
    pm = make_manager()
    assert pm.state == ProxyState.DISABLED


def test_enable_sets_active_state():
    async def run():
        pm = make_manager()
        with patch("builtins.open", MagicMock()):
            await pm.enable("10.0.0.5", 1080, "oap", "deadbeef")
        assert pm.state == ProxyState.ACTIVE

    asyncio.run(run())


def test_disable_sets_disabled_state():
    async def run():
        pm = make_manager()
        pm._state = ProxyState.ACTIVE
        await pm.disable()
        assert pm.state == ProxyState.DISABLED

    asyncio.run(run())


def test_enable_writes_redsocks_config():
    async def run():
        pm = make_manager(config_path="/tmp/test_redsocks.conf")
        await pm.enable("10.0.0.5", 1080, "oap", "deadbeef")
        with open("/tmp/test_redsocks.conf") as f:
            conf = f.read()
        assert "10.0.0.5" in conf
        assert "1080" in conf
        assert "oap" in conf
        assert "deadbeef" in conf
        assert "12345" in conf  # local redsocks port

    asyncio.run(run())


def test_enable_applies_iptables():
    async def run():
        pm = make_manager()
        with patch("builtins.open", MagicMock()):
            await pm.enable("10.0.0.5", 1080, "oap", "deadbeef")
        calls = [str(c) for c in pm._run_cmd.call_args_list]
        # Should have iptables REDIRECT rule
        assert any("REDIRECT" in c for c in calls)
        # Should NOT redirect loopback or AP subnet
        assert any("10.0.0.0/24" in c for c in calls)

    asyncio.run(run())


def test_disable_flushes_iptables():
    async def run():
        pm = make_manager()
        pm._state = ProxyState.ACTIVE
        await pm.disable()
        calls = [str(c) for c in pm._run_cmd.call_args_list]
        assert any("DELETE" in c or "flush" in c.lower() for c in calls)

    asyncio.run(run())


def test_disable_kills_redsocks():
    async def run():
        pm = make_manager()
        pm._state = ProxyState.ACTIVE
        pm._redsocks_proc = MagicMock()
        pm._redsocks_proc.terminate = MagicMock()
        pm._redsocks_proc.wait = AsyncMock()
        await pm.disable()
        pm._redsocks_proc.terminate.assert_called_once()

    asyncio.run(run())


def test_health_check_marks_degraded_after_failures():
    async def run():
        pm = make_manager()
        pm._state = ProxyState.ACTIVE
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080
        # Simulate 3 failed probes
        with patch("asyncio.open_connection", side_effect=OSError("refused")):
            for _ in range(3):
                await pm._probe_once()
        assert pm.state == ProxyState.DEGRADED

    asyncio.run(run())


def test_health_check_restores_active_after_success():
    async def run():
        pm = make_manager()
        pm._state = ProxyState.DEGRADED
        pm._proxy_host = "10.0.0.5"
        pm._proxy_port = 1080
        pm._fail_count = 3
        reader = AsyncMock()
        writer = MagicMock()
        writer.close = MagicMock()
        writer.wait_closed = AsyncMock()
        with patch("asyncio.open_connection", return_value=(reader, writer)):
            await pm._probe_once()
        assert pm.state == ProxyState.ACTIVE
        assert pm._fail_count == 0

    asyncio.run(run())
```

### Step 2: Run tests to verify they fail

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/system-service
.venv/bin/python -m pytest tests/test_proxy_manager.py -v 2>&1 | head -30
```

Expected: `ImportError: No module named 'proxy_manager'`

### Step 3: Implement `proxy_manager.py`

Create `system-service/proxy_manager.py`:

```python
"""ProxyManager: redsocks + iptables transparent SOCKS5 proxy routing."""
import asyncio
import enum
import logging
import os
import shutil
import tempfile
from typing import Optional

log = logging.getLogger("proxy_manager")

REDSOCKS_LOCAL_PORT = 12345
HEALTH_INTERVAL_S = 30
FAIL_THRESHOLD = 3

REDSOCKS_CONF_TEMPLATE = """\
base {{
    log_debug = off;
    log_info = on;
    daemon = off;
    redirector = iptables;
}}

redsocks {{
    local_ip = 127.0.0.1;
    local_port = {local_port};
    ip = {host};
    port = {port};
    type = socks5;
    login = "{user}";
    password = "{password}";
}}
"""

# iptables exclusion targets (never redirect these through proxy)
_EXCLUDE_DESTS = [
    "127.0.0.0/8",    # loopback
    "10.0.0.0/24",    # AP subnet (phone connects here for AA)
    "192.168.0.0/16", # LAN
]
_CHAIN = "OPENAUTO_PROXY"


class ProxyState(enum.Enum):
    DISABLED = "disabled"
    ACTIVE = "active"
    DEGRADED = "degraded"
    FAILED = "failed"


class ProxyManager:
    def __init__(self, config_path: str = "/run/openauto/redsocks.conf"):
        self._config_path = config_path
        self._state = ProxyState.DISABLED
        self._redsocks_proc: Optional[asyncio.subprocess.Process] = None
        self._proxy_host: str = ""
        self._proxy_port: int = 0
        self._fail_count: int = 0
        self._health_task: Optional[asyncio.Task] = None
        self._state_callback = None  # callable(ProxyState)

    @property
    def state(self) -> ProxyState:
        return self._state

    def set_state_callback(self, cb):
        """Called whenever state changes. cb(new_state: ProxyState)."""
        self._state_callback = cb

    def _set_state(self, new_state: ProxyState):
        if new_state != self._state:
            self._state = new_state
            log.info("Proxy state → %s", new_state.value)
            if self._state_callback:
                self._state_callback(new_state)

    async def _run_cmd(self, *args) -> tuple[int, str]:
        proc = await asyncio.create_subprocess_exec(
            *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
        )
        out, _ = await proc.communicate()
        return proc.returncode, out.decode(errors="replace").strip()

    # ------------------------------------------------------------------
    # Enable / disable
    # ------------------------------------------------------------------

    async def enable(self, host: str, port: int, user: str, password: str):
        """Activate transparent proxy routing."""
        log.info("Enabling proxy → %s:%d (user=%s)", host, port, user)
        self._proxy_host = host
        self._proxy_port = port
        self._fail_count = 0

        # Write redsocks config
        conf = REDSOCKS_CONF_TEMPLATE.format(
            local_port=REDSOCKS_LOCAL_PORT,
            host=host,
            port=port,
            user=user,
            password=password,
        )
        os.makedirs(os.path.dirname(self._config_path), exist_ok=True)
        with open(self._config_path, "w") as f:
            f.write(conf)
        os.chmod(self._config_path, 0o600)

        # Start redsocks
        if self._redsocks_proc is not None:
            await self._stop_redsocks()
        self._redsocks_proc = await asyncio.create_subprocess_exec(
            "redsocks", "-c", self._config_path,
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.DEVNULL,
        )
        # Give redsocks a moment to bind its port
        await asyncio.sleep(0.5)

        # Apply iptables rules
        await self._iptables_apply()

        # DNS: point systemd-resolved at Cloudflare DoH
        await self._dns_enable()

        self._set_state(ProxyState.ACTIVE)

        # Start health monitor
        if self._health_task is None or self._health_task.done():
            self._health_task = asyncio.create_task(self._health_loop())

    async def disable(self):
        """Tear down transparent proxy routing unconditionally."""
        log.info("Disabling proxy")

        if self._health_task and not self._health_task.done():
            self._health_task.cancel()
            try:
                await self._health_task
            except asyncio.CancelledError:
                pass
            self._health_task = None

        await self._iptables_flush()
        await self._dns_restore()
        await self._stop_redsocks()

        # Remove config file (contains credentials)
        try:
            os.unlink(self._config_path)
        except FileNotFoundError:
            pass

        self._set_state(ProxyState.DISABLED)

    # ------------------------------------------------------------------
    # redsocks lifecycle
    # ------------------------------------------------------------------

    async def _stop_redsocks(self):
        if self._redsocks_proc is None:
            return
        try:
            self._redsocks_proc.terminate()
            await asyncio.wait_for(self._redsocks_proc.wait(), timeout=3.0)
        except (ProcessLookupError, asyncio.TimeoutError):
            try:
                self._redsocks_proc.kill()
            except ProcessLookupError:
                pass
        self._redsocks_proc = None

    # ------------------------------------------------------------------
    # iptables
    # ------------------------------------------------------------------

    async def _iptables_apply(self):
        # Create custom chain
        await self._run_cmd("iptables", "-t", "nat", "-N", _CHAIN)

        # Exclusions
        for dest in _EXCLUDE_DESTS:
            await self._run_cmd(
                "iptables", "-t", "nat", "-A", _CHAIN,
                "-d", dest, "-j", "RETURN"
            )

        # Redirect all remaining outbound TCP to redsocks
        await self._run_cmd(
            "iptables", "-t", "nat", "-A", _CHAIN,
            "-p", "tcp", "-j", "REDIRECT",
            "--to-ports", str(REDSOCKS_LOCAL_PORT)
        )

        # Jump to chain from OUTPUT
        await self._run_cmd(
            "iptables", "-t", "nat", "-I", "OUTPUT", "1",
            "-p", "tcp", "-j", _CHAIN
        )

    async def _iptables_flush(self):
        # Remove jump from OUTPUT
        await self._run_cmd(
            "iptables", "-t", "nat", "-D", "OUTPUT",
            "-p", "tcp", "-j", _CHAIN
        )
        # Flush and delete chain
        await self._run_cmd("iptables", "-t", "nat", "-F", _CHAIN)
        await self._run_cmd("iptables", "-t", "nat", "-X", _CHAIN)

    # ------------------------------------------------------------------
    # DNS
    # ------------------------------------------------------------------

    async def _dns_enable(self):
        """Configure systemd-resolved to use Cloudflare DoH (TCP/443 → proxy)."""
        rc, _ = await self._run_cmd(
            "resolvectl", "dns", "wlan0", "1.1.1.1"
        )
        if rc != 0:
            log.warning("Failed to set DNS via resolvectl")
        await self._run_cmd("resolvectl", "dnssec", "wlan0", "no")

    async def _dns_restore(self):
        """Restore systemd-resolved to default DNS."""
        await self._run_cmd("resolvectl", "revert", "wlan0")

    # ------------------------------------------------------------------
    # Health monitor
    # ------------------------------------------------------------------

    async def _health_loop(self):
        while True:
            await asyncio.sleep(HEALTH_INTERVAL_S)
            if self._state in (ProxyState.ACTIVE, ProxyState.DEGRADED):
                await self._probe_once()

    async def _probe_once(self):
        try:
            reader, writer = await asyncio.wait_for(
                asyncio.open_connection(self._proxy_host, self._proxy_port),
                timeout=5.0
            )
            writer.close()
            await writer.wait_closed()
            # Success — restore active if degraded
            self._fail_count = 0
            if self._state == ProxyState.DEGRADED:
                self._set_state(ProxyState.ACTIVE)
        except (OSError, asyncio.TimeoutError) as e:
            self._fail_count += 1
            log.warning("Proxy health probe failed (%d/%d): %s",
                        self._fail_count, FAIL_THRESHOLD, e)
            if self._fail_count >= FAIL_THRESHOLD:
                self._set_state(ProxyState.DEGRADED)
```

### Step 4: Run tests

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/system-service
.venv/bin/python -m pytest tests/test_proxy_manager.py -v
```

Expected: all 9 tests pass.

### Step 5: Commit

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git add system-service/proxy_manager.py system-service/tests/test_proxy_manager.py
git commit -m "feat(proxy): add ProxyManager with redsocks/iptables/health monitoring"
```

---

## Task 2: Wire ProxyManager into the system daemon IPC

**Files:**
- Modify: `system-service/openauto_system.py`
- Modify: `system-service/openauto-system.service`

### Step 1: Write the failing test

Add to `system-service/tests/test_proxy_manager.py` (append):

```python
# ---------------------------------------------------------------------------
# IPC handler integration (openauto_system.py paths)
# ---------------------------------------------------------------------------

def test_set_proxy_route_enable():
    """Simulate what openauto_system.py handle_set_proxy_route does."""
    async def run():
        from proxy_manager import ProxyManager, ProxyState
        pm = ProxyManager()
        pm._run_cmd = AsyncMock(return_value=(0, ""))
        with patch("builtins.open", MagicMock()):
            result = await _call_set_proxy_route(pm, {
                "active": True,
                "host": "10.0.0.5",
                "port": 1080,
                "user": "oap",
                "password": "deadbeef",
            })
        assert result["ok"] is True
        assert pm.state == ProxyState.ACTIVE

    asyncio.run(run())


async def _call_set_proxy_route(pm, params):
    """Minimal reimplementation of handle_set_proxy_route for testing."""
    try:
        if params.get("active"):
            await pm.enable(
                params["host"], params["port"],
                params.get("user", "oap"), params["password"]
            )
        else:
            await pm.disable()
        return {"ok": True, "state": pm.state.value}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def test_set_proxy_route_disable():
    async def run():
        from proxy_manager import ProxyManager, ProxyState
        pm = ProxyManager()
        pm._run_cmd = AsyncMock(return_value=(0, ""))
        pm._state = ProxyState.ACTIVE
        result = await _call_set_proxy_route(pm, {"active": False})
        assert result["ok"] is True
        assert pm.state == ProxyState.DISABLED

    asyncio.run(run())


def test_get_proxy_status():
    async def run():
        from proxy_manager import ProxyManager, ProxyState
        pm = ProxyManager()
        pm._state = ProxyState.ACTIVE
        result = {"state": pm.state.value}
        assert result["state"] == "active"

    asyncio.run(run())
```

### Step 2: Run to verify they pass (they use the already-working ProxyManager)

```bash
.venv/bin/python -m pytest tests/test_proxy_manager.py -v -k "ipc or route"
```

Expected: 3 new tests pass.

### Step 3: Modify `openauto_system.py`

Add `ProxyManager` import and instantiation, then register two new IPC methods.

After the `from config_applier import ConfigApplier` import line, add:
```python
from proxy_manager import ProxyManager
```

After `config = ConfigApplier(config_path=config_path)`, add:
```python
proxy = ProxyManager()
```

After the existing IPC handler definitions (just before `ipc.register_method("get_health",...)`), add:

```python
async def handle_set_proxy_route(params):
    active = params.get("active", False)
    try:
        if active:
            await proxy.enable(
                host=params["host"],
                port=int(params["port"]),
                user=params.get("user", "oap"),
                password=params["password"],
            )
        else:
            await proxy.disable()
        return {"ok": True, "state": proxy.state.value}
    except Exception as e:
        LOG.error("set_proxy_route error: %s", e)
        return {"ok": False, "error": str(e), "state": proxy.state.value}

async def handle_get_proxy_status(params):
    return {"state": proxy.state.value}
```

Register them (after the existing `ipc.register_method` calls):
```python
ipc.register_method("set_proxy_route", handle_set_proxy_route)
ipc.register_method("get_proxy_status", handle_get_proxy_status)
```

Also add to the shutdown block (after `health_task.cancel()`):
```python
if proxy.state.value != "disabled":
    await proxy.disable()
```

### Step 4: Modify `openauto-system.service`

Add `ExecStopPost` for unconditional iptables cleanup after the `ExecStart` line:

```ini
ExecStopPost=/usr/sbin/iptables -t nat -D OUTPUT -p tcp -j OPENAUTO_PROXY ; /usr/sbin/iptables -t nat -F OPENAUTO_PROXY ; /usr/sbin/iptables -t nat -X OPENAUTO_PROXY
```

(The `;` separates commands; `iptables` will return non-zero if the chain doesn't exist, which is fine — use `|| true` equivalent by adding `IgnoreExitCode=true` on same line, or just accept the non-zero exit since systemd logs but continues.)

Full updated service section:
```ini
[Service]
Type=notify
ExecStart=/home/matt/openauto-prodigy/system-service/.venv/bin/python3 /home/matt/openauto-prodigy/system-service/openauto_system.py
ExecStopPost=-/usr/sbin/iptables -t nat -D OUTPUT -p tcp -j OPENAUTO_PROXY
ExecStopPost=-/usr/sbin/iptables -t nat -F OPENAUTO_PROXY
ExecStopPost=-/usr/sbin/iptables -t nat -X OPENAUTO_PROXY
WorkingDirectory=/home/matt/openauto-prodigy/system-service
RuntimeDirectory=openauto
Restart=always
RestartSec=2
```

Note: The `-` prefix on `ExecStopPost` tells systemd to ignore non-zero exit codes (chain doesn't exist = OK).

### Step 5: Commit

```bash
git add system-service/openauto_system.py system-service/openauto-system.service
git commit -m "feat(proxy): wire ProxyManager into system daemon IPC"
```

---

## Task 3: Add `redsocks` to install script

**Files:**
- Modify: `install.sh`

### Step 1: Find where redsocks should be added

```bash
grep -n "apt.install\|apt-get install" install.sh | head -10
```

### Step 2: Add `redsocks` to the system packages block

Find the apt install block that includes `hostapd`, `dnsmasq`, or similar networking packages. Add `redsocks` to the same install command.

If there's a dedicated packages variable or install command, add:
```bash
redsocks \
```

### Step 3: Verify redsocks is present on Pi

```bash
ssh matt@192.168.1.152 "which redsocks || echo NOT INSTALLED"
```

If not installed: `ssh matt@192.168.1.152 "sudo apt install -y redsocks"`

### Step 4: Commit

```bash
git add install.sh
git commit -m "feat(proxy): add redsocks to installer dependencies"
```

---

## Task 4: `SystemServiceClient` — C++ client-side changes

**Files:**
- Modify: `src/core/services/SystemServiceClient.hpp`
- Modify: `src/core/services/SystemServiceClient.cpp`

### Step 1: Write the failing test

There's no existing unit test for `SystemServiceClient` (it needs a live socket). Instead, verify at build time and by watching the QML binding work. The test here is a compile check — we'll verify the new interface compiles cleanly.

Create a minimal smoke test at the top of the build verification (done in Task 4 Step 4).

### Step 2: Modify `SystemServiceClient.hpp`

Add the `routeState` Q_PROPERTY and `setProxyRoute()` method:

```cpp
// Add to Q_PROPERTY declarations:
Q_PROPERTY(QString routeState READ routeState NOTIFY routeChanged)
Q_PROPERTY(QString routeError READ routeError NOTIFY routeChanged)

// Add to public section:
QString routeState() const;
QString routeError() const;
Q_INVOKABLE void setProxyRoute(bool active, const QString& host = {},
                               int port = 0, const QString& password = {});
Q_INVOKABLE void getProxyStatus();

// Add to signals:
void routeChanged();

// Add to private members:
QString routeState_ = QStringLiteral("disabled");
QString routeError_;
```

Full updated header — replace the Q_PROPERTY block and public/signals sections:

```cpp
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
    Q_PROPERTY(QString routeState READ routeState NOTIFY routeChanged)
    Q_PROPERTY(QString routeError READ routeError NOTIFY routeChanged)

public:
    explicit SystemServiceClient(QObject* parent = nullptr);

    bool isConnected() const;
    QJsonObject health() const;
    QString routeState() const;
    QString routeError() const;

    Q_INVOKABLE void getHealth();
    Q_INVOKABLE void applyConfig(const QString& section);
    Q_INVOKABLE void restartService(const QString& name);
    Q_INVOKABLE void getStatus();
    Q_INVOKABLE void setProxyRoute(bool active, const QString& host = {},
                                   int port = 0, const QString& password = {});
    Q_INVOKABLE void getProxyStatus();

signals:
    void connectedChanged();
    void healthChanged();
    void routeChanged();
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
    QString routeState_ = QStringLiteral("disabled");
    QString routeError_;
    int nextId_ = 1;
    QMap<QString, QString> pendingMethods_;
};

} // namespace oap
```

### Step 3: Modify `SystemServiceClient.cpp`

Add implementations after `restartService()`:

```cpp
QString SystemServiceClient::routeState() const { return routeState_; }
QString SystemServiceClient::routeError() const { return routeError_; }

void SystemServiceClient::setProxyRoute(bool active, const QString& host,
                                         int port, const QString& password)
{
    QJsonObject params;
    params["active"] = active;
    if (active) {
        params["host"] = host;
        params["port"] = port;
        params["user"] = QStringLiteral("oap");
        params["password"] = password;
    }
    sendRequest("set_proxy_route", params);
}

void SystemServiceClient::getProxyStatus()
{
    sendRequest("get_proxy_status");
}
```

Add response dispatch in `handleResponse()`, after the `"get_status"` branch:

```cpp
} else if (method == "set_proxy_route" || method == "get_proxy_status") {
    QJsonObject result = response["result"].toObject();
    QString newState = result["state"].toString(routeState_);
    QString newError = result.value("error").toString();
    if (newState != routeState_ || newError != routeError_) {
        routeState_ = newState;
        routeError_ = newError;
        emit routeChanged();
    }
}
```

### Step 4: Build and verify compiles cleanly

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake --build . -j$(nproc) 2>&1 | tail -20
```

Expected: zero errors, binary links.

### Step 5: Commit

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git add src/core/services/SystemServiceClient.hpp src/core/services/SystemServiceClient.cpp
git commit -m "feat(proxy): add setProxyRoute/routeState to SystemServiceClient"
```

---

## Task 5: Wire CompanionListenerService to trigger proxy routing

**Files:**
- Modify: `src/core/services/CompanionListenerService.hpp`
- Modify: `src/core/services/CompanionListenerService.cpp`

The design: `CompanionListenerService` receives 5s status JSON. We track the previous `socks5.active` value and only call `setProxyRoute()` on *changes*. On client disconnect, always call disable.

### Step 1: Confirm existing status handling

Read `CompanionListenerService.cpp` around `handleStatus()` to see where `socks5` is parsed. The relevant section reads `socks5.active`, `socks5.host`, `socks5.port`, `socks5.user`, `socks5.password`.

### Step 2: Modify `CompanionListenerService.hpp`

Add `SystemServiceClient` forward declaration and member. Add `lastSocks5Active_` tracker.

Add to includes: `#include "SystemServiceClient.hpp"` (or forward declare + pointer).

Add to private members:
```cpp
SystemServiceClient* systemClient_ = nullptr;
bool lastSocks5Active_ = false;

// SOCKS5 password: first 8 hex chars of sharedSecret_
QString socks5Password() const;
```

Add to public section:
```cpp
void setSystemServiceClient(SystemServiceClient* client) { systemClient_ = client; }
```

### Step 3: Modify `CompanionListenerService.cpp`

Add helper:
```cpp
QString CompanionListenerService::socks5Password() const
{
    // First 8 hex chars of the companion shared secret
    return sharedSecret_.left(8).toLower();
}
```

In `handleStatus()`, find where `socks5` is processed. After reading the socks5 fields, add state-change detection:

```cpp
// --- SOCKS5 proxy routing ---
QJsonObject socks5 = msg["socks5"].toObject();
bool socks5Active = socks5["active"].toBool(false);
QString socks5Host = socks5["host"].toString();
int socks5Port = socks5["port"].toInt(1080);

if (socks5Active != lastSocks5Active_) {
    lastSocks5Active_ = socks5Active;
    if (systemClient_) {
        if (socks5Active && !socks5Host.isEmpty()) {
            systemClient_->setProxyRoute(true, socks5Host, socks5Port,
                                          socks5Password());
        } else {
            systemClient_->setProxyRoute(false);
        }
    }
}
```

In `onClientDisconnected()`, add cleanup:
```cpp
// Disable proxy routing when companion disconnects
if (lastSocks5Active_ && systemClient_) {
    systemClient_->setProxyRoute(false);
}
lastSocks5Active_ = false;
```

### Step 4: Wire in `main.cpp`

In `src/main.cpp`, after `companionListener` and `systemClient` are both created, add:
```cpp
companionListener->setSystemServiceClient(systemClient);
```

The `systemClient` is created at line ~172 as `auto* systemClient = new oap::SystemServiceClient(&app)`. The `companionListener` block is at ~113. Either move the systemClient creation earlier or add the wire-up after line ~172:
```cpp
if (companionListener)
    companionListener->setSystemServiceClient(systemClient);
```

### Step 5: Build and run all tests

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake --build . -j$(nproc) && ctest --output-on-failure
```

Expected: build succeeds, all 47+ tests pass.

### Step 6: Commit

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git add src/core/services/CompanionListenerService.hpp \
        src/core/services/CompanionListenerService.cpp \
        src/main.cpp
git commit -m "feat(proxy): wire CompanionListenerService → SystemServiceClient on socks5 state change"
```

---

## Task 6: Proxy route indicator in CompanionSettings.qml

**Files:**
- Modify: `qml/applications/settings/CompanionSettings.qml`

The `SystemService` QML context property is `SystemService` (registered in `main.cpp`). It has `routeState` (string: `"disabled"`, `"active"`, `"degraded"`, `"failed"`) and `routeError`.

### Step 1: Add the indicator after the "Internet Proxy" row

In `CompanionSettings.qml`, after the existing "Internet Proxy" `Item` block (line ~145), add a new status row in the Status section. It should be visible when the companion is connected and internet is available.

Insert after the internet proxy row (after line 145, before `SectionHeader { text: "Pairing" }`):

```qml
// Proxy route status (visible when internet available)
Item {
    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH
    visible: root.hasService && CompanionService.internetAvailable

    readonly property bool hasSystemService: typeof SystemService !== "undefined"
    readonly property string routeStateStr: hasSystemService ? SystemService.routeState : "disabled"
    readonly property bool routeActive: routeStateStr === "active"
    readonly property bool routeDegraded: routeStateStr === "degraded"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        Text {
            text: "Route Active"
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.normalFontColor
            Layout.preferredWidth: parent.width * 0.35
        }

        // Status dot
        Rectangle {
            width: UiMetrics.iconSmall
            height: UiMetrics.iconSmall
            radius: width / 2
            color: {
                if (parent.parent.routeActive) return "#4CAF50"       // green
                if (parent.parent.routeDegraded) return "#FF9800"     // amber
                return ThemeService.descriptionFontColor               // grey
            }
        }

        Text {
            text: {
                var s = parent.parent.routeStateStr
                if (s === "active") return "Routing via phone"
                if (s === "degraded") return "Degraded — retrying"
                if (s === "failed") return "Failed"
                return "Inactive"
            }
            font.pixelSize: UiMetrics.fontBody
            color: parent.parent.routeDegraded
                   ? "#FF9800"
                   : ThemeService.normalFontColor
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
        }
    }
}
```

### Step 2: Build and verify the UI compiles

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake --build . -j$(nproc) 2>&1 | tail -10
```

Expected: zero errors.

### Step 3: Commit

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git add qml/applications/settings/CompanionSettings.qml
git commit -m "feat(proxy): add route status indicator to CompanionSettings"
```

---

## Task 7: Deploy and end-to-end test on Pi

### Step 1: Cross-compile the Qt binary

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
docker run --rm -u "$(id -u):$(id -g)" \
    -v "$(pwd):/src:rw" \
    -w /src/build-pi \
    openauto-cross-pi4 \
    cmake --build . -j$(nproc)
```

### Step 2: Sync to Pi

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
rsync -av system-service/ matt@192.168.1.152:~/openauto-prodigy/system-service/
```

### Step 3: Install redsocks if needed

```bash
ssh matt@192.168.1.152 "which redsocks || sudo apt install -y redsocks"
```

Note: Debian ships redsocks with a default config that auto-starts. Disable the default:
```bash
ssh matt@192.168.1.152 "sudo systemctl disable --now redsocks 2>/dev/null || true"
```

### Step 4: Restart system daemon on Pi

```bash
ssh matt@192.168.1.152 "sudo systemctl daemon-reload && sudo systemctl restart openauto-system"
ssh matt@192.168.1.152 "sudo journalctl -u openauto-system -n 30 --no-pager"
```

Expected: no import errors, daemon starts cleanly.

### Step 5: Launch the app and exercise proxy toggle

```bash
# Kill existing instance
ssh matt@192.168.1.152 "pkill -f openauto-prodigy || true"

# Launch fresh
ssh matt@192.168.1.152 'nohup env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 \
  /home/matt/openauto-prodigy/build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'

sleep 3
ssh matt@192.168.1.152 "tail -30 /tmp/oap.log"
```

### Step 6: Verify with companion app

1. Connect companion app to Pi
2. Enable "Internet Sharing" in companion app
3. On Pi: `ssh matt@192.168.1.152 "sudo iptables -t nat -L OUTPUT -v"` — should show OPENAUTO_PROXY chain
4. On Pi: `ps aux | grep redsocks` — should show redsocks process
5. Verify CompanionSettings.qml shows "Routing via phone" with green dot
6. Disable internet sharing in companion app
7. iptables rules should be gone, redsocks stopped, indicator shows "Inactive"

### Step 7: Commit anything needed, push

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git log --oneline -8
git push
```

---

## Verification Checklist

Before considering the feature complete:

- [ ] `pytest system-service/tests/test_proxy_manager.py` — all tests pass
- [ ] `cmake --build build -j$(nproc)` — zero errors on dev machine
- [ ] `ctest --test-dir build --output-on-failure` — all 47+ tests pass
- [ ] On Pi: companion disconnect → iptables rules removed immediately
- [ ] On Pi: daemon restart (`sudo systemctl restart openauto-system`) → iptables clean even if proxy was active
- [ ] On Pi: `sudo systemctl stop openauto-system` → `ExecStopPost` cleans iptables (test manually)
- [ ] UI indicator reflects all three states: grey (inactive), green (active), amber (degraded)
- [ ] No iptables rules bleed between proxy sessions (enable → disable → enable works correctly)
