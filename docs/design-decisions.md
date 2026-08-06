# Design Decisions

Architectural rationale for OpenAuto Prodigy implementation choices. This documents *why* decisions were made, not just *what* was built.

---

## Android Auto Service Layer

### Qt-native wireless transport

**Decision:** Run the wireless AA listener, TCP transport, session state
machine, and channel handlers as Qt objects on the main event loop.

**Rationale:** `QTcpServer`, `QTcpSocket`, signals, and timers provide one
ownership model for session lifecycle and eliminate a separate protocol thread
pool. One resolver validates `connection.tcp_port` for the listener; its
default is 5277, and Bluetooth discovery advertises the listener's actual
bound port. Bluetooth discovery supplies WiFi AP
credentials, the phone joins the AP, and the same Qt transport carries version
exchange, TLS, service discovery, and channel traffic.

The listener admits replacement clients only while connection setup is still
pending. Once a session is connected or backgrounded, extra TCP connections
are rejected without changing the active peer, state, or watchdog. Shutdown
closes admission before synchronously stopping the session, avoiding a nested
event-loop wait and reentrant replacement during teardown.

**Historical context:** An earlier prototype used a third-party protocol stack
and explored USB AOAP. Neither is part of the current architecture. The
wireless-only product decision remains, but the implementation is now the
in-tree `prodigy-oaa-protocol` library.

---

## Video Pipeline

### FFmpeg with automatic hardware selection and software fallback

**Decision:** Decode the two shipped projection codecs, H.264 and H.265/HEVC,
through `VideoDecoder` using FFmpeg.

**Rationale:** The configured decoder name wins when present. In automatic mode
the decoder tries available hardware paths, including DRM/V4L2-request
acceleration for HEVC on the Pi, then falls back to FFmpeg's software decoder.
Software output uses recycled YUV420P `QVideoFrame` storage; DRM-prime output is
wrapped by `DmaBufVideoBuffer`. Keeping selection and fallback behind one
decoder preserves the same rendering contract on the Pi and development
machines.

### Rendering: `QVideoSink` + QML `VideoOutput`

**Decision:** Deliver decoded `QVideoFrame`s to `QVideoSink::setVideoFrame()`
and render them with QML `VideoOutput`.

**Rationale:** Qt owns the render-side YUV conversion and QML integration. The
decoder keeps a latest-frame slot so display latency does not grow when more
than one decoded frame becomes ready before the UI consumes it.

### Annex B framing and codec configuration packets

**Decision:** Feed the received Annex B byte stream to FFmpeg's parser without
prepending start codes, and forward both timestamped and untimestamped AV media
payloads.

**Rationale:** Codec configuration units can arrive in the untimestamped media
form while regular frames use the timestamped form. Both reach
`VideoChannelHandler::onMediaData`; dropping the configuration payload leaves
the decoder without the parameter sets required for subsequent frames.

### Configurable negotiated video mode

**Decision:** Advertise the configured fixed AA resolution (480p, 720p, or
1080p), frame rate, and enabled codec descriptors. The default mode is 720p.

**Rationale:** Service discovery computes content margins for the actual display
viewport and optional navbar. The matching content dimensions also define the
phone's touch coordinate space, so video crop and input mapping remain aligned.
The recognized codec list is resolved once per session and supplies both the
advertised descriptors and the video-channel setup-response count, preventing
those protocol surfaces from disagreeing after configuration fallback.

### Decode worker, Qt-owned sink

**Decision:** Parse and decode compressed frames on `VideoDecoder::DecodeWorker`
and update `QVideoSink` on the Qt main thread.

**Rationale:** Decode can run independently without blocking UI and socket
handling, while `frameReady` returns through Qt signal delivery to the sink's
owning thread. Queue-depth load shedding may skip non-reference output, but it
never drops compressed packets that the reference chain needs.

### Stream boundaries are ordered worker commands

**Decision:** Place a decoder reset command in the same worker queue as
compressed video whenever the video channel starts a stream.

**Rationale:** The decoder object is intentionally process-long, while codec,
parser, fallback, queued-packet, and latest-frame state belongs to one phone
stream. Frame signals enter the thread-safe worker queue directly, preserving
their emission order with the stream-start reset. Clearing queued prior-stream
packets and ordering reset before later frames prevents H.264/H.265 state from
crossing reconnects without racing the decode thread or rebuilding the whole
projection plugin.

### OpenMAX IL remains out of scope

Original openauto used OMX via `ilclient` for a Pi-era zero-copy path. The
current Trixie stack uses FFmpeg and Qt Multimedia instead; there is no OMX
compatibility path.

