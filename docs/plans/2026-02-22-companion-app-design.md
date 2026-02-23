# OpenAuto Companion App — Design Document

**Date:** 2026-02-22
**Status:** Approved design, pending implementation planning

## Purpose

A lightweight Android app that shares phone resources with the Raspberry Pi head unit. The Pi in a car has no RTC, no GPS, and no internet access. The phone — already connected via WiFi AP for Android Auto — has all three. This app bridges the gap.

**It is not a remote control for the head unit.** The Pi runs itself. The phone provides:
- **Time** — accurate wall-clock for a Pi with no RTC or NTP
- **GPS** — real location data for a Pi with no GPS hardware
- **Battery** — phone battery level for dashboard display
- **Internet** — SOCKS5 proxy bridging WiFi→cellular for Pi apps

**Future potential:** Native settings UI (WebView wrapper around existing web config), notification forwarding, media metadata.

## Architecture

```
┌─────────────────────────┐            ┌──────────────────────────────┐
│   Raspberry Pi           │            │   Android Phone               │
│   (10.0.0.1)             │            │   (10.0.0.x via DHCP)         │
│                          │            │                                │
│  ┌────────────────────┐  │   TCP      │  ┌────────────────────────┐   │
│  │ CompanionListener  │◄─┼───────────┼──│ Data Push Service       │   │
│  │ (port 9876)        │  │   JSON     │  │ (foreground service)    │   │
│  └────────┬───────────┘  │            │  │ time/GPS/battery push   │   │
│           │              │            │  └────────────────────────┘   │
│  ┌────────▼───────────┐  │            │                                │
│  │ timedatectl        │  │   SOCKS5   │  ┌────────────────────────┐   │
│  │ (D-Bus/polkit)     │  │◄──────────┼──│ SOCKS5 Proxy Server     │   │
│  └────────────────────┘  │            │  │ (port 1080)             │   │
│                          │            │  │ WiFi recv → cellular out │   │
│  Pi apps use:            │            │  │ explicit Network.bind    │   │
│  - ALL_PROXY env var     │            │  └────────────────────────┘   │
│  - proxychains4          │            │                                │
│  - per-app proxy flags   │            │  Both services auto-start on   │
│                          │            │  WiFi SSID detection            │
└──────────────────────────┘            └──────────────────────────────┘
```

### Phone-Side Components

1. **Data Push Service** — Android foreground service with persistent notification. Connects TCP to `10.0.0.1:9876`. Sends newline-delimited JSON every 5 seconds with time, GPS, battery, and proxy status. Authenticated via challenge-response handshake and per-message HMAC.

2. **SOCKS5 Proxy Server** — Listens on port 1080, bound to WiFi interface. Receives CONNECT requests from Pi apps, forwards them out cellular via explicit `Network.bindSocket()` to the cellular network. Username/password auth required. CONNECT-only (no UDP ASSOCIATE or BIND).

### Pi-Side Components

3. **CompanionListener** — New class in `src/core/services/`. TCP server on port 9876. Validates auth, parses JSON, dispatches to subsystems. Exposes state as Q_PROPERTYs for QML. Estimated ~400 lines of C++.

4. **Clock management** — Calls `timedatectl set-time` via D-Bus (polkit-authorized). Only adjusts when delta exceeds 30 seconds. Disables `systemd-timesyncd` interference during companion-driven time sync.

5. **Proxy advertisement** — When companion reports `socks5.active: true`, CompanionListener emits signal. Pi-side scripts or config can set `ALL_PROXY` env var for child processes.

## Data Protocol

### Transport
- TCP, port 9876
- Newline-delimited JSON (`\n` terminated)
- Single connection at a time (reject additional clients)

### Handshake (challenge-response)

```
Phone connects TCP to 10.0.0.1:9876

Pi → Phone:
{
  "type": "challenge",
  "nonce": "random-32-bytes-hex",
  "version": 1
}

Phone → Pi:
{
  "type": "hello",
  "version": 1,
  "token": "hmac-sha256(shared_secret, nonce)",
  "capabilities": ["time", "gps", "battery", "socks5"]
}

Pi → Phone (if token valid):
{
  "type": "hello_ack",
  "accepted": true,
  "session_key": "random-32-bytes-hex-encrypted-with-shared-secret"
}
```

The `session_key` is used for per-message HMAC on all subsequent frames. This prevents replay of old status messages and injection of forged data.

### Status Messages (authenticated)

