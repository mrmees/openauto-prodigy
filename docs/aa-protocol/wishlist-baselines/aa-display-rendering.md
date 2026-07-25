# Android Auto Multi-display Handoff for OpenAuto Prodigy

Date: 2026-07-24
Android Auto reference: `17.3.662804-release` (`173662804`)
Audience: OpenAuto Prodigy maintainer

## Decision in one paragraph

Model every projected Android Auto display as an independent logical display
session. Do **not** ask Android Auto for one panoramic framebuffer and crop it
into screens. For each MAIN, CLUSTER, or AUXILIARY display, Prodigy should
advertise a separate video `ChannelDescriptor`, give it a unique display ID,
provide exactly one input configuration with the same display ID, and create a
separate video-channel handler, decoder state, output sink, and focus state.
Those logical sinks may be composited onto one physical framebuffer afterward,
but that is a local Prodigy rendering decision invisible to Android Auto.

## Confidence labels used here

- **Confirmed — 17.3 static:** directly observed in the decompiled 17.3 phone
  code, with exact source anchors preserved in the sibling protocol-reference
  repository's `android-auto-17.3.md` report.
- **Confirmed — protocol:** represented by the repository's verified protobufs
  and existing capture-backed channel work.
- **Implementation recommendation:** the proposed Prodigy design follows from
  the confirmed protocol model but has not yet been implemented in Prodigy.
- **Open — runtime:** static evidence is strong, but a live multi-display 17.3
  session has not yet been captured.

## The protocol model

The head unit returns a `ServiceDiscoveryResponse` containing one video-capable
`ChannelDescriptor` for every logical projected display. Android Auto 17.3
processes every such descriptor independently:

```text
ServiceDiscoveryResponse
  ChannelDescriptor(wire channel A)
    AVChannel(display_id=0, display_type=MAIN, configs=[...])
  ChannelDescriptor(wire channel B)
    AVChannel(display_id=1, display_type=CLUSTER, configs=[...])
  ChannelDescriptor(wire channel C)
    InputChannelConfig(display_id=0, ...)
  ChannelDescriptor(wire channel D)
    InputChannelConfig(display_id=1, ...)

Android Auto phone                         OpenAuto Prodigy
  MAIN CarDisplay/Surface/encoder   <----> MAIN channel/decoder/sink/input
  CLUSTER CarDisplay/Surface/encoder <----> CLUSTER channel/decoder/sink/input
```

The streams share the encrypted Android Auto transport, but each uses its own
multiplexed channel and its own AV lifecycle. Android Auto does not provide a
wire mechanism that labels rectangles of one stream as separate physical
displays.

### Do not confuse these identifiers

| Identifier | Wire location | Meaning | Prodigy use |
|---|---|---|---|
| Transport channel ID | `ChannelDescriptor.channel_id`, field 1 | Ephemeral multiplexed channel used to route frames and messages | Key the protocol handler/session routing table |
| Display ID / `CarDisplayId` | `AVChannel` field 6, currently named `channel_id` in this repository | Stable identity of one logical display within the projection session | Key the display registry and join video to input |
| Display type | `AVChannel.display_type`, field 7 | `MAIN=0`, `CLUSTER=1`, or `AUXILIARY=2` | Select content role and endpoint behavior |
| Input display ID | `InputChannelConfig.display_id`, field 5 | Reference to the logical display that receives this input | Must equal the corresponding AV display ID |
| Video config index | AV setup/start messages | Selected entry from that display's `VideoConfig` list | Keep inside the individual video session; it is not a display ID |

The field-6 name in `oaa/av/AVChannelData.proto` is historically ambiguous.
The 17.3 phone code reads that field and immediately constructs
`new CarDisplayId(value)`. Prodigy must therefore treat it as a display ID, not
as a duplicate of `ChannelDescriptor.channel_id`. Renaming the canonical proto
field is a separate compatibility/API decision; no wire schema change is
needed.

## Display configuration matrix

