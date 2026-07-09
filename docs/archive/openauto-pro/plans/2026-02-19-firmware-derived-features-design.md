# Firmware-Derived Protocol Features — Design Document

**Date:** 2026-02-19
**Status:** Approved
**Target:** OpenAuto Prodigy v0.3.0
**Sources:** Cross-vendor firmware analysis (Alpine iLX-W650BT, Kenwood DNX/DDX, Sony XAV-AX100, Pioneer DMH-WT8600NEX)

## Summary

Four firmware analyses revealed production AA protocol features that Prodigy should implement. This document specifies six features to build now and two to investigate after Pixel 6 proto extraction. All changes target the existing Prodigy codebase on branch `arch/plugin-system`.

---

## Build Now

### 1. Night Mode Sensor — Live Updates

**Problem:** SensorService sends a single static `NIGHT_DATA = day` on connect and never updates. Every production HU sends live night mode updates. Without them, AA is stuck in day mode permanently.

**Design:** Three configurable night mode sources behind a `NightModeProvider` interface:

| Source | Trigger | Config Key |
|--------|---------|------------|
| **Time-based** (default) | Sunrise/sunset calculation from configurable lat/lon, or fixed times | `sensors.night_mode.source: time` |
| **Theme follower** | Follows ThemeService day/night toggle (user-initiated) | `sensors.night_mode.source: theme` |
| **GPIO** | Reads a GPIO pin connected to the car's illumination wire (12V → 3.3V via voltage divider) | `sensors.night_mode.source: gpio` |

**Config (config.yaml):**
```yaml
sensors:
  night_mode:
    source: time          # time | theme | gpio
    # Time-based settings
    day_start: "07:00"    # 24h format, local time
    night_start: "19:00"
    # GPIO settings
    gpio_pin: 17          # BCM pin number
    gpio_active_high: true  # true = HIGH means headlights on
```

**Architecture:**
```
NightModeProvider (interface)
  ├── TimedNightMode      — QTimer checks every 60s against configured times
  ├── ThemeNightMode      — Connects to ThemeService::nightModeChanged signal
  └── GpioNightMode       — Polls /sys/class/gpio/gpioN/value every 1s

SensorService
  └── owns NightModeProvider*
  └── on provider signal: sends SensorEventIndication(NIGHT_DATA, {is_night})
```

**Integration point:** `ServiceFactory.cpp` — SensorService already exists, just needs the provider wired in and periodic updates instead of one-shot.

**What changes:**
- New: `NightModeProvider.hpp` (interface), `TimedNightMode.cpp`, `ThemeNightMode.cpp`, `GpioNightMode.cpp`
- Modified: `ServiceFactory.cpp` (SensorService section), `config.yaml` (new `sensors` block)

---

### 2. Video Focus State Machine — NATIVE_TRANSIENT

**Problem:** Prodigy sends binary FOCUSED/UNFOCUSED. Production HUs (confirmed via Alpine firmware) use three states: PROJECTION, NATIVE, NATIVE_TRANSIENT. Without NATIVE_TRANSIENT, interrupting AA video (e.g., reverse camera) causes a full unfocus/refocus cycle instead of a smooth return.

**Design:** Add a `VideoFocusMode` enum and expose it to the AA entity:

```cpp
enum class VideoFocusMode {
    Projection,         // AA is active and rendering (current FOCUSED)
    Native,             // HU's own UI, AA session paused (current UNFOCUSED)
    NativeTransient     // Brief interruption, AA should keep session alive
};
```

**Usage scenarios:**
- **Reverse camera:** Send NATIVE_TRANSIENT when shift-to-reverse GPIO triggers, send PROJECTION when gear returns to drive. AA keeps its session and resumes rendering instantly.
- **Incoming call overlay:** PhonePlugin's IncomingCallOverlay sends NATIVE_TRANSIENT. When call ends or user dismisses, send PROJECTION.
- **App switching in launcher:** Send NATIVE when user navigates away from AA plugin, PROJECTION when they return.

**What the phone does with each state:**
- PROJECTION: Phone renders and streams H.264
- NATIVE_TRANSIENT: Phone keeps rendering internally but expects HU to return soon — no teardown
- NATIVE: Phone may stop rendering to save battery

**Integration point:** `VideoService.cpp` — modify `sendVideoFocusIndication()` to accept the mode enum. `AndroidAutoPlugin.cpp` — expose method for other plugins to request focus changes.

**What changes:**
- New: `VideoFocusMode` enum (in `VideoService.hpp` or a shared types header)
- Modified: `VideoService.cpp` (focus indication method), `AndroidAutoPlugin.cpp` (expose to plugin system)

---

### 3. Resolution Advertisement — Full Spectrum

**Problem:** Prodigy only advertises 1280x720. Production SDKs always include 800x480 as mandatory fallback. Alpine firmware logs a warning when it's missing: *"Mandatory 800x480 resolution was missing in the XML. It is added by the stack."*

**Design:** Advertise multiple resolutions in priority order. Make the preferred resolution configurable.

**Resolutions to advertise (from firmware analysis + aasdk enums):**

