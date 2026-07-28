# Configuration Schema

## Overview

OpenAuto Prodigy reads `~/.openauto/config.yaml` at startup. `YamlConfig`
constructs the built-in defaults, deep-merges the user's YAML over them, and
exposes the result through typed accessors and `ConfigService`.

Important behavior:

- Missing keys inherit built-in defaults.
- Unknown keys loaded from an existing file are retained, but generic writes
  accept only scalar keys registered in the defaults tree.
- Maps and sequences use dedicated accessors rather than generic dot-path
  writes.
- Plugin data belongs under `plugin_config` and is accessed with
  `pluginValue()` / `setPluginValue()`.
- A malformed existing file is moved aside with a `.corrupt` suffix and the
  process continues with defaults.
- Saving replaces the file atomically.

## Practical YAML Example

This example uses current public keys and relies on defaults for everything
not shown.

```yaml
hardware_profile: rpi4

display:
  brightness: 80
  screen_size: 7.0
  theme: default
  wallpaper_override: ""
  clock_24h: false
  force_dark_mode: true

connection:
  bt_name: OpenAutoProdigy       # written by the installer; read as the adapter alias
  auto_connect_aa: true
  bt_discoverable: true
  gal_version: "6.0"
  wifi_ap:
    interface: wlan0
    ssid: OpenAutoProdigy
    password: prodigy1234
    channel: 36
    band: a
  tcp_port: 5277
  protocol_capture:
    enabled: false
    format: jsonl
    include_media: false
    path: /tmp/oaa-protocol-capture.jsonl

audio:
  master_volume: 80
  output_device: auto
  buffer_ms:
    media: 500
    speech: 500
    system: 500
  microphone:
    device: auto
    gain: 1.0
  equalizer:
    streams:
      media: { preset: Flat }
      navigation: { preset: Voice }
      system: { preset: Voice }
    user_presets: []

touch:
  device: ""

logging:
  verbose: false
  debug_categories: []

phone:
  reject_sco_during_aa: false
  settle_grace_ms: 2000

video:
  fps: 30
  resolution: 720p
  dpi: 140
  codecs: [h265, h264]
  decoder:
    h264: auto
    h265: auto
    vp9: auto
    av1: auto

identity:
  head_unit_name: OpenAuto Prodigy
  manufacturer: OpenAuto Project
  model: Raspberry Pi 4
  car_model: ""
  car_year: ""
  left_hand_drive: true
  server_id: ""

api:
  enabled: true
  tcp_port: 9810
  ws_port: 9811
  expose_lan: false
  max_queue_bytes: 1048576
  pairing_timeout_s: 120
  handshake_timeout_ms: 5000

sensors:
  night_mode:
    source: time
    day_start: "07:00"
    night_start: "19:00"
    gpio_pin: 17
    gpio_active_high: true
  gps:
    enabled: true
    source: none

plugins:
  enabled: [org.openauto.android-auto]
  disabled: []

plugin_config:
  org.example.my-plugin:
    auto_connect: true

ui:
  scale: 0
  fontScale: 0

home:
  gridDensityBias: 0

navbar:
  edge: bottom
  show_during_aa: true
  gesture:
    tap_max_ms: 200
    short_hold_max_ms: 600
```

`connection.bt_name` is an installer-owned value read by
`BluetoothManager`; it is not part of the generic writable defaults schema.
The example uses a valid WPA2 passphrase; normal installers generate one. The
shorter built-in configuration fallback is not suitable as a hostapd
passphrase.
The application does not use display pixel dimensions from YAML. Window
geometry comes from the runtime display/command-line geometry path.

## Scalar Key Reference

These are the scalar leaves registered by `YamlConfig::initDefaults()` and
therefore readable and writable with `ConfigService` dot paths.

### Platform, Display, and Input