| Configuration | What Prodigy advertises | Confirmed phone behavior | Recommended Prodigy output |
|---|---|---|---|
| MAIN only | One video descriptor with display ID 0 and type MAIN; one matching input config | Required baseline. Display ID 0 must exist and must be MAIN | Existing primary decoder and full-screen sink |
| MAIN + CLUSTER | MAIN ID 0 plus one unique CLUSTER display ID; one matching input config per display | Phone creates two independent display/video objects. At most one CLUSTER is accepted | Add a second video handler, decoder, cluster sink, and focus state |
| MAIN + AUXILIARY | MAIN ID 0 plus one or more unique AUXILIARY display IDs; matching input config for each | Phone creates an independent display/video object for every accepted descriptor. The observed validator does not impose a numeric AUXILIARY limit | Add one `DisplaySession` per AUXILIARY descriptor; constrain count by Prodigy/hardware capability |
| MAIN + CLUSTER + AUXILIARY | One MAIN, no more than one CLUSTER, and each AUXILIARY with unique IDs and matching inputs | Static 17.3 construction and validation support this topology | Run all logical video sessions independently; then map sinks to physical outputs |
| One display with native chrome or reserved regions | One MAIN video descriptor whose `VideoConfig.additional_config` supplies insets, margins, corner radii, hidden elements, or blended native regions | Phone changes composition geometry within that one display | Keep one decoder/sink; composite OEM chrome around or over that sink |
| Native HU-rendered cluster/secondary widgets | Navigation/media/phone semantic channels, possibly without a secondary video descriptor | Separate protocol path; the HU renders its own UI from data rather than pixels | Build native Prodigy widgets and do not pretend they are a second projected stream |

The fourth row is **statically supported**, not yet proven by a simultaneous
17.3 wire capture. In particular, the exact concurrent channel IDs, encoder
start order, and focus transitions remain runtime verification items.

## What Android Auto 17.3 actually constructs

For each video-capable descriptor, 17.3 creates:

- a `CarDisplayId` from `AVChannel` field 6;
- a display type from `AVChannel` field 7;
- one display/composition bridge (`iti`);
- one video service (`itt`);
- one Android `Surface` for that video service;
- one video endpoint (`jdc`) mapped to VIDEO, VIDEO_CLUSTER, or
  VIDEO_AUXILIARY;
- per-instance video configs, encoder, lifecycle, and focus state; and
- one matching input binding selected by display ID.

The topology validator then requires:

- at least one display;
- unique display IDs;
- display ID 0 to exist and be MAIN;
- exactly one MAIN;
- at most one CLUSTER; and
- exactly one matching input configuration per display.

These are object-topology constraints, not viewport constraints. They are the
strongest evidence against the panoramic-canvas interpretation.

### Focus is also per display endpoint

Each display's video endpoint owns its own focus state and handles the standard
video focus modes:

- `PROJECTED`
- `NATIVE`
- `NATIVE_TRANSIENT`
- `PROJECTED_NO_INPUT_FOCUS`

That means Prodigy should dispatch focus messages to the `DisplaySession`
identified by the message's transport channel. A single global video-focus
boolean would couple MAIN and secondary displays incorrectly. Static code
confirms endpoint-local state; a live capture is still needed to characterize
which 17.3 focus transitions occur concurrently in real cars.

## Content policy versus transport capability

The transport model and the content policy are separate questions:

- MAIN receives the full projected Android Auto UI.
- Earlier 16.2 semantic traces route CLUSTER and AUXILIARY projection primarily
  to navigation maps or turn cards.
- The 17.3 manifest still contains dedicated cluster, auxiliary-navigation,
  auxiliary-turn-card, template-cluster, and template-auxiliary services.
- Cluster creation in 17.3 checks navigation-app compatibility and both cluster
  and auxiliary displays can be filtered by power-saving policy.

This is good evidence that secondary projected displays remain navigation-led,
but it is not a live 17.3 proof that every navigation app or content mode works.
Prodigy should not hard-code media or phone projection onto secondary video
surfaces based only on the ability to advertise an AUXILIARY display.

For a first useful cluster experience, native HU rendering is lower risk:
consume navigation turn data, media metadata, and phone status, and draw
Prodigy's own widgets. That path does not require a second video decoder and can
ship before projected CLUSTER support.

