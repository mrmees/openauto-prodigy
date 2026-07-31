# Native Dashboard Turn Card

**Status:** ACTIVE

**Approved:** 2026-07-28

**Grounded on:** `15a45c1`

**Scope:** staged replacement of the phone-rendered dashboard turn card with a
Prodigy-native, theme-aware guidance card while preserving the projected map
mode

## 1. Outcome

When Dashboard Navigation is set to **Map**, Prodigy continues showing the
phone-rendered Android Auto auxiliary map. When it is set to **Turn card**,
Prodigy immediately shows a native QML guidance card driven by Android Auto's
semantic navigation channel. The phone's auxiliary provider remains the
already accepted navigation-map provider in both modes, so changing the local
presentation does not reconnect Android Auto.

The native card uses the approved instrument-panel visual hierarchy inside a
large rounded Prodigy card. It covers every currently defined Android Auto
maneuver code intentionally and renders lane guidance as one continuous
roadway band: free-floating arrows, muted alternatives, highlighted recommended
movements, and only subtle lane-boundary ticks. Lanes must never look like
buttons or individually boxed controls.

Delivery is staged:

1. **Stage 1 — native maneuver card and lane guidance:** local Map/Turn card
   switching, exhaustive maneuver presentation, semantic lane transport,
   exhaustive lane-shape presentation, theme integration, safe empty states,
   and Pi/phone validation.
2. **Stage 2 — richer trip semantics:** explicit rerouting presentation,
   current-road versus instruction separation, roundabout exit detail,
   destination distance and ETA, time to the next step, and multi-step
   lookahead where live delivery supports them.

Stage 2 may add information to reserved or secondary regions. It may not make
the Stage 1 maneuver, distance, road, or lane guidance less readable.

## 2. Why Prodigy must render the card

The compact grey turn card currently seen on the dashboard is a phone-rendered
video surface. The head unit may request automatic, light, or dark video themes
and may declare layout metadata, but it cannot choose the projected card's
colors, typography, or internal composition. Android Auto's Material You token
flow is phone-to-HU so native HU elements can match the phone; it does not let
the HU restyle Google Maps pixels.

The Android Auto navigation channel already sends the semantic information
needed for a native card. Prodigy currently retains the active state, maneuver,
turn side, formatted distance, and one road/instruction string. Modern
`NavigationNotification` messages also contain per-step lane directions and
recommendation flags, but `NavigationChannelHandler` currently logs those
lanes instead of publishing them to the provider.

Official Android navigation references inform the state model and information
hierarchy, but their AndroidX and Navigation SDK classes run inside phone apps;
Prodigy does not instantiate `NavigationTemplate`:

- <https://developers.google.com/maps/documentation/navigation/android-sdk/android-auto>
- <https://developers.google.com/maps/documentation/navigation/android-sdk/tbt-feed#states>
- <https://developer.android.com/reference/androidx/car/app/navigation/model/NavigationTemplate>

## 3. Goals

- Preserve the hardware-accepted projected map path.
- Make Map/Turn card selection immediate and local, without an AA reconnect.
- Render the selected Turn card without depending on projected turn-card
  frames.
- Follow the active Prodigy theme through `ThemeService`.
- Keep primary navigation information legible at arm's length on the 1024x600
  Pi display.
- Provide intentional presentation for every defined `ManeuverType` value.
- Carry and render every defined `LaneShape` value.
- Preserve multiple possible directions within one physical lane and every
  phone-marked recommended direction.
- Clear semantic data on navigation/session loss so stale guidance is not
  presented after the provider reports navigation inactive.
- Keep protocol parsing in the reusable protocol library, state ownership in
  providers, and presentation in QML.

## 4. Non-goals

- Editing `libs/prodigy-oaa-protocol/proto/`; the OAA v1.5 proto submodule is
  hands-off and already defines the required fields.
