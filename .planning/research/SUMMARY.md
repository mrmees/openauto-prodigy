# Project Research Summary

**Project:** OpenAuto Prodigy — v1.0 Feature Completion
**Domain:** Raspberry Pi Android Auto head unit (wireless-only, Qt 6 + C++)
**Researched:** 2026-03-01
**Confidence:** HIGH (most features extend existing, proven architecture; one feature has unverified protocol behavior)

## Executive Summary

OpenAuto Prodigy is already a functioning wireless Android Auto head unit. This research covers the gap between "it works" and "v1.0 releasable" — five concrete feature areas: HFP call audio independence, audio equalization, theme and wallpaper selection, dynamic sidebar reconfiguration, and logging cleanup. The good news is that every feature can be implemented with the existing dependency set (Qt 6.8, PipeWire 1.4.2, BlueZ, FFmpeg, yaml-cpp, protobuf) — no new packages needed beyond verifying `pipewire-module-filter-chain` is present.

The recommended approach is to build in risk order: low-risk, high-impact improvements first (logging cleanup, theme selection) to establish release quality, then the critical daily-driver features (HFP audio independence, audio EQ), and the experimental protocol work (dynamic sidebar) last. Four of the five features are pure extensions of existing patterns with well-understood implementation paths. The fifth — dynamic sidebar reconfiguration via `UpdateHuUiConfigRequest` (message 0x8012) — is architecturally sound in terms of the proto definition but unverified on the wire. Modern phones may or may not honor runtime margin changes. A feature flag and fallback to disconnect+reconnect is mandatory.

The main architectural risk is in HFP audio: PipeWire's bluez5 plugin already owns HFP AG on Trixie. Any attempt to manually manage SCO sockets will create conflicts and silent audio failures. The correct approach is to let PipeWire handle SCO routing entirely, and have PhonePlugin manage only call state monitoring, audio focus requests, and UI lifecycle — completely decoupled from the AA session. This is an architectural decision that must be made correctly before any code is written, because fixing it retroactively is high-cost.

## Key Findings

### Recommended Stack

No new dependencies required. All five features are achievable with the existing build. The key insight from STACK.md is that the app already has everything needed: PipeWire for EQ (inline biquad in the RT callback, not the filter-chain module), QLoggingCategory for logging (Qt built-in), yaml-cpp for theme directory scanning, and the `UiConfigMessages.proto` already in the open-androidauto submodule for dynamic sidebar.

**Core technologies (additions/changes):**
- PipeWire RT callback (existing): Inline biquad EQ — avoids PW graph complexity entirely
- QLoggingCategory (Qt built-in): Replaces bare `qDebug()` — zero new deps, ~230 call sites to migrate
- ThemeService extension (existing): Directory scanning + wallpaper path Q_PROPERTY — no new deps
- UiConfigMessages.proto (existing submodule): 0x8012/0x8013 for dynamic sidebar — needs wire verification
- PipeWire bluez5 + WirePlumber (existing): HFP AG audio — app explicitly must NOT compete with this

**What NOT to use:** ofono (rejected by project), PipeWire filter-chain for EQ (latency + graph complexity), SCO socket management in-app (conflicts with PipeWire), spdlog/Boost.Log (Qt already has logging), EasyEffects (GTK4 desktop app, not embeddable), SVG wallpapers (fixed resolution, JPEG is simpler).

### Expected Features

**Must have for v1.0 (table stakes):**
- HFP audio persistence across AA disconnect — phone calls dropping is a daily-driver dealbreaker
- Audio equalizer with presets — every commercial head unit has EQ; car audio without it sounds bad
- Screen brightness wiring — IDisplayService and QML slider exist, setBrightness() backend is a TODO; must write to `/sys/class/backlight/`
- Theme/wallpaper selection UI — ThemeService supports it, QML picker missing; users expect personalization
- Clean logging (quiet production output) — debug spam in journalctl looks amateur, degrades perceived quality
- First-run experience polish — FirstRunBanner exists but needs to be a complete guided flow

