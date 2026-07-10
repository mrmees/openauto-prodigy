# Android Auto Phone-Side Debug Findings

## Date: 2026-02-22

## Setup
- Phone: Samsung Galaxy (Android, targetSdkVersion=36)
- AA App: `com.google.android.projection.gearhead`
- Head Unit: OpenAuto Prodigy on Raspberry Pi 4
- Debug: Force debug logging ON, X-Ray application mode, via AA Developer Settings

## AA Developer Settings (Hidden Menu)
Access: Android Auto app → Settings → tap version 10 times → Developer settings

| Setting | Type | Description |
|---------|------|-------------|
| Wireless Android Auto | Toggle | Enable wireless projection |
| Application Mode | Selector | Release / Developer / Retail / **X-Ray** (diagnostic overlay) |
| Share screenshot now | Button | Capture projected AA screen |
| Save Video | Toggle | Capture H.264 video stream to disk |
| Dump screenshot | Toggle | Periodic screenshot capture |
| Save Audio | Toggle | Capture audio streams to disk |
| Save Microphone Input | Toggle | Capture mic input to disk |
| Audio Codec | Selector | Allow standard negotiation / Prefer PCM / Prefer AAC-LC / Prefer AAC-LC with ADTS headers |
| Clear all data | Button | Reset AA data |
| Force debug logging | Toggle | Verbose protocol logging to logcat |
| Collect GPS Data | Toggle | GPS sensor data logging |
| Disable ANR Monitoring | Toggle | Disable App Not Responding detection |
| Enable audio latency dump | Toggle | Audio pipeline latency profiling |

## AA Process Architecture

The AA app runs as 5 separate processes:

| Process | PID Suffix | Role |
|---------|-----------|------|
| `:projection` | 12010 | Main UI, Coolwalk rendering, status bar, hotseat rail |
| `:car` | 19618 | Transport (BT, WiFi, TCP), GAL protocol, audio channels |
| `:shared` | 21855 | Connection logging, shared services, setup coordination |
| `:watchdog` | 8910 | Health monitoring |
| `:provider` | 23271 | Content providers (projection state, settings, etc.) |

## Log Tag Reference

| Tag | Process | Purpose |
|-----|---------|---------|
| `GH.ConnLoggerV2` | :shared | Session event logger with timestamps and event IDs |
| `CAR.GAL.GAL.LITE` | :car | Google Automotive Link (GAL) — core protocol |
| `CAR.GAL.SECURITY.LITE` | :car | SSL/TLS handshake |
| `CAR.GAL.VIDEO.LITE` | :car | Video channel flow control |
| `CAR.GAL.AUDIO.LITE` | :car | Audio focus messages |
| `CAR.AUDIO.MEDIA` | :car | Media audio stream (ch 4) |
| `CAR.AUDIO.TTS` | :car | TTS/speech audio stream (ch 5) |
| `CAR.AUDIO.SYSTEM` | :car | System notification audio (ch 6) |
| `CAR.AUDIO.CHANNEL` | :car | Audio stream enable/disable/focus |
| `CAR.AUDIO.FOCUS` | :car | Audio focus arbitration with HU |
| `CAR.AUDIO.AFM` | :car | Android AudioFocusManager bridge |
| `CAR.AUDIO.Policy` | :car | Audio policy/routing |
| `CAR.BT.LITE` | :car | Bluetooth state machine |
| `CAR.BT.SVC.LITE` | :car | BT service (pairing, routing) |
| `CAR.SERVICE` | :car | Car connection lifecycle |
| `CAR.SERVICE.LITE` | :car | Car service state changes |
| `CAR.STARTUP` | :car | Startup/handoff sequencing |
| `CAR.CONMAN.LITE` | :car | Connection manager |
| `CAR.SETUP.WIFI` | :car | WiFi TCP socket creation |
| `GH.WIRELESS.SETUP` | :car | Wireless setup state machine |
| `GH.WPP.CONN` | :car | WiFi Projection Protocol connection |
| `GH.WPP.RFCOMM` | :car | RFCOMM transport |
| `GH.WPP.TCP` | :car | TCP transport |
| `GH.WPP.IO` | :car | WPP read/write IO |
| `GH.WPP.TRANSPORT` | :car | WPP transport lifecycle |
| `GH.WirelessNetRequest` | :car | WiFi network request/callback |
| `GH.WirelessShared` | :car | Shared wireless service lifecycle |
| `GH.WifiBluetoothRcvr` | :shared | BT connection state broadcast receiver |
| `GH.CurrentCarTracker` | :car | Current car BT tracking |
| `GH.BtConnectionTracker` | :car | BT profile connections |
| `GH.CarConnSession` | :car | Car connection session |
| `GH.CarConnSessMgr` | :car | Session management |
| `GH.CarClientManager` | :projection | Car client token lifecycle |
| `GH.CAR.ProxyEP.LITE` | :car | Per-channel proxy endpoints |
| `GH.MediaModel` | :projection | Media browser/controller |
| `GH.StartupMeasure` | :shared | Startup timing measurements |
| `GH.MsgNotifParser` | :projection | Notification parsing for messaging template |
| `GH.NavSugCallback` | :projection | Navigation suggestions |
| `GH.RailHotSeatView` | :projection | Coolwalk hotseat (app shortcuts) |
| `GH.RailStatusBarFrag` | :projection | Status bar (cell, battery) |
| `GH.ProjectionWindowCB` | :projection | Projection window focus |
| `CAR.PROJECTION.PRES` | :projection | Projection UI presenter |

