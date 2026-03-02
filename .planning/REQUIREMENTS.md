# Requirements: OpenAuto Prodigy

**Defined:** 2026-03-01
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.4.1 Requirements

Requirements for audio equalizer milestone. Each maps to roadmap phases.

### DSP Core

- [x] **DSP-01**: User can hear EQ-processed audio through a 10-band graphic equalizer (31Hz-16kHz ISO frequencies)
- [x] **DSP-02**: User experiences smooth audio transitions when switching EQ presets (coefficient interpolation)
- [x] **DSP-03**: User can bypass EQ processing entirely with a single toggle

### Presets

- [x] **PRST-01**: User can select from 8 bundled presets (Flat, Rock, Pop, Jazz, Classical, Bass Boost, Treble Boost, Vocal)
- [x] **PRST-02**: User can apply different EQ profiles to media, navigation, and phone audio streams independently
- [x] **PRST-03**: User can save custom EQ settings as a named preset
- [x] **PRST-04**: User can load and delete user-created presets

### Head Unit UI

- [ ] **UI-01**: User can adjust 10 EQ band gains via vertical sliders on the head unit touchscreen
- [ ] **UI-02**: User can select presets from a picker in the EQ settings view
- [ ] **UI-03**: User can switch between media, navigation, and phone EQ profiles via stream selector

### Config

- [ ] **CFG-01**: User's EQ settings and presets persist across app restarts via YAML config

## Future Requirements

### Web Config Panel EQ

- **WEB-01**: User can adjust EQ bands from the web config panel
- **WEB-02**: User can manage presets from the web config panel
- **WEB-03**: User can assign presets to streams from the web config panel

## Out of Scope

| Feature | Reason |
|---------|--------|
| Parametric EQ (adjustable Q/frequency) | Too complex for touchscreen; fixed-band covers car audio needs |
| Spectrum analyzer / visualizer | Visual complexity, CPU cost on Pi 4, not core EQ value |
| Auto-EQ / room correction | Requires measurement hardware and calibration flow |
| Per-channel EQ (left/right) | Car audio balance is handled at amp/receiver level |
| BT Audio plugin EQ | BT A2DP audio is decoded by phone; EQ at source is more appropriate |
| PipeWire filter-chain approach | Adds latency, breaks rate matching PI controller, no runtime parameter API |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| DSP-01 | Phase 1 | Complete |
| DSP-02 | Phase 1 | Complete |
| DSP-03 | Phase 1 | Complete |
| PRST-01 | Phase 2 | Complete |
| PRST-02 | Phase 2 | Complete |
| PRST-03 | Phase 2 | Complete |
| PRST-04 | Phase 2 | Complete |
| UI-01 | Phase 3 | Pending |
| UI-02 | Phase 3 | Pending |
| UI-03 | Phase 3 | Pending |
| CFG-01 | Phase 2 | Pending |

**Coverage:**
- v0.4.1 requirements: 11 total
- Mapped to phases: 11
- Unmapped: 0

---
*Requirements defined: 2026-03-01*
*Last updated: 2026-03-01 after roadmap creation*
