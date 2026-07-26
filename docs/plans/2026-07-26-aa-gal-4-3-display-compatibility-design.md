# Android Auto GAL 4.3 Display Compatibility Design

Status: ACTIVE

Date: 2026-07-26

Grounded on:

- OpenAuto Prodigy `27a81a3`
- local `open-android-auto` proto pin `c8340ebbc7c7cead904096ebbb32d0d9f9335303`
- audited upstream `open-android-auto` commit
  `cabe46ec9c5e1628264427aa77d910b1f574bb34`
- Pixel 8 / Android Auto 17.3 MAIN, CLUSTER, and temporary AUXILIARY live
  captures completed on 2026-07-24 and 2026-07-25

Implementation plan:
[2026-07-26-aa-gal-4-3-display-compatibility-plan.md](2026-07-26-aa-gal-4-3-display-compatibility-plan.md)

## Decision

Add a default-off GAL 4.3 compatibility mode to the existing projected-display
laboratory while preserving GAL 1.7 as Prodigy's default. GAL 4.3 is the first
version that activates the per-`VideoConfig`
`AdditionalVideoConfig.hidden_ui_elements` behavior relevant to the current
MAIN/CLUSTER work. The implementation will not advertise GAL 5.0, 5.1, or 6.1.

The work is a protocol compatibility step, not an AUXILIARY implementation.
It must prove the 4.3 handshake and existing media flow before enabling any
4.3-only display metadata. It then migrates the one real native capability
Prodigy already provides—the shell clock—from the legacy session mask to the
per-MAIN-video hidden-UI list. A separate lab-only CLUSTER toggle may advertise
that the HU supplies a native turn card solely for an A/B capture; Prodigy does
not claim that feature in its default or accepted configuration.

## Goal

Establish a version-aware Android Auto session and service-discovery contract
that can safely exercise GAL 4.3 display features without changing the
established GAL 1.7, MAIN-only, or default-off CLUSTER behavior.

Success means:

1. The requested GAL tuple and the separately phone-reported tuple are
   observable.
2. GAL 1.7 remains the default and retains its established descriptor bytes,
   margins, clock policy, media ACK cadence, and runtime behavior.
3. GAL 4.3 can complete version exchange, TLS, service discovery, channel open,
   setup, start, and sustained MAIN+CLUSTER media on the Pixel 8.
4. At GAL 4.3, Prodigy can put `UI_ELEMENT_CLOCK` on every advertised MAIN
   `VideoConfig` without disturbing its navbar margins.
5. The CLUSTER lab can A/B `UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE` on the
   CLUSTER `VideoConfig`, with honest labeling and no production claim.
6. The community proto pin is updated without editing its tracked contents,
   and Prodigy's generated-accessor and manual message-ID consumers are
   reconciled against the audited upstream schema.

## Why GAL 4.3

The relevant phone-side behavior changes at these boundaries:

| Requested GAL | Relevant behavior | Prodigy decision |
|---|---|---|
| 1.7 | Current Prodigy source default; legacy `session_configuration` values 1/2/4 control clock/signal/battery | Preserve as default and compatibility baseline |
| 4.3 | Phone consumes per-video `hidden_ui_elements`, including native clock and native CLUSTER turn-card declarations | Implement as the only experimental upgrade target |
| 5.0 | Phone changes audio/video media acknowledgement behavior | Defer; Prodigy currently ACKs accepted media frames |
| 5.1 | Adds vehicle energy forecast service behavior (`0x8008`) | Defer; unrelated to display selection and unimplemented locally |
| 6.1 | No proven additional display-selection benefit for this goal | Defer; no evidence justifies the extra compatibility surface |

Jumping directly to the largest version would conflate display testing with
flow-control and service obligations. GAL 4.3 is the smallest useful and most
diagnosable change.

## Current Evidence

### Source and capture discrepancy

Current source requests GAL 1.7 through
`libs/prodigy-oaa-protocol/include/oaa/Session/SessionConfig.hpp` and sends the
four-byte request in `ControlChannel::sendVersionRequest`. Several historical
documents and prior capture notes say 1.1. The next live session must capture
the raw `VERSION_REQUEST` before any upgrade and establish the deployed truth.
The design treats 1.7 as the source baseline but does not rewrite the historical
capture as though it had already been re-proven.

The existing `VERSION_RESPONSE` parser consumes only the first six bytes:
major, minor, and status. It discards the appended control-channel
configuration used by modern protocol versions and does not retain the
phone-reported version. That is insufficient for a compatibility experiment.

### Multi-display baseline

Prodigy currently owns two explicit projected-display sessions:

- MAIN: display ID 0, video channel 3, input channel 1, application-wide focus
  and physical touch.
- optional CLUSTER: display ID 1, video channel 12, input channel 13, its own
  handler, decoder, sink, lifecycle, and process-lifetime runtime profile.

