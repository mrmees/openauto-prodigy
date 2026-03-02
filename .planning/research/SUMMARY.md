# Project Research Summary

**Project:** OpenAuto Prodigy — v0.4.3 Interface Polish & Settings Reorganization
**Domain:** Automotive head unit UI/UX — Qt 6 QML visual refresh and settings restructuring
**Researched:** 2026-03-02
**Confidence:** HIGH

## Executive Summary

This milestone is a UI-layer overhaul of an existing, working Qt 6/QML automotive head unit application. The app already has a solid plugin architecture, validated AA protocol stack, and working audio/BT subsystems. v0.4.3 is specifically about making the settings experience match automotive UX standards and giving the entire interface a visual polish pass. No new C++ services are needed; this is overwhelmingly a QML-side effort with minimal C++ surface changes (two new ThemeService Q_PROPERTYs).

The recommended approach is evolutionary: restyle existing controls first (visual foundation), then restructure settings navigation (settings reorganization), then wire the NavStrip EQ shortcut as the final integration point. This ordering avoids double-work — new pages inherit polished controls from day one, and the EQ shortcut can only be wired once both its source (NavStrip) and destination (SettingsMenu) are ready. The key structural change is replacing a flat 8-item settings list with a 6-category navigation system that maps logically to user mental models: Android Auto / Display / Audio / Connectivity / Companion / System.

The critical risks are Pi 4 GPU performance (StackView transitions can tank frame rate if not carefully controlled), EQ dual-access state divergence (two navigation paths to the same component causing inconsistent UI state), and the ongoing Qt 6.4/6.8 platform gap between dev VM and Pi target. All three are well-understood and have clear mitigations. No new dependencies are required — everything needed is already in Qt Quick / Qt Quick Controls, already installed, and already validated on Pi 4.

## Key Findings

### Recommended Stack

