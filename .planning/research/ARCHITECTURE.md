# Architecture Patterns

**Domain:** Android Auto head unit — v1.0 feature integration
**Researched:** 2026-03-01

## Existing Architecture Overview

The current system is a Qt 6 + C++17 application with a well-defined plugin/service architecture:

```
+-----------------------------------------------------------------+
|                          Qt QML Shell                           |
|  (Shell.qml, TopBar, BottomBar, NavStrip, plugin views)        |
+-----------------------------------------------------------------+
       |                    |                    |
       v                    v                    v
+---------------+  +----------------+  +------------------+
| PluginManager |  | ApplicationCtl |  | PluginModel      |
| (lifecycle)   |  | (navigation)   |  | (QML list model) |
+---------------+  +----------------+  +------------------+
       |
       v
+------------------------------------------------------------------+
|                      IHostContext                                  |
|  AudioService | ConfigService | ThemeService | EventBus           |
|  BluetoothSvc | DisplaySvc   | ActionRegistry | NotificationSvc  |
+------------------------------------------------------------------+
       |                    |                    |
       v                    v                    v
+---------------+  +----------------+  +------------------+
| AndroidAuto   |  | BtAudio Plugin |  | Phone Plugin     |
| Plugin        |  | (A2DP/AVRCP)   |  | (HFP dialer)    |
+---------------+  +----------------+  +------------------+
       |
       v
+------------------------------------------------------------------+
|                   AndroidAutoOrchestrator                          |
|  TCPTransport | AASession | Video/Audio/Input/Sensor handlers     |
|  VideoDecoder | TouchHandler | EvdevTouchReader                   |
|  ServiceDiscoveryBuilder (margins, codecs, touch config)          |
+------------------------------------------------------------------+
       |                    |                    |
       v                    v                    v
  PipeWire streams     FFmpeg decode        BlueZ D-Bus
  (ring buffer bridge) (hw/sw fallback)     (adapter, profiles)
```

### Key Boundaries

| Boundary | Interface | Thread Model |
|----------|-----------|--------------|
| Plugin <-> Services | `IHostContext` (vtable) | Qt main thread |
| AA Protocol <-> Qt | `QMetaObject::invokeMethod(QueuedConnection)` | ASIO threads -> Qt main |
| Audio write <-> PW playback | `AudioRingBuffer` (lock-free) | Any thread -> PW RT thread |
| Evdev touch <-> AA input | `EvdevTouchReader` (QThread) -> signals | Evdev thread -> Qt main |
| Config <-> Plugins | `ConfigService::configChanged` signal | Qt main thread |
| Inter-plugin events | `IEventBus` (topic/callback) | Any thread -> Qt main |

## Recommended Architecture for v1.0 Features

### Component Boundaries

| Component | Responsibility | Communicates With | New/Modified |
|-----------|---------------|-------------------|--------------|
| **EqualizerService** | Biquad DSP filter chain on PipeWire output | AudioService (stream insertion), ConfigService (presets), IPC (web panel) | **NEW** |
| **ThemeService** (extended) | Multi-theme directory scan, theme switching, wallpaper selection | ConfigService (active theme ID), QML (color properties), IPC (web panel) | **MODIFIED** |
| **PhonePlugin** (extended) | HFP audio routing independent of AA session | AudioService (capture/playback streams), BlueZ D-Bus, EventBus | **MODIFIED** |
| **AndroidAutoOrchestrator** (extended) | Dynamic margin push via `UpdateHuUiConfigRequest` | AASession (video channel message), EvdevTouchReader (recompute), ConfigService (sidebar config) | **MODIFIED** |
| **LoggingService** | Centralized log level control, runtime verbosity toggle | All components (qInstallMessageHandler), ConfigService (level setting), CLI args | **NEW** |

### Data Flow Diagrams

#### 1. Audio EQ Pipeline

