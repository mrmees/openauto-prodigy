# ABI Compatibility Analysis: OAP libaasdk.so vs Upstream/SonOfGib

## Executive Summary

**A drop-in replacement of libaasdk.so with SonOfGib's build will NOT work.** BlueWave made significant additions to both libaasdk.so and libaasdk_proto.so that autoapp depends on. However, the analysis reveals that BlueWave already had the *proto infrastructure* for wireless credentials — they just didn't handle the new AA 12.7+ message flow correctly.

## Library Dependencies

### autoapp → libaasdk.so
- 126 dynamic symbol imports from `f1x::aasdk` namespace
- Depends on **3 BlueWave-added channel classes** not in upstream:
  - `channel::wireless::WirelessServiceChannel` — wireless AA support
  - `channel::navigation::NavigationServiceChannel` — turn-by-turn nav
  - `channel::av::MediaStatusServiceChannel` — media playback status

### autoapp → libaasdk_proto.so
- Imports wireless proto messages already:
  - `WirelessCredentialsResponse` (ctor + dtor)
  - `WirelessInfoResponse` (ctor + dtor)
  - `WirelessTcpConfiguration` (ctor)
  - `WirelessChannel` (Arena allocator)
  - Wireless enums: `WirelessSecurityMode`, `WirelessAccessPointType`, `WirelessInfoResponseStatus`

### libaasdk.so → libaasdk_proto.so
- libaasdk.so imports 44 `f1x::aasdk::proto` symbols from libaasdk_proto.so

## Critical Finding: BlueWave's WirelessServiceChannel

BlueWave **already has** a `WirelessServiceChannel` in libaasdk.so. Exported methods:

```
channel::wireless::WirelessServiceChannel::WirelessServiceChannel(strand&, IMessenger::Pointer)
channel::wireless::WirelessServiceChannel::getId()
channel::wireless::WirelessServiceChannel::messageHandler(Message, IWirelessServiceChannelEventHandler)
channel::wireless::WirelessServiceChannel::receive(IWirelessServiceChannelEventHandler)
```

**What's MISSING** compared to every other channel:
- No `handleChannelOpenRequest()` — other channels have this
- No `sendChannelOpenResponse()` — other channels have this
- No `sendWirelessCredentialsResponse()` — the critical response method
- No `handleWirelessCredentialsRequest()` — the critical request handler

The WirelessServiceChannel logs `[WirelessServiceChannel] message not handled: ` for unrecognized message IDs, confirming it drops them.

Compare SonOfGib's `WIFIServiceChannel` (note: different class name, `wifi` namespace):
- Has `sendChannelOpenResponse()`
- Has `sendWifiSecurityResponse()`
- Has `handleWifiSecurityRequest()` via `IWIFIServiceChannelEventHandler::onWifiSecurityRequest()`

## ROOT CAUSE: Message ID Mismatch

BlueWave and SonOfGib define **different message IDs** for the same protocol messages:

| Message | BlueWave (`WirelessMessage`) | SonOfGib (`WifiChannelMessage`) | AA 12.7+ sends |
|---------|------------------------------|--------------------------------|----------------|
| NONE | 0x0000 | 0x0000 | - |
| INFO_REQUEST | 0x0001 | N/A | ? |
| CREDENTIALS_REQUEST | **0x0002** | **0x8001** | **0x8001** |
| CREDENTIALS_RESPONSE | **0x0003** | **0x8002** | - |
| INFO_RESPONSE | 0x0007 | N/A | - |
| AUTH_DATA | 0x8003 | N/A | - |

SonOfGib's implementation works with AA 12.7-13.1, confirming AA sends `0x8001`. BlueWave's messageHandler has a switch case for `0x0002` (their CREDENTIALS_REQUEST value), which AA never sends. The `0x8001` from AA hits the default case and gets logged as "message not handled."

**This is the bug.** Even if BlueWave's WirelessServiceChannel had all the send/response methods, it would STILL fail because the message IDs don't match what modern Android Auto actually sends.

## Potential Second Bug: Message Interleaving

Upstream f1xpl/aasdk's `MessageInStream` rejects interleaved frames with `MESSENGER_INTERTWINED_CHANNELS`. BlueWave's libaasdk.so has **no** "intertwined" error string, suggesting they may have already fixed this. SonOfGib also fixed this with a per-channel message buffer. Ghidra decompilation of BlueWave's `MessageInStream::receiveFrameHeaderHandler()` is needed to confirm.

## BlueWave's WiFi Architecture (in autoapp, not libaasdk)

The autoapp binary contains application-level WiFi management (hotspot, not AA protocol):
- `autoapp::system::WifiManager` — manages RPi WiFi hotspot (start/stop)
- `autoapp::ui::WifiController` — QML singleton for UI
- `autoapp::autobox::WiFiNameMessage` — autobox dongle communication
- Config: `.openauto/cache/openauto_wifi_recent.ini`

