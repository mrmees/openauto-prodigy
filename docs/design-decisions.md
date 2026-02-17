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

## Reference implementations consulted

- **f1xpl/openauto** `QtVideoOutput.cpp` — Qt 5 QMediaPlayer + SequentialBuffer approach
- **f1xpl/openauto** `OMXVideoOutput.cpp` — OpenMAX IL 4-component pipeline (decoder→scheduler→renderer+clock)
- **SonOfGib fork** — same architecture with threading fixes (`moveToThread` for Qt safety)
- **openDsh/dash** — modernized variant with similar patterns
- **uglyoldbob/android-auto** — Rust implementation, useful protocol reference for message IDs and flow
