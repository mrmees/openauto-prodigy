# Native Dashboard Richer Trip Data Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:executing-plans` to implement this plan task-by-task. Repository
> policy keeps one inline implementation owner by default; use
> `superpowers:subagent-driven-development` only if the user separately requests
> bounded delegation. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** COMPLETED 2026-07-31

**Design:**
`docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-design.md`

**Grounded on:** `e70a012`

**Goal:** Extend the hardware-accepted native Android Auto turn card with exact
rerouting freshness, one source-backed action cue, next-step timing, and
next-destination summary data without changing the projected map path or lane
priority.

**Architecture:** `NavigationChannelHandler` converts the three modern
navigation messages into exact, complete source-level value snapshots and
keeps the deprecated flat turn event as a fallback. `NavigationDataBridge`
owns all GUI-thread freshness, cue-selection, pairing, and formatting policy,
then exposes additive QML-only properties without expanding External API v1.
`NativeNavigationCard.qml` consumes those properties and preserves the
accepted maneuver/lane hierarchy.

**Tech Stack:** C++17, Qt 6.8 Core/QObject/QML/Quick, protobuf-generated OAA
v1.5 bindings, Qt Test, CMake/CTest, embedded QML, Raspberry Pi 4 cross-build.

## Global Constraints

- Work on `dev`; do not create a worktree unless the user asks for one.
- `libs/prodigy-oaa-protocol/proto/` is hands-off. Do not edit generated or
  source proto files.
- Do not add fields to frozen External API v1 or expose these values through
  its JS shim.
- Preserve wireless-only AA, channels 12/13, the AUXILIARY/NAVIGATION
  descriptor, requested-GAL policy, codec policy, and decoder lifecycle.
- Preserve `NavigationTurnEvent` as the older-phone/provider fallback.
- Preserve the accepted maneuver glyphs, continuous lane band, typography
  floors, destination marquee, and immediate local Map/Turn switch.
- Live lanes replace the complete destination/trip footer; the two are never
  shown together.
- Do not add current-road presentation, Maps lookahead, roundabout detail, EV
  presentation, or multi-stop numeric remaining duration.
- Treat every successfully parsed `0x8006` and `0x8007` as a complete
  replacement snapshot. Missing optional fields clear immediately.
- Keep protobuf parsing in the reusable protocol library and product policy in
  `NavigationDataBridge`/QML.
- Execute tasks in order with one coherent commit per task. Do not push during
  execution.

---

## File and Interface Map

### Reusable protocol layer

- Modify
  `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp`
  to define the source-level snapshot types and three modern signals.
- Modify
  `libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp`
  to parse exact state and complete notification/position snapshots.
- Modify `tests/test_navigation_channel_handler.cpp` to prove field presence,
  ordering, replacement semantics, exact state transitions, and legacy-event
  compatibility.

The public source-level types are fixed for this plan:

```cpp
namespace oaa::hu {

enum class NavigationState {
    Unavailable = 0,
    Active = 1,
    Inactive = 2,
    Rerouting = 3,
};

struct NavigationDistanceData {
    bool hasValue = false;
    int value = 0;
    bool hasDisplayText = false;
    QString displayText;
    bool hasUnit = false;
    int unit = 0;
};

struct NavigationNotificationSnapshot {
    int stepCount = 0;
    bool hasManeuver = false;
    int maneuverType = 0;
    bool hasUpcomingRoad = false;
    QString upcomingRoad;
    QStringList actionCues;
    NavigationLaneGuidance lanes;
    QStringList destinations;
};

struct NavigationDestinationDistanceData {
    bool hasDistance = false;
    NavigationDistanceData distance;
    bool hasEstimatedTimeOfArrival = false;
    QString estimatedTimeOfArrival;
    bool hasTimeToArrival = false;
    qint64 timeToArrivalSeconds = 0;
};

struct NavigationPositionSnapshot {
    bool hasStepDistance = false;
    NavigationDistanceData stepDistance;
    bool hasTimeToStep = false;
    qint64 timeToStepSeconds = 0;
    QList<NavigationDestinationDistanceData> destinationDistances;
    bool hasCurrentRoad = false;
    QString currentRoad;
};

} // namespace oaa::hu
```