## Recommended Prodigy architecture

The archived Prodigy design already has the right protocol seams:
`AASession`, multiplexed channel dispatch, `IChannelHandler`,
`IAVChannelHandler`, per-handler `fillChannelDescriptor()`, and a
`ServiceDiscoveryBuilder`. The required change is to make video and input
handlers instances in a display registry rather than singletons.

```text
DisplayRegistry
  by_display_id: map<uint32, DisplaySession>
  by_wire_channel_id: map<int32, DisplaySession*>

DisplaySession
  display_id
  display_type
  video_wire_channel_id
  input_wire_channel_id
  advertised_video_configs
  selected_video_config
  video_handler
  decoder
  decoded_surface_sink
  video_focus_state
  input_router
  lifecycle_state

PhysicalOutputRouter
  DisplaySession sink -> DRM/KMS connector, Qt surface, or compositor region
```

Keep the protocol identity and physical layout in separate layers. For example,
two `DisplaySession` sinks can be placed in two regions of one ultrawide panel,
or sent to two DRM connectors, without changing their Android Auto descriptors.

### Expected integration seams

| Prodigy responsibility | Change needed |
|---|---|
| Configuration | Replace the single display block with a list of logical displays; validate IDs/types before starting AA |
| `ServiceDiscoveryBuilder` | Emit one AV descriptor and one matching input descriptor per logical display |
| Handler construction | Instantiate one video and one input handler per display instead of retaining one global handler/reference |
| `AASession`/Messenger dispatch | Continue routing by transport channel ID; associate each routed handler with one display ID |
| Video decode | Give every display independent codec setup, session/config index, flow-control bookkeeping, decoder, and teardown |
| Focus | Store and transition focus per video handler/display, not globally |
| Input | Transform coordinates in the target display's coordinate space and send them through that display's input channel |
| Rendering | Map decoded logical sinks to physical outputs only after protocol/session routing is resolved |

Those are responsibility-level seams rather than promised current filenames.
The names come from the archived milestone-4 architecture and should be mapped
onto the current implementation before planning changes.

## Suggested delivery order

1. **Native semantic cluster widgets.** Render turn data, media metadata, and
   phone status with existing semantic event channels.
2. **Display registry and discovery model.** Support a list of logical displays,
   validate the topology, and emit paired AV/input descriptors while still
   enabling only MAIN projection.
3. **Projected CLUSTER.** Add a second independent AV handler, decoder, sink,
   focus state, and input route.
4. **Projected AUXILIARY.** Generalize the proven CLUSTER path to zero or more
   auxiliary sessions and add policy/config controls.
5. **Shared physical compositor, if needed.** Place multiple decoded sinks onto
   one framebuffer without changing the protocol model.

This sequence isolates protocol plumbing from hardware composition and provides
useful secondary-screen behavior before concurrent decode is mandatory.

## Acceptance tests for the Prodigy change

### Discovery and topology

- MAIN ID 0 alone succeeds.
- Missing MAIN ID 0 is rejected before session start.
- A non-MAIN display at ID 0 is rejected.
- Duplicate display IDs are rejected.
- Two CLUSTER displays are rejected.
- Every video display has exactly one input config with the same display ID.
- Wire channel IDs are unique and are never substituted for display IDs.

### Session isolation

- MAIN and CLUSTER can negotiate different `VideoConfig` entries.
- Interleaved media frames on two wire channels reach different decoder
  instances and sinks.
- Setup, flow-control ACK, stop, and teardown state from one display cannot
  mutate another display's state.
- A focus request on the CLUSTER channel does not alter MAIN focus state.
- Reopening one channel rebuilds only its `DisplaySession` resources.

### Rendering and input

- Touch coordinates are transformed using the target display's advertised
  dimensions, not the MAIN dimensions.
- Input for display ID N is sent only through N's matched input handler.
- Two physical outputs show their corresponding decoded sinks.
- When one physical framebuffer is shared, compositor placement changes do not
  alter SDP display IDs, AV setup, or input routing.

### Hardware budget

