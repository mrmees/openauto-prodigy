# Debugging Notes

Troubleshooting log for issues encountered during development.

---

## USB Service Channel Issue (RESOLVED — abandoned USB)

**Status:** Won't fix — switched to wireless-only

### Problem

Phone connects and completes the full AA handshake over USB but **never opens any service channels**. After ~15 seconds of silence, a USB write (ping) times out.

### Resolution

Abandoned USB transport entirely. Wireless AA (BT discovery → WiFi AP → TCP) works reliably on first connection. Service channels open immediately after service discovery over TCP.

### Original Log Pattern

```
VideoDecoder initialized
AAService starting
USB AOAP device connected
Services started (VideoService, 3 audio stubs, input, sensor, BT, WiFi)
Version request sent → Version 1.7 OK
SSL handshake (2 messages) → complete
Auth complete sent
Service discovery from Android (samsung SM-S938U)
Responding with 8 channels (253 bytes)
Service discovery sent — connected!
[15 seconds of silence]
Send error: USB_TRANSFER / LIBUSB_TRANSFER_TIMEOUT
```

---

## Video Not Displaying (RESOLVED)

**Status:** Fixed — commit `579152b`

### Problem

Phone connected wirelessly, AA protocol completed, video data was flowing, but every frame failed to decode with:
```
[h264] non-existing PPS 0 referenced
Send packet error: -1094995529 (AVERROR_INVALIDDATA)
```

### Root Cause

**SPS/PPS codec configuration data was being discarded.** Android Auto sends SPS/PPS as `AV_MEDIA_INDICATION` (message ID 0x0001, no timestamp), but `VideoService::onAVMediaIndication()` was ignoring the data with a comment "Non-timestamped media — unlikely for video but handle it."

Without SPS/PPS, the H.264 decoder has no codec parameters and cannot decode any frames.

### Fix

Forward buffer data from `onAVMediaIndication` to the decoder, same as `onAVMediaWithTimestampIndication`:

```cpp
void VideoService::onAVMediaIndication(const aasdk::common::DataConstBuffer& buffer)
{
    emit videoFrameData(QByteArray(reinterpret_cast<const char*>(buffer.cdata), buffer.size));
    channel_->receive(shared_from_this());
}
```

### How We Found It

Hex-dumped the first few video messages after "AV channel start":
1. First message: VideoFocusRequest (not media data)
2. Second message: msg ID 0x0001, payload `00 00 00 01 67 42 80 1F ...` — SPS NAL (0x67) + PPS NAL (0x68)
3. Third message: msg ID 0x0000, payload with 8-byte timestamp + IDR frame (0x65)

Message ID 0x0001 is `AV_MEDIA_INDICATION` (no timestamp) vs 0x0000 is `AV_MEDIA_WITH_TIMESTAMP_INDICATION`.

---

## AnnexB Start Code Double-Prepend (RESOLVED)

**Status:** Fixed — commit `579152b` (same session)

### Problem

`VideoDecoder::processData()` was prepending `00 00 00 01` AnnexB start codes before feeding data to FFmpeg's parser.

### Root Cause

Original assumption was that aasdk delivered raw NAL units without start codes. In reality, the data already contains AnnexB start codes. Double-prepending corrupted the bitstream.

### Fix

Removed the start code prepend. Feed raw data directly to `av_parser_parse2()`.

---

## Touch Calibration (ACTIVE)

**Status:** Under investigation

### Problem

Touch input is working (events reach the phone, UI responds) but the user reports it "seems like we need to calibrate" — touches may not be landing exactly where expected.

### Current State

- Touch debug overlay added showing green crosshair circles at each touch point
- Each circle displays the mapped AA-space coordinates (1024x600)
- Coordinate mapping: `nx = Math.round(tp.x / width * 1024)`, `ny = Math.round(tp.y / height * 600)`
- The display is 1024x600, video is 1280x720, AA touch config is 1024x600

### Next Steps

1. Use the debug overlay to observe where touches register vs where they should land
2. Check if the offset is consistent (translation) or proportional (scaling)
3. Verify the `VideoOutput.Stretch` fill mode doesn't create an aspect ratio mismatch between touch area and video area (1280x720 ≠ 1024x600 aspect ratio)

---

## vtable Undefined Reference for QObject Subclasses (RESOLVED)

**Status:** Fixed — commits `e2b3c6a`, `3f03b4e`

### Problem

```
undefined reference to `vtable for oap::aa::TouchHandler'
```

### Root Cause

`TouchHandler` was a header-only QObject (had `Q_OBJECT` macro but no `.cpp` file). Qt's AUTOMOC needs a `.cpp` file listed in `CMakeLists.txt` to trigger MOC processing.

### Fix

1. Created `TouchHandler.cpp` containing just `#include "TouchHandler.hpp"`
2. Added `core/aa/TouchHandler.cpp` to `qt_add_executable` in `src/CMakeLists.txt`

### Lesson

Any class with `Q_OBJECT` needs a `.cpp` file in the CMake source list. Header-only QObjects don't work with AUTOMOC.
