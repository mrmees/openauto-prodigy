# Technology Stack — v1.0 Feature Additions

**Project:** OpenAuto Prodigy
**Researched:** 2026-03-01
**Scope:** New libraries/APIs needed for HFP call audio, audio EQ, dynamic AA video reconfig, theme selection, and logging cleanup. Does NOT re-document the existing stack (Qt 6.8, PipeWire, BlueZ, FFmpeg, etc.).

## Recommended Additions

### 1. HFP Call Audio — No New Dependencies

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| PipeWire bluez5 plugin (existing) | 1.4.2 (Pi) | HFP AG audio routing | Already handles SCO audio. PipeWire's `bluez5.roles` includes `hfp_ag` by default on Trixie. |
| WirePlumber (existing) | 0.5.x (Pi) | BT profile autoswitch | Handles A2DP <-> HFP profile transitions automatically. |
| BlueZ D-Bus (existing) | 5.x | Call state monitoring | PhonePlugin already monitors `org.bluez.Device1` for HFP UUIDs. |

**Approach:** The Pi's PipeWire + WirePlumber stack already manages HFP AG audio. PipeWire's bluez5 SPA plugin registers as HFP AG, creates SCO audio nodes, and WirePlumber routes them to the default sink/source. The app does NOT need to handle SCO sockets directly.

**What needs to change (code, not deps):**
- PhonePlugin must decouple from AA session lifecycle. Currently the phone plugin monitors devices but doesn't manage audio persistence across AA connect/disconnect cycles.
- Watch `org.freedesktop.DBus.Properties` on the BlueZ `MediaTransport1` interface for SCO transport state transitions (idle/pending/active).
- When AA disconnects mid-call, HFP audio should already be flowing through PipeWire — the key is ensuring the app doesn't tear down BT connections on AA disconnect.

**Confidence:** HIGH — PipeWire's HFP AG is the default on RPi OS Trixie. Confirmed from WirePlumber 0.5.x docs and project memory (HSP HS registered by C++; HFP AG owned by PipeWire's bluez5 plugin).

### 2. Audio Equalizer — PipeWire filter-chain Module

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| PipeWire filter-chain module | 1.4.2 (Pi) | Parametric EQ DSP | Built into PipeWire. No external library needed. Supports biquad filters (peaking, low shelf, high shelf) with configurable frequency/gain/Q. |

**Approach: filter-chain via config files, NOT pw_filter C API.**

Two viable approaches exist. Use the PipeWire filter-chain module (configuration-driven) rather than the `pw_filter` C API (code-driven):

| Approach | Pros | Cons |
|----------|------|------|
| **filter-chain module** (recommended) | Zero new C++ code for DSP. Config-file driven. PipeWire handles the RT processing. Hot-reloadable by restarting the module. Standard PipeWire pattern used by EasyEffects, AutoEQ, etc. | Must spawn/manage a PipeWire module load. Less fine-grained control. |
| `pw_filter` C API | Full control. In-process. Can react to UI changes instantly. | Must implement biquad DSP in RT-safe C++. Must manage float conversion. Significant new code on the RT audio path — high risk of bugs. |

**Implementation pattern:**
1. Store EQ profiles as YAML in `~/.openauto/eq/` (preset name, band count, per-band freq/gain/Q/type).
2. On profile selection, generate a PipeWire filter-chain config fragment.
3. Load it via `pw_context_load_module("libpipewire-module-filter-chain", ...)` or by writing a `.conf` file to `~/.config/pipewire/pipewire.conf.d/` and signaling PipeWire to reload.
4. The filter-chain inserts between the app's playback streams and the output sink.

**EQ filter types needed:**
- `bq_peaking` — parametric bands (the workhorse)
- `bq_lowshelf` — bass shelf
- `bq_highshelf` — treble shelf
- All are builtins in PipeWire's filter-chain module.

**Presets to ship:**
- Flat (bypass), Bass Boost, Treble Boost, Vocal, Rock, Custom (user-defined bands)

**Confidence:** HIGH — PipeWire filter-chain with biquad builtins is well-documented and the standard approach for EQ on PipeWire systems. Works on Pi's PipeWire 1.4.2.

### 3. Dynamic AA Video Reconfiguration — open-androidauto Protos

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| `UiConfigMessages.proto` (existing) | In open-androidauto | Runtime margin/theme push | Already in the proto submodule. Defines `UpdateHuUiConfigRequest` (0x8012) for HU-initiated UI config changes. |
| Protobuf (existing) | 3.x | Serialization | Already a build dependency. |

**Approach:** Send `UpdateHuUiConfigRequest` on the AV channel when sidebar config changes, rather than requiring an AA session restart.

