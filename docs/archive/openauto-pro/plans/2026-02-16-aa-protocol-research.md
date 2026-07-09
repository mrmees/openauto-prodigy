# Android Auto Protocol Reference — Research & Writing Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a comprehensive, wireless-first Android Auto protocol reference at `reference/android-auto-protocol.md` by reading aasdk source code, cross-referencing community docs, and documenting Pi 4 hardware constraints.

**Architecture:** Read Tier 1 sources (aasdk source code) first to establish ground truth, then cross-reference with Tier 2 (community RE docs) and Tier 3 (Google official docs) to fill gaps and verify. Pi 4 hardware research is independent and can run in parallel. Final synthesis combines all findings into a single structured document.

**Tech Stack:** Markdown output. Source material is C++ (aasdk), protobuf definitions, and web resources.

**Key context files:**
- Design doc: `docs/plans/2026-02-16-aa-protocol-reference-design.md`
- Prodigy design: `docs/plans/2026-02-16-openauto-prodigy-design.md`
- Existing RE notes: `docs/reverse-engineering-notes.md`
- Existing AA fix research: `docs/android-auto-fix-research.md`

**Base path for aasdk source:** `../../upstream/sonofgib-aasdk/` (relative to repo root)

---

### Task 1: Read WiFi Channel Proto Files

**Goal:** Extract every message type, field, and enum involved in the wireless AA WiFi credential exchange — the core of wireless AA and where the known bugs live.

**Files to read:**
- `upstream/sonofgib-aasdk/aasdk_proto/WifiChannelMessageIdsEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/WifiChannelData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/WifiInfoRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/WifiInfoResponseMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/WifiSecurityRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/WifiSecurityResponseMessage.proto`

**Step 1: Read all WiFi proto files**

Read each file listed above. For each, document:
- Message/enum name
- All fields with types and field numbers
- Any comments or annotations
- proto2 vs proto3 syntax (this matters — the proto2/proto3 mismatch was a known breaking change)

**Step 2: Write findings to scratch notes**

Create `reference/scratch/wifi-channel-protos.md` with raw findings. Include the exact enum values for `WifiChannelMessageIdsEnum` — the 0x8001 vs 0x0002 discrepancy is the most critical detail in this entire project.

**Step 3: Verify against our existing RE notes**

Cross-check findings against `docs/android-auto-fix-research.md` to confirm the 0x8001 bug details match what's in the actual source code.

---

### Task 2: Read WiFi Channel C++ Implementation

**Goal:** Understand how the WiFi credential exchange actually works in code — message flow, error handling, state machine.

**Files to read:**
- `upstream/sonofgib-aasdk/include/aasdk/Channel/WIFI/WIFIServiceChannel.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Channel/WIFI/IWIFIServiceChannel.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Channel/WIFI/IWIFIServiceChannelEventHandler.hpp`
- Find and read the corresponding `.cpp` implementation files (likely in `src/Channel/WIFI/`)

**Step 1: Read the WiFi channel headers**

Document the public API: what methods exist, what callbacks/events are fired, what the channel lifecycle looks like.

**Step 2: Read the implementation**

Trace the message flow: when a WiFi message arrives, what happens? How does the channel parse credentials? What does it do with them? How does it respond?

**Step 3: Document the WiFi handshake sequence**

Write a step-by-step flow to `reference/scratch/wifi-handshake-flow.md`:
1. What triggers the WiFi channel to activate
2. Who sends first (phone or head unit)
3. What messages are exchanged and in what order
4. What data is in each message (SSID, passphrase, security type, IP, port)
5. How the channel signals "credentials received, connect now"
6. Error/failure paths

---

### Task 3: Read Bluetooth Channel Proto Files & Implementation

**Goal:** Understand the BT pairing protocol within AA — how the phone and head unit negotiate BT profiles through the AA protocol layer (separate from the OS-level BlueZ BT stack).

