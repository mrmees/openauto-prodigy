# Milestone 4: Protocol Correctness (Feb 23-25, 2026)

## What Was Built

### open-androidauto Protocol Library

- Designed and implemented `open-androidauto` (`libs/open-androidauto/`), a Qt-native C++17 Android Auto protocol library replacing aasdk (Boost.ASIO, C++14, custom Promises)
- Four-layer architecture: ITransport (byte I/O) -> Messenger (framing, encryption, multiplexing) -> AASession (FSM lifecycle) -> IChannelHandler (per-channel dispatch)
- Single `IChannelHandler` interface replaces aasdk's 9+ `IXxxServiceChannelEventHandler` interfaces
- `IAVChannelHandler` extended interface adds `onMediaData()` and `canAcceptMedia()` backpressure for AV channels
- `AASession` explicit FSM: Idle -> Connecting -> VersionExchange -> TLSHandshake -> ServiceDiscovery -> Active -> ShuttingDown -> Disconnected
- Table-driven encryption policy replaces scattered conditionals
- Single-writer send queue replaces Boost.ASIO strand serialization
- `ControlChannel` built into library (version exchange, SSL handshake, service discovery, ping)
- Unknown channels get proper `CHANNEL_OPEN_RESPONSE(status=FAIL)` instead of crashes
- Unknown messages logged and emitted via `unknownMessage` signal for protocol analysis
- `ReplayTransport` for testing without a phone using captured wire data

### aasdk Integration Replacement

- Replaced all 13 aasdk-importing files in the app with open-androidauto equivalents
- Removed Boost.ASIO dependency from the AA pipeline entirely
- Threading model: dedicated `QThread` for AA protocol work, signals with `Qt::QueuedConnection` for cross-thread communication (replaces 4 ASIO worker threads + strands)
- Deleted: `AndroidAutoEntity`, `ServiceFactory` (3,600 lines), `IService`
- New channel handlers in `src/core/aa/`: Video, Audio (template for media/speech/system), AVInput, Input, Sensor, Bluetooth, WiFi
- Each handler provides `fillChannelDescriptor()` for service discovery (replaces `fillFeatures()` pattern)
- `TouchHandler` ported from aasdk shared_ptr + ASIO strand to direct `InputChannelHandler` reference
- `AndroidAutoService` rewritten with `QTcpServer` + `oaa::AASession` (replaces ASIO TCP acceptor)
- TCP watchdog preserved via `nativeSocketDescriptor()` on `TCPTransport`

### Library Code Organization

- Moved all 7 channel handlers from `src/core/aa/handlers/` into library at `libs/open-androidauto/{include/oaa,src}/HU/Handlers/`
- Namespace migration: `oap::aa` -> `oaa::hu`
- `ServiceDiscoveryBuilder` moved to `oaa::hu::discovery`
- Single CMake target (`open-androidauto`) kept for build simplicity
- Transitional compatibility wrapper headers added at old paths

### Protocol Enum Expansions (APK v16.1 Cross-Reference)

- **VideoFocusMode**: Added PROJECTED(1), NATIVE(2), NATIVE_TRANSIENT(3), PROJECTED_NO_INPUT_FOCUS(4) — replaces incomplete FOCUSED/UNFOCUSED
- **VideoFocusReason**: Added PHONE_SCREEN_OFF(1), LAUNCH_NATIVE(2), LAST_MODE(3), USER_SELECTION(4)
- **ShutdownReason**: Expanded from 2 values (NONE/QUIT) to 9 values including DEVICE_SWITCH, POWER_DOWN, WIRELESS_PROJECTION_DISABLED
- **StatusCode**: Expanded from 2 values (OK/FAIL) to 33 values covering BT errors, cert errors, radio errors, framing errors
- **AudioFocusType**: Renamed GAIN_NAVI(3) to GAIN_TRANSIENT_MAY_DUCK (correct APK name)
- **BluetoothPairingMethod**: Renamed UNK_1/A2DP/UNK_3/HFP to OOB/NUMERIC_COMPARISON/PASSKEY_ENTRY/PIN
- **SensorType**: Added 5 EV/trailer types (TOLL_CARD through RAW_EV_TRIP_SETTINGS, values 22-26)
- **MediaCodecType**: New enum with AUDIO_PCM(1), AUDIO_AAC_LC(2), VIDEO_H264_BP(3), VIDEO_VP9(5), VIDEO_AV1(6), VIDEO_H265(7)
- **ServiceType**: New enum with all 21 service types (CONTROL through BUFFERED_MEDIA_SINK)

