# Protocol Audit Fix Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix 15 issues found by auditing openauto-prodigy source code against the Android Auto protocol reference (`reference/android-auto-protocol.md`).

**Architecture:** Fixes are organized into 4 tiers by priority. Tier 1 blocks the first hardware test on the Pi. Tier 2 blocks a working AA session. Tier 3 improves usability. Tier 4 is future/nice-to-have.

**Tech Stack:** C++17, Qt 6.4/6.8, Boost.Asio, protobuf (proto2 via SonOfGib aasdk), FFmpeg, hostapd

**Reference:** `/home/matt/claude/reference/android-auto-protocol.md` — the authoritative protocol doc.

**Repo:** `/home/matt/claude/personal/openautopro/openauto-prodigy/`

---

## Tier 1 — Blocks First Hardware Test

These must be fixed before the Pi can even attempt a wireless AA connection.

---

### Task 1: Fix hostapd to use 5GHz WiFi

**Why:** The CYW43455 combo chip on Pi 4/5 shares an antenna for WiFi and Bluetooth. Running hostapd on 2.4GHz (channel 6) causes severe BT coexistence interference — phones can't maintain both BT RFCOMM and WiFi simultaneously. Android Auto requires 5GHz. See protocol reference Section 4.

**Files:**
- Modify: `docs/wireless-setup.md`

**Step 1: Edit the hostapd configuration in wireless-setup.md**

Find the hostapd.conf block and change:
```
# BEFORE (broken):
hw_mode=g
channel=6

# AFTER (correct):
hw_mode=a
channel=36
ieee80211n=1
ieee80211ac=1
wmm_enabled=1
```

Also add a note explaining why 5GHz is mandatory (BT coex on CYW43455).

**Step 2: Update the country_code guidance**

Add a note that 5GHz channels require `country_code=US` (or appropriate locale) and `ieee80211d=1` to be set, or hostapd will refuse to start on DFS channels. Channel 36 is non-DFS in most regions.

**Step 3: Commit**

```bash
git add docs/wireless-setup.md
git commit -m "fix: hostapd must use 5GHz for BT coexistence on Pi 4/5"
```

---

### Task 2: Rewrite RFCOMM handshake to match protocol

**Why:** The current `BluetoothDiscoveryService` uses wrong message IDs and proto types. The phone won't complete the credential exchange. This is the most critical code fix.

**Current (broken) flow:**
1. Phone connects via RFCOMM
2. HU sends `WifiInfoRequest` (msgId=1) with IP+port → wrong name, but correct content
3. HU expects `WifiSecurityRequest` (msgId=2) → wrong, phone sends empty `WifiInfoRequest`
4. HU sends `WifiSecurityResponse` (msgId=3) with SSID+key → wrong name, close content
5. HU expects `WifiInfoResponse` (msgId=7) → wrong ID, should be msgId=6 or 7

**Correct flow (from protocol reference Section 2.1 + aa-proxy-rs):**
1. Phone connects via RFCOMM
2. HU → Phone: `WifiStartRequest` (msgId=1) `{ip_address, port}`
3. Phone → HU: `WifiInfoRequest` (msgId=2) `(empty payload)`
4. HU → Phone: `WifiInfoResponse` (msgId=3) `{ssid, key, security_mode, access_point_type}`
5. Phone → HU: `WifiStartResponse` (msgId=6) — acknowledgement
6. Phone → HU: `WifiConnectionStatus` (msgId=7) `{status}` — WiFi connected

**Files:**
- Modify: `src/core/aa/BluetoothDiscoveryService.hpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.cpp`

**Step 1: Check which proto files exist in aasdk**

Run:
```bash
find libs/aasdk -name "Wifi*" -o -name "wifi*" | head -20
```

We need to know the exact proto message names available. The SonOfGib fork uses:
- `WifiInfoRequestMessage.pb.h` / `WifiInfoResponseMessage.pb.h`
- `WifiSecurityRequestMessage.pb.h` / `WifiSecurityResponseMessage.pb.h`

These may not perfectly match the 7-message protocol. We may need to use existing protos with the correct message IDs, or define raw protobuf encoding for missing message types.

