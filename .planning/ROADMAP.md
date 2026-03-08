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

## v0.5.0 Protocol Compliance

**Milestone Goal:** Align Prodigy's AA protocol implementation with verified open-android-auto proto definitions -- handle all documented channel messages, wire HU-initiated commands, and complete authentication flows.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [x] **Phase 1: Proto Foundation & Observability** - Update submodule to v1.0 and add debug logging for all unhandled channel messages
- [x] **Phase 2: Navigation & Audio Channels** - Handle inbound navigation events and per-channel audio state messages from phone
- [x] **Phase 3: Commands & Authentication** - Wire HU-initiated media/voice commands, BT auth exchange, and haptic feedback
- [x] **Phase 4: OAA Proto Compliance & Library Rename** - Remove retracted proto dead code, fix WiFi bssid semantics, rename protocol library

## Phase Details

### Phase 1: Proto Foundation & Observability
**Goal**: Prodigy builds against v1.0 proto definitions and makes all unhandled protocol traffic visible in debug logs
**Depends on**: Nothing (first phase)
**Requirements**: SUB-01, OBS-01
**Success Criteria** (what must be TRUE):
  1. Project compiles and all existing tests pass with open-android-auto v1.0 submodule
  2. Connecting a phone and navigating produces debug log lines showing previously-silent message IDs, channels, and hex payloads
  3. No existing functionality regresses (video, touch, audio all still work on Pi)
**Plans:** 2 plans

Plans:
- [x] 01-01-PLAN.md -- Update proto submodule to v1.0 and fix compilation breaks
- [x] 01-02-PLAN.md -- Add unhandled message debug logging for protocol observability

### Phase 2: Navigation & Audio Channels
**Goal**: Prodigy correctly receives and processes navigation guidance from the phone (turn events, nav state, step/distance data)
**Depends on**: Phase 1
**Requirements**: NAV-01
**Success Criteria** (what must be TRUE):
  1. During active navigation, HU logs parsed NavigationTurnEvent data (turn type, road name, distance) at ~1Hz
  2. Nav state, step, and distance messages are received and logged
**Plans:** 2 plans

*Note: NAV-02 (NavigationFocusIndication), AUD-01 (AudioFocusState), and AUD-02 (AudioStreamType) were implemented in Phases 2-3 but retracted after v1.2 proto verification revealed misidentified messages. Dead code removed in Phase 4.*

Plans:
- [x] 02-01-PLAN.md -- NavigationTurnEvent parsing, notification enhancement, and NavigationFocusIndication handling
- [x] 02-02-PLAN.md -- AudioFocusState and AudioStreamType per-channel handling

### Phase 3: Commands & Authentication
**Goal**: Prodigy can send commands to the phone (voice activation via keycodes) and complete the BT authentication exchange
**Depends on**: Phase 1
**Requirements**: MED-02, BT-01, INP-01
**Success Criteria** (what must be TRUE):
  1. Pressing a voice button sends VoiceSessionRequest START and Google Assistant activates on the phone; releasing sends STOP
  2. BT pairing exchanges BluetoothAuthenticationData (0x8003) and processes BluetoothAuthenticationResult (0x8004) without errors
  3. InputBindingNotification haptic feedback requests from the phone are received and logged (ready for future haptic output)
**Plans:** 2 plans

*Note: MED-01 (MediaPlaybackCommand) was implemented but retracted after v1.2 proto verification revealed it was a misidentified message (vuy = ActionTakenNotification). Keycode-based media control is the v1.2-correct approach (deferred to separate phase for Pi verification).*

Plans:
- [x] 03-01-PLAN.md -- MediaPlaybackCommand and VoiceSessionRequest outbound commands
- [x] 03-02-PLAN.md -- BT auth data exchange and InputBindingNotification haptic handling

### Phase 4: OAA Proto Compliance & Library Rename
**Goal**: Clean up retracted proto dead code, fix WiFi bssid semantics, rename protocol library to prodigy-oaa-protocol, and ensure all tests pass
**Depends on**: Phase 3
**Requirements**: CLN-01, CLN-02, REN-01, TST-01
**Success Criteria** (what must be TRUE):
  1. No retracted handler stubs, dead signals, or RETRACTED comments remain in source code
  2. WiFi SDP descriptor sends actual BSSID (MAC address), not SSID
  3. Protocol library is at libs/prodigy-oaa-protocol/ with matching CMake target name
  4. All 64 tests pass (100% pass rate)
  5. ROADMAP and REQUIREMENTS accurately reflect v1.2 proto reality
**Plans:** 2 plans

Plans:
- [x] 04-01-PLAN.md -- Dead code cleanup and WiFi bssid fix
- [x] 04-02-PLAN.md -- Library rename, test fixes, and documentation update

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Proto Foundation & Observability | 2/2 | Complete | 2026-03-06 |
| 2. Navigation & Audio Channels | 2/2 | Complete | 2026-03-06 |
| 3. Commands & Authentication | 2/2 | Complete | 2026-03-06 |
| 4. OAA Proto Compliance & Library Rename | 2/2 | Complete | 2026-03-08 |

---
*Last updated: 2026-03-08 -- v0.5.0 milestone complete*
