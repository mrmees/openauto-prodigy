# Phase 06: Content Widgets - Context

**Gathered:** 2026-03-12
**Status:** Ready for planning

<domain>
## Phase Boundary

Wire AA protocol data to QML and build two content widgets: navigation turn-by-turn and unified now playing. Users see live navigation and media info on the home screen without switching to fullscreen AA. Replaces the BT-only NowPlayingWidget with a unified source widget.

</domain>

<decisions>
## Implementation Decisions

### Navigation Widget Layout
- 2x1 (minimum): maneuver icon + distance only — road name omitted at this size
- 3x1: icon + distance + road name in a single row
- 3x2+: progressive reveal — primary turn info on top, next-turn preview below (divider between)
- 4x2: same as 3x2 with more breathing room and larger icon

### Navigation Inactive State
- Muted placeholder at ~40% opacity: dimmed navigation icon + "No navigation" text
- Widget stays visible in grid, does not hide/collapse
- Consistent with Now Playing inactive styling

### Distance Units
- Use the phone's distance_unit field directly (meters=1, km=2, miles=3, feet=4, yards=5)
- No user config needed — matches what the user sees on their phone's nav app
- Format examples: "0.3 mi", "500 m", "0.5 km"

### Source Priority & Switching
- Instant switch to AA metadata on AA connect (even if nothing playing yet)
- BT metadata returns only when AA disconnects
- No manual source toggle — priority is deterministic: AA > BT > none

### Now Playing Inactive State
- Muted placeholder at ~40% opacity: dimmed music icon + "No media" text
- Matches nav widget inactive styling
- Playback controls always visible, dimmed/disabled when no source

### Now Playing Layout
- 2x1 (minimum): track title (single line, elided) above prev/play-pause/next controls — no artist
- 3x1: horizontal strip — title + artist on left, controls on right, source icon far right
- 3x2 (default): full metadata — title, artist on separate lines, larger controls below, source icon
- Source indicator is a small icon (BT icon or AA icon), NOT text label — more glanceable, less space

### Album Art
- Deferred to future (POLISH-02) — skip for Phase 06
- Data bridge should accept and store albumArt bytes for future use, but not expose to QML yet

### Tap-to-Action
- Navigation widget: always opens AA fullscreen view (even when nav inactive, even when AA disconnected)
- Now Playing widget: no tap action on widget background — interaction only through playback control buttons
- Playback controls route to correct backend: AVRCP for BT, InputEvent for AA

### Claude's Discretion
- NavigationDataBridge and MediaDataBridge internal architecture (Q_PROPERTYs, signal wiring)
- QQuickImageProvider implementation for maneuver icon PNGs
- Distance formatting logic (when to show meters vs km, decimal places)
- Control button sizing and spacing at different breakpoints
- How to expose bridges to widget QML context (root context vs WidgetInstanceContext)
- EventBus vs direct signal connections for data flow

</decisions>

<specifics>
## Specific Ideas

- Source indicator as icon matches the automotive glanceability principle — no reading required
- Nav widget progressive reveal inspired by Android Auto's own turn card (compact → detailed)
- Muted placeholder states at ~40% opacity create visual consistency across all widgets when inactive

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `NavigationChannelHandler`: emits `navigationTurnEvent(roadName, maneuverType, turnDirection, turnIcon, distanceMeters, distanceUnit)` + `navigationStateChanged(bool)` — fully parsed, ready to wire
- `MediaStatusChannelHandler`: emits `metadataChanged(title, artist, album, albumArt)` + `playbackStateChanged(state, appName)` — fully parsed including album art bytes
- `BtAudioPlugin`: Q_PROPERTYs for trackTitle, trackArtist, connectionState, playbackState + play/pause/next/previous methods — pattern to follow for data bridges
- `AAStatusWidget.qml`: simple widget using root context properties — pattern for nav widget
- `NowPlayingWidget.qml`: current BT-only widget with pixel breakpoints — to be replaced by unified version
- `EventBus`: string-keyed publish/subscribe with QVariant payloads, thread-safe — available but not yet widely used

### Established Patterns
- Widget QML files require `QT_QML_SKIP_CACHEGEN TRUE` in CMake (known gotcha from v0.5.2)
- WidgetDescriptor stubs already registered in main.cpp: nav (org.openauto.nav-turn, min 2x1, max 4x2, default 3x1) and now-playing (org.openauto.now-playing, min 2x1, max 6x2, default 3x2)
- Pixel-based breakpoints for responsive layout (isFullLayout pattern from NowPlayingWidget)
- AAOrchestrator exposed as root QML context property with aaConnected Q_PROPERTY

### Integration Points
- AndroidAutoOrchestrator holds navHandler_ and mediaStatusHandler_ members — bridge classes connect to these signals
- Root QML context (engine.rootContext()) for exposing bridges
- WidgetRegistry qmlComponent URL must be set on nav-turn and now-playing descriptors
- MaterialIcon component uses `icon:` property with codepoints (e.g. "\ue55c" for navigation)

</code_context>

<deferred>
## Deferred Ideas

- Album art display in Now Playing widget — POLISH-02 (future milestone)
- Lane guidance arrows in nav widget — LANE-01 (uncharacterized bitmap format)

</deferred>

---

*Phase: 06-content-widgets*
*Context gathered: 2026-03-12*
