# External Integrations

**Analysis Date:** 2025-03-01

## APIs & External Services

**Android Auto (AA) Protocol:**
- Standard wireless AA discovery and session establishment
  - SDK/Client: `libs/open-androidauto` (in-tree Qt-native implementation)
  - Transport: Bluetooth SDP discovery → WiFi AP TCP → TLS/frames
  - Protocol: Custom protobuf messages (oaa/control, oaa/av, etc.)
  - Auth: No external auth; phone-to-Pi trust via WiFi pairing

**GitHub API (Installer Only):**
- Endpoint: `https://api.github.com/repos/mrmees/openauto-prodigy/releases`
  - Used by: `install.sh --list-prebuilt` to enumerate release assets
  - Asset naming: `openauto-prodigy-prebuilt-<date>-pi4-aarch64.tar.gz`
  - No authentication required (public repo, rate-limited to 60 req/hr for IPs)
  - Env var override: `OAP_GITHUB_API_URL`

**Companion App (Prodigy Companion — Android):**
- Protocol: TCP JSON over WiFi
  - Port: 9876 (configurable in `CompanionListenerService`)
  - Connection: Phone sends `hello` with HMAC-SHA256 challenge-response auth
  - Messages: GPS, battery, network status, time, proxy configuration
  - QR Code: Generated on-device, shared via web config for pairing
  - Implementation: `src/core/services/CompanionListenerService.cpp`

**Web Configuration Panel:**
- Server: Flask 3.0+ (Python)
  - Port: 8080 (on-device, via 10.0.0.1 WiFi AP address)
  - IPC: Unix domain socket (`/tmp/openauto-prodigy.sock`)
  - Routes: dashboard, settings, themes, audio, plugins
  - Auth: None (local WiFi AP only)
  - Implementation: `web-config/server.py`

## Data Storage

**Databases:**
- Not detected. Configuration is file-based only.

**File Storage:**
- Local filesystem (YAML + INI):
  - `~/.openauto/config.yaml` — Main device configuration
  - `~/.openauto/themes/default/theme.yaml` — Day/night color palette
  - `~/.openauto/plugins/` — User-installed plugin `.so` files
  - `~/.openauto/vehicle.id` — Vehicle UUID v4 (companion pairing)
  - Client: `src/core/YamlConfig.cpp` (YAML parser/writer with deep merge)

**Log Files:**
- `/tmp/openauto-install.log` — Installer debug log (if TUI mode fails)
- stderr/stdout — Real-time logging to console
- No persistent centralized logs (logs to systemd journal on Pi via `journalctl`)

**Caching:**
- PipeWire device registry — In-memory, hot-plugged via pw_registry callbacks
  - Cached in: `src/core/audio/PipeWireDeviceRegistry.cpp`
- AA video frame pool — Recycled QVideoFrame allocations to avoid malloc churn
  - Cached in: `src/core/aa/VideoFramePool.hpp`
- Touch device map — Cached on app startup via `InputDeviceScanner.cpp`

**Caching:**
- PipeWire playback — Lock-free SPSC ring buffer (`AudioRingBuffer`) from ASIO thread to PipeWire RT callback
  - Full period writes required (PipeWire graph timing constraint)

## Authentication & Identity

**Auth Provider:**
- None for main app (local network only, WiFi AP is the access control)
- Companion app uses HMAC-SHA256 with shared secret (QR code pairing)
  - Secret derivation: SHA256(6-digit PIN + vehicle UUID)
  - Implementation: `src/core/services/CompanionListenerService.cpp`

**Bluetooth Pairing:**
- BlueZ Agent1 service (D-Bus)
  - Delegates pairing to system UI (no in-app pairing UI)
  - Adapter type: LOCAL (RPi internal BT), REMOTE (external dongle), or NONE
  - Config key: `bluetooth.adapter_type` in `config.yaml`

**Phone Trust:**
- WiFi SSID/password must match `config.yaml` (device name prefix in SSID)
  - Example: SSID `OpenAutoProdigy_AP` = device name `OpenAutoProdigy`
  - Phone connects to WiFi AP automatically after BT discovery

**D-Bus Security:**
- polkit rules for BlueZ pairing (allow without password):
  - File: `config/bluez-agent-polkit.rules`
  - Installed by: `install.sh` to `/etc/polkit-1/rules.d/`
- polkit rules for companion listener (allow port binding):
  - File: `config/companion-polkit.rules`

## Monitoring & Observability

**Error Tracking:**
- Not detected. No external error reporting (Sentry, Bugsnag, etc.)

