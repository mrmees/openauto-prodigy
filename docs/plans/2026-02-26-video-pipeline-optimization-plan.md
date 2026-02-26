# Video Pipeline Optimization Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Minimize video decode latency, CPU usage, and memory overhead on Pi 4, with portable foundations for future SBCs.

**Architecture:** Seven layered improvements to the decode-to-display pipeline. Each task is independently testable except Task 5 (DRM hwaccel + DmaBuf) which lands as one unit. Priority order: fix metrics first, then reduce copies, bound latency, wire hardware decode, polish allocation.

**Tech Stack:** FFmpeg 7.x (DRM hwaccel, v4l2m2m), Qt 6.8 (QAbstractVideoBuffer, RHI), C++17

**Baseline:** 1080p H.265 software decode on Pi 4: 125% CPU, 22-29ms/frame decode, 4.2ms memcpy, 28-33fps. See `docs/baselines/2026-02-26-video-pipeline-baseline.md`.

---

### Task 1: Fix Timestamp Semantics

**Files:**
- Modify: `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp:186-193`
- Modify: `libs/open-androidauto/include/oaa/HU/Handlers/VideoChannelHandler.hpp:31`
- Modify: `src/core/aa/VideoDecoder.cpp:264,329,447-452,461-465`
- Modify: `src/core/aa/VideoDecoder.hpp:61`
- Test: `tests/test_video_channel_handler.cpp`

**Context:** Currently `onMediaData` passes the AA protocol timestamp (phone clock, microseconds since session start) through the signal as `uint64_t timestamp`. `VideoDecoder::decodeFrame` receives this as `qint64 enqueueTimeNs` and uses it for queue wait metrics. The result is nonsense values (~101 billion ms). The fix: stamp with `steady_clock::now()` at emit time. Preserve the AA timestamp separately if needed later for A/V sync.

**Step 1: Update VideoChannelHandler signal to carry local timestamp**

In `VideoChannelHandler.cpp:186-193`, change `onMediaData` to stamp with local clock:

```cpp
void VideoChannelHandler::onMediaData(const QByteArray& data, uint64_t timestamp)
{
    if (!channelOpen_ || !streaming_)
        return;

    auto now = std::chrono::steady_clock::now();
    qint64 enqueueNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    emit videoFrameData(data, enqueueNs);
    sendAck();
}
```

Update the signal signature in `VideoChannelHandler.hpp:31`:
```cpp
void videoFrameData(const QByteArray& data, qint64 enqueueTimeNs);
```

Add `#include <chrono>` to the `.cpp` file.

**Step 2: Update DecodeWorker::WorkItem to use qint64**

In `VideoDecoder.hpp:61`, the WorkItem is already `qint64 enqueueTimeNs` — no change needed there. But verify the signal-slot connection in `AndroidAutoOrchestrator.cpp:264-265` still works with the new signal signature (`qint64` instead of `uint64_t`).

**Step 3: Update test**

In `tests/test_video_channel_handler.cpp`, the `testMediaDataEmitsFrameAndAck` test (around line 58-83) checks the signal emission. Update to verify the timestamp is a reasonable `steady_clock` value (> 0, < some large bound) rather than comparing to the protocol timestamp.

**Step 4: Build and test**

```bash
cd build && cmake --build . -j$(nproc) && ctest -R "test_video_channel_handler" --output-on-failure
```

**Step 5: Cross-compile, deploy to Pi, verify perf log shows sane queue times**

```bash
docker run --rm -u "$(id -u):$(id -g)" -v "$(pwd):/src:rw" -w /src/build-pi openauto-cross-pi4 cmake --build . -j$(nproc)
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
```

Restart app, play media, check `[Perf] Video:` log lines — queue time should now be 0-5ms instead of ~101 billion ms.

**Step 6: Commit**

```bash
git add libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp \
        libs/open-androidauto/include/oaa/HU/Handlers/VideoChannelHandler.hpp \
        tests/test_video_channel_handler.cpp
git commit -m "fix: use steady_clock for video frame timestamps instead of AA protocol clock"
```

---

### Task 2: Zero-Copy Payload Forwarding

