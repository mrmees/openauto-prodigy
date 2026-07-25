# Android Auto CLUSTER Square Viewport Follow-up

Status: COMPLETED 2026-07-25

Date: 2026-07-25

Grounded on: `5c124ba` (`docs: complete projected cluster widget experiment`)

## Context

The completed projected-CLUSTER spike proved that a Pixel 8 running Android
Auto 17.3 accepts Prodigy's independent CLUSTER display and continuously sends
an H.264 800x480 stream. The spike deliberately used the protocol's smallest
legacy encoded-resolution preset and rendered the entire landscape frame in a
fixed 2x2 dashboard widget.

That result proved transport feasibility, but it did not match the intended
product presentation. The desired surface is approximately 300x300 and is
meant to be upsized into a larger square dashboard region. The earlier phrase
"square widget" described only the grid footprint; it did not make the
phone-rendered viewport square.

This follow-up corrects that interpretation without inventing a nonstandard
Android Auto resolution or rerendering phone content.

## Goal

Have Android Auto render CLUSTER content into a centered 300x300 viewport,
extract that viewport locally from the required 800x480 encoded carrier, and
display it in one fixed 3x3 dashboard widget.

Success means:

1. Prodigy still advertises the standards-backed `VIDEO_800x480` CLUSTER mode.
2. The CLUSTER `VideoConfig` declares total margins of 500 horizontal pixels
   and 180 vertical pixels, leaving a centered 300x300 content rectangle.
3. The widget displays only that 300x300 phone-rendered rectangle and upsizes
   it uniformly inside the largest centered square that fits the 3x3 tile.
4. Prodigy does not decode a second copy, rewrite frames, enhance, interpolate,
   stretch, or ask Android Auto to encode an unknown resolution.
5. MAIN projection, CLUSTER lifecycle isolation, single-sink ownership, and
   the disabled feature path retain their existing behavior.

## Evidence and Constraints

### Android Auto's encoded carrier remains 800x480

The protocol's `VideoResolution` enum exposes named fixed modes. Its smallest
landscape value is `VIDEO_800x480`; there is no 300x300 enum value. Static
inspection of Android Auto 17.3 (`173662804`) confirms that `itl.d(xmy)` maps
those enum values to fixed encoder sizes and that `itq.a(...)` rejects invalid
display dimensions before constructing a display endpoint.

The same 17.3 path computes the usable content rectangle inside that fixed
carrier. `itl.c(xmz, xml)` splits the legacy `margin_width` and
`margin_height` totals across opposing edges. Prodigy's existing live margin
capture independently confirmed this behavior: a total 70-pixel vertical
margin produced a centered 800x410 phone-rendered region with 35-pixel bars.

The 17.3 validator in `itq.a(...)` resolves the enum to the encoded size,
subtracts the declared margins through `itl.b(...)`, and rejects the display
resolution only when the resulting width or height is less than or equal to
zero. A 300x300 rectangle is therefore statically valid, but the repository's
live evidence is narrower: it covers MAIN, one-axis 70-pixel margins, and a
different phone. Pixel 8 CLUSTER behavior with the larger two-axis margins
remains an explicit live acceptance item rather than a proven consequence.

Therefore this change must not add a fake enum, alter the community proto
submodule, or claim that 300x300 is the encoded transport resolution.

### Qt cannot assign `VideoOutput.sourceRect`

Qt 6.8 exposes `VideoOutput.sourceRect` and `contentRect` as read-only
properties. The crop must be expressed with ordinary QML geometry: a clipped
square `Item` contains a larger, centered `VideoOutput` whose size maps the
800x480 carrier at one uniform scale. No shader, frame copy, or writable source
rectangle is required.

### Existing bench placement

A fresh 2026-07-25 read of `~/.openauto/config.yaml` shows
`org.openauto.aa-cluster-16` placed at 2x2 on dashboard `home`, page 1, column
0, row 0. This is newer than the completed-spike handoff, which correctly said
the earlier temporary placement had been removed at that time. A 3x3 placement
at the current origin is within the 8x4 grid and does not overlap another
page-1 placement. Because this feature has not been published, no general
migration belongs in product code. The deploy procedure will re-read the live
file and either update that exact bench placement after validating the expected
frozen YAML keys (`instance_id`, `widget_id`, `col_span`, and `row_span`) or,
if it is absent, let the picker create a fresh 3x3 placement through the normal
`next_instance_id` path.

## Design

### 1. One shared fixed viewport contract

Add an application-owned CLUSTER viewport value beside
`ProjectedClusterConfig` in `ProjectedDisplayConfig.hpp`:

| Value | Pixels |
|---|---:|
| Encoded width | 800 |
| Encoded height | 480 |
| Content X | 250 |
| Content Y | 90 |
| Content width | 300 |
| Content height | 300 |
| Total horizontal margin | 500 |
| Total vertical margin | 180 |

The value is immutable for this follow-up. It stores carrier and square-content
dimensions; margins and offsets are derived rather than independently entered:

```text
marginWidth  = encodedWidth - contentWidth
marginHeight = encodedHeight - contentHeight
contentX     = marginWidth / 2
contentY     = marginHeight / 2
```

Compile-time checks require positive dimensions, square content, content no
larger than the carrier, and even total margins. Those constraints make the
center offsets exact and prevent a one-pixel phone/QML split disagreement.

`ServiceDiscoveryBuilder` consumes this value when building the CLUSTER
descriptor. `ProjectedDisplaySession` exposes the same carrier and derived
content geometry to QML through constant integer properties only for its
`Cluster` role; a `Main` session reports zero for those cluster-only
properties. QML must not duplicate the numeric protocol geometry.

The shared value is Prodigy application policy, not a protocol definition. No
file under `libs/prodigy-oaa-protocol/proto/` or `proto/api/` changes.

### 2. Request a 300x300 phone-rendered viewport

The CLUSTER descriptor remains:

- display ID 1;
- video channel 12 and matching no-input channel 13;
- display type `CLUSTER`;
- `VIDEO_800x480`;
- H.264 baseline profile;
- 30 FPS;
- 140 DPI; and
- exactly one video configuration.

Only the margins change: `margin_width=500` and `margin_height=180`. The static
17.3 path predicts a CLUSTER content rectangle at `(250, 90)` with size 300x300
inside the 800x480 encoded frame; the Pixel live check must prove that expected
behavior. The encoded frame, decoder allocation, ACK cadence, and transport
channel remain unchanged. Large uniform borders should compress cheaply, but
this design makes no bandwidth or CPU reduction claim.

The matching CLUSTER input descriptor remains capability-empty. There are no
touch coordinates to resize or route.

### 3. Crop geometrically and upsize without rerendering

`AAClusterWidget.qml` keeps the existing current-page and single-sink rules.
Its rendering subtree becomes:

```text
widget root (3x3 grid footprint)
  centered square crop viewport, clip=true
    VideoOutput sized and offset from shared carrier/content geometry
```

Let `S = min(root.width, root.height)`. The crop viewport is `S x S`. The
uniform scale is `S / contentWidth`. The `VideoOutput` geometry is:

```text
width  = encodedWidth  * scale
height = encodedHeight * scale
x      = -contentX * scale
y      = -contentY * scale
```

Because the item retains the carrier's 800:480 aspect ratio and keeps
`VideoOutput.PreserveAspectFit`, Qt maps the full decoded frame uniformly. The
clipped parent exposes exactly the centered 300x300 content rectangle. This is
an ordinary texture scale and clip; it does not create another decoder, CPU
frame copy, shader effect, super-resolution pass, or phone-side rerender. The
QML retains exactly one `VideoOutput`; `sourceRect`, transforms, and
`ShaderEffect` remain forbidden. Rendering visibility moves to the square crop
container while sink acquisition still uses that one output's `videoSink`.

The loading/error/duplicate-sink status presentation remains centered and
visible whenever CLUSTER is not rendering.

### 4. Make the picker and placement contract fixed 3x3

The widget descriptor changes all six size values (`min`, `max`, and `default`
rows and columns) from 2 to 3. It remains in the `navigation` category and
retains the same widget ID and QML component.

The picker will list it only when a 3x3 region is available. New placements are
always 3x3 and cannot be resized. Product code will not silently rewrite old
placements because the feature has not shipped. The current bench's known
2x2 instance will be changed to 3x3 as a guarded deployment operation.

## Failure Semantics

- Feature flag false: no CLUSTER descriptor, handler, widget registration, or
  behavior; the established MAIN-only bytes stay unchanged.
- Phone rejects the new margins or does not open CLUSTER: MAIN remains usable;
  the widget reports the existing rejected/waiting state and the experiment is
  rolled back with the preserved zero-margin binary and config.
- Phone opens CLUSTER but content is not centered or is clipped incorrectly:
  preserve the capture and screenshot, restore the known-good binary and
  config, and do not hide the mismatch with arbitrary QML offsets.
- A CLUSTER decoded frame differs from the advertised 800x480 carrier: the
  `frameReadyForGeneration` delivery boundary logs one warning for that decoder
  generation, enters the CLUSTER session's existing terminal `Error` state
  with explicit geometry-mismatch status, and does not deliver the mismatched
  frame to its sink. MAIN has its own session and remains healthy. MAIN does not
  apply the cluster-only geometry check.
- Widget has no 3x3 space: the picker omits it through the existing fitting
  contract; no dashboard page or unrelated placement is moved automatically.
- A second visible widget copy requests the sink: the existing single-owner
  rejection remains authoritative.

## Testing

### Local automated checks

- `test_service_discovery_builder`: enabled CLUSTER remains `VIDEO_800x480`
  and now serializes margins 500/180; disabled MAIN-only golden bytes remain
  unchanged.
- `test_projected_display_config`: the shared carrier/content contract is
  internally valid, derived margins/offsets are centered and even, and geometry
  resolves independently of the feature flag.
