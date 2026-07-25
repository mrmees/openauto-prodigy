# Android Auto Projected CLUSTER Dashboard Widget — Design

Date: 2026-07-24
Status: ACTIVE
Grounded against: `dev` at `3766d4f`

## Goal

Prove that a current Android Auto phone will activate and stream a distinct
projected `CLUSTER` display while Prodigy's existing `MAIN` projection remains
healthy, then render that phone-produced CLUSTER stream inside one fixed square
dashboard widget.

This is an experimental protocol-and-rendering spike. It promotes only the
bounded MAIN-plus-CLUSTER experiment from the long-term projected multi-display
wishlist item. It does not promote a general multi-display product feature.

The design aligns with `docs/project-vision.md`: it uses the existing wireless
AA transport and Qt/QML dashboard stack, preserves protocol definitions, keeps
the feature disabled by default, and requires evidence on the Pi 4 before any
production claim.

## Success Definition

The spike passes when all of the following are observed in one live wireless
AA session:

1. The experimental flag is enabled before connection.
2. Google Maps is actively navigating on the phone.
3. Prodigy's dashboard displays phone-rendered CLUSTER content inside the
   square widget.
4. The existing MAIN projection can be reopened and remains usable.
5. The captured wire lifecycle shows separate MAIN and CLUSTER descriptors,
   channels, setup/start traffic, media frames, acknowledgements, focus, and
   teardown.

A blank or idle CLUSTER surface when no route is active is acceptable. If the
phone does not activate the advertised CLUSTER endpoint, that is a valid
experimental result and must be reported as such rather than replaced with
simulated content.

## Current-State Findings

The repository has most individual pieces but currently owns them as one
projection display:

- `AndroidAutoOrchestrator` retains one `VideoChannelHandler`, one
  `InputChannelHandler`, one `VideoDecoder`, and one global video-focus path.
- `ServiceDiscoveryBuilder` emits one video descriptor on wire channel 3 and
  one input descriptor on wire channel 1. It currently relies on default values
  instead of explicitly identifying the MAIN display in the AV and input
  descriptors.
- `VideoChannelHandler` and `InputChannelHandler` hard-code their existing wire
  channel IDs. Their AV lifecycle state is otherwise already instance-local.
- `VideoDecoder` already has the required per-instance decode worker and one
  attachable `QVideoSink`.
- `AndroidAutoMenu.qml` binds the MAIN decoder to its full projection
  `VideoOutput`; plugin deactivation clears that sink while the AA session
  remains alive.
- The dashboard can register a QML widget with fixed grid dimensions, and QML
  already ships in the application binary.
- Wire channel IDs 12 and 13 are currently unregistered. They are multiplexed
  head-unit-selected identifiers, not protocol schema numerics.

The AA 17.3 static research in
`docs/aa-protocol/wishlist-baselines/projected-multi-display.md` establishes
that every projected display needs a unique logical display ID, a dedicated AV
descriptor and wire channel, exactly one matching input descriptor, and
independent video/focus/lifecycle state. Runtime activation on the current
phone remains open and is the reason for this spike.

## Design

### 1. Use a fixed two-session abstraction

Add an application-owned `ProjectedDisplaySession` that contains exactly one
logical projected display's:

- role (`MAIN` or `CLUSTER`);
- logical display ID;
- video and input wire channel IDs;
- `VideoChannelHandler` and its AV setup/start/ACK state;
- `InputChannelHandler` and its binding state;
- `VideoDecoder` and single attached video sink;
- endpoint-local focus and presentation state; and
- lifecycle diagnostics.

`AndroidAutoOrchestrator` owns two persistent instances:

- MAIN is always enabled and replaces the current standalone video handler,
  input handler, and decoder members without changing their behavior.
- CLUSTER exists as an application object but participates in service
  discovery and session registration only when the experimental flag is true.

This is intentionally not a `DisplayRegistry`, a list of arbitrary displays,
or an AUXILIARY implementation. The abstraction prevents a second set of
one-off orchestrator fields while keeping the experiment's topology fixed and
reviewable.