| Dot Path | Type | Default | Description |
|---|---|---|---|
| `hardware_profile` | string | `rpi4` | Target hardware profile. |
| `display.brightness` | int | `80` | Brightness/dimming level (clamped to 5–100). Startup applies the configured value to the active backend even when it is the default `80`. |
| `display.screen_size` | double | `7.0` | Physical screen diagonal in inches, used for layout and PPI calculations. |
| `display.theme` | string | `default` | Selected theme ID. |
| `display.wallpaper_override` | string | empty | Empty uses the theme wallpaper; `none` disables it; custom choices are `file://` URLs. |
| `display.clock_24h` | bool | `false` | Clock-format preference. |
| `display.force_dark_mode` | bool | `true` | Forces the dark palette while real night state continues to drive AA sensors. |
| `touch.device` | string | empty | evdev device path; empty enables device scanning. |
| `home.gridDensityBias` | int | `0` | Grid density bias; typed access clamps it to -1 through 1. |
| `navbar.edge` | string | `bottom` | `bottom`, `top`, `left`, or `right`. |
| `navbar.show_during_aa` | bool | `true` | Whether the navbar remains visible during projection. |
| `navbar.gesture.tap_max_ms` | int | `200` | Maximum tap duration. |
| `navbar.gesture.short_hold_max_ms` | int | `600` | Upper bound for a short hold. |

### Logging

| Dot Path | Type | Default | Description |
|---|---|---|---|
| `logging.verbose` | bool | `false` | Enables verbose logging; writable through generic scalar dot paths. |

### Connection and API

| Dot Path | Type | Default | Description |
|---|---|---|---|
| `connection.auto_connect_aa` | bool | `true` | Automatically connect when the phone's AA service is available. |
| `connection.bt_discoverable` | bool | `true` | Bluetooth discoverability preference. |
| `connection.gal_version` | string | `6.0` | Durable session-wide requested GAL policy, independent of the CLUSTER lab. Accepted values are exactly `1.7`, `4.3`, `5.0`, `5.1`, and `6.0`; missing or invalid values resolve to the highest accepted value (`6.0`). A real change gracefully reconnects active AA. |
| `connection.wifi_ap.interface` | string | `wlan0` | AP network interface. |
| `connection.wifi_ap.ssid` | string | `OpenAutoProdigy` | AP SSID; runtime startup may synchronize it from hostapd. |
| `connection.wifi_ap.password` | string | `prodigy` | AP passphrase; runtime startup may synchronize it from hostapd. |
| `connection.wifi_ap.channel` | int | `36` | AP channel. |
| `connection.wifi_ap.band` | string | `a` | hostapd band (`a` for 5 GHz, `g` for 2.4 GHz). |
| `connection.tcp_port` | int | `5277` | Wireless Android Auto TCP port. |
| `connection.protocol_capture.enabled` | bool | `false` | Enables protocol frame capture. |
| `connection.protocol_capture.format` | string | `jsonl` | `jsonl` or `tsv`. |
| `connection.protocol_capture.include_media` | bool | `false` | Includes high-volume media frames. |
| `connection.protocol_capture.path` | string | `/tmp/oaa-protocol-capture.jsonl` | Capture output path. |
| `api.enabled` | bool | `true` | Enables the External API listeners. |
| `api.tcp_port` | int | `9810` | External API TCP listener port. |
| `api.ws_port` | int | `9811` | External API WebSocket listener port. |
| `api.expose_lan` | bool | `false` | Binds listeners beyond loopback when enabled. |
| `api.max_queue_bytes` | int | `1048576` | Per-session queued-output limit. |
| `api.pairing_timeout_s` | int | `120` | Pairing-window duration. |
| `api.handshake_timeout_ms` | int | `5000` | Client handshake timeout. |

### Audio, Phone, and Video