---

## Touch Input

### Direct evdev ownership during projection

**Decision:** Read Linux MT Type B events with `EvdevTouchReader` instead of
placing a QML touch handler over projection video.

**Rationale:** While Android Auto owns input, `EVIOCGRAB` prevents duplicate
Wayland/libinput delivery. The reader tracks the complete active pointer set,
maps raw coordinates into the same content space advertised to the phone, and
emits Android MotionEvent-compatible down, move, and up messages. Leaving the
AA view or backgrounding/disconnecting projection releases the grab so normal
QML input resumes.

The reader thread exclusively owns the device descriptor, actual grab state,
slot history, and phone-visible pointer membership. Main-thread configuration
is published as coherent snapshots before or during the reader loop; device
loss closes and reopens on a paced retry that remains interruptible at stop.
Before ungrab, loss, or shutdown clears reader state, any phone-visible
pointers are retired with valid pointer-up/up messages.

### Touch routing and the navbar

**Decision:** Route grabbed evdev input through `TouchRouter` before forwarding
unclaimed pointers to AA.

**Rationale:** `NavbarController` registers priority hit zones through
`EvdevCoordBridge`; those zones can consume a pointer for head-unit controls
even though QML `MouseArea`s cannot receive the grabbed device. Unclaimed
pointers fall through to `TouchHandler` and the AA input channel. Raw slot
activity and AA membership are separate: a zone-claimed pointer never appears
in AA's full-pointer arrays, and same-report transitions are serialized in
Android MotionEvent order. A 3-finger gesture suppresses AA forwarding and
opens the system overlay.

The QML navbar is authoritative for its three rendered rectangles. After an
edge, scale, layout, size, or visibility transition it starts a generation and
reports the actual 20/60/20 control geometry in shell coordinates;
`NavbarController` does not reconstruct layout ratios or bar thickness.
Starting a newer report removes the former claims, and invalid or hidden
geometry leaves the navbar unregistered. `TouchRouter` continues to own
coordinate conversion and sticky routing, so unclaimed projection touches are
unchanged. Each queued navbar callback carries the geometry generation that
claimed it; the controller rejects stale events before they can change gesture
ownership or start a timer.

An accepted evdev slot owns its navbar gesture until matching release or
cancellation. Competing downs cannot replace that owner. Popup generations are
published before QML visibility changes are observed, so cleanup from an
outgoing slider or power menu cannot hide the incoming popup. Popup-button tap
origins are fixed per region and touch slot for that generation; they are not
shared between fingers or allocated on the reader-thread event path.

### Touch debug overlay

**Decision:** Keep an optional QML overlay showing the AA-space touch points
reported by the evdev path.

**Rationale:** `TouchHandler` queues only the debug model update to the main
thread. The overlay is diagnostic; it is not the source of projection input.

---

## Fullscreen and Navbar Ownership

**Decision:** The normal application window starts in `Window.FullScreen`; the
`--geometry` development override is windowed. Plugin fullscreen preference is
an in-shell navbar decision.

**Rationale:** `PluginModel.activePluginFullscreen` controls whether `Navbar`
is visible and therefore whether content receives edge-to-edge shell space. It
does not toggle native window visibility. Android Auto returns
`wantsFullscreen() == false` by default because `navbar.show_during_aa` defaults
to enabled; disabling that setting hides the navbar and gives projection the
full shell viewport.

---

## v0.6-v0.6.1: Widget Framework and Grid Architecture

### Grid Sizing: DPI-based cellSide Formula

**Decision:** `cellSide = diagPx / (9.0 + bias * 0.8)` where `diagPx` is the pixel diagonal and `bias` is a user-adjustable density parameter.

**Rationale:** The grid needs to be resolution-independent. A fixed pixel cell
size breaks on different displays; a fixed cell count wastes space on large
screens and cramps small ones. The diagonal-based divisor scales with the
available pixels. The DPI cascade in `DisplayInfo::updateCellSide()` validates
screen-DPI trustworthiness, but every current path produces the same formula.
Constants `kBaseDivisor` and `kBiasStep` live in `DisplayInfo.hpp`.

### Grid Cols/Rows Computed in QML

**Decision:** Grid columns and rows are computed as reactive QML bindings from
`DisplayInfo.cellSide`. C++ supplies initial dimensions while
`DashboardManager` loads and seeds models, then QML owns the reactive path.

