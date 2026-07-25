# True Projected Multi-Display Android Auto

## Baseline conclusion

AA 17.3 models each projected display as an independent logical instance and
encoded stream. It does not send one panoramic canvas for the HU to crop.
Prodigy must advertise one video descriptor and one matching input
configuration per logical display, then own an independent handler, AV
lifecycle, decoder, sink, focus state, and input transform for each.

Logical sinks may later be composited onto regions of one Linux framebuffer,
but that is local Prodigy rendering invisible to AA.

## Required topology

Static 17.3 validation requires:

- at least one display;
- unique display IDs;
- display ID 0 to exist and be `MAIN`;
- exactly one `MAIN`;
- at most one `CLUSTER`; and
- exactly one matching input configuration per display ID.

Do not confuse the multiplexed transport channel ID with the logical
`CarDisplayId`. The transport ID routes wire messages; the display ID joins a
video descriptor to its input descriptor and display object.

For each accepted descriptor, the phone creates its own `CarDisplayId`, display
bridge, video service, Android `Surface`, video endpoint, configurations,
encoder/lifecycle state, and focus state. MAIN, CLUSTER, and AUXILIARY map to
different phone endpoint roles. Secondary projected content remains
navigation/turn-card-led; transport capability does not prove arbitrary media
or phone UI on a secondary surface.

## Current Prodigy gap

**Code-confirmed — Prodigy:** the orchestrator owns one `videoHandler_`, one
`inputHandler_`, one decoder, and one focus-to-application-state path. Service
discovery emits one video descriptor and one input descriptor. Projected
multi-display therefore requires a registry/multi-instance refactor, not an
extra output pointer on the existing decoder.

Recommended shape:

```text
DisplayRegistry
  DisplaySession(display_id, type, video_wire_channel, input_wire_channel)
    video handler + AV state + decoder + sink + focus
    input handler + coordinate transform

PhysicalOutputRouter
  logical sink -> Qt window / DRM connector / compositor rectangle
```

Keep config index, session ID, ACK/permit accounting, teardown, focus, and
input mapping inside each `DisplaySession`.

## Delivery order

1. Ship the native semantic cluster path independently.
2. Introduce and validate the display registry while still enabling only MAIN.
3. Probe MAIN + CLUSTER descriptors and capture activation/lifecycle.
4. Add the second decoder/sink and independent focus/input.
5. Generalize the proven path to AUXILIARY and optional local composition.

## Runtime gate

Capture a MAIN + CLUSTER session, then MAIN + CLUSTER + AUXILIARY, preserving
service discovery, channel opens, per-channel setup/start/stop, distinct media
streams, focus per wire channel, input association, and phone logcat. Measure
simultaneous decoder count, CPU/GPU/memory bandwidth, compositor latency, and
failure isolation on the Pi before advertising configurations broadly.

The complete 17.3 source anchors and acceptance matrix are retained in the
sibling protocol-reference report:
`analysis/reports/multi-display/prodigy-maintainer-handoff.md`.
