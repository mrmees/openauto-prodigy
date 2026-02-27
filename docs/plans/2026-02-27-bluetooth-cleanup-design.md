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
| Discoverability | Always discoverable; existing toggle repurposed to control Pairable | Phones always see the device; toggle controls whether new pairings are accepted |
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

## Component 5: Unified Device Name & Config Schema

### Install-Time Generation

The install script already generates a unique SSID (`OpenAutoProdigy-<HEX>`) via `head -c 2 /dev/urandom | xxd -p`. We unify this: generate one name used for both WiFi SSID and BT adapter alias.

**Changes to `install.sh` interactive prompts (Step 2):**
- Replace the current `VEHICLE_UUID` / `DEFAULT_SSID` generation with:
  ```bash
  DEVICE_NAME="Prodigy_$(head -c 4 /dev/urandom | xxd -p)"
  ```
- The prompt uses `DEVICE_NAME` as both the WiFi SSID default and BT name.
- `hostapd.conf` gets `ssid=$DEVICE_NAME`.

**Changes to `install.sh` config heredoc (Step 5):**
- Add `bt_name`, `bt_discoverable`, `auto_connect_aa`, `last_paired_mac`, and `last_paired_name` to the heredoc template:
  ```yaml
  connection:
    wifi_ap:
      interface: "${WIFI_IFACE:-wlan0}"
      ssid: "$DEVICE_NAME"
      password: "$WIFI_PASS"
    tcp_port: $TCP_PORT
    bt_name: "$DEVICE_NAME"
    bt_discoverable: true
    auto_connect_aa: true
    last_paired_mac: ""
    last_paired_name: ""
  ```
- No `yq` dependency needed — everything goes through the existing heredoc flow.

**Changes to `install.sh` polkit (Step 5):**
- Install a BlueZ polkit rule alongside the existing companion polkit rule:
  ```bash
  # BlueZ agent polkit rule (allows non-root RequestDefaultAgent)
  sudo cp "$INSTALL_DIR/config/bluez-agent-polkit.rules" \
      /etc/polkit-1/rules.d/50-openauto-bluez.rules
  ```

### Runtime

- `BluetoothManager` reads `connection.bt_name` → sets `Adapter1.Alias` via D-Bus
- `BluetoothDiscoveryService` already reads `connection.wifi_ap.ssid` for RFCOMM credential exchange
- Both fields are set to the same value at install time; divergence only if user edits config manually
- If config key missing, fallback: `OpenAutoProdigy`

### Name Changes

