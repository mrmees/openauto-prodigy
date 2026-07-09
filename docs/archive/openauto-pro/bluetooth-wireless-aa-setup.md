# Bluetooth Wireless Android Auto — Complete Setup Guide

## What This Solves

Getting a Raspberry Pi (or any Linux device running BlueZ) to be discovered by an Android phone as a wireless Android Auto head unit. This covers everything from SDP record registration through WiFi handoff.

**Tested with:** BlueZ 5.82, Raspberry Pi 4, Moto G Play (Android 14), Samsung S25 Ultra. Date: 2024-02-24.

---

## The Problem Chain (and solutions)

### Problem 1: Android rejects BlueZ SDP records ("invalid length for discovery attribute")

**Root cause:** Android's `sdpu_compare_uuid_with_attr()` (in `system/bt/stack/sdp/sdp_utils.cc:760`) does **strict UUID size comparison**. If a 128-bit UUID search encounters a 16-bit UUID attribute (or vice versa), it returns false with "invalid length for discovery attribute."

BlueZ 5.82 encodes standard 16-bit UUIDs (PnP 0x1200, GAP 0x1800, GATT 0x1801, DevInfo 0x1200) in **128-bit form** in SDP records. Android searches for these as 16-bit → size mismatch → rejection. This was introduced as a security fix (CVE-related, commit `6afad4b`).

**Solution:** Remove the core BlueZ SDP records (handles 0x10000-0x10003) using the legacy SDP C API:

```c
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

bdaddr_t any = {{0}};
bdaddr_t local = {{0,0,0,0xff,0xff,0xff}};
sdp_session_t *session = sdp_connect(&any, &local, SDP_RETRY_IF_BUSY);

for (uint32_t h = 0x10000; h <= 0x10003; h++) {
    sdp_device_record_unregister_binary(session, &local, h);
}
```

Requires: `libbluetooth-dev`, BlueZ started with `--compat` flag (enables legacy SDP socket at `/var/run/sdp`).

### Problem 2: Mixed UUID sizes in AA SDP record

Even with core records removed, if the AA SDP record contains BOTH 128-bit UUIDs (the AA UUID) and 16-bit UUIDs (like SPP 0x1101), Android's iterator hits the size mismatch on the 16-bit ones.

**Solution:** The AA SDP record must contain ONLY the 128-bit AA UUID. No SPP UUID in ServiceClassIDList. No SPP ProfileDescriptorList.

```c
// ServiceClassIDList = [AA UUID ONLY]
uint128_t uuid128 = {{
    0x4d,0xe1,0x7a,0x00, 0x52,0xcb,0x11,0xe6,
    0xbd,0xf4,0x08,0x00, 0x20,0x0c,0x9a,0x66
}};
uuid_t aa_uuid;
sdp_uuid128_create(&aa_uuid, &uuid128);
sdp_list_t *cls = sdp_list_append(NULL, &aa_uuid);
sdp_set_service_classes(record, cls);

// Protocol: L2CAP + RFCOMM channel 8 (16-bit UUIDs OK here — they're protocol descriptors, not service class)
// NO ProfileDescriptorList — would add 16-bit SPP UUID
```

### Problem 3: Phone says "No profiles" and disconnects

After SDP discovery, Android's Bluetooth stack categorizes devices by known profiles (A2DP, HFP, etc.). If only the AA UUID is present and no standard profiles, the phone logs `"No profiles. Maybe we will connect later"` and drops the connection.

**Solution:** Register additional standard profiles via BlueZ ProfileManager1 D-Bus API:

- **HSP HS** (UUID `00001108-0000-1000-8000-00805f9b34fb`) — keeps BT connection alive
- **HFP AG** (UUID `0000111f-0000-1000-8000-00805f9b34fb`) — **REQUIRED** by Android Auto

### Problem 4: "WIRELESS_SETUP_FAILED_TO_START_NO_HFP_FROM_HU_PRESENCE"

The Android Auto app **requires an active HFP (Hands-Free Profile) connection** to the head unit before it will initiate wireless AA setup. This is how the phone distinguishes a car head unit from a random Bluetooth device.

**Solution:** Register HFP AG profile. Doesn't need to actually handle calls — just needs to accept the connection and keep the fd open.

### Problem 5: Socket from ProfileManager1 is non-blocking

When BlueZ hands you an fd via `NewConnection`, the socket is in non-blocking mode. Reading immediately after writing gets `EAGAIN`.

**Solution:** `sock.setblocking(True)` / `sock.settimeout(10.0)` immediately after `socket.fromfd()`.