**Step 2: Update message ID constants in BluetoothDiscoveryService.cpp**

Replace:
```cpp
// BEFORE:
static constexpr uint16_t kMsgWifiInfoRequest = 1;
static constexpr uint16_t kMsgWifiSecurityRequest = 2;
static constexpr uint16_t kMsgWifiSecurityResponse = 3;
static constexpr uint16_t kMsgWifiInfoResponse = 7;

// AFTER:
static constexpr uint16_t kMsgWifiStartRequest = 1;    // HU -> Phone: IP + port
static constexpr uint16_t kMsgWifiInfoRequest = 2;      // Phone -> HU: "give me credentials" (empty)
static constexpr uint16_t kMsgWifiInfoResponse = 3;      // HU -> Phone: SSID + key + security
static constexpr uint16_t kMsgWifiVersionRequest = 4;    // Phone -> HU: version check (optional)
static constexpr uint16_t kMsgWifiVersionResponse = 5;   // HU -> Phone: version reply (optional)
static constexpr uint16_t kMsgWifiStartResponse = 6;     // Phone -> HU: acknowledgement
static constexpr uint16_t kMsgWifiConnectionStatus = 7;  // Phone -> HU: WiFi connected
```

**Step 3: Rewrite onClientConnected() to send WifiStartRequest (msgId=1)**

The initial message the HU sends on RFCOMM connect should use msgId=1 with IP and port. The existing `WifiInfoRequest` proto has `ip_address` and `port` fields, which matches `WifiStartRequest` content. Use it with the correct message ID:

```cpp
void BluetoothDiscoveryService::onClientConnected()
{
    // ... existing socket setup code ...

    // Step 1: Send WifiStartRequest with our IP and TCP port
    aasdk::proto::messages::WifiInfoRequest startReq;
    startReq.set_ip_address(localIp);
    startReq.set_port(config_->tcpPort());

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Sending WifiStartRequest: ip=" << localIp
                            << " port=" << config_->tcpPort();
    sendMessage(startReq, kMsgWifiStartRequest);  // msgId=1, not kMsgWifiInfoRequest
}
```

**Step 4: Rewrite readSocket() switch to handle correct message IDs**

```cpp
switch (messageId) {
case kMsgWifiInfoRequest:       // msgId=2: Phone wants credentials
    handleWifiInfoRequest();    // renamed from handleWifiSecurityRequest
    break;
case kMsgWifiVersionRequest:    // msgId=4: Phone version check (optional)
    handleWifiVersionRequest(buffer_, length);
    break;
case kMsgWifiStartResponse:     // msgId=6: Phone acknowledges
    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Phone acknowledged WifiStartRequest";
    break;
case kMsgWifiConnectionStatus:  // msgId=7: Phone connected to WiFi
    handleWifiConnectionStatus(buffer_, length);
    break;
default:
    BOOST_LOG_TRIVIAL(warning) << "[BTDiscovery] Unknown message ID: " << messageId;
    break;
}
```

**Step 5: Rewrite handleWifiInfoRequest (was handleWifiSecurityRequest)**

This responds with msgId=3 containing WiFi credentials. Use `WifiSecurityResponse` proto (it has the right fields: ssid, key, security_mode, access_point_type):

```cpp
void BluetoothDiscoveryService::handleWifiInfoRequest()
{
    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Phone requested WiFi credentials";

    aasdk::proto::messages::WifiSecurityResponse response;
    response.set_ssid(config_->wifiSsid().toStdString());
    response.set_key(config_->wifiPassword().toStdString());
    response.set_security_mode(
        aasdk::proto::messages::WifiSecurityResponse_SecurityMode_WPA2_PERSONAL);
    response.set_access_point_type(
        aasdk::proto::messages::WifiSecurityResponse_AccessPointType_STATIC);

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Sending WifiInfoResponse: ssid="
                            << config_->wifiSsid().toStdString();
    sendMessage(response, kMsgWifiInfoResponse);  // msgId=3
}
```