**Should have (differentiators):**
- Dynamic sidebar reconfiguration during AA — no commercial unit does this; protocol support is experimental
- Per-connection WiFi password rotation — security hardening, unique to Prodigy

**Defer to post-v1.0:**
- USB Android Auto — wireless-only is a deliberate simplification
- CarPlay / screen mirroring — massive scope, not daily-driver needs
- Built-in media player (Kodi), iDrive/IBus controllers, hardware sensor integration
- Multi-display support, companion app (separate repo/timeline), CI automation
- Community plugin examples — plugin system works, examples are documentation

### Architecture Approach

The existing layered architecture (QML Shell → PluginManager → IHostContext services → Plugin implementations → PipeWire/BlueZ/FFmpeg) is solid and should not be restructured. All five features integrate cleanly as targeted modifications: two new services (EqualizerService, LoggingService), one extended service (ThemeService), and two modified plugins (PhonePlugin for audio independence, AndroidAutoOrchestrator for dynamic margin push). The critical thread model constraints — ASIO threads bridging to Qt main via `QMetaObject::invokeMethod(QueuedConnection)`, lock-free ring buffer for audio — must be respected by all new code.

**Major components and responsibilities:**
1. EqualizerService (NEW) — Biquad DSP state, RT-safe atomic coefficient swap, preset YAML storage. Process called inline from `AudioService::onPlaybackProcess()`. Media stream only — navigation and phone streams bypass EQ.
2. ThemeService (EXTENDED) — Add `scanThemeDirectories()`, implement the stub `setTheme(id)`, add `wallpaperPath` Q_PROPERTY. Theme picker QML view in Settings.
3. PhonePlugin (MODIFIED) — Decouple HFP audio lifecycle from AA session. Own audio focus requests independently. Let PipeWire handle actual SCO routing.
4. AndroidAutoOrchestrator (MODIFIED) — Add `pushUiConfigUpdate()` behind feature flag. Send 0x8012, handle 0x8013 response. Fall back to disconnect+reconnect on failure.
5. LoggingService (NEW) — `qInstallMessageHandler()` at startup, `QLoggingCategory` definitions for all subsystems, `--verbose` CLI flag, runtime toggle via web panel.

### Critical Pitfalls

1. **HFP audio ownership conflict with PipeWire** — PipeWire's bluez5 plugin already owns HFP AG on Trixie. Attempting to open SCO sockets directly or register a competing AG profile causes silent audio failures (call connects at signaling level, no audio flows). Prevention: explicitly let PipeWire own SCO; app manages only focus/ducking and call UI state.

2. **HFP call audio drops when AA disconnects** — If PhonePlugin audio lifecycle is coupled to the AA session, calls drop when AA disconnects mid-drive. Prevention: PhonePlugin must own audio focus independently of AndroidAutoPlugin. Test matrix must cover: call starts before AA, during AA, AA disconnects during call. This is HIGH-cost to fix retroactively.

3. **AA margins locked at session start — dynamic sidebar is constrained by protocol** — `ServiceDiscoveryResponse` is sent once at connection time; margins cannot change mid-session per protocol spec. `UpdateHuUiConfigRequest` (0x8012) is the only potential path, but its runtime behavior is unverified. Prevention: design settings UI to clearly show "takes effect on next connection" for sidebar changes. Provide instant visual feedback by toggling sidebar strip visibility (overlay) without changing video margins, while actual resize waits for reconnect.

4. **EQ latency on navigation and phone call audio** — Inserting EQ into ALL audio streams adds latency on top of the existing ~100ms wireless AA transport delay. Navigation prompts and call audio cross the perceptibility threshold. Prevention: apply EQ only to the media/music PipeWire stream. Navigation and phone streams bypass EQ entirely. The three-stream separation in AudioService already enables this.

