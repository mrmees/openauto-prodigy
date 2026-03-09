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

<details>
<summary>v0.4.4 Scalable UI (Phases 1-5) - SHIPPED 2026-03-03</summary>

See .planning/milestones/v0.4.4/ for archived details.

</details>

<details>
<summary>v0.4.5 Navbar Rework (Phases 1-4) - SHIPPED 2026-03-05</summary>

See .planning/milestones/v0.4.5/ for archived details.

</details>

<details>
<summary>v0.5.0 Protocol Compliance (Phases 1-4) - SHIPPED 2026-03-08</summary>

See .planning/milestones/v0.5.0-ROADMAP.md for archived details.

</details>

---

## v0.5.1 DPI Sizing & UI Polish

**Milestone Goal:** Make UI sizing physically meaningful via DPI-aware scaling with user control, and polish visual presentation (clock, theme colors, element depth).

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: DPI Foundation** - Installer EDID probe, DPI-based UiMetrics, config persistence with settings UI (completed 2026-03-08)
- [x] **Phase 2: Clock & Scale Control** - Clock readability and 24h toggle, user-facing scale stepper (completed 2026-03-08)
- [ ] **Phase 3: Theme Color System** - Full semantic color palette aligned with Material Design conventions
- [ ] **Phase 4: Visual Depth** - 3D depth effects on buttons and navbar

## Phase Details

### Phase 1: DPI Foundation
**Goal**: UI sizing is derived from the physical screen size, not just pixel resolution
**Depends on**: Nothing (first phase)
**Requirements**: DPI-01, DPI-02, DPI-03, DPI-04
**Success Criteria** (what must be TRUE):
  1. Running the installer on a Pi with EDID-capable display shows detected screen size and lets the user confirm or override it
  2. A fresh install with no EDID and user skipping entry defaults to 7" and produces correctly scaled UI
  3. UiMetrics-driven element sizes change when the configured screen size changes (e.g., 7" vs 10" produces visibly different control sizes at the same resolution)
  4. User can view screen size and computed DPI in Display settings (read-only; editable via YAML config for advanced users)
**Plans:** 3/3 plans complete
Plans:
- [x] 01-01-PLAN.md -- DisplayInfo DPI properties + config default + unit tests
- [x] 01-02-PLAN.md -- UiMetrics DPI-based scaling + settings UI read-only display
- [x] 01-03-PLAN.md -- Installer EDID probe + screen size prompt + config persistence

### Phase 2: Clock & Scale Control
**Goal**: Clock is readable at glance distance, and user can fine-tune overall UI scale
**Depends on**: Phase 1
**Requirements**: CLK-01, CLK-02, CLK-03, DPI-05
**Success Criteria** (what must be TRUE):
  1. Clock text in the navbar is noticeably larger than current and readable from arm's length in a car
  2. Clock shows bare time without AM/PM suffix in 12h mode
  3. User can toggle between 12h and 24h format in settings, and the navbar clock updates immediately
  4. User can adjust UI scale via stepper in Display settings (increments of 0.1), and all UI elements resize live or on confirm
**Plans:** 2/2 plans complete
Plans:
- [x] 02-01-PLAN.md -- Config plumbing, UiMetrics reactivity fix, clock sizing and 24h toggle
- [x] 02-02-PLAN.md -- Scale stepper control with safety revert overlay

### Phase 3: Theme Color System
**Goal**: All UI elements draw from a complete, consistent semantic color palette
**Depends on**: Phase 1
**Requirements**: THM-01, THM-02, THM-03
**Success Criteria** (what must be TRUE):
  1. ThemeService exposes a full set of named semantic color roles (surface, primary, secondary, accent, error, on-surface, on-primary, etc.) as Q_PROPERTYs
  2. Switching between day and night themes produces a coherent, non-clashing color scheme across all screens (launcher, settings, AA overlay, BT audio, phone)
  3. No hardcoded color hex values remain in any QML file -- all colors reference ThemeService properties
**Plans:** 2/3 plans executed
Plans:
- [x] 03-01-PLAN.md -- ThemeService property rename to AA tokens + derived colors + YAML migration
- [x] 03-02-PLAN.md -- QML hardcode elimination + ThemeService reference update + web config/IPC
- [ ] 03-03-PLAN.md -- Connected Device theme + UiConfigRequest protocol integration

### Phase 03.4: Fix Android Auto rendering (INSERTED)

**Goal:** Fix AA video rendering so PreserveAspectFit replaces PreserveAspectCrop, making all content visible regardless of whether the phone honors SDP margin settings
**Requirements**: FIX-01, FIX-02, FIX-03
**Depends on:** Phase 3
**Success Criteria** (what must be TRUE):
  1. AA video content is fully visible with no cropping when navbar is shown
  2. Touch targets align with visible AA content
  3. Existing SDP margin calculation tests still pass
**Plans:** 1 plan

Plans:
- [ ] 03.4-01-PLAN.md -- Change VideoOutput fillMode to PreserveAspectFit + verify on Pi

### Phase 03.5: Navbar Status Indicators (INSERTED)

**Goal:** Add battery and signal strength indicators to the navbar, remove them from AA status bar via protocol flags
**Requirements**: NAV-01, NAV-02, NAV-03, NAV-04, NAV-05, NAV-06
**Depends on:** Phase 03.4
**Success Criteria** (what must be TRUE):
  1. Phone battery level and signal strength are extracted from AA protocol and exposed as Q_PROPERTYs on the orchestrator
  2. Battery and signal icons flank the clock in the navbar center zone during AA connection
  3. Indicators disappear immediately on AA disconnect (no stale data)
  4. SDP tells phone to hide its own clock, battery, and signal when navbar is visible during AA
  5. Navbar zone proportions updated to 15/70/15 (volume/center/brightness)
  6. Battery icon turns red below 20%; indicators work in horizontal and vertical orientations
