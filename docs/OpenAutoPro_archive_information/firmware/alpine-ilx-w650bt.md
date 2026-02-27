# Alpine iLX-W650BT Firmware Analysis

> **Source:** Alpine iLX-W650BT firmware v1.001_0802 (`Alpine_SYSTEM.FIR`)
> **Date:** August 2019
> **AAP SDK:** Google "tamul" AAP SDK v1.4.1 (proprietary OEM receiver SDK)
> **SSL:** MatrixSSL 3.9.5 (open edition)
> **Protobuf:** Google protobuf (lite runtime)
> **Platform:** Bare-metal ARM RTOS (Alpine "MISO" framework, not Linux)
> **Host SDK:** TMM9200 SDK v2.76 (Toshiba display audio SoC)

This document contains protocol constants, configuration structures, and architectural details extracted from string analysis of the Alpine iLX-W650BT head unit firmware. This is a real-world implementation of Google's official AAP (Android Auto Protocol) receiver SDK, providing ground truth for our open-source reimplementation.

**No proprietary code was decompiled or disassembled.** All information was extracted via `strings` analysis of publicly available firmware update files.

---

## AAP SDK Architecture

### Source File Layout (from debug paths)

The Google AAP receiver SDK ("tamul") has this structure:

```
tamul_aap_1.4.1/
├── external/
│   └── protobuf/src/
│       ├── gen/protos.pb.{h,cc}          # Generated protobuf code
│       └── google/protobuf/              # Protobuf lite runtime
├── tamul_aap/
│   └── matrixssl-3-9-5-open/            # SSL/TLS (not OpenSSL!)
│       ├── core/
│       ├── crypto/{digest,keyformat,layer,pubkey}/
│       └── matrixssl/
└── vendor/auto/projection_protocol/receiver-lib/src/
    ├── AudioSource.cpp                   # Microphone input → phone
    ├── BluetoothEndpoint.cpp             # BT pairing management
    ├── Controller.cpp                    # Session lifecycle controller
    ├── InputSource.cpp                   # Touch/key/rotary input → phone
    ├── MediaBrowserEndpoint.cpp          # Media browsing (metadata lists)
    ├── MediaPlaybackStatusEndpoint.cpp   # Now-playing metadata
    ├── MediaSinkBase.cpp                 # Audio output from phone
    ├── MessageRouter.cpp                 # Channel/message routing
    ├── NavigationStatusEndpoint.cpp      # Nav turn-by-turn data
    ├── PhoneStatusEndpoint.cpp           # Phone call status
    ├── SensorSource.cpp                  # Vehicle sensor data → phone
    ├── SensorSource.h
    └── WifiProjectionEndpoint.cpp        # Wireless AA connection
```

### Key Observations

1. **SSL is MatrixSSL, not OpenSSL** — The SDK uses MatrixSSL 3.9.5 for the TLS handshake. aasdk uses OpenSSL, which is compatible but a different implementation.

2. **Protobuf-lite runtime** — Uses the lite protobuf runtime, same as aasdk. All message definitions are in a single generated file (`protos.pb.{h,cc}`).

3. **USB-primary, WiFi-secondary** — The SDK has both USB (`USBD_DEVICE`) and WiFi projection. USB connection/disconnection is handled via `aap_notify_device_connection`/`aap_notify_device_disconnection`.

4. **XML-driven configuration** — The SDK reads head unit capabilities from an XML config file, not compiled-in constants. Error strings reference `AAP_INVALID_XML_FILE` and `AAP_XML_ERROR_INVALID_TAG`.

---

## Service Discovery Configuration

The SDK reads head unit capabilities from XML config using these attribute paths. This maps directly to what we send in the `ServiceDiscoveryResponse` protobuf.

### Head Unit Identity

| Attribute Path | Description |
|---------------|-------------|
| `/aap_attributes/head_unit/make` | Manufacturer (e.g., "Alpine") |
| `/aap_attributes/head_unit/model` | Model (e.g., "iLX-W650BT") |
| `/aap_attributes/head_unit/client_cert_file` | Client SSL certificate |
| `/aap_attributes/head_unit/private_key_file` | Private key for SSL |
| `/aap_attributes/head_unit/root_cert_file` | Root CA certificate |

### Display

