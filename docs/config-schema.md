# Configuration Schema

## Overview

OpenAuto Prodigy uses YAML configuration stored at `~/.openauto/config.yaml`. The config is managed by the `YamlConfig` class and accessed by core code and plugins through `ConfigService`. Configuration is loaded at startup with defaults from `YamlConfig::initDefaults()`, allowing new installations to work immediately without manual configuration.

**Key characteristics:**
- Single source of truth: YAML file backed by `YamlConfig::root_` (YAML::Node tree)
- Deep merge on load: user config overlays on defaults (missing keys inherit defaults)
- Typed accessors: C++ getters/setters for common keys (e.g., `config->displayBrightness()`)
- Dot-path access: generic `valueByPath("display.brightness")` for scalar config keys (maps/sequences use typed accessors)
- Plugin-scoped storage: `plugin_config` section for per-plugin settings

---

## Complete YAML Example

```yaml
# Hardware profile — identifies the target platform
hardware_profile: "rpi4"

# Display configuration
display:
  brightness: 80                    # 0-100, screen brightness level
  theme: "default"                  # theme filename (without .yaml) in ~/.openauto/themes/
  orientation: "landscape"          # "landscape" or "portrait"
  width: 1024                       # display width in pixels
  height: 600                       # display height in pixels

# WiFi and Android Auto connection
connection:
  auto_connect_aa: true             # auto-connect to phone if AA service is available
  bt_discoverable: true             # allow Bluetooth discovery (for AA/BT Audio/Phone)
  wifi_ap:
    interface: "wlan0"              # wireless interface name for AP mode
    ssid: "OpenAutoProdigy"         # WiFi SSID (must match hostapd.conf)
    password: "prodigy"             # WiFi WPA2 passphrase (must match hostapd.conf)
    channel: 36                     # WiFi channel (5 GHz: 36-48, 149-165)
    band: "a"                       # WiFi band ("a" = 5 GHz, "g" = 2.4 GHz)
  tcp_port: 5277                    # TCP port for AA connection handshake
  protocol_capture:
    enabled: false                  # write AA frame dumps for protobuf regression validation
    format: "jsonl"                 # "jsonl" (validator-ready) or "tsv" (human-readable)
    include_media: false            # include noisy AV media payload frames (usually keep false)
    path: "/tmp/oaa-protocol-capture.jsonl"  # capture output path

# Audio configuration
audio:
  master_volume: 80                 # 0-100, app-wide volume level
  output_device: "auto"             # PipeWire output device ("auto" = system default)
  microphone:
    device: "auto"                  # PipeWire input device ("auto" = system default)
    gain: 1.0                       # microphone gain multiplier

# Touch input configuration
touch:
  device: ""                        # evdev device path (empty = auto-detect by VID:PID 3343:5710)

# Video configuration
video:
  fps: 30                           # framerate (30 or 60, phone will adapt to this)
  resolution: "720p"               # "480p" (800x480), "720p" (1280x720), "1080p" (1920x1080)
  dpi: 140                         # screen DPI (affects AA scaling hints)

# Head unit identity — sent to phone during AA discovery
identity:
  head_unit_name: "OpenAuto Prodigy"  # friendly name
  manufacturer: "OpenAuto Project"    # OEM name (appears in phone AA settings)
  model: "Raspberry Pi 4"             # hardware model name
  sw_version: "0.3.0"                 # app version
  car_model: ""                       # optional: car model name
  car_year: ""                        # optional: car year
  left_hand_drive: true               # true = LHS, false = RHS (affects UI mirroring)

# Sensors and automatic features
sensors:
  night_mode:
    source: "time"                  # "time" = clock-based, "gpio" = GPIO input, "none" = disabled
    day_start: "07:00"              # time to enable day mode (HH:MM)
    night_start: "19:00"            # time to enable night mode (HH:MM)
    gpio_pin: 17                    # GPIO pin for GPIO-based mode
    gpio_active_high: true          # true = active high, false = active low
  gps:
    enabled: true                   # enable GPS features (map auto-pan, ETA, etc.)
    source: "none"                  # GPS source ("none", "gpsd", "android", "ublox", etc.)

# Navigation strip (icon bar at bottom)
nav_strip:
  order:
    - "org.openauto.android-auto"  # list of enabled plugin IDs in display order
  show_labels: true                 # show text labels under icons (false = icons only)

# Launcher tiles — home screen quick-access buttons
launcher:
  tiles:
    - id: "org.openauto.android-auto"
      label: "Android Auto"
      icon: "\ueff7"                # Material Symbols codepoint (directions_car)
      action: "plugin:org.openauto.android-auto"
    - id: "org.openauto.bt-audio"
      label: "Music"
      icon: "\uf01f"                # Material Symbols codepoint (headphones)
      action: "plugin:org.openauto.bt-audio"
    - id: "org.openauto.phone"
      label: "Phone"
      icon: "\uf0d4"                # Material Symbols codepoint (phone)
      action: "plugin:org.openauto.phone"
    - id: "settings"
      label: "Settings"
      icon: "\ue8b8"                # Material Symbols codepoint (settings)
      action: "navigate:settings"

# Plugin management
plugins:
  enabled:
    - "org.openauto.android-auto"  # list of enabled plugin IDs (loaded at startup)
  disabled: []                      # plugins to disable (plugin IDs that exist but shouldn't load)

# Plugin-scoped configuration — plugins store their settings here
plugin_config:
  org.openauto.bt-audio:
    # example: plugins can define custom keys under their plugin ID
    some_setting: "some_value"
  org.openauto.phone:
    another_setting: 42
```