**Plans:** 1/2 plans executed

Plans:
- [ ] 03.5-01-PLAN.md -- Protocol data extraction + orchestrator Q_PROPERTYs + SDP hidden_ui_elements + tests
- [ ] 03.5-02-PLAN.md -- QML navbar layout + indicator icons + visual checkpoint

### Phase 03.3: M3 Color Role Mapping Fix (INSERTED)

**Goal:** Remap all QML color references from legacy AA custom tokens to semantically correct M3 roles, apply surface container hierarchy, and adopt M3 interaction patterns for pressed/toggle states
**Requirements**: M3R-01, M3R-02, M3R-03, M3R-04, M3R-05
**Depends on:** Phase 03.2
**Success Criteria** (what must be TRUE):
  1. All custom/legacy AA token Q_PROPERTYs (textPrimary, textSecondary, red, onRed, yellow, onYellow, pressed) removed from ThemeService
  2. All QML color references use M3 standard role names (onSurface, onSurfaceVariant, error, onError, tertiary)
  3. Surface container hierarchy applied -- UI elements use appropriate elevation levels (Lowest/Low/Container/High/Highest)
  4. Button pressed states use primaryContainer/onPrimaryContainer (M3 contained button pattern)
  5. Toggle/segmented selected states use secondaryContainer/onSecondaryContainer
**Plans:** 2/2 plans complete

Plans:
- [ ] 03.3-01-PLAN.md -- Token removal (C++ Q_PROPERTYs + YAML cleanup) + bulk QML rename to M3 equivalents
- [ ] 03.3-02-PLAN.md -- Surface container hierarchy + pressed/toggle state M3 patterns + visual verification

### Phase 03.1: get AA theme information working from stream (INSERTED)

**Goal:** Flip UiConfigRequest direction to receive phone's Material You palette, apply/cache tokens to ThemeService, and acknowledge with ACCEPTED response
**Requirements**: AAT-01, AAT-02, AAT-03, AAT-04
**Depends on:** Phase 3
**Success Criteria** (what must be TRUE):
  1. Phone-sent UiConfigRequest (0x8011) is parsed into day/night token maps by VideoChannelHandler
  2. UpdateHuUiConfigResponse (0x8012) ACCEPTED is sent back to the phone
  3. Tokens flow through signal chain to ThemeService and are applied (if connected-device active) or cached (always)
  4. All theme keys use hyphens matching the AA wire token format
  5. Outbound sendUiConfigRequest is completely removed
**Plans:** 2/2 plans complete

Plans:
- [x] 03.1-01-PLAN.md -- Key format migration (underscore->hyphen) + applyAATokens dual-map signature + always-cache + tests
- [x] 03.1-02-PLAN.md -- Protocol direction flip (inbound 0x8011 handler + 0x8012 response) + orchestrator wiring + submodule bump

### Phase 03.2: Companion Theme Import (INSERTED)

**Goal:** Receive M3 color palettes and wallpapers from companion app over TCP, expand ThemeService to full 34-role M3 palette, auto-apply companion themes with wallpaper
**Requirements**: CTI-01, CTI-02, CTI-03, CTI-04, CTI-05, CTI-06
**Depends on:** Phase 3.1
**Success Criteria** (what must be TRUE):
  1. ThemeService exposes all 34 M3 color roles as Q_PROPERTYs accessible from QML
  2. All bundled themes specify all 34 M3 roles in both day and night maps
  3. Companion app sends M3 palette + wallpaper over TCP, HU receives, applies as named theme, sends ack
  4. AA wire token path (0x8011/0x8012) is disabled; companion is sole theme source
  5. Companion themes appear in theme picker; users can delete them but not bundled themes
  6. hello_ack contains display resolution for companion wallpaper cropping
**Plans:** 3 plans

Plans:
- [x] 03.2-01-PLAN.md -- ThemeService 34 M3 role expansion + bundled theme updates + import/delete methods
- [ ] 03.2-02-PLAN.md -- Companion protocol handler + display info + AA wire disable
- [ ] 03.2-03-PLAN.md -- Theme deletion UI + settings integration + human verification

### Phase 4: Visual Depth
**Goal**: Buttons and navbar have subtle physical depth that makes the UI feel polished
**Depends on**: Phase 3
**Requirements**: STY-01, STY-02
**Success Criteria** (what must be TRUE):
  1. Buttons across the app (settings tiles, launcher tiles, dialog actions) have a visible but subtle 3D effect (shadow, gradient, or bevel) in their resting state
  2. Button press state visibly changes the depth effect (pressed-in appearance) in addition to existing scale+opacity feedback
  3. Navbar has a depth treatment (drop shadow, gradient border, or elevation) that visually separates it from the content area
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 3.1 -> 3.2 -> 3.3 -> 3.4 -> 3.5 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. DPI Foundation | 3/3 | Complete   | 2026-03-08 |
| 2. Clock & Scale Control | 2/2 | Complete   | 2026-03-08 |
| 3. Theme Color System | 2/3 | In Progress|  |
| 3.1 AA Theme Stream | 2/2 | Complete | 2026-03-08 |
| 3.2 Companion Theme Import | 2/3 | In Progress|  |
| 3.3 M3 Color Role Mapping | 2/2 | Complete   | 2026-03-09 |
| 3.4 Fix AA Rendering | 1/1 | Complete | 2026-03-09 |
| 3.5 Navbar Status Indicators | 0/2 | Not started | - |
| 4. Visual Depth | 0/? | Not started | - |

---
*Last updated: 2026-03-09 -- Phase 03.5 planned (navbar status indicators)*
