# Video Pipeline Optimization Design

**Goal:** Minimize video decode latency, CPU usage, and memory overhead for the AA video pipeline on Pi 4, while establishing a portable foundation for future SBCs.

**Architecture:** Seven independent improvements to the decode-to-display pipeline. Each is testable in isolation. The DRM hwaccel and DmaBuf zero-copy tasks are co-dependent and land together.

**Tech Stack:** FFmpeg 7.x (DRM hwaccel, v4l2m2m), Qt 6.8 (QAbstractVideoBuffer, RHI), PipeWire, C++17

**Baseline (1080p H.265 software decode on Pi 4):**

| Metric | Value |
|--------|-------|
| CPU | 125% (1.25 of 4 cores) |
| Decode | 22-29ms/frame |
| YUV memcpy | 4.2ms/frame |
| FPS | 28-33 (33ms budget) |
| Temperature | 62.3°C, no throttling |

---

## Task 1: Fix Timestamp Semantics

**Files:** `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp`, `src/core/aa/VideoDecoder.cpp`

Stamp frames with `std::chrono::steady_clock::now()` at the emit site in `VideoChannelHandler::onMediaData`. Currently the AA protocol timestamp (phone clock, microseconds since session start) is passed through as `enqueueTimeNs`, producing nonsense queue wait metrics (~101 billion ms). The AA protocol timestamp should be preserved separately if needed for A/V sync, but the perf metric path must use local steady_clock.

## Task 2: Zero-Copy Payload Forwarding

**Files:** `libs/open-androidauto/include/oaa/HU/Handlers/VideoChannelHandler.hpp`, `VideoChannelHandler.cpp`, `src/core/aa/VideoDecoder.cpp`, `src/core/aa/AndroidAutoOrchestrator.cpp`

Replace the `Qt::QueuedConnection` signal that deep-copies `QByteArray` across threads with `std::shared_ptr<const QByteArray>` passed through a direct connection to the decode worker's thread-safe queue. At 1080p, H.265 keyframes are 50-150KB — eliminating the per-frame allocation+copy reduces jitter at the protocol-to-decode boundary.

## Task 3: Bounded Decode Queue with Keyframe Awareness

**Files:** `src/core/aa/VideoDecoder.cpp`, `src/core/aa/VideoDecoder.hpp`

Cap the decode queue at 2 frames. On overflow:
1. Drop oldest **non-keyframe** first
2. If only keyframes remain, drop oldest and set `awaiting_keyframe = true`
3. While `awaiting_keyframe`, discard all non-IDR frames and call `avcodec_flush_buffers()` to clear stale references
4. Resume normal decode when next keyframe arrives

Keyframe detection: parse HEVC NAL type from Annex B header (IDR = types 19, 20). For H.264: IDR = NAL type 5.