Declare/register `NavigationState`, `NavigationNotificationSnapshot`, and
`NavigationPositionSnapshot` as Qt metatypes. The handler signals are:

```cpp
void navigationStateSnapshotChanged(oaa::hu::NavigationState state);
void navigationNotificationChanged(
    const oaa::hu::NavigationNotificationSnapshot& snapshot);
void navigationPositionChanged(
    const oaa::hu::NavigationPositionSnapshot& snapshot);
```

Add the direct header dependencies used by these value types (`QString`,
`QStringList`, `QList`, `QMetaType`, and `QtGlobal`); do not rely on generated
protobuf headers to supply them transitively.

Task 1 adds the new signals while temporarily retaining and emitting the old
Boolean state and granular modern signals so the tree stays buildable while
the bridge and QML test fixtures migrate. Task 3 removes
`navigationStateChanged(bool)`, `navigationStepChanged`, `navigationDistanceChanged`,
`navigationNotificationReceived`, and `navigationLaneGuidanceChanged` after
their in-repository consumers move to the snapshots. Keep
`navigationTurnEvent` and `vehicleEnergyForecastReceived` unchanged.

### Product provider layer

- Modify `src/core/aa/NavigationDataBridge.hpp` and
  `src/core/aa/NavigationDataBridge.cpp` to consume snapshots, own freshness,
  and expose the new presentation values.
- Keep `src/core/services/INavigationProvider.hpp` unchanged so the frozen
  External API serializer and unrelated provider fakes do not acquire the new
  internal UI capability.
- Modify `src/core/aa/AndroidAutoOrchestrator.cpp` only to preserve the existing
  legacy EventBus topic payloads from the new snapshots; do not add richer
  EventBus fields or topics.
- Modify `tests/test_navigation_data_bridge.cpp` for exact state, freshness,
  clearing, selection, pairing, and formatting coverage.

`NavigationDataBridge` adds these QML properties while retaining all existing
`INavigationProvider` overrides:

```cpp
Q_PROPERTY(int navigationState READ navigationState
           NOTIFY navigationPresentationChanged)
Q_PROPERTY(bool guidanceFresh READ guidanceFresh
           NOTIFY navigationPresentationChanged)
Q_PROPERTY(bool rerouting READ rerouting
           NOTIFY navigationPresentationChanged)
Q_PROPERTY(bool hasActionCue READ hasActionCue NOTIFY turnDataChanged)
Q_PROPERTY(QString actionCue READ actionCue NOTIFY turnDataChanged)
Q_PROPERTY(bool hasTimeToStep READ hasTimeToStep NOTIFY distanceChanged)
Q_PROPERTY(QString formattedTimeToStep READ formattedTimeToStep
           NOTIFY distanceChanged)
Q_PROPERTY(int destinationCount READ destinationCount NOTIFY tripDataChanged)
Q_PROPERTY(bool hasDestinationDistance READ hasDestinationDistance
           NOTIFY tripDataChanged)
Q_PROPERTY(QString formattedDestinationDistance
           READ formattedDestinationDistance NOTIFY tripDataChanged)
Q_PROPERTY(bool hasDestinationEta READ hasDestinationEta
           NOTIFY tripDataChanged)
Q_PROPERTY(QString destinationEta READ destinationEta NOTIFY tripDataChanged)
Q_PROPERTY(bool hasTimeToArrival READ hasTimeToArrival NOTIFY tripDataChanged)
Q_PROPERTY(QString formattedTimeToArrival READ formattedTimeToArrival
           NOTIFY tripDataChanged)
```

`rerouting` is true during the exact `Rerouting` state and remains true after
the following `Active` edge until a fresh notification or legacy turn event
arrives. `navActive` remains true for exact `Active` and `Rerouting` states.

### QML presentation layer

- Modify `qml/widgets/NativeNavigationCard.qml` for rerouting, the action-cue
  line, step timing, and adaptive footer metrics.