5. **ThemeService live reload requires Q_PROPERTY bindings, not signal assumptions** — QML bindings to function calls like `ThemeService.color("key")` are NOT automatically re-evaluated when `colorsChanged()` emits. Must use Q_PROPERTY for each color value, or explicit `Connections {}` blocks. Prevention: verify existing color bindings work on theme switch before building the picker UI.

## Implications for Roadmap

Based on research, all five features are independent (no feature depends on another). The ordering below is purely by risk/impact ratio: highest-confidence lowest-risk first, experimental last.

### Phase 1: Logging Cleanup
**Rationale:** Zero risk, immediate developer quality-of-life benefit that makes debugging ALL subsequent phases easier. Mechanical work (replace ~230 `qDebug()` calls with categorized equivalents) that doesn't touch business logic.
**Delivers:** Production-quality journald output. `--verbose` flag. Runtime toggle via web panel. Quiet defaults.
**Addresses:** "Clean logging" table stakes from FEATURES.md
**Avoids:** `fprintf` in RT audio callbacks (performance trap from PITFALLS.md)
**Research flag:** Skip — QLoggingCategory is thoroughly documented, well-understood Qt pattern.

### Phase 2: Theme and Wallpaper Selection
**Rationale:** High confidence, self-contained, zero protocol risk. Extends existing ThemeService patterns. Delivers visible polish that builds confidence in release quality.
**Delivers:** Theme directory scanning. `setTheme()` implemented. QML picker in Settings. Wallpaper support. 2-3 bundled themes.
**Addresses:** "Theme/wallpaper selection UI" table stakes from FEATURES.md
**Avoids:** Missing Q_PROPERTY bindings for live reload (PITFALLS.md integration gotcha). Pre-scale wallpapers to 1024x600 on selection (PITFALLS.md performance trap).
**Research flag:** Skip — pure extension of existing ThemeService patterns, no new system boundaries.

### Phase 3: HFP Call Audio Independence
**Rationale:** Critical for daily-driver use. Phone calls dropping is a showstopper. Must be done before EQ (audio architecture must be understood first) and has an architectural decision that's expensive to reverse. Wire verification needed on Pi.
**Delivers:** Phone calls survive AA disconnect/reconnect. Audio focus management independent of AA session. Bidirectional call audio (both directions must be verified).
**Addresses:** "HFP audio persistence across AA state" table stakes from FEATURES.md
**Avoids:** HFP audio ownership conflict with PipeWire (Critical Pitfall 1). Coupling audio lifecycle to AA session (Critical Pitfall 2). PipeWire's `org.pipewire.Telephony` API (1.4+) for call control if needed.
**Research flag:** Flag for wire testing on Pi. PipeWire's automatic HFP SCO behavior needs verification — especially mSBC vs CVSD codec negotiation and whether app needs to do anything beyond audio focus management.

### Phase 4: Audio Equalizer
**Rationale:** Table stakes for car audio. More complex than themes (biquad math, RT-safe coefficient swap, two UIs — QML preset selector and web panel band editor) but well-understood. Benefits from clean logging (Phase 1) for debugging.
**Delivers:** 10-band parametric EQ with preset YAML storage. Real-time coefficient updates (no restart). EQ applies to media stream only, bypasses navigation/phone. Presets: Flat (default off), Bass Boost, Treble Boost, Vocal Clarity, Road Noise Compensation.
**Addresses:** "Audio equalizer with presets" table stakes from FEATURES.md
**Avoids:** EQ latency on navigation/call audio (Critical Pitfall 4). PipeWire filter-chain approach (Anti-Pattern 1 from ARCHITECTURE.md — graph complexity, WirePlumber conflict). `fprintf` in RT callback (performance trap).
**Research flag:** Skip — biquad DSP is well-understood mathematics. Inline-in-RT-callback pattern is documented in ARCHITECTURE.md with code example.