**Files:**
- Modify: `libs/open-androidauto/include/oaa/HU/Handlers/VideoChannelHandler.hpp:31`
- Modify: `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp:186-193`
- Modify: `src/core/aa/VideoDecoder.hpp:40,61`
- Modify: `src/core/aa/VideoDecoder.cpp:233-237,490-495,515-517`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp:264-265`
- Test: `tests/test_video_channel_handler.cpp`

**Context:** The `Qt::QueuedConnection` at `AndroidAutoOrchestrator.cpp:264` deep-copies the `QByteArray` payload when crossing from the protocol thread to the decode worker thread. At 1080p, keyframes are 50-150KB. Fix: use `std::shared_ptr<const QByteArray>` and a direct connection to the worker's thread-safe `enqueue()`.

**Step 1: Change signal to carry shared_ptr**

In `VideoChannelHandler.hpp:31`:
```cpp
void videoFrameData(std::shared_ptr<const QByteArray> data, qint64 enqueueTimeNs);
```

In `VideoChannelHandler.cpp` `onMediaData`:
```cpp
auto shared = std::make_shared<const QByteArray>(std::move(data));
emit videoFrameData(shared, enqueueNs);
```

Note: `onMediaData` receives `const QByteArray& data` from the protocol layer. We must copy once into the shared_ptr — but that's the *only* copy. The QueuedConnection previously made a second copy.

Register the metatype in the `.cpp` file:
```cpp
Q_DECLARE_METATYPE(std::shared_ptr<const QByteArray>)
```

**Step 2: Update VideoDecoder to accept shared_ptr**

In `VideoDecoder.hpp:61`, change WorkItem:
```cpp
struct WorkItem { std::shared_ptr<const QByteArray> data; qint64 enqueueTimeNs; };
```

Update `decodeFrame` slot (line 40) to accept `std::shared_ptr<const QByteArray>`.

In `VideoDecoder.cpp:233-237` (`decodeFrame`):
```cpp
void VideoDecoder::decodeFrame(std::shared_ptr<const QByteArray> h264Data, qint64 enqueueTimeNs)
{
    worker_->enqueue(std::move(h264Data), enqueueTimeNs);
}
```

In `DecodeWorker::enqueue` (line 490-495):
```cpp
void DecodeWorker::enqueue(std::shared_ptr<const QByteArray> data, qint64 enqueueTimeNs)
```

In `DecodeWorker::run` (line 517), dereference the shared_ptr when calling processFrame:
```cpp
decoder_->processFrame(*item.data, item.enqueueTimeNs);
```

**Step 3: Update AndroidAutoOrchestrator connection**

In `AndroidAutoOrchestrator.cpp:264-265`, keep `Qt::QueuedConnection` — but now the QByteArray is wrapped in a shared_ptr, so Qt copies the shared_ptr (cheap refcount bump) instead of the full payload.

**Step 4: Update tests**

In `tests/test_video_channel_handler.cpp`, the signal spy for `videoFrameData` needs to use the new signature. Update assertions to dereference the shared_ptr.

**Step 5: Build and test**

```bash
cd build && cmake --build . -j$(nproc) && ctest -R "test_video" --output-on-failure
```

**Step 6: Commit**

```bash
git commit -m "perf: zero-copy video payload forwarding via shared_ptr"
```

---

### Task 3: Bounded Decode Queue with Keyframe Awareness

**Files:**
- Modify: `src/core/aa/VideoDecoder.hpp:51-64`
- Modify: `src/core/aa/VideoDecoder.cpp:490-519`
- Create: `tests/test_video_decode_queue.cpp`
- Modify: `tests/CMakeLists.txt`

**Context:** The decode queue (`std::queue<WorkItem>` in DecodeWorker) is unbounded. If decode falls behind, frames accumulate and latency grows without limit. Fix: cap at 2 frames, drop oldest non-keyframe on overflow. If forced to drop a keyframe, set `awaitingKeyframe_` flag and discard until next IDR. Always continue sending ACKs for dropped frames.

**Step 1: Add keyframe detection helper**

In `VideoDecoder.cpp`, add a static helper:
```cpp
// Detect IDR/keyframe from Annex B NAL header
static bool isKeyframe(const QByteArray& data)
{
    // Find first NAL unit start code (0x00000001 or 0x000001)
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data.constData());
    int len = data.size();
    for (int i = 0; i + 4 < len; ++i) {
        if (p[i] == 0 && p[i+1] == 0) {
            int nalStart = -1;
            if (p[i+2] == 1) nalStart = i + 3;
            else if (p[i+2] == 0 && i + 4 < len && p[i+3] == 1) nalStart = i + 4;
            if (nalStart >= 0 && nalStart < len) {
                uint8_t nalType;
                uint8_t firstByte = p[nalStart];
                if ((firstByte & 0x80) == 0) {
                    // H.264: NAL type is lower 5 bits. IDR = 5, SPS = 7, PPS = 8
                    nalType = firstByte & 0x1F;
                    return nalType == 5 || nalType == 7 || nalType == 8;
                } else {
                    // H.265: NAL type is bits 1-6 of first byte. IDR_W_RADL=19, IDR_N_LP=20, VPS=32, SPS=33, PPS=34
                    nalType = (firstByte >> 1) & 0x3F;
                    return nalType == 19 || nalType == 20 || nalType == 32 || nalType == 33 || nalType == 34;
                }
            }
        }
    }
    return false; // Assume non-keyframe if we can't detect
}
```

**Step 2: Add queue management to DecodeWorker**

In `VideoDecoder.hpp`, update DecodeWorker members:
```cpp
static constexpr int MAX_QUEUE_SIZE = 2;
bool awaitingKeyframe_ = false;
uint32_t droppedFrames_ = 0;
```

In `DecodeWorker::enqueue`:
```cpp
void DecodeWorker::enqueue(std::shared_ptr<const QByteArray> data, qint64 enqueueTimeNs)
{
    QMutexLocker lock(&mutex_);

    bool keyframe = isKeyframe(*data);

    // If awaiting keyframe, drop non-keyframes
    if (awaitingKeyframe_) {
        if (!keyframe) {
            ++droppedFrames_;
            cond_.wakeOne();
            return;
        }
        awaitingKeyframe_ = false;
    }

    // Bound queue size
    while (queue_.size() >= MAX_QUEUE_SIZE) {
        // Drop oldest non-keyframe if possible
        // Simple policy: just drop front (oldest)
        queue_.pop();
        ++droppedFrames_;
    }

    queue_.push({std::move(data), enqueueTimeNs, keyframe});
    cond_.wakeOne();
}
```

Update WorkItem to include keyframe flag:
```cpp
struct WorkItem {
    std::shared_ptr<const QByteArray> data;
    qint64 enqueueTimeNs;
    bool isKeyframe;
};
```

In `DecodeWorker::run`, after dropping a keyframe (if that happens in the queue overflow path), call `avcodec_flush_buffers(decoder_->codecCtx_)` to avoid corrupted references. Add a method `VideoDecoder::flushDecoder()` for this.

**Step 3: Write test**

Create `tests/test_video_decode_queue.cpp` — test the queue bounding behavior:
- Enqueue 5 frames in a paused worker → only 2 remain
- Enqueue with `awaitingKeyframe` active → non-keyframes dropped
- Keyframe while `awaitingKeyframe` → queue accepts it

Add to `tests/CMakeLists.txt`.

**Step 4: Build and test**

```bash
cd build && cmake --build . -j$(nproc) && ctest -R "test_video_decode_queue" --output-on-failure
```

**Step 5: Commit**

```bash
git commit -m "perf: bounded decode queue with keyframe-aware frame dropping"
```

---

### Task 4: Latest-Frame-Wins Display Delivery

**Files:**
- Modify: `src/core/aa/VideoDecoder.cpp:340-366,438-452`
- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`