**Logs:**
- stderr output: Real-time app logging to console
  - Qt categories: `qt.qml`, `qt.network`, custom `openauto.*` categories
  - Systemd journal capture: `journalctl -u openauto-prodigy.service -f`
- Installer logs: `/tmp/openauto-install.log` (aggregate debug log)
  - Format: `[HH:MM:SS] [LEVEL] message`

**Metrics:**
- Not detected. No metrics collection or aggregation.

**Debugging:**
- Protocol logger: `src/oaa/Messenger/ProtocolLogger.hpp`
  - Conditional: `#ifdef DEBUG_AA_PROTOCOL`
  - Frame trace output to stderr
- QML debugging: Qt Quick Debugger on USB (ADB bridge, dev only)

## CI/CD & Deployment

**Hosting:**
- GitHub repository: `https://github.com/mrmees/openauto-prodigy`
- Release assets: Prebuilt Pi 4 aarch64 binaries (GitHub Releases)
- Installation: Source or prebuilt tarball via `install.sh`

**CI Pipeline:**
- Not detected. No GitHub Actions, GitLab CI, or external CI service configured in repo.
  - Manual testing: `install.sh` runs on fresh Pi OS Trixie
  - Cross-compile local: `build-pi/` dir on dev VM (no CI orchestration)

**Deployment:**
- Installer script: `install.sh`
  - Interactive TUI mode (default) or non-interactive scripted mode
  - Installs to `~/openauto-prodigy/`
  - Sets up systemd service: `openauto-prodigy.service`
  - Optionally installs web config panel
- systemd service:
  - Unit file: Generated by `install.sh` and placed in `/etc/systemd/system/`
  - Startup: `Type=notify` with systemd-notify heartbeat
  - Auto-restart: `Restart=always` (supervised)
  - Audio socket ordering: `After=pulseaudio.service` or PipeWire unit

**Updates:**
- Manual: `git pull` in repo, rebuild, restart service
- Prebuilt: Download from GitHub Releases, extract, run `install.sh --mode prebuilt`

**Configuration Management:**
- Single-writer rule: Only Qt app writes to `~/.openauto/config.yaml`
- Web config reads/writes via IPC (Qt app is authority)
- System service reads config on startup, watches for changes

## Environment Configuration

**Required env vars:**
- None required for normal operation (all config in YAML files)

**Optional env vars:**
- `OAP_IPC_SOCKET` — Path to Qt app IPC socket (default: `/tmp/openauto-prodigy.sock`)
- `OAP_GITHUB_API_URL` — GitHub API endpoint for installer (default: official releases API)
- `WAYLAND_DISPLAY` — Wayland socket (set by labwc, usually `wayland-0`)

**Secrets location:**
- No secrets stored in files
- Companion shared secret: Derived on-the-fly from pairing PIN
- TLS certs: Generated by OpenSSL at runtime for AA transport (ephemeral, not stored)

**Config files:**
- `~/.openauto/config.yaml` — YAML format with keys:
  - `device.name` — Broadcast name (in SSID, D-Bus friendly name)
  - `wifi.ssid` — WiFi AP broadcast SSID
  - `wifi.password` — WPA2 passphrase
  - `wifi.country_code` — Regulatory domain (auto-detected or user-specified)
  - `network.tcp_port` — AA TCP listen port (default: 5277)
  - `display.*` — Screen type, DPI, handedness, margins
  - `bluetooth.adapter_type` — LOCAL, REMOTE, or NONE
  - `video.fps` — Frames per second cap (default: 30)
  - `video.codec_preferences` — Priority order: h264, h265, vp9, av1

## Webhooks & Callbacks

**Incoming:**
- Not detected. No webhook endpoints.

**Outgoing:**
- None. App does not call external webhooks or APIs (besides GitHub API for releases).

## System Service Integration

**openauto-system daemon (Python, separate process):**
- Purpose: Privileged system operations (hostapd, udhcp, iptables, BlueZ policy)
- IPC: JSON-RPC over Unix socket (`/tmp/openauto-system.sock`)
- Implementation: `system-service/main.py`
- Dependencies: dbus-next, PyYAML, pytest
- Routes:
  - D-Bus BlueZ ProfileManager (HFP, HSP, AA profile registration)
  - Network proxy configuration (SOCKS5 via companion app)
  - WiFi AP restart on config changes
  - hostapd country code update

**Systemd Integration:**
- ExecStart: Launches openauto-prodigy Qt app
- ExecStartPost: Optional openauto-system daemon start
- Systemd override: `/etc/systemd/system/bluetooth.service.d/override.conf`
  - Adds `--compat` flag to BlueZ for SDP registration

---

*Integration audit: 2025-03-01*
