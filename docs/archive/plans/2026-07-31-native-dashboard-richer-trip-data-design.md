# Native Dashboard Richer Trip Data

**Status:** COMPLETED 2026-07-31

**Approved:** 2026-07-31

**Grounded on:** `7c47172`

**Scope:** extend the hardware-accepted native Android Auto dashboard turn card
with live-proven rerouting, action-cue, next-step timing, and next-destination
summary data while preserving the accepted maneuver, lane, typography, and
projected-map behavior

## 1. Outcome

Prodigy's native dashboard turn card continues to make the current maneuver,
distance, and upcoming road the primary in-dash hierarchy. It adds only
information proven on the supported Samsung/Maps/GAL 6.0 path:

- an exact navigation state with a friendly `Finding a new route` presentation;
- one optional action cue distinct from the upcoming road label;
- coarse time to the next maneuver;
- next-destination distance, formatted ETA, and single-destination remaining
  duration in the existing destination footer.

The accepted lane-priority rule does not change. When lane guidance is present,
the continuous lane band replaces the complete destination/trip-summary
footer. Trip data is never squeezed into the maneuver region or lane band.

The implementation treats native navigation messages as independent complete
snapshots. It does not retain an optional timing value after the phone omits
that value, does not show cached guidance during rerouting, and does not infer
current-road, lookahead, multi-stop duration, or other absent data.

## 2. Evidence base

### 2.1 Moving-route capture

A Samsung S25+ at GAL 6.0 ran one complete Maps route through a mock-location
provider. The navigation-only capture has SHA-256
`61a145a0ba3a3c2612007215e78d8b92cdca2b3885236815311a16a0e6262f7e`.
It contained 1,151 synchronized `NavigationNotification` (`0x8006`) and
`NavigationNextTurnDistanceEvent` (`0x8007`) pairs plus the complete navigation
state sequence.

Observed delivery:

| Field or transition | Evidence |
|---|---|
| Step distance | present in every `0x8007` |
| Destination distance | present in every `0x8007` |
| Formatted ETA | present in every `0x8007` |
| `time_to_step_seconds` | present in 1,133/1,151 events |
| `time_to_arrival_seconds` | present in 1,146/1,151 events |
| Upcoming road label | present in every `0x8006` |
| Action-cue alternatives | present in every `0x8006`; differed from the upcoming road in 653 events |
| Rerouting | two `ACTIVE -> REROUTING -> ACTIVE` cycles, about 0.46 and 0.51 seconds |
| Route completion | final `INACTIVE` state |
| Current road | absent from every event |
| Multiple steps | absent; every notification contained exactly one step |
| Multiple destinations/distances | absent; each list contained exactly one entry |
| Roundabout parameters | not exercised by this route |

Protocol capture was returned to disabled after the run. Only the filtered
navigation evidence was retained; unrelated control traffic was removed.

### 2.2 Maps and Gearhead source trace

The focused Maps 26.30.05 / Android Auto Gearhead 17.3 trace is recorded in the
external research response with SHA-256
`cc49376c83e1575527a66216ef945fa820734a7274397c47c988736383d46c7d`.
Its decisive conclusions are:

1. Maps sends a navigation summary followed by one current turn event.
   Gearhead converts retained provider state into fresh `0x8003`, `0x8006`, and
   `0x8007` protobuf builders.
2. Native `0x8006` and `0x8007` messages are complete current snapshots, not
   deltas. Gearhead omits timing fields unless their current value is strictly
   positive. Absence means unavailable/non-positive now, not unchanged.
3. Wire `steps[0].instruction.text` comes from the framework's upcoming
   `roadName`. Wire `road_info.road_names` comes from ordered
   `cueAlternateTexts`; those strings are complete alternatives, not road-name
   fragments to concatenate.
4. Maps sends `REROUTING` state and returns without new guidance. On recovery,
   `ACTIVE` precedes the fresh `0x8006` by a small interval, so cached maneuver
   data remains stale until a post-reroute notification arrives.
5. Destination addresses are remaining waypoints in route order. Maps emits
   one destination-distance entry for the next stop, corresponding to index
   zero. Numeric time-to-arrival may describe the final route on a multi-stop
   trip and therefore is not promoted for that case.
6. Maps 26.30.05 intentionally constructs one current step. The repeated
   protobuf field does not establish Maps lookahead support.
7. The inspected Maps builder never assigns `current_road`; its absence in the
   live route is expected.

