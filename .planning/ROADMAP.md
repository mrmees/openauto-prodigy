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

**Milestone Goal:** Align Prodigy's AA protocol implementation with verified open-android-auto v1.0 proto definitions — handle all documented channel messages, wire HU-initiated commands, and complete authentication flows.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: Proto Foundation & Observability** - Update submodule to v1.0 and add debug logging for all unhandled channel messages
- [ ] **Phase 2: Navigation & Audio Channels** - Handle inbound navigation events and per-channel audio state messages from phone
- [ ] **Phase 3: Commands & Authentication** - Wire HU-initiated media/voice commands, BT auth exchange, and haptic feedback

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
- [ ] 01-01-PLAN.md -- Update proto submodule to v1.0 and fix compilation breaks
- [ ] 01-02-PLAN.md -- Add unhandled message debug logging for protocol observability

### Phase 2: Navigation & Audio Channels
**Goal**: Prodigy correctly receives and processes navigation guidance and per-channel audio state from the phone
**Depends on**: Phase 1
**Requirements**: NAV-01, NAV-02, AUD-01, AUD-02
**Success Criteria** (what must be TRUE):
  1. During active navigation, HU logs parsed NavigationTurnEvent data (turn type, road name, distance) at ~1Hz
  2. NavigationFocusIndication from phone is acknowledged, completing the focus negotiation handshake
  3. AudioFocusState changes per channel are received and logged (focus gained/lost/transient for media/nav/phone)
  4. AudioStreamType per channel is received and logged, identifying which stream type each channel carries
**Plans:** 2 plans

Plans:
- [ ] 02-01-PLAN.md -- NavigationTurnEvent parsing, notification enhancement, and NavigationFocusIndication handling
- [ ] 02-02-PLAN.md -- AudioFocusState and AudioStreamType per-channel handling

### Phase 3: Commands & Authentication
**Goal**: Prodigy can send commands to the phone (media control, voice activation) and complete the BT authentication exchange
**Depends on**: Phase 1
**Requirements**: MED-01, MED-02, BT-01, INP-01
**Success Criteria** (what must be TRUE):
  1. Pressing a media control (pause/resume) in the HU sends the corresponding MediaPlaybackCommand and the phone responds
  2. Pressing a voice button sends VoiceSessionRequest START and Google Assistant activates on the phone; releasing sends STOP
  3. BT pairing exchanges BluetoothAuthenticationData (0x8003) and processes BluetoothAuthenticationResult (0x8004) without errors
  4. InputBindingNotification haptic feedback requests from the phone are received and logged (ready for future haptic output)
**Plans:** 2 plans

Plans:
- [ ] 03-01-PLAN.md -- MediaPlaybackCommand and VoiceSessionRequest outbound commands
- [ ] 03-02-PLAN.md -- BT auth data exchange and InputBindingNotification haptic handling

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Proto Foundation & Observability | 0/2 | Not started | - |
| 2. Navigation & Audio Channels | 0/2 | Not started | - |
| 3. Commands & Authentication | 0/2 | Not started | - |

---
*Last updated: 2026-03-06 -- Phase 3 plans created*
