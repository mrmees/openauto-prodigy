# Roadmap: OpenAuto Prodigy v0.4.1 — Audio Equalizer

## Overview

Add a 10-band graphic equalizer to OpenAuto Prodigy. Build bottom-up: RT-safe DSP engine first (unit-testable, no dependencies), then the service layer that manages presets/config/per-stream profiles and wires it into the audio pipeline, then the touch UI on top. Three phases, each delivering a verifiable capability.

## Milestones

<details>
<summary>v0.4 Logging & Theming (Phases 1-2) - SHIPPED 2026-03-01</summary>

See .planning/milestones/v0.4/ for archived details.

</details>

### v0.4.1 Audio Equalizer (In Progress)

**Milestone Goal:** Users can shape their audio with a 10-band graphic EQ, per-stream profiles, bundled and custom presets, all controllable from the head unit touchscreen.

## Phases

- [ ] **Phase 3: DSP Core** - RT-safe 10-band biquad engine with coefficient interpolation, bypass, and unit tests
- [ ] **Phase 4: Service & Config** - EqualizerService with presets, per-stream profiles, AudioService integration, YAML persistence
- [ ] **Phase 5: Head Unit EQ UI** - Touch-friendly QML interface with sliders, preset picker, and stream selector

## Phase Details

### Phase 3: DSP Core
**Goal**: A correct, RT-safe 10-band biquad equalizer engine exists with full unit test coverage, ready to be wired into any audio pipeline
**Depends on**: Nothing (self-contained DSP math)
**Requirements**: DSP-01, DSP-02, DSP-03
**Success Criteria** (what must be TRUE):
  1. Unit tests confirm 10-band biquad filter produces correct frequency response curves (boost/cut at each ISO center frequency)
  2. Coefficient interpolation smoothly transitions between two gain settings without audible clicks (verified by test checking ramp-over-N-samples behavior)
  3. Bypass toggle passes audio through unmodified (bit-exact or within floating-point epsilon)
  4. All processing is lock-free and uses no mutex (atomic coefficient swap only)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD
- [ ] 03-02: TBD

### Phase 4: Service & Config
**Goal**: Users hear EQ-processed audio on all three AA streams with per-stream profiles, bundled presets, user preset save/load, and settings that survive reboot
**Depends on**: Phase 3
**Requirements**: PRST-01, PRST-02, PRST-03, PRST-04, CFG-01
**Success Criteria** (what must be TRUE):
  1. User can select any of 8 bundled presets and hear the audio change in real time
  2. Media, navigation, and phone audio streams each have independent EQ profiles (changing one does not affect the others)
  3. User can save a custom EQ curve as a named preset, and load or delete it later
  4. All EQ settings (active preset per stream, custom presets, band gains) persist across app restart via YAML config
  5. Navigation and phone streams default to Flat/Voice preset on first use (not the user's music EQ)
**Plans**: TBD

Plans:
- [ ] 04-01: TBD
- [ ] 04-02: TBD

### Phase 5: Head Unit EQ UI
**Goal**: Users can see and control the equalizer from the head unit touchscreen with car-friendly touch targets
**Depends on**: Phase 4
**Requirements**: UI-01, UI-02, UI-03
**Success Criteria** (what must be TRUE):
  1. User can drag 10 vertical sliders to adjust individual band gains and hear the change immediately
  2. User can pick a preset from a list and see the sliders snap to that preset's values
  3. User can switch between media, navigation, and phone stream profiles via a visible selector
  4. All controls are usable with a finger on a 1024x600 touchscreen (no precision tapping required)
**Plans**: TBD

Plans:
- [ ] 05-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 3. DSP Core | 0/? | Not started | - |
| 4. Service & Config | 0/? | Not started | - |
| 5. Head Unit EQ UI | 0/? | Not started | - |

---
*Roadmap created: 2026-03-01*
*Last updated: 2026-03-01*
