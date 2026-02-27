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

---

## Component 1: BluetoothManager Service

**Files:** `src/core/services/BluetoothManager.hpp`, `src/core/services/BluetoothManager.cpp`

Expands the existing `IBluetoothService` interface. Registered in `IHostContext` via `HostContext::setBluetoothService()`.

### Responsibilities

1. **Adapter setup** — On startup: power on adapter, set `Alias` from `connection.bt_name`, set `Discoverable=true` with `DiscoverableTimeout=0`, set `Pairable=false` (until user requests).
2. **Profile registration** — Register HFP AG + HSP HS via `ProfileManager1` D-Bus. Includes `BluezProfile1Adaptor` (fd-holding logic, moved from `BluetoothDiscoveryService`).
3. **Pairing agent** — Implement `org.bluez.Agent1` D-Bus interface. `RequestConfirmation(device, passkey)` emits Qt signal; QML overlay shows confirm/reject; D-Bus method return sent on user action or 30s timeout.
4. **Pairable toggle** — `setPairable(bool)` sets `org.bluez.Adapter1.Pairable` D-Bus property. Auto-disables after 120s timeout.
5. **Auto-connect** — On startup, read `connection.last_paired_mac`. If set and `connection.auto_connect_aa` is true, attempt `org.bluez.Device1.Connect()` with backoff schedule. Cancel on `Connected=true` property change or pairing mode entry.
6. **Connection state** — Watch `org.bluez.Device1.PropertiesChanged` for `Connected` changes. Emit `deviceConnected(name, address)` / `deviceDisconnected(name, address)`.
7. **MAC persistence** — On successful pairing, write `connection.last_paired_mac` and `connection.last_paired_name` to config.

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
5. Overlay dismisses

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
4. Each attempt: `org.bluez.Device1.Connect()` via D-Bus async call
5. Cancel on:
   - `Device1.PropertiesChanged` → `Connected=true` (phone connected from either side)
   - User enters pairing mode (don't fight a new pairing with reconnect attempts)
   - App shutdown

### Edge Cases

- **Phone already connected** (phone was faster): `PropertiesChanged` watch fires, retry never starts or cancels immediately.
- **Phone out of range**: Retries exhaust, device stays discoverable for passive reconnect.
- **BT adapter restart** (health monitor recovery): BluetoothManager re-registers profiles and restarts auto-connect loop.

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

### Remove from Python system service

- Delete `system-service/bt_profiles.py`
- Remove `BtProfileManager` from `openauto_system.py`
- Remove `bt_restart_callback` from health monitor (or repurpose for BluetoothManager notification)

### Stays in BluetoothDiscoveryService

- AA SDP record registration (coupled to RFCOMM server channel number and session lifetime)
- RFCOMM server, WiFi credential exchange — all AA-wireless-specific logic

### Validation Strategy

1. Add C++ profile registration in BluetoothManager
2. Run on Pi with Python registration still active (BlueZ returns "already registered" — harmless)
3. Verify phone connects, profiles show in `bluetoothctl info`
4. Remove Python registration
5. Verify again

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

## What This Design Does NOT Cover

- **HFP call audio** (SCO routing, AT commands) — separate roadmap item, depends on this cleanup
- **Audio equalizer** — unrelated
- **Multi-phone support** — single paired phone only (head unit use case)
- **BLE advertising** — existing `IBluetoothService` has start/stopAdvertising stubs; not needed for classic BT profiles