**Rationale:** QML bindings automatically re-evaluate when the window resizes,
density bias changes, or any input to the formula changes. The startup
dimensions let dashboard models load before the QML engine is ready. After
startup, `HomeMenu.qml` pushes its snap-aware dimensions to all dashboard
models.

### Auto-Snap Threshold for Gutter Space Recovery

**Decision:** Two-pass snap with a 50% waste threshold
(`kSnapThreshold: 0.5`) and a cascade guard preventing runaway iteration.

**Rationale:** When the computed grid has significant wasted space (gutter),
adding one more column or row recovers it at the cost of slightly smaller
cells. Pass 1 snaps axes where waste exceeds 50% of the base cell size. Pass 2
catches cascaded waste from Pass 1's cell shrinkage, but only on axes that did
not snap in Pass 1. This prevents iterative packing (for example, 7x4 -> 8x5
-> 9x5). The single re-evaluation is sufficient because the cell shrinkage
from one additional row or column is bounded.

### Dock Replaced by Singleton Widgets

**Decision:** Replace the z=10 overlay dock (which sat outside the ColumnLayout) with singleton launcher widgets on a reserved page.

**Rationale:** The dock consumed fixed vertical space that could not be reclaimed by the grid. Singleton widgets (e.g., `org.openauto.settings-launcher`, `org.openauto.aa-launcher`) participate in the same grid model as regular widgets, unifying the layout system. They are marked `singleton = true` — system-seeded, non-removable, and hidden from the picker. The reserved page is derived from singleton presence, not stored as explicit state.

### Model-owned widget-picker categories

**Decision:** `WidgetPickerModel` filters descriptors by available space,
sorts them by category/name, exposes the available category tabs, and applies
the active category filter. QML renders the resulting model as a full-screen
three-column catalog.

**Rationale:** The registry is assembled at startup from built-ins, plugin
contributions, and optional web-widget packages, so its size is not a stable
constant. Space/category filtering and ordering belong in the typed model;
QML owns only touch presentation. The full-screen catalog avoids nested
horizontal scrollers, keeps category targets at least 64 pixels high on the
reference display, and leaves enough room for readable metadata.

### Widget picker uses default-size placement

**Decision:** Tapping a picker card places the widget at its descriptor's
default size immediately. Cards show a filled default-size indicator plus one
outlined minimum-to-maximum range; actual resizing remains in dashboard edit
mode. Descriptions remain descriptor metadata but are omitted from the compact
in-vehicle card.

If the default size cannot fit, the picker stays open and displays the failure
inside its own overlay. It does not silently shrink to the minimum: placement
size remains the descriptor default, matching the card's filled indicator.

**Rationale:** Choosing a size before placement duplicated the existing resize
workflow and added a modal step in a touch-first vehicle UI. Descriptor authors
retain full min/default/max control, while users can see the sizing envelope
without being forced through another decision.

### Category Order Hardcoded

**Decision:** Category display order is a static map: status=0, media=1, navigation=2, launcher=3.

**Rationale:** The four built-in categories have fixed semantics. The ordering
map in `WidgetPickerModel` handles those categories explicitly and places
unknown extension categories after them without rejecting them.

### Remap Clamps Oversized Spans

**Decision:** When grid dimensions change (display resize, density bias), widgets whose spans exceed the new grid bounds are clamped to fit rather than hidden.

**Rationale:** Hiding widgets would make them disappear silently — users would think they were deleted. Clamping preserves the widget at a reduced size, maximizing visibility. The original span is stored in `basePlacements_` and restored if the grid grows back.

### Remap Keeps Every Placement on a Reachable Page

**Decision:** The base snapshot includes its intentional page count as well as
placements and dimensions. A remap derives the exact reachable `pageCount` from
that baseline plus any current spill, before publishing changed placements.
Returning to the baseline dimensions removes surplus spill-only pages while
preserving intentionally empty user pages. Pages containing singleton launchers
remain the trailing reserved pages in stable order. Loaded placements expand a
stale persisted page count, and negative loaded page values normalize to the
first page.

**Rationale:** A placement outside `0 <= page < pageCount` cannot be reached by
the dashboard pager and can later be mistaken for abandoned state. Growing the
model-owned page range at the placement boundary keeps navigation and YAML
persistence consistent without adding consumer-side correction.
Explicit page add/remove operations and placement edits promote the current page
topology into the baseline, so only automatic spill growth is reversed.

### Hidden Placements Remain Page Occupants

**Decision:** The model exposes visible widget count separately from total
retained placement count. Destructive empty-page cleanup uses the total count,
including placements temporarily hidden because their minimum span does not fit
the current grid.