**Files to read:**
- `upstream/sonofgib-aasdk/aasdk_proto/BluetoothChannelMessageIdsEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/BluetoothChannelData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/BluetoothPairingRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/BluetoothPairingResponseMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/BluetoothPairingStatusEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/BluetoothPairingMethodEnum.proto`
- `upstream/sonofgib-aasdk/include/aasdk/Channel/Bluetooth/BluetoothServiceChannel.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Channel/Bluetooth/IBluetoothServiceChannelEventHandler.hpp`
- Find and read corresponding `.cpp` files

**Step 1: Read all BT proto files**

Document all message types, enums (especially pairing methods and status values), and field definitions.

**Step 2: Read the BT channel implementation**

Trace the pairing flow: what initiates pairing, what methods are supported, how status is communicated.

**Step 3: Write BT findings to scratch**

Create `reference/scratch/bluetooth-channel.md` documenting:
- AA-protocol-level BT messages (distinct from OS-level BlueZ)
- Pairing methods supported
- How BT channel relates to WiFi channel (BT discovery → WiFi credential exchange)
- What the head unit needs to advertise over BT for the phone to find it

---

### Task 4: Read Transport & Messenger Layers

**Goal:** Understand the framing format, encryption, and channel multiplexing that all AA communication rides on.

**Files to read:**
- `upstream/sonofgib-aasdk/include/aasdk/Transport/TCPTransport.hpp` + `.cpp`
- `upstream/sonofgib-aasdk/include/aasdk/Transport/Transport.hpp` + `.cpp`
- `upstream/sonofgib-aasdk/include/aasdk/Transport/SSLWrapper.hpp` + `.cpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/FrameHeader.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/FrameType.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/FrameSize.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/FrameSizeType.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/EncryptionType.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/MessageType.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/MessageId.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/ChannelId.hpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/Messenger.hpp` + `.cpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/MessageInStream.hpp` + `.cpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/MessageOutStream.hpp` + `.cpp`
- `upstream/sonofgib-aasdk/include/aasdk/Messenger/Cryptor.hpp` + `.cpp`

**Step 1: Read frame/message type enums**

Document: FrameType values, EncryptionType values, MessageType values, ChannelId enum (all channel IDs), how messages are identified.

**Step 2: Read the framing layer**

Document the wire format: header byte layout, how channel ID is encoded, how frame size is determined, how frames are reassembled into messages.

**Step 3: Read the encryption layer**

Document: when encryption activates (after SSL handshake), how the Cryptor wraps/unwraps messages, what's encrypted vs plaintext (version handshake is plaintext, everything after SSL is encrypted).

**Step 4: Read TCP transport**

Document: connection setup, how raw bytes flow between TCP socket and messenger, any keep-alive or timeout behavior.

**Step 5: Write transport findings to scratch**

Create `reference/scratch/transport-framing.md` with:
- Exact byte-level frame format diagram
- Channel ID → channel type mapping table
- Encryption activation sequence
- TCP connection lifecycle

---

### Task 5: Read Control Channel (Service Discovery & Version Negotiation)

**Goal:** Understand the initial handshake that every AA session starts with — version negotiation and service discovery establish what both sides can do.

**Files to read:**
- `upstream/sonofgib-aasdk/aasdk_proto/ControlMessageIdsEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ServiceDiscoveryRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ServiceDiscoveryResponseMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ChannelOpenRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ChannelOpenResponseMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ChannelDescriptorData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/PingRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/PingResponseMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ShutdownRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ShutdownResponseMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ShutdownReasonEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AuthCompleteIndicationMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/BindingRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/BindingResponseMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/StatusEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/VersionResponseStatusEnum.proto`
- `upstream/sonofgib-aasdk/include/aasdk/Channel/Control/ControlServiceChannel.hpp` + `.cpp`
- `upstream/sonofgib-aasdk/include/aasdk/Version.hpp`