---

## Key Reference Table

All keys are documented with their dot-path (for `valueByPath()`), type, default value, and description.

| Dot-Path | Type | Default | Description |
|----------|------|---------|-------------|
| `hardware_profile` | string | `"rpi4"` | Hardware profile ID (currently only `"rpi4"` is defined) |
| `display.brightness` | int | `80` | Display brightness level (0-100) |
| `display.theme` | string | `"default"` | Theme filename to load from `~/.openauto/themes/` |
| `display.orientation` | string | `"landscape"` | Display orientation (`"landscape"` or `"portrait"`) |
| `display.width` | int | `1024` | Display width in pixels |
| `display.height` | int | `600` | Display height in pixels |
| `connection.auto_connect_aa` | bool | `true` | Auto-connect to AA when phone becomes available |
| `connection.bt_discoverable` | bool | `true` | Allow Bluetooth discovery from phone (required for AA) |
| `connection.wifi_ap.interface` | string | `"wlan0"` | WiFi interface name for AP mode |
| `connection.wifi_ap.ssid` | string | `"OpenAutoProdigy"` | WiFi SSID broadcast by AP (must match hostapd.conf) |
| `connection.wifi_ap.password` | string | `"prodigy"` | WiFi passphrase (must match hostapd.conf) |
| `connection.wifi_ap.channel` | int | `36` | WiFi channel (5 GHz: 36-48, 149-165; 2.4 GHz: 1-13) |
| `connection.wifi_ap.band` | string | `"a"` | WiFi band (`"a"` = 5 GHz, `"g"` = 2.4 GHz) |
| `connection.tcp_port` | int | `5277` | TCP port for AA handshake and connection |
| `connection.protocol_capture.enabled` | bool | `false` | Enable protocol frame capture dumps |
| `connection.protocol_capture.format` | string | `"jsonl"` | Capture format (`"jsonl"` for tooling, `"tsv"` for manual inspection) |
| `connection.protocol_capture.include_media` | bool | `false` | Include AV media data frames (large/noisy) |
| `connection.protocol_capture.path` | string | `"/tmp/oaa-protocol-capture.jsonl"` | Output file path for capture dump |
| `audio.master_volume` | int | `80` | Master volume level (0-100, used by audio plugins) |
| `audio.output_device` | string | `"auto"` | PipeWire output device name (`"auto"` = system default) |
| `audio.microphone.device` | string | `"auto"` | PipeWire input device name (`"auto"` = system default) |
| `audio.microphone.gain` | double | `1.0` | Microphone input gain multiplier (0.5-4.0 typical) |
| `touch.device` | string | `""` | evdev device path (`""` = auto-detect DFRobot USB Multi Touch by VID:PID 3343:5710) |
| `video.fps` | int | `30` | Target framerate (30 or 60; phone will adapt) |
| `video.resolution` | string | `"720p"` | AA video resolution (`"480p"`, `"720p"`, `"1080p"`) |
| `video.dpi` | int | `140` | Screen DPI (hints to Android Auto for scaling) |
| `identity.head_unit_name` | string | `"OpenAuto Prodigy"` | Friendly name shown in phone AA settings |
| `identity.manufacturer` | string | `"OpenAuto Project"` | OEM manufacturer name |
| `identity.model` | string | `"Raspberry Pi 4"` | Hardware model name |
| `identity.sw_version` | string | `"0.3.0"` | App software version |
| `identity.car_model` | string | `""` | Optional car model name |
| `identity.car_year` | string | `""` | Optional car year |
| `identity.left_hand_drive` | bool | `true` | `true` = LHS, `false` = RHS (may affect UI layout) |
| `sensors.night_mode.source` | string | `"time"` | Night mode source (`"time"`, `"gpio"`, `"none"`) |
| `sensors.night_mode.day_start` | string | `"07:00"` | Time to enable day mode (HH:MM format) |
| `sensors.night_mode.night_start` | string | `"19:00"` | Time to enable night mode (HH:MM format) |
| `sensors.night_mode.gpio_pin` | int | `17` | GPIO pin number for GPIO-based night mode |
| `sensors.night_mode.gpio_active_high` | bool | `true` | `true` = high activates night mode, `false` = low activates |
| `sensors.gps.enabled` | bool | `true` | Enable GPS features (when source is configured) |
| `sensors.gps.source` | string | `"none"` | GPS source (`"none"`, `"gpsd"`, `"android"`, `"ublox"`, etc.) |
| `nav_strip.show_labels` | bool | `true` | Show text labels under navigation strip icons |
| `plugins.enabled` | list[string] | `["org.openauto.android-auto"]` | Plugin IDs to load at startup |
| `plugins.disabled` | list[string] | `[]` | Plugin IDs to disable (if discovered) |