- Drawing or restyling Google Maps' projected pixels.
- Adding a second projected display, decoder, or physical-screen abstraction.
- Suspending the hidden map decoder in Stage 1. The existing secondary decoder
  already runs when its dashboard page has no sink; optimization requires a
  separate lifecycle design.
- Adding touch controls to the native card or lane strip.
- Adding lane data to External API v1 in Stage 1. Any later public exposure is
  additive-only and needs an explicit capability field.
- Renaming the existing internal CLUSTER/AUXILIARY types during this feature.
- Synthesizing missing ETA, road, lane, or route data.

## 5. Runtime architecture

### 5.1 Phone-side provider remains Map

An enabled secondary display continues to be advertised as `AUXILIARY`
display 1 on video/input channels 12/13. Service discovery always serializes
`KEYCODE_NAVIGATION` for that display. The current
`KEYCODE_TURN_CARD` descriptor path is no longer selected by
`video.secondary_display_content`.

The durable configuration key and accepted values remain:

- `video.secondary_display_content: map`
- `video.secondary_display_content: turn_card`

Their meaning becomes local dashboard presentation rather than phone-provider
selection. Existing user configuration therefore migrates without a new key.
Changing this key is removed from Android Auto's session-renegotiation list.

### 5.2 Widget owns the local switch

`AAClusterWidget.qml` observes `ConfigService` and selects one of two local
surfaces:

- **Map:** retain the existing current-page-only `QVideoSink` claim and the
  centered auxiliary-map crop.
- **Turn card:** release or avoid the video-sink claim and show the native
  semantic component.

Switching either direction is immediate. Returning to Map reuses the live map
stream and the existing bounded sink-claim retry behavior. The selected native
card does not send projected input or a content-selection keycode.

### 5.3 Provider boundary

The native component consumes provider state only. It does not bind to
protobuf messages, `NavigationChannelHandler`, EventBus topics, or transport
objects.

Stage 1 adds a lane-guidance model to the navigation provider boundary:

- one model row per physical lane in the current step;
- each row contains the ordered directions for that lane;
- each direction exposes a stable Prodigy shape token and `recommended` flag;
- provider updates replace one complete lane snapshot atomically;
- navigation inactive/session loss clears the model.

The protocol library emits small source-level value types containing the raw
lane shape and recommendation flag. It does not expose generated protobuf
objects to Prodigy. `NavigationDataBridge` normalizes raw OAA values into the
stable provider tokens consumed by QML. The in-tree static library has no
separate ABI, so signal and call-site changes ship together.

Only the first/current `NavigationNotification.steps[0]` supplies Stage 1 lane
guidance. Lanes from lookahead steps must not be flattened into the current
step. Multi-step presentation belongs to Stage 2.

## 6. Stage 1 visual contract

### 6.1 Card composition

The approved 1024x600 composition is:

- a large rounded card inset from the widget boundary;
- a small status header;
- a dominant maneuver area on the left;
- distance, unit, road/instruction, and optional short cue on the right;
- a full-width lane band at the bottom only when lane data exists.

The maneuver tile may use depth, an outline, and the active theme's primary
color. The lane band uses a single continuous surface. It has no rounded lane
cells, per-lane outlines, shadows that resemble buttons, or pointer handlers.
Subtle vertical ticks may mark lane boundaries.

Recommended lane directions use a high-contrast theme-primary treatment.
Non-recommended directions remain visible using a muted on-surface variant.
Recommendation applies to each direction, not merely the lane container.

### 6.2 Typography floor

At the native 1024x600 target, the design targets are:

| Element | Target size |
|---|---:|
| Distance | 92-100 px |
| Distance unit | 38 px |
| Road/instruction | 40 px |
| Secondary cue | 28 px |
| Status labels | 22 px |

Responsive sizing may scale with the actual widget, but ordinary dashboard
body-text sizes are not an acceptable fallback for this in-dash surface. Lane
guidance receives a fixed-height region and must not shrink the primary type
below its readability floor. Long road/instruction text elides instead of
shrinking into unreadability.

### 6.3 Theme contract