```json
{
  "type": "status",
  "seq": 42,
  "sent_mono_ms": 123456789,
  "time_ms": 1740250000000,
  "timezone": "America/Chicago",
  "gps": {
    "lat": 32.7767,
    "lon": -96.7970,
    "accuracy": 8.5,
    "speed": 45.2,
    "bearing": 180.0,
    "age_ms": 1200
  },
  "battery": {
    "level": 72,
    "charging": true
  },
  "socks5": {
    "port": 1080,
    "active": true
  },
  "mac": "hmac-sha256(session_key, seq + payload_without_mac)"
}
```

**Field notes:**
- `seq` — monotonically increasing, Pi rejects out-of-order or replayed sequence numbers (sliding window of 10)
- `sent_mono_ms` — phone's monotonic clock, useful for RTT estimation but not cross-device freshness
- `time_ms` — Unix epoch milliseconds (wall clock)
- `gps.age_ms` — milliseconds since last GPS fix, allows Pi to assess staleness
- `mac` — HMAC-SHA256 of the message content (excluding the mac field itself) using session key

### Stale Data Policy
- **Time:** reject backward jumps >5 minutes unless 3 consecutive messages agree. Accept forward jumps freely (phone restarted, timezone change).
- **GPS:** expose `age_ms` to consumers. Mark location as stale if `age_ms > 30000` (30s). Clear location if no update for 60s.

## SOCKS5 Proxy

### Implementation
- CONNECT-only (RFC 1928 CMD 0x01). No UDP ASSOCIATE, no BIND.
- Username/password auth (RFC 1929). Credentials derived from shared secret.
- Bound to WiFi interface IP only (not 0.0.0.0).
- Outbound connections explicitly bound to cellular `Network` via `ConnectivityManager.requestNetwork(TRANSPORT_CELLULAR)` + `Network.bindSocket()`.

### Security Controls
- **Egress ACLs:** Block connections to RFC1918 (10.x, 172.16-31.x, 192.168.x), link-local (169.254.x), loopback (127.x), and multicast ranges. Prevents proxy abuse as a network scanner.
- **Connection limits:** Max 20 concurrent connections. Rate limit auth failures (3 failures → 30s lockout).
- **Timeouts:** Idle connection timeout 120s. Connect timeout 10s.

### Pi-Side Usage
| Tool | Configuration |
|------|--------------|
| Chromium | `chromium --proxy-server=socks5://10.0.0.x:1080` |
| curl/wget | `ALL_PROXY=socks5://user:pass@10.0.0.x:1080` |
| apt | `Acquire::http::Proxy "socks5h://user:pass@10.0.0.x:1080";` in apt.conf |
| pip/npm | Respect `ALL_PROXY` env var |
| General TCP | `proxychains4` wrapper |

**Note:** Raw UDP traffic (e.g., NTP, raw DNS) does not traverse SOCKS5 CONNECT. Time sync is handled by direct push. DNS resolution uses `socks5h://` (proxy resolves DNS) or a local stub resolver.

## Clock Setting

### Mechanism
`timedatectl set-time` via D-Bus call to `org.freedesktop.timedate1.SetTime`. Requires polkit authorization for the service user.

### Polkit Policy
Create `/etc/polkit-1/rules.d/50-openauto-time.rules`:
```javascript
polkit.addRule(function(action, subject) {
    if (action.id === "org.freedesktop.timedate1.set-time" &&
        subject.user === "matt") {
        return polkit.Result.YES;
    }
});
```

### Safety Rules
- Only set clock if `|phone_time - pi_time| > 30 seconds`
- Reject backward jumps >5 minutes unless confirmed by 3 consecutive messages
- Log every adjustment with old time, new time, and delta
- Disable `systemd-timesyncd` when companion is providing time (re-enable on disconnect if NTP is available)

## Pairing

### First-Time Setup
1. User opens Companion app on phone
2. User navigates to pairing screen in Pi's settings (or web config)
3. Pi generates 6-digit numeric PIN and displays it
4. User enters PIN in Companion app
5. Both sides derive shared secret: `HKDF-SHA256(PIN + device_id + timestamp)`
6. Shared secret stored: Android Keystore (phone), `~/.openauto/companion.key` with 600 permissions (Pi)

### Re-pairing
- Either side can initiate re-pair (invalidates old secret)
- Pi shows new PIN, phone enters it
- Old sessions immediately rejected

## Android Permissions