The two protocol-library handlers gain constructor-injected wire channel IDs,
with defaults that preserve channels 3 and 1 for existing users. The video
handler also accepts a constructor-injected setup focus mode, defaulting to the
current `PROJECTED` behavior. These are behavior-library changes; nothing under
`libs/prodigy-oaa-protocol/proto/` changes.

### 2. Advertise one explicit MAIN and one optional CLUSTER

Service discovery uses this fixed matrix:

| Role | Video wire channel | Input wire channel | Display ID | Display type | Video configuration | Input capability |
|---|---:|---:|---:|---|---|---|
| MAIN | 3 | 1 | 0 | `MAIN` | Existing configured resolution/codecs/FPS/DPI and margins | Existing touch and key capabilities |
| CLUSTER | 12 | 13 | 1 | `CLUSTER` | H.264, 800×480, 30 FPS, 140 DPI, zero margins | Matching input descriptor only; no touch, keys, touchpad, or haptics |

MAIN's AV field 6, AV display type, input display ID, and touch display type are
set explicitly. CLUSTER's AV field 6 is logical display ID 1; it is not wire
channel 12. Its input descriptor carries display ID 1 so the phone can satisfy
its per-display topology rule even though the dashboard surface accepts no AA
input.

The feature flag is
`plugin_config.org.openauto.android-auto.experimental_cluster_display`.
Missing or false means no CLUSTER descriptors, handlers, widget registration,
or runtime behavior. The flag is startup-scoped for this experiment: changing
it requires restarting Prodigy and reconnecting AA because both the widget
catalog and AA topology are established before the session starts. There is no
settings UI in this pass.

### 3. Keep MAIN and CLUSTER lifecycles isolated

At session creation the orchestrator registers each enabled display session's
video and input handlers with `AASession`. Each video wire channel drives only
its own setup state, config count, session ID, ACK permits, decoder queue, sink,
focus state, and teardown.

`ProjectedDisplaySession` connects its handler and decoder once for the
application lifetime rather than reconnecting the same signals on every phone
session. Channel open/close and stream start/stop reset only that display's
state. A fresh stream boundary calls only that display's decoder
`beginStream()`.

MAIN continues to own these existing product effects:

- plugin activation/deactivation;
- evdev grab and touch routing;
- the public `connectionState` transition between `Connected` and
  `Backgrounded`; and
- the `aa.requestFocus` and exit-to-car actions.

CLUSTER focus never changes those global effects. Its unsolicited setup focus
indication is `PROJECTED_NO_INPUT_FOCUS`, matching its lack of advertised input.
If the phone sends a later endpoint-local focus request, the handler preserves
the existing request/indication exchange on the CLUSTER wire channel and logs
the resulting mode without mutating MAIN.

CLUSTER decoding may continue while the dashboard or its widget is not visible,
matching the existing MAIN behavior when its sink is detached. This keeps AV
flow control healthy and avoids relying on a new keyframe when the widget
reappears. Removing or hiding the widget detaches only its video sink; it does
not stop, renegotiate, or refocus either display.

### 4. Render one fixed square dashboard widget

When the experimental flag is enabled, register `AAClusterWidget.qml` as a
normal dashboard widget with a fixed 2×2 footprint:

- minimum, maximum, and default columns: 2;
- minimum, maximum, and default rows: 2; and
- category: navigation.

The widget contains one noninteractive `VideoOutput`. It attaches to the
CLUSTER session's decoder sink while instantiated and detaches only its own
sink when destroyed. The session accepts at most one live sink; a second
placement cannot steal the first widget's stream and instead reports that the
CLUSTER surface is already in use. This local ownership rule avoids expanding
the dashboard model merely for an experimental widget.

`VideoOutput.PreserveAspectFit` displays the decoded 800×480 texture as large
as the square permits. Qt only upsizes or downsizes the existing decoded frame;
Prodigy does not rerender, enhance, crop, stretch, or ask AA for a new
resolution when the widget appears. The unused square area remains normal
widget background.