The implementation uses `ThemeService` surface, primary, on-surface,
on-surface-variant, and outline tokens. The cyan design mockup represents the
active theme's primary color; cyan is not hardcoded. The card must remain
legible in both light and dark Prodigy themes.

### 6.4 Empty and partial states

| State | Presentation |
|---|---|
| AA disconnected | Friendly `Connect Android Auto` card |
| AA connected, navigation inactive | `Start a route in Android Auto` |
| Navigation active with maneuver and distance | Full maneuver card |
| Navigation active without lanes | Full card with lane band omitted |
| Lanes arrive or clear | Replace the complete lane snapshot; never mix old and new lanes |
| Known maneuver with missing text | Show maneuver and distance without invented road text |
| Unknown/future maneuver | Generic navigation glyph; retain valid distance/text |

Stage 1 preserves the provider's existing active/inactive model. Explicit
`REROUTING` versus `ACTIVE` presentation is a Stage 2 addition; the Stage 2
implementation must clear the obsolete maneuver while rerouting rather than
present stale guidance.

## 7. Exhaustive maneuver presentation

Prodigy owns one clean-room 24x24 navigation geometry system. Hero maneuvers
whose visual semantics match lane directions instantiate the exact same
straight, slight, normal, sharp, or U-turn component used by the lane band.
Genuinely distinct keep/fork, off-ramp, merge, roundabout, ferry, train,
destination, depart, and fallback semantics use original Canvas geometry with
the same normalized viewport, stroke width, caps, joins, color, and mirroring
contract. Extracted Google Maps assets are reference-only and are not copied,
traced, imported, or shipped.

A dedicated presentation helper maps every defined raw maneuver intentionally.
Values may share a visual when the audited reference family does: name-change
uses straight, and on-ramp slight/normal/sharp/U-turn values use their matching
lane primitives. Only undefined or future values take the fallback.

| Raw values | Android Auto maneuver | Required presentation |
|---|---|---|
| 0 | `UNKNOWN` | Generic navigation fallback |
| 1 | `DEPART` | Depart/heading glyph |
| 2 | `NAME_CHANGE` | Straight/continue glyph |
| 3, 4 | `KEEP_LEFT`, `KEEP_RIGHT` | Side-specific keep glyph |
| 5, 6 | Slight left/right | Side-specific slight-turn glyph |
| 7, 8 | Normal left/right | Side-specific normal-turn glyph |
| 9, 10 | Sharp left/right | Side-specific sharp-turn glyph |
| 11, 12 | U-turn left/right | Side-specific U-turn glyph |
| 13-18 | On-ramp slight/normal/sharp left/right | Matching shared lane-direction primitive |
| 19, 20 | On-ramp U-turn left/right | Matching shared U-turn primitive |
| 21-24 | Off-ramp slight/normal left/right | Side-specific off-ramp presentation |
| 25, 26 | Fork left/right | Side-specific fork glyph |
| 27, 28 | Merge left/right | Side-specific merge presentation |
| 29 | Merge side unspecified | Neutral merge glyph |
| 32, 33 | Roundabout enter-and-exit clockwise | Clockwise roundabout glyph |
| 34, 35 | Roundabout enter-and-exit counterclockwise | Counterclockwise roundabout glyph |
| 36 | Straight | Straight glyph |
| 37 | Ferry boat | Boat glyph |
| 38 | Ferry train | Train glyph |
| 39, 40 | Destination / destination straight | Shared destination presentation |
| 41, 42 | Destination left/right | Mirrored side-specific destination presentation |
| 43, 45 | Roundabout enter clockwise/counterclockwise | Direction-specific roundabout entry |
| 44, 46 | Roundabout exit clockwise/counterclockwise | Direction-specific roundabout exit |
| 47, 48 | Ferry boat left/right | Boat with side-specific direction cue |
| 49, 50 | Ferry train left/right | Train with side-specific direction cue |