The Pixel 8 has live-proven simultaneous MAIN+CLUSTER video. A disposable role
swap also proved AUXILIARY/TURN_CARD on the same secondary channels, then the
tree and Pi were restored to CLUSTER. This design does not generalize those two
sessions or introduce a third display.

### Wire fields that must remain distinct

| Field | Meaning in this design |
|---|---|
| `ChannelDescriptor.channel_id` field 1 | transport service/channel ID |
| `AVChannel` field 6 | logical display ID; audited upstream accessor is `display_id` |
| `AVChannel.display_type` field 7 | MAIN, CLUSTER, or AUXILIARY role |
| `AVChannel.keycode` field 8 | initial AUXILIARY selector; omitted for CLUSTER |
| `InputChannelConfig.display_id` field 5 | matching logical display ID |
| `VideoConfig.additional_config` field 11 | per-video modern display/UI metadata |
| `AdditionalVideoConfig.hidden_ui_elements` field 5 | HU-provided native UI declarations at requested GAL 4.3+ |
| `ServiceDiscoveryResponse.session_configuration` field 13 | legacy session flags used below GAL 4.3 |

GAL 4.3 does not select CLUSTER versus AUXILIARY, does not choose an AUXILIARY
provider, and does not replace AV field 8.

## Design

### 1. Pin the audited community schema; do not patch it locally

Advance the `libs/prodigy-oaa-protocol/proto` gitlink from `c8340eb` to the
audited upstream commit `cabe46e`. No tracked file inside the submodule may be
edited. If execution occurs after upstream advances, the implementation still
uses the audited commit unless a new review explicitly re-grounds this design.

The pin changes generated C++ source contracts while retaining the relevant
wire numbers:

- AV field 6 is renamed from `channel_id` to `display_id`.
- AV field 1 uses `MediaCodecType` rather than `AVStreamType`; existing audio
  value 1 and video value 3 stay wire-compatible.
- `KEYCODE_NAVIGATION` value 65538 becomes available beside
  `KEYCODE_TURN_CARD` value 65544, but neither is used by this phase.
- Additional-video fields 1-3 are corrected to inset-shaped messages and field
  8 adds blended-UI configuration. This phase sets none of those fields.
- The generated AV message-ID map corrects the modern IDs. Prodigy's manual
  `MessageIds.hpp` must be reconciled before any 4.3 live run so incoming
  messages are not dispatched under stale shifted names.

The compatibility pass must explicitly test stable numeric values and the
unchanged serialized legacy descriptors. A successful compile is not enough.

### 2. Represent versions as ordered numeric values

The protocol library continues to carry requested major/minor values in
`oaa::SessionConfig`, but application policy uses a typed GAL version value.
It must compare `(major, minor)` numerically and format only at the UI/logging
boundary. Floating point and lexicographic string comparisons are forbidden.

The projected-display lab accepts exactly two modes:

- `1.7` — default, status-only legacy mode.
- `4.3` — experimental, minimum-compatible-response mode.

No arbitrary major/minor entry is exposed. This prevents an accidental 5.x or
6.x request from silently crossing obligations outside this design.

For implementation economy, the process-lifetime selector lives in the
existing CLUSTER lab profile and uses its existing atomic staging and AA-only
reconnect boundary. That ownership is a laboratory control affinity, not a
claim that GAL is display-scoped. `ServiceDiscoveryBuilder` extracts the value
into the session-wide `SessionConfig`; per-display flags remain on their
respective `VideoConfig` messages.

The mode is available only when the existing
`experimental_cluster_display` startup flag created the lab controller. A
durable global GAL setting is follow-up work and is not needed for this bounded
experiment.

### 3. Preserve the full version response and make 4.3 fail closed

`ControlChannel` must deliver:

- response major;
- response minor;
- raw 16-bit status; and
- all bytes after the fixed six-byte prefix.

The trailing bytes are preserved even if the optional generated
`ControlChannelConfigWrapper` cannot parse them. Diagnostics may log a bounded
hex length/summary and a protobuf `ShortDebugString` when parsing succeeds.
Malformed optional configuration does not turn a successful legacy response
into an out-of-bounds read or crash.

`AASession` logs the requested version, phone-reported version, status, and
whether trailing configuration was present. Existing 1.7 acceptance semantics
remain unchanged: status MATCH is authoritative, as it is today. Experimental
4.3 additionally requires the phone-reported numeric tuple to be greater than
or equal to the requested 4.3 tuple. Reported 4.3 and 6.0 are compatible;
reported 4.2 fails before TLS/service discovery with `VersionMismatch`.

The requested 4.3 tuple remains the sole input to Prodigy's local feature
policy. The separately reported compatible tuple is diagnostic only and is
never promoted into `SessionConfig`, descriptors, ACK policy, or 5.x/6.x
behavior.