### New Message IDs and Protocol Messages

- **Control channel**: Added CHANNEL_CLOSE_NOTIFICATION(0x0009), CALL_AVAILABILITY_STATUS(0x0018), SERVICE_DISCOVERY_UPDATE(0x001A)
- **AV channel**: Added 13 new message IDs (0x8009-0x8015) — VIDEO_FOCUS_NOTIFICATION, OVERLAY_*, MEDIA_STATS, MEDIA_OPTIONS, etc.
- **CallAvailabilityMessage.proto**: New proto for call availability status (sent by commercial HUs)
- Newer AV messages logged instead of emitting `unknownMessage` (reduces noise in protocol logs)

### Sensor and WiFi Fixes

- Added `pushParkingBrake(bool)` to SensorChannelHandler — mandatory sensor per Sony firmware analysis
- Trimmed ServiceDiscoveryBuilder to only advertise sensors we actually populate (removed LOCATION, COMPASS, ACCEL, GYRO; added PARKING_BRAKE)
- Added missing tire pressure field 18 to SensorEventIndication proto
- Identified WiFi proto field mismatch: APK uses field 5 for `access_point_type` and WPA2_PERSONAL=8 vs our WPA2_PERSONAL=1

### Protocol Plumbing

- Bridged AA audio focus requests to PipeWire stream ducking via `audioFocusChanged` signal on AASession
- Wired voice session request handling (log + acknowledge VoiceSessionRequest 0x0011)
- Published navigation events to IEventBus (aa.nav.state, aa.nav.step, aa.nav.distance)
- Published phone status and media playback events to IEventBus for plugin consumption

### Proto Validation Pipeline

- Built `tools/validate_protos.py` — rerunnable cross-reference of 150 proto files against APK v16.1
- Three-stage pipeline: proto parser -> match loader -> field-level comparator
- `tools/proto_parser.py`: Regex-based proto2 file parser extracting fields, types, cardinality, enums
- `tools/match_loader.py`: Merges auto-detected APK class matches with manual overrides from `proto_matches.json`
- `tools/proto_comparator.py`: Field-by-field comparison with wire-compatibility awareness (int32==enum on wire, bytes==message on wire)
- Outputs: JSON report (`tools/proto_validation_report.json`) and Markdown report (`docs/proto-validation-report.md`)
- Final score: 93 verified / 32 partial / 58 unmatched (out of 183 proto messages)

### Proto Partial Match Investigation

- Investigated all 33 partial matches systematically
- 16 were int32-vs-enum (cosmetic, wire-identical — no fix needed)
- 8 were bytes-vs-message (wire-compatible, unused in code — no fix needed)
- 4 were validator bugs (oneof syntax, enum-only files, descriptor format — proto correct)
- Real fixes applied: ChannelOpenRequest field 1 (int32 -> sint32, ZigZag encoding difference), InputChannelConfig fields 3-5 (corrected to TouchScreenConfig/TouchPadConfig/enum)

### ChannelDescriptor Field Number Migration

- Corrected ChannelDescriptor field numbers from aasdk's legacy values to APK v16.1 wire format
- Fields 7+ were shuffled in aasdk relative to APK; corrected mapping:
  - 7=wifi_projection (NEW), 8=navigation, 9=phone_status, 10=notification
  - 11=media_browser, 12=media_info, 13=audio_focus (NEW)
  - 14=wifi, 15=car_control, 16=radio, 17=vendor_extension
- Created `WiFiProjectionChannelData.proto` and `AudioFocusChannelData.proto`
- Discovery: phone silently discards misaligned sub-channel config from ChannelDescriptor, then negotiates capabilities per-channel during setup — explains why aasdk's wrong numbers worked

### APK Indexing Pipeline

- Built reproducible APK preprocessing pipeline at `analysis/tools/apk_indexer/`
- Auto-detects APK version from `apktool.yml` or AndroidManifest
- Organizes artifacts under versioned folder: `analysis/android_auto_<version>_<code>/`
- SQLite + JSON indexes covering: UUIDs, constants, proto accesses, proto writes, enum maps, switch maps, call edges
- Summary report generator for fast protocol triage
- Database at `analysis/android_auto_16.1.660414-release_161660414/apk-index/sqlite/apk_index.db`

### Descriptor Decoder Tool