Raw values 30 and 31 are not defined in OAA v1.5 and therefore exercise the
unknown/future fallback. Any out-of-range future value does the same. Tests
enumerate every currently defined value and separately prove the undefined and
out-of-range fallbacks.

## 8. Exhaustive lane presentation

Every `LaneShape` value has an intentional presentation:

| Raw value | Lane shape | Required presentation |
|---:|---|---|
| 0 | `UNKNOWN` | Neutral lane stem without a misleading arrowhead |
| 1 | `STRAIGHT` | Straight arrow |
| 2 | `SLIGHT_LEFT` | Slight-left arrow |
| 3 | `SLIGHT_RIGHT` | Slight-right arrow |
| 4 | `NORMAL_LEFT` | Normal-left arrow |
| 5 | `NORMAL_RIGHT` | Normal-right arrow |
| 6 | `SHARP_LEFT` | Sharp-left arrow |
| 7 | `SHARP_RIGHT` | Sharp-right arrow |
| 8 | `U_TURN_LEFT` | Left U-turn arrow |
| 9 | `U_TURN_RIGHT` | Right U-turn arrow |

One lane may contain multiple direction records. The lane band shows all of
them within that lane's horizontal allocation and highlights each record whose
`is_recommended` flag is true. Multiple recommended lanes and multiple
recommended directions in one lane are valid. The order received from the
phone is preserved.

The band lays out all current-step lanes in one noninteractive row. It scales
lane glyphs within the fixed band instead of shrinking the maneuver text. A
pathological future shape or count may use the neutral fallback, but it must
not crash QML, overrun the card, or convert lanes into a scrollable/tappable
control.

All directions belonging to one physical lane are overlaid on the same shared
stem and coordinate frame. Single-direction lanes and hero primitives use a
1.0 optical scale. Compound lanes uniformly scale the complete overlaid glyph
to 0.90 around the 24x24 center; individual branches are never scaled
independently.

## 9. Stage 2 semantic expansion

Stage 2 begins only after Stage 1 proves the live lane path and native card on
the Pi. It extends the same provider snapshot rather than adding QML access to
protocol objects.

Candidate fields already present in OAA v1.5 include:

- exact `UNAVAILABLE`, `ACTIVE`, `INACTIVE`, and `REROUTING` state;
- primary instruction and `road_info.road_names` as distinct values;
- roundabout exit number and turn angle;
- current road from `NavigationNextTurnDistanceEvent`;
- time to the current step;
- destination distance, ETA, and time to arrival;
- additional navigation steps for bounded lookahead.

Each field is promoted only after a capture shows live delivery on a supported
phone/Maps route. Absent fields remain absent; Prodigy does not infer or
synthesize them. Rerouting explicitly hides the obsolete maneuver and shows a
large `Finding a new route` state until fresh guidance arrives.

ETA/destination information may occupy a secondary footer or header region.
Lookahead and roundabout detail may enrich the main card. Lane guidance retains
the continuous roadway treatment approved in Stage 1.

The first live-proven Stage 2 field is the destination address carried by
`NavigationNotification`. When lane guidance is absent, the native card uses
the reserved lower band for a two-row destination footer: a compact label and
pin above a full-width, in-dash-sized destination line. The destination stays
stationary when it fits; overflow waits before scrolling slowly and resets
when the footer hides. Live lane guidance always replaces this footer, and the
primary maneuver region expands when neither is available.

A stationary Samsung S25+ route on 2026-07-31 repeatedly delivered the
destination address but did not deliver `NavigationNextTurnDistanceEvent`.
Destination distance, ETA, and time to arrival therefore remain gated pending
a moving-route capture; the card does not synthesize them.

## 10. Data flow and lifecycle