**Context:** Currently every decoded frame is posted to the Qt main thread via `QMetaObject::invokeMethod(Qt::QueuedConnection)`. If the main thread is busy, frames queue up in the event loop. Fix: write to a mutex-guarded latest-frame slot. A timer on the main thread reads the slot at ~60Hz.

**Step 1: Add latest-frame slot to VideoDecoder**

In `VideoDecoder.hpp`, add members:
```cpp
std::mutex latestFrameMutex_;
QVideoFrame latestFrame_;
std::atomic<bool> hasLatestFrame_{false};
```

**Step 2: Replace invokeMethod with slot write**

In `VideoDecoder.cpp`, both the DRM_PRIME path (lines 351-354) and the SW path (lines 438-441), replace `QMetaObject::invokeMethod` with:
```cpp
{
    std::lock_guard<std::mutex> lock(latestFrameMutex_);
    latestFrame_ = std::move(videoFrame);
}
hasLatestFrame_.store(true, std::memory_order_release);
```

**Step 3: Add display tick in AndroidAutoOrchestrator**

In `AndroidAutoOrchestrator.cpp`, create a `QTimer` that fires at ~60Hz (16ms). On tick:
```cpp
if (videoDecoder_.hasLatestFrame_.exchange(false, std::memory_order_acq_rel)) {
    QVideoFrame frame;
    {
        std::lock_guard<std::mutex> lock(videoDecoder_.latestFrameMutex_);
        frame = std::move(videoDecoder_.latestFrame_);
    }
    if (videoSink_)
        videoSink_->setVideoFrame(frame);
}
```

