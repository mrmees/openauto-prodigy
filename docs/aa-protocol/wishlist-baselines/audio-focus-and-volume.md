# Android Auto Audio Focus and Per-Stream Volume

## Baseline conclusion

AA separates projected audio by role and asks the HU for audio focus. It does
not provide a protocol for the phone to set Prodigy's per-role volume sliders.
The wishlist's per-AA-channel levels and “keep system sounds audible” behavior
are therefore Prodigy audio-policy features built on the existing separate
PipeWire streams, focus requests, and stream lifecycle.

Prodigy currently advertises and owns three AA output paths:

| Prodigy transport channel | Advertised role | Product use |
|---:|---|---|
| 4 | Media | music/podcast playback |
| 5 | Speech | navigation and Assistant speech |
| 6 | System | short system/call-related projected audio as delivered by the phone |

The exact content the phone routes into a role can evolve; policy should key on
the advertised stream role and lifecycle, not app-name guesses.

## Protocol boundary

On the control channel, the phone sends an audio-focus request and the HU
returns the resulting focus state. Requests distinguish long-lived gain,
transient gain, duck-compatible navigation/guidance gain, and release. AV
start/stop messages on each audio channel provide the authoritative stream
lifecycle.

Recommended separation:

- **AA protocol handler:** parse/respond to focus and AV lifecycle correctly.
- **AudioService:** arbitrate active Prodigy sources, duck/pause/restore, and
  apply the per-stream base level.
- **Configuration/UI:** persist `media`, `speech`, and `system` user offsets
  plus master volume.
- **EQ/routing:** remain per Prodigy stream; no AA wire change is required.

“Keep system sounds audible” should be expressed as a mix/duck rule for short
System activity while Media is active, with calls and Assistant treated by
their actual focus/lifecycle requirements. Do not fake a focus response merely
to obtain the desired volume; grant honestly, then implement the local mix.

## Questions for design, not reverse engineering

- Are per-role levels absolute or offsets from master volume?
- Does mute affect all roles, and are safety/guidance exceptions allowed?
- Which role owns phone calls on each supported phone/session path?
- What are the duck factor, ramp duration, restore order, and persistence
  behavior?
- How do AA media, local media, Bluetooth A2DP, HFP/SCO, and radio participate
  in the same recency/focus model?

## Minimum capture matrix

Record focus request, AV start/stop, active PipeWire nodes, and audible result
for media alone; nav over media; Assistant over media; touch/system sound over
media; incoming/outgoing call; rapid interruption; and reconnect. The capture
decides role classification and timing, while the desired mix remains a
Prodigy product-policy decision.