## Wireless Connection Sequence (Full Timeline)

### Phase 1: BT Discovery (t+0s)
```
WIRELESS_SETUP_START
WIRELESS_SETUP_SHARED_A2DP_CONNECTING
WIRELESS_SDP_MANAGER_PREVIOULSY_KNOWN_UUID (port 2240)
WIRELESS_SETUP_SHARED_HFP_CONNECTING
WIRELESS_SETUP_SHARED_SERVICE_CREATED
WIRELESS_SETUP_BLUETOOTH_TRACKER_STARTED
WIRELESS_CDM_REQUESTED → WIRELESS_CDM_APPROVED
WIRELESS_SETUP_HU_TRACKER_READY
WIRELESS_SETUP_CDM_AND_HU_TRACKER_READY
```

### Phase 2: BT Connected + RFCOMM (t+3s)
```
ANDROID_AUTO_BLUETOOTH_CONNECTED
ANDROID_AUTO_WIRELESS_CAPABLE_BLUETOOTH_CONNECTED
GH.WPP.TCP: Initializing WPP TCP manager...
GH.WIRELESS.SETUP: Starting manager of type: RFCOMM
GH.WIRELESS.SETUP: State changed to CONNECTING_RFCOMM
GH.WPP.RFCOMM: Connecting the socket
WIRELESS_WIFI_CREDENTIALS_CACHE_NOT_FOUND
```

### Phase 3: RFCOMM Exchange (t+3.5s)
```
GH.WPP.RFCOMM: Creating the IO stream
GH.WPP.RFCOMM: IO stream will sleep for 10 ms instead of flushing after each write. SDK: 36
GH.WPP.RFCOMM: Creating the transport
GH.WIRELESS.SETUP: State changed to CONNECTED_RFCOMM
WIRELESS_WIFI_PROJECTION_PROTOCOL_RFCOMM_START_REQUEST_RECEIVED
GH.WIRELESS.SETUP: Send WifiStartResponse, protocol=RFCOMM, status=STATUS_SUCCESS
WIRELESS_WIFI_VERSION_NOT_REQUESTED
WIRELESS_WIFI_PROJECTION_REQUESTED_BY_HU_WITHOUT_REASON
WIRELESS_WIFI_PROJECTION_PROTOCOL_RFCOMM_INFO_REQUEST_SENT
WIRELESS_WIFI_PROJECTION_PROTOCOL_RFCOMM_INFO_RESPONSE_RECEIVED
WIRELESS_WIFI_MD_WPA3_SECURITY_SUPPORTED (phone supports WPA3!)
WIRELESS_WIFI_USING_WPA2_SECURITY (but we use WPA2)
WIRELESS_CONNECTING_WIFI_WITH_TIMEOUT (7000ms)
```