Or alternatively, expose a `VideoDecoder::takeLatestFrame()` method that encapsulates the lock.

**Step 4: Build, cross-compile, test on Pi**

Verify video still renders smoothly. Check that stale frames don't accumulate by comparing perf metrics before/after.

**Step 5: Commit**

```bash
git commit -m "perf: latest-frame-wins display delivery via atomic frame slot"
```

---

### Task 5: H.265 DRM Hwaccel + DmaBuf Zero-Copy

**Files:**
- Modify: `src/core/aa/VideoDecoder.cpp:47-150,239-488`
- Modify: `src/core/aa/VideoDecoder.hpp`
- Modify: `src/core/aa/DmaBufVideoBuffer.cpp`
- Modify: `src/core/aa/DmaBufVideoBuffer.hpp`
- Modify: `src/core/aa/CodecCapability.cpp:19-66`
- Modify: `src/core/aa/CodecCapability.hpp`
- Test: `tests/test_codec_capability.cpp`

**This is the largest task and the biggest performance win.** It has two halves that must land together:

#### Part A: DRM Hwaccel Decoder Selection

**Context:** Pi 4's HEVC decoder uses the stateless V4L2 request API. `hevc_v4l2m2m` tries stateful and fails. The correct FFmpeg path: create a DRM hardware device context, attach it to the codec context, and request `AV_PIX_FMT_DRM_PRIME` output via `get_format`.

**Step 1: Add DRM render node discovery to CodecCapability**

In `CodecCapability.cpp`, add a function that probes `/dev/dri/renderD*` and returns the first valid one:
```cpp
static QString findDrmRenderNode()
{
    QDir dri("/dev/dri");
    for (const auto& entry : dri.entryList({"renderD*"}, QDir::System, QDir::Name)) {
        return "/dev/dri/" + entry;
    }
    return {};
}
```

**Step 2: Add DRM hwaccel candidate type**

Extend `CodecCapability` to know about the DRM hwaccel path. Add a new candidate type alongside the existing named decoders:
```cpp
struct DecoderCandidate {
    QString name;       // e.g. "hevc" (the standard decoder name)
    bool isHardware;
    bool isDrmHwaccel;  // NEW: uses av_hwdevice_ctx instead of named decoder
};
```

Update `kCandidates` to include DRM hwaccel entries before the named v4l2m2m entries:
```cpp
// H.265 candidates:
{"hevc", true, true},          // DRM hwaccel (stateless, Pi 4 rpi-hevc-dec)
{"hevc_v4l2m2m", true, false}, // stateful v4l2m2m
{"hevc_vaapi", true, false},   // VAAPI
{"hevc", false, false},        // software
```

**Step 3: Add DRM hwaccel init path to VideoDecoder::initCodec**