The source trace independently corroborates the capture. Where the trace could
not establish a universal contract, this design stays conservative.

## 3. Goals

- Preserve the hardware-accepted Map/Turn card switch without reconnecting AA.
- Preserve the current maneuver tile, primary distance hierarchy, centered
  upcoming-road row, exhaustive glyph coverage, and continuous lane band.
- Preserve exact native navigation state instead of collapsing `ACTIVE` and
  `REROUTING` into one Boolean.
- Never expose a pre-reroute maneuver as fresh guidance.
- Carry complete current-notification and current-position snapshots through
  source-level value types rather than exposing generated protobuf objects.
- Treat missing optional timing as absent immediately.
- Present the upcoming road and one ordered action-cue alternative with their
  correct meanings.
- Present next-destination summary data only where the phone supplies it and
  the destination association is source-backed.
- Retain compatibility with the accepted deprecated `NavigationTurnEvent`
  fallback used by older phone/provider paths.
- Keep ordinary in-dash type at the accepted readability floors.

## 4. Non-goals

- Editing `libs/prodigy-oaa-protocol/proto/`; OAA v1.5 already defines every
  required field and remains hands-off.
- Adding navigation fields to frozen External API v1 or its JS shim.
- Displaying trip summary while live lane guidance is present.
- Displaying a Maps current-road value; Maps 26.30.05 does not populate it.
- Implementing Maps multi-step lookahead; the inspected provider constructs
  only the current step.
- Treating cue alternatives as joinable road-name fragments or TTS text.
- Showing numeric remaining duration for multi-stop routes until a capture
  establishes whether it means next stop or final route end.
- Supporting multiple simultaneous destination-distance entries in the UI.
- Promoting roundabout exit number/angle without a focused live route capture.
- Adding EV charging or energy-forecast presentation.
- Restyling or replacing the accepted maneuver/lane glyph system.
- Changing projected display roles, GAL policy, codecs, or decoder lifecycle.

## 5. Runtime architecture

### 5.1 Protocol-library snapshots

`NavigationChannelHandler` continues to own protobuf parsing inside the
reusable protocol library. It emits small source-level values with presence
encoded explicitly rather than leaking generated messages or sentinel values.

The modern path produces three independent streams:

- exact navigation state: `UNAVAILABLE`, `ACTIVE`, `INACTIVE`, or `REROUTING`;
- current notification snapshot from `0x8006`:
  - current maneuver type;
  - upcoming road/sign/destination label from `instruction.text`;
  - ordered complete action-cue alternatives from `road_info.road_names`;
  - current-step lanes;
  - ordered destination addresses;
- current-position snapshot from `0x8007`:
  - step distance display/value/unit;
  - optional positive time to step;
  - ordered destination-distance entries containing distance, formatted ETA,
    and optional positive time to arrival;
  - current road only if some future provider actually supplies it.

Each successfully parsed message replaces its complete corresponding snapshot.
An absent optional field is represented as absent in the emitted value; it is
not copied from the preceding wire message.

The handler preserves protobuf order. Prodigy consumes only notification step
zero and destination/destination-distance index zero in this feature. It does
not flatten lookahead steps, join cue strings, or pair entries beyond the
source-backed index-zero Maps rule.

The deprecated `NavigationTurnEvent` signal remains supported. A new legacy
turn event replaces the legacy maneuver/distance snapshot as it does today and
may satisfy freshness for both primary guidance and distance on that path.

### 5.2 Provider ownership

`NavigationDataBridge` owns GUI-thread state exposed through
`INavigationProvider`. The provider adds internal, additive properties for:

- exact navigation state;
- whether primary guidance is fresh;
- upcoming road label;
- selected action cue;
- optional time to step;
- destination count and next-destination address;
- next-destination formatted distance and ETA;
- optional single-destination remaining time;
- independent presence flags for every optional presentation field.

Protocol value types crossing a queued Qt connection are registered before
delivery. Provider state remains the only QML boundary. QML does not access
protobufs, protocol handlers, EventBus topics, or transport objects.

The provider selects the first nonempty action-cue alternative in received
order that is not identical to the upcoming road label. If no distinct cue
exists, the action cue is absent. It never concatenates alternatives.

The provider uses destination and destination-distance index zero. If the
notification contains exactly one destination, a positive
`time_to_arrival_seconds` may be presented. If it contains more than one,
destination distance and formatted ETA remain eligible for index zero but
numeric remaining duration is hidden.