### Phase 4: WiFi Connect (t+5-11s)
```
WIRELESS_WIFI_STATE_ENABLED
ANDROID_AUTO_BLUETOOTH_A2DP_CONNECTED
ANDROID_AUTO_BLUETOOTH_HEADSET_CONNECTED
WIRELESS_WIFI_SCAN_ISSUED
WIRELESS_WIFI_SCAN_RESULTS_RECEIVED
WIRELESS_WIFI_SCAN_RESULTS_NETWORK_FOUND (signal: -43 dBm)
WIRELESS_WIFI_SCAN_RESULTS_TARGET_NETWORK_SUPPORTS_WPA2
WIRELESS_WIFI_MANUAL_NETWORK_TIMEOUT_TRIGGERED
WIRELESS_WIFI_RECONNECTION_ATTEMPT
WIRELESS_WIFI_ON_LINK_PROPERTIES_CHANGED_CALLBACK_INVOKED
GH.WIRELESS.SETUP: State changed to CONNECTED_WIFI
GH.WIRELESS.SETUP: Send WifiConnectStatus, status=STATUS_SUCCESS
GH.WIRELESS.SETUP: State changed to PROJECTION_INITIATED
```

### Phase 5: TCP + GAL Handshake (t+11.3s)
```
CAR.SETUP.WIFI: Creating socket, attempt 0
CAR.SETUP.WIFI: Socket connected to 10.0.0.1:5288

CAR.GAL.GAL.LITE: Car requests protocol version v1.1
CAR.GAL.GAL.LITE: Negotiated protocol version v1.7 (STATUS_SUCCESS)

CAR.GAL.SECURITY.LITE: Phone will wait for car to send more SSL handshake data
CAR.GAL.SECURITY.LITE: Peer certificate subject: OU=01,O=JVC Kenwood,L=Hachioji,ST=Tokyo,C=JP
CAR.GAL.SECURITY.LITE: Peer certificate serial: 27 (0x1b)
CAR.GAL.SECURITY.LITE: notBefore=Thu Jul 03 19:00:00 CDT 2014
CAR.GAL.SECURITY.LITE: notAfter=Sat Apr 29 09:28:38 CDT 2045
CAR.GAL.SECURITY.LITE: SSL context protocol: TLSv1.2, provider: GmsCore_OpenSSL

SDP_REQUEST_SENT → SDP_RESPONSE_RECEIVED (9 services)
```

### Phase 6: Channel Setup (t+11.6s)
```
Services registered:
  - SensorSourceService (id=2): sensors=[type=10, type=13, type=1]
  - BluetoothService (id=8): carAddress=DC:A6:32:E7:5A:FF, pairingMethods=[4]

Channels opened (in order): 0, 3, 4, 5, 6, 7, 1, 2, 8, 14
Per-channel queues created:
  - Channels 4, 5, 6: AudioPerChannelQueue
  - All others: PerChannelQueue

Audio config:
  - MEDIA: config 0, 48kHz, buffer 65536, frame ~42.67ms, MAX_UNACK=1
  - TTS: config 0, 16kHz, buffer 16384, frame ~64ms, MAX_UNACK=1
  - SYSTEM: config 0, MAX_UNACK=1
  - Note: "48Khz guidance is not supported in GAL 1.5 and below"

Video config:
  - MAX_UNACK=10
```

### Phase 7: Projection Start (t+12s)
```
CAR.SERVICE: DISCONNECTED → CONNECTING → PREFLIGHT → CONNECTED
Audio focus: GAIN requested and granted
PROJECTION_START → PROJECTION_STARTED_WIFI
PROJECTION_MODE_STARTED
115 car connection listeners notified
```

## Key Discoveries

### Protocol Version 1.7
Our HU advertises v1.1, phone negotiates to v1.7. This means the phone supports features beyond what we request. Unknown what v1.2-1.7 add, but "48Khz TTS guidance" is mentioned as requiring GAL 1.5+.

