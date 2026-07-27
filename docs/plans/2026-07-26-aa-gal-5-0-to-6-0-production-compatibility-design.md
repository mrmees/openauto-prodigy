# Android Auto GAL 5.0–6.0 Production Compatibility Design

Status: ACTIVE

Date: 2026-07-26

Grounded on:

- OpenAuto Prodigy `deeecca8e66455f25cc384d1fb5e99cfa38be4d6`
- hardware-accepted GAL 4.3 behavior at
  `d06fa40a2d9141f4a62155ce75e3bb3d2d2550f3`
- open-android-auto release `v1.5`, tagged main
  `61eab61c5f9968154ff1a80faa8c0a427b208479`
- open-android-auto consumer `dist`
  `5ff4aa218dd33913237993f2968bf70e16dc464e`
- Android Auto 17.3 static gate audit at
  [open-android-auto `61eab61c`](https://github.com/mrmees/open-android-auto/blob/61eab61c5f9968154ff1a80faa8c0a427b208479/analysis/reports/gal-version-gates-above-4.3.md)
- Pixel 8 live evidence that a requested GAL 4.3 session can receive a
  reported GAL 6.0/MATCH response while retaining requested-4.3 behavior

Implementation plan:
[2026-07-26-aa-gal-5-0-to-6-0-production-compatibility-plan.md](2026-07-26-aa-gal-5-0-to-6-0-production-compatibility-plan.md)

## Decision

Prodigy will implement GAL 5.0, 5.1, and 6.0 as staged production
compatibility levels. A version is experimental only while its implementation
branch is being tested. Once its repository and hardware acceptance gates
pass, it becomes a supported production choice and the compiled default
advances to that highest accepted version.

The completed product will support exactly GAL 1.7, 4.3, 5.0, 5.1, and 6.0.
GAL 6.0 will be the default after its final hardware matrix passes. GAL 1.7
remains an explicit compatibility fallback, not the final default. GAL 6.1 is
excluded because the current evidence identifies it as a phone response
ceiling without a distinct requested-version benefit.

The upgrade is a protocol-compatibility program, not a display lab. GAL
selection becomes session-wide durable production configuration and is
removed from the CLUSTER laboratory profile. Existing CLUSTER geometry and
native-turn-card experiments remain separately scoped.

## Goal and completion boundary

Completion means Prodigy can request GAL 6.0 by default and sustain the
supported wireless Android Auto workflow on the Pi:

1. version exchange, TLS, service discovery, and channel lifecycle complete;
2. MAIN and optional CLUSTER video remain correctly composed with Navbar
   margins and companion insets;
3. media, speech, system, and assistant audio remain healthy;
4. touch, phone keys, navigation, focus, and AA-only reconnect remain healthy;
5. every evidenced phone-to-HU message enabled through GAL 6.0 is typed and
   bounded locally, even when Prodigy does not consume its application meaning;
6. each advertised display has one codec type at GAL 5.0 and above;
7. the existing hardware-accepted H.265 product default remains first while
   H.264 remains a supported explicit fallback; and
8. explicit lower-version choices continue to use only the obligations of
   their requested version.

Success does not require GAL 6.1, integrated overlays, an EV UI, semantic
interpretation of unresolved media-option fields, outgoing media statistics,
automatic version fallback, a third display, AUXILIARY changes, or a protocol
repository split.

## Evidence and version semantics

Android Auto stores the raw HU-requested tuple separately from the tuple it
reports in `VERSION_RESPONSE`. Phone-side feature gates use the raw requested
tuple. Prodigy therefore keeps two values with different authority:

| Value | Authority |
|---|---|
| Requested GAL | Sole input to local descriptors, ACK policy, message expectations, codec constraints, and every other local feature gate |
| Phone-reported GAL | Admission and diagnostics only; never upgrades local behavior |

For requested GAL 4.3 and above, MATCH plus a reported tuple numerically
greater than or equal to the request is accepted. A lower reported tuple
fails before TLS. The legacy 1.7 path retains its established status-based
acceptance behavior. Prodigy does not silently retry a lower version; the
operator can select an explicit supported fallback when diagnosing an older
phone.

The Android Auto 17.3 audit predicts a 6.1 response ceiling, while the live
Pixel returned 6.0 for a 4.3 request. This discrepancy remains diagnostic. It
does not affect local policy and must be re-observed at every live phase.

## Protocol release boundary

`libs/prodigy-oaa-protocol/proto/` remains hands-off. Before new GAL behavior,
Prodigy will move its gitlink from the temporary open-android-auto main commit
`cabe46ec9c5e1628264427aa77d910b1f574bb34` to exact `v1.5` consumer `dist`
commit `5ff4aa218dd33913237993f2968bf70e16dc464e`.

The two pins produce byte-identical descriptor sets. Their only `.proto`
source differences are comments in `AVChannelMediaConfigMessage.proto` and
`AVChannelStartIndicationMessage.proto`. The `dist` snapshot intentionally
contains only 248 protos, README, and LICENSE. Research and audit documents
stay on open-android-auto main and are cited by immutable URL/SHA rather than
consumed through the submodule.

Future schema corrections follow the same route: open-android-auto main audit,
tag, generated `dist`, exact Prodigy gitlink. Prodigy never patches community
schemas locally.

## Session-wide policy architecture

### One typed requested-version policy

Create a small reusable protocol-library value object in
`oaa/Session/SessionProtocolPolicy.hpp`:

```cpp
struct ProtocolVersion {
    uint16_t major;
    uint16_t minor;
    // Numeric pair comparisons; no floating-point or string ordering.
};

class SessionProtocolPolicy {
public:
    explicit constexpr SessionProtocolPolicy(ProtocolVersion requested);
    constexpr ProtocolVersion requestedVersion() const;
    constexpr bool atLeast(ProtocolVersion threshold) const;
    constexpr bool requiresMinimumCompatibleResponse() const;
    constexpr bool usesModernDisplayPolicy() const;
    constexpr bool usesAcklessAudio() const;
    constexpr bool requiresSingleVideoCodecPerDisplay() const;
    constexpr bool acceptsAudioMediaOptions() const;
    constexpr bool acceptsVehicleEnergyForecast() const;
    constexpr bool acceptsVideoMediaOptions() const;
};
```

The same header defines exact constants for 1.7, 4.3, 5.0, 5.1, and 6.0.
`SessionConfig.protocolMajor/minor` remain the serialized source tuple, but
`requireMinimumCompatibleProtocolVersion` is replaced by policy derivation so
the tuple and admission rule cannot diverge. Direct handler tests default to a
1.7 policy.

`IChannelHandler` gains a default no-op
`configureSession(const SessionProtocolPolicy&)`. `AASession::registerChannel`
calls it before a handler can open. Audio, video, and navigation handlers store
the injected value for that session. This is the correct boundary because the
handlers live for the orchestrator lifetime while `AASession` and
`SessionConfig` are recreated for every connection.

For start-detail diagnostics, an absent `session_type` is represented as `-1`
and an absent `media_config` produces `hasMediaConfig=false` with an empty
summary. This preserves the distinction between an absent optional field and
the enum's numeric zero value.

Known modern messages are always parsed safely if they arrive. A policy
threshold changes expected diagnostics or active behavior; it does not turn a
well-formed known message into a disconnect merely because a phone sent it
below the expected threshold.

### Capability thresholds

| Requested GAL | Local policy activated |
|---|---|
| 1.7 | Legacy session UI mask and per-frame audio/video ACK behavior |
| 4.3+ | Per-video AdditionalVideoConfig UI policy and companion insets |
| 4.5+ | No new HU wire message; telephone-key behavior changes on the phone and is regression-tested |
| 5.0+ | Ackless audio, extended audio-start tolerance, and one codec type per display |
| 5.1+ | Typed audio MediaOptions and navigation VehicleEnergyForecast handling |
| 6.0+ | Typed extended video start and standalone video MediaOptions handling; accepted H.265 default with H.264 fallback |

The 5.0 gate is audio-specific in the current evidence. Video continues its
per-packet receive ACKs through GAL 6.0 unless new framed evidence establishes
a different HU obligation. AVInput is the reverse HU-to-phone flow and is not
made ackless by this policy.

## Production configuration and promotion

The durable setting is `connection.gal_version`. It accepts only the versions
Prodigy has implemented and hardware-accepted at that point. The final picker
contains `1.7`, `4.3`, `5.0`, `5.1`, and `6.0`; arbitrary tuples and 6.1 are
rejected.

The compiled default advances only with accepted evidence:

| Checkpoint | Compiled default after acceptance |
|---|---|
| Existing accepted baseline | 4.3 |
| GAL 5.0 matrix | 5.0 |
| GAL 5.1 matrix | 5.1 |
| GAL 6.0 matrix | 6.0 |

A missing or invalid YAML value resolves to the highest accepted version with
a bounded warning. An explicit valid lower-version choice remains pinned
across upgrades. Changing the setting during an active/backgrounded AA session
uses the existing graceful AA-only reconnect. A change made before an active
session is picked up by the next session snapshot.

The normal Android Auto Settings page owns the production picker. Debug
Settings and `aa.cluster.applyProfile` lose `gal_version`; the CLUSTER profile
continues to own only resolution, DPI, content geometry, and its experimental
native-turn declaration. The native-turn declaration is serialized only when
the independent session GAL policy is 4.3 or newer.

## Staged behavior

### Stage 0 — released schema baseline

Update only the submodule gitlink to the exact `v1.5` `dist` SHA and prove that
the generated bindings, app, tests, and ARM artifact remain green. This stage
must land before behavioral changes so later regressions cannot be attributed
to an unreleased schema checkout.

### Stage 1 — production policy and accepted 4.3 promotion

Move GAL ownership out of `ProjectedClusterProfile`, add the production
setting and session-policy injection, and generalize the accepted 4.3 display
checks from equality to the policy threshold. The compiled default becomes
4.3 because `d06fa40` already passed the live GAL 4.3 matrix.

The serialized GAL 1.7 descriptors remain golden-compatible. GAL 4.3 keeps
the accepted MAIN clock/inset and CLUSTER native-turn behavior. This stage
must not change audio ACKs or codec cardinality below 5.0.

### Stage 2 — GAL 5.0

At requested 5.0 and above:

- audio channels 4, 5, and 6 deliver accepted packets without sending
  `AVMediaAckIndication`;
- video continues receive ACKs;
- `AVChannelStartIndication` is fully parsed and bounded diagnostics include
  `config`, optional `session_type`, and optional 13-field `media_config`;
- each display advertises exactly one codec type; and
- MAIN and CLUSTER use the same first recognized configured codec, with the
  shipped ordering making H.265 the default and H.264 the fallback.

Setup response `max_unacked` remains unchanged unless live framed evidence
shows it must differ. The current evidence proves that the phone stops
depending on audio ACK replenishment; it does not prove a new value for that
field.

The 5.0 hardware checkpoint proves sustained media, speech, system audio,
Assistant microphone, MAIN/CLUSTER video, input, direct telephone-key behavior,
and reconnect with no audio ACKs on the wire. Only then does 5.0 become the
default.

### Stage 3 — GAL 5.1

Audio `0x8014` is parsed as `AVChannelMediaOptions`. Because the 13 field
meanings remain unresolved, Prodigy records only presence, size, and a bounded
`ShortDebugString`; it does not apply the nested `PingConfiguration` values to
session keepalive or media timing.

Navigation `0x8008` is parsed first as `VehicleEnergyForecastMessage`, then
its bytes field is conditionally parsed as `VehicleEnergyForecast`. The
handler records a bounded structural summary and whether the inner message
parsed. No EV UI, range calculation, persistence, or response is added.

Synthetic serialized messages prove both paths even if the non-EV test vehicle
identity causes the phone not to deliver them live. The live checkpoint still
proves route, music, assistant, display, input, and reconnect health before
5.1 becomes the default.

### Stage 4 — GAL 6.0 and H.265 regression acceptance

Video start indications parse and diagnose optional `session_type` and
`media_config`. Standalone video `0x8014` parses the same audited options
envelope. Malformed known payloads produce bounded warnings and do not crash,
read out of bounds, or mutate session policy.

Prodigy does not fabricate `AVChannelMediaStats` (`0x8013`). Existing
instrumentation measures received packets, ACKs, decoder queue time, decode
time, copy time, receive-to-frame-storage time, FPS, and queue depth. The
schema's presentation, drop, cadence, and unit semantics are not sufficiently
proven to send truthful stats, and current evidence does not establish that
GAL 6.0 requires them.

H.265 is an existing accepted product decision, repeatedly validated in live
Pi/phone projection before this GAL program. GAL 5.0's single-codec rule made
the legacy H.264-first capability list decisive by accident; it did not expose
a new codec, decoder, render, framing, or ACK constraint. Restore the compiled
ordering to H.265 then H.264. Explicit configuration order remains
authoritative, and the H.264 GAL 6.0 fallback was proven on `2bc574e`.

The GAL 6.0 hardware gate therefore runs one H.265 regression/acceptance
matrix, not a new codec bake-off. The operator checks negotiation and one
H.265 descriptor per MAIN/CLUSTER display, projection and visual health, every
audio role, Assistant, navigation, input, focus, and service stability as
independent checkpoints. The matrix also includes three AA-only reconnects, a
second-phone connection/media/reconnect smoke, and explicit lower-GAL smokes
before restoring 6.0. Diagnostics remain evidence, but there is no timed
functional choreography or comparative resource threshold.

## Error handling and diagnostics

- Unsupported configured GAL strings never become wire requests; resolve to
  the highest accepted version and log once per session build.
- MATCH with a reported tuple below a requested modern tuple fails before TLS.
- The phone-reported tuple never raises local feature policy.
- Malformed extended-start, media-options, or energy-forecast messages produce
  bounded diagnostics and no unbounded hex/protobuf dump.
- A malformed optional energy-forecast inner payload does not invalidate an
  otherwise parseable outer wrapper.
- Unknown messages remain observable without adding speculative semantics.
- No phase adds an automatic downgrade loop. Operators retain exact supported
  lower-version choices.
- After two failed remediation attempts in one phase, restore the last
  hardware-accepted checkpoint and re-evaluate the evidence before continuing.

## Verification and promotion gates

Every code stage is test-first where behavior is deterministic. Focused tests
cover numeric version ordering, allowed settings, response admission,
descriptor thresholds, codec cardinality, policy injection across reused
handlers, ACK presence/absence, extended-start fields, media options, energy
forecast, malformed payloads, and legacy behavior.

Each candidate then passes the repository's native build, explicit app target,
CTest, ARM cross-build, and relevant Pi/phone checkpoint. Hardware evidence
records requested/reported tuples, descriptor/codec choice, channel opens,
audio ACK counts, video receive/ACK counts, decoder summaries, reconnects,
service health, executable identity, and rollback artifact.

The Pixel 8 is the required reference phone. At GAL 6.0, at least one other
available supported test phone receives a connection/media/reconnect smoke
test before production promotion. Absence of a conditionally delivered
MediaOptions or VehicleEnergyForecast message is recorded as an evidence
limit; its typed synthetic/replay coverage remains mandatory.

The complete major range receives one bounded Fable review after final green
verification. Hardware-accepted SHAs are recorded at every phase boundary.
Findings do not widen this design: new capabilities go to the wishlist and
concrete unrelated defects go to the engineering backlog.

## Documentation impact

Implementation updates the current AA rendering/protocol guidance, YAML
schema, Settings reference, roadmap, index, and session handoff. Historical
archived plans remain unchanged. On completion, this design and its plan are
marked completed and moved to `docs/archive/plans/` in the same commit.