| Resolution | Aspect | Enum Value | Priority | Notes |
|-----------|--------|------------|----------|-------|
| 1920x1080 | 16:9 | (TBD from Pixel extraction) | Optional | Only if configured |
| 1280x720 | 16:9 | (known) | Default preferred | Current behavior |
| 800x480 | 5:3 | (known) | Mandatory | Every production SDK requires this |

Portrait variants (480x800, 720x1280, 1080x1920) exist in the protocol but are irrelevant for a landscape-mounted car display. Skip for now.

**After Pixel 6 extraction:** Fill in all enum values from the canonical proto definitions. There may be additional resolutions (e.g., 960x540, 1600x900) we don't know about yet.

**Config:**
```yaml
video:
  resolution: 720p    # preferred: 480p | 720p | 1080p
  fps: 30             # 30 | 60
  dpi: 140            # pixel density for AA rendering
```

**What changes:**
- Modified: `ServiceFactory.cpp` — add 800x480 `video_config` entry, make preferred resolution configurable
- Modified: `VideoDecoder.cpp` — ensure decoder handles resolution switches (phone may negotiate down)

---

### 4. GPS Sensor — Advertise with No-Fix Default

**Problem:** Every production HU sends GPS data through the sensor channel. Not advertising it may cause warnings or degraded behavior in newer AA versions.

**Design:** Advertise `LOCATION_DATA` sensor type. Send "no fix" by default. Optionally read from `gpsd` if a USB GPS dongle is present.

**Config:**
```yaml
sensors:
  gps:
    enabled: true           # advertise GPS capability
    source: none            # none | gpsd
    gpsd_host: localhost     # gpsd server address
    gpsd_port: 2947          # gpsd port
```

**Behavior:**
- `source: none` — Advertise capability but never send location updates. Phone uses its own GPS.
- `source: gpsd` — Connect to gpsd socket, forward TPV (Time-Position-Velocity) reports as `SensorEventIndication(LOCATION_DATA, {...})`.

**Sensor data fields (from proto):**
```
timestamp (int64), latitude (int32, 1e-7 degrees), longitude (int32, 1e-7 degrees),
accuracy (uint32, mm), altitude (int32, mm above WGS84), speed (int32, mm/s),
bearing (int32, 1e-6 degrees)
```

**What changes:**
- Modified: `ServiceFactory.cpp` — add LOCATION_DATA to sensor channel advertisement
- New: `GpsProvider.hpp/cpp` — optional gpsd client (only instantiated if `source: gpsd`)
- Modified: SensorService stub — forward GPS provider updates

---

### 5. Microphone Input — PipeWire Capture

**Problem:** AVInputService is a stub. It responds to the phone's mic open request but sends no audio. This blocks Google Assistant, voice commands, and voice search — one of the most-used AA features.

**Design:** Open a PipeWire capture stream when the phone requests microphone input. Forward PCM frames through the AVInput channel.

**Audio spec (from protocol reference):**
- Sample rate: 16kHz
- Channels: 1 (mono)
- Bit depth: 16-bit PCM
- Direction: HU → Phone

**Architecture:**
```
Phone sends AVInputOpenRequest
  → AVInputService opens PipeWire capture stream ("AA Microphone", 16kHz mono)
  → PipeWire callback delivers PCM buffers
  → AVInputService sends AVMediaIndication frames to phone
  → Phone processes speech (Google Assistant / voice search)

Phone sends AVInputOpenRequest with open=false (or session ends)
  → AVInputService closes PipeWire capture stream
```

**Integration point:** AudioService already manages PipeWire streams for output. Add a `openCaptureStream()` / `closeCaptureStream()` method that creates a `PW_DIRECTION_INPUT` stream.

**Config:**
```yaml
audio:
  microphone:
    device: auto          # PipeWire device name, or "auto" for default
    gain: 1.0             # capture gain multiplier
```

**What changes:**
- Modified: `AudioService.hpp/cpp` — add capture stream support (PW_DIRECTION_INPUT)
- Modified: `ServiceFactory.cpp` — wire AVInputService stub to actually capture and send audio
- Modified: AVInputService stub — implement `onAVInputOpenRequest()` to start/stop capture

---

### 6. ServiceDiscoveryResponse Identity

**Problem:** Prodigy sends default/empty identity fields. The phone displays these in AA settings and uses them for diagnostics. Production HUs fill them properly (Sony: "Sony" / "XAV-AX100", Kenwood: "JVCKENWOOD" / model).

**Design:** Fill identity fields from config, with sensible defaults.

**Config:**
```yaml
identity:
  head_unit_name: "OpenAuto Prodigy"
  manufacturer: "OpenAuto Project"
  model: "Raspberry Pi 4"
  sw_version: "0.3.0"               # auto-populated from build
  # User-configurable vehicle info (shows in AA settings)
  car_model: ""                      # e.g., "Mazda Miata"
  car_year: ""                       # e.g., "2000"
  left_hand_drive: true
```