- Modify `tests/test_aa_cluster_widget.cpp` to exercise the real card at
  1024x600 and 364x364 through the bridge snapshot signals.
- Do not modify maneuver or lane glyph components.

### Documentation and closure

- Update `src/core/aa/AGENTS.md` and
  `docs/aa-protocol/aa-display-rendering.md` with shipped semantics.
- Update `docs/roadmap-current.md`, `docs/INDEX.md`, and
  `docs/session-handoffs.md` after the final gate and hardware acceptance.
- Mark both this plan and its design `COMPLETED <date>` and move them to
  `docs/archive/plans/` in the closure commit.

---

### Task 1: Emit exact modern navigation snapshots

**Files:**

- Modify:
  `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp`
- Modify:
  `libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Test: `tests/test_navigation_channel_handler.cpp`

**Consumes:** OAA v1.5 generated `NavigationState`,
`NavigationNotification`, and `NavigationNextTurnDistanceEvent` messages.

**Produces:** the exact types and three signals in the File and Interface Map;
temporarily retained granular modern signals for the still-unmigrated bridge;
unchanged legacy turn-event and vehicle-energy signals; unchanged EventBus
topic names and payload keys.

- [ ] **Step 1: Replace the Boolean-state test with exact-state coverage**

  In `tests/test_navigation_channel_handler.cpp`, replace
  `testReroutingRemainsNavigationActive` with a test that sends
  `REROUTING`, `ACTIVE`, duplicate `ACTIVE`, `INACTIVE`, and `UNAVAILABLE`.
  Assert emitted values are, in order, `Rerouting`, `Active`, `Inactive`, and
  `Unavailable`; the duplicate produces no signal. Add a close-path assertion
  that an active channel emits `Unavailable` once on `onChannelClosed()`.

  ```cpp
  QCOMPARE(qvariant_cast<oaa::hu::NavigationState>(stateSpy[0][0]),
           oaa::hu::NavigationState::Rerouting);
  QCOMPARE(qvariant_cast<oaa::hu::NavigationState>(stateSpy[1][0]),
           oaa::hu::NavigationState::Active);
  ```

- [ ] **Step 2: Add a complete notification-snapshot test**

  Build two protobuf notifications. The first has two steps, a first-step
  maneuver, upcoming-road text, ordered cues `US-75 North` then `Downtown`,
  two first-step lanes, and two destinations (the second step must not leak
  into current-step fields). The second is an empty notification. Assert the
  first emitted snapshot preserves step count, cue/destination order, and only
  step-zero lanes; assert the second snapshot clears every optional/list field.

  ```cpp
  QCOMPARE(first.actionCues,
           QStringList({QStringLiteral("US-75 North"),
                        QStringLiteral("Downtown")}));
  QCOMPARE(first.destinations,
           QStringList({QStringLiteral("Stop One"),
                        QStringLiteral("Stop Two")}));
  QVERIFY(second.actionCues.isEmpty());
  QVERIFY(second.destinations.isEmpty());
  QVERIFY(second.lanes.isEmpty());
  ```

  Replace both existing granular notification/lane tests with this snapshot
  coverage; no handler test should depend on the compatibility signals.

- [ ] **Step 3: Add a complete position-snapshot test**

  Build one `0x8007` containing all optional step-distance fields,
  `time_to_step_seconds`, two destination distances, ETA text,
  `time_to_arrival_seconds`, and current road. Follow it with an empty `0x8007`.
  Assert field presence and list order in the first snapshot, then assert the
  second has no step distance, timing, destinations, or current road. Include
  one negative/non-positive timing case and assert the handler preserves wire
  presence/value; the bridge, not the protocol layer, decides display
  eligibility.

  ```cpp
  QCOMPARE(first.stepDistance.displayText, QStringLiteral("0.3"));
  QCOMPARE(first.destinationDistances[0].estimatedTimeOfArrival,
           QStringLiteral("4:42 PM"));
  QVERIFY(first.destinationDistances[0].hasEstimatedTimeOfArrival);
  QVERIFY(!second.hasStepDistance);
  QVERIFY(second.destinationDistances.isEmpty());
  ```

  Replace the existing granular audited-position test with this snapshot
  coverage.

- [ ] **Step 4: Run the focused handler test and confirm it fails**

  Run:

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_navigation_channel_handler -j$(nproc)
  ~/builds/openauto-prodigy/tests/test_navigation_channel_handler
  ```

  Expected: compilation fails because the snapshot types/signals do not exist,
  or the new exact assertions fail against the Boolean/granular implementation.