**Step 1: Read control channel proto files**

Document all control message types and their fields. Pay special attention to `ServiceDiscoveryResponse` — it defines what channels/capabilities the head unit advertises.

**Step 2: Read the control channel implementation**

Trace the startup sequence: version request → version response → SSL → auth complete → service discovery → channel opens.

**Step 3: Read video/audio config protos**

These are advertised during service discovery:
- `upstream/sonofgib-aasdk/aasdk_proto/VideoConfigData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/VideoResolutionEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/VideoFPSEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AudioConfigData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AudioTypeEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/TouchConfigData.proto`

**Step 4: Write control channel findings to scratch**

Create `reference/scratch/control-channel.md` with:
- Complete startup handshake sequence diagram
- Service discovery request/response structure
- What capabilities the head unit must advertise
- Channel open/close lifecycle
- Ping/keepalive behavior
- Shutdown sequence

---

### Task 6: Read Remaining Channel Proto Files (AV, Input, Sensor)

**Goal:** Document the data channels for completeness — video, audio, input, and sensor. These are less critical for wireless-specific work but needed for the full reference.

**Files to read:**
- `upstream/sonofgib-aasdk/aasdk_proto/AVChannelMessageIdsEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVChannelData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVChannelSetupRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVChannelSetupResponseMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVChannelSetupStatusEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVChannelStartIndicationMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVChannelStopIndicationMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVStreamTypeEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVMediaAckIndicationMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVInputOpenRequestMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/AVInputChannelData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/InputChannelMessageIdsEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/InputChannelData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/InputEventIndicationMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/TouchActionEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/TouchEventData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/TouchLocationData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ButtonCodeEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ButtonEventData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/ButtonEventsData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/SensorChannelMessageIdsEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/SensorChannelData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/SensorTypeEnum.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/SensorData.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/SensorEventIndicationMessage.proto`
- `upstream/sonofgib-aasdk/aasdk_proto/SensorStartResponseMessage.proto`
- Video/audio focus protos (`VideoFocus*.proto`, `AudioFocus*.proto`)
- Navigation protos (`Navigation*.proto`)

**Step 1: Read and document AV channel protos**

Focus on: setup negotiation, start/stop flow, stream types, ack mechanism.

**Step 2: Read and document Input channel protos**

Focus on: touch event format, button codes, absolute vs relative input.

**Step 3: Read and document Sensor channel protos**

Focus on: sensor types, which are required vs optional, data formats.

**Step 4: Write findings to scratch**

Create `reference/scratch/data-channels.md` covering AV, Input, and Sensor channels.

---

### Task 7: Cross-Reference with opencardev/openauto

**Goal:** See how a working implementation actually wires up wireless AA — the aasdk is the protocol library, openauto is the application that uses it.

**What to examine (on GitHub, not cloned locally):**
- opencardev/openauto wireless projection setup code
- How BT advertisement is configured
- How WiFi credentials from the AA protocol are applied to the OS network stack
- How the TCP connection is established after WiFi connects
- Recent commits (updated Feb 10, 2026) — check for any protocol changes

**Step 1: Fetch and review openauto's wireless projection code**

Use web fetch on key source files from `github.com/opencardev/openauto`. Focus on:
- WiFi projection service/manager
- Bluetooth service setup
- How aasdk channels are instantiated for wireless mode

**Step 2: Check recent commits for protocol-relevant changes**

Use `gh` or web fetch to review commits from last 3 months on both `opencardev/openauto` and `opencardev/aasdk`.

**Step 3: Document implementation patterns**

Add to `reference/scratch/openauto-wireless-impl.md`:
- How the app transitions from BT discovery → WiFi → TCP → AA session
- Any workarounds or hacks for known issues
- Differences from SonOfGib's approach

---

### Task 8: Fetch External Protocol Documentation