| Attribute Path | Description |
|---------------|-------------|
| `/aap_attributes/head_unit/display/width` | Display width in pixels |
| `/aap_attributes/head_unit/display/height` | Display height in pixels |
| `/aap_attributes/head_unit/display/par` | Pixel aspect ratio |

### Input Devices

#### Touch Screen
| Attribute Path | Description |
|---------------|-------------|
| `/aap_attributes/head_unit/input_devices/touch_screen/type` | `capacitive` or `resistive` |
| `/aap_attributes/head_unit/input_devices/touch_screen/path` | Device path |
| `/aap_attributes/head_unit/input_devices/touch_screen/use_internally` | Whether HU uses touch events internally |

#### Touch Pad (for rotary-style input)
| Attribute Path | Description |
|---------------|-------------|
| `/aap_attributes/head_unit/input_devices/touch_pad/width` | Touch pad logical width |
| `/aap_attributes/head_unit/input_devices/touch_pad/height` | Touch pad logical height |
| `/aap_attributes/head_unit/input_devices/touch_pad/physical_width` | Physical width in mm |
| `/aap_attributes/head_unit/input_devices/touch_pad/physical_height` | Physical height in mm |

#### Key Codes
| Attribute Path | Description |
|---------------|-------------|
| `/aap_attributes/head_unit/input_devices/key_codes/value` | Supported key codes (see full list below) |

### Video

| Attribute Path | Description |
|---------------|-------------|
| `/aap_attributes/application/video/fps` | Frame rate |
| `/aap_attributes/application/video/resolution/value` | Resolution enum (480p, 720p, 1080p) |
| `/aap_attributes/application/video/resolution/density` | Display density (DPI) |
| `/aap_attributes/application/video/resolution/real_density` | Real display density |
| `/aap_attributes/application/video/codec_profile/value` | H.264 profile |
| `/aap_attributes/application/video/codec_profile/decoder_additional_depth` | Decoder buffer depth |

**Notes from firmware strings:**
- `Mandatory 800x480 resolution was missing in the XML. It is added by the stack` — **480p (800x480) is always required** and auto-added if missing
- Maximum resolution count is capped (firmware logs "Maximum video resolutions limit reached")
- Resolution enum values: `480p`, `720p`, `1080p`

### Audio Streams

| Attribute Path | Description |
|---------------|-------------|
| `/aap_attributes/application/audio/stream_media/type` | Media stream type |
| `/aap_attributes/application/audio/stream_media/device_id` | Audio device for media |
| `/aap_attributes/application/audio/stream_media/bps` | Bits per second |
| `/aap_attributes/application/audio/stream_guidance/type` | Navigation audio type |
| `/aap_attributes/application/audio/stream_guidance/device_id` | Audio device for nav |
| `/aap_attributes/application/audio/stream_guidance/bps` | Bits per second |
| `/aap_attributes/application/audio/stream_microphone/type` | Microphone input type |
| `/aap_attributes/application/audio/stream_microphone/device_id` | Microphone device |
| `/aap_attributes/application/audio/stream_microphone/bps` | Bits per second |

### Subscription Services

Services the head unit can subscribe to for status updates from the phone:

| Attribute Path | Description |
|---------------|-------------|
| `/aap_attributes/application/subscription_list/navigation/subscribe` | Enable nav status |
| `/aap_attributes/application/subscription_list/navigation/cluster_type` | Instrument cluster type |
| `/aap_attributes/application/subscription_list/navigation/icon_width` | Nav icon width |
| `/aap_attributes/application/subscription_list/navigation/icon_height` | Nav icon height |
| `/aap_attributes/application/subscription_list/navigation/icon_color_depth` | Icon color depth |
| `/aap_attributes/application/subscription_list/navigation/min_interval` | Min update interval |
| `/aap_attributes/application/subscription_list/media_playback/subscribe` | Enable now-playing |
| `/aap_attributes/application/subscription_list/media_browsing/subscribe` | Enable media browse |
| `/aap_attributes/application/subscription_list/notification/subscribe` | Enable notifications |
| `/aap_attributes/application/subscription_list/phone/subscribe` | Enable phone status |
| `/aap_attributes/application/subscription_list/radio/subscribe` | Enable radio control |
| `/aap_attributes/application/subscription_list/radio/radio_config/type` | Radio type |
| `/aap_attributes/application/subscription_list/radio/radio_config/start_freq` | Start frequency |
| `/aap_attributes/application/subscription_list/radio/radio_config/end_freq` | End frequency |
| `/aap_attributes/application/subscription_list/radio/radio_config/channel_gap` | Channel spacing |
| `/aap_attributes/application/subscription_list/vec/service/service_name` | Vehicle extension service name |
| `/aap_attributes/application/subscription_list/vec/service/app` | Vehicle extension app |