| Dot Path | Type | Default | Description |
|---|---|---|---|
| `audio.master_volume` | int | `80` | Master output volume. |
| `audio.output_device` | string | `auto` | PipeWire output node or automatic selection. |
| `audio.buffer_ms.media` | int | `500` | Static media playback buffer target in milliseconds; clamped to 500–5000. |
| `audio.buffer_ms.speech` | int | `500` | Static speech/navigation buffer target in milliseconds; clamped to 500–5000. |
| `audio.buffer_ms.system` | int | `500` | Static AA system-sound buffer target in milliseconds; clamped to 500–5000. |
| `audio.microphone.device` | string | `auto` | PipeWire capture node or automatic selection. |
| `audio.microphone.gain` | double | `1.0` | Microphone gain multiplier, normalized to 0.5–4.0 when Assistant AVInput capture starts. |
| `audio.equalizer.streams.media.preset` | string | `Flat` | Media/local/BT-tap EQ preset. |
| `audio.equalizer.streams.navigation.preset` | string | `Voice` | Navigation EQ preset. |
| `audio.equalizer.streams.system.preset` | string | `Voice` | AA system-sound EQ preset; this is not HFP call audio. |
| `phone.reject_sco_during_aa` | bool | `false` | Call-audio coexistence policy switch. |
| `phone.settle_grace_ms` | int | `2000` | HFP settle grace interval. |
| `video.fps` | int | `30` | Requested projection frame rate. |
| `video.resolution` | string | `720p` | Requested mode: `480p`, `720p`, or `1080p`. |
| `video.dpi` | int | `140` | Android Auto density hint. |
| `video.decoder.h264` | string | `auto` | H.264 decoder choice. |
| `video.decoder.h265` | string | `auto` | H.265 decoder choice. |
| `video.decoder.vp9` | string | `auto` | VP9 decoder choice if the codec is enabled. |
| `video.decoder.av1` | string | `auto` | AV1 decoder choice if the codec is enabled. |

### Identity and Sensors

| Dot Path | Type | Default | Description |
|---|---|---|---|
| `identity.head_unit_name` | string | `OpenAuto Prodigy` | Friendly head-unit name. |
| `identity.manufacturer` | string | `OpenAuto Project` | Informational manufacturer shown in the on-screen Information page. AA discovery uses a fixed compatibility identity instead. |
| `identity.model` | string | `Raspberry Pi 4` | Informational model shown in the on-screen Information page. AA discovery uses a fixed compatibility identity instead. |
| `identity.car_model` | string | empty | Optional vehicle model. |
| `identity.car_year` | string | empty | Optional vehicle year. |
| `identity.left_hand_drive` | bool | `true` | Handedness for UI layout. |
| `identity.server_id` | string | empty | Stable External API server ID, minted on first API start. |
| `sensors.night_mode.source` | string | `time` | `time`, `gpio`, or `none`. |
| `sensors.night_mode.day_start` | string | `07:00` | Day-mode start in `HH:MM` form. |
| `sensors.night_mode.night_start` | string | `19:00` | Night-mode start in `HH:MM` form. |
| `sensors.night_mode.gpio_pin` | int | `17` | GPIO pin used by GPIO night mode. |
| `sensors.night_mode.gpio_active_high` | bool | `true` | GPIO active polarity. |
| `sensors.gps.enabled` | bool | `true` | Enables the configured GPS source. |
| `sensors.gps.source` | string | `none` | GPS source identifier. |

The configured night source is owned for the full application lifetime. Its
last valid state drives both the shell's real day/night palette state and the
Android Auto `NIGHT_DATA` cache; starting or stopping a projection session does
not restart the source. Time boundaries are inclusive at `day_start` and
`night_start`. GPIO setup and reads retry once per second after export,
direction, or value failures, retaining the last valid state until recovery.
If the exported GPIO directory disappears, the next polls repeat export and
direction setup.
The existing `none` behavior remains a fallback to the configured time policy.

### UI Overrides

All UI overrides default to `0`, meaning “use the automatically derived
value.” `ui.scale` and `ui.fontScale` are global multipliers. `ui.fontFloor`
is the legacy global font floor. The following token leaves are registered
under `ui.tokens`:

`rowH`, `touchMin`, `fontTitle`, `fontBody`, `fontSmall`, `fontHeading`,
`fontTiny`, `headerH`, `iconSize`, `radius`, `radiusSmall`, `radiusLarge`,
`tileW`, `tileH`, `trackThick`, `trackThin`, `knobSize`, `knobSizeSmall`,
`albumArt`, `callBtnSize`, `overlayBtnW`, `overlayBtnH`, and `navbarThick`.

## Structured Configuration

The following maps or sequences are not writable as whole values through
`setValueByPath()`:

