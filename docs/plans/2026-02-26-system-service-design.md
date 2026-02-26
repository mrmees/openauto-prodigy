# OpenAuto System Service Design

## Goal

A Python asyncio daemon (`openauto-system`) that runs as root, manages system configuration, monitors service health, and exposes a JSON-over-Unix-socket API to the Prodigy Qt app. This gives the app a reliable backend for reading/writing system settings and ensures the Pi's system services stay healthy without manual intervention.

## Architecture

```
┌─────────────────────────────────────────────┐
│           openauto-prodigy (Qt app)          │
│  ┌──────────┐                               │
│  │ Settings  │──── JSON/Unix socket ────┐   │
│  │   UI      │                          │   │
│  └──────────┘                           │   │
└─────────────────────────────────────────│───┘
                                          │
                    /run/openauto/system.sock
                                          │
┌─────────────────────────────────────────│───┐
│         openauto-system (Python daemon)  │   │
│                                          ▼   │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ IPC      │  │ Health   │  │ Config   │  │
│  │ Server   │  │ Monitor  │  │ Applier  │  │
│  └──────────┘  └──────────┘  └──────────┘  │
│       │              │             │         │
│       └──────────────┼─────────────┘         │
│                      ▼                       │
│          systemctl, hostapd.conf,            │
│          networkd configs, bluetoothctl      │
└──────────────────────────────────────────────┘
```

**Runtime:** Python 3, asyncio, single process. Runs as root via systemd.

**Config source:** Reads from the app's existing `~/.openauto/config.yaml`. The app writes config, the daemon reads and applies to system files.

## IPC Protocol

Socket: `/run/openauto/system.sock` (Unix domain, stream).

Newline-delimited JSON request/response. The `id` field matches responses to requests.

**Request:**
```json
{"id": "abc123", "method": "get_health"}
{"id": "abc124", "method": "apply_config", "params": {"section": "wifi"}}
```

**Response:**
```json
{"id": "abc123", "result": {"hostapd": "active", "networkd": "active", "bluetooth": "active"}}
{"id": "abc124", "result": {"ok": true, "restarted": ["hostapd"]}}
```

**Error:**
```json
{"id": "abc124", "error": {"code": "RESTART_FAILED", "message": "hostapd failed to start"}}
```

**Methods:**

| Method | Description |
|--------|-------------|
| `get_health` | Returns state of all monitored services + functional checks |
| `apply_config` | Re-read config.yaml, rewrite system files for given section, restart affected services |
| `get_status` | Full dump: health + current applied config + daemon uptime |
| `restart_service` | Force-restart a specific service (hostapd, bluetooth, networkd) |

## Health Monitor

**Polling:** Every 5 seconds, checks three systemd units plus functional verification.

| Service | Unit Check | Functional Check |
|---------|-----------|-----------------|
| WiFi AP | `systemctl is-active hostapd` | `ip addr show wlan0` has correct IP |
| Network | `systemctl is-active systemd-networkd` | wlan0 has expected IP + DHCP range |
| Bluetooth | `systemctl is-active bluetooth` | `hci0` is UP RUNNING |

**Auto-recovery:**

| Situation | Action |
|-----------|--------|
| Service `inactive`/`failed` | Restart, up to 3 fast retries (5s backoff) |
| Service `activating` for > 30s | Treat as failed |
| Recovered after restart | Log success, reset retry counter |
| Failed after 3 fast retries | Switch to slow retry (every 60s, indefinitely) |
| Healthy for 120s | Reset retry counter |

**Subprocess timeout:** 5 seconds on all `systemctl` and health check calls.

**Immediate check:** On daemon startup, run a full health check before accepting IPC connections.

**BT profile registration:** After detecting `bluetooth.service` restart (or on daemon boot), re-register BT profiles:
- AA SDP record
- HFP AG profile (`0000111f`)
- HSP HS profile

Registration via `dbus-next` calling BlueZ `ProfileManager1` D-Bus interface. The daemon owns BT profile registration exclusively — removed from the app's init path.

## Config Applier

Reads `~/.openauto/config.yaml`, rewrites system config files from templates, restarts affected services.

**Config sections:**

| Section | YAML Path | System File | Restart |
|---------|-----------|-------------|---------|
| `wifi` | `wifi.ssid`, `wifi.password`, `wifi.channel`, `wifi.band` | `/etc/hostapd/hostapd.conf` | `hostapd` |
| `network` | `network.ap_ip`, `network.dhcp_range_start`, `network.dhcp_range_end` | `/etc/systemd/network/10-openauto-ap.network` | `systemd-networkd` |
| `bluetooth` | `bluetooth.name`, `bluetooth.discoverable` | BlueZ D-Bus calls | None |
| `time` | `time.timezone` | `timedatectl set-timezone` | None |

**Template approach:** Each config file has a Python template string. Values filled from config.yaml, written atomically (write `.tmp`, `os.rename`). Daemon owns the entire file content — no sed/regex on existing files.

**Validation:** Before applying, validate values (SSID length 1-32, WPA2 password >= 8 chars, valid IP addresses). Return error if validation fails, don't touch system files.

## Daemon Lifecycle

**Systemd unit:** `/etc/systemd/system/openauto-system.service`

```ini
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

- `Before=openauto-prodigy.service` — daemon starts first, ensures system health
- `Type=notify` — sends `sd_notify(READY=1)` after first health check passes
- `RuntimeDirectory=openauto` — systemd creates `/run/openauto/` automatically

## File Layout

```
system-service/
├── openauto_system.py      # Entry point + asyncio main loop
├── health_monitor.py       # Periodic health checks + auto-recovery
├── config_applier.py       # Config validation + system file templates + apply
├── ipc_server.py           # Unix socket server + JSON protocol
├── bt_profiles.py          # BT profile registration via dbus-next
└── templates/
    ├── hostapd.conf.tmpl
    └── 10-openauto-ap.network.tmpl
```

**Dependencies:** Python stdlib + `dbus-next` (pure-Python async D-Bus library).

## App Integration

**New class:** `SystemServiceClient` (C++, `src/core/services/`)

- Connects to `/run/openauto/system.sock` on startup
- `Q_INVOKABLE` methods for QML: `getHealth()`, `applyConfig(section)`, `restartService(name)`
- Async — sends request, emits signal when response arrives
- Graceful degradation — if daemon not running, methods return error state, app still works

**Changes to existing code:**
- Remove BT profile registration from app init (daemon owns this)
- Add `SystemServiceClient` to `main.cpp`, expose to QML context

**What stays in the app:**
- CompanionListenerService (tightly coupled to QML for QR display)
- All AA protocol, video, audio, touch
- Config.yaml writing
- All UI

**What moves to the daemon:**
- BT profile registration (SDP, HFP AG, HSP)
- System config file management (hostapd.conf, networkd config)
- Service health monitoring and auto-recovery

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Language | Python | Already on Pi (Flask web panel), natural for system scripting, asyncio for concurrency |
| Privilege | Root | Single-user embedded system, avoids polkit/sudo complexity |
| Architecture | Monolithic asyncio | One process, async subprocess for non-blocking systemctl calls |
| IPC | JSON over Unix socket | Consistent with existing IpcServer pattern in codebase |
| Config source | App's config.yaml | Single source of truth, no config drift |
| Polling interval | 5 seconds | Balance between responsiveness and overhead |
| BT registration | dbus-next + ProfileManager1 | Cleaner than shelling out to bluetoothctl |
| Health checks | Functional (not just is-active) | Catches "running but broken" states |