The widget does not forward tap, touch, key, or rotary input to AA. The normal
dashboard long-press path remains available for widget management. Returning
to MAIN projection continues through the existing AA launcher or focus action,
not through this surface.

### 5. Expose explicit experimental presentation state

The CLUSTER session exposes a narrow QML-facing state object, not raw protocol
objects. Its presentation states are:

- `Disabled` — experimental flag is false;
- `Disconnected` — no active AA session;
- `WaitingForChannel` — AA is connected but the phone has not opened CLUSTER;
- `WaitingForFrames` — channel/stream setup exists but no decoded frame is
  available;
- `Rendering` — at least one valid decoded CLUSTER frame is available; and
- `Error` — the CLUSTER handler or decoder reported a local terminal failure.

The widget renders the video only in `Rendering`. Other states use a subdued
CLUSTER icon plus concise status copy. A stream with no navigation frames may
remain `WaitingForFrames`; that is not automatically an error.

The QML-facing object exposes only state, status text, and controlled sink
attachment. It does not expose `AASession`, channel IDs, protobufs, or a general
video-focus mutation surface.

### 6. Fail locally and log enough to decide feasibility

With the feature disabled, serialized discovery and runtime behavior must match
the current single-display topology except for making MAIN's already-defaulted
identity fields explicit.

After the phone accepts service discovery, a CLUSTER channel, handler, decoder,
or sink failure must not close the transport, tear down MAIN, clear MAIN's
decoder, alter MAIN focus, or change global projection state. The widget moves
to its local error/unavailable presentation and the experiment remains
observable.

AA service discovery is one phone-level transaction. A phone may reject the
entire experimental topology before either display starts; Prodigy cannot
truthfully guarantee MAIN survival in that case. The spike records that as a
topology-rejection result and requires disabling the startup flag to return to
the proven MAIN-only topology. It does not automatically reconnect MAIN-only,
because doing so would obscure the activation evidence this experiment exists
to collect.

Use the existing `lcAA` category for app-side logs and retain the protocol
capture. Diagnostics must distinguish display role, logical display ID, and
wire channel ID at:

- descriptor construction;
- channel open/close;
- AV setup response and selected codec/config;
- stream start/stop;
- initial and subsequent focus exchanges;
- first media frame and first decoded frame, including decoded dimensions; and
- sink attach/detach, decoder failure, and session teardown.

Do not log every video frame. First-frame and state-transition diagnostics are
sufficient; protocol capture retains the wire sequence.

## Failure Semantics

- Flag absent or false: MAIN-only discovery; no CLUSTER widget or handlers are
  registered with the phone session.
- Phone ignores CLUSTER but accepts MAIN: MAIN remains usable; widget stays in
  `WaitingForChannel`.
- Phone opens CLUSTER but sends no frames: MAIN remains usable; widget stays in
  `WaitingForFrames`.
- CLUSTER decode or sink failure: only CLUSTER enters `Error`; MAIN continues.
- CLUSTER channel closes: clear only CLUSTER stream state and decoded-frame
  availability; accept a later reopen without rebuilding MAIN.
- MAIN channel/focus behavior: unchanged and still authoritative for global
  projection state.
- Whole-topology rejection: record the evidence, stop claiming live
  feasibility, and restore the proven topology by disabling the flag before
  the next connection.
- Session disconnect or application shutdown: detach sinks, close/reset both
  enabled display sessions, and prevent stale frames or focus from crossing a
  reconnect.

## Out of Scope

- Any edit under `libs/prodigy-oaa-protocol/proto/` or `proto/api/`.
- AUXILIARY displays, arbitrary display lists, a general display registry, or
  multiple simultaneously rendered CLUSTER widgets.
- Touch, key, rotary, mouse, or gesture input on CLUSTER.
- Configurable CLUSTER codecs, resolution, FPS, DPI, margins, widget aspect,
  widget dimensions, or runtime renegotiation.