- Measure concurrent decoder count, codec support, maximum resolution/FPS,
  memory bandwidth, and compositor latency on every supported target.
- Advertise only configurations that the target can decode concurrently.
- Fail or downgrade configuration locally instead of advertising a display
  whose stream cannot be serviced.

## Failure patterns to avoid

- Building one decoder and switching it between MAIN and CLUSTER channels.
- Treating `AVChannel` field 6 as the transport channel ID.
- Using one global focus state or one global config/session index.
- Advertising a video display without one matching input configuration, even
  when the physical display is not a touchscreen.
- Scaling all touch coordinates through MAIN's resolution.
- Treating `AdditionalVideoConfig` insets or blended-UI regions as physical
  display routing.
- Assuming a native semantic cluster widget and a projected CLUSTER video
  surface are the same protocol feature.
- Advertising projected CLUSTER/AUXILIARY broadly before runtime captures and
  target decoder limits are known.

## Evidence map

| Finding | 17.3 evidence |
|---|---|
| Descriptor field 6 becomes `CarDisplayId`; field 7 becomes display type | `itq.java:213-238` |
| One display/video service pair per accepted AV descriptor | `itq.java:286-312` |
| Per-display configs, encoder, type, ID, and `Surface` | `itt.java:25-102` |
| Independent surface delivery and enablement | `its.java:26-39`, `its.java:77-86` |
| Endpoint-local focus and type mapping | `jdc.java:32-95`, `jdc.java:338-377` |
| Composition bound to one `CarDisplayId` | `iti.java:19-54`, `iti.java:124-160` |
| Unique IDs, MAIN ID 0, exactly one MAIN, at most one CLUSTER | `jnb.java:223-250`, `jnb.java:513` |
| Exactly one matching input per display | `jnb.java:251-308` |
| Insets/blended UI are built inside one display's parameters | `itt.java:139-150`, `itt.java:255-350` |

The preserved bundle SHA-256 is
`1db7ce995aa52b2cde47a01abfb0364220fb57fc60217de3ec714e3034795344`;
the extracted `base.apk` SHA-256 is
`5557827f259898bdab97b489e1a0aef937fd6ec711d87361cf25d51af6f48619`.
The large JADX tree is intentionally ignored; the 17.3 analysis report in the
sibling protocol-reference repository is the permanent review artifact.

### Canonical protocol files

These paths are in the sibling protocol-reference repository:

- `oaa/control/ChannelDescriptorData.proto`
- `oaa/av/AVChannelData.proto`
- `oaa/video/DisplayTypeEnum.proto`
- `oaa/video/VideoConfigData.proto`
- `oaa/video/AdditionalVideoConfigData.proto`
- `oaa/input/InputChannelConfigData.proto`
- `oaa/input/TouchScreenConfigData.proto`
- `oaa/video/VideoFocusRequestMessage.proto`
- `oaa/video/VideoFocusIndicationMessage.proto`

### Supporting references

These paths are also retained in the sibling research repositories:

- `docs/channels/display-routing.md` — detailed 16.2 content-routing trace
- `docs/channels/video.md` — AV setup, focus, flow control, and video
  configuration
- `docs/channels/input.md` — display-ID input matching and event routing
- `research/archive/openauto-prodigy/docs/plans/milestone-04-protocol-correctness.md`
  — archived Prodigy architecture seams

## Runtime confirmation still requested

Before declaring projected multi-display production-ready, capture a live 17.3
session advertising MAIN + CLUSTER + AUXILIARY and retain:

1. the complete `ServiceDiscoveryResponse`, with wire channel IDs, display IDs,
   display types, video configs, and matched input configs decoded;
2. channel-open and per-channel AV setup/start/stop exchanges;
3. separate media streams or NAL-unit timelines for every display;
4. independent focus requests/indications by video channel;
5. logcat evidence for `CarDisplayId` creation and secondary activity/service
   launches; and
6. Prodigy CPU/GPU/decoder/compositor measurements on target hardware.

Static analysis answers the architecture question: **separate logical display
instances and streams**. The capture is needed to verify operational details and
content policy, not to choose between that model and a panoramic canvas.
