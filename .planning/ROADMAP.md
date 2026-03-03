# Roadmap: OpenAuto Prodigy

## Milestones

<details>
<summary>v0.4 Logging & Theming (Phases 1-2) - SHIPPED 2026-03-01</summary>

See .planning/milestones/v0.4/ for archived details.

</details>

<details>
<summary>v0.4.1 Audio Equalizer (Phases 1-3) - SHIPPED 2026-03-02</summary>

See .planning/milestones/v0.4.1/ for archived details.

</details>

<details>
<summary>v0.4.2 Service Hardening (Phases 1-3) - SHIPPED 2026-03-02</summary>

See .planning/milestones/v0.4.2/ for archived details.

</details>

<details>
<summary>v0.4.3 Interface Polish & Settings Reorganization (Phases 1-4) - SHIPPED 2026-03-03</summary>

See .planning/milestones/v0.4.3/ for archived details.

</details>

## v0.4.4 Scalable UI (In Progress)

**Milestone Goal:** Make the entire QML UI resolution-independent, scaling correctly from 800x480 through ultrawide displays. User owns display config; app adapts automatically.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: Config Overrides** - YAML config schema for UI scale, font scale, and per-token overrides so users can always tune their setup
- [x] **Phase 2: UiMetrics Foundation + Touch Pipeline** - Unclamped scale formula, new tokens, C++ sidebar hit zones, display config detection (completed 2026-03-03)
- [ ] **Phase 3: QML Hardcoded Island Audit** - Replace all hardcoded pixel values across 9+ QML files with UiMetrics tokens
- [ ] **Phase 4: Layout Adaptation + Validation** - Grid overflow fixes, EQ guard, full resolution validation sweep
- [ ] **Phase 5: Runtime Adaptation** - Auto-detect resolution, dynamic resize response, remove manual display config

## Phase Details

### Phase 1: Config Overrides
**Goal**: Users have YAML config knobs to override any auto-derived scaling value, so the app always works even on unusual displays
**Depends on**: Nothing (first phase)
**Requirements**: CFG-01, CFG-02, CFG-03, CFG-04
**Success Criteria** (what must be TRUE):
  1. Setting `ui.scale: 1.2` in config.yaml produces a globally 20% larger UI
  2. Setting `ui.fontScale: 0.9` reduces all font sizes independently of layout scale
  3. Setting individual token overrides (e.g. `ui.tokens.rowH: 48`) overrides that specific value
  4. Config overrides are applied after auto-derived values (user always wins over auto-detection)
**Plans**: 1 plan

Plans:
- [ ] 01-01-PLAN.md — Config defaults, UiMetrics override precedence chain, startup logging

### Phase 2: UiMetrics Foundation + Touch Pipeline
**Goal**: The scale system produces correct sizing at any resolution, and touch works accurately on non-1024x600 displays
**Depends on**: Phase 1
**Requirements**: SCALE-01, SCALE-02, SCALE-03, SCALE-04, SCALE-05, TOUCH-01, TOUCH-02
**Success Criteria** (what must be TRUE):
  1. App launched at 800x480 shows visibly smaller UI elements than at 1024x600 (no artificial clamping)
  2. NormalText and SpecialText render at proportionally correct sizes across resolutions (pixel floors prevent illegibility)
  3. Sidebar touch zones on AA view align with visual sidebar elements at 800x480 (taps hit the right buttons)
  4. UiMetrics scale is derived from window dimensions, not Screen.* (verified by startup log on Pi)
**Plans**: 2 plans

Plans:
- [ ] 02-01-PLAN.md — DisplayInfo C++ bridge, dual-axis UiMetrics rewrite, font floors, new tokens
- [ ] 02-02-PLAN.md — EvdevTouchReader dynamic display dims, proportional sidebar zones, ServiceDiscoveryBuilder wiring

### Phase 3: QML Hardcoded Island Audit
**Goal**: Every QML file uses UiMetrics tokens for all sizing -- zero hardcoded pixel values remain
**Depends on**: Phase 2
**Requirements**: AUDIT-01, AUDIT-02, AUDIT-03, AUDIT-04, AUDIT-05, AUDIT-06, AUDIT-07, AUDIT-08, AUDIT-09, AUDIT-10
**Success Criteria** (what must be TRUE):
  1. GestureOverlay, Sidebar, PhoneView, IncomingCallOverlay, and BtAudioView all scale proportionally at 800x480 (no clipped text, no overflow)
  2. Caller ID text on PhoneView and IncomingCallOverlay is readable at arm's length on 800x480 display
  3. A grep for hardcoded pixelSize/font.pixelSize literals in QML returns zero results (excluding intentional debug overlays)
  4. All controls (tiles, buttons, dialogs) maintain touch-target minimums at 800x480
**Plans**: 3 plans

Plans:
- [ ] 03-01-PLAN.md -- New UiMetrics tokens + shell chrome tokenization (TopBar, BottomBar, NavStrip, NotificationArea)
- [ ] 03-02-PLAN.md -- Plugin view tokenization (Sidebar, GestureOverlay, PhoneView, IncomingCallOverlay, BtAudioView)
- [ ] 03-03-PLAN.md -- Controls, dialogs, remaining files + final grep verification (AUDIT-10)

### Phase 4: Layout Adaptation + Validation
**Goal**: Grid layouts adapt to available space without overflow or clipping, validated at all target resolutions
**Depends on**: Phase 3
**Requirements**: LAYOUT-01, LAYOUT-02, LAYOUT-03
**Success Criteria** (what must be TRUE):
  1. LauncherMenu tile grid displays all tiles without horizontal overflow at 800x480
  2. Settings tile grid displays all 6 category tiles without clipping at 800x480
  3. EQ band sliders are usable (draggable, visually distinct) at 800x480
  4. Full UI is verified functional at 800x480, 1024x600, and 1280x720 (or simulated equivalent)
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

### Phase 5: Runtime Adaptation
**Goal**: App detects and adapts to whatever display the user has configured -- no manual display config in the app
**Depends on**: Phase 2
**Requirements**: ADAPT-01, ADAPT-02, ADAPT-03, ADAPT-04
**Success Criteria** (what must be TRUE):
  1. App launched on 800x480 display auto-detects resolution and scales correctly without any config changes
  2. Swapping displays (e.g. DFRobot -> Pi official) produces correct scaling on next app launch without manual intervention
  3. Portrait/landscape orientation setting removed from Display settings -- app derives orientation from window dimensions
  4. Brightness auto-adapts to display hardware (sysfs backlight on Pi official, software fallback on DFRobot)
**Plans**: TBD

Plans:
- [ ] 05-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Config Overrides | 1/1 | Complete | 2026-03-03 |
| 2. UiMetrics Foundation + Touch Pipeline | 2/2 | Complete    | 2026-03-03 |
| 3. QML Hardcoded Island Audit | 3/3 | Complete | 2026-03-03 |
| 4. Layout Adaptation + Validation | 0/0 | Not started | - |
| 5. Runtime Adaptation | 0/0 | Not started | - |

---
*Last updated: 2026-03-03 -- Phase 3 complete (all QML files tokenized)*
