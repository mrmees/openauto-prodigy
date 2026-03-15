# Design Decisions

Architectural rationale for OpenAuto Prodigy implementation choices. This documents *why* decisions were made, not just *what* was built.

---

## Task 5: Android Auto Service Layer

### Threading Model: 4 ASIO + Qt main

aasdk's promise-based async runs on Boost.ASIO `io_service`. Qt UI lives on the main thread. We bridge with `QMetaObject::invokeMethod(Qt::QueuedConnection)` for ASIO-to-Qt, and `strand_.dispatch()` for Qt-to-ASIO.

- 4 ASIO threads: matches original openauto, enough parallelism for 10 channels
- All QObject state mutations happen on Qt main thread via queued invocations
- Touch events flow from Qt main thread → ASIO strand via `strand_.dispatch()` in TouchHandler

### Transport: Wireless Only (TCP)

USB (AOAP) was abandoned after extensive testing showed that while the full handshake completes successfully, service channels **never open** over USB. The phone sends ServiceDiscoveryRequest, we respond, then silence — no data on any channel. This was consistent across code revisions and phone reboots.

Wireless AA works reliably on first connection: BT discovery → WiFi AP → TCP on port 5288. The TCP transport feeds the same messenger/entity stack that USB would have.

### aasdk fork: SonOfGib's

SonOfGib's fork fixes WiFi message routing (message ID `0x8001` properly handled per channel) and has various threading safety improvements over f1xpl's original.

---

## Task 6: Video Pipeline

### Video Decode: FFmpeg software over V4L2 hardware

**Decision:** Use `libavcodec` software H.264 decoder, not V4L2 `h264_v4l2m2m`.