**Goal:** Pull in community-produced protocol documentation to fill gaps the source code alone can't explain (e.g., "why" decisions, historical context, undocumented behavior).

**Sources:**
- `milek7.pl/.stuff/galdocs/readme.md` — AA protocol research with extracted protobuf defs
- XDA RE thread (page 1 + page 2) — original protocol reverse engineering
- `github.com/tomasz-grobelny/AACS` — Java AA server README and protocol notes
- Google DHU docs — `developer.android.com/training/cars/testing/dhu`

**Step 1: Fetch milek7 protocol research**

Web fetch the full document. Extract:
- Protobuf definitions (compare against aasdk protos for differences)
- Wireshark dissector details
- TLS key extraction method
- Any protocol details not visible in aasdk source

**Step 2: Fetch XDA thread technical details**

Web fetch both pages. Extract protocol-level details only (skip user troubleshooting posts).

**Step 3: Review AACS README**

Web fetch. Note any protocol details that differ from aasdk's implementation.

**Step 4: Fetch Google DHU docs**

Web fetch. Extract:
- headunit.ini configuration options (maps to protocol capabilities)
- Port numbers
- Supported modes
- Any protocol version info

**Step 5: Write external findings to scratch**

Create `reference/scratch/external-protocol-docs.md` with findings, noting where external docs agree/disagree with aasdk source.

---

### Task 9: Research Pi 4 Radio Hardware Constraints

**Goal:** Document what the Pi 4's BCM43455 can and can't do for simultaneous BT + WiFi, which is the core hardware question for wireless AA.

**This task is independent of Tasks 1-8 and can run in parallel.**

**Step 1: Research BCM43455 capabilities**

Web search for:
- BCM43455 datasheet / capabilities
- Simultaneous WiFi AP + BT on Raspberry Pi 4
- Pi 4 WiFi modes (AP, client, direct) that work with BT active
- `brcmfmac` driver limitations on Pi

**Step 2: Research community Pi 4 wireless AA experiences**

Web search for:
- "Raspberry Pi 4 wireless android auto" setup guides and reports
- crankshaft/openauto Pi 4 WiFi issues
- openDsh/dash BT + WiFi coexistence
- Whether wireless AA uses phone-as-AP or head-unit-as-AP

**Step 3: Research BlueZ 5 configuration for AA**

Web search for:
- BlueZ 5 SPP profile setup (needed for AA discovery)
- BT advertisement configuration on Pi
- How to register AA-specific BT service UUIDs
- NetworkManager vs wpa_supplicant for WiFi management alongside BlueZ

**Step 4: Research USB radio options**

Web search for:
- USB WiFi adapters known to work with Pi for AP mode
- USB BT adapters for Pi (if onboard BT is insufficient)
- Which chipsets have good Linux driver support
- Community recommendations from openauto/crankshaft/dash users

**Step 5: Write hardware findings to scratch**

Create `reference/scratch/pi4-radio-hardware.md` with:
- BCM43455 capability matrix (what mode combinations work)
- Known limitations and workarounds
- Community success/failure reports
- USB radio recommendations if needed
- BlueZ 5 configuration notes

---

### Task 10: Synthesize Reference Document

**Goal:** Combine all scratch notes into the final `reference/android-auto-protocol.md`, structured per the design doc.

**Depends on:** Tasks 1-9 all complete.

**Step 1: Create the document skeleton**

Create `reference/android-auto-protocol.md` with all section headers from the design doc, empty content.

**Step 2: Write Section 1 — Protocol Overview**

Source: `reference/scratch/transport-framing.md`

Content: framing format, TLS layer, channel model, protobuf serialization, version handshake. Include byte-level frame diagram.

**Step 3: Write Section 2 — Wireless AA Connection Flow**

Sources: `reference/scratch/wifi-channel-protos.md`, `reference/scratch/wifi-handshake-flow.md`, `reference/scratch/bluetooth-channel.md`, `reference/scratch/openauto-wireless-impl.md`