**What needs to change (code, not deps):**
- Wire `UiConfigMessages.pb.h` into the AV channel message handler (register message ID 0x8012 for sending, 0x8013 for receiving the response).
- When sidebar position/size changes in the UI: recalculate margins using the existing `calcMargins` logic from `VideoService`, build an `UpdateHuUiConfigRequest` with the new `Insets`, and send it over the AV channel.
- Handle `UpdateHuUiConfigResponse` (0x8013) to confirm the phone accepted the change.
- Update `EvdevTouchReader` hit zones and touch coordinate mapping to reflect the new layout.

**Risk:** This is unverified on the wire. The proto exists (sourced from aa-proxy-rs) but no one has confirmed the phone actually honors runtime margin changes via 0x8012. May need to fall back to a session restart if the phone ignores or NACKs the message.

**Confidence:** MEDIUM — Proto structure is known, but runtime behavior is unverified. Flag for wire testing before committing to this approach. Fallback (session restart) is always available.

### 4. Theme Selection — No New Dependencies

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| yaml-cpp (existing) | 0.8.x | Theme YAML parsing | ThemeService already loads `theme.yaml` via yaml-cpp. |
| Qt Quick (existing) | 6.8 | Wallpaper rendering | QML `Image` element for wallpaper display. Already have `Wallpaper.qml` (currently a solid color rect). |

**Approach:** Extend the existing ThemeService, don't replace it.

**Theme directory structure:**
```
~/.openauto/themes/
  default/
    theme.yaml        # Color palette (day/night) — existing format
    wallpapers/       # NEW: wallpaper images
      day.jpg
      night.jpg
  midnight-blue/
    theme.yaml
    wallpapers/
      day.jpg
      night.jpg
```

**What needs to change (code, not deps):**
- `ThemeService::setTheme(id)` — currently a stub returning false. Implement directory scanning of `~/.openauto/themes/` and `config/themes/`.
- Add `wallpaperPath` Q_PROPERTY to ThemeService (day/night aware).
- `Wallpaper.qml` — replace solid `Rectangle` with `Image` that sources from `ThemeService.wallpaperPath`, falling back to solid color if no image.
- Theme picker UI in Settings — `ListView` of discovered themes with preview colors.
- Persist selected theme ID in YAML config (`ui.theme`).

**Wallpaper format:** JPEG at 1024x600 (target display resolution). No SVG, no dynamic wallpapers. Keep it dead simple. Ship 2-3 bundled themes; users can add their own by dropping directories into `~/.openauto/themes/`.

**Confidence:** HIGH — Extends existing code with no new dependencies. ThemeService already has the loading infrastructure; it just needs directory scanning and wallpaper path support.

### 5. Logging Cleanup — QLoggingCategory (Qt Built-in)

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| QLoggingCategory (existing) | Qt 6.4+ | Categorized log filtering | Built into Qt. Zero new deps. Industry-standard Qt logging pattern. |

**Approach:** Replace raw `qDebug()`/`qInfo()`/`fprintf(stderr)` calls with categorized logging using `Q_LOGGING_CATEGORY` and `qCDebug()`/`qCInfo()`.

**Category structure:**
```cpp
Q_LOGGING_CATEGORY(lcAA,        "oap.aa")         // AA protocol
Q_LOGGING_CATEGORY(lcAAVideo,   "oap.aa.video")    // Video decode
Q_LOGGING_CATEGORY(lcAATouch,   "oap.aa.touch")    // Touch input
Q_LOGGING_CATEGORY(lcAudio,     "oap.audio")       // PipeWire audio
Q_LOGGING_CATEGORY(lcBluetooth, "oap.bluetooth")   // BlueZ/BT
Q_LOGGING_CATEGORY(lcPlugin,    "oap.plugin")      // Plugin lifecycle
Q_LOGGING_CATEGORY(lcUI,        "oap.ui")          // QML/UI
Q_LOGGING_CATEGORY(lcConfig,    "oap.config")      // Config loading
```

**Default log rules (quiet production):**
```
oap.*.debug=false
oap.aa.video.info=false
oap.aa.touch.info=false
```

**Debug mode activation:**
- `--verbose` CLI flag: sets `QLoggingCategory::setFilterRules("oap.*=true")`
- Config option: `logging.verbose: true` in YAML
- Runtime toggle via web config panel

**Migration scope:** ~230 log calls across 23 files (counted above). Mechanical replacement — no logic changes.