**Rationale:**
- V4L2 `h264_v4l2m2m` has a known kernel 6.x regression ([raspberrypi/linux#6554](https://github.com/raspberrypi/linux/issues/6554), [jc-kynesim/rpi-ffmpeg#97](https://github.com/jc-kynesim/rpi-ffmpeg/issues/97)) — still unresolved as of Feb 2026
- Software H.264 decode benchmarks at ~243 fps for 720p on Pi 4's Cortex-A72 (NEON optimized). We need 30fps = ~12% of one core. Plenty of headroom.
- Hardware decode outputs `AV_PIX_FMT_DRM_PRIME` or `NV12` requiring extra format handling; software outputs `YUV420P` which maps directly to `QVideoFrameFormat::Format_YUV420P`
- Same code runs on Pi and dev VM — zero `#ifdef` platform-specific code
- Architecture isolates the decoder behind `VideoDecoder` class — swap in HW decode later when kernel bug is fixed, no other code changes needed

**Confirmed in practice:** First frame decoded at 1280x720, sustained 30fps with no dropped frames on Pi 4.

### Rendering: QVideoSink + VideoOutput over GStreamer/QMediaPlayer/custom OpenGL

**Decision:** Use `QVideoSink::setVideoFrame()` + QML `VideoOutput`.

**Rationale:**
- Canonical Qt 6 approach for custom video sources
- API stable across Qt 6.4 (dev VM) and 6.8 (Pi target)
- Qt RHI layer handles YUV→RGB conversion in GPU shaders — no manual pixel format conversion
- QMediaPlayer approach (piping H.264 into QIODevice buffer) was how original openauto did it on Qt 5, but unreliable in Qt 6 with FFmpeg backend — needs proper container framing that raw NAL units don't have
- GStreamer pipeline adds unnecessary complexity. Direct FFmpeg is ~50 lines of code.
- Custom QQuickItem with OpenGL texture gives maximum control + zero-copy DMA-BUF, but way more code. Only worth it if QVideoSink latency is unacceptable at 30fps (unlikely).

### NAL Framing: No start code prepending needed

**Decision:** Feed H.264 data directly to FFmpeg's `av_parser_parse2()` without modification.

**Rationale:**
- **Original assumption was wrong:** We initially believed aasdk delivered raw NAL units WITHOUT AnnexB start codes, and prepended `00 00 00 01` before each packet. This corrupted the bitstream.
- **Reality:** aasdk delivers data WITH AnnexB start codes already present. Confirmed by hex-dumping first bytes of video data: `00 00 00 01 67 42 80 1F` (SPS NAL with start code).
- FFmpeg's `av_parser_parse2()` handles the AnnexB stream directly.

### SPS/PPS Delivery: Two message types, both must be handled

**Decision:** Forward data from both `onAVMediaWithTimestampIndication` and `onAVMediaIndication` to the decoder.

**Rationale:**
- Android Auto sends SPS/PPS codec configuration as `AV_MEDIA_INDICATION` (message ID 0x0001, **no timestamp**)
- Regular video frames (IDR, P-frames) arrive as `AV_MEDIA_WITH_TIMESTAMP_INDICATION` (message ID 0x0000, 8-byte timestamp prefix)
- The original `onAVMediaIndication` handler discarded the data with a comment about it being "unlikely for video" — this was the root cause of the decoder failing with `non-existing PPS 0 referenced` on every frame
- Without SPS/PPS, the H.264 decoder has no codec parameters and cannot decode any frames
- This was confirmed by examining the raw wire payloads: first video message after channel open is always SPS+PPS via message ID 0x0001

### Resolution: 720p

**Decision:** Offer 720p (1280x720) in service discovery.

**Rationale:**
- DFRobot screen is 1024x600 — the phone renders at 1280x720 and Qt scales to fit via `VideoOutput.Stretch`
- Phone chose 1280x720 automatically when offered
- 720p software decode is well within Pi 4 budget (~12% of one core)
- Higher resolution means more detail even at slightly different aspect ratio

### OpenMAX IL: Not an option

Original openauto used OMX via `ilclient` for zero-copy GPU decode — fastest path on Pi 3. OMX and `bcm_host.h` are completely removed in Trixie/Bookworm. Dead end.

### Threading: Decode on Qt main thread

**Decision:** Marshal H.264 data from ASIO thread to Qt main thread for decoding.

**Rationale:**
- Video data arrives on ASIO worker thread via `onAVMediaWithTimestampIndication()` and `onAVMediaIndication()`
- `QVideoFrame`/`QVideoSink` aren't thread-safe — must be used from the thread owning the sink
- 720p30 software decode is fast enough for main thread (~3ms per frame)
- Bridge via `QMetaObject::invokeMethod(Qt::QueuedConnection)` — same proven pattern used for connection state updates

---

## Touch Input

### Multi-touch via MultiPointTouchArea

**Decision:** Use QML `MultiPointTouchArea` instead of `MouseArea` for touch input.

**Rationale:**
- Android Auto supports multi-touch (pinch-to-zoom in maps, etc.)
- `MouseArea` only supports single touch point
- `MultiPointTouchArea` provides per-finger `pointId` which maps to AA's `pointer_id` in `TouchLocation`
- Touch coordinates mapped from screen space (QML item dimensions) to AA touch space (1024x600 as declared in `touch_screen_config`)

### TouchHandler: QObject bridge pattern

**Decision:** Separate `TouchHandler` QObject that QML calls via `Q_INVOKABLE` methods.

**Rationale:**
- QML needs to call into the AA Input service channel, which lives on ASIO strand
- TouchHandler holds a reference to `InputServiceChannel` and the ASIO strand
- `Q_INVOKABLE` methods dispatch to the strand for thread-safe protobuf construction and sending
- Exposed to QML as a context property (`TouchHandler`)
- Wired to the InputServiceChannel by ServiceFactory during service creation

### Touch debug overlay

**Decision:** Add an optional visual overlay showing touch points with coordinates.

**Rationale:**
- First hardware test revealed touch was registering but may need calibration
- Overlay shows green crosshair circles at each touch point with AA-space coordinates displayed
- Essential for diagnosing coordinate mapping issues without printf debugging
- Uses a `ListModel` + `Repeater` pattern to track active touch points

---

## Fullscreen

### Window.FullScreen for true fullscreen

**Decision:** Toggle `Window.visibility` between `Window.FullScreen` and `Window.Windowed` based on AA connection state.

**Rationale:**
- Simply hiding TopBar/BottomBar within the app still leaves the labwc taskbar and window decorations visible
- `Window.FullScreen` tells the Wayland compositor to give us the entire screen
- Toggled automatically: fullscreen when AA is connected (state 3) and current app is AndroidAuto, windowed otherwise
- TopBar/BottomBar also hidden (Layout.preferredHeight: 0) to reclaim internal layout space

---

## v0.6-v0.6.1: Widget Framework and Grid Architecture

### Grid Sizing: DPI-based cellSide Formula

**Decision:** `cellSide = diagPx / (9.0 + bias * 0.8)` where `diagPx` is the pixel diagonal and `bias` is a user-adjustable density parameter.

**Rationale:** The grid needs to be resolution-independent. A fixed pixel cell size breaks on different displays; a fixed cell count wastes space on large screens and cramps small ones. The diagonal-based divisor naturally scales: a 1024x600 display gets ~7-8 columns while a 1920x1080 display gets proportionally more. The DPI cascade in `DisplayInfo::updateCellSide()` validates screen DPI trustworthiness but currently all paths produce the same formula — this is structural scaffolding for a future mm-based targeting path where physical DPI would produce different output. Constants `kBaseDivisor = 9.0` and `kBiasStep = 0.8` are in `DisplayInfo.hpp`.

### Grid Cols/Rows Computed in QML

**Decision:** Grid columns and rows are computed as reactive QML bindings from `DisplayInfo.cellSide`. C++ computes the initial grid dimensions at startup (`main.cpp` line 560) for widget seeding and model initialization, then QML takes over the reactive path.

**Rationale:** QML bindings automatically re-evaluate when the window resizes, density bias changes, or any input to the formula changes. The C++ startup computation is needed before the QML engine is running to seed initial placements. After startup, the QML approach in `HomeMenu.qml` (lines 25-55) keeps the grid layout fully reactive with zero imperative code.

### Auto-Snap Threshold for Gutter Space Recovery

**Decision:** Two-pass snap with 50% waste threshold (`kSnapThreshold: 0.5`) against `baseCellSide`, with a cascade guard preventing runaway iteration.

**Rationale:** When the computed grid has significant wasted space (gutter), adding one more column or row recovers it at the cost of slightly smaller cells. Pass 1 snaps axes where waste exceeds 50% of the base cell size. Pass 2 catches cascaded waste from Pass 1's cell shrinkage, but only on axes that did NOT snap in Pass 1. This prevents iterative packing (e.g., 7x4 -> 8x5 -> 9x5). The single re-evaluation is sufficient because the cell shrinkage from one additional row/column is bounded. Implementation is in `HomeMenu.qml` lines 26-50.

### Dock Replaced by Singleton Widgets

**Decision:** Replace the z=10 overlay dock (which sat outside the ColumnLayout) with singleton launcher widgets on a reserved page.

**Rationale:** The dock consumed fixed vertical space that could not be reclaimed by the grid. Singleton widgets (e.g., `org.openauto.settings-launcher`, `org.openauto.aa-launcher`) participate in the same grid model as regular widgets, unifying the layout system. They are marked `singleton = true` — system-seeded, non-removable, and hidden from the picker. The reserved page is derived from singleton presence, not stored as explicit state.

### JS-Based Category Filtering in QML

**Decision:** Widget picker categories are filtered using JavaScript array operations in QML, not a C++ proxy model.

**Rationale:** The widget catalog is small (6 widgets currently). A C++ `QSortFilterProxyModel` would add a class, header, registration, and maintenance for no performance benefit. The JS filtering in the QML Repeater handles the current scale and is trivially understandable. If the catalog grows to hundreds of widgets, a C++ model would be warranted.

### Category Order Hardcoded

**Decision:** Category display order is a static map: status=0, media=1, navigation=2, launcher=3.

**Rationale:** Four categories with fixed semantics do not need a dynamic ordering system. The hardcoded map in the picker is simple, matches the spec, and has no extensibility requirement. If user-defined categories are ever added, this becomes a data-driven lookup.

### Remap Clamps Oversized Spans

**Decision:** When grid dimensions change (display resize, density bias), widgets whose spans exceed the new grid bounds are clamped to fit rather than hidden.

**Rationale:** Hiding widgets would make them disappear silently — users would think they were deleted. Clamping preserves the widget at a reduced size, maximizing visibility. The original span is stored in `basePlacements_` and restored if the grid grows back.

### promoteToBase() on Every Mutation

**Decision:** After every user edit (move, resize, add, remove, opacity change), `promoteToBase()` copies `livePlacements_` to `basePlacements_`.

**Rationale:** Remap derives its output from `basePlacements_`, so base must always reflect the latest user intent. Without this, a remap triggered by a resize would revert to an older base state, discarding the user's recent edits. The copy is cheap (small QList of POD-like structs).

### Base/Live Snapshot Pattern

**Decision:** Two parallel placement lists: `basePlacements_` (persisted from YAML, updated by promoteToBase) and `livePlacements_` (runtime state exposed to QML).

**Rationale:** Clean separation of persisted vs runtime state. Remap reads from base and writes to live, ensuring the remap algorithm always has a stable input. Structural edits (move, resize, add, remove) update live first, then `promoteToBase()` copies live back to base so future remaps reflect the latest user intent. YAML serialization reads from live. The QAbstractListModel interface exposes live placements.

### Reserved Page Derived from Singleton Presence

**Decision:** Whether a page is "reserved" (protected from deletion) is derived from `pageHasSingleton()` at runtime, not stored as an explicit page flag.

**Rationale:** Derived state is more robust than stored state. If a singleton widget is moved to a different page, the reservation follows it automatically. A stored flag would require synchronization logic to track widget moves.

### Fixed instanceIds for Seeded Singletons

**Decision:** System-seeded singleton widgets get deterministic instance IDs like `"aa-launcher-reserved"` and `"settings-launcher-reserved"`.

**Rationale:** Seeding only runs when `savedPlacements.isEmpty()` (fresh install, `main.cpp` line 593), so duplicates are prevented by the empty-check gate, not by ID-based deduplication. The fixed IDs serve a different purpose: they make the seeded placements predictable and debuggable, and ensure consistency across fresh installs. YAML load/save does not deduplicate by instance ID.

### Clock as Active Page Indicator

**Decision:** The navbar clock control shows page indicator dots: `leftDotCount = activePage`, `rightDotCount = pageCount - activePage - 1`.

**Rationale:** Repurposes an existing navbar element instead of adding a separate page indicator UI. The dot pattern is intuitive (dots to the left = pages before, dots to the right = pages after). Implementation is in `NavbarControl.qml` lines 18-19.

### WidgetContextFactory as Dedicated Class

**Decision:** Context creation for widget instances is handled by `WidgetContextFactory`, a separate QObject, not by methods on `WidgetGridModel`.

**Rationale:** Keeps the model as pure data (placements, roles, CRUD). The factory owns the `IHostContext` reference and cell geometry needed to construct `WidgetInstanceContext` instances. This separation prevents the model from accumulating service dependencies. The factory's `cellSide` Q_PROPERTY provides the initial cell dimensions at context creation time. Ongoing reactivity comes from `HomeMenu.qml` `Binding` elements that update `cellWidth`, `cellHeight`, `colSpan`, `rowSpan`, and `isCurrentPage` directly on each `WidgetInstanceContext` instance (HomeMenu.qml lines 245-251).

### Context Injection via Loader.onLoaded + Binding

**Decision:** Widget context is injected into QML via `Loader.onLoaded` with `Binding` elements for reactive properties, not via QML context properties.

**Rationale:** Context properties are untyped, undocumented, and invisible to tooling. The `property QtObject widgetContext: null` declaration in widget QML is typed, documented, and supports NOTIFY. `Binding` elements for `colSpan`, `rowSpan`, `cellWidth`, `cellHeight`, and `isCurrentPage` keep values reactive without imperative update code. The `when: widgetCtx !== null` guard prevents binding errors during the brief Loader initialization window.

### NowPlayingWidget Controls as Provider Methods

**Decision:** Media transport controls (play/pause, next, previous) are invoked as typed methods on `IMediaStatusProvider`, not dispatched through `ActionRegistry`.

**Rationale:** Transport controls are media-specific operations that only make sense in the context of an active media source. `ActionRegistry` is for general-purpose application commands (`app.home`, `theme.toggle`). Making transport controls into registry actions would require the caller to know which media source is active and route accordingly — logic that `MediaStatusService` already encapsulates.

---

## Reference implementations consulted

- **f1xpl/openauto** `QtVideoOutput.cpp` — Qt 5 QMediaPlayer + SequentialBuffer approach
- **f1xpl/openauto** `OMXVideoOutput.cpp` — OpenMAX IL 4-component pipeline (decoder→scheduler→renderer+clock)
- **SonOfGib fork** — same architecture with threading fixes (`moveToThread` for Qt safety)
- **openDsh/dash** — modernized variant with similar patterns
- **uglyoldbob/android-auto** — Rust implementation, useful protocol reference for message IDs and flow
