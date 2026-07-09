# Android Auto Protocol Reference — Design Document

**Date:** 2026-02-16
**Purpose:** Build a comprehensive, wireless-first Android Auto protocol reference for use in OpenAuto Prodigy development and as a standalone community resource.

## Problem

Claude's training data has general AA knowledge but will get specific message IDs, handshake sequences, BT profile details, and Pi 4 hardware constraints wrong without a verified reference. Google does not publish official protocol documentation — everything comes from reverse engineering and working implementations.

The reference must be:
- **Accurate** — derived from working source code, not secondhand summaries
- **Wireless-first** — USB AA is being deprecated; wireless is the hard problem
- **Hardware-aware** — Pi 4's single BCM43455 radio creates real constraints
- **Session-loadable** — structured so relevant sections can be loaded without blowing the context window
- **Community-shareable** — standalone document, no Claude-specific jargon

## Output

A single document at `reference/android-auto-protocol.md`, estimated 2000-3000 lines when complete.

## Document Structure

### Section 1: Protocol Overview
- Framing format (header structure, size fields, encryption flags)
- TLS/SSL encryption layer
- Channel multiplexing model
- Protobuf message serialization
- Protocol versioning (version request/response handshake)

### Section 2: Wireless AA Connection Flow (primary focus)
- **Bluetooth discovery & advertisement** — how the head unit advertises to the phone, BT service UUIDs, pairing flow
- **WiFi credential exchange** — the critical handshake where the phone sends WiFi network info to the head unit. This is where the 0x8001 bug lives. Message IDs: `WifiInfoRequest`, `WifiInfoResponse`, `WifiSecurityRequest`, `WifiSecurityResponse`, `WifiChannelData`
- **TCP connection establishment** — who connects to whom, port numbers, timing
- **SSL/TLS handshake** — certificate exchange, session key establishment, encryption activation
- **Service discovery** — `ServiceDiscoveryRequest`/`Response`, channel negotiation, capability advertisement

### Section 3: Bluetooth Profile Requirements
- **SPP (Serial Port Profile)** — role in AA discovery and initial data exchange
- **HFP (Hands-Free Profile)** — phone call audio routing
- **A2DP (Advanced Audio Distribution)** — media audio streaming (BT path vs WiFi path)
- **PBAP (Phone Book Access Profile)** — contact sync for caller ID
- **AVRCP (Audio/Video Remote Control)** — media playback control
- Which profiles are required for AA vs optional enhancements
- BlueZ 5 configuration specifics

### Section 4: Pi 4 Radio Hardware Constraints
- **BCM43455 specs** — WiFi (802.11ac dual-band) + BT 5.0/BLE on single chip
- **Simultaneous operation** — can BT and WiFi coexist? What modes work?
- **WiFi AP vs client mode** — does wireless AA need the Pi as AP, or does the phone host?
- **Known issues** — interference, bandwidth limits, driver quirks on RPi OS Trixie
- **When USB radios are necessary** — scenarios where the onboard radio isn't enough
- **Candidate USB radios** — if needed, which chipsets are known to work

### Section 5: Channel Architecture
- **Control channel** — service discovery, ping, shutdown, version negotiation
- **Video channel** — H.264 stream setup, resolution/FPS negotiation, focus requests
- **Audio channels** — media, speech, system audio; config negotiation, focus management
- **Input channel** — touch events, button events, absolute/relative input
- **Sensor channel** — vehicle data (GPS, speed, night mode, driving status)
- **Bluetooth channel** — pairing requests/responses within AA protocol
- **WiFi channel** — credential exchange messages (the wireless-specific channel)
- Message ID enums for each channel with descriptions

### Section 6: Audio Pipeline (Wireless-Specific)
- Audio stream delivery over WiFi TCP vs BT A2DP — which path for what
- PipeWire routing for AA audio streams
- Microphone input (speech recognition, phone calls)
- Multiple simultaneous audio streams (navigation + media)
- Latency considerations for wireless vs wired

