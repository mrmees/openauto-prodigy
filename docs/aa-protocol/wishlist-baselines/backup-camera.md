# Backup Camera and Android Auto

## Answer for the maintainer

The backup-camera feed should not be sent through Android Auto. No dedicated
rear-camera video service or HU-to-phone camera stream was found in the AA 17.3
phone endpoint set. Capture and render the camera as a native, low-latency
Prodigy surface. Coordinate the temporary display takeover with AA using the
MAIN video endpoint's focus state.

For a reverse-camera interruption, the intended state is
`NATIVE_TRANSIENT`, followed by `PROJECTED` when the interruption ends.
`NATIVE` is suitable for a persistent native Prodigy screen. `NONE` is a proto
default/sentinel and is not the correct “show the car UI” state.

## Confirmed 17.3 focus behavior

The phone endpoint is `defpackage/jdc.java` (`CAR.GAL.VIDEO`). Its direct trace
establishes the directions that matter here:

| ID | Direction | Message | Meaning |
|---|---|---|---|
| `0x8007` | Phone -> HU | `VideoFocusRequest` | Phone requests a focus mode and supplies a reason |
| `0x8008` | HU -> Phone | `VideoFocusIndication` | HU reports the display's current focus mode; `unrequested=true` marks an HU-initiated change |

Focus modes are:

| Value | Mode | Camera relevance |
|---:|---|---|
| 0 | `NONE` | Sentinel; do not use to leave projection |
| 1 | `PROJECTED` | AA visible and eligible for input |
| 2 | `NATIVE` | Persistent HU-native display ownership |
| 3 | `NATIVE_TRANSIENT` | Temporary native interruption; appropriate for camera |
| 4 | `PROJECTED_NO_INPUT_FOCUS` | Projection visible, but input focus is elsewhere |

`jdc.java:49-95` distinguishes a transient loss from a persistent native loss
and passes the transient flag into the phone display lifecycle. The
HU-to-phone indication has only `focus_mode` and `unrequested`; it carries no
camera-specific reason. `VideoFocusReason` belongs to the opposite-direction
phone request and contains no reverse-camera reason.

## Recommended transition

```text
Reverse signal / canonical camera action
        |
        +--> CameraController opens or reuses local capture
        +--> Prodigy claims local display and input
        +--> best-effort VideoFocusIndication(
               NATIVE_TRANSIENT, unrequested=true)
        +--> native camera surface becomes visible

Reverse signal clears (after configured debounce)
        |
        +--> hide/stop camera according to warm-standby policy
        +--> VideoFocusIndication(PROJECTED, unrequested=true)
        +--> restore AA surface and AA input mapping together
```

The local reverse signal is authoritative and safety critical. Camera
activation must not wait for AA, a connected phone, a focus acknowledgment, an
External API client, or a vehicle-sensor subscription. AA disconnect/failure
must leave the camera usable.

The AA `GEAR` sensor is a separate HU-to-phone semantic report. Prodigy may
publish `REVERSE` to AA when it has trustworthy gear data, but that report must
not own local camera activation.

## Why integrated overlays are not the camera path

The integrated-overlay messages in 17.3 support a phone-rendered overlay
session composited with native HU content. They do not accept an HU camera
surface or raw camera frames:

| ID | Direction in 17.3 | Behavior |
|---|---|---|
| `0x800D` | Phone -> HU | Integrated overlay parameters notification |
| `0x800E` | HU -> Phone | Start phone-side integrated overlay session |
| `0x800F` | HU -> Phone | Stop phone-side integrated overlay session |

Evidence is `itt.java:624-643` for the parameters sender and
`jdc.java:236-276` for start/stop handling. This surface may later be useful
for AA content over a Prodigy-owned dashboard, but it is not a transport for a
rear-view camera.

## Current Prodigy gap

**Code-confirmed — Prodigy:** `VideoChannelHandler::requestVideoFocus(false)`
currently sends `VideoFocusMode::NONE`, and `requestExitToCar()` uses that
boolean path. The orchestrator handles only modes 1 (`PROJECTED`) and 2
(`NATIVE`); it ignores mode 3 (`NATIVE_TRANSIENT`) and mode 4
(`PROJECTED_NO_INPUT_FOCUS`). Before camera integration, replace the boolean
API with an explicit focus-mode API and define all four non-sentinel states.

That correction belongs in a future behavior-changing plan. This baseline does
not modify the hands-off protocol library or runtime code.

## Prodigy ownership boundary

Recommended components:

- `CameraBackend`: enumerates/opens the selected V4L2 or other capture source,
  reports availability, and delivers bounded-latency frames.
- `CameraController`: owns activation, debounce, warm/cold lifetime, failure
  state, and ActionRegistry integration.
- native camera surface: owns scaling/cropping and any legally required
  guidance overlay.
- AA focus coordinator: sends focus indications best-effort and atomically
  changes AA input eligibility with the visible surface.

GPIO, CAN, automation, or a third-party daemon should dispatch one canonical
Prodigy action. They should not create UI, manipulate the AA handler, or own
camera lifecycle directly.

## Minimum promotion probe

1. Prototype with a USB webcam and a local action; measure trigger-to-first-frame
   and steady-state latency without AA.
2. While AA 17.3 is projecting, send `NATIVE_TRANSIENT` and record 0x8008,
   video start/stop behavior, last-frame behavior, and logcat.
3. Restore `PROJECTED`; confirm AA rendering and touch resume without a new
   projection session.
4. Repeat during AA connect, disconnect, phone loss, camera loss, rapid reverse
   toggles, and application restart.
5. Decide audio policy independently: preserve current media, duck it, or mute
   it based on product policy. Video focus alone is not audio focus.

Success means the native camera appears within the selected latency budget and
remains available with AA absent or failed. AA focus coordination is a
coexistence enhancement, not a prerequisite for the safety path.
