# Feature Landscape

**Domain:** Raspberry Pi Android Auto head unit (wireless-only)
**Researched:** 2026-03-01
**Comparisons:** OpenAuto Pro (predecessor), Crankshaft (open-source), commercial aftermarket head units (Kenwood, Pioneer, Sony, ATOTO)

## Table Stakes

Features users expect from a head unit app. Missing any of these means the product feels incomplete or broken at v1.0.

| Feature | Why Expected | Complexity | Status | Notes |
|---------|--------------|------------|--------|-------|
| Reliable wireless AA connection | Core purpose of the product | -- | DONE | BT discovery, WiFi AP, TCP, auto-reconnect all working |
| Video projection with low latency | Core purpose | -- | DONE | Multi-codec decode, hw/sw fallback, ~50fps on Pi 4 |
| Multi-touch input | Every head unit has touch | -- | DONE | Direct evdev, MT Type B, 3-finger gesture |
| Audio playback (media, nav, phone) | No audio = unusable | -- | DONE | 3 PipeWire streams with audio focus |
| Bluetooth audio (A2DP) | Users play music without AA | -- | DONE | BtAudioPlugin with AVRCP controls |
| Phone calls (HFP) | Users expect hands-free calling | -- | DONE | PhonePlugin with dialer + incoming call overlay |
| Volume control | Every head unit has this | Low | DONE | AudioService + QML sliders + gesture overlay |
| Screen brightness control | Every head unit has this | Low | PARTIAL | IDisplayService interface exists, settings slider exists, but setBrightness() is not wired to actual backlight control |
| Day/night theme | All head units switch colors for night driving | Low | DONE | ThemeService with time/GPIO/manual sources, AA night mode toggle |
| Settings UI | Users need to configure without SSH | -- | DONE | 8 settings views (Audio, Video, Display, Connection, System, etc.) |
| Bluetooth pairing UI | Users pair phones from the touchscreen | -- | DONE | PairingDialog, PairedDevicesModel, auto-connect retry |
| Auto-start on boot | Head unit should be ready when car starts | Low | DONE | systemd service via installer |
| Installer that works | Non-developers need to set this up | -- | DONE | Interactive install.sh validated on fresh Trixie |
| HFP audio persistence across AA state | Phone calls must not drop when AA disconnects/crashes | Med | IN PROGRESS | Active item in roadmap. Critical for daily driver use. |
| Audio equalizer with presets | Every commercial head unit has EQ; OAP had it | Med | IN PROGRESS | Active item in roadmap. PipeWire filter chain integration needed. |
| Clean default logging | Debug spam in production is unprofessional | Low | PLANNED | In roadmap "Later" section |
| Theme/wallpaper selection UI | OAP had this; users expect to personalize | Med | PLANNED | Color palette picker + wallpaper chooser. ThemeService supports it, needs QML UI. |
| Graceful shutdown/reboot | Users need to power off safely from touchscreen | Low | PARTIAL | ExitDialog.qml exists but needs verification that it actually triggers system shutdown/reboot |
| First-run experience | New users need guided initial setup | Low | PARTIAL | FirstRunBanner.qml exists, but unclear if it covers BT pairing + WiFi config on first boot |

## Differentiators