- [ ] **Step 5: Implement source-level types and exact state parsing**

  Add the mapped types, metatype declarations, registrations, and signal
  signatures. Replace `navActive_` with `NavigationState navigationState_`.
  Map protobuf values explicitly; unknown enum values map to `Unavailable`.
  Emit only on exact value changes. `onChannelClosed()` transitions any
  non-`Unavailable` state to `Unavailable`. Temporarily continue emitting the
  old `navigationStateChanged(bool)` only when the derived active Boolean
  changes, so unmigrated fixtures remain buildable through Task 2.

- [ ] **Step 6: Implement complete `0x8006` snapshot parsing**

  Parse step zero only for maneuver/upcoming road/cues/lanes, but preserve
  `steps_size()` in `stepCount`. Copy `road_info.road_names` in wire order as
  complete cue alternatives and copy every destination address in wire order.
  Emit exactly one `navigationNotificationChanged` per successfully parsed
  message, including an empty replacement snapshot.

- [ ] **Step 7: Implement complete `0x8007` snapshot parsing**

  Copy each protobuf `has_*` bit into the corresponding source-level presence
  flag. Preserve signed `qint64` timing values without applying product policy.
  Copy destination-distance entries in wire order and emit exactly one
  `navigationPositionChanged` per successfully parsed message, including an
  empty replacement snapshot. During this task, continue emitting the old
  granular signals after each new snapshot so the existing bridge still
  compiles and behaves until Task 2 migrates it.

- [ ] **Step 8: Preserve EventBus compatibility without richer exposure**

  Update `AndroidAutoOrchestrator.cpp` to consume
  `navigationStateSnapshotChanged` and derive the existing
  `aa.nav.state` `active` Boolean and the existing `aa.nav.step` and
  `aa.nav.distance` keys from the new snapshots. Do not publish action cues,
  timing, destination summary, exact state numerics, lanes, or protobuf
  objects. Fold the old enhanced-notification diagnostic log into the new
  notification connection.

- [ ] **Step 9: Run the handler test to green**

  Run the Step 4 commands. Expected: all handler cases pass, including existing
  legacy turn-event and energy-forecast tests.

- [ ] **Step 10: Commit the protocol snapshot boundary**

  ```bash
  git add \
    libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp \
    libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp \
    src/core/aa/AndroidAutoOrchestrator.cpp \
    tests/test_navigation_channel_handler.cpp
  git commit -m "refactor(aa): publish complete navigation snapshots"
  ```

### Task 2: Apply freshness and trip policy in `NavigationDataBridge`

**Files:**

- Modify: `src/core/aa/NavigationDataBridge.hpp`
- Modify: `src/core/aa/NavigationDataBridge.cpp`
- Test: `tests/test_navigation_data_bridge.cpp`

**Consumes:** `NavigationState`, `NavigationNotificationSnapshot`, and
`NavigationPositionSnapshot` from Task 1; unchanged `navigationTurnEvent`.

**Produces:** the exact QML properties in the File and Interface Map plus the
existing `INavigationProvider` contract. `INavigationProvider.hpp` and
External API serializers remain unchanged.

- [ ] **Step 1: Convert bridge fixtures to the snapshot interface**

  Add local fixture builders for active state, a notification snapshot, and a
  position snapshot. Convert existing modern notification/distance/lane tests
  from removed granular signals to the new snapshot signals. Keep the legacy
  turn-event tests unchanged.

  ```cpp
  emit handler.navigationStateSnapshotChanged(
      oaa::hu::NavigationState::Active);
  emit handler.navigationNotificationChanged(notification);
  emit handler.navigationPositionChanged(position);
  ```