| Permission | Purpose | API Level |
|-----------|---------|-----------|
| `INTERNET` | TCP + SOCKS5 proxy | All |
| `ACCESS_FINE_LOCATION` | GPS coordinates | All |
| `ACCESS_WIFI_STATE` | SSID detection | All |
| `ACCESS_NETWORK_STATE` | Network type detection | All |
| `FOREGROUND_SERVICE` | Keep service alive | 28+ |
| `FOREGROUND_SERVICE_LOCATION` | FGS with location access | 34+ |
| `POST_NOTIFICATIONS` | Foreground service notification | 33+ |
| `NEARBY_WIFI_DEVICES` | WiFi SSID access | 33+ |
| `CHANGE_NETWORK_STATE` | `requestNetwork` for cellular binding | All |

### FGS Compliance (Android 14+)
- Declare `foregroundServiceType="location"` in manifest
- Start foreground service with `FOREGROUND_SERVICE_TYPE_LOCATION`
- Location permission must be granted before FGS start

## Activation & Lifecycle

1. **Install:** User sideloads APK (no Play Store for v1)
2. **Permissions:** App requests location, notification permissions on first launch
3. **Pair:** Enter 6-digit PIN from Pi
4. **Auto-start:** `ConnectivityManager.NetworkCallback` registered for WiFi with SSID matching configured value (default "OpenAutoProdigy")
5. **On WiFi connect:** Foreground service starts → TCP connect to 10.0.0.1:9876 → handshake → data push begins. SOCKS5 proxy starts if enabled.
6. **On WiFi disconnect:** Both services stop. Pi-side CompanionListener detects TCP close, clears state.
7. **Battery:** Minimal — GPS listener at 5s interval, JSON serialization, TCP write. SOCKS5 proxy only active when Pi apps use it.

## Tech Stack

- **Language:** Kotlin
- **Min SDK:** 26 (Android 8.0 — 95%+ of AA-capable phones)
- **Target SDK:** 35 (current, required for Play Store if we go there later)
- **Build:** Gradle with Kotlin DSL
- **UI:** Jetpack Compose (single status screen + settings)
- **Dependencies:** None for v1 (SOCKS5 CONNECT-only is ~200-300 lines)
- **Testing:** Kotlin coroutines test, mock network for SOCKS5, instrumented test for FGS lifecycle

## v1 Scope

### In
- Time sync with >30s threshold and backward-jump protection
- GPS push with age tracking and staleness policy
- Battery level and charging state
- SOCKS5 proxy (CONNECT-only) with auth and egress ACLs
- Challenge-response auth + per-message HMAC
- Pairing via 6-digit PIN
- Auto-start on WiFi SSID detection
- Foreground service with notification
- Status UI (connected/disconnected, what's active)
- Pi-side CompanionListener with Q_PROPERTY bindings

### Out (future versions)
- Settings UI (WebView wrapper around web config)
- Notification forwarding from phone
- Media metadata push
- Full transparent internet (VpnService reverse tether — spike feasibility)
- Play Store distribution
- iOS companion app
- OTA update delivery to Pi

## Risk Register

| Risk | Severity | Mitigation |
|------|----------|------------|
| Dual-network routing fails on some OEMs | High | Explicit `Network.bindSocket()` to cellular. Test on Samsung S25 early. |
| SOCKS5 cleartext auth on local network | Medium | WPA2 on AP provides link-layer encryption. Acceptable for v1. |
| timedatectl polkit denied in service context | Medium | Ship polkit rule in installer. Test non-interactive D-Bus call. |
| Android kills foreground service | Medium | Proper FGS type, battery optimization exemption prompt. |
| Phone GPS fix unavailable (tunnels, parking garages) | Low | `age_ms` tracking, staleness policy, graceful degradation. |
| SOCKS5 server bugs (half-close, timeouts) | Medium | CONNECT-only scope, fuzz testing, connection limits. |

## References

- [Android VpnService API](https://developer.android.com/reference/kotlin/android/net/VpnService) — evaluated and deferred
- [Google vpn-reverse-tether](https://github.com/google/vpn-reverse-tether) — reference for future VPN spike
- [Gnirehtet](https://github.com/Genymobile/gnirehtet) — reverse tethering reference
- [SOCKS5 RFC 1928](https://datatracker.ietf.org/doc/html/rfc1928)
- [SOCKS5 username/password RFC 1929](https://datatracker.ietf.org/doc/rfc1929/)
- [Android FGS restrictions](https://developer.android.com/develop/background-work/services/fgs/restrictions-bg-start)
- [Android WiFi permissions](https://developer.android.com/develop/connectivity/wifi/wifi-permissions)
- [systemd-timedated D-Bus](https://www.freedesktop.org/software/systemd/man/org.freedesktop.timedate1.html)
- [apt SOCKS proxy](https://manpages.debian.org/trixie/apt/apt-transport-http.1.en.html)