**Confidence:** HIGH — QLoggingCategory is the standard Qt pattern. Available in Qt 6.4+ (both dev VM and Pi). Well-documented with runtime filter support.

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| EQ DSP | PipeWire filter-chain module | `pw_filter` C API | Writing RT-safe biquad DSP code is unnecessary risk when PipeWire provides it as a built-in module. The filter-chain approach is what EasyEffects and the broader PipeWire ecosystem use. |
| EQ DSP | PipeWire filter-chain module | EasyEffects / JDSP4Linux | Heavy desktop apps with GTK4 dependencies. Overkill for a head unit. We want embedded EQ, not a desktop audio suite. |
| HFP audio | PipeWire bluez5 (native) | ofono | Ofono is unreliable on Trixie (existing project decision). PipeWire's native HFP backend is the default and works. |
| HFP audio | PipeWire bluez5 (native) | Direct SCO socket handling | Reinventing what PipeWire already does. The bluez5 SPA plugin handles SCO transport negotiation and audio routing. |
| Logging | QLoggingCategory | spdlog / Boost.Log | External dependency for a problem Qt solves natively. The app is already Qt-based; adding a second logging framework creates split-brain logging. |
| Logging | QLoggingCategory | Custom log levels via HostContext | Already exists for plugin logging, but doesn't help with core/AA code. QLoggingCategory is the Qt-native solution that covers everything. |
| Theme format | YAML + JPEG wallpapers | QSS (Qt Style Sheets) | QSS is for widgets, not QML. The app uses QML exclusively. |
| Theme format | YAML + JPEG wallpapers | SVG wallpapers | Unnecessary complexity for a fixed-resolution display. JPEG at 1024x600 is trivial to load and fast to render. |
| Dynamic sidebar | `UpdateHuUiConfigRequest` (0x8012) | Session restart | Session restart works but is disruptive (2-3 second reconnection). 0x8012 is the proper protocol mechanism if the phone supports it. |
| Dynamic sidebar | `UpdateHuUiConfigRequest` (0x8012) | Fake margin via QML crop | QML-only approach can't change what the phone renders — content layout would be wrong. Must tell the phone about the new margins. |

## What NOT to Use

| Technology | Why Not |
|------------|---------|
| ofono | Unreliable on Trixie. Already rejected as a project decision. |
| PulseAudio / pactl for EQ | PipeWire is the audio stack. Don't mix audio servers. |
| EasyEffects | Desktop GUI app, GTK4 dependency, massive overkill for preset-based EQ. |
| LADSPA/LV2 plugins for EQ | Filter-chain builtins (bq_peaking, bq_lowshelf, bq_highshelf) cover all needed EQ filter types without external plugin packages. |
| spdlog / glog / Boost.Log | Qt already has QLoggingCategory. Don't add a second logging framework to a Qt app. |
| QSS themes | Wrong technology — QSS is for QWidget, not QML. |
| SVG wallpapers | Over-engineered for a single fixed-resolution display. |

## No New Packages Required

Every feature can be implemented with libraries already in the build:

```bash
# Already installed — no changes to install.sh or CMakeLists.txt deps:
# Qt 6.8 (Core, Gui, Quick, QuickControls2, Multimedia, Network, DBus)
# PipeWire 1.4.2 (libpipewire-0.3)
# yaml-cpp
# Protobuf
# BlueZ + D-Bus
```

The only potential addition is ensuring `pipewire-module-filter-chain` is available on the Pi. On Trixie with PipeWire 1.4.2, this module ships with the base `pipewire` package — verify during implementation with:
```bash
ls /usr/lib/*/pipewire-0.3/libpipewire-module-filter-chain.so
```

## Sources

- [PipeWire Filter-Chain Module](https://docs.pipewire.org/page_module_filter_chain.html) — builtin biquad EQ filters, configuration format
- [PipeWire Parametric Equalizer](https://docs.pipewire.org/page_module_parametric_equalizer.html) — AutoEQ-compatible parametric EQ module
- [PipeWire pw_filter API](https://docs.pipewire.org/group__pw__filter.html) — C API for custom audio DSP (considered, not recommended)
- [PipeWire audio-dsp-filter.c example](https://github.com/PipeWire/pipewire/blob/master/src/examples/audio-dsp-filter.c) — reference implementation of pw_filter
- [WirePlumber 0.5.x Bluetooth Configuration](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/bluetooth.html) — bluez5.roles, HFP AG config, SCO routing
- [QLoggingCategory (Qt 6)](https://doc.qt.io/qt-6/qloggingcategory.html) — categorized logging with runtime filtering
- [Qt Logging Types (Qt 6)](https://doc.qt.io/qt-6/qtlogging.html) — qCDebug, qCInfo, message handler installation
- [UiConfigMessages.proto](../libs/open-androidauto/proto/oaa/av/UiConfigMessages.proto) — in-tree proto for runtime UI config push (0x8012/0x8013)
- [Writing a PipeWire parametric equalizer module](https://asymptotic.io/blog/pipewire-parametric-autoeq/) — practical guide to PipeWire EQ implementation

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| HFP call audio | HIGH | PipeWire's bluez5 HFP AG is default on Trixie. Confirmed by project memory. No new deps. |
| Audio EQ | HIGH | PipeWire filter-chain with biquad builtins is well-documented standard approach. Ships with PipeWire. |
| Dynamic sidebar | MEDIUM | Proto exists but 0x8012 runtime behavior is unverified on the wire. Needs testing. |
| Theme selection | HIGH | Pure extension of existing ThemeService + YAML + QML. No new deps. |
| Logging cleanup | HIGH | QLoggingCategory is built-in Qt. Mechanical migration of ~230 log calls. |
