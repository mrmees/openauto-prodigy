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

### v0.4.5 Navbar Rework (In Progress)

**Milestone Goal:** Replace TopBar, NavStrip, AA sidebar overlay, and GestureOverlay with a single configurable navbar backed by a zone-based evdev touch routing layer. Build bottom-up: touch routing foundation first (everything depends on it), then navbar controls and QML, then AA integration with on-device validation, then cleanup of old UI.

## Phases

**Phase Numbering:**
- Each milestone starts at Phase 1.
- Decimal phases (1.1, 1.2): Urgent insertions if needed (marked with INSERTED)

- [ ] **Phase 1: Touch Routing** - Zone-based evdev dispatch layer replacing hardcoded sidebar logic
- [ ] **Phase 2: Navbar** - 3-control navbar with gestures, edge positioning, popups, and QML visuals
- [ ] **Phase 3: AA Integration** - Viewport margins, touch exclusion, and on-device validation with real AA sessions
- [ ] **Phase 4: Cleanup** - Remove TopBar, NavStrip, sidebar overlay, and migrate remaining actions

## Phase Details

### Phase 1: Touch Routing
**Goal**: Touches during AA are dispatched through a zone registry instead of hardcoded sidebar hit-tests
**Depends on**: Nothing (first phase of v0.4.5)
**Requirements**: TOUCH-01, TOUCH-02, TOUCH-03, TOUCH-04, TOUCH-05
**Success Criteria** (what must be TRUE):
  1. TouchRouter dispatches evdev touches to registered zones by priority, and zones can be added/removed at runtime
  2. A finger that starts in one zone stays routed to that zone for all MOVE/UP events regardless of where it drags
  3. QML components report their pixel geometry to C++, and EvdevCoordBridge converts to evdev space -- no duplicated layout math
  4. EvdevTouchReader uses TouchRouter for all dispatch -- hardcoded sidebar hit-test logic is replaced -- and existing sidebar behavior is preserved
  5. QML MouseArea input works in launcher mode; evdev zone dispatch works during AA (EVIOCGRAB active)

Plans:
- [ ] 01-01: TouchRouter + EvdevCoordBridge (pure logic + unit tests)
- [ ] 01-02: EvdevTouchReader integration (replace sidebar dispatch, preserve behavior)

### Phase 2: Navbar
**Goal**: Users interact with a 3-control navbar (volume/clock-home/brightness) that supports tap, short-hold-release, and long-hold gestures on any screen edge
**Depends on**: Phase 1
**Requirements**: NAV-01, NAV-02, NAV-03, NAV-04, NAV-05, NAV-06, NAV-07, EDGE-01, EDGE-02, EDGE-03
**Success Criteria** (what must be TRUE):
  1. Navbar displays volume, clock/home, and brightness controls -- volume on driver side, brightness on passenger side, swapped when LHD/RHD config changes
  2. Tap on volume shows a popup slider; short-hold-release opens EQ; long-hold mutes. Same gesture pattern for brightness (slider/display settings/night mode) and clock (home/settings/power menu)
  3. Navbar renders horizontally on top/bottom edges, vertically on left/right edges, and touch zones update automatically when edge position changes
  4. Popup sliders dismiss on outside tap and do not leak taps to underlying views

Plans:
- [ ] 02-01: NavbarController + gesture recognition (C++ state machine, zone registration, testable without QML)
- [ ] 02-02: Navbar QML + popups (visual components, edge positioning, geometry reporting, Shell integration)

### Phase 3: AA Integration
**Goal**: Navbar works correctly during active AA sessions -- visible with reserved viewport space, touch correctly routed between navbar and phone
**Depends on**: Phase 2
**Requirements**: AA-01, AA-02, AA-03, AA-04, AA-05, AA-06
**Success Criteria** (what must be TRUE):
  1. "Show Navbar during AA" toggle exists in settings and persists across restarts
  2. When navbar is visible during AA, the phone renders in a sub-region with margins matching navbar size -- no content hidden behind navbar
  3. Touches in the navbar zone during AA are consumed by navbar controls (not forwarded to phone); touches outside navbar forward to phone normally
  4. Gesture overlay touch passthrough bug is fixed -- controls in overlay zones respond to touch during AA
  5. Sidebar overlay is fully replaced by navbar -- there is no separate sidebar component active during AA

Plans:
- [ ] 03-01: AA margin calculation + settings toggle + on-device validation

### Phase 4: Cleanup
**Goal**: Old navigation UI is removed -- the codebase has one navigation system, not two
**Depends on**: Phase 3
**Requirements**: CLEAN-01, CLEAN-02, CLEAN-03, CLEAN-04
**Success Criteria** (what must be TRUE):
  1. TopBar component is removed from Shell.qml and its source files are deleted
  2. Old NavStrip (plugin icon bar) is removed -- all its actions (home, EQ shortcut, back, day/night) have confirmed new homes in navbar or launcher
  3. Sidebar overlay component and its evdev hit zone code are removed
  4. Clock display is integrated into navbar center control -- no separate clock component remains in the shell

Plans:
- [ ] 04-01: Remove old UI components and verify action migration

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Touch Routing | 0/2 | Not started | - |
| 2. Navbar | 0/2 | Not started | - |
| 3. AA Integration | 0/1 | Not started | - |
| 4. Cleanup | 0/1 | Not started | - |

---
*Last updated: 2026-03-03 -- v0.4.5 roadmap created*