No automatic retry at 1.7 is added. A silent fallback would contaminate the
experiment and make captures ambiguous; the operator can select 1.7 and
reconnect explicitly.

### 4. Keep the GAL 1.7 path byte-compatible

When the lab requests 1.7:

- `SessionConfig.protocolMajor/minor` remain 1/7.
- navbar clock ownership continues to set legacy
  `session_configuration = 1`.
- no `VideoConfig.additional_config` is created by this feature.
- the old experimental `session_configuration |= 16` behavior is removed;
  AA 17.3 has no consumer for it.
- MAIN-only and CLUSTER-disabled descriptors retain their current golden
  bytes.
- media ACK behavior is unchanged.

The native-turn-card lab option is false by default and cannot be true in a
valid 1.7 profile. A single atomic update may move to 4.3 and enable it, or move
back to 1.7 and disable it. Invalid combinations are rejected without partial
mutation or reconnect.

### 5. Serialize only supported 4.3 UI features

At requested GAL 4.3, feature policy moves from a session mask to the relevant
video configuration:

| Prodigy behavior | Descriptor output |
|---|---|
| Navbar shown during AA, so Prodigy supplies a clock | Add `UI_ELEMENT_CLOCK` to every MAIN `VideoConfig.additional_config.hidden_ui_elements` |
| Navbar hidden during AA | Do not advertise `UI_ELEMENT_CLOCK` |
| CLUSTER lab native-turn-card toggle false | Do not create a CLUSTER additional config solely for this feature |
| CLUSTER lab native-turn-card toggle true | Add `UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE` to the one CLUSTER video config |

The 4.3 path sets legacy `session_configuration` to zero for these UI features.
It never sets AdditionalVideoConfig fields 1-4 or 6-8. In particular, it does
not populate the corrected inset fields and does not duplicate the legacy
margin values inside field 11.

Every MAIN codec configuration gets the same clock declaration. Otherwise a
phone codec choice could also change UI behavior.

The CLUSTER toggle is renamed from the misleading session-bit wording to
`native_turn_card_available` at the runtime action boundary and
`nativeTurnCardAvailable` internally. Its Debug Settings label must say that it
advertises a native HU turn card and that it is a lab experiment. The enum
means the HU already supplies that feature; it does not ask the phone to send
turn data. Because Prodigy does not yet implement a native semantic turn-card
widget, the accepted/default state is false and every live true run must be
returned to false after capture.

### 6. Preserve margins and display roles

Adding field 11 must not alter the existing legacy margin fields on the same
`VideoConfig`. The final tree must prove both structurally and on hardware that
the 300x300 CLUSTER content rectangle still arrives centered inside its
800x480 carrier and that MAIN navbar-aware margins remain unchanged.

The experiment does not change:

- display IDs, service channels, or input pairing;
- MAIN and CLUSTER display types;
- CLUSTER omission of AV field 8;
- configured carrier resolution, DPI, codec, or frame rate;
- decoder, crop, sink, focus, or touch ownership; or
- channel reopen and session teardown rules.

### 7. Reuse the current runtime staging boundary

Extend the existing `aa.cluster.applyProfile` map with:

| Key | Accepted values | Default |
|---|---|---|
| `gal_version` | `"1.7"`, `"4.3"` | `"1.7"` |
| `native_turn_card_available` | boolean | `false` |

The existing resolution, DPI, and content geometry keys are unchanged. The old
`turn_data_available` key is rejected rather than silently retaining the
incorrect session-bit interpretation. These actions are experimental,
registered only with the CLUSTER lab, and are not frozen External API
protobuf fields.

A valid change stages one complete profile generation. If AA is connected, it
uses the existing graceful AA-only disconnect/retrigger path. The requested
version, service-discovery policy, decoder geometry, and QML crop activate from
the same staged generation before the replacement session. Invalid or no-op
updates do not reconnect.

Reset restores the full baseline: GAL 1.7, 480p carrier, 140 DPI, 300x300
content, and native-turn-card false.

### 8. Stage the live experiment to isolate causality

Hardware validation is deliberately split:

1. Capture the unmodified deployed request/response and established 1.7
   session before changing the proto pin.
2. After the schema compatibility pass, add the 4.3 request and response
   handling but no modern descriptor output. Cross-build and prove a sustained
   4.3 MAIN+CLUSTER session. If this fails, stop and return to 1.7; do not add
   hidden UI metadata to an unproven handshake.
3. Add the version-gated hidden-UI serialization and run the final matrix.

This intermediate artifact is a test instrument, not a publication target.
It is not pushed or tagged mid-execution.

## Live Capture Matrix

