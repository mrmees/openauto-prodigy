# Phase 3: Audio Features - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

HFP phone call audio independence from the AA session lifecycle, plus a parametric EQ system with built-in presets, custom user profiles, and web config panel editing. EQ applies to music/media streams only (not navigation or phone call audio).

</domain>

<decisions>
## Implementation Decisions

### EQ Band Design
- 5-band parametric EQ with fixed center frequencies on the head unit (classic car audio layout: sub-bass, bass, mid, upper-mid, treble)
- Web config panel exposes full parametric control per band: adjustable center frequency, gain, and Q factor (bandwidth)
- Gain range: ±12dB per band (standard car audio, avoids clipping risk)
- Built-in presets include both car-specific AND genre-based (~10 total): Flat, Bass Boost, Vocal/Talk, Road Noise, Loudness, Rock, Pop, Jazz, Classical, Hip-Hop, Electronic
- EQ profiles importable/exportable as JSON via the web config panel for community sharing

### Call/AA Interaction
- When AA disconnects during an active HFP call, seamlessly switch to PhonePlugin's call UI — user sees active call screen with hangup button, no jarring transition
- AA media ducks to ~20% volume with ~300ms fade when HFP call starts, restores when call ends
- After a mid-call AA disconnect, auto-reconnect AA once the call ends (don't attempt during the call — BT/WiFi handshake could disrupt call audio)

### EQ Scope & Routing
- EQ processing via PipeWire filter-chain module (not in-app DSP) — zero custom DSP code, industry standard on Linux
- EQ applies to all music/media streams (both AA media and BT A2DP) — consistent sound regardless of source
- Navigation and phone call audio bypass EQ entirely
- EQ changes apply live (moving a slider immediately changes the sound) — no save-then-apply on head unit
- Installer sets up PipeWire filter-chain config automatically — EQ works out of the box

### Web Config EQ Editor
- Interactive frequency response curve visualization (20Hz–20kHz graph with draggable band nodes)
- Full parametric per band: frequency, gain, and Q factor — head unit stays simple 5-band gain-only
- Changes apply via save button (existing IPC JSON request/response) — not real-time WebSocket
- JSON import/export for EQ profiles

### Claude's Discretion
- Exact center frequencies for the 5 fixed bands
- Navigation audio behavior during phone calls (mute vs duck)
- PipeWire filter-chain plugin selection (LSP, built-in, etc.)
- Frequency response curve rendering approach in web panel (Canvas, SVG, etc.)
- Fade curve shape for ducking (linear, exponential, etc.)

</decisions>

<specifics>
## Specific Ideas

- Head unit EQ should feel like a car stereo — simple gain sliders, instant feedback, no complexity
- Web panel EQ should feel like a pro audio tool — interactive curve, full parametric, the payoff for using a browser on a bigger screen
- The two interfaces control the same underlying EQ but at different levels of detail

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `AudioService` (src/core/services/AudioService.hpp): Already has `AudioFocusType` enum with Gain/GainTransient/GainTransientMayDuck/Loss types, plus `requestAudioFocus()`/`releaseAudioFocus()` — ducking plumbing exists but isn't wired to HFP
- `PhonePlugin` (src/plugins/phone/PhonePlugin.hpp): Tracks `CallState` (Idle/Ringing/Active/Held) via BlueZ D-Bus — call state detection is already implemented
- `IpcServer` (src/core/services/IpcServer.cpp): JSON request/response protocol over Unix socket — web config panel communication already works for settings/themes
- `PipeWireDeviceRegistry` (src/core/audio/PipeWireDeviceRegistry.cpp): Hot-plug device enumeration — could be extended for filter-chain management
- `AudioDeviceModel` (src/ui/AudioDeviceModel.hpp): QAbstractListModel for device ComboBoxes — pattern for EQ preset picker model
- `web-config/server.py`: Flask web config with existing routes for dashboard, settings, themes, plugins — EQ route follows same pattern

### Established Patterns
- PipeWire streams are already per-type (media, navigation, phone) — EQ routing per stream type is architecturally natural
- YAML config with deep merge (YamlConfig) — EQ profiles fit naturally as config entries
- Qt signals/slots for inter-component communication — EQ changes propagate via signals
- Q_INVOKABLE methods for QML-to-C++ calls — EQ slider interaction follows existing pattern

### Integration Points
- AudioService needs ducking logic wired to PhonePlugin's callStateChanged signal
- PipeWire filter-chain config lives in ~/.config/pipewire/ — installer writes it, app reloads it
- IpcServer needs new EQ routes (get/set profiles, list presets)
- web-config/server.py needs new EQ page with frequency response canvas
- install.sh needs PipeWire filter-chain setup step

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-audio-features*
*Context gathered: 2026-03-01*