**Fields set in ServiceDiscoveryResponse:**
```
head_unit_name = config.identity.head_unit_name
headunit_manufacturer = config.identity.manufacturer
headunit_model = config.identity.model
sw_version = config.identity.sw_version
sw_build = git commit hash (compile-time)
car_model = config.identity.car_model
car_year = config.identity.car_year
left_hand_drive_vehicle = config.identity.left_hand_drive
can_play_native_media_during_vr = true
```

**What changes:**
- Modified: `ServiceFactory.cpp` — populate identity fields from config
- Modified: `config.yaml` — add `identity` section
- Modified: `CMakeLists.txt` — inject git hash and version as compile-time defines

---

## Investigate After Pixel 6 Extraction

### 7. GenericNotification Service — HU-to-Phone Direction

**Goal:** Determine if the HU can push notifications *into* the AA UI (not just receive phone notifications). If yes, this replaces the need for a custom overlay control system — Prodigy could send native AA-styled notifications for:

- Bluetooth device connected/disconnected
- Incoming call with answer/decline actions
- Volume/brightness changes
- Plugin status messages
- Reverse camera active

**What we need from the Pixel 6:**
- The `GenericNotification` proto definition (message structure, field numbers)
- Whether the notification channel is bidirectional or phone-to-HU only
- Message IDs for this channel (not in aasdk)
- What notification actions are supported (dismiss, action buttons, etc.)

**If HU-to-phone works:** Build a `NotificationService` plugin that other plugins can call:
```cpp
notificationService->send("Bluetooth", "Galaxy S24 connected", NotificationPriority::Low);
notificationService->send("Phone", "Incoming: John", NotificationPriority::High, {"Answer", "Decline"});
```

**If phone-to-HU only:** Still useful — display phone notifications as native QML toasts when not in AA projection view. Lower priority but still a good feature.

### 8. NavigationNextTurn Service — Native Turn-by-Turn Widget

**Goal:** Display turn-by-turn directions on Prodigy's native UI (status bar or corner widget) even when AA video isn't in focus.

**What we need from the Pixel 6:**
- The `NavigationNextTurn` proto definition
- Channel ID and message IDs
- Data fields: turn icon, distance, street name, ETA
- Image format for turn icons (PNG? SVG? enum of standard icons?)

**If it works:** Build a `NavWidget` QML component that shows in TopBar.qml:
```
┌──────────────────────────────────────────────┐
│ 🕐 3:42 PM    ↱ 0.3 mi — Turn right on Main │
├──────────────────────────────────────────────┤
│                                              │
│          [BT Audio / Launcher / etc]         │
│                                              │
└──────────────────────────────────────────────┘
```

This pairs naturally with the notification investigation since both are "AA data rendered natively" features requiring the same proto extraction work.

---

## Not Doing

| Feature | Reason |
|---------|--------|
| MediaBrowser | AA's own media UI is adequate |
| RadioEndpoint | No radio hardware on Pi |
| InstrumentCluster | No secondary display / gauge cluster |
| VendorExtension | No vendor-specific services needed |
| Extended vehicle sensors (TPMS, HVAC, doors, fuel) | No CAN bus connection |
| Split-screen | 1024x600 too small to meaningfully split |

---

## Config Summary

All new config lives in `~/.openauto/config.yaml`:

```yaml
sensors:
  night_mode:
    source: time          # time | theme | gpio
    day_start: "07:00"
    night_start: "19:00"
    gpio_pin: 17
    gpio_active_high: true
  gps:
    enabled: true
    source: none          # none | gpsd
    gpsd_host: localhost
    gpsd_port: 2947

video:
  resolution: 720p        # 480p | 720p | 1080p
  fps: 30
  dpi: 140

audio:
  microphone:
    device: auto
    gain: 1.0

identity:
  head_unit_name: "OpenAuto Prodigy"
  manufacturer: "OpenAuto Project"
  model: "Raspberry Pi 4"
  sw_version: "0.3.0"
  car_model: ""
  car_year: ""
  left_hand_drive: true
```

---

## Implementation Order

Features are ordered by dependency and impact:

1. **ServiceDiscoveryResponse identity** — No dependencies, fills in metadata for all other changes
2. **Resolution advertisement** — No dependencies, two-line fix + config
3. **Night mode sensor** — Depends on NightModeProvider abstraction, but SensorService already exists
4. **Video focus state machine** — Independent, small change to VideoService
5. **GPS sensor** — Builds on sensor infrastructure from night mode
6. **Microphone input** — Depends on AudioService capture extension, largest piece of work

Items 7-8 (notification, nav turn) are investigation-only until Pixel 6 data is available.

---

## Success Criteria

- AA connects and displays night mode correctly based on configured source
- Video focus transitions are smooth (no reconnection on transient interrupts)
- Phone shows "OpenAuto Prodigy" in AA settings with correct manufacturer/model
- 480p fallback is advertised; phone negotiates 720p as expected
- "Hey Google" / voice commands work via connected USB microphone
- GPS sensor doesn't cause warnings (no-fix default is silent)
- All existing tests continue to pass
- Smoke test on Pi: connect AA wirelessly, verify all six features work
