# Requirements: OpenAuto Prodigy

**Defined:** 2026-03-04
**Core Value:** A person with a Raspberry Pi 4 and a touchscreen can install this, pair their phone, and get a reliable wireless Android Auto experience — every time, without SSH.

## v0.5.0 Requirements

Requirements for protocol compliance milestone. Each maps to roadmap phases.

### Submodule

- [x] **SUB-01**: Submodule updated to open-android-auto v1.0 with verified proto definitions

### Navigation

- [ ] **NAV-01**: HU parses NavigationTurnEvent (0x8004) and exposes turn type, road name, distance, and icon data
- [ ] **NAV-02**: HU handles NavigationFocusIndication from phone to complete focus negotiation flow

### Audio

- [ ] **AUD-01**: HU receives and processes AudioFocusState (0x8021) per-channel focus indicator
- [ ] **AUD-02**: HU receives and processes AudioStreamType (0x8022) per-channel stream type

### Media & Voice

- [ ] **MED-01**: HU can send MediaPlaybackCommand (PAUSE/RESUME) to phone
- [ ] **MED-02**: HU sends VoiceSessionRequest START on voice button press and STOP on release

### Bluetooth

- [ ] **BT-01**: HU exchanges BluetoothAuthenticationData (0x8003) and handles BluetoothAuthenticationResult (0x8004) during pairing

### Input

- [ ] **INP-01**: HU receives InputBindingNotification (0x8004) haptic feedback requests from phone

### Protocol Observability

- [x] **OBS-01**: All unhandled channel messages are logged at debug level with message ID, channel, and hex payload

## Future Requirements

### Optional Capabilities

- **CAR-01**: Car control channel (VHAL vehicle property access)
- **RAD-01**: Radio channel (broadcast radio tuner control)
- **DISP-01**: Multi-display routing (cluster, auxiliary)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Car control channel implementation | New capability, not a compliance gap — log only for now |
| Radio channel implementation | New capability, not a compliance gap — log only for now |
| Multi-display support | Requires hardware not available; log display routing messages |
| Proto schema changes | Submodule is read-only; corrections go to open-android-auto repo |
| CoolWalk layout control | Phone-side only; HU influences via DPI/resolution in SDP |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| SUB-01 | Phase 1 | Complete |
| OBS-01 | Phase 1 | Complete |
| NAV-01 | Phase 2 | Pending |
| NAV-02 | Phase 2 | Pending |
| AUD-01 | Phase 2 | Pending |
| AUD-02 | Phase 2 | Pending |
| MED-01 | Phase 3 | Pending |
| MED-02 | Phase 3 | Pending |
| BT-01 | Phase 3 | Pending |
| INP-01 | Phase 3 | Pending |

**Coverage:**
- v0.5.0 requirements: 10 total
- Mapped to phases: 10
- Unmapped: 0

---
*Requirements defined: 2026-03-04*
*Last updated: 2026-03-05 after roadmap creation*
