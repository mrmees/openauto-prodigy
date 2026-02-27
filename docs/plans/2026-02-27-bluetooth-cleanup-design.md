# Bluetooth Cleanup — Design Document

**Date:** 2026-02-27
**Status:** Approved

## Goal

Bring the Pi's Bluetooth stack to head-unit quality: proper profile advertising, user-friendly pairing with on-screen confirmation, auto-reconnect on startup, unified device name across BT and WiFi, and consolidated profile ownership in the C++ app.

## Architecture

A new `BluetoothManager` core service (C++/Qt) becomes the single owner of all adapter-level BT concerns. It replaces the Python system service's profile registration and the duplicated HFP/HSP registration in `BluetoothDiscoveryService`. The Python system service retains only health monitoring (adapter up, `/var/run/sdp` permissions).

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Profile registration owner | C++ BluetoothManager | Single process owns BT state; eliminates Python/C++ duplication |
| Pairing UX | 6-digit confirmation dialog | Matches real head unit behavior; secure without being cumbersome |
| Discoverability | Always discoverable, pairable on demand | Phones can see the device anytime; pairing requires user action |
| Auto-connect | Active reconnect + discoverable | Faster reconnect than passive-only; covers "phone boots slower than Pi" |
| Retry policy | Backoff: 5s x6, 30s x4, 60s x3 (~5 min) | Covers slow phone boot without wasting BT bandwidth indefinitely |
| Adapter/WiFi name | Unified `Prodigy_<8-hex>`, generated at install | Avoids confusion; unique per vehicle for multi-install deployments |
| Consolidation safety | Validate C++ registration on Pi before removing Python | Belt-and-suspenders during transition |
| Auto-connect success criterion | RFCOMM NewConnection (not generic Connected) | `Device1.Connected=true` can fire on wrong bearer (BLE vs BR/EDR) |
| Pairable timeout | Adapter-side `PairableTimeout=120` | Survives app crash; app-only QTimer leaves adapter pairable if we die |
| BlueZ restart recovery | `QDBusServiceWatcher` on `org.bluez` | Re-run full init (agent, profiles, adapter props, reconnect) on daemon restart |
| Polkit for Agent1 | Install polkit policy via install.sh | Non-root `RequestDefaultAgent` returns NotAuthorized without it |

---

## Component 1: BluetoothManager Service

**Files:** `src/core/services/BluetoothManager.hpp`, `src/core/services/BluetoothManager.cpp`

Expands the existing `IBluetoothService` interface. Registered in `IHostContext` via `HostContext::setBluetoothService()`.

### Responsibilities