**Step 6: Replace handleWifiInfoResponse with handleWifiConnectionStatus**

Rename and update to handle msgId=7 (connection status) instead of msgId=7 with the old name:

```cpp
void BluetoothDiscoveryService::handleWifiConnectionStatus(
    const QByteArray& data, uint16_t length)
{
    aasdk::proto::messages::WifiInfoResponse msg;
    if (!msg.ParseFromArray(data.data() + 4, length)) {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] Failed to parse WifiConnectionStatus";
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] WifiConnectionStatus: "
                            << msg.ShortDebugString();

    if (msg.status() == aasdk::proto::messages::WifiInfoResponse_Status_STATUS_SUCCESS) {
        BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Phone connected to WiFi!";
        emit phoneWillConnect();
    } else {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] Phone WiFi connection failed: "
                                 << msg.status();
        emit error(QString("Phone WiFi connection failed (status %1)").arg(msg.status()));
    }
}
```

**Step 7: Update header file**

Update `BluetoothDiscoveryService.hpp` private method signatures to match:
- `handleWifiInfoRequest()` (no params — payload is empty)
- `handleWifiConnectionStatus(const QByteArray&, uint16_t)`
- Optionally add `handleWifiVersionRequest(const QByteArray&, uint16_t)`

**Step 8: Build and verify compilation**

```bash
cd build && cmake .. && make -j$(nproc) 2>&1 | tail -20
```

