# Audio/Video Pipeline Optimization Design

## Goal

Reduce CPU usage, latency, and memory allocations across the entire AA audio/video pipeline. Make the software path fast enough for 1080p@30 on Pi 4, and enable zero-copy hardware decode on platforms that support it. Widen the viable hardware audience beyond Pi 4 to other ARM SBCs.

## Architecture

Five optimization layers, each independently valuable and incrementally shippable. The software path improvements (layers 1, 4, 5) benefit all hardware. The hardware decode path (layers 2, 3) layers on top for platforms with capable FFmpeg + Qt 6.8+.

## Layer 1: Codec Capability Detection + Smart ServiceDiscovery

### Problem

ServiceDiscoveryBuilder currently advertises a static codec list (H.264 + H.265). If the system can't decode a codec efficiently, we waste the phone's bandwidth sending it and burn CPU on software decode.

### Design

**Startup probe:** At app startup, probe FFmpeg for available decoders per codec:

```
Probe order per codec:
  H.264: h264_v4l2m2m -> h264_vaapi -> h264 (software)
  H.265: hevc_v4l2m2m -> hevc_vaapi -> hevc (software)
  VP9:   vp9_v4l2m2m  -> vp9_vaapi  -> vp9 (software)
  AV1:   av1_v4l2m2m  -> av1_vaapi  -> dav1d (software)
```

Probe uses `avcodec_find_decoder_by_name()` — checks if the decoder exists in the linked FFmpeg, does NOT test actual decode. Results cached in a `CodecCapability` struct: `{codec, decoderName, isHardware}`.

**Installer/first-run:** Runs the probe once, writes results to YAML:

```yaml
video:
  capabilities:          # written by installer, read-only at runtime
    h264:
      hardware: [h264_v4l2m2m]
      software: [h264]
    h265:
      hardware: []
      software: [hevc]
    vp9:
      hardware: []
      software: [vp9, libvpx-vp9]
    av1:
      hardware: []
      software: [dav1d]
  codecs: [h264, h265]  # active codecs for ServiceDiscovery
  decoder:
    h264: h264_v4l2m2m   # selected decoder per codec
    h265: hevc
```

**ServiceDiscoveryBuilder** reads `video.codecs` to build the codec list in ServiceDiscoveryResponse. Only advertises codecs the system can decode.

**Settings UI (Video page):**