1. **Adapter setup** — Sequenced startup: power on adapter → wait for `Powered=true` → set `Alias` from `connection.bt_name` → set `DiscoverableTimeout=0` and `PairableTimeout=120` → set `Discoverable=true`, `Pairable=false`. Explicit ordering prevents race conditions on slow adapter init.
2. **Profile registration** — Register HFP AG + HSP HS via `ProfileManager1` D-Bus. Includes `BluezProfile1Adaptor` (fd-holding logic, moved from `BluetoothDiscoveryService`). Remove `AutoConnect` option (it's for client role, not server).
3. **Pairing agent** — Implement `org.bluez.Agent1` D-Bus interface with `RequestConfirmation`, `AuthorizeService`, `Release`, and `Cancel`. Track pending request ID to handle BlueZ-initiated cancellation (BlueZ has its own ~60s timeout, not configurable via Agent1 API). On `Cancel`, dismiss dialog and discard any late UI responses.
4. **Pairable toggle** — `setPairable(bool)` sets `org.bluez.Adapter1.Pairable` D-Bus property. Use adapter-side `PairableTimeout=120` (survives app crashes) rather than app-only QTimer.
5. **Auto-connect** — On startup, read `connection.last_paired_mac`. If set and `connection.auto_connect_aa` is true, attempt `org.bluez.Device1.Connect()` with backoff schedule. Gate to one in-flight attempt (schedule next only after completion/error to avoid `InProgress` spam). Cancel on RFCOMM `NewConnection` (AA-specific readiness) or pairing mode entry. Do NOT use generic `Device1.Connected=true` — on dual-mode phones it can fire on wrong bearer (BLE vs BR/EDR) without AA coming up.
6. **Connection state** — Watch `org.bluez.Device1.PropertiesChanged` for `Connected` changes. Emit `deviceConnected(name, address)` / `deviceDisconnected(name, address)`.
7. **MAC persistence** — On successful pairing, write `connection.last_paired_mac` and `connection.last_paired_name` to config. Also set `Trusted=true` on the device so BlueZ allows auto-reconnect without re-pairing.
8. **BlueZ restart recovery** — `QDBusServiceWatcher` monitors `org.bluez` name ownership. On daemon restart, re-run full init: register agent + request default, register profiles, set adapter properties, restart auto-connect loop.
9. **Polkit policy** — `install.sh` installs a polkit rule allowing the app user to call `RequestDefaultAgent`. Without this, non-root agent registration fails with `NotAuthorized`.

### D-Bus Interfaces Used

| Interface | Purpose |
|-----------|---------|
| `org.bluez.Adapter1` | Properties: Alias, Discoverable, DiscoverableTimeout, Pairable, Powered |
| `org.bluez.ProfileManager1` | RegisterProfile / UnregisterProfile for HFP AG, HSP HS |
| `org.bluez.Agent1` | Implement: RequestConfirmation, AuthorizeService, Release, Cancel |
| `org.bluez.AgentManager1` | RegisterAgent / RequestDefaultAgent |
| `org.bluez.Device1` | Connect(), Properties: Connected, Name, Address, Paired, Trusted |
| `org.freedesktop.DBus.Properties` | PropertiesChanged signal for live state updates |

---

## Component 2: Pairing Dialog

**File:** `qml/components/PairingDialog.qml`

Modal overlay at Shell level (not inside settings — pairing requests can arrive during AA projection or any screen).

### Flow

1. `BluetoothManager.pairingRequested(deviceName, passkey)` signal
2. Shell shows PairingDialog overlay with:
   - Device name (e.g., "Pixel 8")
   - 6-digit passkey displayed prominently
   - Confirm button (large, UiMetrics-sized)
   - Reject button
3. User taps Confirm → `BluetoothManager.confirmPairing()` → D-Bus success reply
4. User taps Reject or 30s timeout → `BluetoothManager.rejectPairing()` → D-Bus rejection
5. BlueZ calls `Agent1.Cancel()` (e.g., phone disconnects mid-pair or BlueZ's ~60s timeout) → dialog auto-dismisses, late UI taps ignored
6. Overlay dismisses

### Design Constraints

- Touch targets: minimum `UiMetrics.rowH` height
- Passkey font: `UiMetrics.fontHeader` or larger — must be readable from driver distance
- Overlay blocks touch passthrough to AA (same z-order approach as GestureOverlay)

---

## Component 3: Auto-Connect

**Lives inside:** `BluetoothManager`

### Startup Sequence

1. Read `connection.last_paired_mac` from config
2. If empty or `connection.auto_connect_aa` is false, skip — just stay discoverable
3. Start QTimer-based retry loop:
   - Phase 1: every 5s, 6 attempts (30s)
   - Phase 2: every 30s, 4 attempts (2 min)
   - Phase 3: every 60s, 3 attempts (3 min)
   - Total window: ~5.5 minutes
4. Each attempt: `org.bluez.Device1.Connect()` via D-Bus async call. Gate to one in-flight attempt — schedule next only after previous completes or errors (avoids `InProgress` D-Bus errors when phone is out of range).
5. Cancel on:
   - RFCOMM `NewConnection` from `BluezProfile1Adaptor` (AA-specific readiness — phone connected on the right profile via BR/EDR)
   - User enters pairing mode (don't fight a new pairing with reconnect attempts)
   - App shutdown

**Why not `Connected=true`?** On dual-mode phones, `Device1.Connect()` can land on BLE transport and report `Connected=true` without any classic profiles coming up. We already hit this — see MEMORY.md reconnect notes about needing A2DP UUID to force BR/EDR. RFCOMM NewConnection is the definitive signal that the phone is connected on the right bearer for AA.

### Edge Cases

- **Phone already connected** (phone was faster): RFCOMM `NewConnection` fires from BluetoothDiscoveryService, retry never starts or cancels immediately.
- **Phone out of range**: Retries exhaust, device stays discoverable for passive reconnect.
- **BT adapter restart** (BlueZ daemon restart): `QDBusServiceWatcher` triggers full re-init including auto-connect restart.
- **Connect() returns InProgress**: Previous attempt still pending. Ignored — next retry scheduled after current attempt resolves.

---

## Component 4: Profile Consolidation

### Move to BluetoothManager (C++)

- HFP AG profile registration (`ProfileManager1.RegisterProfile`)
- HSP HS profile registration (`ProfileManager1.RegisterProfile`)
- `BluezProfile1Adaptor` QObject (NewConnection fd-hold, RequestDisconnection, Release)

### Remove from BluetoothDiscoveryService

- `registerBluetoothProfiles()` / `unregisterBluetoothProfiles()` methods
- `BluezProfile1Adaptor` class
- `profileObjects_`, `profileFds_`, `registeredProfilePaths_` members

### Remove from Python system service (Phase 2 — after Pi validation)

- Delete `system-service/bt_profiles.py`
- Remove `BtProfileManager` from `openauto_system.py`
- Remove `bt_restart_callback` from health monitor

**Note:** During validation, keep Python registration active as a safety net. The ~8s startup window where only Python has profiles registered is acceptable. Remove Python only after confirming C++ handles all traffic correctly.

### Stays in BluetoothDiscoveryService

- AA SDP record registration (coupled to RFCOMM server channel number and session lifetime)
- RFCOMM server, WiFi credential exchange — all AA-wireless-specific logic

### Validation Strategy

1. Add C++ profile registration in BluetoothManager
2. Run on Pi with Python registration still active (BlueZ returns "already registered" — harmless)
3. Verify phone connects, profiles show in `bluetoothctl info`
4. **Disable Python registration** (comment out in openauto_system.py) so C++ `BluezProfile1Adaptor` actually receives `NewConnection` traffic — "already registered" means Python's adaptor wins
5. Verify phone connects and C++ adaptor handles the fd correctly
6. Delete `bt_profiles.py` and clean up openauto_system.py

---

## Component 5: Unified Device Name

### Install-Time Generation

In `install.sh`, during first install:

```bash
if ! grep -q 'bt_name' "$CONFIG_FILE" 2>/dev/null; then
    DEVICE_NAME="Prodigy_$(head -c 4 /dev/urandom | xxd -p)"
    # Write to both bt_name and wifi SSID
    yq -i ".connection.bt_name = \"$DEVICE_NAME\"" "$CONFIG_FILE"
    yq -i ".connection.wifi_ap.ssid = \"$DEVICE_NAME\"" "$CONFIG_FILE"
fi
```

Also written to `hostapd.conf` as the SSID.

### Runtime

- `BluetoothManager` reads `connection.bt_name` → sets `Adapter1.Alias` via D-Bus
- `BluetoothDiscoveryService` already reads `connection.wifi_ap.ssid` for RFCOMM credential exchange
- If config key missing, fallback: `OpenAutoProdigy`

### Name Changes

- BT alias updates live (D-Bus property set, no restart)
- WiFi SSID requires hostapd restart (can't change SSID on a running AP)
- Web panel handles the restart; on-head-unit settings shows the name read-only

---

## Component 6: Connection Settings UI Updates

**File:** `qml/applications/settings/ConnectionSettings.qml`

### New Controls (Bluetooth section)

| Control | Type | Config Key | Behavior |
|---------|------|-----------|----------|
| Device Name | ReadOnlyField | `connection.bt_name` | Display only; edit via web panel |
| Pair New Device | Action button | — | Sets `Pairable=true` for 120s, shows "Ready to pair..." indicator |
| Connected Device | ReadOnlyField | — | Shows name of connected phone, or "None" |
| Paired Devices | List | — | Shows paired device names with "Forget" action |

### Layout

```
SectionHeader: "Bluetooth"
  ReadOnlyField: Device Name
  SettingsToggle: Discoverable          (existing)
  ActionButton: Pair New Device
  ReadOnlyField: Connected Device
  [Paired device list with Forget buttons]
```

---

---

## Codex Review Findings (incorporated above)

The following issues were identified during automated review and folded into the design:

1. **HIGH — `Connected=true` unreliable for AA**: Dual-mode phones can report Connected on BLE bearer without AA coming up. Fixed: use RFCOMM `NewConnection` as stop criterion.
2. **HIGH — BlueZ restart recovery underspecified**: Fixed: added `QDBusServiceWatcher` on `org.bluez` with full re-init state machine.
3. **HIGH — Agent default requires polkit**: Non-root `RequestDefaultAgent` fails. Fixed: install polkit policy via install.sh.
4. **HIGH — Startup window without HFP/HSP**: ~8s gap real. Fixed: keep Python registration during validation phase, remove only after confirming C++ handles traffic.
5. **MEDIUM — Overlapping Connect() calls**: Fixed: gate to one in-flight attempt.
6. **MEDIUM — Validation doesn't exercise C++ handlers**: "Already registered" means Python wins. Fixed: validation step 4 disables Python first.
7. **MEDIUM — Agent Cancel/timeout handling**: BlueZ has ~60s internal timeout. Fixed: track pending request, handle Cancel, ignore late UI responses.
8. **MEDIUM — Adapter property sequencing**: Fixed: explicit power-on → wait → timeouts → booleans ordering.
9. **LOW — AutoConnect option for server role**: BlueZ Profile API says it's client-only. Fixed: removed from profile options.

---

## What This Design Does NOT Cover

- **HFP call audio** (SCO routing, AT commands) — separate roadmap item, depends on this cleanup
- **Audio equalizer** — unrelated
- **Multi-phone support** — single paired phone only (head unit use case)
- **BLE advertising** — existing `IBluetoothService` has start/stopAdvertising stubs; not needed for classic BT profiles