### CarInfoInternal (Phone's View of Our HU)
```
manufacturer=OpenAuto Project
model=Universal
headUnitProtocolVersion=1.1
modelYear=2026
vehicleId=e29d78b9cfad803b
headUnitMake=OpenAuto Project
headUnitModel=Raspberry Pi 4
headUnitSoftwareBuild=b6a5995
headUnitSoftwareVersion=0.3.0
canPlayNativeMediaDuringVr=false
hidePhoneSignal=false
hideBatteryLevel=false
wifiProjectionProtocolOnTcp=false
```

### WPA3 Support
Phone reports `WIRELESS_WIFI_MD_WPA3_SECURITY_SUPPORTED` but falls back to WPA2 because our hostapd is configured for WPA2. Could upgrade for better security.

### Audio System Architecture
Phone captures system audio via `AudioRecord` with an `AudioPolicy`, then streams it over the AA channels. This is the "system sound streaming" approach — it captures whatever Android is playing and sends it. Routing is via `device type 0`.

### BT Pairing Method 4
Consistently uses method 4. In the BluetoothPairingMethod enum, this likely corresponds to numeric comparison or a PIN-based method.

### WiFi Channel
Connected on 5180 MHz (5GHz channel 36). Signal strength -43 dBm (good).

### Startup Timing (Warm Reconnect)
- RFCOMM connect: 2.534s
- WiFi start request received: 3.114s
- Projection mode started: 11.67s (total BT→projected)

### Display Regions
```
CarRegionGroup{groupId=3} = [regionId=8, regionId=7]  (status bar area?)
CarRegionGroup{groupId=1} = [regionId=0, regionId=2131428408, regionId=2131428814, regionId=2131428812]  (content areas)
```

### Connection Event IDs
The ConnLoggerV2 uses sequential event IDs per session. During our capture, events 1735-1902 covered a full disconnect→reconnect cycle. Event types include:
- `WIRELESS_SETUP_*` — BT/WiFi setup phases
- `WIRELESS_WIFI_*` — WiFi connection phases
- `PROJECTION_*` — Projection lifecycle
- `CAR_CONNECTION_*` — CDM (Companion Device Manager) auth
- `VERSION_NEGOTIATION_*` — Protocol version
- `SSL_*` — TLS handshake
- `SDP_*` — Service discovery
- `FRAMER_*` — Wire framing
- `AUTHORIZATION_*` — User authorization

## Available Content Providers (Not Directly Queryable via ADB)
- `SharedPreferencesProvider` — AA config/preferences
- `ProjectionStateProvider` — Current projection state
- `DeveloperSettingsFileProvider` — Dev settings storage
- `BugreportFileProvider` — Bug report data
- `CoolwalkColorsProvider` — Theme colors
- `GhMicrophoneContentProvider` — Microphone access
- `TroubleshooterContentProvider` — Troubleshooting data

## Interesting Services
- `CarControlCarAppService` — Car control demo
- `ClustersimSettingsService` — Instrument cluster simulator
- `ClusterTurnCardCarActivityService` — Cluster turn cards
- `SecondaryScreenTurnCardCarActivityService` — Secondary display support
- `ComposeCatalogCarActivityService` — Coolwalk UI component catalog
- `GearSnacksService` — Snackbar notification system
- `MediaSearchTemplateService` — Media browsing template
- `MessagingRemoteScreenService` — Messaging template

## Broadcast Actions
- `ACTION_LOG_WIRELESS_EVENT` — Wireless event logging
- `ACTION_LOG_WIRELESS_STATE` — Wireless state logging

---

## Pi-Side Correlation (HU Perspective)

