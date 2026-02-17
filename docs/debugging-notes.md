# Debugging Notes

Active troubleshooting log for issues encountered during development.

---

## Service Channel Issue (Task 6 — Active)

**Status:** Unresolved as of 2026-02-16

### Problem

Phone connects and completes the full AA handshake but **never opens any service channels**. After ~15 seconds of silence, a USB write (ping) times out.

This is NOT a video pipeline issue — the same behavior occurs with the old stub services.

### What Works

- FFmpeg H.264 software decoder initializes successfully
- All 8 services start and call `channel_->receive()`
- Full handshake completes: version 1.7 → SSL → auth → service discovery
- Phone identified as Samsung SM-S938U (Galaxy S24 Ultra)
- ServiceDiscoveryResponse sends 253 bytes across 8 channels
- Phone sometimes sends AudioFocusRequest (we respond with GAIN)

### What Doesn't Work

- After service discovery response, phone NEVER opens any service channels
- Zero non-control frames observed in the entire session
- After ~15s, our ping write times out (`USB_TRANSFER` / `LIBUSB_TRANSFER_TIMEOUT`)
- Sometimes handshake doesn't even start (stale AOAP state from prior startup)

### Things Ruled Out

1. **ServiceDiscoveryResponse format** — compared field-by-field with f1xpl and SonOfGib upstream. All 12 required fields match.
2. **Channel descriptors** — all 8 channels properly configured (VIDEO 480p/30fps, 3 audio, input, sensor, BT, WiFi)
3. **`available_while_in_call`** — added to video + all audio channels, no effect
4. **`unrequested` flag** — corrected from `true` to `false` in VideoFocusIndication, no effect
5. **Phone reboot** — same behavior after fresh reboot
6. **Code regression** — same behavior confirmed with old VideoServiceStub binary

### Things Still To Try

1. **Clear Android Auto cache/data on phone** — Settings → Apps → Android Auto → Clear Data
2. **Try a different USB cable** — rule out signal integrity
3. **Try a different phone** — rule out Samsung-specific behavior
4. **Build upstream openauto directly on the Pi** — verify that known-working code actually works on this hardware + OS combination
5. **Hex-dump the serialized ServiceDiscoveryResponse** — compare byte-for-byte with upstream's output
6. **Check for unhandled control messages** — phone may send something between auth and service discovery that we're silently dropping
7. **Check ControlServiceChannel message dispatch** — messages may be arriving but getting dropped by the channel dispatcher
8. **Wireshark/usbmon USB traffic capture** — compare with a known-working session

### Typical Log Pattern

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

### Diagnostic Logging In Place

- `ShortDebugString()` for both ServiceDiscoveryRequest and ServiceDiscoveryResponse
- Serialized response size logged
- All service start/stop logged
- All channel errors logged with error codes

### Files Modified for Debugging

- `src/core/aa/AndroidAutoEntity.cpp` — proto diagnostic logging
- Three debug commits: `42ee155`, `1f53b77`, `00edf26`