### Problem 6: Wrong protobuf field encoding

The WiFi credential exchange uses protobuf. Hand-rolled encoding with wrong field numbers or wrong wire types causes the phone to reject the BSSID as invalid.

**Solution:** Use correct protobuf encoding per the proto definitions:

```protobuf
// WifiInfoResponse
message WifiInfoResponse {
    required string ssid = 1;            // field tag 0x0a (LEN)
    required string key = 2;             // field tag 0x12 (LEN)
    required string bssid = 3;           // field tag 0x1a (LEN) — NOT 0x18 (VARINT)!
    required SecurityMode security_mode = 4;   // field tag 0x20 (VARINT), WPA2_PERSONAL = 8
    required AccessPointType access_point_type = 5;  // field tag 0x28 (VARINT), DYNAMIC = 1
}
```

BSSID format: colon-separated uppercase MAC string, e.g. `"DC:A6:32:E7:5A:FE"`.

### Problem 7: Extra SDP records from system services

WirePlumber registers HFP/A2DP/AVRCP profiles via BlueZ D-Bus, creating additional SDP records with problematic encoding. Dual Bluetooth controllers (built-in + USB dongle) double all records.

**Solution:**
- Disable USB BT dongle: udev rule for CSR 0a12:0001
- Stop WirePlumber: `systemctl --user stop wireplumber`
- Disable all BlueZ plugins: `bluetoothd --compat -P *`

---

## Complete Working Configuration

### BlueZ Setup

```bash
# /lib/systemd/system/bluetooth.service — ExecStart line:
ExecStart=/usr/libexec/bluetooth/bluetoothd --compat -P *
```

- `--compat`: Enables legacy SDP socket at `/var/run/sdp` (needed for C SDP API)
- `-P *`: Disables ALL plugins (prevents auto-registration of problematic SDP records)

### Required Components (3 processes)

#### 1. SDP Record Manager (`sdp_clean`) — C program

Removes core BlueZ SDP records and registers a clean AA-only record. Must stay running (SDP record is deregistered when process exits).

Compile: `gcc -o sdp_clean sdp_clean.c -lbluetooth`

Key properties of the SDP record:
- ServiceClassIDList: AA UUID only (128-bit `4de17a00-52cb-11e6-bdf4-0800200c9a66`)
- ProtocolDescriptorList: L2CAP + RFCOMM channel 8
- BrowseGroupList: Public Browse Root
- ServiceName: "Android Auto Wireless"
- NO SPP UUID anywhere
- NO ProfileDescriptorList

#### 2. Profile Manager (`aa-combined.py`) — Python D-Bus

Registers three BlueZ profiles via ProfileManager1:

| Profile | UUID | Purpose |
|---------|------|---------|
| HFP AG | `0000111f-0000-1000-8000-00805f9b34fb` | **Required** — AA won't start without HFP |
| HSP HS | `00001108-0000-1000-8000-00805f9b34fb` | Keeps BT connection alive |
| AA RFCOMM | `4de17a00-52cb-11e6-bdf4-0800200c9a66` | Handles WiFi credential handshake |

All profiles set `RequireAuthentication=False`, `RequireAuthorization=False`.