```text
Phone NavigationNotification / NavigationNextTurnDistanceEvent
        |
        v
prodigy-oaa-protocol NavigationChannelHandler
  - parse generated protobufs
  - emit current-step lane value snapshot
        |
        v
NavigationDataBridge / INavigationProvider
  - own active maneuver, distance, text, destination, and normalized lane model
  - replace snapshots atomically
  - clear on inactive/session loss
        |
        v
AAClusterWidget.qml
  map mode ------------------> existing auxiliary QVideoSink
  turn_card mode ------------> NativeNavigationCard.qml
                                  - ManeuverGlyph
                                  - continuous LaneGuidanceBand
                                  - ThemeService tokens
```

The handler and bridge remain on their existing Qt ownership path. Any new
value types used by a potentially queued signal are registered with Qt before
delivery. QML sees only GUI-thread-owned provider/model state.

## 11. Verification and acceptance

### 11.1 Focused automated coverage

- Service discovery always emits `KEYCODE_NAVIGATION` for the enabled
  auxiliary display regardless of local `map`/`turn_card` presentation.
- Changing `video.secondary_display_content` is removed from the AA reconnect
  boundary.
- Map mode claims/releases the sink using the existing current-page rules.
- Turn card mode owns no video sink and reacts immediately to configuration
  changes.
- Navigation inactive/session loss clears lane and maneuver state according to
  the provider contract.
- The first step's lanes are preserved in order; lookahead lanes are not
  flattened into them.
- Multiple directions and recommendation flags survive handler-to-provider
  transport.
- Every defined maneuver code resolves to an intentional presentation entry.
- Undefined values 30/31 and out-of-range values resolve to the fallback.
- Every lane-shape value resolves to an intentional presentation entry.
- Native card QML loads in disconnected, inactive, active-without-lanes, and
  active-with-lanes fixtures.
- Source/structure tests prohibit pointer handlers and per-lane button/cell
  styling in the lane band.

### 11.2 Repository gate

Because Stage 1 changes C++, QML, CMake/test coverage, runtime configuration,
and an embedded Pi UI, the final gate is:

```bash
cmake --build ~/builds/openauto-prodigy -j$(nproc)
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j$(nproc)
cd ~/builds/openauto-prodigy && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
./cross-build.sh
```

The implementation receives one bounded Fable review after the required gates
are green because this is protocol-path and embedded-UI work.

### 11.3 Pi/phone acceptance

On the available one-screen rig, inspect Map and Turn card sequentially rather
than claiming simultaneous visual coverage:

1. Connect AA at the current production GAL and confirm Map renders normally.
2. Start music and a route; select Turn card and confirm there is no AA
   reconnect or audio interruption.
3. Confirm maneuver, distance, road/instruction, and theme are readable at the
   installed viewing distance.
4. Exercise left, right, straight, ramp/fork/merge, roundabout, U-turn, and
   destination maneuvers where routes permit. Automated exhaustive mapping
   covers values that cannot be practically forced on one drive.
5. Select a route/location that produces lane guidance; confirm physical lane
   order, multiple directions, and every recommended movement match the phone.
6. Confirm the lane band is continuous and noninteractive, with no button-like
   cells.
7. Stop navigation and confirm the friendly inactive card contains no stale
   maneuver or lanes.
8. Switch back to Map and confirm immediate restoration without reconnect.
9. Repeat the core route/lane check on the Pixel and Samsung when available;
   note absent phone-provided lane data as evidence, not a rendering failure.

Stage 1 is accepted only after the Pi binary containing embedded QML is
deployed and the live Map/Turn card path passes. Stage 2 gets its own focused
live matrix for each newly promoted semantic field.

## 12. Executor guidance

- Read root, `src/core/aa/`, `qml/`, and
  `libs/prodigy-oaa-protocol/` `AGENTS.md` files before implementation.
- Do not modify the proto submodule.
- Start from provider/handler tests, then maneuver/lane mapping tests, then QML
  behavior.
- Keep the existing projected map path intact; local presentation selection is
  the only Stage 1 service-discovery policy change.
- Do not add Stage 2 fields opportunistically while implementing Stage 1.
- Preserve the user's unrelated dirty or untracked files.
- Record Stage 1 hardware evidence before beginning Stage 2 presentation work.
