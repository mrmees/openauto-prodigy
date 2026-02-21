# Configuration Schema

## Overview

OpenAuto Prodigy uses YAML configuration stored at `~/.openauto/config.yaml`. The config is managed by `YamlConfig` and accessed by plugins through `ConfigService`.

## Schema

```yaml
# WiFi AP settings (for wireless Android Auto)
wifi:
  ssid: "OpenAutoProdigy"      # string — hostapd SSID
  password: "openauto123"       # string — WPA2 passphrase
  interface: "wlan0"            # string — wireless interface name

# Android Auto connection
connection:
  tcp_port: 5277                # int — TCP port for AA connection

# Video settings
video:
  fps: 60                       # int — target framerate (30 or 60)

# Display settings
display:
  width: 1024                   # int — display width in pixels
  height: 600                   # int — display height in pixels

# Touch input
touch:
  device: ""                    # string — evdev path (auto-detected if empty)

# Night mode
night_mode:
  mode: "manual"                # string — "manual", "timed", "gpio"
  start_hour: 20                # int — hour to enable night mode (timed mode)
  end_hour: 6                   # int — hour to disable night mode (timed mode)
  gpio_pin: -1                  # int — GPIO pin number (gpio mode)

# Launcher tiles
launcher:
  tiles:
    - id: "android-auto"
      label: "Android Auto"
      icon: "\uEFF7"            # Material Icon codepoint (directions_car)
      action: "plugin:org.openauto.android-auto"
    - id: "music"
      label: "Music"
      icon: "\uE405"            # Material Icon codepoint (headphones)
      action: "plugin:org.openauto.bt-audio"
    - id: "phone"
      label: "Phone"
      icon: "\uF0D4"            # Material Icon codepoint (phone)
      action: "plugin:org.openauto.phone"
    - id: "settings"
      label: "Settings"
      icon: "\uE8B8"            # Material Icon codepoint (settings)
      action: "navigate:settings"
```

## Accessing Config from Plugins

```cpp
// In plugin initialize():
auto* config = context->configService();
int fps = config->value("video.fps").toInt();
config->setValue("video.fps", 30);
```

## Plugin Config Namespace

Plugins should store their configuration under `plugins.<plugin-id>`:

```yaml
plugins:
  org.openauto.bt-audio:
    auto_connect: true
    default_volume: 80
```

Access via `configService()->value("plugins.org.openauto.bt-audio.auto_connect")`.

## Migration Policy

- **Additive only** for minor versions — new keys may be added, existing keys keep their meaning.
- **Breaking changes** require a major version bump and migration logic in `YamlConfig::load()`.
- Default values are defined in `YamlConfig::initDefaults()` — missing keys resolve to defaults.
- Legacy INI config (`openauto_system.ini`) is auto-migrated to YAML on first run.

## Theme Configuration

Themes are separate from the main config. See `~/.openauto/themes/default/theme.yaml`:

```yaml
name: "Default"
day:
  background: "#1a1a2e"
  text: "#e0e0e0"
  accent: "#0f3460"
night:
  background: "#0d0d1a"
  text: "#c0c0d0"
  accent: "#0a2540"
```

Themes are loaded by `ThemeService::loadTheme(path)`.
