# AA Disconnect/Reconnect Testing Script

## Overview

`Testing/reconnect.sh` automates the full AA disconnect → clean state → reconnect cycle between the Pi head unit and an Android phone. This is the foundation for protocol exploration and regression testing — it eliminates the manual BT/WiFi dance and makes AA sessions reproducible.

## Usage

```bash
./Testing/reconnect.sh [wait_seconds]
```

- `wait_seconds` (default: 10) — delay between kill and restart, giving the Pi time to release ports/resources.
- Requires: ADB-connected phone, SSH access to Pi (key auth, no password prompts).

## Prerequisites

| Requirement | Details |
|-------------|---------|
| ADB | `platform-tools/adb` (relative to script dir) with phone authorized |
| SSH | Key-based auth to `matt@192.168.1.149`, no passphrase |
| Pi app | Built at `/home/matt/openauto-prodigy/build/src/openauto-prodigy` |
| BT pairing | Phone already paired with Pi as "OpenAutoProdigy" |
| WiFi AP | Pi's `wlan0` running hostapd (SSID: OpenAutoProdigy) |

## Sequence

```
[1/5] Disconnect
      Pi:    bluetoothctl disconnect <PHONE_MAC>
      Phone: svc wifi disable  (via ADB)

[2/5] Kill app
      Pi:    pkill → sleep 2 → pkill -9 (graceful then force)

[3/5] Wait
      Sleep $WAIT seconds (default 10) for port/resource cleanup

[4/5] Restart app
      Pi:    ssh -f launches app in background
      Wait 8s for RFCOMM listener + SDP registration
      Verify app is running (pgrep)

[5/5] BT connect (retry loop)
      Pi:    bluetoothctl connect <MAC> <A2DP_UUID>
      Up to 6 attempts, 10s timeout each, 5s between retries
      Phone auto-discovers WiFi AP via AA BT handshake
      Wait 20s for AA session, check for VIDEO frames in log
```

## Key Design Decisions

### Why `ssh -f` for app launch (not `nohup &`)

SSH with `BatchMode=yes` waits for all child FDs to close before exiting. `nohup cmd & disown` doesn't fully detach — SSH hangs indefinitely. `ssh -f` backgrounds the SSH client process itself, avoiding the hang while still getting the app started.

### Why A2DP UUID in bluetoothctl connect

`bluetoothctl connect <MAC>` on dual-mode devices (BR/EDR + BLE) randomly picks BLE transport, which fails for AA. Specifying the A2DP Audio Source UUID (`0000110a-0000-1000-8000-00805f9b34fb`) forces classic BR/EDR transport.

### Why no phone WiFi re-enable

The AA Bluetooth handshake includes WiFi AP credentials. After BT connect, the phone automatically:
1. Enables WiFi
2. Scans for the advertised SSID
3. Connects to the Pi's AP
4. Establishes the TCP session

No `svc wifi enable` needed — and adding it can actually race with the BT handshake.

### Why retry loop for BT connect

After app restart, the Pi needs ~8 seconds for RFCOMM listener and SDP service registration. The phone may report "not available" initially. The retry loop (6 attempts × 10s timeout) handles this startup window reliably. In practice, connection succeeds on attempt 1-3.

### Why `timeout` runs on Pi side

`timeout 10 bluetoothctl connect ...` inside the SSH command. Local `timeout` around SSH doesn't reliably kill the remote `bluetoothctl` process — the remote end keeps running even after SSH is terminated.

### Why `pi()` function instead of variable

Storing `ssh -o ConnectTimeout=5 ...` in a variable and using with `timeout` breaks due to word splitting. A shell function preserves argument boundaries correctly.

## Gotchas Discovered

| Gotcha | Details |
|--------|---------|
| BLE transport selection | `bluetoothctl connect` randomly picks BLE on dual-mode devices. Must specify A2DP UUID. |
| `svc bluetooth enable` unreliable | ADB command sometimes doesn't turn BT back on. Avoid toggling the BT radio entirely. |
| `pkill` 15-char limit | Process names >15 chars silently match nothing. Use `pkill -f` for full command line matching. |
| SSH BatchMode + nohup | SSH waits for child FDs. Use `ssh -f` for backgrounded launches. |
| RFCOMM registration delay | ~8s after app start before phone can discover the AA service. |
| Phone WiFi auto-connect | AA BT handshake tells phone which AP to join. Manual WiFi enable is unnecessary and can race. |

## Codex Review Findings

Reviewed 2026-02-23. Summary of actionable items:

| Severity | Finding | Status |
|----------|---------|--------|
| High | SSH hang on restart (BatchMode + nohup) | **Fixed** — uses `ssh -f` |
| High | App readiness uses fixed 8s sleep vs log polling | Known — works reliably enough for testing |
| Medium | Unquoted variable expansions | **Fixed** — all variables quoted |
| Medium | WAIT input not validated | **Fixed** — integer regex check added |
| Medium | Retry timing message inaccurate | **Fixed** — computed from MAX_ATTEMPTS * timeout |
| Low | Success detection depends on "Connection successful" text | Accepted — bluetoothctl output is stable |
| Low | No ADB binary existence check | **Fixed** — `-x` check in preflight |

## Verified Working

Tested end-to-end 2026-02-23:
- Full disconnect → reconnect cycle completes in ~50s
- BT connection succeeds (typically attempt 1-2)
- AA session establishes with video streaming (479 frames observed)
- Script exits cleanly with status report