- [ ] **Step 2: Add exact rerouting/freshness tests**

  Cover this sequence in one table-driven test:

  1. `Active`, fresh notification, fresh position: guidance/distance visible.
  2. `Rerouting`: `navActive` remains true, `rerouting` becomes true, and both
     freshness groups plus every visible guidance/trip field clear.
  3. `Active` alone: `rerouting` remains true and cached fields stay hidden.
  4. Position first: retain its values but keep the rerouting presentation.
  5. Notification second: `guidanceFresh` becomes true and rerouting clears;
     the already-fresh position fields become eligible.
  6. `Inactive` and `Unavailable`: clear all state and make `navActive` false.

  Assert the bridge emits inherited `navActiveChanged`, `turnDataChanged`,
  `distanceChanged`, and `laneGuidanceChanged` notifications for the same
  public-property changes it reports today; add `tripDataChanged` and
  `navigationPresentationChanged` assertions for the additive properties.

- [ ] **Step 3: Add replacement and cue-selection tests**

  Send a notification with upcoming road `I-35 North` and cues containing
  empty strings, `I-35 North`, `US-77 North`, and `Downtown`. Assert the bridge
  selects `US-77 North`. Send a replacement with no cues and assert
  `hasActionCue == false`, `actionCue` is empty, and lanes/destination are also
  cleared when omitted. Comparison uses trimmed, case-sensitive text; cue
  alternatives are never concatenated.

- [ ] **Step 4: Add destination pairing and multi-stop tests**

  Assert destination and distance entry zero are paired. For exactly one
  destination, positive arrival seconds expose all three summary metrics. For
  two destinations, distance and formatted ETA remain visible but remaining
  duration is hidden. Empty destination or position replacements clear the
  corresponding values immediately.

  ```cpp
  QCOMPARE(bridge.destination(), QStringLiteral("Stop One"));
  QCOMPARE(bridge.formattedDestinationDistance(), QStringLiteral("12 mi"));
  QCOMPARE(bridge.destinationEta(), QStringLiteral("4:42 PM"));
  QVERIFY(!bridge.hasTimeToArrival()); // after a two-destination snapshot
  ```

- [ ] **Step 5: Add duration and distance-format tests**

  Exercise absent, 1, 59, 60, 3,599, 3,600, 3,900, and `qint64` maximum-safe
  timing inputs. Expected strings are `<1 min`, `1 min`, `59 min`, `1 h`, and
  `1 h 5 min` as applicable. Assert non-positive values are absent. Reuse the
  existing distance-unit suffix policy and whole-mile-above-9.9 rule for both
  step and destination distance.

- [ ] **Step 6: Run the bridge test and confirm it fails**

  Run:

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_navigation_data_bridge -j$(nproc)
  ~/builds/openauto-prodigy/tests/test_navigation_data_bridge
  ```

  Expected: compilation or assertions fail because the new properties and
  freshness behavior are not implemented.

- [ ] **Step 7: Implement bridge state and snapshot ownership**

  Connect only the three modern snapshot signals plus the legacy turn event.
  Add `notificationFresh_`, `positionFresh_`, and `awaitingPostReroute_`.
  On rerouting, clear presentation values and both freshness groups. On the
  following active edge, retain `awaitingPostReroute_`; clear it only on a new
  notification or legacy turn event. Position-first recovery stores current
  values without ending the rerouting presentation.

- [ ] **Step 8: Implement replacement, selection, and pairing policy**

  Replace every notification-owned field from every notification snapshot and
  every position-owned field from every position snapshot. Select the first
  trimmed, nonempty, case-sensitive cue unequal to the trimmed upcoming road.
  Use only destination and destination-distance index zero. Allow positive
  arrival duration only when `destinationCount == 1`. Never concatenate cues,
  synthesize ETA, or consume `currentRoad`. Recompute the index-zero summary
  after either stream changes so position-first recovery becomes visible only
  after the current destination snapshot makes it eligible.

- [ ] **Step 9: Implement shared formatting helpers**

  Generalize the current distance formatting so step and destination distance
  share unit suffixes and the 9.9-mile threshold. Implement duration using
  integer division only:

  ```cpp
  if (seconds <= 0) return {};
  if (seconds < 60) return QStringLiteral("<1 min");
  const qint64 minutes = seconds / 60;
  if (minutes < 60) return QStringLiteral("%1 min").arg(minutes);
  const qint64 hours = minutes / 60;
  const qint64 remainder = minutes % 60;
  return remainder == 0
      ? QStringLiteral("%1 h").arg(hours)
      : QStringLiteral("%1 h %2 min").arg(hours).arg(remainder);
  ```

- [ ] **Step 10: Preserve the legacy fallback**

  A legacy turn event updates maneuver, road, icon, distance, and both
  freshness groups, and ends post-reroute waiting. It does not invent action
  cues, destination summary, or timing. Keep the accepted icon-provider and
  legacy distance behavior intact.

- [ ] **Step 11: Run focused provider, handler, and API regression tests**

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_navigation_channel_handler test_navigation_data_bridge \
    test_api_serializers test_api_publishers -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R '^(test_navigation_channel_handler|test_navigation_data_bridge|test_api_serializers|test_api_publishers)$'
  ```

  Expected: all selected tests pass and no External API expectation changes.

