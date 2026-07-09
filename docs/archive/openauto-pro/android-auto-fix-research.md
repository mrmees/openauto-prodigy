# Android Auto Fix Research

## The Problem

Android Auto versions > 12.6 (released ~Sept 2024) broke compatibility with OpenAuto Pro, Crankshaft, and all openauto-based head units. Symptoms: the screen shows "Lock" or goes blank after phone connection — no video output from AA.

## Root Cause (identified by SonOfGib)

**The WiFi service is the culprit.**

When openauto announces its capabilities to Android Auto during service discovery, it declares WiFi channel support. Starting with AA 12.7+, Android Auto began sending a new message on this WiFi channel that openauto doesn't understand. Since openauto's `WifiService` had no message handler (it was essentially a stub), it silently ignored the message. Android Auto interpreted this silence as a failure and refused to send video.

### The specific message AA sends:
```
[AaSdk] [MessageInStream::receiveFrameHeaderHandler] Processing Frame Header: Ch (null) Fr BULK Enc ENCRYPTED Msg CONTROL
[AaSdk] [MessageInStream] Could not find existing message.
[AaSdk] [MessageInStream::receiveFramePayloadHandler] Message contents [6] 00070800100E
```

This is a **WiFi Security/Credentials Request** — AA is asking the head unit for WiFi credentials (SSID, password, security mode) to set up wireless AA.

## The Fix

SonOfGib implemented the fix in two parts:

### 1. aasdk changes ([SonOfGib/aasdk@284c366](https://github.com/SonOfGib/aasdk/commit/284c36661c870bf0a557431c559490608e8cb898))

- Added `WIFIServiceChannel` class (new channel handler for WiFi messages)
- Added `IWIFIServiceChannelEventHandler` interface
- Added `WifiChannelMessageIdsEnum.proto` (CREDENTIALS_REQUEST = 0x8001, CREDENTIALS_RESPONSE = 0x8002)
- Added `WifiSecurityRequestMessage.proto` (empty message — just a request trigger)
- Fixed `WifiSecurityResponseMessage.proto`:
  - Renamed `WifiSecurityReponse` → `WifiSecurityResponse` (typo fix)
  - Reordered fields: key=1, security_mode=2, ssid=3, supported_wifi_channels=4, access_point_type=5
  - Changed fields from `required` to `optional`
- Fixed `VideoFPSEnum.proto`: swapped 30fps and 60fps values (were backwards)
- Fixed `VideoFocusRequestMessage.proto`: made fields optional instead of required
- Fixed `ChannelDescriptorData.proto`: WiFi channel ID changed from 16 to 14

### 2. openauto changes ([SonOfGib/openauto@e3aa777](https://github.com/SonOfGib/openauto/commit/e3aa777467e14e15a011e3262c521b2c388de9af))

- Rewrote `WifiService` to properly implement `IWIFIServiceChannelEventHandler`
- WifiService now:
  - Creates a `WIFIServiceChannel` and listens for messages
  - Handles `onChannelOpenRequest` — sends channel open response
  - Handles `onWifiSecurityRequest` — responds with WiFi credentials from hostapd config
- Re-enabled WiFi service in `ServiceFactory` (was previously commented out as a workaround)
- Added configurable boost logging via `openauto-logs.ini`

## Applying to OpenAuto Pro

This is the challenge. OpenAuto Pro's `autoapp` binary is a monolithic compiled executable. The fix requires changes to both `libaasdk.so` and the `autoapp` binary itself.

### Potential approaches:

1. **Replace libaasdk.so + libaasdk_proto.so** — Build SonOfGib's fixed aasdk against the same Boost 1.67 / protobuf 17 / OpenSSL 1.1 that OAP uses, and swap the shared libraries. This handles the protocol-level fix but won't add the WifiService handler to autoapp.

2. **LD_PRELOAD hook** — Intercept the WifiService's `fillFeatures()` call to either disable WiFi channel announcement entirely, or inject a proper handler. This is hacky but doesn't require rebuilding autoapp.

3. **Binary patch** — Find the WiFi service feature registration in autoapp's binary and NOP it out, effectively disabling WiFi channel support. AA won't send the problematic message if the channel isn't advertised. This is the simplest fix but loses wireless AA capability.

4. **Full rebuild** — Rebuild the entire application using the recovered QML, proto definitions, and SonOfGib's fixed aasdk/openauto as a foundation. This is the long-term solution.

## Key Forks & References

- **SonOfGib/openauto** branch `sonofgib-newdev` — Working fix, confirmed with AA 12.8-13.1
- **SonOfGib/aasdk** branch `sonofgib-newdev` — Protocol fixes
- **SonOfGib/crankshaft** — Pre-built image with fix applied
- **opencardev/crankshaft#680** — Full discussion thread
- **sjdean** in the thread — Working on proto 1.6/1.7 updates, additional WiFi investigation

## Quick Fix for OAP (Binary Patch)

The simplest immediate fix would be to binary-patch `autoapp` to skip WiFi service registration in `ServiceFactory::create()`. The `fillFeatures` call that adds the WiFi channel descriptor is what triggers AA's new behavior. If we can find and NOP the `add_channels()` call for WiFi in the binary, AA will never know the head unit supports WiFi and won't send the problematic message.

This needs Ghidra analysis to locate the exact bytes to patch.

## Timeline

- Sept 2024: AA 12.7+ breaks all openauto-based head units
- Oct 14, 2024: SonOfGib identifies WiFi service as the cause
- Oct 18, 2024: Initial fix (commenting out WiFi service entirely)
- Oct 29, 2024: Proper fix with WiFi channel handler implemented
- Nov 7, 2024: Confirmed working with AA 13.1 / Android 14