---

## Protocol Messages

### Core Protocol Messages

| Message | Direction | Description |
|---------|-----------|-------------|
| `ServiceDiscoveryRequest` | Phone → HU | Phone requests HU capabilities |
| `ServiceDiscoveryResponse` | HU → Phone | HU sends capability manifest |
| `ChannelOpenRequest` | Phone → HU | Phone requests channel open |
| `ChannelOpenResponse` | HU → Phone | HU confirms channel open |
| `ByeByeRequest` | Either | Clean disconnect request |
| `ByeByeResponse` | Either | Disconnect acknowledgment |

### Input Messages (HU → Phone)

| Message | Description |
|---------|-------------|
| `InputReport` | Touch/key/button events |
| `TouchEvent.Pointer` | Touch point data within InputReport |
| `KeyEvent.Key` | Key press/release data |
| `AbsoluteEvent.Abs` | Absolute axis (e.g., rotary encoder) |
| `RelativeEvent.Rel` | Relative axis (e.g., scroll) |

### Sensor Messages (HU → Phone)

| Message | Description |
|---------|-------------|
| `SensorRequest` | Phone requests sensor data |
| `SensorResponse` | HU sends sensor config |
| `SensorBatch` | Batch of sensor readings |
| `SensorError` | Sensor error indication |

**Note:** `Mandatory AAP_SENSOR_DRIVING_STATUS_DATA was missing in the XML. It is added by the stack` — **Driving status sensor is always required.**

### Audio Focus

| Message | Direction | Description |
|---------|-----------|-------------|
| `AudioFocusRequestNotification` | Phone → HU | Phone requests audio focus |
| `AudioFocusNotification` | HU → Phone | HU grants/denies focus |

### Video Focus

| Message | Direction | Description |
|---------|-----------|-------------|
| `VideoFocusRequestNotification` | Phone → HU | Phone requests video focus |
| `VideoFocusNotification` | HU → Phone | HU grants/denies focus |

### Status Services (Phone → HU)

| Message | Description |
|---------|-------------|
| `NavigationStatus` | Current nav state |
| `NavigationStatusService.ImageOptions` | Nav icon rendering options |
| `MediaPlaybackMetadata` | Now-playing info |
| `MediaPlaybackStatus` | Playback state |
| `MediaBrowserInput` | Browse media library |
| `PhoneStatus` | Phone call state |
| `PhoneStatus.Call` | Individual call info |
| `PhoneStatusInput` | Phone control commands |
| `WifiProjectionService` | Wireless connection setup |

---

## Protobuf Enum Types

Complete list of protobuf enums found in the firmware. These map directly to the `.proto` definitions.

### Audio

```
AudioStreamType         — media, guidance, voice, microphone, system
AudioFocusRequestType   — gain, gain_transient, gain_transient_may_duck, loss
AudioFocusStateType     — gain, gain_media_only, gain_transient, gain_transient_guidance_only,
                          loss, loss_transient, loss_transient_can_duck
```

### Video

```
VideoCodecResolutionType — 480p (800x480), 720p (1280x720), 1080p (1920x1080)
VideoFrameRateType       — frame rate enum
VideoFocusMode           — mode enum
VideoFocusReason         — reason enum
MediaCodecType           — codec type (H.264)
```

### Input

```
TouchScreenType  — capacitive, resistive
PointerAction    — down(0), up(1), move(2), pointer_down(5), pointer_up(6)
```

### Navigation

```
NavigationStatus_NavigationStatusEnum             — native, projection
NavigationStatusService_InstrumentClusterType     — cluster type
NavigationNextTurnEvent_NextTurnEnum              — turn type
NavigationNextTurnEvent_TurnSide                  — left/right
NavigationNextTurnDistanceEvent_DistanceUnits     — distance unit
NavFocusType                                      — nav focus state
```

### Phone

```
PhoneStatus_State — phone call state
```

### Media

```
MediaPlaybackStatus_State — playing, paused, stopped, etc.
MediaList_Type            — media list type
```

### Vehicle