Content: Complete step-by-step flow from BT discovery to active AA session. This is the most important section — be thorough. Include sequence diagram.

**Step 4: Write Section 3 — Bluetooth Profile Requirements**

Sources: `reference/scratch/bluetooth-channel.md`, `reference/scratch/pi4-radio-hardware.md`

Content: Each BT profile, its role, whether required or optional, BlueZ 5 configuration.

**Step 5: Write Section 4 — Pi 4 Radio Hardware Constraints**

Source: `reference/scratch/pi4-radio-hardware.md`

Content: BCM43455 capabilities, simultaneous operation, WiFi mode requirements, when USB radios are needed.

**Step 6: Write Section 5 — Channel Architecture**

Sources: `reference/scratch/control-channel.md`, `reference/scratch/data-channels.md`

Content: Each channel type with message IDs, purpose, setup flow. Include complete message ID tables.

**Step 7: Write Section 6 — Audio Pipeline**

Sources: `reference/scratch/data-channels.md`, `reference/scratch/openauto-wireless-impl.md`

Content: Audio routing for wireless AA, PipeWire integration notes, multi-stream handling.

**Step 8: Write Section 7 — Known Breaking Changes**

Sources: All scratch notes, `docs/android-auto-fix-research.md`, external docs

Content: Timeline of known breaks with AA app versions, what changed, how it was fixed.

**Step 9: Write Section 8 — Freshness Check Resources**

Content: URLs to monitor, search queries, changelog format. Copy from design doc and expand with any new sources discovered during research.

**Step 10: Write Section 9 — Appendix: Wired USB AA**

Source: `reference/scratch/transport-framing.md`, USB headers from aasdk

Content: Brief AOAP initialization flow, note that post-transport the protocol is identical.

---

### Task 11: Update Prodigy CLAUDE.md

**Goal:** Add loading directives so future sessions know which sections of the reference to read.

**Files:**
- Modify: `CLAUDE.md` (repo root)

**Step 1: Add reference loading section**

Add to the CLAUDE.md after the existing "Workflow Reminder" section:

```markdown
## AA Protocol Reference Loading

When working on AA-related code, load relevant sections from
`../../reference/android-auto-protocol.md`:

- Wireless connection work → Sections 1-4
- Audio implementation → Add Section 6
- Debugging connection failures → Add Section 7
- Channel implementation → Add Section 5
- Full context (rare) → All sections

The reference includes exact message IDs, proto field names, and handshake
sequences verified against SonOfGib's aasdk source code.
```

**Step 2: Clean up scratch notes**

Once the final document is verified, the scratch files can be deleted or kept as working notes. Ask Matthew which he prefers.

---

## Task Dependency Graph

```
Tasks 1-6 (aasdk source reading) → sequential, each builds context
Task 7 (opencardev cross-reference) → after Tasks 1-6
Task 8 (external docs) → independent, can parallel with 1-6
Task 9 (Pi 4 hardware) → independent, can parallel with 1-6

All above → Task 10 (synthesis)
Task 10 → Task 11 (CLAUDE.md update)
```

**Parallelizable:** Tasks 8 and 9 can run alongside Tasks 1-6.
**Sequential:** Tasks 1-6 should be done in order (each builds on prior context). Task 7 needs 1-6 complete. Task 10 needs everything. Task 11 needs Task 10.

## Execution Notes

- **Scratch directory:** Create `reference/scratch/` for intermediate notes. These are working documents, not final output.
- **Source verification:** When documenting message IDs or field names, always quote the exact proto/header file and line number. No paraphrasing protocol details from memory.
- **Conflicts between sources:** When aasdk source disagrees with external docs, aasdk wins (it's the code we'll actually build against). Note the disagreement.
- **Context management:** Each task produces a focused scratch file. The synthesis task (10) combines them. This prevents any single task from requiring too much context.