Video ACKs must still be sent for dropped frames (the phone's flow controller stalls without them).

## Task 4: Latest-Frame-Wins Display Delivery

**Files:** `src/core/aa/VideoDecoder.cpp`, `src/core/aa/AndroidAutoOrchestrator.cpp`

Replace per-frame `QMetaObject::invokeMethod(Qt::QueuedConnection)` with a mutex-guarded latest-frame slot:

```cpp
std::mutex latestMutex_;
QVideoFrame latestFrame_;
std::atomic<bool> hasLatest_{false};
```

Decode thread: lock, swap frame, unlock, set flag.
Render tick (QTimer or vsync-driven): if `hasLatest_.exchange(false)`, lock, move frame, unlock, call `setVideoFrame()`.

`QVideoFrame` is implicitly shared — swap is cheap. Contention at 30fps is negligible.

## Task 5: H.265 DRM Hwaccel + DmaBuf Zero-Copy

**Files:** `src/core/aa/VideoDecoder.cpp`, `src/core/aa/VideoDecoder.hpp`, `src/core/aa/DmaBufVideoBuffer.cpp`, `src/core/aa/DmaBufVideoBuffer.hpp`, `src/core/aa/CodecCapability.cpp`

### DRM Hwaccel (decoder side)

Generic capability-driven decoder selection:
1. Probe `/dev/dri/renderD*` nodes
2. `av_hwdevice_ctx_create(AV_HWDEVICE_TYPE_DRM, renderNode, nullptr, nullptr, 0)`
3. Attach to codec context: `codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx)`
4. Set `get_format` callback

Fallback chain per codec: DRM hwaccel → v4l2m2m named decoder → software.

`get_format` callback logic:
- Prefer `AV_PIX_FMT_DRM_PRIME` only when RHI zero-copy path is available (DmaBuf textureHandle implemented)
- Otherwise prefer `AV_PIX_FMT_NV12` or `AV_PIX_FMT_YUV420P` for CPU-mappable output

Low-latency decoder flags: `AV_CODEC_FLAG_LOW_DELAY | AV_CODEC_FLAG2_FAST`

First-frame hw→sw fallback preserved from v0.4.0.

### DmaBuf Zero-Copy (display side)

Current `DmaBufVideoBuffer::map()` calls `av_hwframe_transfer_data()` to CPU on the render thread — defeats zero-copy. Fix:

1. Implement `handleType()` → `QVideoFrame::RhiTextureHandle`
2. Implement `handle(QRhi* rhi)` → construct `QRhiTexture` from dmabuf via EGL image import
3. Read `AVDRMFrameDescriptor` from `AVFrame->data[0]` — respect per-plane modifiers
4. Keep `map()` as CPU fallback only (used when RHI path unavailable)

Qt 6.8's `VideoOutput` RHI backend calls `textureHandle()` when `handleType()` returns `RhiTextureHandle`, bypassing `map()` entirely.

### SBC Portability

No Pi-specific device paths in selection logic. The DRM render node is discovered by probing `/dev/dri/renderD*`. The codec capabilities are detected at startup via `CodecCapability`. Future SBCs (Rock 5B with rkmpp, Orange Pi with cedar) will work if their FFmpeg builds expose the appropriate decoders.

## Task 6: VideoFramePool Actual Recycling

**Files:** `src/core/aa/VideoFramePool.hpp`, `src/core/aa/VideoDecoder.cpp`

Pool **software-path** `QVideoFrame` allocations only. DmaBuf frames are tied to FFmpeg's dmabuf lifetime and cannot be recycled by us.

Ring of pre-allocated frames (pool size 4-5). Return-on-release pattern: when `QVideoFrame`'s ref count drops to 1 (only the pool holds it), it's available for reuse.

## Task 7: Telemetry Split

**Files:** `src/core/aa/VideoDecoder.cpp`, `src/core/aa/PerfStats.hpp`

With timestamps fixed (Task 1), split the single "total" metric into:
- **Queue wait:** time from emit to decode start
- **Decode:** FFmpeg decode time
- **Upload/submit:** time to construct QVideoFrame (memcpy or dmabuf import)
- **Display latency:** time from decode complete to frame visible (requires render-side timestamp)

This gives per-task before/after measurement capability.

---

## What Doesn't Change

- Protocol layer (Messenger, FrameParser, CircularBuffer) — optimized in v0.4.0
- Audio pipeline — just shipped with adaptive rate matching
- QML VideoOutput — no changes needed
- Video ACK flow control — already correct (per-frame ACK)

## Expected Results (Post-Optimization)

| Metric | Before | After (estimated) |
|--------|--------|-------------------|
| CPU (1080p H.265) | 125% | 15-25% |
| Decode time | 22-29ms | 2-5ms (hw) |
| Copy/upload time | 4.2ms | <0.5ms (zero-copy) |
| Worst-case latency | Unbounded | ~66ms (2-frame queue) |
| Memory per frame | malloc each | Recycled pool |
| Payload copy | 50-150KB deep copy | Shared pointer |
