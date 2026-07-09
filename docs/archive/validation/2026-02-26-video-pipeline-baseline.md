# Video Pipeline Baseline — 2026-02-26

Measurements taken during live AA session on Pi 4, Samsung S25 Ultra (Android 15).

## Test Configuration

| Setting | Value |
|---------|-------|
| Resolution | 1080p (1920x1080) |
| Codec | H.265 (HEVC) |
| Decoder | `hevc` (software) — `hevc_v4l2m2m` fails on Pi 4 stateless decoder |
| Target FPS | 30 |
| Pi model | Raspberry Pi 4 (4 cores, 1.8GHz) |
| OS | RPi OS Trixie (Debian 13) |
| FFmpeg | 7.x |
| Qt | 6.8 |

## Results: 1080p H.265 Software Decode

| Metric | Value | Notes |
|--------|-------|-------|
| CPU usage | 125% | 1.25 of 4 cores — decode thread dominates |
| Decode time | 22-29ms/frame | Near 33ms budget at 30fps |
| Copy time (YUV memcpy) | 4.2ms/frame | Y+U+V plane copy for 1080p = ~3.1MB |
| FPS | 28-33 fps | Barely keeping up, spikes cause drops |
| Memory (RSS) | 240MB | |
| Temperature | 62.3°C | No throttling (0x0) |
| Thread count | 12 | |
| Queue wait | N/A | Metric broken — AA protocol timestamp passed instead of steady_clock |

## Results: 480p H.265 Software Decode (earlier session)

| Metric | Value |
|--------|-------|
| Decode time | 3.5-6ms/frame |
| Copy time | 0.9-1.3ms/frame |
| FPS | ~30 fps (stable) |
| CPU usage | ~25% (estimated) |

## Decoder Fallback Log

```
CodecCapability: found SW decoder h264 for h264
CodecCapability: found HW decoder hevc_v4l2m2m for h265
CodecCapability: found SW decoder hevc for h265
[VideoDecoder] Auto-detected hw decoder: hevc_v4l2m2m
[hevc_v4l2m2m @ 0x7f88001440] Could not find a valid device
[hevc_v4l2m2m @ 0x7f88001440] can't configure decoder
[VideoDecoder] hevc_v4l2m2m failed to open, falling back to software
[VideoDecoder] Using hevc (software)
```

## Key Observations

1. **H.265 software decode is the bottleneck.** At 1080p, decode alone takes 22-29ms of the 33ms frame budget. Any jitter pushes past budget.
2. **Copy cost is non-trivial at 1080p.** 4.2ms per frame for YUV plane memcpy adds ~13% overhead on top of decode.
3. **CPU headroom is thin.** 125% CPU usage for video alone leaves only ~275% for audio, protocol, UI, compositor, and OS.
4. **Pi 4 has a hardware HEVC decoder** (`/dev/video19`, `rpi-hevc-dec`) that uses the stateless V4L2 request API. The current `hevc_v4l2m2m` path tries the stateful API and fails. The correct FFmpeg path is DRM hwaccel via `av_hwdevice_ctx_create(AV_HWDEVICE_TYPE_DRM, "/dev/dri/renderD128")`.
5. **Zero-copy (DmaBuf) would eliminate the 4.2ms copy** by passing DRM_PRIME frames directly to Qt's renderer.

## Expected Improvements (Post-Optimization)

| Optimization | Expected decode time | Expected CPU | Notes |
|-------------|---------------------|-------------|-------|
| H.265 DRM hwaccel | 2-5ms | ~15-20% | Hardware HEVC decoder |
| + DmaBuf zero-copy | 2-5ms | ~10-15% | Eliminates 4.2ms memcpy |
| + Frame drop policy | Same | Same | Bounds worst-case latency |