### Phase 5: Dynamic Sidebar Reconfiguration
**Rationale:** Only after core features are solid and daily-driver quality is established. Highest risk (protocol behavior unverified). Even partial success (dynamic night-mode push via 0x8012 without margin changes) is valuable.
**Delivers:** `UpdateHuUiConfigRequest` (0x8012) sending behind feature flag. Protocol capture to verify phone response. Fallback to disconnect+reconnect if phone rejects. Clear "takes effect on next connection" UX for rejected/unsupported scenarios.
**Addresses:** "Dynamic sidebar" differentiator from FEATURES.md
**Avoids:** Designing the settings UI without "takes effect on next connection" messaging (UX pitfall). Missing evdev touch zone recalculation after sidebar state changes (PITFALLS.md "Looks Done But Isn't" checklist).
**Research flag:** FLAG — requires wire testing with protocol capture enabled before committing to the implementation approach. The 0x8012 message exists in the proto (confirmed) but phone-side behavior is unverified. Margin changes may not work; theme-only changes may still work. Design must handle all outcomes gracefully.

### Phase 6: Release Hardening
**Rationale:** The "Looks Done But Isn't" checklist from PITFALLS.md covers items that don't fit in feature phases but are required for a trustworthy release. Grouped here rather than scattered across phases.
**Delivers:** Screen brightness backend wired to `/sys/class/backlight/`. systemd watchdog integration. Log rotation bounds. Clean SIGTERM shutdown (EVIOCGRAB released, BT ungrabbed, config flushed). Graceful AudioService degradation when PipeWire is unavailable. First-run experience polish. ExitDialog shutdown/reboot verification.
**Addresses:** Partial-status items from FEATURES.md (brightness, first-run, graceful shutdown). Release hardening section from PITFALLS.md.
**Research flag:** Skip for most items. Watchdog integration is well-documented for Pi 4 BCM watchdog. sysfs backlight path is hardware-specific — verify on Pi before coding.

### Phase Ordering Rationale

- Logging first: makes debugging phases 2-6 measurably easier. Zero-risk warmup.
- Theme before HFP: confidence building, no risk. Validates the YAML/QML extension patterns before touching audio.
- HFP before EQ: audio architecture understanding (what PipeWire owns vs what app owns) informs EQ stream selection (media only). Wrong HFP architecture breaks EQ later.
- EQ before dynamic sidebar: EQ is well-understood; sidebar is experimental. Ship the high-confidence features first.
- Dynamic sidebar last: protocol-dependent, has the only unverified assumption in all the research. Doesn't block v1.0 if it fails.
- Release hardening last: crosses all other phases, validates the whole system.

### Research Flags

Phases needing deeper research during planning:
- **Phase 3 (HFP Call Audio):** Wire test on Pi required. PipeWire's automatic HFP SCO behavior, mSBC/CVSD codec handling, and the `org.pipewire.Telephony` D-Bus API (new in 1.4) need verification before finalizing the PhonePlugin architecture. Session memory confirms HFP AG is owned by PipeWire, but the exact audio routing path hasn't been exercised.
- **Phase 5 (Dynamic Sidebar):** Protocol behavior of 0x8012 on modern AA (16.x) is completely unverified. Must test with protocol capture before committing to implementation. Fallback path (reconnect) is required regardless.

Phases with standard patterns (skip research):
- **Phase 1 (Logging):** QLoggingCategory is thoroughly documented Qt functionality.
- **Phase 2 (Theme):** Pure extension of existing ThemeService. No new system boundaries.
- **Phase 4 (EQ):** Biquad DSP math is standard DSP. RT-safe atomic swap pattern is documented with code in ARCHITECTURE.md.
- **Phase 6 (Release Hardening):** Mostly Pi/systemd-specific work with clear sysfs/D-Bus APIs.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All features use existing deps. PipeWire filter-chain vs inline biquad decision has been analyzed from multiple angles; inline wins on all metrics for this use case. |
| Features | HIGH | Feature set well-defined. Table stakes derived from OAP predecessor + commercial benchmarks. Anti-features explicitly justified. |
| Architecture | HIGH (4/5 features) / LOW (sidebar margins) | Logging, theme, EQ, HFP architecture all HIGH confidence — pure extensions of existing patterns. Dynamic margin push via 0x8012 is LOW confidence on runtime phone behavior. |
| Pitfalls | MEDIUM-HIGH | HFP audio ownership conflict and EQ latency pitfalls are well-grounded in codebase analysis. PipeWire Telephony API confidence is MEDIUM (new API, may evolve). Watchdog specifics are MEDIUM (community sources). |