- [ ] **Step 12: Commit provider freshness and trip policy**

  ```bash
  git add src/core/aa/NavigationDataBridge.hpp \
    src/core/aa/NavigationDataBridge.cpp \
    tests/test_navigation_data_bridge.cpp
  git commit -m "feat(aa): model fresh dashboard trip data"
  ```

### Task 3: Render adaptive richer trip data without disturbing lanes

**Files:**

- Modify:
  `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp`
- Modify:
  `libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp`
- Modify: `qml/widgets/NativeNavigationCard.qml`
- Modify: `tests/test_aa_cluster_widget.cpp`

**Consumes:** the `NavigationDataBridge` QML properties from Task 2 and the
existing lane model/maneuver glyph.

**Produces:** exact rerouting presentation, action-cue/step-time line, and the
adaptive two-row footer. It does not create a second lane or maneuver model.

- [ ] **Step 1: Update the runtime fixture to send snapshots**

  Convert the native-card fixture from the removed granular signals to exact
  state, notification, and position snapshots. Preserve all existing
  assertions for Map/Turn switching, maneuver sizing, centered road text,
  lane geometry, destination marquee, and connection/no-route states.

- [ ] **Step 2: Add rerouting and partial-freshness assertions**

  Locate `navigationStateText` and `guidanceContent`. Assert `Rerouting` and
  the following `Active` edge show
  `Finding a new route`, hide maneuver/road/lane/footer, and never show the
  inactive copy. Send position first and assert the placeholder remains; send
  notification next and assert guidance returns.

- [ ] **Step 3: Add action-cue and time-fit assertions**

  Add `objectName`s `secondaryCueText` and `stepTimeText`. At 1024x600, assert
  a distinct cue replaces `Next turn`, both texts are right aligned, and
  `5 min` appears after the cue. Supply a long cue and resize to 364x364;
  assert the cue remains at the typography floor while `stepTimeText` becomes
  invisible. With no action cue, assert the label returns to `Next turn`.

- [ ] **Step 4: Add footer priority and lane-exclusion assertions**

  Add object names `destinationMetricRow`, `destinationDistanceText`,
  `destinationEtaText`, and `destinationDurationText`. At 1024x600, assert all
  eligible single-destination values appear above the unchanged marquee row.
  At 430x364, assert distance and ETA remain visible, duration is hidden, and
  the literal destination label remains. At 364x364, assert distance and ETA
  remain visible while both duration and the literal label are hidden. At all
  three sizes, assert the metric fonts stay at the accepted `labelSize` floor.
  Send lanes and assert the complete footer and every metric/address item are
  invisible while the continuous band is visible.