| Path | Default / Shape | Owner |
|---|---|---|
| `video.codecs` | `[h265, h264]` | Ordered service-discovery codec list read by the typed `videoCodecs()` accessor. At GAL 5.0+, the first recognized entry is the single codec advertised for MAIN and CLUSTER; the accepted default is H.265 first, while H.264 remains the supported fallback. Explicit YAML order is authoritative. Edit it in YAML: the current QML scalar bridge cannot read or write the sequence. Keep it to H.264/H.265 because the decoder does not identify VP9/AV1 streams. |
| `logging.debug_categories` | `[]` of strings | Selective debug categories using the canonical values `aa`, `bt`, `audio`, `plugin`, `ui`, `core`, and `eq`; `aa` also enables the Android Auto protocol library category. Owned by the typed `loggingDebugCategories()` / `setLoggingDebugCategories()` accessors. Generic dot-path access remains scalar-only. |
| `audio.equalizer.streams.<stream>.gains` | Optional list of exactly ten finite values, clamped to ±12 dB | `EqualizerService` dedicated accessors. |
| `audio.equalizer.streams.<stream>.bypassed` | Optional bool, effectively `false` when absent | `EqualizerService` dedicated accessors. |
| `audio.equalizer.user_presets` | `[]`; entries contain a non-blank, non-bundled, unique `name` and ten `gains` | `EqualizerService` dedicated accessors. Invalid/duplicate restored entries are ignored in memory without causing a clean-start rewrite. |
| `plugins.enabled` | `[org.openauto.android-auto]` | Plugin manager / typed accessor. |
| `plugins.disabled` | `[]` | Reserved plugin-disable list. |
| `plugin_config.<plugin-id>` | Empty map by default | Per-plugin scalar or string-list storage. |
| `widget_grid` | Version 4, active dashboard `home`, one two-page Home dashboard with no placements | Dashboard manager. |
| `widget_config` | Version 1 legacy pane layout | Retained only for backward compatibility. |

Current grid dashboard entries use this shape:

```yaml
widget_grid:
  version: 4
  active_dashboard: home
  grid_cols: 8                 # optional saved dimensions
  grid_rows: 5
  dashboards:
    - id: home
      name: Home
      next_instance_id: 1
      page_count: 2
      placements:
        - instance_id: org.openauto.clock-0
          widget_id: org.openauto.clock
          col: 0
          row: 0
          col_span: 2
          row_span: 2
          opacity: 0.25
          page: 0
          config: { style: digital }
```

The placement field names are persistent API: append fields rather than
renaming or repurposing them.

## Access Patterns

### Generic Config Values

```cpp
oap::IConfigService* config = context->configService();

int fps = config->value("video.fps").toInt();
config->setValue("video.fps", 60);
config->save();
```

Generic writes accept scalar paths present in the built-in defaults. They do
not perform range validation, so callers must enforce the documented domain.

### Plugin-Scoped Values

```cpp
oap::IConfigService* config = context->configService();

bool enabled = config->pluginValue(id(), "auto_connect").toBool();
config->setPluginValue(id(), "auto_connect", true);
config->save();
```

Plugin IDs are literal keys, including dots. Do not try to address a plugin's
namespace through a generic dotted path; use the plugin-scoped methods.

## Themes

Themes live in `~/.openauto/themes/<theme-id>/theme.yaml`, and
`display.theme` stores `<theme-id>`. A theme file declares `id`, `name`,
`version`, `font_family`, and `day`/`night` Material Design 3 color maps. For
example, the bundled `default` ID has the display name `Prodigy`:

```yaml
id: default
name: Prodigy
version: 2.0.0
font_family: Lato
day:
  primary: "#ff3531e0"
  on-primary: "#ffffffff"
  surface: "#fffcf8ff"
  on-surface: "#ff1b1b24"
night:
  primary: "#ffc1c1ff"
  on-primary: "#ff1200a9"
  surface: "#ff12131c"
  on-surface: "#ffe4e1ee"
```

Theme selection and wallpaper overrides apply live through `ThemeService`.

## Migration and Removed Keys

- The legacy EQ stream `audio.equalizer.streams.phone` migrates to
  `audio.equalizer.streams.system`. If both exist, `system` wins.
- Legacy widget-grid versions are migrated to the version-4 dashboard shape.
- `identity.sw_version` is removed. Version identity comes from compiled
  `OAP_VERSION`; leftover YAML is retained but ignored.

## File Location and Reloading

The runtime path is fixed to `~/.openauto/config.yaml`. The application does
not provide a configuration-path environment override. On-screen settings and
IPC operations can save individual changes, but the application does not watch
the file for arbitrary external edits; restart after editing YAML by hand.