- `tools/descriptor_decoder.py` — decodes APK Java class descriptor strings (RawMessageInfo) into wire field mappings
- Replicates `aaah.java`'s protobuf-lite schema parsing logic
- Type codes 0-17 = singular (3 varints), 18-34 = repeated (2 varints), 35-48 = packed (2 varints + lookup table)

### AA Protocol Deep Dive

- Decoded 1,940 of 1,943 protobuf message classes from the APK
- Structural matching identified 29 high-confidence unique matches between APK classes and our proto library
- Graph-walk approach from known roots through all referenced sub-messages and enums
- Built JSON knowledge base and markdown reference organized by channel

## Key Design Decisions

### Why Replace aasdk Entirely (Big-Bang Swap)

aasdk's Boost.ASIO strands, shared_ptr lifetimes, and custom Promise pattern created an impedance mismatch with Qt's signals/slots. Every interaction required cross-thread marshaling via `QMetaObject::invokeMethod`. The threading bugs and lifecycle complexity made the codebase unreasonable. A big-bang swap on a feature branch was chosen over an adapter layer — the old code stays on `main` as git fallback.

### Single Protocol Thread, Not Main Thread

Moving everything to the Qt main thread risked stalling the UI during video decode and audio writes. A dedicated `QThread` for AA protocol work preserves parallelism without ASIO complexity. Qt's `QueuedConnection` across threads provides strand-equivalent safety.

### Evidence-Gated Protocol Expansion

After Codex review of the original "implement all 21 services" plan, the approach was revised to require real wire captures before adding new services. This prevented speculative implementation of ~70 proto files + 10 handlers. New services enter the library only with: (1) at least one real capture showing channel open + message flow, (2) decompiled reference aligned with capture data, (3) unit test + replay test using captured data.

### Proto2 Required, No Migration to Proto3

The AA wire protocol uses proto2 semantics. Proto3 would change wire behavior (different default handling, no required fields). Wire compatibility is non-negotiable.

### Channel Handlers in Library, Not App

Channel handlers are pure protocol logic with zero app dependencies. Moving them to `oaa::hu` namespace in the library makes them reusable for any AA head unit implementation, supporting the project goal of enabling hobbyist builds.

### ServiceDiscoveryBuilder Stays in App

Unlike handlers, ServiceDiscoveryBuilder depends on `oap::YamlConfig` (app-level config). Moving it would require abstracting the config interface. It was moved to `oaa::hu::discovery` but retains the config dependency.

### Validator Limitations Accepted

The proto validation pipeline has known limitations: cannot parse `oneof` syntax, cannot validate enum-only files, cannot decode proto3-style descriptors. These affect 4 of 32 partial matches. Fixing the validator was deprioritized because the protos are known correct.

### int32 vs enum Fields Left As-Is

16 proto fields use int32 where the APK uses enum. These are wire-identical (both varint-encoded). Converting to proper enum types would require creating ~16 new enum proto files. The effort was deemed not worth it since the wire format is correct.

## Lessons Learned

### Protocol Wire Format

- **Proto2 negative enum values work.** The protobuf compiler accepts them (e.g., StatusCode goes from -255 to +1). Useful for error codes that match the APK.
- **sint32 uses ZigZag encoding, int32 does not.** If the APK sends sint32 and we parse as int32, we read double the actual value. Found this in ChannelOpenRequest priority field.
- **int32 and enum are wire-identical.** Both use varint encoding. A proto field declared as int32 will correctly parse an enum value and vice versa.
- **bytes and message are wire-identical.** Both are length-delimited (wire type 2). Using `bytes` is opaque but safe; using `message` adds auto-deserialization.
- **Phone silently discards misaligned ChannelDescriptor sub-channel config.** aasdk's wrong field numbers worked because the phone negotiates capabilities per-channel during setup, ignoring the wrapper descriptor.

### APK Decompilation

- **APK uses obfuscated class names** (e.g., `wbw` = ChannelDescriptor, `vys` = AVChannel, `wcz` = VideoConfig). The `aaah.java` file is the Rosetta Stone — it contains the protobuf-lite schema compiler.
- **Descriptor type codes have specific bit layouts.** Lower byte = proto type, upper bits = flags (required, packed, etc.). Type codes 0-17 are singular, 18-34 are repeated, 35-48 are packed.
- **`RawMessageInfo` descriptor strings use high-Unicode chars** to encode type information. The descriptor decoder must handle chars up to U+1500.
- **Java field order doesn't match proto field order.** The descriptor's field list follows the Object[] array, not the Java field declaration order.
- **5 Java fields but 4 proto fields is possible** — the extra Java field may be a oneof case discriminator or a map entry helper.
- **S25 Ultra codec priority:** highest resolution, H.265 > H.264. VP9/AV1 ignored.