- BT alias updates live (D-Bus property set, no restart)
- WiFi SSID requires hostapd restart (can't change SSID on a running AP)
- Web panel handles the restart; on-head-unit settings shows the name read-only

### Config Schema Summary

| Key | Type | Default | Written by |
|-----|------|---------|-----------|
| `connection.bt_name` | string | `Prodigy_<8-hex>` | install.sh heredoc |
| `connection.bt_discoverable` | bool | `true` | install.sh heredoc |
| `connection.auto_connect_aa` | bool | `true` | install.sh heredoc |
| `connection.last_paired_mac` | string | `""` | BluetoothManager at runtime |
| `connection.last_paired_name` | string | `""` | BluetoothManager at runtime |
| `connection.wifi_ap.ssid` | string | `Prodigy_<8-hex>` (same) | install.sh heredoc |

---

## Component 6: Connection Settings UI Updates

**File:** `qml/applications/settings/ConnectionSettings.qml`

### Changes to Existing Controls

- **Discoverable toggle** → repurposed: label changes to "Accept New Pairings", config path changes to `connection.bt_pairable` (or we drive it directly via BluetoothManager signal, not config). When toggled ON, BluetoothManager sets `Adapter1.Pairable=true` with `PairableTimeout=120`. Auto-toggles OFF after timeout.

### New Controls (Bluetooth section)

| Control | Type | Binding | Behavior |
|---------|------|---------|----------|
| Device Name | ReadOnlyField | `connection.bt_name` | Display only; edit via web panel |
| Accept New Pairings | SettingsToggle | BluetoothManager.pairable | Repurposed from Discoverable; controls Pairable with 120s auto-off |
| Connected Device | ReadOnlyField | BluetoothManager.connectedDeviceName | Shows name of connected phone, or "None" |

### Layout

```
SectionHeader: "Bluetooth"
  ReadOnlyField: Device Name
  SettingsToggle: Accept New Pairings
  ReadOnlyField: Connected Device
```

**Note on "Paired Devices" list:** The design scope is single paired phone (head unit use case). A paired devices list with Forget implies multi-device management, which contradicts the single-phone scope. Deferred — if we add Forget functionality later, it goes in the web panel where the user has more screen space and isn't driving.

---

## Component 7: AA Bluetooth Channel Reconciliation

**File:** `libs/open-androidauto/src/HU/Handlers/BluetoothChannelHandler.cpp`

### Current Behavior

The AA protocol's Bluetooth channel handler always responds `already_paired: true` to `PAIRING_REQUEST` messages from the phone. This was correct when pairing was always handled externally (pre-paired via `bluetoothctl`).

### Required Change

With the new pairing UX, the response should reflect actual pairing state:
- If phone's BT MAC is already paired in BlueZ → respond `already_paired: true` (current behavior)
- If phone is not yet paired → respond `already_paired: false`, which tells the phone to initiate BT pairing

This is a **future refinement**, not a blocker for initial implementation. The initial BT cleanup can ship with `already_paired: true` (existing behavior) since pairing still happens at the BT level before AA connects. The AA pairing request is a secondary confirmation, not the primary pairing mechanism. We'll revisit once the BluetoothManager is stable.

---

## Component 8: BluetoothManager Lifecycle & Ownership

### Bootstrap (in `main.cpp`)

BluetoothManager is a **core service**, not tied to any plugin's lifecycle. It must be created and initialized early — before any plugin that uses BT (AndroidAutoPlugin, BtAudioPlugin, PhonePlugin).

```
main.cpp startup sequence:
  1. ConfigService (reads YAML)
  2. ThemeService
  3. AudioService (PipeWire)
  4. BluetoothManager::create() ← NEW
  5. BluetoothManager::initialize() (adapter setup, agent, profiles, auto-connect)
  6. HostContext::setBluetoothService(btManager)
  7. PluginManager::initializeAll() (plugins get IHostContext with BT service ready)
```

### Shutdown

```
main.cpp shutdown:
  1. PluginManager::shutdownAll()
  2. BluetoothManager::shutdown() (unregister agent, profiles, cancel auto-connect)
  3. AudioService shutdown
  4. ...
```

### QML Exposure

BluetoothManager needs Q_PROPERTYs for QML binding in Connection settings:
- `pairable` (bool, read/write) — drives the toggle
- `connectedDeviceName` (QString, read-only, NOTIFY) — for the connected device field
- `pairingActive` (bool, read-only, NOTIFY) — true when pairing dialog should show
- `pairingDeviceName` (QString) — device requesting pairing
- `pairingPasskey` (QString) — 6-digit code to display

Exposed to QML via `rootContext()->setContextProperty("BluetoothManager", btManager)` in main.cpp.

---

---

## Codex Review Findings (incorporated above)

### Review Round 1

1. **HIGH — `Connected=true` unreliable for AA**: Dual-mode phones can report Connected on BLE bearer without AA coming up. Fixed: use RFCOMM `NewConnection` as stop criterion.
2. **HIGH — BlueZ restart recovery underspecified**: Fixed: added `QDBusServiceWatcher` on `org.bluez` with full re-init state machine.
3. **HIGH — Agent default requires polkit**: Non-root `RequestDefaultAgent` fails. Fixed: install polkit policy via install.sh.
4. **HIGH — Startup window without HFP/HSP**: ~8s gap real. Fixed: keep Python registration during validation phase, remove only after confirming C++ handles traffic.
5. **MEDIUM — Overlapping Connect() calls**: Fixed: gate to one in-flight attempt.
6. **MEDIUM — Validation doesn't exercise C++ handlers**: "Already registered" means Python wins. Fixed: validation step 4 disables Python first.
7. **MEDIUM — Agent Cancel/timeout handling**: BlueZ has ~60s internal timeout. Fixed: track pending request, handle Cancel, ignore late UI responses.
8. **MEDIUM — Adapter property sequencing**: Fixed: explicit power-on → wait → timeouts → booleans ordering.
9. **LOW — AutoConnect option for server role**: BlueZ Profile API says it's client-only. Fixed: removed from profile options.

### Review Round 2

1. **HIGH — Config schema not implementation-ready**: New keys (`bt_name`, `last_paired_mac`, `last_paired_name`) not defined in install heredoc. Design used `yq` which isn't a dep. Fixed: all new keys added to existing heredoc template, no `yq` needed.
2. **HIGH — Discoverable toggle contradicts "always discoverable"**: Fixed: toggle repurposed to control `Pairable`, not `Discoverable`. Adapter is always discoverable; toggle gates new pairing acceptance with 120s auto-off.
3. **HIGH — BT restart recovery removes Python callback without replacement**: Fixed: `QDBusServiceWatcher` on `org.bluez` triggers full re-init in C++. Python health monitor keeps adapter-level recovery (hci0 up, /var/run/sdp perms).
4. **MEDIUM — BluetoothManager lifecycle/ownership not defined**: Fixed: added Component 8 with explicit main.cpp bootstrap sequence, shutdown order, and QML property exposure.
5. **MEDIUM — AA channel `already_paired` not reconciled with new pairing UX**: Fixed: added Component 7. Initial implementation keeps `already_paired: true` (safe default). Future refinement to check actual BlueZ pairing state.
6. **LOW — Paired devices list contradicts single-phone scope**: Fixed: removed paired devices list from on-screen UI. Forget functionality deferred to web panel.

---

## What This Design Does NOT Cover

- **HFP call audio** (SCO routing, AT commands) — separate roadmap item, depends on this cleanup
- **Audio equalizer** — unrelated
- **Multi-phone support** — single paired phone only (head unit use case)
- **BLE advertising** — existing `IBluetoothService` has start/stopAdvertising stubs; not needed for classic BT profiles