```
Phone -> AA protocol -> AudioChannelHandler
                             |
                             v
                      AudioService.writeAudio()
                             |
                             v
                      AudioRingBuffer (lock-free)
                             |
                             v
                      PW playback callback (onPlaybackProcess)
                             |
                             v  [NEW: insert here]
                      EqualizerService.process()
                             |  (biquad cascade, in-place float DSP)
                             v
                      pw_stream output -> speakers

Config change (preset select / band adjust)
  -> ConfigService.configChanged signal
  -> EqualizerService.updateCoefficients()
  -> Atomic coefficient swap (lock-free, RT-safe)
```

**Architecture decision: In-process biquad DSP, not PipeWire filter-chain module.**

Rationale:
- The filter-chain module (`libpipewire-module-filter-chain`) is designed for static configuration loaded from files. It does not expose a clean API for runtime parameter updates (changing bands, switching presets) without tearing down and rebuilding the graph.
- `pw_filter` (PipeWire's DSP filter API) is an option but adds complexity: it creates a separate PW node that must be linked into the graph between the app's output stream and the sink. On a Pi, managing graph topology programmatically is fragile.
- **Best approach:** Apply biquad processing directly in `onPlaybackProcess()` before writing to the PW buffer. This is zero-overhead when EQ is bypassed (skip the processing block), requires no PW graph manipulation, and allows atomic coefficient updates from the UI thread. The AudioService already runs in the PW RT callback context with direct buffer access.

Implementation:
- `EqualizerService` holds an array of biquad filter states (one per band per channel).
- Coefficients are computed on the UI thread when presets change, then swapped atomically into the RT-accessible struct.
- `AudioService::onPlaybackProcess()` calls `EqualizerService::process(float* samples, int frameCount, int channels)` if EQ is enabled.
- Presets stored in YAML under `audio.eq.presets[]`, active preset in `audio.eq.active`.
- Web config panel provides the full EQ editor (band count, frequency, gain, Q, type); head unit QML provides preset selection and a simple band-level visualization.

**Confidence: HIGH** — biquad DSP is well-understood, PipeWire's RT callback model is proven in the existing code, and this avoids PW graph complexity entirely.

#### 2. HFP Audio Independence

```
Current (broken):
  PhonePlugin detects HFP call via BlueZ D-Bus
  AA session creates speech/media PW streams
  AA disconnects -> streams destroyed -> call audio lost

Target:
  PhonePlugin owns its OWN PW streams for HFP audio
  These persist independent of AA session lifecycle

BlueZ D-Bus
  |
  v
PhonePlugin.onPropertiesChanged()
  |
  +--[call state: Active]-->  AudioService.createStream("HFP-Playback")
  |                           AudioService.openCaptureStream("HFP-Capture")
  |                           AudioService.requestAudioFocus(GainTransient)
  |                             |
  |                             v
  |                           PipeWire: HFP SCO -> capture callback -> playback stream
  |
  +--[call state: Idle]--->   AudioService.releaseAudioFocus()
  |                           AudioService.destroyStream()
  |                           AudioService.closeCaptureStream()

AA connect/disconnect has NO effect on PhonePlugin streams.
```

**Key insight:** PipeWire on Trixie already acts as the HFP AG (via its bluez5 plugin). When an HFP call is active, PipeWire creates SCO audio nodes automatically. The PhonePlugin does NOT need to create its own PW streams for the actual SCO audio path — PipeWire handles that. What the PhonePlugin DOES need is:

1. **Audio focus management** — request `GainTransient` focus when a call starts so AA media gets ducked/muted.
2. **Lifecycle independence** — the PhonePlugin's D-Bus monitoring and call state tracking must not be coupled to the AA session. Currently it isn't (PhonePlugin.initialize() sets up D-Bus independently), but audio focus requests go through AudioService which creates AA-specific streams. The fix is to create a lightweight "focus-only" stream handle that participates in ducking without carrying actual audio data, OR to extend AudioFocus to support focus claims without a stream.
3. **Mic routing awareness** — during AA, the capture stream feeds the AA speech channel. During HFP (no AA), capture feeds PipeWire's SCO sink. This routing is automatic via PipeWire's session manager, but the app must not grab the capture device exclusively for AA when HFP is active.

**Confidence: MEDIUM** — PipeWire's automatic HFP SCO handling needs wire verification on Pi. The D-Bus call state tracking in PhonePlugin is solid, but the audio focus interaction between AA streams and HFP needs careful testing.

#### 3. Dynamic Sidebar / Video Reconfiguration

```
User changes sidebar setting (QML toggle / web panel)
  |
  v
ConfigService.configChanged("video.sidebar.*")
  |
  v
AndroidAutoPlugin.onConfigChanged()
  |
  +--[AA not connected]--> Store for next session (existing behavior)
  |
  +--[AA connected]--> [NEW PATH]
       |
       v
     AndroidAutoOrchestrator.pushUiConfigUpdate()
       |
       v
     Build UpdateHuUiConfigRequest protobuf:
       - Compute new Insets.margins from sidebar config
       - Set UiTheme (preserve current day/night)
       |
       v
     Send 0x8012 on Video AV channel
       |
       v
     Phone responds with 0x8013 (UpdateHuUiConfigResponse)
       |
       +--[success]--> EvdevTouchReader.recomputeLetterbox()
       |               QML sidebar layout updates via property binding
       |
       +--[failure/unsupported]--> Fall back to disconnect+reconnect
```

**Critical risk: `UpdateHuUiConfigRequest` (0x8012) is UNVERIFIED on the wire.** The proto definition exists (from aa-proxy-rs) and the message IDs are confirmed in the AA APK's enum table, but no one has confirmed that modern phones (AA 16.x) actually honor runtime margin pushes. The phone might:
- Accept and re-render (best case)
- Accept the message but ignore margin changes (only honor theme changes)
- Reject/ignore entirely

**Recommended approach:**
1. Build the `UpdateHuUiConfigRequest` sending code behind a feature flag.
2. Test with protocol capture enabled to see the phone's response.
3. If the phone rejects margin changes, fall back to the existing `disconnectAndRetrigger()` approach (which works but causes a 3-5 second reconnection delay).
4. Even if 0x8012 only works for theme/night-mode (not margins), that's still valuable — it would replace the current sensor-based night mode hack.

**Confidence: LOW for dynamic margins, MEDIUM for dynamic theme/night-mode via 0x8012.**

#### 4. Theme Selection System

```
~/.openauto/themes/
  |
  +-- default/
  |     +-- theme.yaml  (id, name, day colors, night colors, font)
  |
  +-- midnight/
  |     +-- theme.yaml
  |     +-- wallpaper.jpg  (optional)
  |
  +-- ocean/
        +-- theme.yaml
        +-- wallpaper-day.jpg
        +-- wallpaper-night.jpg

ThemeService (extended)
  |
  +-- scanThemeDirectories()  -> ThemeListModel (id, name, preview color)
  |
  +-- setTheme(themeId)
  |     -> loadTheme(dir)
  |     -> ConfigService.setValue("theme.active", themeId)
  |     -> emit colorsChanged()  (all QML bindings update instantly)
  |
  +-- wallpaperPath() -> QML Image source
        -> returns theme-specific wallpaper if present
        -> falls back to built-in default
```

**This is the simplest feature.** The ThemeService already has:
- YAML loading with day/night color maps
- `colorsChanged()` signal that triggers all QML property bindings
- `setNightMode()` for mode switching

What's missing:
- `setTheme()` currently returns false (stub). Needs directory scanning.
- No `ThemeListModel` for the settings UI dropdown.
- No wallpaper support (currently hardcoded in QML).
- No theme preview/thumbnail.

Architecture is straightforward: scan `~/.openauto/themes/`, build a model, `setTheme()` loads the selected dir, everything else flows through existing signals.

**Confidence: HIGH** — purely extending existing patterns, no new system boundaries.

#### 5. Logging Cleanup

```
main.cpp
  |
  +-- qInstallMessageHandler(LoggingService::handler)
  |
  v
LoggingService (singleton)
  |
  +-- setVerbosity(level)  // from CLI --verbose or config
  |     -> filters qDebug/qInfo/qWarning based on level
  |
  +-- Per-category filtering (optional)
  |     -> "BtManager" -> suppress unless verbose
  |     -> "AAOrchestrator" -> always show
  |
  +-- Runtime toggle via:
       -> Config: "system.log_level" (quiet/normal/verbose/debug)
       -> CLI: --verbose, --quiet
       -> IPC: web panel toggle (for remote debugging)
```

**Implementation:** Qt's built-in `QLoggingCategory` system is the right tool. Define categories (`oap.bt`, `oap.aa`, `oap.audio`, `oap.plugin`) and use `qCDebug()`/`qCInfo()` instead of `qDebug()`/`qInfo()`. Default rules suppress debug-level output. `--verbose` enables all categories at debug level.

**Confidence: HIGH** — Qt logging categories are mature and well-documented.

## Anti-Patterns to Avoid

### Anti-Pattern 1: EQ via PipeWire Graph Manipulation
**What:** Loading `libpipewire-module-filter-chain` programmatically and managing graph links to insert EQ between app output and hardware sink.
**Why bad:** PW graph topology is managed by WirePlumber (the session manager). Inserting filter nodes conflicts with WirePlumber's automatic routing. Teardown/rebuild on preset change causes audible glitches. Pi 4 has limited PW overhead budget.
**Instead:** In-process biquad DSP in the existing RT callback.

### Anti-Pattern 2: HFP Audio Managed by App
**What:** Creating PW capture/playback streams in PhonePlugin to manually bridge HFP SCO audio.
**Why bad:** PipeWire's bluez5 plugin already creates SCO nodes and routes them. Duplicating this creates echo, double-audio, and fights with the session manager.
**Instead:** Let PipeWire handle SCO routing. App manages focus/ducking and call UI state only.

### Anti-Pattern 3: Full Session Restart for Sidebar Toggle
**What:** Always using `disconnectAndRetrigger()` when sidebar config changes.
**Why bad:** 3-5 second reconnection delay, user-visible interruption, phone may not reconnect reliably.
**Instead:** Try `UpdateHuUiConfigRequest` first, fall back to reconnect only if the phone rejects it.

### Anti-Pattern 4: Logging via qDebug() Without Categories
**What:** Using bare `qDebug()` everywhere, then trying to filter with string matching in a custom handler.
**Why bad:** Fragile string matching, can't be configured per-module, no compile-time optimization.
**Instead:** `QLoggingCategory` with `qCDebug(category)` — Qt disables the string formatting at the call site when the category is disabled.

## Patterns to Follow

### Pattern 1: Atomic Coefficient Swap for RT-Safe EQ
**What:** Compute biquad coefficients on UI thread, swap into RT-accessible struct atomically.
**When:** Any time DSP parameters change from a non-RT context.
**Example:**
```cpp
struct EqCoefficients {
    struct Band { float b0, b1, b2, a1, a2; };
    std::array<Band, 10> bands;  // max 10 bands
    int activeBands = 0;
    bool bypass = true;
};

// Shared between UI thread and PW RT thread
std::atomic<EqCoefficients*> activeCoeffs_{nullptr};

// UI thread: compute and swap
void updatePreset(const EqPreset& preset) {
    auto* newCoeffs = new EqCoefficients();
    // ... compute biquad coefficients from preset ...
    auto* old = activeCoeffs_.exchange(newCoeffs, std::memory_order_acq_rel);
    // Schedule old for deletion on UI thread (not RT thread)
    QTimer::singleShot(100, [old]() { delete old; });
}

// PW RT thread: read atomically
void process(float* samples, int frames, int channels) {
    auto* coeffs = activeCoeffs_.load(std::memory_order_acquire);
    if (!coeffs || coeffs->bypass) return;
    // Apply biquad cascade...
}
```

### Pattern 2: Feature Flag for Unverified Protocol Messages
**What:** Gate new protocol features behind config flags so they can be disabled without code changes.
**When:** Any unverified AA protocol message (like 0x8012).
```cpp
bool dynamicUiConfig = yamlConfig_->valueByPath("video.dynamic_ui_config").toBool();
if (dynamicUiConfig) {
    // Try UpdateHuUiConfigRequest
} else {
    // Reconnect approach
}
```

### Pattern 3: Service Extension via IHostContext
**What:** New services (EqualizerService, LoggingService) should be accessible through `IHostContext` if plugins need them.
**When:** A service needs to be shared across plugins or exposed to IPC.
**Note:** Not every new component needs to be a service. The EQ and logging are core infrastructure — they should be services. Theme selection is already a service. HFP audio independence is internal to PhonePlugin — no new service needed.

## Suggested Build Order

Based on dependency analysis:

```
Phase 1: Logging Cleanup
  - No dependencies on other features
  - Makes debugging ALL subsequent work easier
  - Quick win: define categories, replace qDebug calls, add --verbose flag

Phase 2: HFP Audio Independence
  - Depends on: understanding PipeWire's automatic HFP SCO routing (verify on Pi)
  - Blocked by: nothing (PhonePlugin already has D-Bus monitoring)
  - Risk: PipeWire SCO behavior may need investigation
  - Must complete before "release quality" — calls dropping is a showstopper

Phase 3: Theme Selection
  - Depends on: nothing (ThemeService infrastructure exists)
  - Simple extension of existing patterns
  - User-visible polish that builds confidence in the release

Phase 4: Audio Equalizer
  - Depends on: AudioService internals (well understood)
  - More complex: biquad math, RT-safe coefficient swap, two UIs (QML + web)
  - Can be built incrementally: core DSP -> presets -> QML selector -> web editor

Phase 5: Dynamic Sidebar
  - Depends on: protocol verification (0x8012 on wire)
  - Highest risk: may not work, requires fallback path
  - Should be attempted after core features are solid
  - Even partial success (dynamic night mode) is valuable
```

**Dependency graph:**
```
Logging -----> (all phases benefit)
               |
HFP Audio ---> (independent, high priority)
               |
Theme -------> (independent, medium priority)
               |
EQ ----------> (independent, medium complexity)
               |
Dynamic Sidebar -> (highest risk, do last)
```

No feature depends on another feature. The ordering is purely by risk/impact ratio: low-risk high-impact first, high-risk last.

## Scalability Considerations

Not applicable in the traditional sense (this is a single-user embedded device), but relevant concerns:

| Concern | Current State | v1.0 Target |
|---------|---------------|-------------|
| Audio latency budget | ~20ms end-to-end | EQ adds <1ms (biquad cascade is trivial on ARM) |
| CPU headroom | ~25% during AA | EQ adds ~1-2% (10-band biquad on 48kHz stereo) |
| Memory | ~80MB RSS | Theme images add ~2-5MB, EQ state is negligible |
| Config complexity | ~30 YAML keys | ~50 keys (EQ presets, theme ID, log level) |
| Startup time | ~2s | Theme scan adds ~50ms, logging init is negligible |

## Sources

- [PipeWire Filter-Chain Module](https://docs.pipewire.org/page_module_filter_chain.html) — biquad types, filter-chain architecture
- [PipeWire Tutorial 7: Audio DSP Filter](https://docs.pipewire.org/devel/page_tutorial7.html) — pw_filter API for in-process DSP
- [PipeWire Parametric Equalizer Module](https://docs.pipewire.org/page_module_parametric_equalizer.html) — static EQ config format
- [Writing a PipeWire Parametric EQ Module (asymptotic.io)](https://asymptotic.io/blog/pipewire-parametric-autoeq/) — practical implementation reference
- [EarLevel Biquad C++ Source](https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/) — reference biquad implementation
- Existing codebase: `AudioService.cpp`, `ThemeService.cpp`, `PhonePlugin.cpp`, `AndroidAutoOrchestrator.cpp`, `AndroidAutoPlugin.cpp`, `UiConfigMessages.proto`