Expected: Clean compilation (BT code is `#ifdef HAS_BLUETOOTH` guarded, so WSL2 build won't compile it, but syntax should be verified).

**Step 9: Commit**

```bash
git add src/core/aa/BluetoothDiscoveryService.cpp src/core/aa/BluetoothDiscoveryService.hpp
git commit -m "fix: rewrite RFCOMM handshake to match AA wireless protocol

Correct message IDs: 1=WifiStartRequest, 2=WifiInfoRequest,
3=WifiInfoResponse, 6=WifiStartResponse, 7=WifiConnectionStatus.
Previous code used wrong proto type names and message IDs."
```

---

### Task 3: Change default TCP port to 5288

**Why:** Current code already uses 5288 as default, which matches aa-proxy-rs. The protocol reference confirms 5288 is standard for aa-proxy-rs. However, the port is communicated dynamically via RFCOMM WifiStartRequest (msgId=1), so the exact default matters less than correctness of the handshake (Task 2). Verify this is consistent.

**Files:**
- Verify: `src/core/Configuration.hpp` (check `m_tcpPort` default)
- Verify: `docs/wireless-setup.md` (check documented port)

**Step 1: Verify port is consistent across code and docs**

Check that `Configuration.hpp` has `m_tcpPort = 5288` and `wireless-setup.md` documents the same port. If already consistent, this is a no-op.

**Step 2: Commit if changes were needed**

```bash
git add -A && git commit -m "fix: ensure TCP port 5288 is consistent across code and docs"
```

---

## Tier 2 — Blocks Working AA Session

These prevent a successful AA session even if the wireless handshake works.

---

### Task 4: Respond to VideoFocusRequest

**Why:** When the phone sends `VideoFocusRequest`, we must respond with `VideoFocusIndication` containing `FOCUSED` mode. Without this, the phone may pause or stop sending video frames. See protocol reference Section 3 (channel lifecycle).

**Files:**
- Modify: `src/core/aa/ServiceFactory.cpp` (VideoServiceStub::onVideoFocusRequest)

**Step 1: Find the onVideoFocusRequest handler**

In `ServiceFactory.cpp`, locate the `VideoServiceStub::onVideoFocusRequest` method. It currently only logs.

**Step 2: Add VideoFocusIndication response**

```cpp
void onVideoFocusRequest(const aasdk::proto::messages::VideoFocusRequest& request) override
{
    BOOST_LOG_TRIVIAL(info) << "[VideoService] VideoFocusRequest: "
                            << request.ShortDebugString();

    aasdk::proto::messages::VideoFocusIndication indication;
    indication.set_focus_mode(
        aasdk::proto::enums::VideoFocusMode::VIDEO_FOCUS_NATIVE);
    indication.set_unrequest(false);

    channel_->sendVideoFocusIndication(indication,
        [](const aasdk::error::Error& e) {
            if (e != aasdk::error::Error()) {
                BOOST_LOG_TRIVIAL(error) << "[VideoService] Failed to send VideoFocusIndication: "
                                         << e.what();
            }
        });
}
```

Note: Check the exact `sendVideoFocusIndication` method signature on the channel — it may be `sendVideoFocusIndicationMessage` or similar. Grep the aasdk channel header to confirm.

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/ServiceFactory.cpp
git commit -m "fix: respond to VideoFocusRequest with FOCUSED indication"
```

---

### Task 5: Set real Bluetooth adapter MAC address

**Why:** The Bluetooth channel's `adapter_address` is hardcoded to `"00:00:00:00:00:00"`. The phone uses this to verify the BT adapter identity. A null MAC will likely cause pairing or identity verification failures.

**Files:**
- Modify: `src/core/aa/ServiceFactory.cpp` (BluetoothServiceStub channel descriptor)

**Step 1: Read the real BT adapter MAC at runtime**

Add a helper to read the adapter MAC. On Linux, the default adapter MAC is available via:
```cpp
#ifdef HAS_BLUETOOTH
#include <QBluetoothLocalDevice>
// ...
QBluetoothLocalDevice localDevice;
std::string btMac = localDevice.address().toString().toStdString();
#else
std::string btMac = "00:00:00:00:00:00";
#endif
```

**Step 2: Use the runtime MAC in the channel descriptor**

Replace the hardcoded `"00:00:00:00:00:00"` with the runtime value.

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/ServiceFactory.cpp
git commit -m "fix: use real BT adapter MAC in Bluetooth channel descriptor"
```

---

### Task 6: Offer 720p video resolution

**Why:** The current code only offers 480p (800x480). The target display is 1024x600, which is closer to 720p. Offering only 480p means the phone renders at lower quality than the display can show. See protocol reference for supported resolutions.

**Files:**
- Modify: `src/core/aa/ServiceFactory.cpp` (video channel descriptor)

**Step 1: Find the video config in ServiceFactory.cpp**

Locate where `VideoConfigData` is populated. Currently it sets resolution to 800x480.

**Step 2: Add 720p as primary, keep 480p as fallback**

```cpp
auto* videoConfig = avData->mutable_video_config();
videoConfig->set_video_resolution(
    aasdk::proto::enums::VideoResolution::_720p);
videoConfig->set_video_fps(
    aasdk::proto::enums::VideoFPS::_60);
```

Note: Check the exact enum values in `VideoResolutionEnum.pb.h`. The phone negotiates the best resolution it supports from what we offer.

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/ServiceFactory.cpp
git commit -m "fix: offer 720p video resolution for 1024x600 display"
```

---

### Task 7: Increase max_unacked for video channel

**Why:** `max_unacked=1` means the phone must wait for an ACK after every single video frame before sending the next. This creates massive latency. Protocol reference suggests ~10 for video.

**Files:**
- Modify: `src/core/aa/ServiceFactory.cpp` (video AV setup response)

**Step 1: Find max_unacked setting for video**

In the video channel setup, find where `max_unacked` is set.

**Step 2: Change from 1 to 10**

```cpp
setupResponse.set_max_unacked(10);
```

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/ServiceFactory.cpp
git commit -m "fix: increase video max_unacked from 1 to 10 to reduce latency"
```

---

## Tier 3 — Needed for Usable Experience

These don't prevent connection but make the AA experience incomplete or broken.

---

### Task 8: Fix audio focus response logic

**Why:** Current code always responds with `AUDIO_FOCUS_GAIN` regardless of request type. Should return contextually: GAIN for media, GAIN_TRANSIENT for nav guidance, LOSS_TRANSIENT_CAN_DUCK for ducking. Incorrect focus responses cause audio to play over each other or cut out unexpectedly.

**Files:**
- Modify: `src/core/aa/AndroidAutoEntity.cpp` (onAudioFocusRequest handler)

**Step 1: Read the current audio focus handler**

Find `onAudioFocusRequest` in `AndroidAutoEntity.cpp`.

**Step 2: Implement contextual focus response**

Map the request's `audio_focus_type` to an appropriate response:
```cpp
void AndroidAutoEntity::onAudioFocusRequest(
    const aasdk::proto::messages::AudioFocusRequest& request)
{
    BOOST_LOG_TRIVIAL(info) << "[Entity] AudioFocusRequest: "
                            << request.ShortDebugString();

    aasdk::proto::messages::AudioFocusResponse response;

    // Mirror the request type — the HU grants what the phone asks for
    switch (request.audio_focus_type()) {
    case aasdk::proto::enums::AudioFocusType::GAIN:
        response.set_audio_focus_state(
            aasdk::proto::enums::AudioFocusState::AUDIO_FOCUS_STATE_GAIN);
        break;
    case aasdk::proto::enums::AudioFocusType::GAIN_TRANSIENT:
        response.set_audio_focus_state(
            aasdk::proto::enums::AudioFocusState::AUDIO_FOCUS_STATE_GAIN_TRANSIENT);
        break;
    case aasdk::proto::enums::AudioFocusType::GAIN_TRANSIENT_MAY_DUCK:
        response.set_audio_focus_state(
            aasdk::proto::enums::AudioFocusState::AUDIO_FOCUS_STATE_GAIN_TRANSIENT_MAY_DUCK);
        break;
    case aasdk::proto::enums::AudioFocusType::RELEASE:
        response.set_audio_focus_state(
            aasdk::proto::enums::AudioFocusState::AUDIO_FOCUS_STATE_LOSS);
        break;
    default:
        response.set_audio_focus_state(
            aasdk::proto::enums::AudioFocusState::AUDIO_FOCUS_STATE_GAIN);
        break;
    }

    controlChannel_->sendAudioFocusResponse(response,
        [this](const aasdk::error::Error& e) { onChannelSendError(e); });
}
```

Note: Verify exact enum names in aasdk proto headers — they may differ slightly (e.g., `AUDIO_FOCUS_GAIN` vs `AUDIO_FOCUS_STATE_GAIN`).

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/AndroidAutoEntity.cpp
git commit -m "fix: mirror audio focus request type in response instead of always GAIN"
```

---

### Task 9: Send sensor data (night mode + driving status)

**Why:** The sensor channel advertises NIGHT_DATA and DRIVING_STATUS in service discovery but never sends actual data. Without DRIVING_STATUS, the phone may restrict UI interactions (assumes parked). Without NIGHT_DATA, the phone can't switch to dark/light theme.

**Files:**
- Modify: `src/core/aa/ServiceFactory.cpp` (SensorServiceStub)

**Step 1: Find the SensorServiceStub onSensorStartRequest handler**

Currently it sends a start response but never sends sensor data.

**Step 2: Send initial sensor data after start response**

After sending the start response, send actual sensor data:

```cpp
// Send night mode data
aasdk::proto::messages::SensorData nightData;
nightData.set_sensor_type(aasdk::proto::enums::SensorType::NIGHT_DATA);
nightData.mutable_night_mode()->set_is_night(false);  // or read from system
// Send via channel

// Send driving status
aasdk::proto::messages::SensorData drivingData;
drivingData.set_sensor_type(aasdk::proto::enums::SensorType::DRIVING_STATUS);
drivingData.mutable_driving_status()->set_status(
    aasdk::proto::enums::DrivingStatus::UNRESTRICTED);
// Send via channel
```

Note: The exact protobuf field names need to be verified against the aasdk proto definitions. The key requirement is that we send both NIGHT_DATA and DRIVING_STATUS after the phone sends SensorStartRequest.

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/ServiceFactory.cpp
git commit -m "fix: send night mode and driving status sensor data"
```

---

### Task 10: Fix input channel keycodes

**Why:** Current keycodes are `{0, 3, 4}` where 0 is `KEYCODE_NONE` (useless). Missing `KEYCODE_MICROPHONE_1` (0x54 = 84) which is essential for voice commands. See protocol reference for keycode list.

**Files:**
- Modify: `src/core/aa/ServiceFactory.cpp` (InputServiceStub channel descriptor)

**Step 1: Find the input keycode configuration**

Locate where supported keycodes are set in the input channel descriptor.

**Step 2: Replace with useful keycodes**

```cpp
// Standard head unit keycodes:
// KEYCODE_HOME = 3
// KEYCODE_BACK = 4
// KEYCODE_CALL = 5
// KEYCODE_ENDCALL = 6
// KEYCODE_DPAD_UP = 19
// KEYCODE_DPAD_DOWN = 20
// KEYCODE_DPAD_LEFT = 21
// KEYCODE_DPAD_RIGHT = 22
// KEYCODE_DPAD_CENTER = 23
// KEYCODE_MICROPHONE_1 = 84 (0x54)
// KEYCODE_MEDIA_PLAY_PAUSE = 85
// KEYCODE_SCROLL_WHEEL = 65536 (0x10000)

// Remove KEYCODE_NONE (0), add MICROPHONE_1
inputData.add_supported_keycodes(3);   // HOME
inputData.add_supported_keycodes(4);   // BACK
inputData.add_supported_keycodes(84);  // MICROPHONE_1
```

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/ServiceFactory.cpp
git commit -m "fix: replace KEYCODE_NONE with MICROPHONE_1 in supported keycodes"
```

---

### Task 11: Respond to WiFi channel security request

**Why:** The WiFi service channel `onWifiSecurityRequest` handler only logs the request without sending a response. The phone expects a `WifiSecurityResponse` on this in-session channel (separate from the RFCOMM handshake WiFi exchange).

**Files:**
- Modify: `src/core/aa/ServiceFactory.cpp` (WIFIServiceStub)

**Step 1: Find the onWifiSecurityRequest handler**

Currently it just logs and doesn't respond.

**Step 2: Send WifiSecurityResponse with credentials**

```cpp
void onWifiSecurityRequest(
    const aasdk::proto::messages::WifiSecurityRequest& request) override
{
    BOOST_LOG_TRIVIAL(info) << "[WIFIService] WifiSecurityRequest: "
                            << request.ShortDebugString();

    aasdk::proto::messages::WifiSecurityResponse response;
    response.set_ssid(config_->wifiSsid().toStdString());
    response.set_key(config_->wifiPassword().toStdString());
    response.set_security_mode(
        aasdk::proto::messages::WifiSecurityResponse_SecurityMode_WPA2_PERSONAL);
    response.set_access_point_type(
        aasdk::proto::messages::WifiSecurityResponse_AccessPointType_STATIC);

    channel_->sendWifiSecurityResponse(response,
        [](const aasdk::error::Error& e) {
            if (e != aasdk::error::Error()) {
                BOOST_LOG_TRIVIAL(error) << "[WIFIService] Failed to send WifiSecurityResponse";
            }
        });
}
```

Note: Verify `sendWifiSecurityResponse` method exists on the WiFi channel. Grep aasdk headers.

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/ServiceFactory.cpp
git commit -m "fix: respond to in-session WiFi security request with credentials"
```

---

### Task 12: Add supported_pairing_methods to Bluetooth channel

**Why:** The Bluetooth channel descriptor has no `supported_pairing_methods`. Without this, the phone doesn't know how to pair with the head unit for phone calls/HFP.

**Files:**
- Modify: `src/core/aa/ServiceFactory.cpp` (BluetoothServiceStub channel descriptor)

**Step 1: Find the Bluetooth channel descriptor**

Locate where `BluetoothChannelData` is populated.

**Step 2: Add pairing methods**

```cpp
btData.add_supported_pairing_methods(
    aasdk::proto::enums::BluetoothPairingMethod::HFP);
```

Note: Verify exact enum name in aasdk protos. May be defined in `BluetoothPairingMethodEnum.pb.h` or similar.

**Step 3: Build and verify**

```bash
cd build && make -j$(nproc) 2>&1 | tail -10
```

**Step 4: Commit**

```bash
git add src/core/aa/ServiceFactory.cpp
git commit -m "fix: add HFP pairing method to Bluetooth channel descriptor"
```

---

## Tier 4 — Future / Investigation

These are lower priority and may require hardware testing to validate.

---

### Task 13: Add BLE advertisement for auto-reconnect

**Why:** Newer phones use BLE to find the head unit for auto-reconnect without a full BT Classic discovery scan. UUID: `9b3f6c10-a4d2-418e-a2b9-0700300de8f4`. This is a nice-to-have — the initial BT Classic discovery works without it.

**Files:**
- Modify: `src/core/aa/BluetoothDiscoveryService.cpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.hpp`

**Implementation notes:**
- Use `QLowEnergyController` and `QLowEnergyServiceData` to register a GATT service
- Advertise the BLE UUID alongside the existing RFCOMM SDP service
- Requires `qt6-connectivity-dev` (already a dependency for BT support)
- Defer until basic RFCOMM handshake is verified working on hardware

**Step 1: Implement BLE GATT service registration**

(Detailed implementation deferred — needs hardware testing first)

**Step 2: Build and verify**

**Step 3: Commit**

---

### Task 14: Fix channel count in CLAUDE.md

**Why:** CLAUDE.md says "8 AA channels" but the actual count is 10 (including control channel 0 and WiFi channel 14). Minor doc accuracy issue.

**Files:**
- Modify: `CLAUDE.md`

**Step 1: Update channel count**

Change "8 AA channels" to "10 AA channels" and list: CONTROL=0, INPUT=1, SENSOR=2, VIDEO=3, MEDIA_AUDIO=4, SPEECH_AUDIO=5, SYSTEM_AUDIO=6, AV_INPUT=7, BLUETOOTH=8, WIFI=14.

**Step 2: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: fix channel count (10 not 8) in CLAUDE.md"
```

---

### Task 15: Investigate AnnexB start code double-prepend

**Why:** `VideoDecoder.cpp` always prepends AnnexB start code `00 00 00 01` before each NAL unit. If aasdk already delivers NALUs with start codes, we'd be double-prepending, which corrupts the bitstream. Needs investigation with actual video data.

**Files:**
- Investigate: `src/core/aa/VideoDecoder.cpp`
- Investigate: aasdk video channel output format

**Step 1: Add diagnostic logging**

Before the start code prepend, log the first 4 bytes of the incoming data to check if start codes are already present:

```cpp
if (data.size() >= 4) {
    BOOST_LOG_TRIVIAL(debug) << "[VideoDecoder] First 4 bytes: "
        << std::hex << (int)data[0] << " " << (int)data[1]
        << " " << (int)data[2] << " " << (int)data[3];
}
```

**Step 2: Test with real phone connection**

Run on Pi, connect phone, check logs. If first bytes are already `00 00 00 01`, remove the prepend.

**Step 3: Fix if needed and commit**

---

## Summary

| Tier | Task | Finding | Severity |
|------|------|---------|----------|
| 1 | 1 | hostapd 2.4GHz → 5GHz | CRITICAL |
| 1 | 2 | RFCOMM handshake rewrite | CRITICAL |
| 1 | 3 | TCP port consistency check | CRITICAL |
| 2 | 4 | VideoFocusRequest response | IMPORTANT |
| 2 | 5 | BT adapter MAC placeholder | IMPORTANT |
| 2 | 6 | 720p video resolution | IMPORTANT |
| 2 | 7 | max_unacked too low | IMPORTANT |
| 3 | 8 | Audio focus always GAIN | IMPORTANT |
| 3 | 9 | Sensors never send data | IMPORTANT |
| 3 | 10 | Input keycodes wrong | IMPORTANT |
| 3 | 11 | WiFi stub no response | IMPORTANT |
| 3 | 12 | BT pairing methods missing | MINOR |
| 4 | 13 | BLE advertisement | MINOR |
| 4 | 14 | Doc channel count | MINOR |
| 4 | 15 | AnnexB investigation | MINOR |

**Estimated total: ~15 tasks, Tiers 1-3 are code changes, Tier 4 is investigation/future.**
