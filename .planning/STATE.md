---
gsd_state_version: 1.0
milestone: v0.5
milestone_name: milestone
status: in_progress
stopped_at: Phase 06 nearly complete — blocked on hostapd for Pi verification
last_updated: "2026-03-12"
last_activity: "2026-03-12 - Phase 06 plans 01-03 code complete, Pi verification blocked by hostapd"
progress:
  total_phases: 5
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 100
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-12)

**Core value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience -- every time, without SSH.
**Current focus:** v0.5.3 Widget Grid & Content Widgets -- Phase 06 in progress

## Current Position

Phase: 06 of 7 (Content Widgets)
Plan: 03 of 3 — code complete, Pi verification pending
Status: All code committed. Blocked on hostapd failure preventing AA connection for live widget testing.
Last activity: 2026-03-12 - Phase 06 code complete, Pi verification blocked by hostapd

Progress: [#########-] 90%

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.

- Roadmap: WidgetPlacementModel is a clean REPLACEMENT, not extension (pane model can't represent grid coords)
- Roadmap: Content widgets (Phase 06) before edit mode (Phase 07) -- delivers user value earlier
- Roadmap: Multi-page (Phase 08) last -- depends on edit mode for swipe disable coupling
- Roadmap: Phase 08 is the natural cut line -- ship 04-07 if schedule tightens
- Codex review: Phase 04 split into model/persistence (04) and rendering/revision (05) -- original was overloaded
- Codex review: GRID-05 split -- page persistence moved to PAGE-09 (pages don't exist until Phase 08)
- Codex review: Added GRID-08 (resolution portability), EDIT-08/09/10 (EVIOCGRAB exit, atomic writes, no-space feedback), PAGE-06/07/08/09 (page management, lazy instantiation, page persistence)
- Codex review: REV-01 scoped to "shipped v0.5.3 widgets" not future plugins
- Codex review: Resolve MouseArea+Drag vs DragHandler before Phase 07 implementation (Qt 6.4 compat)
- Codex review: Do not ship dense grid without Pi touch validation
- 04-01: Grid density constants = 150px cellW, 125px cellH (produces 6x4 for 1024x600, 5x3 for 800x480)
- 04-01: 1280x720 maps to 8x4 grid (not 7x4 as research estimated)
- 04-01: Legacy WidgetPlacement/PageDescriptor kept temporarily -- Plan 02 removes them
- 04-01: WidgetPickerModel.filterForSize -> filterByAvailableSpace
- 04-02: Occupancy grid stores instanceId per cell (not bool) for collision resolution during clamping
- 04-02: YAML schema v2 uses snake_case keys (instance_id, col_span) for consistency
- 04-02: QML WidgetPlacementModel context property removed -- Phase 05 handles QML migration
- 04-02: Phase 06 stubs registered with empty qmlComponent, excluded from picker
- 05-01: Inline delegate in HomeMenu (not separate GridCell.qml) -- simple enough, avoids SKIP_CACHEGEN
- 05-01: Blank canvas on fresh install per user decision -- GRID-06 deferred to post Phase 06/07
- 05-02: Clock date format "MMMM d" (no year) -- irrelevant on car display
- 05-02: AAStatusWidget ColumnLayout for both tiers (icon size changes, not layout type)
- 05-02: NowPlaying compact strip omits artist entirely -- too tight at 2x1

### Pending Todos

None yet.

### Roadmap Evolution

- Phase 05.1 inserted after Phase 05: Fix AA wireless connection failure (URGENT)

### Blockers/Concerns

- Nav turn events are logged but not wired to EventBus -- must fix before nav widget (Phase 06)
- AA media metadata exists in orchestrator but is discarded -- must surface via MediaDataBridge (Phase 06)
- New widget QML files MUST set QT_QML_SKIP_CACHEGEN (known gotcha from v0.5.2)
- Use ghost rectangle for resize preview in edit mode -- animating width/height is janky on Pi
- Grid density formula needs real Pi touch validation before shipping

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 1 | Fix AA wireless connection failure | 2026-03-12 | config-only | [1-fix-the-aa-wireless-connection-failure](./quick/1-fix-the-aa-wireless-connection-failure/) |

## Session Continuity

Last session: 2026-03-12
Stopped at: Phase 06 code complete, hostapd blocking Pi AA verification

### Session Handoff (2026-03-12)

**What was done:**
- Plans 06-01 (NavigationDataBridge + ManeuverIconProvider) and 06-02 (MediaDataBridge) — committed, tested, summarized
- Plan 06-03 Task 1 (QML widgets + main.cpp wiring) — committed
- Added long-press widget picker to HomeMenu grid (was missing from Phase 05 grid rewrite) — committed
- Widget picker confirmed working on Pi touchscreen (long-press → pick → place works)
- YamlConfig tcp_port default was 5277, changed to 5288 in Pi config — but AA still won't connect

**BLOCKER: hostapd not fully starting**
- `hostapd` systemd says "active" but never reaches ENABLED state
- Stuck at `UNINITIALIZED->COUNTRY_UPDATE` — never progresses
- `hostapd_cli` can't connect (control socket missing)
- Phone BT-connects, gets WiFi creds, but can't actually join AP (no clients in `hostapd_cli all_sta`)
- Reboot did NOT fix it
- Config: channel=36, hw_mode=a, country_code=US, ht_capab=[HT40+][SHORT-GI-20][SHORT-GI-40]
- This is likely the same class of issue as the quick-fix session but on a fresh Trixie install
- **Debug next:** Run `sudo hostapd -dd /etc/hostapd/hostapd.conf` manually to see the actual error. Check `rfkill list`, `dmesg | grep -i wifi`, regulatory domain issues.

**What needs Pi verification (once hostapd fixed):**
1. Navigation widget: inactive placeholder visible, shows turn data when AA nav active
2. Now Playing widget: unified AA+BT, source switching, playback controls
3. Both widgets at various grid sizes (2x1, 3x1, 3x2)
4. Old BT Now Playing widget gone from picker

**Commits this session:**
- e3cdfc1 feat(06-01): add NavigationDataBridge and ManeuverIconProvider
- 5a14fb5 feat(06-02): add MediaDataBridge with AA/BT source priority
- 1d65927 docs(06): wave 1 complete
- 23fe9f3 feat(06-03): wire navigation and now-playing content widgets
- e5d8c51 feat(06-03): add long-press widget picker to home grid

**YamlConfig default port mismatch:** `YamlConfig.cpp` defaults tcp_port to 5277, `Configuration.hpp` defaults to 5288. Should align these — 5288 is what works.