- [ ] **Step 5: Run the QML/widget test and confirm it fails**

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_aa_cluster_widget -j$(nproc)
  QT_QPA_PLATFORM=offscreen \
    ~/builds/openauto-prodigy/tests/test_aa_cluster_widget
  ```

  Expected: assertions fail because the new presentation objects and state
  rules do not exist.

- [ ] **Step 6: Implement the rerouting presentation state**

  Make guidance visible only when AA is connected, navigation is active,
  primary guidance is fresh, and `rerouting` is false. In the existing state
  area, choose copy in this order: disconnected `Connect Android Auto`;
  active-but-not-fresh/rerouting `Finding a new route`; otherwise
  `Start a route in Android Auto`. Use existing theme tokens and font floors.

- [ ] **Step 7: Implement the right-aligned cue/time row**

  Keep cue and duration as separate `Text` items in a right-aligned row so cue
  text wins. Bind cue to `actionCue` or `Next turn`. Show step time only when
  present and the sum of cue implicit width, duration implicit width, separator
  spacing, and margins fits the available top-info width. Elide cue text; never
  shrink the primary distance or either accepted font floor.

- [ ] **Step 8: Implement the adaptive footer metric row**

  Keep the footer height, pin, bottom address viewport, 2-second dwell, and
  24-pixel/second marquee unchanged. Put destination distance, phone ETA, and
  remaining duration in the top row as noninteractive text separated by
  spacing/dividers. Fit priority is distance, ETA, duration, literal
  `DESTINATION`; the pin never disappears. Missing values collapse without
  placeholders. Preserve `destinationFooter.visible` as
  `showGuidance && !laneBand.visible && destination.length > 0`, ensuring lane
  presence hides the whole footer.

- [ ] **Step 9: Remove the temporary granular modern signals**

  After the runtime fixture consumes snapshots, remove the old Boolean and four
  granular signal declarations plus their compatibility emissions from
  `NavigationChannelHandler`. Confirm `rg` finds no remaining references to
  `navigationStateChanged(bool)`, `navigationStepChanged`, `navigationDistanceChanged`,
  `navigationNotificationReceived`, or `navigationLaneGuidanceChanged` in
  `src/`, `tests/`, or `libs/prodigy-oaa-protocol/`.

- [ ] **Step 10: Run focused UI and handler tests to green**

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_navigation_channel_handler test_aa_cluster_widget -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R '^(test_navigation_channel_handler|test_aa_cluster_widget)$'
  ```

  Expected: handler snapshots pass; the complete existing glyph/lane/card
  suite and new richer-trip assertions pass offscreen at 1024x600, 430x364,
  and 364x364.

- [ ] **Step 11: Commit the embedded QML presentation**

  ```bash
  git add \
    libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp \
    libs/prodigy-oaa-protocol/src/HU/Handlers/NavigationChannelHandler.cpp \
    qml/widgets/NativeNavigationCard.qml \
    tests/test_aa_cluster_widget.cpp
  git commit -m "feat(ui): render richer dashboard trip data"
  ```

### Task 4: Verify, deploy, accept, review, and close the feature

**Files:**