**Overall confidence:** HIGH for the 4 low-risk features. MEDIUM for dynamic sidebar. Ready to roadmap.

### Gaps to Address

- **0x8012 runtime behavior on modern phones:** Only gap with no clear resolution from research. Must be tested on wire during Phase 5. Fallback (reconnect) removes blocking risk, but true dynamic resize capability is uncertain. Design Phase 5 to deliver value even if 0x8012 only works for night-mode/theme (not margins).
- **PipeWire Telephony API stability:** `org.pipewire.Telephony` is new (PipeWire 1.4, Feb 2025). Documentation exists but the API may evolve. If it proves unstable, fall back to monitoring call state via BlueZ D-Bus `org.bluez.VoiceGateway` properties. The existing PhonePlugin D-Bus monitoring path is a safe baseline.
- **mSBC codec availability on all test phones:** HFP wideband (mSBC) vs narrowband (CVSD) — must verify both codecs produce working call audio on Pi Trixie. Samsung S25 Ultra is the primary test phone (session memory), but mSBC behavior varies by phone model.
- **Screen brightness sysfs path:** `/sys/class/backlight/` path is hardware/driver-specific on Pi 4 with DFRobot display. Verify exact path before Phase 6 implementation.

## Sources

### Primary (HIGH confidence)
- Existing codebase: `AudioService.cpp`, `ThemeService.cpp`, `PhonePlugin.cpp`, `AndroidAutoOrchestrator.cpp`, `docs/aa-display-rendering.md` — primary source for architecture decisions
- Project session memory — HFP AG on PipeWire confirmed on fresh Trixie install; HT40/VHT80 WiFi validated
- [PipeWire Filter-Chain Module](https://docs.pipewire.org/page_module_filter_chain.html) — biquad EQ filter types and configuration
- [PipeWire Parametric Equalizer Module](https://docs.pipewire.org/page_module_parametric_equalizer.html) — static EQ config format
- [WirePlumber Bluetooth Configuration](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/bluetooth.html) — bluez5.roles, HFP AG, SCO routing
- [QLoggingCategory (Qt 6)](https://doc.qt.io/qt-6/qloggingcategory.html) — categorized logging with runtime filter support
- OpenAuto Pro manual PDF (local) — predecessor feature set reference
- UiConfigMessages.proto (in-tree submodule) — 0x8012/0x8013 message definitions

### Secondary (MEDIUM confidence)
- [PipeWire Bluetooth Telephony Support (George Kiagiadakis, Feb 2025)](https://gkiagia.gr/2025-02-20-pipewire-telephony/) — new Telephony D-Bus API details
- [Writing a PipeWire Parametric EQ Module (asymptotic.io)](https://asymptotic.io/blog/pipewire-parametric-autoeq/) — practical EQ implementation guide
- [EarLevel Biquad C++ Source](https://www.earlevel.com/main/2012/11/26/biquad-c-source-code/) — reference biquad implementation
- [Raspberry Pi Watchdog (community forums)](https://forums.raspberrypi.com/viewtopic.php?t=326779) — Pi 4 BCM watchdog behavior
- [Best Android Auto Head Units 2025 (T3)](https://www.t3.com/features/best-android-auto-head-unit) — commercial feature benchmarks

### Tertiary (LOW confidence)
- aa-proxy-rs source (community) — 0x8012 message ID origin; runtime phone behavior unverified
- [Crankshaft](https://getcrankshaft.com/) — open-source competitor (unmaintained, USB-only; useful as negative comparison only)

---
*Research completed: 2026-03-01*
*Ready for roadmap: yes*