### ServiceDiscoveryResponse (HU → Phone)
```
channels:
  ch3: VIDEO — 480p @ 30fps, margin_width=0, margin_height=70, dpi=140
  ch4: AUDIO MEDIA — 48kHz stereo 16-bit
  ch5: AUDIO SPEECH — 16kHz mono 16-bit
  ch6: AUDIO SYSTEM — 16kHz mono 16-bit
  ch1: INPUT — touch_screen 800x410, keycodes: [3, 4, 84]
  ch2: SENSOR — NIGHT_DATA, DRIVING_STATUS, LOCATION
  ch8: BLUETOOTH — DC:A6:32:E7:5A:FF, pairing: HFP
  ch14: WIFI — ssid: "OpenAutoProdigy"
  ch7: AV_INPUT (microphone)

head_unit_name: "OpenAuto Prodigy"
car_model: "Universal"
manufacturer: "OpenAuto Project"
```

### Phone's ServiceDiscoveryRequest
```
device_name: "Android"
device_brand: "samsung SM-S938U"
3 embedded PNG icons: 32x32, 64x64, 128x128 (green color)
```

### Pi-Side Connection Sequence
```
[BTDiscovery] Phone connected via BT: MATTHEW's S25 Ultra
[BTDiscovery] Sending WifiStartRequest: ip=10.0.0.1 port=5288
[BTDiscovery] Sending WifiInfoResponse: ssid=OpenAutoProdigy bssid=DC:A6:32:E7:5A:FE
[BTDiscovery] WifiConnectionStatus: STATUS_SUCCESS
[AAService] Wireless AA connection from 10.0.0.26:47550
[AAService] TCP keepalive: idle=5s, interval=3s, count=3
[AndroidAutoEntity] Version request sent (v1.1)
[AndroidAutoEntity] Version response: 1.7 status=0
[AndroidAutoEntity] SSL handshake complete (2348 bytes from phone, 51 bytes back)
[ServiceFactory] Created 9 services
Channel opens: Video, Audio:4, Audio:5, Audio:6, Input, Sensor, Bluetooth, WiFi, AVInput
```

### BT Pairing Behavior
- Phone sends BT pairing request every 2 minutes during active AA session
- Uses pairing method 4 consistently
- BT pairing requests: `BluetoothPairingRequest { address: <phone MAC>, pairing_method: 4 }`

---

## Phone-Side Detailed Analysis

### Display Rendering Parameters
The phone selected our 480p config and applied the margin:
```
DisplayParams {
  selectedIndex: 0 (480p)
  codecWidth: 800, codecHeight: 480
  fps: 30
  dispWidth: 800, dispHeight: 410
  dispLeft: 0, dispTop: 35
  dpi: 140
  pixelAspectRatio: 1.0
  decoderAdditionalDepth: 4
  stableInsets: Rect(0, 0 - 0, 0)
  contentInsets: Rect(0, 0 - 0, 0)
}
```
The `dispTop=35` confirms the phone renders AA content in a centered 800x410 sub-region with 35px black bars top and bottom (70px margin_height / 2).

### CarModuleFeatures (Phone Feature Detection)
The phone queries the HU for supported features. We're currently exposing this full set:
```
WINDOW_OUTSIDE_TOUCHES        CAR_WINDOW_REQUEST_FOCUS
START_DUPLEX_CONNECTION        HERO_CAR_CONTROLS
AUDIO_STREAM_DIAGNOSTICS       HERO_THEMING
LIFECYCLE_BEFORE_LIFETIME      DRIVER_POSITION_SETTING
MULTI_REGION                   POWER_SAVING_CONFIGURATION
MICROPHONE_DIAGNOSTICS         NATIVE_APPS
THIRD_PARTY_ACCESSIBLE_SETTINGS  CLEAR_DATA
CONNECTION_STATE_HISTORY       CAR_WINDOW_RESIZABLE
MANIFEST_QUERYING              CLIENT_SIDE_FLAGS
MULTI_DISPLAY                  START_CAR_ACTIVITY_WITH_OPTIONS
COOLWALK                       INT_SETTINGS_AVAILABLE
INITIAL_FOCUS_SETTINGS         STICKY_WINDOW_FOCUS
INDEPENDENT_NIGHT_MODE         PREFLIGHT
HERO_CAR_LOCAL_MEDIA           CONTENT_WINDOW_INSETS
GH_DRIVEN_RESIZING             ENHANCED_NAVIGATION_METADATA
ASSISTANT_Z
```

