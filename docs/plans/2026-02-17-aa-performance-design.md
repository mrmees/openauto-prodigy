# Android Auto Wireless Performance Optimization — Design
> **Status:** COMPLETED

**Goal:** Make AA video streaming and touch input feel as responsive as a commercial head unit. Measure first, fix in priority order, stop when good enough.

**Approach:** Incremental, data-driven. Instrument the hot paths, fix the biggest bottlenecks, re-measure after each change.

---

## Current Bottlenecks (identified by code review)

### Video Pipeline

1. **Per-frame QVideoFrame allocation** — `new QVideoFrame(format)` + `QVideoFrameFormat` constructor on every frame (30x/sec).
2. **Line-by-line YUV plane copy** — Three nested loops copying Y/U/V planes one scanline at a time. At 720p YUV420P, that's ~1.38 MB/frame = ~41 MB/s of memcpy, all in small chunks that prevent CPU/memory optimization.
3. **Two deep copies of H.264 data** — `QByteArray(ptr, size)` deep-copies on the ASIO thread, then `Qt::QueuedConnection` copies again when marshaling to the main thread.
4. **Decode blocks main thread** — `decodeFrame()` runs on Qt main thread. ~3-5ms of decode + copy per frame blocks touch event processing and QML rendering. At 30fps that's ~150ms/sec of main thread time consumed.

### Touch Pipeline

5. **Per-finger protobuf messages** — Each touch point gets its own `InputEventIndication`, `strand_.dispatch()`, `SendPromise::defer()`, and network send. AA protocol supports multiple `touch_location` entries per message.
6. **No batching** — Multi-touch events (pinch-to-zoom) send N messages where 1 would do.

---

## Phase A: Instrumentation

Add lightweight timing to both hot paths. No architectural changes.

### Video Timing Points

Measured inside `VideoDecoder`:

| Checkpoint | Location | What it measures |
|-----------|----------|-----------------|
| `t_enqueue` | Signal emit in `VideoService` | When H.264 data leaves ASIO thread |
| `t_decode_start` | Top of `decodeFrame()` | When main/worker thread picks it up (queue delay) |
| `t_decode_done` | After `avcodec_receive_frame` | FFmpeg decode time |
| `t_copy_done` | After YUV plane copy | Frame copy time |
| `t_display` | After `setVideoFrame` | Total pipeline latency |

### Touch Timing Points

| Checkpoint | Location | What it measures |
|-----------|----------|-----------------|
| `t_qml` | Captured in QML, passed to C++ | When the touch event fired |
| `t_dispatch` | Inside strand dispatch lambda | QML→ASIO crossing time |
| `t_send` | After `sendInputEventIndication` | Protobuf build + send time |

### Output Format

Rolling stats logged every 5 seconds:

```
[Perf] Video: queue=2.1ms decode=3.4ms copy=1.8ms total=7.3ms (p99=12.1ms) | 30.0 fps
[Perf] Touch: qml→dispatch=1.2ms send=0.3ms total=1.5ms (events/sec=47)
```

Implementation: a small stats struct with running min/max/sum/count, reset every log interval. No heap allocations, no threads, no files. Just `steady_clock::now()` at each checkpoint.

---

## Phase B: Optimizations (in priority order)

### B1: Video Frame Copy Elimination

**Files:** `VideoDecoder.hpp`, `VideoDecoder.cpp`

Changes:
- **Cache `QVideoFrameFormat`** — Create once on first decoded frame, store as member. Reuse for all subsequent frames.
- **Stride-aware bulk copy** — Check if FFmpeg `linesize` == QVideoFrame `bytesPerLine`. If equal (common for standard resolutions), do one `memcpy` per plane instead of per-line. Fall back to line-by-line only when strides differ.
- **Double-buffered QVideoFrame** — Pre-allocate two `QVideoFrame` objects, alternate between them. Eliminates heap allocation per frame. Qt's internal refcounting keeps the previous frame valid while GPU reads it.