Features that set Prodigy apart from competitors. Not expected, but valued when present. These create competitive advantage against commercial units and the defunct OAP.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Open source (GPLv3) | No vendor lock-in, community can fix bugs, survives company death (unlike OAP) | -- | Already true. THE differentiator. |
| Plugin architecture | Extensibility beyond what any closed-source head unit offers. OBD, cameras, GPIO, custom apps. | -- | Already exists. Static + dynamic plugin loading. |
| Web config panel | Configure from phone/laptop without touching the screen. Unique among Pi head units. | -- | Already exists (Flask + Unix socket IPC). |
| Dynamic sidebar during AA | Change sidebar position/visibility without AA reconnect. No commercial unit does this. | High | Depends on `UpdateHuUiConfigRequest` (0x8012) protocol message. Experimental. |
| Multi-codec video (H.265/VP9/AV1) | Future-proofing as phones adopt newer codecs. Most head units are H.264-only. | -- | Already implemented. |
| 3-finger gesture overlay | Quick controls (volume, brightness, day/night) without leaving AA. Better than OAP's top-bar pulldown. | -- | Already implemented. |
| Per-connection WiFi password rotation | Security hardening unique to Prodigy. Fresh WPA password each connection. | Med | Wishlist item. hostapd_cli integration. |
| Protocol capture/debug logging | Invaluable for debugging AA issues. No other head unit exposes this. | -- | Already implemented. JSONL/TSV with settings UI. |
| Companion Android app | Phone-side GPS, battery, time sync, internet sharing. Only AAWireless has a companion app in this space. | High | Separate repo, separate timeline. Not blocking v1.0. |
| Prebuilt release distribution | Users can install without compiling. Lowers the bar massively vs Crankshaft/original OpenAuto. | -- | Already implemented. |
| Cross-compilation toolchain | Developer quality-of-life. Fast iteration without building on Pi. | -- | Already set up. |

## Anti-Features

Features to explicitly NOT build for v1.0. Each has been considered and rejected with rationale.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| USB Android Auto | Adds AOAP protocol complexity, libusb, hotplug handling. Every phone since 2022 supports wireless AA. Wireless-only is a deliberate simplification. | Keep wireless-only. Revisit only if user demand is overwhelming post-v1.0. |
| CarPlay / non-AA protocols | Massive scope expansion. Apple's protocol is proprietary. Would double the codebase. | Single protocol focus for quality. Let someone else fork for CarPlay. |
| Screen mirroring | OAP had this but it's a niche feature requiring scrcpy/VNC integration. Not what daily drivers need. | Defer post-v1.0. Could be a plugin if someone contributes it. |
| Built-in media player (Kodi) | OAP bundled Kodi. Huge dependency, limited use case (most people stream from phone via BT/AA). | BT audio A2DP is already there. External app launcher could point to Kodi if user installs it separately. |
| BMW iDrive / IBus / MMI controllers | Niche hardware integration that OAP supported. Enormous maintenance burden for tiny user base. | Post-v1.0 as community-contributed plugins. Plugin system already supports this pattern. |
| Hardware sensor integration (TSL2561, DS18B20) | OAP supported i2c light sensor and temperature sensor. Niche, requires specific hardware. | GPIO day/night switching is already in settings. Light sensor could be a post-v1.0 plugin. |
| External application launcher | OAP had this (up to 8 apps). Requires process lifecycle management, window stacking. | Plugin system is the replacement. Plugins run in-process with proper lifecycle. External apps can be launched from the Pi desktop if needed. |
| Multi-display / multi-resolution | Supporting arbitrary screen sizes adds testing complexity. | Lock to 1024x600 target for v1.0. UiMetrics scale factor handles minor variation. |
| Cloud services / telemetry / accounts | Privacy violation. Against project principles. No server infrastructure to maintain. | Offline-only, forever. |
| Pi 5 / other SBC support | Adds hardware matrix, different GPU drivers, different display pipelines. | Pi 4 reference hardware only for v1.0. Community can test on Pi 5 but it's not a supported target. |

## Feature Dependencies

```
Screen brightness control (wire to backlight) --> Gesture overlay brightness slider (already exists, needs wiring)
HFP audio persistence --> Audio focus management refactor (HFP must outlive AA session)
Audio equalizer --> PipeWire filter-chain integration --> EQ presets in YAML config --> EQ UI in settings + web panel
Theme selection UI --> ThemeService palette loading (exists) --> QML color palette picker --> Wallpaper file browser
Dynamic sidebar --> UpdateHuUiConfigRequest protocol support --> AA video renegotiation --> Sidebar position settings
Clean logging --> Log level config option --> Quiet default, --verbose flag
First-run experience --> BT pairing flow --> WiFi status check --> "Ready to connect" screen
```