In `VideoDecoder.cpp`, when a DRM hwaccel candidate is selected:
```cpp
if (candidate.isDrmHwaccel) {
    QString renderNode = CodecCapability::findDrmRenderNode();
    if (!renderNode.isEmpty()) {
        AVBufferRef* hwDeviceCtx = nullptr;
        int ret = av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_DRM,
                                          renderNode.toUtf8().constData(), nullptr, 0);
        if (ret >= 0) {
            hwDeviceCtx_ = hwDeviceCtx;
            codecCtx_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
            codecCtx_->get_format = getHwFormat;
        }
    }
}
```

Add `get_format` callback:
```cpp
static enum AVPixelFormat getHwFormat(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    for (const auto* p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == AV_PIX_FMT_DRM_PRIME)
            return *p;
    }
    // Fallback to first non-NONE format
    return pix_fmts[0];
}
```

Add `AVBufferRef* hwDeviceCtx_ = nullptr` member to `VideoDecoder.hpp`. Free in destructor with `av_buffer_unref`.

**Step 4: Add low-latency decoder flags**

In `tryOpenCodec` (line 53-55), add:
```cpp
codecCtx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
codecCtx_->flags2 |= AV_CODEC_FLAG2_FAST;
```

These are already partially there — verify and add if missing.

#### Part B: DmaBuf Zero-Copy Display

**Context:** Current `DmaBufVideoBuffer::map()` calls `av_hwframe_transfer_data` to CPU — defeats zero-copy. Need `textureHandle()` for Qt 6.8 RHI path.

**Step 5: Investigate Qt 6.8 QAbstractVideoBuffer API on Pi**

Before writing code, SSH to Pi and check the actual Qt 6.8 header:
```bash
ssh matt@192.168.1.152 'find /usr -name "qabstractvideobuffer.h" -o -name "qvideoframe.h" | head -5'
```

Read the header to find the exact virtual method signatures for texture handle exposure. The API may be `mapTextures()` or `textureHandle()` depending on Qt 6.8 minor version. **Do not guess — read the actual header on the Pi.**

**Step 6: Implement textureHandle (or equivalent)**

Based on what Step 5 reveals, implement the RHI texture handle path in `DmaBufVideoBuffer`:
- Read `AVDRMFrameDescriptor` from `frame_->data[0]`
- Extract dmabuf fd, pitch, offset, modifier per plane
- Create EGLImage via `eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT, ...)`
- Wrap in QRhiTexture or return via Qt's handle API
- Keep `map()` as CPU fallback

Update `format()` to return the correct pixel format (likely NV12 for Pi HEVC output, not hardcoded YUV420P).

**Step 7: Update get_format to be conditional**

The `get_format` callback should only return `DRM_PRIME` when the DmaBuf RHI path is available. Add a compile-time and runtime check:
```cpp
static enum AVPixelFormat getHwFormat(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    for (const auto* p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
        if (*p == AV_PIX_FMT_DRM_PRIME)
            return *p;
#endif
    }
    // Prefer NV12 for hw decode with CPU transfer
    for (const auto* p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == AV_PIX_FMT_NV12 || *p == AV_PIX_FMT_YUV420P)
            return *p;
    }
    return pix_fmts[0];
}
```

**Step 8: Update processFrame DRM_PRIME path**

In `VideoDecoder.cpp:340-366`, the existing DRM_PRIME path constructs a `DmaBufVideoBuffer`. This should now work with the improved buffer. Verify the frame output format and adjust if needed (NV12 vs YUV420P).

**Step 9: Test on Pi**

This task requires Pi testing — hw decode doesn't work on the dev VM. Deploy, connect phone, verify:
1. `[VideoDecoder] Using hevc (DRM hwaccel)` in log (not "software")
2. Decode time drops from 22-29ms to 2-5ms
3. CPU drops from 125% to ~15-25%
4. Video renders correctly (no green/garbled frames)
5. No thermal throttling

**Step 10: Commit**

```bash
git commit -m "feat: H.265 DRM hwaccel decode + DmaBuf zero-copy display"
```

---

### Task 6: VideoFramePool Actual Recycling

**Files:**
- Modify: `src/core/aa/VideoFramePool.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp:388`
- Test: `tests/test_video_frame_pool.cpp`