**Notable features:**
- `COOLWALK` — Coolwalk UI is active (the newer split-screen interface)
- `MULTI_DISPLAY` — protocol supports multiple displays
- `HERO_THEMING` — phone-to-HU color theming (we don't implement)
- `INDEPENDENT_NIGHT_MODE` — night mode from HU sensor, not phone
- `ENHANCED_NAVIGATION_METADATA` — rich navigation data available
- `GH_DRIVEN_RESIZING` — phone-driven display resizing
- `NATIVE_APPS` — HU can run native Android Auto apps (Car App Library)
- `ASSISTANT_Z` — latest Google Assistant integration

### CarDisplayUiFeatures
```
CarDisplayUiFeatures {
  resizeType: 0
  hasClock: false
  hasBatteryLevel: false
  hasPhoneSignal: false
  hasNativeUiAffordance: false
  hasClusterTurnCard: false
}
```
All `false` because we don't advertise these in ServiceDiscovery. Setting these would let AA render native status bar elements.

### Theming Protocol
```
CarThemeVersionLD: Updating theming version to: 0
ThemingManager: Not updating theme, palette version doesn't match.
  Current version: 2, HU version: 0
SysChromeController: Setting background color for palette
  VersionedPalette(palette=RuntimePalette(baseColor=-10914876,
  themeVersion=DEFAULT), themeVersion=DEFAULT)
```
The phone has a palette version 2 theming system. Our HU reports version 0, so the phone falls back to DEFAULT palette. If we advertised version 2, the phone would send us a Material You color palette derived from the user's wallpaper.

### Sensor Gaps
Phone requested sensors we don't advertise:
| Sensor ID | Type | Notes |
|-----------|------|-------|
| 2 | COMPASS_DATA | Heading for nav |
| 6 | GYROSCOPE_DATA | Vehicle dynamics |
| 7 | ACCELEROMETER_DATA | Vehicle dynamics |

These requests are non-fatal — phone logs them as "not supported" and continues with its own sensors.

### Audio Configuration Details
```
MEDIA:  config 0, 48kHz stereo,  buffer=65536, frame=42.67ms, MAX_UNACK=1
TTS:    config 0, 16kHz mono,    buffer=16384, frame=64ms,    MAX_UNACK=1
SYSTEM: config 0,                MAX_UNACK=1

"48Khz guidance is not supported in GAL 1.5 and below"
"Unsupported stream type: 1" (VOICE — phone tried to configure voice stream)
```
The VOICE stream type (1) warning suggests the phone expected a separate voice audio channel that we haven't advertised.

### Video Configuration
```
videoCodecType: 3 (H.264/AVC)
Video encoder: c2.qti.avc.encoder (Qualcomm hardware encoder)
Video flow control: MAX_UNACK=10
Watermark: showWatermark=false (cert serial 0x1b recognized)
```

### Google Assistant Integration
```
Assistant API Version: 5 (MIN_VERSION=0, MAX_VERSION=5)
Feature flags: [8,1, 16,0, 24,1, 40,1, 48,1, 56,0, 64,1, 80,1, 88,1, 96,0, 104,0, 112,0, 120,1]
Gemini: useAsr=true, UseGeminiGhKs=true, isGeminiEnabled=false

App whitelist for Assistant actions:
  - io.homeassistant.companion.android
  - com.google.android.apps.messaging
  - com.google.android.apps.maps
  - au.com.shiftyjelly.pocketcasts
  - com.google.android.apps.youtube.music
  - com.samsung.android.dialer
  - (and others: Target, Teams, Signal, Facebook Messenger, etc.)
```

---

## Protocol Version Analysis (v1.1 vs v1.7)

### What We Know

Our HU sends version 1.1 (hardcoded in aasdk). The phone negotiates up to 1.7 with STATUS_SUCCESS. The phone is backwards-compatible and works fine with 1.1, but version-gated features are disabled.

### Version-Gated Features (Confirmed)

| Feature | Min Version | Evidence |
|---------|------------|---------|
| 48kHz TTS/guidance audio | GAL 1.6+ | Phone log: "48Khz guidance is not supported in GAL 1.5 and below" |

### Version-Gated Features (Suspected from Kenwood Analysis)

The Kenwood firmware ships two AA library versions — base and `_2_2_1` (HUIG v2.2.1). The newer version adds:
- `reportRpmData()` — engine RPM sensor support
- `reportDrivingStatusData()` — driving status reporting
- Separate `AAPConnectionManager` class (improved connection management)

This HUIG (Head Unit Integration Guide) versioning may correlate with GAL protocol versions.

### Cross-Vendor SDK Evolution

| Feature | Alpine (tamul 1.4.1) | Kenwood (libreceiver-lib) | Sony (GAL Protocol) |
|---------|---------------------|--------------------------|---------------------|
| Radio service | No | Yes | Yes |
| Notifications | No | Yes | Yes |
| Instrument cluster | No | Yes | Yes |
| Voice session | No | Yes | Yes |
| Media browser | Basic | Full tree | Full tree |
| Vehicle sensors | Basic | Comprehensive | Comprehensive |
| Split-screen | No | Yes (400x358) | No |
| GAL verification | Basic | Extended | Basic |

### Actionable Findings

1. **Bump version to 1.7**: Modify aasdk `Version.hpp` from `AASDK_MINOR = 1` to `AASDK_MINOR = 7`. The phone already negotiates 1.7 — we just need to request it. This should unlock 48kHz TTS and potentially other features. Low risk since the phone already supports fallback.

2. **Advertise palette version 2**: Would enable Material You theming from the phone — AA colors adapt to user's wallpaper. Needs investigation into what the HU must implement.

3. **Add VOICE audio channel**: The "Unsupported stream type: 1" warning suggests we should advertise a separate voice audio stream (for Google Assistant output). Currently only MEDIA, SPEECH (TTS/nav), and SYSTEM channels exist.

4. **Report CarDisplayUiFeatures**: Setting `hasClock=true`, `hasBatteryLevel=true`, `hasPhoneSignal=true` would let AA use our native status bar for phone state display, reducing AA's own chrome overhead.

5. **Upgrade hostapd to WPA3**: Phone supports WPA3 (`WIRELESS_WIFI_MD_WPA3_SECURITY_SUPPORTED`). Easy security upgrade.

---

## How to Reproduce This Debug Session

### Prerequisites
- ADB connected to phone (`adb devices` shows device)
- AA Developer Settings enabled (tap version 10x in AA settings)
- Force debug logging: ON
- Application Mode: X-Ray (optional, enables diagnostic overlay)

### Capture Process
```bash
# 1. Clear logcat buffer
adb logcat -c

# 2. Disconnect/reconnect AA (toggle BT or restart Prodigy on Pi)

# 3. Wait for projection to start (watch Pi log: "Video focus gained")

# 4. Dump buffer (don't stream — pipe fills and clips)
adb logcat -d > /tmp/aa_full_dump.txt

# 5. Filter to AA-relevant tags
grep -E 'CAR\.|GH\.|WIRELESS|PROJECTION|WPP' /tmp/aa_full_dump.txt > /tmp/aa_filtered.txt
```

### Key Log Tags to Watch
```
CAR.GAL.GAL.LITE        — Protocol negotiation, channel opens
CAR.GAL.SECURITY.LITE   — SSL handshake, certificate details
CAR.AUDIO.*             — Audio config, stream setup, focus
CAR.VIDEO               — Codec selection, encoder details
CAR.SENSOR              — Sensor requests, unsupported sensors
CAR.SERVICE.LITE        — Connection lifecycle
CAR.WM.*                — Display params, rendering config
GH.ConnLoggerV2         — Session events with timestamps
GH.WIRELESS.SETUP       — BT/WiFi connection phases
GH.Assistant.Controller — Assistant version, feature flags
GH.ThemingManager       — Theme/palette negotiation
CAR.CLIENT              — CarModuleFeatures cache
```