HFP AG handler: just hold the fd open (don't close it).
AA handler: perform the 5-stage RFCOMM handshake (see below).

#### 3. BT Pairing Agent (`bt-agent.py`) — Python D-Bus

Auto-confirms pairing with `DisplayYesNo` capability.

### RFCOMM Handshake Protocol

Packet format: `[length:u16_be][msg_id:u16_be][protobuf_payload]`

| Stage | Direction | MsgID | Message | Notes |
|-------|-----------|-------|---------|-------|
| 1 | HU→Phone | 1 | WifiStartRequest | IP + port of TCP server |
| 2 | Phone→HU | 7 | WifiStartResponse | Status (0 = success) |
| 3 | Phone→HU | 2 | WifiInfoRequest | Empty or with cached info |
| 4 | HU→Phone | 3 | WifiInfoResponse | SSID, key, BSSID, security, AP type |
| 5 | Phone→HU | 6 | WifiConnectStatus | Status (0 = success, phone joined WiFi) |

After stage 5, phone connects via TCP to the IP:port from stage 1.

### WifiStartRequest Protobuf

```protobuf
message WifiStartRequest {
    required string ip_address = 1;  // e.g. "10.0.0.1"
    required uint32 port = 2;        // e.g. 5288
}
```

### WifiInfoResponse Protobuf

```protobuf
enum SecurityMode {
    UNKNOWN_SECURITY_MODE = 0;
    OPEN = 1;
    WEP_64 = 2;
    WEP_128 = 3;
    WPA_PERSONAL = 4;
    WPA2_PERSONAL = 8;       // ← use this
    WPA_WPA2_PERSONAL = 12;
}

enum AccessPointType {
    STATIC = 0;
    DYNAMIC = 1;             // ← use this
}

message WifiInfoResponse {
    required string ssid = 1;
    required string key = 2;
    required string bssid = 3;           // colon-separated uppercase MAC
    required SecurityMode security_mode = 4;
    required AccessPointType access_point_type = 5;
}
```

---

## End-to-End Connection Flow

```
Phone                              Pi (Head Unit)
  |                                   |
  |--- BT Pair (DisplayYesNo) -----→ |  bt-agent auto-confirms
  |                                   |
  |--- SDP Service Search ----------→ |  Returns AA UUID (128-bit only)
  |                                   |
  |--- HFP AG Connect -------------→ |  DummyProfile holds fd open
  |                                   |
  |--- RFCOMM Connect (ch 8) ------→ |  AAProfile.NewConnection()
  |                                   |
  |←-- WifiStartRequest (msgId=1) -- |  IP=10.0.0.1, port=5288
  |                                   |
  |--- WifiStartResponse (msgId=7) → |  status=SUCCESS
  |                                   |
  |--- WifiInfoRequest (msgId=2) --→ |  (empty)
  |                                   |
  |←-- WifiInfoResponse (msgId=3) -- |  SSID, key, BSSID, WPA2, DYNAMIC
  |                                   |
  |... joins WiFi AP (wlan0) ...      |
  |                                   |
  |--- WifiConnectStatus (msgId=6) → |  status=0 (SUCCESS)
  |                                   |
  |--- TCP connect 10.0.0.1:5288 --→ |  AA protocol session begins
```

---

## Diagnostic Tools

### btmon (capture SDP exchange)
```bash
sudo btmon -w /tmp/btmon-capture.snoop &
sudo btmon > /tmp/btmon-text.log 2>&1 &
# After test: grep for "Service Search Attribute" in text log
```

### sdptool (verify SDP records)
```bash
sdptool browse local
# Should show ONLY the AA record (no core records, no SPP)
```

### Phone logcat (AA-specific)
```bash
adb logcat | grep -E 'GH\.|WPP|WIRELESS|bt_sdp|invalid length|record count'
```

Key success markers:
- `GH.WIRELESS.SETUP: State changed to CONNECTED_RFCOMM`
- `GH.WIRELESS.SETUP: Received WifiStartRequest`
- `GH.WIRELESS.SETUP: Info response received`
- `GH.WIRELESS.SETUP: State changed to WIFI_CONNECTING` (not `WIFI_INVALID_BSSID`)
- `WIRELESS_WIFI_SOCKET_CONNECTING` → needs TCP listener on 5288

Key failure markers:
- `invalid length for discovery attribute` → mixed UUID sizes in SDP
- `No profiles. Maybe we will connect later` → need HSP/HFP profiles
- `WIRELESS_SETUP_FAILED_TO_START_NO_HFP_FROM_HU_PRESENCE` → need HFP AG
- `WIFI_INVALID_BSSID` → wrong protobuf encoding for BSSID field
- `WIRELESS_RFCOMM_INFO_RESPONSE_MISSING` → socket non-blocking / timeout

---

## Reference: aa-proxy-rs Comparison

aa-proxy-rs (working Rust implementation) uses `bluer` crate for BlueZ profile registration — equivalent to ProfileManager1 D-Bus API. It registers:
- AA RFCOMM profile (UUID, channel 8, server role)
- HSP HS profile (compatibility trigger)

It does NOT remove core SDP records or use custom SDP XML. This works because:
1. Different phone models may be more tolerant of mixed UUID sizes
2. Different BlueZ versions may encode UUIDs differently
3. The system may have fewer background services creating problematic records

Our approach (legacy SDP + ProfileManager1 routing) is more robust across all Android versions.

---

## Files Reference

Working test scripts (on Pi at `/tmp/`):
- `sdp_clean.c` / `sdp_clean` — SDP record manager (C)
- `aa-combined.py` — Profile manager + RFCOMM handshake (Python)
- `bt-agent.py` — Pairing agent (Python)

Production implementation target:
- `openauto-prodigy/libs/open-androidauto/src/Bluetooth/BluetoothDiscoveryService.cpp`