- `test_projected_display_session`: CLUSTER exposes the shared geometry to QML,
  MAIN reports no CLUSTER geometry, a matching 800x480 frame reaches the sink,
  and a mismatched frame enters CLUSTER-only `Error` without sink delivery;
  lifecycle, decoder generation, and sink ownership behavior otherwise stay
  unchanged.
- `test_aa_cluster_widget`: descriptor is fixed 3x3, does not fit 2x2 space,
  and appears in 3x3 space. Its fake display obtains geometry properties from
  the same C++ viewport value, and runtime checks at two distinct square sizes
  prove that the configured source region exactly fills the centered clip.
- `test_service_discovery_builder` and `test_aa_cluster_widget` assert expected
  margins and geometry through the shared viewport value rather than repeating
  independent 500/180/250/90 literals.
- Existing focused AA display tests, full local build, explicit application
  target, and full CTest remain green.

### Pi live checks

1. Before deployment, record SHA-256 values and preserve recoverable copies of
   the currently deployed known-good aarch64 binary and
   `~/.openauto/config.yaml` on the Pi.
2. Cross-build and stage the reviewed application, then stop Prodigy before
   replacing the binary or editing config so its shutdown flush cannot
   overwrite the external change.
3. Re-read `~/.openauto/config.yaml`. If the exact bench placement remains 2x2
   at page 1, column 0, row 0 and 3x3 is collision-free, change only its
   `col_span` and `row_span` to 3. If absent, start the new binary and add the
   widget normally through the picker; do not synthesize an instance ID or
   mutate `next_instance_id` externally. Any other placement state stops the
   deploy for re-triage.
4. Start Prodigy and confirm the experimental flag and 3x3 placement persist.
5. Reconnect the Pixel, verify CLUSTER channels 12/13 open and frames arrive,
   confirm the first decoded frame is exactly 800x480, then exit MAIN to the
   dashboard.
6. Confirm the widget shows only the square phone-rendered CLUSTER viewport,
   with no encoded-frame borders, stretching, or overlap; reopen MAIN and
   confirm touch/focus remain healthy.
7. Record one process, service restart count, and CLUSTER stream diagnostics.
   Retain both backups until Matthew accepts the presentation. On any
   activation, geometry, or layout failure, stop Prodigy, restore both files,
   restart, verify their SHA-256 values, and confirm the zero-margin known-good
   CLUSTER plus MAIN behavior without waiting for a rebuild.

## Out of Scope

- A nonstandard 300x300 `VideoResolution` enum or any proto edit.
- Reducing the encoded carrier, decoder allocation, or negotiated frame rate.
- Configurable CLUSTER geometry, codecs, DPI, margins, or widget sizes.
- Automatic production migration of arbitrary historical placements.
- Shader effects, CPU frame cropping, frame enhancement, interpolation, or
  super-resolution.
- Touch/key/rotary input, multiple CLUSTER widgets, AUXILIARY displays, physical
  second-monitor routing, or generalized multi-display configuration.
- Promoting the experimental flag to a public settings toggle or default-on
  product behavior.

## Completion Criteria

The follow-up is complete only when the spec and implementation plan receive
Opus review, the repository review gate is adjudicated, all required builds and
tests pass, the aarch64 binary is deployed, and Matthew confirms the live 3x3
square presentation. The behavior change updates
`docs/aa-protocol/aa-display-rendering.md`; the generic widget developer guide
does not change because its existing min/max/default sizing contract already
covers a fixed-size descriptor. Completion archives this design and its plan
together and records the exact live outcome in `docs/session-handoffs.md`.

## Opus Design Review Adjudication

Opus reviewed commit `f05c651` and returned six major and three minor findings.
All were adjudicated:

- Findings 1, 2, 3, 5, 6, 7, and 8 were confirmed and incorporated: the spec
  now separates static prediction from live proof, defines mismatch ownership
  and tests, derives centered geometry under compile-time invariants, preserves
  a known-good binary, wires tests to the shared value, makes session properties
  role-safe, and preserves the existing QML rendering constraints.
- Finding 4's claimed contradiction was dismissed because the historical
  handoff predates Matthew's fresh 2026-07-25 placement; the robustness portion
  was still adopted by defining both present and absent placement branches.
- Finding 9 was confirmed for the AA rendering guide and dismissed for the
  generic widget guide, whose sizing contract is unchanged.

The final Opus pass reviewed commit `a29ea48` and returned `PASS`, with no
blocker or major findings. It identified two minor plan-time wiring
requirements that are binding on the implementation plan:

- Update `VideoDecoderTestAccess::publishFrame` so ordinary CLUSTER session
  tests can publish the expected carrier size; reserve a mismatched size for
  the dedicated rejection test.
- When completion archives the design and plan, update `docs/INDEX.md` and
  `docs/roadmap-current.md` alongside the AA rendering guide and session
  handoff so no active-document links become stale.
