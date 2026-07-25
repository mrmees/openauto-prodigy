# Android Auto CLUSTER Square Viewport Follow-up

Status: ACTIVE

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

Therefore this change must not add a fake enum, alter the community proto
submodule, or claim that 300x300 is the encoded transport resolution.

### Qt cannot assign `VideoOutput.sourceRect`

Qt 6.8 exposes `VideoOutput.sourceRect` and `contentRect` as read-only
properties. The crop must be expressed with ordinary QML geometry: a clipped
square `Item` contains a larger, centered `VideoOutput` whose size maps the
800x480 carrier at one uniform scale. No shader, frame copy, or writable source
rectangle is required.

### Existing bench placement

The Pi currently has `org.openauto.aa-cluster-16` placed at 2x2 on dashboard
`home`, page 1, column 0, row 0. A 3x3 placement at that origin is currently
within the 8x4 grid and does not overlap another page-1 placement. Because this
feature has not been published, no general migration belongs in product code.
The deploy procedure will update that exact bench placement only after
rechecking those preconditions with Prodigy stopped.

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

The value is immutable for this follow-up. Compile-time checks require a
positive content rectangle fully contained in the carrier and require the
derived margins to equal carrier size minus content size.

`ServiceDiscoveryBuilder` consumes this value when building the CLUSTER
descriptor. `ProjectedDisplaySession` exposes the same carrier and content
geometry to QML through constant integer properties. QML must not duplicate
the numeric protocol geometry.

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

Only the margins change: `margin_width=500` and `margin_height=180`. Android
Auto consequently composes CLUSTER content at `(250, 90)` with size 300x300
inside the 800x480 encoded frame. The encoded frame, decoder allocation, ACK
cadence, and transport channel remain unchanged. Large uniform borders should
compress cheaply, but this design makes no bandwidth or CPU reduction claim.

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

Because the item retains the carrier's 800:480 aspect ratio, Qt maps the full
decoded frame uniformly. The clipped parent exposes exactly the centered
300x300 content rectangle. This is an ordinary texture scale and clip; it does
not create another decoder, CPU frame copy, shader effect, super-resolution
pass, or phone-side rerender.

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
  rolled back to the proven zero-margin configuration.
- Phone opens CLUSTER but content is not centered or is clipped incorrectly:
  preserve the capture and screenshot, restore zero margins, and do not hide
  the mismatch with arbitrary QML offsets.
- Decoded frame dimensions differ from the advertised 800x480 carrier: report
  a geometry mismatch and keep MAIN healthy; do not infer a new crop.
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
  internally valid and resolves independently of the feature flag.
- `test_projected_display_session`: CLUSTER exposes the shared geometry to QML
  while lifecycle, decoder generation, and sink ownership behavior stay
  unchanged.
- `test_aa_cluster_widget`: descriptor is fixed 3x3, does not fit 2x2 space,
  and appears in 3x3 space; runtime QML geometry maps the carrier so that the
  configured 300x300 source region exactly fills the centered square clip.
- Existing focused AA display tests, full local build, explicit application
  target, and full CTest remain green.

### Pi live checks

1. Cross-build and deploy the reviewed application.
2. Stop Prodigy before any external config edit so its shutdown flush cannot
   overwrite the change.
3. Recheck that the exact bench placement remains 2x2 at page 1, column 0,
   row 0 and that 3x3 is collision-free; back up the config and change only
   that placement's spans to 3x3.
4. Start Prodigy and confirm the experimental flag and 3x3 placement persist.
5. Reconnect the Pixel, verify CLUSTER channels 12/13 open and frames arrive,
   then exit MAIN to the dashboard.
6. Confirm the widget shows only the square phone-rendered CLUSTER viewport,
   with no encoded-frame borders, stretching, or overlap; reopen MAIN and
   confirm touch/focus remain healthy.
7. Record one process, service restart count, and CLUSTER stream diagnostics.
   Retain the config backup until Matthew accepts the presentation.

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
square presentation. Completion archives this design and its plan together and
records the exact live outcome in `docs/session-handoffs.md`.