No new packages or dependencies are needed for this milestone. The animation system (OpacityAnimator, XAnimator, Behavior, ColorAnimation, StackView transitions) is built into QtQuick and already a project dependency. Material Symbols Outlined variable font is already bundled at 10MB. The only C++ changes are two new Q_PROPERTY additions to ThemeService (`dividerColor`, `pressedColor`) with corresponding YAML keys. UiMetrics additions are QML-only (it's already a QML singleton).

**Core technologies:**
- `OpacityAnimator` / `XAnimator`: Page transitions — render-thread execution stays smooth even when UI thread is busy (available since Qt 5.2, no compat concerns)
- `Behavior on <property>` + `ColorAnimation`: Control feedback and theme transitions — already used in EqBandSlider, Sidebar, NotificationArea; extend the existing pattern
- `StackView` custom transitions: Settings category drill-down — already in use; needs transition properties populated and performance-validated on Pi 4
- Material Symbols Outlined variable font: All iconography — already bundled; add `font.variableAxes` support on Qt 6.8 via runtime `hasOwnProperty` guard
- Static `Grid` in `Flickable`: Category tile grid — 6 fixed items, no dynamic model or GridView needed

**Critical constraints:**
- All animation durations 80-200ms maximum (automotive = instant feel). 150ms for page transitions, 80ms for press feedback
- `font.variableAxes` is Qt 6.8-only — must use runtime check, not conditional compilation
- Never mix `Animator` types with `NumberAnimation` in the same `ParallelAnimation` — defeats render-thread optimization
- Do NOT use anchors on root-level items pushed to StackView — blocks position-based transition animations

### Expected Features

The research identified a clear gap between current state and automotive table stakes. The single biggest UX miss is press feedback — every tappable control currently gives no visual confirmation of a tap, which violates basic automotive HMI requirements and Google Design for Driving guidelines (330ms ripple minimum, per spec). Visual feedback has been shown to reduce glance time by 40%.

**Must have (table stakes — defines v0.4.3):**
- Press feedback on ALL tappable elements — largest single UX gap; without this the UI feels dead. Add pressed color state (highlight at 20% opacity) or scale 0.95 to SettingsListItem, Tile, NavStrip buttons, FullScreenPicker rows, EQ controls
- Settings reorganization into 6 category tiles — headline feature; replaces flat 8-item list with a 2x3 tile grid
- Video settings moved into Android Auto category — rearrangement, not new features; video config only makes sense in AA context
- EQ moved into Audio category — audio feature belongs in audio settings; keep NavStrip shortcut for quick access
- Consistent icons on all settings rows — add leading icon support to SettingsToggle, SettingsSlider, SegmentedButton, FullScreenPicker

**Should have (quality improvements, do if time allows):**
- BT device swipe-to-forget — port EQ preset swipe pattern to ConnectionSettings; replace tiny "Forget" text link
- Inline status on category tiles ("5GHz Ch36", "2 paired") — differentiator, requires optional `subtitle` prop on Tile
- Font size bump for automotive readability — bump fontBody from 20 to 22-24px base; test at arm's length on Pi
- NavStrip EQ shortcut button — quick-access icon between day/night toggle and settings gear

**Defer to future milestone:**
- Touch target size audit (touchMin from 56 to 76px) — needs full regression test across all views
- Animated transition tuning beyond initial implementation
- Read-only field visual overhaul
- Settings value preview on category tiles
- Ripple/wave animation feedback — requires fragment shader, overkill for this milestone

**Proposed settings reorganization (6 categories):**
- **Android Auto**: Resolution, FPS, DPI, codecs, sidebar, auto-connect, TCP port, protocol capture
- **Display**: Brightness, theme, wallpaper, orientation, day/night source, GPIO night pin
- **Audio**: Master volume, output device, input device, mic gain + EQ sub-page (Output/Input and Equalizer)
- **Connectivity**: WiFi channel/band, BT name, accept pairings, paired devices list
- **Companion**: All companion app settings (unchanged)
- **System / About**: HU identity, hardware profile, touch device, version, close app

### Architecture Approach

The architecture is evolutionary, not revolutionary. The existing StackView + signal-based navigation pattern is sound and extends cleanly to the new two-level settings hierarchy. No new C++ services are needed. The SettingsMenu.qml gets a structural rewrite; all other navigation plumbing stays the same. The critical architectural principle: category pages emit signals, SettingsMenu handles navigation — category components never reference the StackView directly.

**Component change map:**
1. `SettingsMenu.qml` (REWRITTEN) — StackView orchestrator; adds `openDirectPage()` for NavStrip EQ shortcut; handles back navigation and TopBar title sync
2. `SettingsCategoryList.qml` (NEW) — 6-item static category list + dynamically appended plugin settings; emits `categorySelected` signal
3. `AndroidAutoSettings.qml` (NEW) — merges ConnectionSettings (AA/capture sections) + VideoSettings (all content)
4. `ConnectivitySettings.qml` (NEW) — extracts BT + WiFi AP sections from ConnectionSettings
5. `AudioSettingsCategory.qml` (NEW) — subcategory page: "Output & Input" and "Equalizer" sub-items
6. `SystemAboutSettings.qml` (NEW) — merges SystemSettings + AboutSettings
7. All existing controls (MODIFIED) — visual refresh + press state + icon prop; same public API preserved

**Files deleted (content migrated, not lost):** ConnectionSettings.qml, VideoSettings.qml, SystemSettings.qml, AboutSettings.qml

**Key patterns to follow:**
- All settings pages use Flickable + ColumnLayout template (already universal in codebase)
- All controls bind to ConfigService via `configPath` string — config paths are NEVER changed during this reorganization
- Signal-based sub-navigation: category pages emit signals, SettingsMenu pushes pages
- NavStrip EQ shortcut is pure QML-to-QML: `settingsView.openDirectPage("eq")` — zero C++ changes needed

### Critical Pitfalls

1. **StackView transitions drop to 15-20fps on Pi 4** — VideoCore VI cannot composit two full-screen blended layers at 60fps while AA video may be decoding. Start with `null` transitions (instant). Only add animation if Pi 4 testing confirms it runs smooth. Use `QSG_RENDER_TIMING=1` to measure. The existing Sidebar uses 80ms animations — treat that as the proven ceiling.

2. **EQ dual-access creates two instances with divergent state** — StackView `push(Component)` creates a new EqSettings instance each time. Local QML state (`currentStream`, `currentBypassed`) initializes fresh, diverging from whatever the other navigation path last showed. Fix: either use a single shared instance (Loader parented to Shell), or make all visual state fully derived from `EqualizerService` C++ singleton properties. This architecture decision must be made before Phase 3.

3. **`layer.enabled` causes GPU memory exhaustion** — the codebase currently has zero `layer.enabled` usage. Maintain this. For rounded corners use `Rectangle` radius (GPU-native). For subtree opacity animate individual children. Ban `layer.enabled` entirely for this milestone.

4. **Config path breakage during settings reorganization** — moving a control to a different settings page tempts reorganizing config paths to match (e.g. `video.resolution` → `android_auto.video.resolution`). This breaks existing installations. UI categories and YAML config paths are independent. Never rename a config path during a UI reskin.

5. **Qt 6.4 / 6.8 platform gap** — dev VM (Qt 6.4) vs Pi target (Qt 6.8). Forbidden APIs: `loadFromModule()` (already documented), `font.variableAxes` (requires runtime guard), `Popup.popupType`, `Text.renderType: CurveRendering`, anything marked "since Qt 6.5+" in the docs. Test on both platforms after every QML component change.

## Implications for Roadmap

Based on combined research, a 3-phase structure is recommended. Phase boundaries are driven by: (a) the need for polished controls before new pages are built, (b) the EQ shortcut depending on both NavStrip and SettingsMenu changes, and (c) risk management benefit of validating control styling on the existing structure before restructuring it.

### Phase 1: Visual Foundation — Control Polish
**Rationale:** Restyle existing controls while the settings structure is unchanged. New settings pages will inherit polished controls from day one, eliminating double-work. This is also the lowest-risk phase — same public API, only styling changes — so it can be validated on both Qt 6.4 and 6.8 without structural risk.
**Delivers:** Press feedback on all tappable elements. Optional icon prop on all controls. SettingsToggle/SettingsSlider/SegmentedButton/FullScreenPicker visually refreshed. Tile and LauncherMenu flatter. NavStrip spacing polished (EQ button not yet wired). UiMetrics additions (categoryRowH, animDuration, animDurationFast). ThemeService additions (dividerColor, pressedColor).
**Addresses:** Press feedback gap (table stakes #1), icon consistency (#5), automotive visual language throughout
**Avoids:** `layer.enabled` temptation (banned from day one), hardcoded pixels (enforce UiMetrics from day one), Qt 6.4/6.8 compat violations (test on both platforms per control)

### Phase 2: Settings Restructuring
**Rationale:** Build new settings pages using the freshly polished control library. Each new page is independently buildable and testable. Delete old files only after content is fully migrated and verified. Config paths are never changed.
**Delivers:** SettingsCategoryList (6-category top-level), SettingsMenu rewritten to 2-level hierarchy with `openDirectPage()`, AndroidAutoSettings (merges 3 source files), ConnectivitySettings (extracted), SystemAboutSettings (merged), AudioSettingsCategory (subcategory page). Old files deleted. All configPaths verified against current config.yaml schema.
**Addresses:** Settings reorganization into 6 categories (#2), Video into Android Auto (#3), EQ in Audio (#4)
**Avoids:** Config path breakage (UI categories independent of YAML paths), StackView state leaks (decide Loader vs destroyOnPop architecture before building), deep nesting (2 levels max — Audio is the only subcategory)

### Phase 3: EQ Dual-Access and Integration Validation
**Rationale:** NavStrip EQ shortcut is the final integration point — it requires NavStrip modifications (Phase 1) and SettingsMenu's `openDirectPage()` (Phase 2). End-to-end navigation testing covers all paths that touch the new structure. EQ state architecture must be resolved here.
**Delivers:** NavStrip EQ button wired. EQ accessible from Audio category AND NavStrip shortcut. `openDirectPage` resets StackView to depth 1 before pushing (prevents stale stack state). Full navigation regression: category→detail→back, EQ from launcher/AA/other-settings, plugin settings still work.
**Addresses:** NavStrip EQ shortcut (should-have), EQ state divergence resolution
**Avoids:** StackView state leak on direct navigation (pop to depth 1 before push), animation during video decode (test on Pi 4 with active AA session, max 80ms, use Animator types)

### Phase Ordering Rationale

- Visual-first prevents double-work: polished controls are needed in new pages, so building pages first means restyling twice
- SettingsMenu rewrite must follow SettingsCategoryList creation (obvious dependency)
- AndroidAutoSettings is the largest single merge (3 source files) — build early in Phase 2 to surface issues while there is time to course-correct
- EQ shortcut last because it requires both NavStrip changes and SettingsMenu's `openDirectPage()` — it is the integration test for the whole milestone
- Config path verification is a hard gate at end of Phase 2, not a cleanup task at end of Phase 3

### Research Flags

Phases with standard patterns (skip `/gsd:research-phase`):
- **Phase 1 (control polish):** Well-documented Qt Quick animation patterns; codebase already uses them on Pi 4; Sidebar at 80ms is direct evidence they work
- **Phase 2 (settings restructuring):** Pure QML rearrangement; StackView navigation is well-documented; signal-based architecture already in use
- **Phase 3 (EQ wiring):** `openDirectPage` pattern is fully specified in ARCHITECTURE.md; no unknown territory

Phases needing careful empirical validation rather than deeper research:
- **Phase 1:** Test press feedback and icon sizing on Pi 4 at arm's length — readability is a physical validation problem, not a research problem
- **Phase 3:** StackView transition performance must be measured on Pi 4 (start null, add only if `QSG_RENDER_TIMING=1` confirms smooth). EQ state divergence architecture (shared Loader vs. C++-derived state) must be decided before Phase 3 begins.

No phases need `/gsd:research-phase`. The domain is well-documented, all APIs are verified, and the codebase was read directly during research.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All recommendations use Qt APIs verified in official docs. Codebase already uses these patterns on Pi 4. No new dependencies. |
| Features | HIGH | Grounded in Google Design for Driving guidelines, OEM pattern analysis, and direct inventory of all 43 QML files and 11 existing controls. |
| Architecture | HIGH | Based on direct analysis of all relevant source files. All recommended patterns already exist in the codebase — no speculation. |
| Pitfalls | HIGH (critical), MEDIUM (Pi 4 perf specifics) | Qt official docs confirm layer.enabled, clipping, and animation warnings. Pi 4 frame rate numbers under transition load are from community sources — treat as directional, validate empirically. |

**Overall confidence:** HIGH

### Gaps to Address

- **EQ state divergence architecture decision**: Research identified the problem and two solutions (shared Loader instance at Shell level, or fully C++-derived state in EqualizerService). The choice has downstream implications — Loader is simpler but requires Shell.qml changes; C++ approach is cleaner long-term but requires new EqualizerService properties. Decide before Phase 3 begins.
- **StackView `destroyOnPop: false` availability in Qt 6.4**: Research flagged this property may not exist in Qt 6.4. Verify before relying on it for scroll position preservation; fall back to persistent Loader approach if needed.
- **Press feedback visual values**: Research recommends pressed color at 20% opacity or scale 0.95, but exact values need visual testing on Pi 4 at car-mount distance (60cm). Automotive ambient light differs from desktop monitors.
- **Font size bump layout impact**: Bumping fontBody from 20 to 22-24px needs a full layout audit across all existing settings pages — some may truncate labels at 1024px wide with larger text. Do this within Phase 1, not as an afterthought.

## Sources

### Primary (HIGH confidence)
- Qt 6 StackView documentation — transition properties, anchor caveat, destroyOnPop behavior
- Qt 6 Animator type documentation — render thread execution model, available subtypes, ParallelAnimation caveat
- Qt 6 Behavior / NumberAnimation / ColorAnimation documentation — implicit animation patterns
- Qt 6 Item documentation — layer.enabled GPU memory warnings, clipping costs inside delegates
- Qt 6.8 What's New — documents APIs unavailable in Qt 6.4 (font.variableAxes, Popup.popupType, etc.)
- Qt 6 Quick Performance Considerations — official performance guidance (layer, clipping, animation)
- Google Design for Driving — Sizing (76dp touch targets), Components, Buttons (330ms ripple, 156dp min width)
- Material Symbols developer guide — variable font axes, optical size range 20-48dp, static vs variable tradeoffs
- Android AOSP Car Settings documentation — settings hierarchy and grouping patterns
- Direct codebase analysis — all 43 QML files, all settings pages, all control components, ApplicationController.hpp, ThemeService.hpp, CLAUDE.md, MEMORY.md

### Secondary (MEDIUM confidence)
- Snapp Automotive — touch area sizes in car interfaces (80px min, 100px for frequent actions)
- Qt Forum: QML animations slow on Raspberry Pi — Pi 4 GPU frame rate constraints under animation load
- Qt Forum: Keep state of view in StackView — state loss on push/pop behavior documentation
- MDPI: UI Design Patterns for Infotainment Systems — 9 IVI design patterns
- Spyro Soft: QML Performance Dos and Don'ts — aligns with official Qt performance guidance
- Automotive UX Basics (UXPin) — clean/minimal principle, large text, dark-first for cars

### Tertiary (directional only)
- Pi 4 frame rate numbers during StackView transitions (15-20fps) — community estimate; validate empirically with `QSG_RENDER_TIMING=1` before committing to any transition approach
- Touch feedback readability at 60cm arm's length — automotive UX convention; validate on actual Pi display at realistic mounting distance

---
*Research completed: 2026-03-02*
*Ready for roadmap: yes*
