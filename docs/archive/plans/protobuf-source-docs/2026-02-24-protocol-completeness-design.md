# Protocol Completeness — Design Document (Revised)

> **Date:** 2026-02-24
> **Revised:** After Codex review — evidence-gated approach
> **Goal:** Fix proven gaps in existing protocol surface, stage new services behind wire evidence
> **Scope:** Library only — handlers emit signals, consumers decide what to do

## Codex Review Summary

The original plan tried to implement all 21 services speculatively based on decompiled APK analysis. Codex correctly flagged:
- Proto definitions from obfuscated source are not verified against wire captures
- Channel ID assignments for new services are guesses (phone picks IDs via ChannelOpenRequest)
- Core interop gaps in existing working channels should be fixed before adding new surface area
- ~70 proto files + 10 handlers with no verification gates is high-risk

## Revised Approach: Evidence-Gated Phases

### Hard Rule

**No new service enters the library API without:**
1. At least one real capture showing channel open + message flow
2. Decompiled reference aligned with capture data
3. Unit test + replay test using captured data

### Success Criteria

Before any P1/P2 work: "Phone initiates RFCOMM → completes WiFi handoff → stable AA session with video + touch for 5+ minutes" must be reliable.

---

## P0: Fix Proven Gaps in Working Channels

These are things we KNOW happen during working sessions. They affect real behavior today.

### P0.1 Enum Expansions

**VideoFocusMode** — Currently `UNFOCUSED(0)/FOCUSED(1)`. Phones send values 1-4:
```
UNFOCUSED = 0
PROJECTED = 1                  // (currently mapped to FOCUSED)
NATIVE = 2                     // HU showing native UI
NATIVE_TRANSIENT = 3           // Brief native (e.g. reverse camera)
PROJECTED_NO_INPUT_FOCUS = 4   // Video visible, touch goes elsewhere
```
**Evidence:** VideoFocusRequest/Indication on wire during every session.

**ShutdownReason** — Currently hardcoded `QUIT(1)`. Full enum from APK (`vwe.java`):
```
USER_SELECTION = 1, DEVICE_SWITCH = 2, NOT_SUPPORTED = 3,
NOT_CURRENTLY_SUPPORTED = 4, PROBE_SUPPORTED = 5,
WIRELESS_PROJECTION_DISABLED = 6, POWER_DOWN = 7, USER_PROFILE_SWITCH = 8
```
**Evidence:** ShutdownRequest on wire during every disconnect. Phone may handle POWER_DOWN differently than USER_SELECTION.

**StatusCode** — Currently `OK(0)/FAIL(1)`. Full enum from APK (`vyw.java`), 33 values (-255 to +1). Enables proper error responses instead of generic FAIL.
**Evidence:** Used in ChannelOpenResponse, AuthComplete — on wire during every session.

### P0.2 Control Channel — Missing Message Handlers

Messages that arrive on the control channel today but get silently dropped:

| Wire ID | Name | Direction | Action |
|---------|------|-----------|--------|
| 24 | CALL_AVAILABILITY_STATUS | HU→Phone | Send during session — phone needs this for call UI |
| 9 | CHANNEL_CLOSE_NOTIFICATION | Either | Handle gracefully instead of dropping |

**Evidence needed:** Capture a session and check for `unknownMessage` signals on channel 0. CALL_AVAILABILITY is sent by every commercial HU (Sony, Kenwood, Pioneer).

Messages to defer (no evidence they matter for current interop):
- 20/21 (CarConnectedDevices), 22/25 (UserSwitch), 23 (BatteryStatus), 26 (ServiceUpdate)

### P0.3 AV Channel — Acknowledge Newer Message IDs

Messages 0x8009-0x8015 arrive from modern phones and get dropped as `unknownMessage`:

| Wire ID | Name | Priority | Reason |
|---------|------|----------|--------|
| 0x8009 | VIDEO_FOCUS_NOTIFICATION | **Log** | May duplicate 0x8008 |
| 0x800C | ACTION_TAKEN | **Log** | Phone telling us about user action |
| 0x800D | OVERLAY_PARAMETERS | **Log** | Google Maps overlay setup |
| 0x800E-0x8010 | OVERLAY_START/STOP/UPDATE | **Log** | Overlay lifecycle |
| 0x8014 | MEDIA_STATS | **Log** | Performance telemetry from phone |