**Note:** `launcher.tiles` and `nav_strip.order` are arrays with complex structure; see examples below.

---

## Access Patterns

### From Core Code (C++)

**Typed accessors** — the most common and recommended pattern:

```cpp
#include "core/YamlConfig.hpp"

// In core services or UI code with access to YamlConfig:
YamlConfig* config = /* obtained from initialization */;

int fps = config->videoFps();                         // get: 30
config->setVideoFps(60);                              // set

QString ssid = config->wifiSsid();                    // get: "OpenAutoProdigy"
config->setWifiSsid("MyWiFi");                        // set

bool autoConnect = config->autoConnectAA();           // get: true
config->setAutoConnectAA(false);                      // set
```

**Dot-path access** — for generic or dynamic keys:

```cpp
#include "core/services/ConfigService.hpp"

// In plugins or services with ConfigService:
ConfigService* configService = /* from HostContext */;

QVariant value = configService->value("display.brightness");  // QVariant(80)
configService->setValue("display.brightness", 50);             // set to 50

// Read with automatic type conversion:
int brightness = configService->value("display.brightness").toInt();
QString ssid = configService->value("connection.wifi_ap.ssid").toString();
bool autoConnect = configService->value("connection.auto_connect_aa").toBool();
```

### From Plugins (via ConfigService)

**Plugin-scoped config** — each plugin reads/writes its own namespace:

```cpp
#include "core/services/ConfigService.hpp"

ConfigService* configService = /* from HostContext in initialize() */;

// Read plugin-specific settings (from plugin_config.<plugin_id>.*)
QVariant customValue = configService->pluginValue(
    "org.openauto.bt-audio",
    "auto_connect"
);

// Write plugin-specific settings
configService->setPluginValue(
    "org.openauto.bt-audio",
    "auto_connect",
    true
);
```

**Generic dot-path access** — plugins can also read general config:

```cpp
int fps = configService->value("video.fps").toInt();
QString ssid = configService->value("connection.wifi_ap.ssid").toString();
```

---

## Plugin Config Namespace

Plugin-scoped configuration is stored under the `plugin_config` section:

```yaml
plugin_config:
  org.openauto.bt-audio:
    auto_connect: true
    volume_on_connect: 80
  org.openauto.phone:
    dialer_theme: "dark"
    ring_volume: 100
  org.example.custom-plugin:
    custom_key_1: "value1"
    custom_key_2: 42
```

**Access in C++:**

```cpp
// From ConfigService (preferred for plugins):
ConfigService* config = context->configService();

bool autoConnect = config->pluginValue("org.openauto.bt-audio", "auto_connect").toBool();
config->setPluginValue("org.openauto.bt-audio", "auto_connect", false);

// Or via dot-path (if you prefer):
QVariant val = config->value("plugin_config.org.openauto.bt-audio.auto_connect");
```

**Type safety:** `pluginValue()` and `setPluginValue()` automatically handle type conversion (bool, int, double, string).

---

## Launcher Tiles

Launcher tiles are quick-access buttons on the home screen. Each tile is defined as an object with `id`, `label`, `icon`, and `action`:

```yaml
launcher:
  tiles:
    - id: "org.openauto.android-auto"
      label: "Android Auto"
      icon: "\ueff7"                # Material Symbols: directions_car
      action: "plugin:org.openauto.android-auto"
    - id: "org.openauto.bt-audio"
      label: "Music"
      icon: "\uf01f"                # Material Symbols: headphones
      action: "plugin:org.openauto.bt-audio"
    - id: "org.openauto.phone"
      label: "Phone"
      icon: "\uf0d4"                # Material Symbols: phone
      action: "plugin:org.openauto.phone"
    - id: "settings"
      label: "Settings"
      icon: "\ue8b8"                # Material Symbols: settings
      action: "navigate:settings"
```