## Gaps: What Prodigy Has vs What's Needed for v1.0

### Already Complete (no work needed)
- Wireless AA connection flow (BT + WiFi AP + TCP)
- Video decode (multi-codec, hw/sw fallback)
- Touch input (direct evdev, multi-touch, 3-finger gesture)
- Audio playback (3 PipeWire streams)
- BT Audio plugin (A2DP + AVRCP)
- Phone plugin (HFP dialer + call overlay)
- Plugin system (static + dynamic)
- Settings UI (8 views)
- Bluetooth pairing UI
- Day/night theme (time/GPIO/manual)
- Installer (interactive, validated)
- Web config panel
- Protocol capture logging
- Prebuilt release distribution
- Cross-compilation toolchain

### In Progress (active development)
- HFP audio persistence across AA state
- Audio equalizer with presets

### Needs Work for v1.0
- **Screen brightness wiring:** IDisplayService exists, QML slider exists, but setBrightness() is a TODO. Must write to `/sys/class/backlight/` on Pi.
- **Theme/wallpaper selection UI:** ThemeService supports palettes, but no QML picker. Users can only change theme via web panel or YAML.
- **Clean logging:** Debug spam in production output. Needs log level config + quiet defaults.
- **First-run polish:** FirstRunBanner exists but needs to be a complete guided setup (pair phone, verify WiFi, test connection).
- **Graceful shutdown verification:** ExitDialog.qml exists but needs confirmation that reboot/shutdown actually work via systemd D-Bus calls.

### Nice-to-Have for v1.0 (not blocking, but adds polish)
- Dynamic sidebar reconfiguration during AA (depends on protocol discovery)
- Per-connection WiFi password rotation

## MVP Recommendation

For v1.0 release, prioritize in this order:

1. **HFP audio persistence** -- table stakes for daily driver use. Phone calls dropping is a dealbreaker.
2. **Audio equalizer** -- table stakes. Every commercial head unit and OAP had this. Car audio without EQ sounds terrible.
3. **Screen brightness wiring** -- low effort, high impact. The UI already exists, just needs the sysfs backend.
4. **Theme/wallpaper selection UI** -- moderate effort. Users expect to personalize their head unit.
5. **Clean logging** -- low effort, high impact for perceived quality. Debug spam in journalctl looks amateur.
6. **First-run experience polish** -- moderate effort. Makes the difference between "I installed it and it works" vs "I installed it and had to SSH in to fix things."

**Defer to post-v1.0:**
- Dynamic sidebar reconfiguration: depends on undiscovered protocol behavior, high risk
- Per-connection WiFi rotation: security enhancement, not user-facing
- Companion app: separate repo/timeline, not blocking
- Community plugin examples: plugin system works, examples are documentation not features
- CI automation: desirable but not user-facing

## Sources

- [OpenAuto Pro User Guide (PDF)](https://www.readkong.com/page/openauto-pro-7564639) -- predecessor feature set reference
- [OpenAuto Pro Wiki](https://github.com/f1xpl/openauto/wiki/OpenAuto-Pro) -- feature list
- [Crankshaft](https://getcrankshaft.com/) -- open-source competitor (USB-only, unmaintained)
- [Best Android Auto Head Units 2025 (T3)](https://www.t3.com/features/best-android-auto-head-unit) -- commercial feature benchmarks
- [Best Aftermarket Head Units (Auto Sound NYC)](https://autosoundnyc.com/2026/02/22/best-aftermarket-head-units-for-android-auto/) -- commercial feature benchmarks
- [2025 Android Car Stereo Guide (ATOTO)](https://www.atotodirect.com/blogs/guides/2025-android-car-stereo-guide-head-unit-checklist-upgrade-tips) -- feature checklist
- [AAWireless](https://www.aawireless.io/en) -- wireless AA dongle with companion app
- OpenAuto Pro manual PDF (local: `/home/matt/claude/personal/openautopro/oap_manual_1.pdf`) -- detailed feature/settings reference