- Frame rerendering, enhancement, interpolation, super-resolution, cropping,
  or stretching.
- A polished settings UI, automatic feature fallback, phone compatibility
  matrix, or production enablement.
- Physical second-monitor/DRM routing, native semantic cluster widgets,
  blended MAIN projection, or generalized multi-display hardware policy.
- Changes to AA audio, Assistant microphone, HFP, Bluetooth, WiFi transport,
  local media, navigation semantics, External API, or Companion.

## Verification and Live Acceptance Matrix

| Check | Level | Acceptance |
|---|---|---|
| Disabled discovery | Required local | Flag absent/false emits no channels 12/13 and preserves the current MAIN video/input capabilities with explicit display identity |
| Enabled topology | Required local | Exactly one MAIN ID 0 and one CLUSTER ID 1 exist; video/input IDs match; all four wire IDs are unique; CLUSTER is H.264 800×480 at 30 FPS and has no input capabilities |
| Handler configurability | Required local | Default handlers retain channels 3/1; injected handlers send responses, focus, media ACKs, and binding replies only on channels 12/13 |
| Display isolation | Required local | Interleaved MAIN/CLUSTER start, frame, ACK, focus, close, and reopen events mutate only the target session and decoder |
| Widget ownership | Required local | The enabled widget is fixed at 2×2, attaches one CLUSTER sink, rejects a competing sink, preserves aspect ratio, and detaches safely |
| Presentation state | Required local | Disabled, disconnected, waiting-channel, waiting-frame, rendering, duplicate-sink, error, teardown, and reconnect transitions are deterministic |
| Repository gates | Required local | Focused tests, local build, explicit `openauto-prodigy` target, full CTest, documentation links apart from explicitly preserved unrelated artifacts, and diff checks pass |
| Review gates | Required before execution/push | Opus reviews this design and its plan; implementation completes the repository Codex review gate and every finding is adjudicated |
| aarch64 application | Required before deploy | `./cross-build.sh` succeeds from the reviewed implementation |
| Live CLUSTER activation | Required live | With the flag enabled and Google Maps actively navigating, the square dashboard widget shows phone-rendered CLUSTER content |
| MAIN coexistence | Required live | MAIN projection remains healthy, can be reopened normally, and retains existing touch/focus behavior |
| Wire evidence | Required live | Capture records separate descriptor IDs, wire channels, setup/start, frames/ACKs, focus, and teardown, or clearly records phone rejection/nonactivation |
| Pi budget | Required live | Record application CPU and memory plus active decoder/codec diagnostics during simultaneous MAIN and CLUSTER; no production capacity claim is inferred from one run |
| Pi health | Required live | One responsive Prodigy process remains and wireless AA can disconnect/reconnect without restarting hostapd or Bluetooth |

The user's standing Pi authorization covers the scoped binary deployment,
application restart, temporary experimental flag change, AA reconnect, and
restoration of the original configuration. The work does not restart Bluetooth
or hostapd and preserves unrelated Pi and repository state.

## Executor Guidance

- Read root `AGENTS.md`, `src/AGENTS.md`, `src/core/aa/AGENTS.md`,
  `qml/AGENTS.md`, and `libs/prodigy-oaa-protocol/AGENTS.md` before editing.
- Keep `ProjectedDisplaySession` focused on one projected video/input endpoint;
  do not turn this spike into a registry or physical-output abstraction.
- Preserve handler defaults so the protocol library remains source-compatible
  for its current MAIN-only call sites.
- Keep every QObject, handler, session, decoder, and sink transition on its
  established owner thread; the decoder worker may emit only through the
  existing queued frame boundary.
- Use TDD for descriptor topology, configurable handler IDs/focus, session
  isolation, and widget state/ownership.
- Treat the live phone capture as the feasibility verdict. Static schema
  support or a compiling second handler is not proof that Android Auto exposes
  CLUSTER content.
