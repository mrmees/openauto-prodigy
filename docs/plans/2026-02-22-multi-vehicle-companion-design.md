# Multi-Vehicle Companion App Support — Design Document

**Date:** 2026-02-22
**Status:** Approved, pending implementation

## Problem

The companion app currently supports exactly one vehicle. Users with multiple OpenAuto Prodigy installs (multiple cars, bench + car, etc.) cannot pair to more than one head unit. The app also defaults all Pis to the same SSID ("OpenAutoProdigy"), making it impossible for the app to distinguish between vehicles.

## Design

### Pi-Side: Install Script SSID Default

**Current:** `install.sh` prompts `WiFi hotspot SSID [OpenAutoProdigy]:` with a static default.

**Change:** Generate a 4-char hex ID at install time and include it in the default SSID.

```bash
VEHICLE_UUID=$(head -c 2 /dev/urandom | xxd -p | tr '[:lower:]' '[:upper:]')
# Default becomes e.g. "OpenAutoProdigy-A3F2"

read -p "WiFi hotspot SSID [OpenAutoProdigy-$VEHICLE_UUID]: " WIFI_SSID
WIFI_SSID=${WIFI_SSID:-OpenAutoProdigy-$VEHICLE_UUID}
```

The prompt explains:
> The companion app uses the SSID to identify this vehicle. The default includes a unique suffix to avoid conflicts if you have multiple vehicles. If you enter a custom name, make sure it's unique across your vehicles.

If the user enters a custom SSID, it's used verbatim — no suffix appended. They own uniqueness.

**No other Pi-side changes.** The Pi doesn't know or care about multi-vehicle. Each Pi is its own world.

### Companion App: Multi-Vehicle Storage

**Current `CompanionPrefs`:**
```kotlin
sharedSecret: String      // single value
targetSsid: String        // single value
socks5Enabled: Boolean    // single value
```

**Proposed `CompanionPrefs`:**
```kotlin
data class Vehicle(
    val id: String,             // UUID for internal reference
    val ssid: String,           // WiFi SSID to match (e.g. "OpenAutoProdigy-A3F2")
    val name: String,           // user-friendly label (defaults to SSID)
    val sharedSecret: String,   // derived from pairing PIN
    val socks5Enabled: Boolean = true
)

// Stored as JSON array in SharedPreferences
var vehicles: List<Vehicle>
val isPaired: Boolean get() = vehicles.isNotEmpty()
```

**Migration:** On first launch after update, if legacy single-vehicle prefs exist (`shared_secret` is non-empty), automatically create a one-item vehicle list from them. Migration is idempotent — if vehicles list already exists, skip. Handle partial legacy state (secret but no SSID, or vice versa) gracefully.

**Soft cap:** 20 vehicles. More than enough for any real use case, prevents UI/perf edge cases.

### Companion App: Connection Logic

**Current `WifiMonitor`:** Watches for one `targetSsid`, starts `CompanionService` with one `sharedSecret`.

**Proposed:** `WifiMonitor` receives the full vehicle list. On WiFi capability change:
1. Read SSID from `NetworkCapabilities.transportInfo`
2. Match against all paired vehicles' SSIDs
3. If match found, start `CompanionService` with that vehicle's secret and name
4. If no match, stop service (if running)

**Race condition handling:** Use a connection generation counter. Each start increments the generation; async callbacks from stale generations are ignored. This prevents flapping when the phone briefly sees multiple networks.

**SSID edge cases:** Android may report `null` or `<unknown ssid>` transiently. Ignore these — only act on known, quoted SSID strings that match a paired vehicle.

### Companion App: SOCKS5 Per-Vehicle

Each vehicle already gets its own `sharedSecret`, so SOCKS5 credentials are naturally per-vehicle (username `oap`, password = first 8 chars of that vehicle's secret). No change needed beyond using the correct vehicle's secret when starting the proxy.

The `socks5Enabled` preference is per-vehicle, allowing users to enable internet sharing for one car but not another.

### Companion App: Service Notification

**Current:** Notification says "Connected to OpenAuto Prodigy".

**Proposed:** Notification says "Connected to {vehicle.name}" (e.g. "Connected to Miata").

### Companion App: UI Changes

**Three screens:**

1. **VehicleListScreen** (new) — Home screen when at least one vehicle is paired.
   - Shows all paired vehicles in a list
   - Each row: vehicle name, SSID (smaller), connection indicator (green dot if connected)
   - Tap a vehicle → StatusScreen for that vehicle
   - "Add Vehicle" button at bottom
   - Long-press or swipe-to-delete to unpair a single vehicle

2. **PairingScreen** (modified) — Now also collects the vehicle's WiFi SSID.
   - Text field for SSID (pre-filled if phone is currently connected to an unrecognized AP matching `OpenAutoProdigy*` pattern)
   - Optional friendly name field (defaults to SSID)
   - Existing PIN field
   - Still shown as the only screen on first launch (no vehicles paired)

3. **StatusScreen** (modified) — Shows which vehicle is connected.
   - Header shows vehicle name instead of generic "OpenAuto Companion"
   - "Unpair" becomes per-vehicle (removes this vehicle from the list, returns to VehicleListScreen)
   - All sharing status rows and settings work as today

**Navigation flow:**
```
First launch (no vehicles):
  PairingScreen → VehicleListScreen

Normal use (vehicles paired):
  VehicleListScreen → StatusScreen (tap vehicle)
  VehicleListScreen → PairingScreen (tap "Add Vehicle")
```

### What's NOT Changing

- **Pi-side code** — No changes to `CompanionListenerService`, config schema, or AA protocol
- **Companion protocol** — Same challenge/response handshake, same status push format
- **SOCKS5 server** — Same implementation, credentials naturally vary per vehicle
- **Bluetooth discovery** — Unchanged, still per-Pi

## Security Notes

- App-layer auth (HMAC challenge-response) prevents connecting to wrong Pi even if SSIDs collide
- SOCKS5 credentials are per-vehicle (derived from per-vehicle secret)
- SSID collision between two user-owned Pis: companion connects to whichever AP the phone joins; auth will succeed only for the matching Pi. Worst case: connection fails, retries on next WiFi event.

## Files Affected

| File | Change |
|------|--------|
| `install.sh` | SSID prompt with UUID default |
| `CompanionPrefs.kt` | Vehicle list storage, migration |
| `WifiMonitor.kt` | Multi-SSID matching, generation counter |
| `CompanionService.kt` | Accept vehicle name in intent, per-vehicle notification |
| `MainActivity.kt` | Navigation between 3 screens, vehicle-aware state |
| `VehicleListScreen.kt` | New file |
| `PairingScreen.kt` | Add SSID + name fields |
| `StatusScreen.kt` | Vehicle name in header, per-vehicle unpair |