- Modify: `src/core/aa/AGENTS.md`
- Modify: `docs/aa-protocol/aa-display-rendering.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move after acceptance:
  `docs/plans/2026-07-31-native-dashboard-richer-trip-data-design.md` to
  `docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-design.md`
- Move after acceptance:
  `docs/plans/2026-07-31-native-dashboard-richer-trip-data-plan.md` to
  `docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-plan.md`

**Consumes:** the final implementation tree from Tasks 1–3.

**Produces:** green native/ARM evidence, sequential Pi/phone acceptance, one
bounded Fable verdict, an accepted SHA and ARM hash, and completed canonical
documentation. Publication remains a separate user-authorized action.

- [ ] **Step 1: Run the three focused suites together**

  ```bash
  cmake --build ~/builds/openauto-prodigy \
    --target test_navigation_channel_handler \
    test_navigation_data_bridge test_aa_cluster_widget -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R '^(test_navigation_channel_handler|test_navigation_data_bridge|test_aa_cluster_widget)$'
  ```

  Expected: all three pass.

- [ ] **Step 2: Run the repository native gate once on the final code tree**

  ```bash
  cmake --build ~/builds/openauto-prodigy -j$(nproc)
  cmake --build ~/builds/openauto-prodigy \
    --target openauto-prodigy -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure
  git diff --check
  ```

  Expected: build, explicit app target, full CTest suite, and whitespace check
  all pass.

- [ ] **Step 3: Cross-build and record the ARM artifact identity**

  ```bash
  ./cross-build.sh
  sha256sum build-pi/src/openauto-prodigy
  git rev-parse HEAD
  ```

  Record both values before deployment.

- [ ] **Step 4: Deploy the embedded-QML binary and restart Prodigy**

  ```bash
  rsync -av build-pi/src/openauto-prodigy \
    matt@192.168.1.149:~/openauto-prodigy/build/src/
  ssh matt@192.168.1.149 \
    'sudo systemctl restart openauto-prodigy.service'
  ```

  Confirm the Pi binary hash matches the local ARM artifact before live
  testing. If restart leaves a stale process, use the documented
  `~/openauto-prodigy/restart.sh --force-kill` recovery once.

- [ ] **Step 5: Run one-screen Pi/phone acceptance sequentially**

  Direct one case at a time; do not put the user on a timer:

  1. Map mode still renders normally.
  2. Switch locally to Turn card without reconnecting or interrupting audio.
  3. With no lanes, verify upcoming road, distinct action cue, next-step time,
     destination distance, ETA, single-destination duration, and marquee.
  4. With lanes, verify the entire footer disappears and the accepted lane
     band is unchanged.
  5. Trigger rerouting and verify `Finding a new route` replaces all stale
     guidance until the first fresh post-reroute notification.
  6. End the route and verify every primary, lane, timing, and footer field
     clears.
  7. Return to Map and verify immediate restoration without reconnecting.

  Record the accepted code SHA. Multi-stop, roundabout, current-road, and
  lookahead behavior are explicitly not accepted by this run.

- [ ] **Step 6: Run the single major review gate after hardware acceptance**

  ```bash
  richer_trip_accepted_sha="$(git rev-parse HEAD)"
  bash scripts/review-gate.sh --author codex --major \
    --base e70a012 --accepted "$richer_trip_accepted_sha"
  ```

  Adjudicate every finding under repository rules. Only a supported-production
  blocker may churn the hardware-accepted tree. If remediation is required,
  rerun the affected focused tests, native/app/full CTest gate, cross-build,
  relevant live case, and the one allowed remediation review. Do not start a
  third review pass.

- [ ] **Step 7: Update shipped behavior documentation**

  Replace the Stage 1 evidence gate in `src/core/aa/AGENTS.md` and
  `docs/aa-protocol/aa-display-rendering.md` with the exact shipped snapshot,
  rerouting, cue, timing, destination, and lane-priority semantics. Move the
  roadmap item from Now to Done with the accepted code SHA. Keep unresolved
  multi-stop, roundabout, other-provider lookahead, and current-road questions
  in `docs/validation-current.md`; do not claim them completed.

- [ ] **Step 8: Complete and archive both active documents**

  Read the execution date with `date +%F`, change both status headers to
  `COMPLETED` followed by that date, move both files into
  `docs/archive/plans/`, update `docs/INDEX.md` links, and append a concise
  session handoff with code SHA, ARM hash, focused/native/full/ARM results,
  hardware cases, and confirmed/dismissed/deferred review counts.

- [ ] **Step 9: Run documentation closure checks**

  ```bash
  python3 scripts/check-doc-links.py --scope tracked-live
  git diff --check
  ```

  Expected: zero broken links and no whitespace errors. No second application
  build is required if the closure commit changes documentation only.

- [ ] **Step 10: Commit closure without pushing**

  ```bash
  git add src/core/aa/AGENTS.md docs/aa-protocol/aa-display-rendering.md \
    docs/roadmap-current.md docs/INDEX.md docs/session-handoffs.md \
    docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-design.md \
    docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-plan.md
  git commit -m "docs(aa): close richer dashboard trip data"
  ```

  Report the final commits, verification, accepted SHA/ARM hash, review
  adjudication, and untouched user files. Push or open a PR only after the
  user's explicit go-ahead.