### 5.3 Independent-message freshness

Navigation state, notification, and position messages are not an atomic
transaction. Freshness is tracked per stream.

On `UNAVAILABLE` or `INACTIVE`:

- clear all maneuver, lane, position, destination, timing, and freshness state;
- show the existing friendly inactive presentation when AA remains connected.

On `REROUTING`:

- mark the current notification and position snapshots stale;
- retain them internally only if useful for diagnostics;
- hide the maneuver, lanes, distances, road/cue, and destination summary;
- show the full-card `Finding a new route` presentation.

On `ACTIVE` after rerouting:

- do not make cached data visible merely because the state changed;
- a post-reroute `0x8006` or legacy turn event makes the maneuver/road/lane
  region fresh;
- a post-reroute `0x8007` or legacy turn event makes distance/trip-summary data
  fresh;
- if position arrives first, retain it but keep the rerouting placeholder until
  primary guidance is fresh;
- once primary guidance is fresh, show the card; each optional position field
  remains hidden until its own stream is fresh and present.

This avoids both stale-maneuver flashes and deadlock on legacy providers that
do not use the modern message pair.

### 5.4 Optional-field clearing

Every modern notification or position message is a replacement snapshot.
Specifically:

- absent `time_to_step_seconds` clears the previous time-to-step immediately;
- absent `time_to_arrival_seconds` clears the previous remaining duration;
- absent destination distance or formatted ETA clears that field;
- absent lane data replaces the lane model with an empty snapshot;
- absent cue alternatives clears the selected cue;
- absent destination data clears the footer destination/summary state.

The implementation does not debounce by retaining stale values. Presentation
may transition cleanly, but it may not claim that an omitted value is still
current. This also closes the accepted review lead where modern distance
presence could remain latched after empty display text.

## 6. Presentation contract

### 6.1 Primary guidance

The accepted top-level hierarchy remains:

- wider label-free maneuver tile on the left;
- right-aligned cue and primary next-turn distance on the right;
- centered upcoming-road label below across the card width;
- lower lane band or destination/trip footer, never both.

When a distinct action cue is available, it replaces the generic `Next turn`
copy in the existing right-aligned secondary-cue slot. It is one line, uses the
accepted secondary-cue size, and elides rather than shrinking. When no distinct
cue exists, the slot continues to say `Next turn`.

When a positive time to step is present and space permits, the cue line appends
a coarse duration separated from the cue. It never displays a seconds counter:

- 1–59 seconds: `<1 min`;
- 60–3,599 seconds: whole minutes;
- 3,600 seconds or more: compact hours and minutes.

If cue text plus duration does not fit at the typography floor, cue text wins
and time to step is omitted. The primary distance remains unchanged and is
never shrunk to make timing fit.

### 6.2 Destination/trip-summary footer

When lane guidance is absent and a destination exists, the existing footer
retains its fixed lower-band height and two-row structure:

- top row: destination pin/label plus available trip metrics;
- bottom row: the full-width destination address with the accepted
  overflow-only marquee, dwell, and speed.

Eligible top-row metrics are:

1. next-destination formatted distance;
2. phone-formatted ETA text;
3. coarse remaining duration, only for exactly one destination.

Metrics are separated visually but are not button-like or interactive. They
use the accepted status-label floor. At compact widths, the row preserves
destination distance and ETA first, then omits remaining duration, and finally
omits the literal `DESTINATION` label before reducing type. The pin remains.
Missing fields collapse cleanly without placeholders or invented values.

Distance formatting reuses the accepted compact policy: miles retain one
decimal through 9.9 and round to a whole mile above 9.9. ETA text is displayed
verbatim from the phone. Remaining duration is derived only from a present
positive seconds value and rendered coarsely; it is never reconstructed from
ETA or wall-clock arithmetic.

When live lanes are present, the entire footer—including destination address
and every trip metric—is hidden. The continuous roadway-style lane band keeps
the full accepted width and height.

### 6.3 Rerouting and partial states

| State | Presentation |
|---|---|
| AA disconnected | Existing `Connect Android Auto` card |
| AA connected, navigation inactive/unavailable | Existing `Start a route in Android Auto` card |
| Rerouting | Large friendly `Finding a new route` card; no stale maneuver or trip data |
| ACTIVE but awaiting a fresh post-reroute notification | Continue rerouting placeholder |
| Fresh notification, position pending | Fresh maneuver/road/lanes; distance and trip fields hidden |
| Fresh notification and position | Full eligible card |
| Timing omitted at a maneuver boundary | Timing disappears; prior countdown is not retained |