| Case | Requested GAL | Descriptor policy | Native turn-card flag | Required observations |
|---|---:|---|---:|---|
| A | 1.7 | legacy baseline | false | raw request/full response, exact existing MAIN+CLUSTER descriptors, channel/media health |
| B | 4.3 | request-only intermediate artifact | false | MATCH response reporting at least 4.3, TLS/discovery, sustained MAIN+CLUSTER video/audio, ACK health |
| C | 4.3 | MAIN clock through field 11 | false | phone status-bar clock response, unchanged MAIN margins and touch |
| D | 4.3 | MAIN clock; CLUSTER field 5 absent | false | route active/inactive CLUSTER behavior and exact 300x300 crop |
| E | 4.3 | MAIN clock; CLUSTER field 5 value 5 present | true | phone logcat feature state/turn-card policy, route active/inactive CLUSTER behavior, exact crop |

Each case records:

- exact Prodigy commit and aarch64 binary hash;
- raw `VERSION_REQUEST` and complete `VERSION_RESPONSE`;
- decoded service-discovery response and serialized per-display video configs;
- channel open/setup/start, first NAL/frame, ACK cadence, focus, and input logs;
- phone logcat evidence for provider selection and
  `EXTRA_SHOW_TURN_CARD`/equivalent feature state where available;
- MAIN and CLUSTER screenshots with an active route and no route;
- CLUSTER decoded carrier and content geometry; and
- Pi CPU/memory plus continuity of audio and touch.

Cases F (AUXILIARY/NAVIGATION) and G (three simultaneous displays) are not part
of this plan. Their presence in a future capture matrix requires separate
promotion after this compatibility layer is accepted.

## Failure and Rollback Semantics

- Baseline request is not 1.7: preserve the raw capture, correct the baseline
  documents/design before proceeding, and do not infer the deployed version
  from source.
- Proto pin changes a legacy serialized descriptor: stop and explain every
  byte before a live run; numeric coincidence is not assumed.
- 4.3 response status is non-MATCH or reports below 4.3: disconnect
  before TLS/service discovery, log the mismatch, restore/select 1.7.
- Optional response configuration is malformed: retain bounded raw bytes and
  continue only if the fixed response and version policy are valid.
- 4.3 handshake succeeds but media stalls or ACK behavior changes: restore
  1.7 and treat GAL 4.3 as not yet compatible; do not jump to 5.x or suppress
  ACKs speculatively.
- MAIN or CLUSTER margins change after field 11 appears: capture and roll back
  the modern descriptor policy; do not compensate with QML offsets.
- Native-turn-card true suppresses phone content: record it as confirmation of
  the declaration semantics, return the flag to false, and do not call it a
  missing phone stream.
- Any regression outside the AA session: restore the preserved Pi binary and
  configuration through the existing guarded deployment procedure.

## Explicit Non-goals

- GAL 5.0, 5.1, 6.1, or arbitrary version selection.
- ACK suppression or other ackless-media implementation.
- Vehicle energy forecast support.
- A durable production GAL setting or YAML migration.
- AUXILIARY/NAVIGATION, durable AUXILIARY/TURN_CARD, or a third display.
- A generalized display registry or dynamic channel allocation.
- A native semantic navigation/turn-card widget.
- Provider forcing, live descriptor replacement, or
  `ServiceDiscoveryUpdate` use.
- Overlay, blended UI, inset, resize-action, theme, or UI-config protocol work.
- Any edit within `libs/prodigy-oaa-protocol/proto/` beyond advancing its
  gitlink to the audited upstream commit.
- Any change to frozen `proto/api/` or new External API capability.

## Acceptance Criteria

- The source and deployed baseline version discrepancy is resolved with a raw
  capture.
- The audited proto gitlink is advanced with no local submodule content edits.
- Generated accessor/type changes and manual AV message IDs are reconciled and
  numerically tested.
- GAL 1.7 remains the default; its MAIN-only and CLUSTER-enabled legacy
  descriptors and session policy remain covered by golden tests.
- The complete phone version response is bounded, retained, and diagnosed.
- Experimental 4.3 accepts a MATCH response at or above 4.3 and rejects a
  lower or non-MATCH response before discovery.
- The handshake-only 4.3 checkpoint sustains existing MAIN+CLUSTER media before
  field 11 is enabled.
- GAL 4.3 clock metadata appears on every MAIN video config exactly when the
  navbar supplies a clock.
- The CLUSTER native-turn-card metadata is absent by default and appears only
  under the explicit 4.3 lab toggle.
- No 1.7 descriptor contains `additional_config`; no path emits the disproven
  session bit 16.
- MAIN margins/touch and the CLUSTER 800x480 carrier/300x300 centered content
  remain correct in automated and live checks.
- The final tree passes the repository's native build, explicit application
  target, CTest, aarch64 cross-build, documentation checks, and one bounded
  major review.