**Rationale:** Visibility is a presentation result of the current dimensions,
not placement ownership. Retaining the page allows the hidden widget to become
visible again when the grid grows instead of deleting it as collateral cleanup.

### promoteToBase() on Every Mutation

**Decision:** Before a selected-widget dimension remap and user mutation can
overlap, the pending remap is applied. After every user edit (move, either
resize path, add, remove, opacity, or configuration change), `promoteToBase()`
copies `livePlacements_` to `basePlacements_`.

**Rationale:** Remap derives its output from `basePlacements_`, so base must
always reflect the latest user intent in one coordinate system. Applying a
queued remap first prevents an edit from promoting stale coordinates under the
new dimensions. Without this, a later resize can revert or displace recent
edits. The copy is cheap (small QList of POD-like structs).

### Base/Live Snapshot Pattern

**Decision:** Two parallel placement lists: `basePlacements_` (persisted to
YAML, updated by explicit user mutation) and `livePlacements_` (runtime state
exposed to QML). `DashboardManager` serializes the baseline placements, page
count, and matching baseline dimensions; automatic remap notifications never
serialize live-only topology.

**Rationale:** Clean separation of persisted vs runtime state. Remap reads from
base and writes to live, ensuring the remap algorithm always has a stable input
even across a restart at constrained dimensions. Structural edits (move,
resize, add, remove) update live first, then `promoteToBase()` copies live back
to base so future remaps reflect the latest user intent. Because grid dimensions
are one global YAML value, a model's user-mutation signal makes the manager
adopt every dashboard's current live topology at the same dimensions before one
save. This prevents different dashboards from being serialized against mixed
coordinate systems while keeping QML bound to live placements.

### Reserved Page Derived from Singleton Presence

**Decision:** Whether a page is "reserved" (protected from deletion) is derived from `pageHasSingleton()` at runtime, not stored as an explicit page flag.

**Rationale:** Derived state is more robust than stored state. If a singleton
widget is moved to a different page, the reservation follows it automatically.
A stored flag would require synchronization logic to track widget moves. When
spill expands the dashboard, reserved pages retain their relative order at the
end of the reachable page range.

### Fixed instanceIds for Seeded Singletons

**Decision:** System-seeded singleton widgets get deterministic instance IDs like `"aa-launcher-reserved"` and `"settings-launcher-reserved"`.

**Rationale:** `DashboardManager` seeds only the `home` dashboard when its
loaded placement list is empty, so duplicates are prevented by that empty-check
gate rather than ID-based deduplication. The fixed IDs make the seeded
placements predictable and consistent. YAML load/save does not deduplicate by
instance ID.

### Clock as Active Page Indicator

**Decision:** The navbar clock control shows page indicator dots: `leftDotCount = activePage`, `rightDotCount = pageCount - activePage - 1`.

**Rationale:** Repurposes an existing navbar element instead of adding a
separate page indicator UI. The dot pattern is intuitive: dots on either side
represent pages before and after the active page.

### WidgetContextFactory as Dedicated Class

**Decision:** Context creation for widget instances is handled by `WidgetContextFactory`, a separate QObject, not by methods on `WidgetGridModel`.

**Rationale:** Keeps the model as pure data (placements, roles, CRUD). The
factory receives the `IHostContext` reference and cell geometry needed to
construct `WidgetInstanceContext` instances. This separation prevents the
model from accumulating service dependencies. The factory's `cellSide`
property provides the initial dimensions; bindings in `HomeMenu.qml` keep cell
size, spans, and current-page state reactive on each instance context.

### Context Injection via Loader.onLoaded + Binding

**Decision:** Widget context is injected into QML via `Loader.onLoaded` with `Binding` elements for reactive properties, not via QML context properties.

**Rationale:** Context properties are untyped, undocumented, and invisible to tooling. The `property QtObject widgetContext: null` declaration in widget QML is typed, documented, and supports NOTIFY. `Binding` elements for `colSpan`, `rowSpan`, `cellWidth`, `cellHeight`, and `isCurrentPage` keep values reactive without imperative update code. The `when: widgetCtx !== null` guard prevents binding errors during the brief Loader initialization window.

### NowPlayingWidget controls through shared actions

**Decision:** The widget reads state from its `IMediaStatusProvider`-backed
instance context and dispatches `media.playPause`, `media.next`, and
`media.previous` through `ActionRegistry`.

**Rationale:** The widget does not need to know whether Android Auto,
Bluetooth, or local playback currently owns the media surface.
`MediaStatusService` remains the routing authority, while the action path gives
QML and external clients the same mutation boundary.

