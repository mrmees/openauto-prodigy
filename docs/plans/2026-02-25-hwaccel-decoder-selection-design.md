# HW Accelerated Decoder Selection — Design

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Support both named-decoder and hwaccel-based hardware video decode in a single, data-driven fallback chain that's portable across SBCs.

**Architecture:** Lightweight struct-based candidate list with a flat iteration loop. No virtual class hierarchy. Two code paths (named decoder vs hwaccel) handled via a switch in a single `tryOpenCandidate()` helper.

**Tech Stack:** FFmpeg libavcodec/libavutil, V4L2 (stateful + stateless/request API), DRM hwaccel, VAAPI

---

## Problem

Pi 4's HEVC hardware decoder (`rpi-hevc-dec` on `/dev/video19`) uses the **stateless V4L2 request API**, not the stateful m2m API. FFmpeg's `hevc_v4l2m2m` tries the stateful path and fails with "Could not find a valid device."

However, `ffmpeg -hwaccel drm -hwaccel_device /dev/dri/renderD128` works — FFmpeg auto-selects the v4l2request backend when given a DRM hw device context. The Pi's FFmpeg 7.1.3 (`+rpt1`) is built with `--enable-v4l2-request`.

This means we have two fundamentally different hw decode paths:
- **Named decoder** (e.g. `h264_v4l2m2m`): standalone decoder via `avcodec_find_decoder_by_name()`
- **Hwaccel** (e.g. DRM): standard software decoder with `hw_device_ctx` attached via `av_hwdevice_ctx_create()`

## Design

### Core Types

```cpp
struct DecoderCandidate {
    enum class Type { Named, Hwaccel };
    Type type;
    const char* name;              // decoder name or hwaccel keyword
    AVHWDeviceType hwDeviceType;   // only for Hwaccel (e.g. AV_HWDEVICE_TYPE_DRM)
    const char* devicePath;        // e.g. "/dev/dri/renderD128" (only for Hwaccel)
};
```

Frame output path is inferred at runtime after successful open, not declared per-candidate:
- `AV_PIX_FMT_DRM_PRIME` → DmaBufVideoBuffer (Qt 6.8) or transfer to CPU (Qt 6.4)
- `AV_PIX_FMT_YUV420P` / `YUVJ420P` → VideoFramePool (current software path)
- Anything else → `av_hwframe_transfer_data()` to CPU, then YUV path

### Default Fallback Chains

| Codec | Chain |
|-------|-------|
| H.264 | `h264_v4l2m2m` → DRM hwaccel → `h264_vaapi` → `h264` (sw) |
| H.265 | `hevc_v4l2m2m` → DRM hwaccel → `hevc_vaapi` → `hevc` (sw) |
| VP9   | `vp9_v4l2m2m` → DRM hwaccel → `vp9_vaapi` → `vp9` (sw) |
| AV1   | `av1_v4l2m2m` → DRM hwaccel → `av1_vaapi` → `dav1d` (sw) |

### Three Helper Functions

1. **`buildCandidates(codecId, userPref)`** — Returns ordered `std::vector<DecoderCandidate>`.
   - `"auto"` → default chain for codec
   - `"hevc_v4l2m2m"` → named decoder first, then rest of chain
   - `"drm"` → DRM hwaccel first, then rest of chain
   - `"vaapi"` → VAAPI hwaccel first, then rest of chain
   - Any other string → treat as named decoder

2. **`tryOpenCandidate(candidate, codecId)`** — Switch on `Type`:
   - **Named**: `avcodec_find_decoder_by_name()` → alloc context → `avcodec_open2()` → init parser
   - **Hwaccel**: `av_hwdevice_ctx_create(type, devicePath)` → `avcodec_find_decoder(codecId)` (software) → assign `hw_device_ctx` to context → `avcodec_open2()` → init parser

3. **`inferFramePath(codecCtx, frame)`** — After first decode, check `frame->format`:
   - `AV_PIX_FMT_DRM_PRIME` → DmaBufVideoBuffer path
   - `AV_PIX_FMT_YUV420P` / `YUVJ420P` → frame pool path
   - Other hw format → `av_hwframe_transfer_data()` then frame pool

### Config Schema

Existing YAML config unchanged. Extended interpretation of values:

```yaml
video:
  decoder:
    h264: "auto"          # default chain
    h265: "drm"           # force DRM hwaccel path
    vp9: "vp9_v4l2m2m"   # force specific named decoder
    av1: "auto"           # default chain
```

### Detection

- **Named decoder**: `avcodec_find_decoder_by_name()` — instant, returns NULL if not available
- **Hwaccel**: `av_hwdevice_ctx_create()` — fast, fails immediately if device doesn't exist or driver doesn't support it
- No enumeration of `/dev/video*` at startup. Let FFmpeg own device discovery.

## SBC Portability

| SBC | H.264 path | H.265 path | Notes |
|-----|-----------|-----------|-------|
| Pi 4 | `h264_v4l2m2m` (stateful) | DRM hwaccel (stateless v4l2request) | hevc_v4l2m2m fails on Pi 4 |
| Pi 5 | DRM hwaccel (likely) | DRM hwaccel | VideoCore VII, same driver family |
| Rockchip RK3588 | DRM hwaccel (rkvdec2) | DRM hwaccel (rkvdec2) | Stateless for all codecs |
| Amlogic S905X3/4 | Named v4l2m2m (stateful) | Named v4l2m2m (stateful) | Stateful for all codecs |
| Intel/AMD x86 | VAAPI | VAAPI | Standard desktop path |

Adding a new SBC = add/reorder entries in the per-codec candidate list. No new classes or code paths.

## What Changes

**Modified files:**
- `src/core/aa/VideoDecoder.hpp` — Add `DecoderCandidate` struct, `AVBufferRef* hwDeviceCtx_` member
- `src/core/aa/VideoDecoder.cpp` — Rewrite `initCodec()` to use candidate list, add `buildCandidates()` and `tryOpenCandidate()`, update `processFrame()` frame path dispatch
- `src/core/aa/DmaBufVideoBuffer.cpp` — May need NV12 format support (v4l2m2m/DRM often output NV12 not YUV420P)

**Unchanged:**
- `CodecCapability::probe()` and `CodecCapabilityModel` (Settings UI)
- `ServiceDiscoveryBuilder` codec advertisement
- First-frame hw→sw fallback logic
- `PerfStats` instrumentation
- YAML config schema (extended interpretation only)

## Discovered During Pi Testing (2026-02-25)

- Pi 4 V4L2 devices: `/dev/video10` = bcm2835-codec (H.264 stateful), `/dev/video19` = rpi-hevc-dec (HEVC stateless)
- `hevc_v4l2m2m` error: "Could not find a valid device" — bcm2835-codec doesn't do HEVC
- `ffmpeg -hwaccel drm -hwaccel_device /dev/dri/renderD128` successfully decodes HEVC
- FFmpeg 7.1.3+rpt1 on kernel 6.12 is the required combo (forum: raspberrypi.com/viewtopic.php?t=386267)
- Software HEVC decode at 480p: ~10-15ms/frame, 30fps stable, but higher CPU than necessary
- Samsung S25 Ultra prefers H.265 when offered alongside H.264