**Context:** `VideoFramePool::acquire()` allocates a fresh `QVideoFrame` every call. The `poolSize_` member exists but is unused. Fix: maintain a ring of pre-allocated frames. Only pool software-path frames — DmaBuf frames are tied to FFmpeg's buffer lifetime.

**Step 1: Implement recycling in VideoFramePool**

Replace `acquire()` with a pool that checks for returned frames:
```cpp
QVideoFrame acquire()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Check for recyclable frames (ref count == 1 means only pool holds it)
    for (auto& slot : pool_) {
        if (!slot.isValid()) continue;
        // QVideoFrame is implicitly shared — if no one else holds it, reuse
        // Unfortunately QVideoFrame doesn't expose ref count directly.
        // Use a separate tracking mechanism or just pre-allocate and cycle.
    }
    ++totalAllocated_;
    return QVideoFrame(format_);
}
```

Note: QVideoFrame's implicit sharing makes true recycling tricky without Qt internals. A simpler approach: pre-allocate `poolSize_` frames and cycle through them round-robin. The previous frame must be consumed by Qt before we overwrite it. With latest-frame-wins (Task 4), only 1 frame is ever "in flight" to Qt, so a pool of 3-4 is sufficient.

**Step 2: Update tests**

Update `tests/test_video_frame_pool.cpp` to verify pool recycling behavior.

**Step 3: Build and test**

```bash
cd build && cmake --build . -j$(nproc) && ctest -R "test_video_frame_pool" --output-on-failure
```

**Step 4: Commit**

```bash
git commit -m "perf: VideoFramePool actual frame recycling for software decode path"
```

---

### Task 7: Telemetry Split

**Files:**
- Modify: `src/core/aa/VideoDecoder.cpp:443-482`
- Modify: `src/core/aa/PerfStats.hpp`

**Context:** With Task 1 fixing timestamps, the perf metrics now show real values. Split the single log line into more granular metrics for before/after comparison on each optimization.

**Step 1: Add upload/submit metric**

In `VideoDecoder.hpp`, add:
```cpp
PerfStats::Metric metricUpload_; // frame construction time (memcpy or dmabuf import)
```

Rename `metricCopy_` → `metricUpload_` for clarity (it already tracks this, but the name is misleading for the DmaBuf path where there's no copy).

**Step 2: Add display latency metric**

Track time from decode complete to frame delivery (the latest-frame slot write):
```cpp
auto t_uploadDone = PerfStats::Clock::now();
metricUpload_.record(PerfStats::msElapsed(t_decodeDone, t_uploadDone));
```

**Step 3: Update log format**

Change the rolling stats log (lines 463-482) to show all four metrics clearly:
```
[Perf] Video: queue=0.3ms decode=3.2ms upload=0.1ms total=3.6ms (max=5.1ms) | 29.8 fps drops=0
```

Include the queue drop count from Task 3.

**Step 4: Build and test**

```bash
cd build && cmake --build . -j$(nproc) && ctest --output-on-failure
```

**Step 5: Deploy to Pi, collect new baseline with all optimizations**

Compare against `docs/baselines/2026-02-26-video-pipeline-baseline.md`.

**Step 6: Commit**

```bash
git commit -m "feat: split video perf metrics into queue/decode/upload/total with drop count"
```

---

## Build & Test Reference

**Local (dev VM):**
```bash
cd build && cmake --build . -j$(nproc)
ctest --output-on-failure
```

**Cross-compile (Pi 4):**
```bash
docker run --rm -u "$(id -u):$(id -g)" -v "$(pwd):/src:rw" -w /src/build-pi openauto-cross-pi4 cmake --build . -j$(nproc)
```

**Deploy:**
```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
```

**Launch on Pi:**
```bash
ssh matt@192.168.1.152 'pkill -f openauto-prodigy; sleep 1; cd /home/matt/openauto-prodigy && nohup env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 ./build/src/openauto-prodigy > /tmp/oap.log 2>&1 &'
```

**Check logs:**
```bash
ssh matt@192.168.1.152 'grep "\[Perf\] Video" /tmp/oap.log | tail -5'
ssh matt@192.168.1.152 'grep "VideoDecoder" /tmp/oap.log | tail -10'
```