---

### BlueZ ObjectManager snapshots own Bluetooth device state

**Decision:** `BluetoothManager` asynchronously reads and coalesces BlueZ
ObjectManager snapshots, then derives adapter, paired, connected, first-run,
and auto-connect state together from each complete snapshot.

**Rationale:** Independent property reads race startup and object removal, can
miss a phone that was connected before the application started, and block the
Qt event loop during dynamic D-Bus introspection. One carried-property snapshot
provides a consistent startup and update contract. Agent display prompts remain
distinct from delayed confirmation/authorization requests so QML never offers
accept/reject controls for a display-only passkey.

### AVRCP discovery consumes carried ObjectManager state

**Decision:** `BtAudioPlugin` subscribes first, enumerates BlueZ asynchronously
with a coalesced `GetManagedObjects`, and sends complete startup snapshots and
hot-plug interface payloads through one adoption contract for Device1 names,
A2DP transport activity, and the tracked AVRCP player.

**Rationale:** Synchronous interface construction and property reads can block
the Qt event loop and can race the ObjectManager event that prompted them.
BlueZ already carries the relevant properties in its snapshot and add signal;
an omitted value is safely unknown until later delivery. Treating player
removal as an authoritative stopped/unknown snapshot also prevents stale
playback, metadata, or progress from surviving a vanished player while keeping
all observable signals edge-only.

### HFP transport and call state require selected-phone evidence

**Decision:** The first selected PipeWire AudioGateway owns only the
`AudioGatewayTransport1` interface on that exact object path. Transport
properties and removals from other gateway objects are ignored. SCO may resolve
an existing setup/settle transition or confirm the loss of an Active call, but
cannot transition Idle to Active by itself. Call objects are accepted only
under the selected gateway's object path. A transport property invalidation
makes the cached value unknown; only explicit idle state or interface removal
is call-end evidence. Other discovered gateways remain cached so removal of the
selected phone can hand ownership to the next stable object-path candidate.

**Rationale:** PipeWire publishes one gateway object per connected phone, with
its transport interface co-located on that object. Accepting a second phone's
transport would let its idle state terminate the selected phone's call.
Separately, a running SCO node is audio-routing evidence, not sufficient proof
that a call exists; requiring setup/call evidence prevents stale or unrelated
nodes from creating a phantom call.

### Local-media restore state has explicit ownership

**Decision:** A saved local-media position belongs only to its exact saved
track. If that path is absent at boot, an available fallback is adopted paused
at zero while the raw exact-track restore remains pending. Play/pause, next,
previous, queue selection, and seek explicitly take transport ownership and
discard that pending restore. Shuffle/repeat persist at their control boundary;
a seek persists its bounded requested millisecond value rather than an
asynchronous player readback. Taking transport ownership also immediately
replaces the pending persisted restore with the visible (or empty) queue, so a
no-op edge action followed by a power cut cannot resurrect abandoned state.

**Rationale:** Boot-time USB mounting and Qt Multimedia delivery are both
asynchronous. Treating decode policy as restore ownership can permanently lose
a valid late-mount retry, while applying one track's saved time to another or
reading position immediately after a seek creates internally inconsistent
persistence. Explicit ownership makes late restoration possible until the user
acts and makes every power-cut-sensitive mutation durable at its source.

---

### Application-lifetime night state

**Decision:** `NightModeService` owns the configured time/GPIO provider for the
application lifetime and publishes one validity-gated physical state to both
`ThemeService` and Android Auto's persistent sensor cache.

**Rationale:** Projection sessions are consumers, not owners, of vehicle state.
Keeping the provider alive across reconnects prevents shell and phone state
from diverging, preserves the first subscription's cached indication, and lets
GPIO setup/read failures recover on a paced retry without replacing the last
authoritative value. If an exported GPIO directory disappears, the provider
re-enters export and direction setup instead of remaining stuck in its former
configured state. The shell-only force-dark override remains independent of
the physical state sent to Android Auto.

---

## Reference implementations consulted

- **f1xpl/openauto** `QtVideoOutput.cpp` — Qt 5 QMediaPlayer + SequentialBuffer approach
- **f1xpl/openauto** `OMXVideoOutput.cpp` — OpenMAX IL 4-component pipeline (decoder→scheduler→renderer+clock)
- **SonOfGib fork** — same architecture with threading fixes (`moveToThread` for Qt safety)
- **openDsh/dash** — modernized variant with similar patterns
- **uglyoldbob/android-auto** — Rust implementation, useful protocol reference for message IDs and flow