### Section 7: Known Breaking Changes & Version Quirks
- **AA 12.7+** — WiFi credential message ID changed to 0x8001 (BlueWave's enum had 0x0002)
- **proto2 vs proto3** — WifiChannelData.proto syntax change broke parsing
- **VideoFocusRequest required field** — newer AA versions enforce previously optional field
- **touch_event.action_index** — runtime errors in older implementations
- **AVInputOpenRequest** — compatibility issues with newer AA
- Timeline of known breaks with AA app version numbers

### Section 8: Freshness Check Resources
- **URLs to monitor:**
  - `github.com/opencardev/aasdk` — commits and issues
  - `github.com/opencardev/openauto` — commits and issues
  - `github.com/nicka101/aasdk` (SonOfGib) — commits
  - `github.com/nicka101/openauto` (SonOfGib) — commits
  - `github.com/tomasz-grobelny/AACS` — alternative implementation
  - `xdaforums.com` wireless AA threads
  - `milek7.pl/.stuff/galdocs/readme.md` — protocol research
- **Search queries for updates:**
  - `android auto wireless broken update [year]`
  - `aasdk compatibility fix`
  - `openauto wireless projection`
- **Changelog** — running log of protocol changes with dates and AA app versions

### Section 9: Appendix — Wired USB AA (Brief)
- Android Open Accessory Protocol (AOAP) initialization
- USB bulk endpoint setup
- Same SSL/service discovery flow as wireless after transport established
- Kept brief — wired is legacy path

## Research Sources

### Tier 1 — Ground Truth (Working Code)
| Source | Location | What to extract |
|--------|----------|-----------------|
| SonOfGib's aasdk | `upstream/sonofgib-aasdk/` | Proto definitions, WiFi channel impl, transport layers, message IDs |
| opencardev/openauto | GitHub (updated 2026-02-10) | Wireless projection setup, BT integration, service wiring |
| opencardev/aasdk | GitHub | Compatibility fixes, proto updates |

### Tier 2 — Protocol Documentation (Reverse Engineered)
| Source | URL | What to extract |
|--------|-----|-----------------|
| milek7 AA protocol research | `milek7.pl/.stuff/galdocs/readme.md` | Protobuf definitions, Wireshark dissector, TLS details |
| XDA RE thread | `xdaforums.com/t/...3059481/` | Original protocol RE, message formats, handshake sequences |
| AACS project | `github.com/tomasz-grobelny/AACS` | Java implementation perspective, different architectural choices |
| Our RE notes | `docs/reverse-engineering-notes.md` | Binary analysis findings, namespace structure |
| AA fix research | `docs/android-auto-fix-research.md` | 0x8001 bug details, Ghidra findings |

### Tier 3 — Official Google (Limited)
| Source | URL | What to extract |
|--------|-----|-----------------|
| DHU developer docs | `developer.android.com/training/cars/testing/dhu` | headunit.ini config, supported modes, port numbers |
| AOA protocol docs | Android developer docs | USB accessory initialization (for appendix) |

### Tier 4 — Community Signal
| Source | What to extract |
|--------|-----------------|
| opencardev/crankshaft issues | WiFi/BT coexistence reports on Pi hardware |
| openDsh/dash | BT integration approach, Pi 4 wireless config |
| Reddit r/AndroidAuto | "Stopped working after update" reports for breaking changes timeline |
| XDA wireless AA threads | Real-world Pi 4 wireless AA success/failure reports |

## Research Execution Plan

### Phase 1: Read aasdk Source (Wireless Focus)
1. All WiFi channel proto files (`WifiChannel*.proto`, `WifiInfo*.proto`, `WifiSecurity*.proto`)
2. WiFi channel C++ implementation (`Channel/WIFI/`)
3. Bluetooth channel proto files and implementation (`Channel/Bluetooth/`)
4. Transport layer — TCP transport (`Transport/TCPTransport`) and SSL wrapper
5. Messenger layer — framing format, encryption, channel multiplexing
6. Control channel — service discovery, version negotiation

### Phase 2: Cross-Reference with openauto
1. How wireless projection is initialized (opencardev's working implementation)
2. Bluetooth service setup and advertisement
3. WiFi connection management
4. How channels are composed into a working AA session

### Phase 3: External Protocol Docs
1. Fetch and synthesize milek7 protocol research
2. Review XDA thread for details not in source code
3. Check AACS for alternative perspectives on tricky protocol areas
4. Pull Google DHU docs for any official details

### Phase 4: Pi 4 Hardware Research
1. BCM43455 datasheet — modes, limitations
2. Community reports on Pi 4 BT + WiFi simultaneous operation
3. BlueZ 5 + NetworkManager/wpa_supplicant configuration for AA
4. Document when/why USB radios become necessary

### Phase 5: Synthesize & Write
1. Compile findings into `reference/android-auto-protocol.md`
2. Cross-verify critical details (message IDs, handshake sequence) against multiple sources
3. Flag anything where sources disagree — note the conflict and which source to trust
4. Add freshness check section with URLs and search queries

## Session Loading Strategy

The full document is too large to load every session. The Prodigy CLAUDE.md will include directives:

```
## AA Protocol Reference Loading

When working on AA-related code, load the relevant sections from
`reference/android-auto-protocol.md`:

- Wireless connection work → Sections 1-4
- Audio implementation → Add Section 6
- Debugging connection failures → Add Section 7
- Channel implementation → Add Section 5
- Full context (rare) → All sections
```

Each section will be self-contained with cross-references to other sections where needed. Section headers will use consistent formatting for easy `grep`/navigation.

## Success Criteria

- [ ] Can accurately describe the full wireless AA connection flow from BT discovery to video streaming without consulting external sources
- [ ] Message IDs and proto field names match what's actually in aasdk source code
- [ ] Pi 4 hardware constraints are documented with specific chipset details, not hand-waving
- [ ] Known breaking changes include AA app version numbers and dates where available
- [ ] Document is useful to someone who's never used Claude — no Claude-specific assumptions
- [ ] Sections load independently without losing critical context