**Action:** Add message ID constants. In handlers, log these messages instead of emitting `unknownMessage`. Define proto stubs so they can be parsed later. Don't build full handling until we see what phones actually send.

### P0.4 Sensor Channel — Add Parking Brake

Sony firmware enables 3 mandatory sensors: night mode (10), driving status (13), **parking brake (7)**. We push the first two but not parking brake. Without it, AA may restrict features while "driving."

**Action:** Add `pushParkingBrake(bool engaged)` to SensorChannelHandler. Trivial — same pattern as existing push methods.

### P0.5 WiFi Channel — Fix Proto Field Mismatch

From APK deep dive: `WifiCredentialsResponse` uses field 5 for `access_point_type` (not field 4). And `security_mode` values differ: APK uses WPA2_PERSONAL=8, our proto has WPA2_PERSONAL=1. This may explain credential caching issues noted in MEMORY.md.

**Action:** Verify against wire capture. If confirmed, fix the proto.

---

## P1: Capture-Backed New Services

These services have strong evidence from both Sony firmware AND APK, but need wire captures before implementation.

**How to capture:** Add protocol logging to AASession that dumps all `unknownMessage` signals with full payload hex. Connect phone, use AA normally, check logs.

### P1.1 Navigation Status (Service 10)

**Why it matters:** Turn-by-turn data for native UI (HUD, sidebar). Phone sends this if HU advertises the service.
**Capture trigger:** Advertise nav status in ServiceDiscoveryResponse, start Google Maps nav, capture message flow.
**Proto source:** Sony debug symbols (excellent names) + APK class `hzy.java`

### P1.2 Media Playback Status (Service 11)

**Why it matters:** Now-playing metadata for native UI (launcher, sidebar, notification bar).
**Capture trigger:** Advertise media playback in ServiceDiscoveryResponse, play music, capture.
**Proto source:** Sony symbols + APK class `hzt.java`

### P1.3 Phone Status (Service 13)

**Why it matters:** Incoming call display in native overlay.
**Capture trigger:** Advertise phone status, receive a call during AA, capture.
**Proto source:** Sony symbols + APK class `iae.java`

---

## P2: Speculative / Rare Services (Deferred)

No implementation until real use case + wire evidence. Proto definitions saved as reference only.

| Service | ID | Reason Deferred |
|---------|-----|-----------------|
| Notification | 14 | APK has no dedicated endpoint — may not be a real AA channel |
| Media Browser | 12 | APK has no dedicated endpoint — may use Android MediaBrowser instead |
| Radio | 15 | Requires tuner hardware we don't have |
| Vendor Extension | 16 | Raw data passthrough — OEM-specific, no standard behavior |
| Car Control | 19 | APK-only, complex property system, Pi has no car bus |
| Car Local Media | 20 | Stub in APK |
| Buffered Media Sink | 21 | Stub in APK |
| WiFi Discovery | 18 | No handler in APK either |

### Reference Proto Archive

Save the APK deep dive findings as `docs/apk-proto-reference.md` so they're available when P2 services are needed. Don't create .proto files or handlers until evidence gates are met.

---

## Implementation Scope (P0 Only)

| Category | Files Modified | Files Created | Tests |
|----------|---------------|---------------|-------|
| Enum fixes | 4-5 existing .proto | 1-2 new .proto | Update existing |
| Control channel | ControlChannel.cpp/.hpp | 1-2 new .proto | Add cases |
| AV message IDs | MessageIds.hpp, VideoChannelHandler, AudioChannelHandler | 0 | Log verification |
| Sensor parking brake | SensorChannelHandler.cpp/.hpp | 0 | Add test case |
| WiFi field fix | WifiSecurityResponseMessage.proto | 0 | Update test |
| **Total** | **~8 files** | **~3 proto files** | **~5 test updates** |

This is a focused, verifiable scope that improves every working session without speculative surface area.