**Fields:**
- `id` (string): unique tile identifier
- `label` (string): displayed text (may be hidden on small screens)
- `icon` (string): Unicode codepoint from Material Symbols font (e.g., `"\ueff7"`)
- `action` (string): action on tap — either `"plugin:org.openauto.plugin-id"` or `"navigate:view-name"`

**Access in C++:**

```cpp
QList<QVariantMap> tiles = config->launcherTiles();
for (const auto& tile : tiles) {
    QString id = tile["id"].toString();
    QString label = tile["label"].toString();
    QString icon = tile["icon"].toString();
    QString action = tile["action"].toString();
}

// Update tiles:
QList<QVariantMap> newTiles = { /* ... */ };
config->setLauncherTiles(newTiles);
```

---

## Navigation Strip Order

The navigation strip (icon bar) displays plugin shortcuts. The order and visibility are controlled by `nav_strip.order` and `nav_strip.show_labels`:

```yaml
nav_strip:
  order:
    - "org.openauto.android-auto"
    - "org.openauto.bt-audio"
    - "org.openauto.phone"
  show_labels: true                 # false = icon-only (recommended for 1024x600)
```

**Access in C++:**

```cpp
QStringList enabled = config->enabledPlugins();  // reads plugins.enabled
// nav_strip.order is a separate list accessed via typed accessor (not dot-path)
bool showLabels = config->value("nav_strip.show_labels").toBool();
```

---

## Theme Configuration

Themes are separate from the main config.yaml and live in `~/.openauto/themes/<theme_name>/theme.yaml`. The selected theme is referenced by name in `display.theme`:

```yaml
# From config.yaml:
display:
  theme: "default"

# This loads ~/.openauto/themes/default/theme.yaml
```

**Theme file structure** (`~/.openauto/themes/default/theme.yaml`):

```yaml
name: "Default"
day:
  background: "#1a1a2e"
  text: "#e0e0e0"
  accent: "#0f3460"
  accent_secondary: "#16a34a"
night:
  background: "#0d0d1a"
  text: "#c0c0d0"
  accent: "#0a2540"
  accent_secondary: "#15803d"
```

Themes are loaded by `ThemeService::loadTheme(themeName)` and colors are exposed as Q_PROPERTY bindings for QML. See `docs/development.md` for theme development details.

---

## Migration Policy

**Versioning & Changes:**
- **Additive changes** (minor versions): new keys added to `initDefaults()`, existing keys keep their meaning. No migration needed.
- **Breaking changes** (major versions): require a new migration function in `YamlConfig::load()` to transform old config to new schema.

**Default values:** Any key missing from the user's config.yaml inherits its default from `YamlConfig::initDefaults()`. This allows new installations to work immediately and new keys to be introduced without breaking existing configs.

**Schema validation:** `setValueByPath()` validates against `buildDefaultsNode()` — you can only set values for keys that exist in the defaults schema. This prevents accidental creation of invalid keys.

---

## Config File Location

- **Default:** `~/.openauto/config.yaml`
- **Environment override:** set `OPENAUTO_CONFIG_PATH` to use a different location
- **Web config panel:** provides a GUI for common settings (uses IPC to communicate with the app)
- **Hot reload:** currently not supported; changes require app restart

---

## Example: Adding a New Plugin Setting

To add a plugin-specific setting that can be toggled from the settings UI:

**1. In `config.yaml`:**
```yaml
plugin_config:
  org.openauto.my-plugin:
    my_setting: true
```

**2. In plugin code:**
```cpp
void MyPlugin::initialize(IHostContext* context) {
    ConfigService* config = context->configService();
    bool mySetting = config->pluginValue("org.openauto.my-plugin", "my_setting").toBool();
}

void MyPlugin::updateSetting(bool value) {
    ConfigService* config = context->configService();
    config->setPluginValue("org.openauto.my-plugin", "my_setting", value);
}
```

**3. In settings UI (QML), access via:** `ApplicationController` exposes config service to QML bindings.

---

## Known Limitations & TODOs

- **Hot reload:** Config changes require app restart; live reload on config file change not yet implemented
- **Validation:** `setValueByPath()` only validates against schema presence; no type checking or range validation
- **Migrations:** Only additive changes are currently tested; breaking change migration is untested
- **Theme live switch:** Theme changes require app restart
