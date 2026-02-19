# OpenAuto Prodigy: Architecture & UX Design

**Date:** 2026-02-18
**Status:** Draft — revised after architectural review

---

## 1. Project Vision

OpenAuto Prodigy is a clean-room open-source car head unit application for Raspberry Pi. It replaces the defunct OpenAuto Pro (BlueWave Studio) with a modern, extensible, community-driven platform.

**Target audience:** DIY car enthusiasts and tinkerers who want a Pi-based head unit with wireless Android Auto and room to grow.

**Design principles:**
- Pi 4 is the golden path; other hardware supported via profiles
- Modular from the start — features are plugins, not monolithic code
- Ship minimal, extend easily
- Configuration should be powerful for tinkerers, invisible for casual users

---

## 2. Target Hardware

### Primary Platform
- Raspberry Pi 4 (ARM64), RPi OS Trixie (Debian 13)
- Standard HDMI + USB touchscreens (DFRobot 1024x600, official Pi 7" 800x480)
- Built-in WiFi (AP mode for AA) and Bluetooth

### Hardware Profile System
Platform-specific behavior (WiFi AP management, display enumeration, touch device detection, audio device selection, GPIO access) is abstracted behind a **hardware profile**. Each profile is a YAML file in `~/.openauto/hardware-profiles/`.

```yaml
# hardware-profiles/rpi4.yaml
id: rpi4
name: "Raspberry Pi 4"
wifi:
  ap_interface: wlan0
  ap_manager: hostapd        # hostapd | networkmanager
  ap_config: /etc/hostapd/hostapd.conf
audio:
  backend: pipewire           # pipewire | pulseaudio | alsa
  default_output: auto        # auto-detect or specific device
  default_input: auto
display:
  backend: wayland            # wayland | x11
  compositor: labwc
touch:
  detection: evdev-auto       # evdev-auto | evdev-manual | libinput
  device: auto                # auto-detect or /dev/input/eventN
bluetooth:
  adapter: auto               # auto or hci0, hci1, etc.
system:
  restart_command: "sudo systemctl restart openauto-prodigy"
  shutdown_command: "sudo shutdown -h now"
```

The app ships with `rpi4.yaml` as default. Community members contribute profiles for other boards (Pi 5, Orange Pi, etc.) by creating new YAML files — no code changes needed for pure configuration differences.

**Config vs. behavior distinction:** Hardware profiles are **pure configuration** — they select values and name strategies, but they do not contain logic. When profile values imply different behavior (e.g., `ap_manager: hostapd` vs `ap_manager: networkmanager`), the app dispatches to small **strategy classes** (`IWiFiStrategy`, `IAudioBackendStrategy`, etc.) that encapsulate the behavioral differences. This prevents scattered conditional logic throughout the codebase.

```
Profile YAML (config)          Strategy Classes (behavior)
┌─────────────────────┐        ┌──────────────────────────┐
│ ap_manager: hostapd  │───────→│ HostapdWiFiStrategy      │
│ ap_manager: nm       │───────→│ NetworkManagerWiFiStrategy│
│ backend: pipewire    │───────→│ PipeWireAudioStrategy    │
│ backend: alsa        │───────→│ AlsaAudioStrategy        │
└─────────────────────┘        └──────────────────────────┘
```

Adding support for a new platform that uses the same tools (hostapd, PipeWire, etc.) but with different config values = new YAML file only. Adding support for a fundamentally different tool (e.g., a new WiFi AP manager) = new strategy class + new YAML file.

---

## 3. Display & Resolution

### Adaptive Layout
The UI adapts to any resolution and aspect ratio. No hardcoded pixel values in layouts — everything uses proportional sizing, anchors, and responsive breakpoints.

**Primary test targets:**
- 1024x600 (DFRobot 7" HDMI) — current dev hardware
- 800x480 (official Pi 7" DSI) — most common community display
- 1280x800 (10" displays) — stretch goal

**Android Auto video** uses fixed protocol resolutions (480p, 720p, 1080p). The app selects the best fit for the physical display and renders with `PreserveAspectFit` (letterboxing as needed). Touch coordinates are mapped from physical display space through the letterbox region to AA video space.

### Layout Structure
Follows the standard car head unit pattern:

```
+------------------------------------------+
|  Status Bar (clock, BT/WiFi, connection) |  <- 5-8% height
+------------------------------------------+
|                                          |
|           Main Content Area              |  <- 75-80% height
|     (active plugin's UI goes here)       |
|                                          |
+------------------------------------------+
|  Nav Strip (plugin tiles/shortcuts)      |  <- 12-15% height
+------------------------------------------+
```

- **Status bar:** Always visible except during fullscreen AA. Shows clock, connection status, audio source indicator.
- **Main content area:** Hosts the active plugin's QML component. Launcher shows a dashboard view when no plugin is active.
- **Navigation strip:** Row of plugin tiles. Tapping switches the active plugin. Configurable order and visibility.

When Android Auto is active, the entire screen is used for video. Status bar and nav strip are hidden. A **3-finger tap gesture** reveals a translucent overlay with quick controls (volume, brightness, home button).

---

## 4. Plugin Architecture

### Design Philosophy
Every user-facing feature is a plugin. Core features (AA, BT Audio, Phone) are "static plugins" compiled into the binary. Third-party features are "dynamic plugins" loaded at runtime. Both types implement the same interface.

### Two Plugin Tiers

**Full Plugins (C++ .so + QML)**
For features requiring hardware access, protocols, or performance:
- Android Auto (wireless AA protocol, video decode, touch forwarding)
- Bluetooth Audio (A2DP sink, audio routing)
- Phone (HFP profile, dialer UI)
- Future: OBD-II, FM/DAB radio, USB camera

**QML-Only Plugins**
For UI extensions that don't need native code:
- Custom dashboard widgets (clock styles, weather, trip info)
- Theme extensions
- Layout customizations
- Future: community info panels, gauge displays

### Plugin Interface (C++)

```cpp
// Minimal, ABI-stable interface. Avoid complex Qt types at the boundary.
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Identity
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual int apiVersion() const = 0;

    // Lifecycle
    virtual bool initialize(IHostContext* context) = 0;
    virtual void shutdown() = 0;

    // UI
    virtual QUrl qmlComponent() const = 0;      // Main UI component
    virtual QUrl iconSource() const = 0;         // Nav strip icon
    virtual QUrl settingsComponent() const = 0;  // Settings panel (optional)

    // Capabilities
    virtual QStringList requiredServices() const = 0;  // e.g., ["AudioService", "BluetoothService"]
};
```

**v1 constraint: only the core provides services.** Plugins consume services through `IHostContext` but do not provide services to other plugins. This avoids dependency graphs, load-order complexity, and partial-availability states. If plugin-to-plugin communication is needed, use the `IEventBus`. Plugin-provided services may be introduced in a future version if a real need emerges.

### Host Service API (IHostContext)

Plugins access core functionality through a service registry, never through direct object access.

```cpp
class IHostContext {
public:
    virtual ~IHostContext() = default;

    // Service access
    virtual IAudioService* audioService() = 0;
    virtual IBluetoothService* bluetoothService() = 0;
    virtual IConfigService* configService() = 0;
    virtual IThemeService* themeService() = 0;
    virtual IDisplayService* displayService() = 0;
    virtual IEventBus* eventBus() = 0;

    // Plugin can register QML types for its UI
    virtual void registerQmlType(const char* uri, int major, int minor,
                                  const char* typeName, QObject* singleton) = 0;

    // Logging
    virtual void log(LogLevel level, const QString& message) = 0;
};
```

**Key services:**
| Service | Purpose |
|---------|---------|
| `IAudioService` | Play audio, set volume, register audio sources, priority routing |
| `IBluetoothService` | BT adapter access, pairing, profile management |
| `IConfigService` | Read/write plugin config (scoped to plugin's namespace) |
| `IThemeService` | Access current theme colors, fonts, icon paths |
| `IDisplayService` | Screen dimensions, orientation, brightness control |
| `IEventBus` | Publish/subscribe for cross-plugin events (AA connected, phone ringing, etc.) |

### Plugin Manifest

Every plugin has a `plugin.yaml` in its directory:

```yaml
id: org.openauto.android-auto
name: "Android Auto"
version: "1.0.0"
api_version: 1
type: full                    # full | qml-only
description: "Wireless Android Auto"
author: "OpenAuto Prodigy"
icon: icons/android-auto.svg

requires:
  services:
    - AudioService >= 1.0
    - BluetoothService >= 1.0
  system:
    - hostapd
    - dnsmasq

settings:
  - key: auto_connect
    type: bool
    default: true
    label: "Auto-connect on boot"
  - key: video_fps
    type: enum
    values: [30, 60]
    default: 60
    label: "Video framerate"
  - key: video_resolution
    type: enum
    values: [480p, 720p, 1080p]
    default: 720p
    label: "Video resolution"

nav_strip:
  order: 1                    # Position in nav strip (lower = further left)
  visible: true
```

### Plugin Lifecycle

**v1 constraint: plugins load at startup and unload at shutdown only.** No hot-loading or runtime unloading. This keeps lifecycle management simple and avoids service lifetime, thread cleanup, and QML teardown complexity.

```
Discovered    → manifest found in plugins directory
Validated     → manifest parsed, API version compatible, required services available
Loaded        → .so loaded (full plugins) or QML component located (QML-only)
Initialized   → initialize(IHostContext*) called, plugin sets up internal state
Running       → plugin is active and responsive
Shutdown      → shutdown() called at app exit, resources released
```

### Failure Model

Plugins must not take down the core application:

- **`initialize()` returns false:** Plugin is logged as failed and disabled for this session. Core continues running. The nav strip slot shows an error indicator or is hidden.
- **Plugin crashes at runtime:** Core catches what it can (signal handlers for segfaults in plugin code are best-effort). The plugin is marked as failed, its UI is removed, and core continues. A watchdog systemd service restarts the entire app if the main process dies.
- **Invalid QML:** QML loader errors are caught. The plugin's UI slot shows a "plugin error" placeholder. Core continues.
- **`initialize()` blocks too long:** Plugins must not block `initialize()`. Long-running setup (e.g., hardware enumeration, network connections) must happen on a background thread. The plugin manager may enforce a timeout in the future but v1 relies on plugin developer discipline.

### Threading Rules for Plugins

- UI work (QML property updates, signal emissions to QML) must occur on the Qt main thread.
- `initialize()` and `shutdown()` are called on the main thread — do not block them.
- Plugins are responsible for their own background threads for long-running work.
- Use `QMetaObject::invokeMethod(Qt::QueuedConnection)` to safely cross from worker threads to the main thread.

### Android Auto Authority Model

When the Android Auto plugin is active, it is the **dominant plugin**:

- AA owns the full screen (status bar and nav strip hidden).
- AA owns all touch input (forwarded to phone via AA protocol).
- AA holds top audio priority for its active channels.
- Other plugins are backgrounded — they continue running but do not control display or input.
- The only escape hatch is the 3-finger tap overlay, which is handled at the evdev layer before touches reach AA.

This pattern may apply to future fullscreen plugins (e.g., a video player), but for v1 only AA uses it.

### Plugin Directory Structure

```
~/.openauto/plugins/
├── org.openauto.android-auto/
│   ├── plugin.yaml
│   ├── libandroid-auto.so      # C++ backend (full plugins only)
│   ├── qml/
│   │   ├── AndroidAutoView.qml
│   │   └── AndroidAutoSettings.qml
│   └── icons/
│       └── android-auto.svg
├── org.openauto.bt-audio/
│   ├── plugin.yaml
│   ├── libbt-audio.so
│   └── qml/
│       └── BtAudioView.qml
├── com.community.clock-widget/     # QML-only plugin example
│   ├── plugin.yaml
│   └── qml/
│       └── FancyClock.qml
```

Core (static) plugins ship in the app's resource bundle. They use the same manifest format but are compiled into the binary via `Q_IMPORT_PLUGIN`.

### Plugin SDK & Distribution

For third-party plugin development:
- **Plugin SDK:** Header-only package containing `IPlugin`, `IHostContext`, and all service interfaces. Versioned and published alongside releases.
- **Build environment:** Provide a Docker image matching the target platform (RPi OS Trixie, Qt 6.8, ARM64 cross-compile or native) so plugin developers have a reproducible build.
- **Distribution:** Plugins are distributed as tarballs or git repos. The install script or a future plugin manager handles installation to `~/.openauto/plugins/`.

---

## 5. Theme System

### Theme Structure

A theme is a directory containing colors, fonts, and optional icon overrides:

```
~/.openauto/themes/
├── default/
│   ├── theme.yaml
│   ├── colors.yaml
│   ├── fonts/
│   │   └── (optional custom fonts)
│   └── icons/
│       └── (optional icon overrides)
├── dark-carbon/
│   ├── theme.yaml
│   ├── colors.yaml
│   └── icons/
│       └── ...
```

### Theme Manifest

```yaml
# theme.yaml
id: dark-carbon
name: "Dark Carbon"
author: "CommunityUser"
version: "1.0.0"
description: "Dark theme with carbon fiber textures"
parent: default              # Inherit from default, override specifics

colors: colors.yaml          # Color definitions file
font_family: "Inter"         # Primary font (falls back to system default)
font_family_mono: "JetBrains Mono"
icon_style: filled           # filled | outlined | rounded
```

### Color Definitions

```yaml
# colors.yaml
background:
  primary: "#1a1a1a"
  secondary: "#2d2d2d"
  surface: "#333333"
accent:
  primary: "#00b4d8"
  secondary: "#0077b6"
text:
  primary: "#ffffff"
  secondary: "#b0b0b0"
  disabled: "#666666"
status:
  success: "#4caf50"
  warning: "#ff9800"
  error: "#f44336"
  info: "#2196f3"
nav_strip:
  background: "#111111"
  active: "#00b4d8"
  inactive: "#666666"
```

The `IThemeService` exposes these values to plugins as QML properties. Plugins bind to theme properties (not hardcoded colors), so theme changes propagate automatically.

---

## 6. Audio Architecture

### PipeWire-Based Audio Stack

PipeWire is the audio backend (default on RPi OS Trixie). All audio sources are PipeWire nodes with priority-based routing.

```
Audio Sources (PipeWire nodes)
├── AA Media Audio        (priority: 50)
├── AA Navigation Audio   (priority: 80, ducks media)
├── AA Speech/Phone Audio (priority: 90, mutes media)
├── BT A2DP Stream        (priority: 40)
├── Local Media Player    (priority: 30)
├── System Sounds         (priority: 20)
└── Future: FM Radio      (priority: 35)
```

**Routing rules:**
- Higher priority sources duck or mute lower priority sources
- Navigation prompts temporarily reduce media volume (ducking)
- Phone calls mute all other audio
- When AA disconnects, BT A2DP or local media resumes

### Audio Ownership: PipeWire Does the Heavy Lifting

**PipeWire owns all mixing, routing, and hardware output.** The `IAudioService` is a thin helper layer that assists plugins in creating and managing PipeWire streams — it does NOT implement a custom mixer or audio engine.

```cpp
class IAudioService {
public:
    // Stream lifecycle — creates a PipeWire stream node for the plugin
    virtual PwStreamHandle createStream(const QString& name, int priority,
                                         const AudioFormat& format) = 0;
    virtual void destroyStream(PwStreamHandle handle) = 0;

    // Volume control (applies to the PipeWire sink)
    virtual void setMasterVolume(float level) = 0;  // 0.0 - 1.0
    virtual float masterVolume() const = 0;
    virtual void setMuted(bool muted) = 0;

    // Ducking policy — tells PipeWire to duck lower-priority streams
    virtual void requestAudioFocus(PwStreamHandle handle, FocusType type) = 0;
    virtual void releaseAudioFocus(PwStreamHandle handle) = 0;
};
```

**How it works:** Plugins call `createStream()` to get a PipeWire stream handle, then write PCM data directly to the PipeWire stream. PipeWire handles mixing multiple streams to the hardware output. The `requestAudioFocus()` / `releaseAudioFocus()` methods configure PipeWire's ducking and priority behavior (e.g., navigation prompts duck media audio).

---

## 7. Configuration System

### YAML-Based Configuration

All configuration uses YAML. The current INI system will be migrated.

```
~/.openauto/
├── config.yaml              # Main app config
├── hardware-profiles/
│   └── rpi4.yaml            # Active hardware profile
├── plugins/                 # Plugin directories
├── themes/                  # Theme directories
└── logs/                    # Runtime logs
```

### Main Config

```yaml
# config.yaml
hardware_profile: rpi4

display:
  brightness: 80             # 0-100
  theme: default
  orientation: landscape     # landscape | portrait

connection:
  auto_connect_aa: true
  bt_discoverable: true
  wifi_ap:
    ssid: "OpenAutoProdigy"
    password: "changeme123"
    channel: 36              # 5GHz channel
    band: a                  # a (5GHz) | g (2.4GHz)

audio:
  master_volume: 80          # 0-100
  output_device: auto

nav_strip:
  order:                     # Plugin display order
    - org.openauto.android-auto
    - org.openauto.bt-audio
    - org.openauto.phone
  show_labels: true

plugins:
  enabled:
    - org.openauto.android-auto
    - org.openauto.bt-audio
    - org.openauto.phone
  disabled: []
```

### Plugin-Scoped Config

Each plugin's settings are stored in its own section, keyed by plugin ID. The `IConfigService` scopes read/write access so plugins can only modify their own config.

### Configuration Ownership

**The Qt main app is the single writer of all config files.** No other process (web config server, install script post-setup) writes to `config.yaml` or plugin configs directly. The web config server sends structured change requests to the main app via IPC (Unix socket), and the main app validates and writes. This prevents partial writes, corruption, and race conditions.

---

## 8. Connection Lifecycle & UX

### Boot Sequence

```
Power On
  → Load config.yaml + hardware profile
  → Initialize core services (audio, BT, display, theme)
  → Discover and load enabled plugins
  → Start Bluetooth (A2DP sink + HFP)
  → Show launcher (dashboard view)
  → If auto_connect_aa: start BT advertising for AA discovery
  → Phone connects via BT → WiFi credential exchange → TCP 5288
  → AA plugin activates → fullscreen video
```

### Operating Modes

**Standalone Mode (no AA)**
- Head unit acts as BT audio receiver (A2DP) and phone (HFP)
- Launcher shows dashboard with active plugin tiles
- User can browse plugins, adjust settings

**Android Auto Mode**
- Fullscreen video, all touch forwarded to phone
- Status bar and nav strip hidden
- 3-finger tap reveals overlay controls
- AA disconnect returns to standalone mode

**Configuration Mode**
- Triggered from settings or overlay
- Web config panel accessible at `http://10.0.0.1:8080`
- Full theme editor, plugin management, hardware profile editing
- QR code displayed on screen for easy phone access

### 3-Finger Tap Overlay

When AA is active, a simultaneous 3-finger tap on the touchscreen triggers a translucent overlay:

```
+------------------------------------------+
|                                          |
|              AA Video                    |
|                                          |
|     +----------------------------+       |
|     |  [Vol-] ████████ [Vol+]    |       |
|     |  [Bright-] ██████ [Bright+]|       |
|     |                            |       |
|     |  [Home]  [Settings]  [X]   |       |
|     +----------------------------+       |
|                                          |
+------------------------------------------+
```

- **Implementation:** EvdevTouchReader detects 3 simultaneous touch-down events within a time window (~200ms). These touches are NOT forwarded to AA.
- **Dismiss:** Tap the X button, or tap outside the overlay, or timeout after 5 seconds of inactivity.
- **Controls:** Volume, brightness, return to home (disconnects AA), open settings, dismiss.

---

## 9. Settings Architecture

### Two-Tier Settings

**On-Screen Quick Settings (touchscreen)**
- Accessible from nav strip or 3-finger overlay
- Large touch targets for in-car use
- Controls: volume, brightness, theme toggle (light/dark), audio source, AA connect/disconnect
- Auto-generated from plugin `settings` declarations (simple types only: bool, enum, slider)

**Web Configuration Panel**
- Runs on the Pi, accessible at `http://10.0.0.1:8080` (over the WiFi AP)
- Full settings editor for all config, themes, plugins, hardware profile
- Built with lightweight web framework (likely Python Flask or Go) — separate process from the Qt app
- Sends change requests to the main app via Unix socket IPC — the main app is the single writer of all config files (see Section 7)
- QR code displayed on the touchscreen for easy phone access

### Auto-Generated Plugin Settings

Plugins declare their settings schema in `plugin.yaml`. The settings UI auto-generates controls:

| YAML Type | On-Screen Control | Web Control |
|-----------|-------------------|-------------|
| `bool` | Toggle switch | Checkbox |
| `enum` | Segmented buttons | Dropdown |
| `int` / `float` + range | Slider | Slider + text input |
| `string` | (web only) | Text input |
| `color` | (web only) | Color picker |

---

## 10. Installation

### Interactive Install Script

A single script that runs on stock RPi OS Trixie:

```bash
curl -sSL https://raw.githubusercontent.com/mrmees/openauto-prodigy/main/install.sh | bash
```

**Script flow:**
1. Check system requirements (RPi OS version, architecture, available storage)
2. Install system dependencies (Qt 6, Boost, FFmpeg, PipeWire, hostapd, dnsmasq)
3. **Interactive hardware setup:**
   - Select hardware profile (Pi 4, Pi 5, or custom)
   - Detect connected displays, confirm resolution
   - Detect touch devices, confirm mapping
   - Configure WiFi AP (SSID, password, channel)
   - Configure audio output device
4. Clone and build from source (or download pre-built ARM64 binary for tagged releases)
5. Install systemd service for autostart
6. Configure labwc compositor (if Wayland)
7. Create initial `config.yaml` from user's selections
8. Write a setup completion summary

### Directory Layout (Installed)

```
/opt/openauto-prodigy/          # Application binary + core plugin .so files
~/.openauto/
├── config.yaml
├── hardware-profiles/
│   └── rpi4.yaml
├── plugins/                    # Third-party plugins
├── themes/
│   └── default/
└── logs/
```

---

## 11. v1.0 Feature Scope

### Ships With

| Feature | Type | Description |
|---------|------|-------------|
| Android Auto | Static plugin | Wireless AA (video, audio, touch, mic) |
| Bluetooth Audio | Static plugin | A2DP sink — standalone music receiver |
| Phone | Static plugin | HFP — make/receive calls via BT |
| Launcher | Core | Dashboard home screen with plugin tiles |
| Quick Settings | Core | On-screen volume, brightness, theme toggle |
| Web Config Panel | Core | Full settings via browser |
| Theme System | Core | Swappable theme directories |
| Plugin SDK | Dev tooling | Headers + Docker build environment |
| Install Script | Dev tooling | Interactive setup on RPi OS Trixie |

### Deferred to Post-v1.0

| Feature | Notes |
|---------|-------|
| OBD-II / CAN bus | Dynamic plugin, needs hardware (ELM327 or CAN hat) |
| FM/DAB Radio | Dynamic plugin, needs USB dongle |
| Local media player | Dynamic plugin |
| USB camera / dashcam | Dynamic plugin |
| Plugin marketplace / manager | Nice-to-have, not essential for v1.0 |
| Pre-built SD card image | Install script first, image later |

---

## 12. Architecture Summary

```
┌─────────────────────────────────────────────┐
│                  QML UI Layer                │
│  ┌─────────┐ ┌──────────┐ ┌──────────────┐  │
│  │Launcher │ │ Plugin   │ │  Settings    │  │
│  │Dashboard│ │ Views    │ │  (Quick +    │  │
│  │         │ │ (dynamic)│ │   Web Panel) │  │
│  └─────────┘ └──────────┘ └──────────────┘  │
├─────────────────────────────────────────────┤
│              Plugin Manager                  │
│  ┌──────────────────────────────────────┐   │
│  │  Discovery → Validate → Load → Init  │   │
│  │  Manifest parsing, lifecycle mgmt    │   │
│  └──────────────────────────────────────┘   │
├─────────────────────────────────────────────┤
│           Host Service Layer                 │
│  ┌────────┐ ┌─────┐ ┌──────┐ ┌──────────┐  │
│  │ Audio  │ │ BT  │ │Theme │ │  Config  │  │
│  │Service │ │Svc  │ │ Svc  │ │  Service │  │
│  └────────┘ └─────┘ └──────┘ └──────────┘  │
│  ┌─────────┐ ┌──────────┐                   │
│  │Display  │ │ EventBus │                   │
│  │Service  │ │          │                   │
│  └─────────┘ └──────────┘                   │
├─────────────────────────────────────────────┤
│          Platform Abstraction                │
│  ┌──────────────────────────────────────┐   │
│  │  Hardware Profile (YAML-driven)      │   │
│  │  WiFi AP, Audio Backend, Display,    │   │
│  │  Touch Input, Bluetooth Adapter      │   │
│  └──────────────────────────────────────┘   │
├─────────────────────────────────────────────┤
│              System Layer                    │
│  PipeWire │ hostapd │ BlueZ │ evdev │ labwc │
└─────────────────────────────────────────────┘
```

### Threading Model

```
Main Thread (Qt Event Loop)
├── QML UI rendering
├── Plugin QML components
├── Quick settings interaction
└── Theme/config changes

ASIO Thread Pool (4 threads)
├── AA protocol message dispatch
├── AA service channels (10 channels)
└── TCP transport

Video Decode Thread (QThread)
├── FFmpeg H.264 decode
└── QVideoFrame creation → main thread via QueuedConnection

Evdev Touch Thread (QThread)
├── Raw touch input reading
├── Gesture detection (3-finger tap)
├── Coordinate mapping
└── AA touch forwarding via ASIO strand

PipeWire Thread
├── Audio mixing
├── Source priority routing
└── Hardware output

Web Config Server (separate process)
├── HTTP server
└── IPC to main app (Unix socket)
```

---

## 13. Migration Path from Current Codebase

The current codebase has working AA, video, touch, and BT discovery. The migration to this architecture involves:

1. **Extract service interfaces** — Define `IAudioService`, `IBluetoothService`, etc. from existing concrete implementations
2. **Define `IPlugin` + `IHostContext`** — Create the plugin boundary
3. **Wrap existing AA code as a static plugin** — Move `AndroidAutoService`, `VideoService`, `VideoDecoder`, `EvdevTouchReader`, `TouchHandler` into an AA plugin that implements `IPlugin`
4. **Migrate config from INI to YAML** — Replace `QSettings` with a YAML parser (yaml-cpp)
5. **Implement plugin discovery and loading** — `PluginManager` class that scans directories, validates manifests, manages lifecycle
6. **Refactor QML** — Split monolithic `main.qml` into shell (status bar, nav strip, content area) + plugin views
7. **Implement theme system** — `ThemeService` that loads theme YAML and exposes properties to QML
8. **Add audio pipeline** — `AudioService` wrapping PipeWire, with source registration and priority routing
9. **Add BT Audio plugin** — A2DP sink using BlueZ/PipeWire
10. **Add Phone plugin** — HFP profile using BlueZ + ofono or custom AT commands
11. **Build web config panel** — Lightweight HTTP server with settings UI
12. **Create install script** — Interactive setup for fresh RPi OS installs

This is a significant refactor but the existing working code (AA protocol, video decode, touch input) stays mostly intact — it just gets wrapped in the plugin interface and wired through services instead of direct object access.

---

## 14. Plugin API Versioning Policy

**Pre-1.0:** The plugin API may change at any time. Plugin developers should expect breakage and rebuilds. Changes will be communicated in release notes.

**Post-1.0:** Plugin API breaks only on major version bumps (2.0, 3.0, etc.). Minor and patch releases maintain backward compatibility. The `api_version` field in plugin manifests is checked at load time — plugins built against a newer API version than the host supports will be rejected with a clear error message.

This is a hobby project. We're not making enterprise stability guarantees, but we respect contributors' time enough to not break things silently.

---

## 15. Open Questions

These should be resolved during implementation planning:

1. **YAML parser choice:** yaml-cpp is the standard C++ option. Confirm it's available on Trixie and integrates with CMake cleanly.
2. **Web config framework:** Flask (Python) vs Go vs Node. Needs to be lightweight, run on Pi, and communicate with the Qt app.
3. **BT HFP implementation:** ofono vs custom AT command handling. ofono is complex but feature-complete; custom is simpler but limited.
4. **Plugin SDK packaging:** How to distribute headers and build environment. Docker image? Debian package? Both?
5. **3-finger gesture tuning:** Exact timing window, distance threshold, and how to handle partial detections without interfering with AA touch.