Per enabled codec:
- Hardware/Software toggle (Hardware disabled if no hw decoders available)
- Dropdown of available decoders for the selected mode (repopulates on toggle)
- H.264 checkbox always on (universal fallback, can't disable)
- Codec enable/disable checkboxes for H.265, VP9, AV1 (only shown if at least one decoder available)

Changes trigger the existing disconnect/retrigger flow for new ServiceDiscovery.

## Layer 2: FFmpeg Decoder Selection with Runtime Fallback

### Problem

If the configured hardware decoder fails (driver issue, device busy, thermal), there's no fallback — decode just fails and video dies.

### Design

**Decoder initialization** (when first video frame arrives):

1. Read user's selection: `video.decoder.h264: h264_v4l2m2m`
2. `avcodec_find_decoder_by_name("h264_v4l2m2m")`
3. `avcodec_open2()` with appropriate hw config
4. If open fails, log warning and fall back to software decoder
5. If open succeeds but first `avcodec_send_packet()` + `avcodec_receive_frame()` fails, tear down codec context and retry with software

**Hardware decoder FFmpeg config** (for DRM_PRIME output):
- Set `hw_device_ctx` via `av_hwdevice_ctx_create()` with `AV_HWDEVICE_TYPE_DRM`
- Set `get_format` callback to prefer `AV_PIX_FMT_DRM_PRIME`
- If hw decoder doesn't output DRM_PRIME, fall back to its default output format

**Fallback is per-session.** Next AA connection retries hardware. This handles transient failures without permanently downgrading.

**Logging:** Always log which decoder is active: `[VideoDecoder] Using h264_v4l2m2m (hardware)` or `[VideoDecoder] h264_v4l2m2m failed (error), falling back to h264 (software)`.

## Layer 3: Hybrid QVideoFrame (Zero-Copy HW, Optimized SW)

### Problem

Current path: FFmpeg decode -> memcpy Y/U/V planes into fresh QVideoFrame -> Qt uploads to GPU for rendering. At 1280x720 that's ~1.4 MB/frame * 30fps = 41.5 MB/s of pure memcpy, plus per-frame QVideoFrame allocation.

### Design

Two paths selected at runtime based on FFmpeg decoder output format:

#### Hardware Path (Qt >= 6.8, DRM_PRIME frames)

**Gate:** `#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)` + `AVFrame->format == AV_PIX_FMT_DRM_PRIME`

**Flow:**
1. FFmpeg hw decoder outputs `AVFrame` with `format=AV_PIX_FMT_DRM_PRIME`
2. Extract dmabuf fd from `AVDRMFrameDescriptor` in `frame->data[0]`
3. Import dmabuf as EGL image via `eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT, ...)`
4. Create GL texture from EGL image via `glEGLImageTargetTexture2DOES()`
5. Wrap in custom `QAbstractVideoBuffer` subclass that exposes the RHI texture handle
6. Construct `QVideoFrame` from the buffer
7. Qt renders directly from GPU texture — zero CPU copy

**Fallback:** If EGL import fails or Qt can't use the RHI texture, call `av_hwframe_transfer_data()` to copy hw frame to CPU, then use the software path.

**Thread safety:** `QAbstractVideoBuffer` destructor must be thread-safe (Qt render thread may destroy it). Use atomic ref counting or `deleteLater()` pattern for EGL/GL resource cleanup.

#### Software Path (all Qt versions)

**Frame pool:**
- Pre-allocate pool of raw Y/U/V buffers (5-8 buffers for safety margin)
- Each buffer wrapped in a custom destructor object that returns it to the pool on last QVideoFrame release
- Thread-safe return: pool uses a lock-free queue (or simple mutex — not on RT path)
- If no pool buffer available (all in flight), allocate a fresh one (graceful degradation, not a stall)

**Optimized copy:**
- Cache `QVideoFrameFormat` (already done)
- Stride-matched bulk copy: if FFmpeg stride == QVideoFrame stride, single `memcpy` per plane
- If strides don't match, row-by-row copy (current behavior, but only as fallback)

**Per-frame allocation eliminated:** Pool reuse means no `new QVideoFrame` on the hot path in steady state.

## Layer 4: Transport/Framing Copy Reduction

### Problem

Multiple unnecessary buffer copies and allocations in the message receive and send paths. At 30fps video + 3 audio streams, these add up.

### 4a. FrameParser Circular Buffer

**Current:** `m_buffer.append(data)` + `m_buffer.remove(0, N)` — `remove` does `memmove` of entire remaining buffer on every frame parse.

**Fix:** Replace `QByteArray m_buffer` with a circular buffer:
- Read/write cursors, pre-allocated backing store
- `append()` advances write cursor
- `consume(N)` advances read cursor — O(1), no memmove
- When current frame spans the wrap point, linearize just that frame into a small temp buffer
- Initial size: 64KB (covers several AA frames). Grows if needed.

### 4b. Messenger Zero-Copy Message ID Extraction

**Current:** `payload.mid(2)` allocates a new QByteArray, copying everything after the 2-byte message ID. Happens on every single AA message.

**Fix:** Pass payload + offset to handlers. Handlers receive `{QByteArray payload, int dataOffset}` and read from `payload.constData() + dataOffset`. Zero allocation, zero copy.

This changes the `IChannelHandler::onMessage()` signature. All handlers updated to use offset-based access.

### 4c. Send Path Allocation Reduction

**Current:** Per-message: allocate QByteArray for messageId prepend, QList<QByteArray> for frame serialization, new QByteArray per encrypted frame rebuild.

**Fix:** Pre-sized single output buffer per message:
1. Calculate total frame size upfront (header + messageId + payload + encryption overhead)
2. Allocate once via `QByteArray::reserve()`
3. Write header, messageId, payload sequentially
4. For encrypted path: encrypt in-place where possible (SSLv2-compatible ciphers allow this)

## Layer 5: Audio Latency Reduction

### Problem

Ring buffer hardcoded at 100ms adds noticeable lip-sync delay. This is the single largest latency contributor in the audio path.

### 5a. Per-Stream Buffer Sizing

Different stream types have different latency/stability trade-offs:

| Stream | Default | Rationale |
|--------|---------|-----------|
| Media | 50ms | Steady-state music/podcast. Stability > latency. |
| Speech | 35ms | Navigation prompts. Low latency preferred. |
| System | 35ms | Chimes/alerts. Low latency preferred. |

YAML config:
```yaml
audio:
  buffer_ms:
    media: 50
    speech: 35
    system: 35
```

### 5b. Adaptive Buffer Growth

**Trigger:** PipeWire exposes xrun (underrun) counters via stream info/events.

**Policy:**
1. Monitor xrun count per stream
2. If xruns exceed 2 in a 5-second window, grow buffer by 10ms
3. Cap at 100ms (original size)
4. Log each adaptation: `[Audio] Media stream: buffer grown to 60ms (2 xruns in 3.2s)`
5. Reset to configured default on next AA session

**YAML:**
```yaml
audio:
  adaptive: true    # enable adaptive buffer growth
```

### 5c. Silence Insertion (Water-Mark)

**Problem:** Returning short reads from the ring buffer (less than one PipeWire period) can cause discontinuities or glitches.

**Fix:** Minimum water-mark check in PipeWire RT callback:
- If ring buffer has fewer bytes than one PipeWire period, fill the remainder with silence
- This prevents partial-period reads that cause audible pops
- Log underrun for adaptive policy to act on

### 5d. PipeWire Alignment and Format Matching

**Quantum alignment:** Ensure ring buffer writes are aligned to PipeWire's period boundaries where possible. Misaligned writes amplify scheduling jitter.

**Format matching:** Verify AA audio format (sample rate, channels, bit depth) matches PipeWire stream format at stream creation. If mismatch, configure PipeWire stream to match AA format rather than resampling. AA typically sends 48kHz stereo 16-bit PCM, which PipeWire handles natively.

**Clock source:** Playback timing driven by PipeWire's clock (pull model via RT callback), not by AA packet arrival (push). The ring buffer absorbs jitter between the two clocks.

## Testing Strategy

### Layer 1-2 (Codec detection + decoder selection)
- Unit test: mock `avcodec_find_decoder_by_name()` responses, verify capability struct
- Unit test: decoder fallback (mock hw decoder failure, verify sw fallback activated)
- Integration: run on Pi 4, verify `h264_v4l2m2m` detected and used
- Integration: run on dev VM (no hw decoders), verify software-only path

### Layer 3 (Hybrid QVideoFrame)
- Unit test: frame pool allocation/return lifecycle, thread-safe return
- Benchmark: measure frame delivery time (memcpy vs pool reuse) on Pi 4
- Integration: verify video renders correctly with pool (no visual artifacts)
- Integration (Qt 6.8): verify DRM_PRIME path on Pi 4 with v4l2m2m

### Layer 4 (Transport optimization)
- Unit test: circular buffer correctness (wrap, linearize, grow)
- Unit test: offset-based message parsing (verify no data corruption)
- Benchmark: FrameParser throughput before/after on synthetic data

### Layer 5 (Audio latency)
- Unit test: ring buffer sizing from config
- Unit test: adaptive growth policy (mock xrun events)
- Unit test: silence insertion at water-mark boundary
- Integration: audio playback on Pi 4, verify no underruns at default settings
- Subjective: lip-sync test with video (compare 100ms vs 50ms vs 35ms)

## Implementation Order

Layers ordered by standalone value and risk:

1. **Layer 5 (audio latency)** — highest user-perceived impact, lowest risk, no API changes
2. **Layer 4 (transport copies)** — measurable CPU reduction, contained to open-androidauto library
3. **Layer 1 (codec detection)** — enables smart codec selection, needed before Layer 2
4. **Layer 3 software path** (frame pool) — reduces allocations, needed before hw path
5. **Layer 2 (decoder selection)** — hw decoder with fallback, depends on Layer 1
6. **Layer 3 hardware path** (QAbstractVideoBuffer) — biggest CPU win, depends on Layer 2 + Qt 6.8

## Platform Compatibility

| Feature | Qt 6.4 (dev VM) | Qt 6.8 (Pi/target) |
|---------|-----------------|---------------------|
| Codec detection | Yes | Yes |
| Decoder fallback | Yes | Yes |
| Frame pool (sw) | Yes | Yes |
| QAbstractVideoBuffer (hw) | No (ifdef'd out) | Yes |
| Transport optimizations | Yes | Yes |
| Audio latency | Yes | Yes |

All optimizations except the hardware QAbstractVideoBuffer path work on both Qt versions.