```
SensorType            — sensor types (driving_status always required)
SensorErrorType       — sensor error codes
DriverPosition        — left/right hand drive
FuelType              — fuel type
EvConnectorType       — EV connector type
Gear                  — gear position
HeadLightState        — headlight state
TurnIndicatorState    — turn signal state
```

### Session

```
ByeByeReason          — disconnect reason codes
MessageStatus         — message delivery status
WifiSecurityMode      — WiFi security (WPA2, etc.)
VoiceSessionStatus    — voice assistant state
UserSwitchStatus      — user switch state
Config_Status         — configuration status
FeedbackEvent         — feedback event types
```

---

## Audio Focus State Machine

The AAP SDK constants reveal the complete audio focus model (mirrors Android's AudioFocus API):

```
Stream Types:
  AAP_AUDIO_STREAM_MEDIA       — Music, podcasts
  AAP_AUDIO_STREAM_GUIDANCE    — Navigation prompts
  AAP_AUDIO_STREAM_VOICE       — Voice assistant
  AAP_AUDIO_STREAM_MICROPHONE  — Mic input to phone
  AAP_AUDIO_STREAM_SYSTEM      — System sounds

Focus Request Types:
  AAP_AUDIO_STREAM_FOCUS_REQ_GAIN                     — Permanent focus
  AAP_AUDIO_STREAM_FOCUS_REQ_GAIN_TRANSIENT           — Temporary focus
  AAP_AUDIO_STREAM_FOCUS_REQ_GAIN_TRANSIENT_MAY_DUCK  — Temporary, others can duck
  AAP_AUDIO_STREAM_FOCUS_REQ_LOSS                     — Release focus

Focus States (response from HU):
  AAP_AUDIO_STATE_GAIN                          — Full focus granted
  AAP_AUDIO_STATE_GAIN_MEDIA_ONLY               — Media focus only
  AAP_AUDIO_STATE_GAIN_TRANSIENT                — Temporary focus
  AAP_AUDIO_STATE_GAIN_TRANSIENT_GUIDANCE_ONLY  — Nav guidance only
  AAP_AUDIO_STATE_LOSS                          — Focus lost permanently
  AAP_AUDIO_STATE_LOSS_TRANSIENT                — Focus lost temporarily
  AAP_AUDIO_STATE_LOSS_TRANSIENT_CAN_DUCK       — Can continue at lower volume
```

---

## Video Focus State Machine

```
Video States:
  AAP_VIDEO_STATE_NATIVE             — HU native UI shown
  AAP_VIDEO_STATE_NATIVE_TRANSIENT   — Temporarily showing HU UI
  AAP_VIDEO_STATE_PROJECTION         — AA video projection active

Video Focus Requests:
  AAP_VIDEO_STREAM_FOCUS_REQ_GAIN              — Phone wants video
  AAP_VIDEO_STREAM_FOCUS_REQ_LOSS              — Phone releasing video
  AAP_VIDEO_STREAM_FOCUS_REQ_LOSS_TRANSIENT    — Temporarily releasing video
```

**Insight for Prodigy:** This explains the video focus handshake. When the HU needs to show its own UI (e.g., reverse camera, settings), it sends `LOSS_TRANSIENT`. When done, it sends `GAIN` back. We currently don't implement video focus at all — we just grab/release fullscreen.

---

## Navigation State Machine

```
Navigation States:
  AAP_NAVIGATION_STATE_NATIVE      — HU's built-in nav active
  AAP_NAVIGATION_STATE_PROJECTION  — Phone's nav projected

Navigation Focus:
  AAP_NAVIGATION_STREAM_FOCUS_REQ_GAIN  — Phone wants nav stream
  AAP_NAVIGATION_STREAM_FOCUS_REQ_LOSS  — Phone releasing nav
```

---

## Session Lifecycle

```
Session States:
  AAP_SESSION_STATE_DISCONNECTED  — No connection
  AAP_SESSION_STATE_CONNECTING    — Handshake in progress
  AAP_SESSION_STATE_INVALID       — Error state

Connection Events:
  aap_notify_device_connection    — USB device attached
  aap_notify_device_disconnection — USB device detached

Services Start:
  app_disc_start_services         — After service discovery, start all channels
```

---

## Bluetooth Pairing

```
Pairing Methods:
  AAP_BT_PAIRING_NUMERIC  — Numeric comparison (most common)
  AAP_BT_PAIRING_PASSKEY  — Passkey entry
  AAP_BT_PAIRING_PIN      — Legacy PIN

Related Functions:
  miso_fw_connectivity_AndroidAuto_BT_PairConfirm  — Confirm BT pairing
  miso_fw_base_connectivity_aa_bt_pairing_request_cb — Pairing request callback
```

---

## AAP Keycodes (Complete List)

The full keycode table from the SDK. These map to Android KeyEvent constants and are sent in `InputReport` messages.

<details>
<summary>Click to expand full keycode list (250+ entries)</summary>

### Navigation & Media
- `AAP_KEYCODE_HOME` — Home button
- `AAP_KEYCODE_BACK` — Back button
- `AAP_KEYCODE_MENU` — Menu button
- `AAP_KEYCODE_SEARCH` — Search
- `AAP_KEYCODE_NAVIGATION` — Switch to nav
- `AAP_KEYCODE_MEDIA` — Switch to media
- `AAP_KEYCODE_MUSIC` — Music app
- `AAP_KEYCODE_TEL` — Phone app
- `AAP_KEYCODE_CONTACTS` — Contacts
- `AAP_KEYCODE_CALL` — Answer call
- `AAP_KEYCODE_ENDCALL` — End call
- `AAP_KEYCODE_VOICE_ASSIST` — Voice assistant (Google Assistant)
- `AAP_KEYCODE_ASSIST` — Generic assist
- `AAP_KEYCODE_SETTINGS` — Settings
- `AAP_KEYCODE_NOTIFICATION` — Notifications
- `AAP_KEYCODE_APP_SWITCH` — Recent apps

### Media Playback
- `AAP_KEYCODE_MEDIA_PLAY`
- `AAP_KEYCODE_MEDIA_PAUSE`
- `AAP_KEYCODE_MEDIA_PLAY_PAUSE`
- `AAP_KEYCODE_MEDIA_STOP`
- `AAP_KEYCODE_MEDIA_NEXT`
- `AAP_KEYCODE_MEDIA_PREVIOUS`
- `AAP_KEYCODE_MEDIA_FAST_FORWARD`
- `AAP_KEYCODE_MEDIA_REWIND`
- `AAP_KEYCODE_MEDIA_RECORD`
- `AAP_KEYCODE_MEDIA_CLOSE`
- `AAP_KEYCODE_MEDIA_EJECT`
- `AAP_KEYCODE_MEDIA_AUDIO_TRACK`
- `AAP_KEYCODE_MEDIA_TOP_MENU`

### Volume
- `AAP_KEYCODE_VOLUME_UP`
- `AAP_KEYCODE_VOLUME_DOWN`
- `AAP_KEYCODE_VOLUME_MUTE`
- `AAP_KEYCODE_MUTE`

### D-Pad / Rotary
- `AAP_KEYCODE_DPAD_UP`
- `AAP_KEYCODE_DPAD_DOWN`
- `AAP_KEYCODE_DPAD_LEFT`
- `AAP_KEYCODE_DPAD_RIGHT`
- `AAP_KEYCODE_DPAD_CENTER`
- `AAP_KEYCODE_DPAD_UP_LEFT`
- `AAP_KEYCODE_DPAD_UP_RIGHT`
- `AAP_KEYCODE_DPAD_DOWN_LEFT`
- `AAP_KEYCODE_DPAD_DOWN_RIGHT`
- `AAP_KEYCODE_ROTARY_CONTROLLER`

### Navigation
- `AAP_KEYCODE_NAVIGATE_IN`
- `AAP_KEYCODE_NAVIGATE_OUT`
- `AAP_KEYCODE_NAVIGATE_NEXT`
- `AAP_KEYCODE_NAVIGATE_PREVIOUS`
- `AAP_KEYCODE_ZOOM_IN`
- `AAP_KEYCODE_ZOOM_OUT`

### Radio
- `AAP_KEYCODE_RADIO`
- `AAP_KEYCODE_CHANNEL_UP`
- `AAP_KEYCODE_CHANNEL_DOWN`
- `AAP_KEYCODE_LAST_CHANNEL`

### Automotive
- `AAP_KEYCODE_HEADSETHOOK` — Steering wheel button
- `AAP_KEYCODE_PAIRING` — BT pairing button
- `AAP_KEYCODE_BRIGHTNESS_UP`
- `AAP_KEYCODE_BRIGHTNESS_DOWN`
- `AAP_KEYCODE_3D_MODE`

### Misc
- `AAP_KEYCODE_POWER`
- `AAP_KEYCODE_SLEEP`
- `AAP_KEYCODE_WAKEUP`
- `AAP_KEYCODE_BOOKMARK`
- `AAP_KEYCODE_CALENDAR`
- `AAP_KEYCODE_CALCULATOR`
- `AAP_KEYCODE_CAMERA`
- `AAP_KEYCODE_EXPLORER`
- `AAP_KEYCODE_ENVELOPE`
- `AAP_KEYCODE_DVR`
- `AAP_KEYCODE_INFO`
- `AAP_KEYCODE_GUIDE`
- `AAP_KEYCODE_CAPTIONS`
- `AAP_KEYCODE_FOCUS`
- `AAP_KEYCODE_MANNER_MODE`

### Full Keyboard (A-Z, 0-9, F1-F12, punctuation, modifiers)
All standard Android keycodes are present, including full alphabet (A-Z), digits (0-9), function keys (F1-F12), numpad, and all modifier keys (Shift, Ctrl, Alt, Meta).

</details>

---

## Interesting Details

### Also Supports Baidu CarLife
The firmware includes full Baidu CarLife protobuf definitions (`com.baidu.carlife.protobuf.*`), including:
- CarLife authentication (request/result)
- CarLife BT HFP integration
- CarLife BT pairing/identification

This means the Alpine HU supports both Android Auto and Baidu CarLife for the Chinese market.

### Also Supports Apple CarPlay
References to CarPlay are scattered throughout:
- `miso_fw_connectivity_inner_carpaly_start` (note the typo "carpaly")
- `miso_fw_connectivity_inner_Enter_CarPlay__Fv`
- `miso_fw_base_connectivity_carplay_detached_cb`
- CarPlay platform audio via `miso_fw_connectivity_allgo_CarPlayPlatformAudioiRTOS.c`

The CarPlay implementation is provided by **Allgo** (a CarPlay/AA middleware vendor).

### Audio Routing
```
miso_fw_connectivity_resource_management_audio   — Audio resource arbiter
miso_fw_connectivity_take_audio                  — Take audio focus
miso_fw_connectivity_take_audio_screen           — Take audio + screen
miso_fw_connectivity_take_screen                 — Take screen only
miso_fw_connectivity_resource_management_screen  — Screen resource arbiter
```

This reveals the resource management pattern: audio and video are independently managed resources that can be "taken" by different subsystems (AA, CarPlay, BT audio, native UI).

### A2DP Audio
```
[BT_DEVICE] A2DP_SetSamplingFreq : 44100
[BT_DEVICE] A2DP_SetSamplingFreq : 48000
```
Supports both 44.1kHz and 48kHz A2DP.

---

## Implications for OpenAuto Prodigy

### What We're Doing Right
1. ✅ Our `ServiceDiscoveryResponse` structure matches the SDK's attribute tree
2. ✅ Our audio focus model (gain/loss/transient) matches the SDK's enum
3. ✅ Our touch input handling matches `PointerAction` and `TouchEvent.Pointer`
4. ✅ 720p default resolution is correct (480p is always added as fallback)
5. ✅ Using protobuf for all message serialization

### What We Could Improve
1. **Video focus protocol** — We don't implement video focus negotiation. Real HUs use `GAIN`/`LOSS`/`LOSS_TRANSIENT` to switch between native UI and AA projection. This would let us properly handle overlay scenarios (reverse camera, settings screens).
2. **Audio focus completeness** — We have basic focus but not all states (GAIN_MEDIA_ONLY, GAIN_TRANSIENT_GUIDANCE_ONLY, LOSS_TRANSIENT_CAN_DUCK).
3. **Driving status sensor** — Always required by the SDK. We should send it.
4. **Navigation status subscription** — We could subscribe to nav turn-by-turn data for an instrument cluster display.
5. **Media browsing/playback status** — We could show now-playing info in our native UI when AA is in the background.
6. **Phone status** — We could show incoming call info in our native UI overlay.
7. **ByeBye protocol** — Clean disconnect with reason codes instead of just dropping the connection.

### Key Protocol Detail: 480p Always Required
The firmware explicitly states that 800x480 resolution is mandatory and auto-added. This suggests the phone always expects at least 480p support, even if 720p/1080p are preferred. We should ensure 480p is in our `ServiceDiscoveryResponse`.