The short rerouting interval must not flash the inactive or disconnected copy.
The placeholder uses theme tokens and the accepted in-dash font floors.

## 7. Compatibility and boundaries

- The projected map provider, AUXILIARY/NAVIGATION descriptor, channels 12/13,
  and local Map/Turn card setting do not change.
- Requested GAL remains the sole local protocol policy authority.
- Modern snapshots extend the provider without removing the accepted legacy
  flat-turn path.
- Existing maneuver and lane models remain the source for the hero and lane
  band; richer data does not create a second navigation presentation model.
- The protocol submodule's generated schema files remain untouched.
- External API v1 remains unchanged. Any future public trip-summary capability
  requires additive fields and an explicit capability flag.
- Protocol capture remains disabled by default and is not required at runtime.

## 8. Verification and acceptance

### 8.1 Focused automated coverage

- Exact state values survive handler-to-provider transport.
- `REROUTING` invalidates both snapshot freshness groups without exposing stale
  data.
- `ACTIVE` alone does not restore cached guidance after rerouting.
- A fresh notification restores only primary guidance; a fresh position
  restores only eligible distance/summary fields.
- Legacy turn events retain their accepted behavior and satisfy the appropriate
  freshness groups.
- Modern notification and position messages replace complete snapshots.
- Missing optional timing, cue, destination, and distance fields clear their
  preceding values.
- Ordered cue alternatives are preserved; presentation selects the first
  nonempty value distinct from the upcoming road and never concatenates.
- Destination and destination-distance index zero pair correctly.
- Multi-destination input hides numeric remaining duration while retaining
  index-zero distance and formatted ETA.
- Rerouting, partial-freshness, lanes-present, and lanes-absent QML fixtures load
  at both 1024x600 and the compact dashboard widget size.
- Lane presence hides the complete destination/trip footer.
- Compact footer layout drops lower-priority content before shrinking below the
  typography floor.
- Time formatting covers absent, sub-minute, minute, hour, and overflow-safe
  inputs.

### 8.2 Repository gate

The implementation changes reusable protocol code, C++, QML, embedded
resources, and target behavior. Its final gate is:

```bash
cmake --build ~/builds/openauto-prodigy -j$(nproc)
cmake --build ~/builds/openauto-prodigy \
  --target openauto-prodigy -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir ~/builds/openauto-prodigy \
  --output-on-failure
./cross-build.sh
```

Because this is protocol-path and embedded dashboard work, it receives the one
repository-required high-judgment Fable review after the green final gate.

### 8.3 Pi/phone acceptance

Use the one-screen rig sequentially and direct one case at a time:

1. Confirm the accepted projected Map path remains unchanged.
2. Start a single-destination mock route and switch locally to Turn card without
   reconnecting AA or interrupting audio.
3. With no lanes, confirm destination distance, ETA, remaining duration, and
   address are readable and stable; temporary missing timing must not leave an
   old value visible.
4. At a route segment with lanes, confirm the complete trip footer disappears
   and the accepted lane band is unchanged.
5. Confirm upcoming road and action cue have the captured meanings and do not
   appear concatenated.
6. Trigger rerouting. Confirm `Finding a new route` replaces stale guidance,
   then fresh guidance returns without an inactive-card flash.
7. End the route and confirm all primary, lane, destination, and timing state
   clears.
8. Return to Map and confirm immediate restoration without reconnect.

Multi-stop, roundabout, current-road, and lookahead acceptance are not claimed
by this feature.

## 9. Executor guidance

- Read root, `src/core/aa/`, `qml/`, and
  `libs/prodigy-oaa-protocol/` `AGENTS.md` files before implementation.
- Start with handler and bridge tests for snapshot replacement and rerouting
  ordering, then add QML presentation tests.
- Keep protobuf parsing in the protocol library and product policy in the
  provider/QML layers.
- Do not edit generated or source proto files in the hands-off submodule.
- Do not rename internal CLUSTER/AUXILIARY terminology in this feature.
- Do not add generic Maps lookahead, current-road, roundabout, EV, or multi-stop
  duration work while executing this design.
- Preserve the accepted maneuver/lane visuals and one-screen validation model.
- Record the final accepted SHA, ARM hash, live state sequence, and review
  adjudication in `docs/session-handoffs.md`.