### Validation Pipeline

- **Structural matching produces false positives.** Two messages with the same field count but different types can score as matches. Manual verification via `proto_matches.json` overrides are essential.
- **50 of 1,943 APK classes fail the descriptor decoder.** These are edge cases (proto3-style encoding, high field numbers, unusual type chars). Most are not AA-relevant.
- **The partial match investigation saved debugging time.** Knowing that 16 partials are cosmetic and 8 are wire-compatible eliminated 24 false alarms from the validation report.

### Commercial Head Unit Behavior (from firmware analysis)

- Sony firmware enables 3 mandatory sensors: night mode (10), driving status (13), parking brake (7). Without parking brake, AA may restrict features while "driving."
- Every commercial HU sends CALL_AVAILABILITY_STATUS (0x0018) on the control channel.
- Production HUs use the phone's `connection_configuration` data for ping interval/timeout (not hardcoded values).

### APK Class Mappings (Reference)

| APK Class | Proto Message | Notes |
|-----------|--------------|-------|
| wby | ServiceDiscoveryResponse | Top-level response |
| wbw | ChannelDescriptor | 17 fields, field number correction critical |
| vys | AVChannel | field 8 = keycode (enum in APK, int32 in ours) |
| vya | InputChannel | fields 3-5 corrected |
| wbu | SensorChannel | fuel/EV connector enums |
| vwc | BluetoothChannel | pairing method enums |
| vzr | NavigationChannel | type enum |
| wdh | WifiChannel | |
| vyt | AVInputChannel | |
| vvp | AudioConfig | perfect 3-field match |
| wcz | VideoConfig | 11 fields, field 11 = additional config (bytes) |
| vxn | TouchConfig | missing bool field 3 in ours |
| vyn | MediaCodecType | enum discovered via APK indexer |

## Source Documents

1. `2026-02-23-oaa-integration-design.md` — Design for replacing aasdk with open-androidauto in the app
2. `2026-02-23-oaa-integration-implementation.md` — 25-task implementation plan for the integration
3. `2026-02-23-open-androidauto-design.md` — Architecture design for the open-androidauto library
4. `2026-02-23-open-androidauto-implementation.md` — Bottom-up implementation plan (Transport -> Messenger -> Session)
5. `2026-02-23-open-androidauto-integration.md` — Integration plan for wiring library into app (feature branch, channel handlers, orchestrator)
6. `2026-02-24-move-handlers-to-library.md` — Plan to move channel handlers from app to library (oap::aa -> oaa::hu)
7. `2026-02-24-open-androidauto-organization-design.md` — Design for HU-facing module organization in library
8. `2026-02-24-open-androidauto-organization-implementation.md` — Implementation plan for library reorganization
9. `2026-02-24-protocol-completeness-design.md` — Evidence-gated protocol expansion design (P0/P1/P2 tiers)
10. `2026-02-24-protocol-p0-implementation.md` — Implementation plan for P0 proven gaps (enum expansions, message IDs, sensors)
11. `2026-02-25-aa-proto-deep-dive-design.md` — Design for graph-walking all proto dependencies from known APK roots
12. `2026-02-25-apk-preprocessing-design.md` — Design for reproducible APK indexing pipeline
13. `2026-02-25-apk-preprocessing-implementation.md` — Implementation plan for APK indexer (7 tasks)
14. `2026-02-25-protocol-plumbing.md` — Plan to wire existing proto handlers to app services (audio focus, nav events, sensors)
15. `2026-02-25-proto-partial-match-investigation.md` — Investigation of all 33 partial proto matches with findings
16. `2026-02-25-proto-phase-a-migration-manifest.md` — Migration manifest for ChannelDescriptor field number correction
17. `2026-02-25-proto-phase-a-patch-checklist.md` — Execution checklist for Phase A proto patches
18. `2026-02-25-proto-validation-implementation.md` — Implementation plan for proto validation pipeline (6 tasks)
19. `2026-02-25-proto-validation-pipeline-design.md` — Design for systematic proto-vs-APK validation tool