This is **separate from** the AA protocol WiFi credential exchange. It manages the Pi's WiFi AP for wireless AA dongle connections.

## OAP libaasdk.so vs Upstream: BlueWave Additions

### Channels added to libaasdk.so (not in upstream f1xpl/aasdk):
| Channel | Namespace | In Upstream? | In SonOfGib? |
|---------|-----------|--------------|--------------|
| WirelessServiceChannel | channel::wireless | NO | NO (they have WIFIServiceChannel in channel::wifi) |
| NavigationServiceChannel | channel::navigation | NO | YES (SonOfGib added it too) |
| MediaStatusServiceChannel | channel::av | NO | YES (SonOfGib added it too) |

### Proto additions in libaasdk_proto.so (not in upstream):
- WirelessCredentialsResponse, WirelessInfoResponse, WirelessTcpConfiguration
- WirelessChannel (data type)
- WirelessSecurityMode, WirelessAccessPointType, WirelessInfoResponseStatus (enums)
- NavigationChannel, MediaStatusChannel (data types for service discovery)

## Fix Approaches — Updated Assessment

### ❌ Drop-in libaasdk.so replacement (SonOfGib's build)
**Won't work.** Missing WirelessServiceChannel, and SonOfGib uses different class names/namespaces (WIFIServiceChannel vs WirelessServiceChannel).

### ❌ Drop-in libaasdk_proto.so replacement
**Won't work.** BlueWave's proto messages use different names than SonOfGib's (WirelessCredentialsResponse vs WifiSecurityResponse, etc.)

### ⚠️ Binary patch of autoapp
**Possible but complex.** Would need to patch the WirelessServiceChannel::messageHandler() to actually handle credential requests and send responses. Requires understanding the vtable layout and message dispatch.

### ⚠️ LD_PRELOAD hook
**Most promising minimal fix.** Could intercept the WirelessServiceChannel::messageHandler() and inject proper credential response handling. Would need to:
1. Hook `WirelessServiceChannel::messageHandler()`
2. Check for credential request message ID
3. Construct and send a WirelessCredentialsResponse
4. This requires the proto library (already loaded by autoapp)

### ✅ Rebuild libaasdk.so with BlueWave additions
**Best long-term fix.** Fork SonOfGib's aasdk, add BlueWave's wireless/navigation/media-status channels with proper implementations. Must match exact ABI (Boost 1.67, protobuf 17, OpenSSL 1.1, armhf).

### ✅ Full rebuild of everything
**Ultimate goal.** Rebuild autoapp + libaasdk.so + libaasdk_proto.so together. Most work but most control.

## ABI Version Requirements

For any library replacement to work, must match:
- **Boost:** 1.67.0 (libboost_*.so.1.67.0)
- **Protobuf:** 17 (libprotobuf.so.17)
- **OpenSSL:** 1.1 (libssl.so.1.1, libcrypto.so.1.1)
- **GCC ABI:** cxx11 (confirmed by `[abi:cxx11]` tags in symbols)
- **Architecture:** ARM armhf (32-bit)
- **RUNPATH:** /home/pi/workdir/openauto/lib

## SonOfGib's Fix: Scope Summary

**aasdk layer (~500 lines of functional changes):**
- New `WIFIServiceChannel` class (271 lines) — handles WiFi credential request/response
- Rewrote `MessageInStream` (107 lines) — per-channel message buffer for interleaved frames
- Updated `ControlServiceChannel` (55 lines) — added ping response + voice session handling
- Fixed `Cryptor::decrypt()` (26 lines) — frame-length-based decryption
- 7 new proto files for WiFi messages

**openauto layer (small):**
- New `WifiService` class — reads SSID/password from `/etc/hostapd/hostapd.conf`, sends response
- Updated `ServiceFactory` — instantiates WifiService alongside BluetoothService

## Next Steps

1. **Ghidra: Decompile WirelessServiceChannel::messageHandler()** — confirm the switch statement and which message IDs it handles
2. **Ghidra: Decompile MessageInStream::receiveFrameHeaderHandler()** — check if BlueWave fixed interleaving
3. **Design fix approach** — now that root cause is known (message ID mismatch), options are:
   a. **Binary patch libaasdk.so** — change the switch constant from 0x0002 to 0x8001 (smallest change)
   b. **Binary patch libaasdk_proto.so** — change the enum value for WIRELESS_CREDENTIALS_REQUEST
   c. **LD_PRELOAD hook** — intercept messageHandler, add case for 0x8001
   d. **Rebuild libaasdk.so** — merge SonOfGib's fix into BlueWave's codebase
4. **Verify WirelessCredentialsResponse proto fields** — confirm BlueWave's response message has the fields AA expects
