# Design Decisions

Architectural rationale for OpenAuto Prodigy implementation choices. This documents *why* decisions were made, not just *what* was built.

---

## Task 5: Android Auto Service Layer

### Threading Model: 4 ASIO + 2 libusb + Qt main

aasdk's promise-based async runs on Boost.ASIO `io_service`. Qt UI lives on the main thread. We bridge with `QMetaObject::invokeMethod(Qt::QueuedConnection)` for ASIO-to-Qt, and `strand_.dispatch()` for Qt-to-ASIO.

- 4 ASIO threads: matches original openauto, enough parallelism for 9 channels
- 2 libusb event threads: handles USB hotplug and transfers
- All QObject state mutations happen on Qt main thread via queued invocations

### Transport: USB + TCP dual-mode

USB (AOAP) for wired connections, TCP port 5277 for wireless AA. Both produce an `ITransport` that feeds the same messenger/entity stack. TCP listener runs alongside USB hub — first connection wins.

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
- Same code runs on Pi and WSL2 dev build — zero `#ifdef` platform-specific code
- Architecture isolates the decoder behind `VideoDecoder` class — swap in HW decode later when kernel bug is fixed, no other code changes needed

### Rendering: QVideoSink + VideoOutput over GStreamer/QMediaPlayer/custom OpenGL

**Decision:** Use `QVideoSink::setVideoFrame()` + QML `VideoOutput`.

**Rationale:**
- Canonical Qt 6 approach for custom video sources
- API stable across Qt 6.4 (WSL2 dev) and 6.8 (Pi target)
- Qt RHI layer handles YUV→RGB conversion in GPU shaders — no manual pixel format conversion
- QMediaPlayer approach (piping H.264 into QIODevice buffer) was how original openauto did it on Qt 5, but unreliable in Qt 6 with FFmpeg backend — needs proper container framing that raw NAL units don't have
- GStreamer pipeline (`appsrc ! h264parse ! v4l2h264dec ! appsink`) adds unnecessary complexity — pipeline state machine, caps negotiation, additional dependency. Direct FFmpeg is ~50 lines of code.
- Custom QQuickItem with OpenGL texture (what Moonlight-Qt does) gives maximum control + zero-copy DMA-BUF, but way more code. Only worth it if QVideoSink latency is unacceptable at 30fps (unlikely).

### NAL Framing: Prepend AnnexB start codes

**Decision:** Prepend `00 00 00 01` to each NAL unit before feeding to FFmpeg.

**Rationale:**
- aasdk delivers raw H.264 NAL units WITHOUT AnnexB start codes — confirmed by examining `VideoServiceChannel.cpp` message extraction
- FFmpeg's `av_parser_parse2()` expects AnnexB format
- Parser handles reassembly into complete access units (SPS/PPS + IDR + slices)

### Threading: Decode on Qt main thread

**Decision:** Marshal H.264 data from ASIO thread to Qt main thread for decoding.

**Rationale:**
- Video data arrives on ASIO worker thread via `onAVMediaWithTimestampIndication()`
- `QVideoFrame`/`QVideoSink` aren't thread-safe — must be used from the thread owning the sink
- 720p30 software decode is fast enough for main thread (~3ms per frame)
- Bridge via `QMetaObject::invokeMethod(Qt::QueuedConnection)` — same proven pattern used for connection state updates

### Resolution: 480p default

**Decision:** Start with 480p (800x480).

**Rationale:**
- DFRobot screen is 1024x600 — closest standard AA resolution is 480p (800x480)
- 720p (1280x720) also works, phone will downscale to fit, but more decode overhead
- Can bump to 720p trivially by changing the VideoResolution enum in service discovery

### OpenMAX IL: Not an option

Original openauto used OMX via `ilclient` for zero-copy GPU decode — fastest path on Pi 3. OMX and `bcm_host.h` are completely removed in Trixie/Bookworm. Dead end.

### Reference implementations consulted

- **f1xpl/openauto** `QtVideoOutput.cpp` — Qt 5 QMediaPlayer + SequentialBuffer approach
- **f1xpl/openauto** `OMXVideoOutput.cpp` — OpenMAX IL 4-component pipeline (decoder→scheduler→renderer+clock)
- **SonOfGib fork** — same architecture with threading fixes (`moveToThread` for Qt safety)
- **openDsh/dash** — modernized variant with similar patterns