**Expected impact:** Eliminate ~30 heap allocations/sec, reduce memcpy overhead significantly for matching-stride case.

**Risk:** Low. Purely internal to VideoDecoder, no API changes.

### B2: Decode Off Main Thread

**Files:** `VideoDecoder.hpp`, `VideoDecoder.cpp`, `VideoService.cpp`

Changes:
- New `QThread`-based decode worker owned by `VideoDecoder`
- H.264 data pushed to `QMutex`-protected queue (one producer, one consumer, ~30-60 enqueues/sec — lock-free not needed)
- Worker thread runs: parse → decode → copy into QVideoFrame
- Only `videoSink_->setVideoFrame()` is invoked on Qt main thread via `QMetaObject::invokeMethod(Qt::QueuedConnection)`
- `VideoService` signal connects to worker thread instead of main thread

**Expected impact:** Free main thread from ~150ms/sec of decode work. Touch events and QML rendering no longer queue behind video decode.

**Risk:** Moderate. Adds a thread with a clean boundary (bytes in, QVideoFrame out). Shared state limited to the frame queue and video sink pointer.

### B3: Touch Event Batching

**Files:** `TouchHandler.hpp`, `TouchHandler.cpp`, `AndroidAutoMenu.qml`

Changes:
- New `Q_INVOKABLE sendBatchTouchEvent(QVariantList points, int action)` on `TouchHandler`
- QML callbacks pass all touch points from a single event as one list
- Single `InputEventIndication` protobuf with multiple `touch_location` entries
- One `strand_.dispatch()` per QML callback instead of per finger
- One `SendPromise` per callback instead of per finger

**Expected impact:** Halve protobuf allocations, strand dispatches, and network sends during multi-touch. Matches AA protocol spec better.

**Risk:** Low. Protocol explicitly supports multiple touch locations per message.

### B4: Configurable Video FPS

**Files:** `Configuration.hpp`, `Configuration.cpp`, `VideoService.cpp`

Changes:
- New INI setting: `[Video] fps=60` (default 60, valid values 30 and 60)
- `VideoService::fillFeatures()` reads config and sets `VideoFPS::_60` or `VideoFPS::_30`
- AA protocol supports both (`VideoFPSEnum.proto`: `_60 = 1`, `_30 = 2`)

**Expected impact:** 60fps video is the single most visible smoothness improvement. Pi 4 can handle 720p60 in software (~24% of one core).

**Risk:** Low. One-line change in service discovery + config plumbing.

---

## Phase C: Review Against Targets

### Performance Targets

| Metric | Target | Current estimate |
|--------|--------|-----------------|
| Video frame latency (receive → display) | < 10ms p99 | ~7-12ms (unmeasured) |
| Touch latency (QML → protobuf sent) | < 5ms p99 | ~2-5ms (unmeasured) |
| Main thread blocked by decode | < 20ms/sec | ~150ms/sec |
| Dropped frames | 0 at configured FPS | Unknown |

### Decision Criteria

- **Hit targets:** Ship it, stop optimizing.
- **Miss targets:** Instrumentation shows exactly where time is spent. Consider advanced techniques (lock-free queues, zero-copy DMA-BUF frame handoff, pre-allocated protobuf pools) only where data justifies it.

### Permanently Retained

- Instrumentation logging (can be toggled via log level)
- Touch debug overlay (already visual, consider adding config toggle)

---

## Branch Strategy

Feature branch `perf/aa-streaming` off current `main`. Commits per optimization step. Merge back when Phase B complete and numbers are acceptable.

---

## Out of Scope

- Audio pipeline optimization (audio isn't implemented yet)
- Network-level TCP tuning (unlikely bottleneck for local WiFi)
- H.264 hardware decode (kernel regression still unresolved)
- Zero-copy DMA-BUF frame handoff (Phase C candidate if needed)
