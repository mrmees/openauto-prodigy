# Wireless Android Auto Initial Handshake Findings

## Scope

This document captures initial wireless Android Auto pairing/bring-up behavior from:

- `https://github.com/nisargjhaveri/WirelessAndroidAutoDongle`
- Local checkout used for analysis: `/tmp/WirelessAndroidAutoDongle`

## Implementation Summary

The dongle (`aawgd`) uses BlueZ `ProfileManager1` to expose an Android Auto Wireless RFCOMM profile, then performs a short protobuf handshake over RFCOMM to bootstrap Wi-Fi credentials, and finally switches to TCP over Wi-Fi for AA data forwarding to USB accessory mode.

## Initial Pairing and Bring-Up Flow

1. Daemon starts TCP server for AA data path.
- Default endpoint comes from config/env: IP `10.0.0.1`, port `5288`.

2. Bluetooth setup and profile registration.
- Adapter alias set to `AndroidAuto-Dongle-*` or `WirelessAADongle-*`.
- Adapter set discoverable + pairable.
- Registers BlueZ profile:
  - UUID: `4de17a00-52cb-11e6-bdf4-0800200c9a66`
  - Role: `server`
  - Channel: `8`
- Also starts BLE advertising for same UUID.

3. Phone pairs over Bluetooth and opens RFCOMM.
- BlueZ invokes `org.bluez.Profile1.NewConnection`.
- App receives an RFCOMM file descriptor from BlueZ.

4. RFCOMM handshake (protobuf framed).
- Frame format: `[2-byte payload len][2-byte msgId][payload]` (big-endian).
- Sequence used by dongle:
  - HU -> phone: `WifiStartRequest` (`msgId=1`) with AP IP + TCP port.
  - Phone -> HU: `WifiInfoRequest` (`msgId=2`).
  - HU -> phone: `WifiInfoResponse` (`msgId=3`) with SSID/password/BSSID/security.
  - HU then reads two more control messages (typically start/connect status).

5. Phone joins Wi-Fi AP using provided credentials.

6. Phone opens TCP connection to dongle.
- Dongle accepts TCP client.
- Dongle opens `/dev/usb_accessory`.
- Dongle forwards data bidirectionally: TCP <-> USB (AA session traffic).

## Wi-Fi Credential Payload Defaults

From runtime config (`Config::getWifiInfo()`):

- SSID: `AAWirelessDongle`
- Password: `ConnectAAWirelessDongle`
- BSSID: `wlan0` MAC
- Security mode: `WPA2_PERSONAL`
- AP type: `DYNAMIC`
- Proxy IP: `10.0.0.1`
- Proxy port: `5288`

## Evidence (Key Files)

- `/tmp/WirelessAndroidAutoDongle/aa_wireless_dongle/package/aawg/src/aawgd.cpp`
- `/tmp/WirelessAndroidAutoDongle/aa_wireless_dongle/package/aawg/src/bluetoothHandler.cpp`
- `/tmp/WirelessAndroidAutoDongle/aa_wireless_dongle/package/aawg/src/bluetoothProfiles.cpp`
- `/tmp/WirelessAndroidAutoDongle/aa_wireless_dongle/package/aawg/src/proxyHandler.cpp`
- `/tmp/WirelessAndroidAutoDongle/aa_wireless_dongle/package/aawg/src/common.cpp`
- `/tmp/WirelessAndroidAutoDongle/README.md`

---

# Packet-Level Validation Checklist

Use this when debugging wireless AA setup failures, especially around SDP/RFCOMM bootstrap.

## 0) Prep

1. On the dongle, clear old logs and restart service.
```bash
sudo systemctl restart aawgd bluetooth
sudo journalctl -u aawgd -u bluetooth -n 100 --no-pager
```

2. On phone, clear Bluetooth logs and start fresh capture.
```bash
adb logcat -c
adb logcat -b all | tee /tmp/phone-wireless-aa.log
```

3. On dongle, start HCI trace.
```bash
sudo btmon | tee /tmp/dongle-btmon.log
```

## 1) Profile Exposure / SDP Discovery

Goal: confirm phone discovers AA UUID and extracts RFCOMM channel.

1. Pair/reconnect from phone.
2. In phone logcat, verify:
- Search UUID contains `4de17a00-52cb-11e6-bdf4-0800200c9a66`.
- SDP callback success with non-zero records.
- No `invalid length for discovery attribute`.
3. In dongle logs, verify profile registration happened before connect:
- `Bluetooth AA Wireless profile active`.

Fail signature:
- `invalid length for discovery attribute`
- `Record count: 0`

Interpretation:
- SDP record encoding/attribute type mismatch, not RFCOMM transport failure.

## 2) RFCOMM Channel Open

Goal: confirm RFCOMM session is actually established.

1. In dongle logs:
- `AA Wireless NewConnection`
- FD printed in `Path: ..., fd: ...`
2. In phone logs:
- State transitions to RFCOMM connecting/connected.

Fail signature:
- No `NewConnection` callback.

Interpretation:
- Phone discovered service but did not/could not open RFCOMM (channel mismatch or profile issue).

## 3) RFCOMM Message Exchange

Goal: validate handshake message ordering and framing.

Expected sequence:

1. Dongle sends `WifiStartRequest` (id `1`).
2. Dongle reads `WifiInfoRequest` (id `2`).
3. Dongle sends `WifiInfoResponse` (id `3`).
4. Dongle reads two follow-up messages.

Validation:

1. In dongle logs check these exact lines:
- `Sending WifiStartRequest (...)`
- `Read WifiInfoRequest`
- `Sending WifiInfoResponse (...)`
2. If step 2 is missing, phone rejected HU bootstrap message or closed RFCOMM early.

## 4) Wi-Fi Join and TCP Bring-Up

Goal: confirm transition from BT control channel to Wi-Fi TCP.

1. In dongle logs:
- `Tcp server listening on <port>`
- `Tcp server accepted connection`
2. Validate phone associated to AP (SSID/BSSID expected).
3. Confirm USB accessory open:
- `Opening usb accessory`
- `Forwarding data between TCP and USB`

Fail signature:
- RFCOMM success but no TCP connect.

Interpretation:
- Wrong SSID/password/BSSID/security mode, AP not reachable, or phone blocked join.

## 5) Data Path Stability

Goal: ensure AA traffic continuously forwards after bootstrap.

1. Look for repeated forward-loop errors:
- `Read from TCP failed ...`
- `Write to USB failed ...`
2. Check if forwarding stops immediately:
- `Forwarding stopped`

Interpretation:
- Session started but data path/USB side failed (not SDP issue).

## Quick Triage Mapping

- `SDP invalid length` + `Record count: 0`:
  - Fix SDP record structure/attribute encoding first.
- SDP success + no `NewConnection`:
  - Check RFCOMM channel/profile registration consistency.
- `NewConnection` + no `WifiInfoRequest`:
  - Inspect first protobuf frame format and payload.
- RFCOMM handshake success + no TCP accept:
  - Inspect Wi-Fi credentials and AP setup.
- TCP accepted + immediate forward errors:
  - Inspect USB accessory gadget path and forwarding loop.

---

# BlueZ 5.82 + `--compat` Runbook (Debian Trixie)

This runbook is tailored for your environment:

- Raspberry Pi 4
- BlueZ `5.82`
- Debian Trixie
- Android phone as AA wireless client

## A) One-Time Environment Sanity

1. Verify BlueZ version.
```bash
bluetoothd -v
```

2. Verify bluetoothd is running with `--compat`.
```bash
ps -ef | rg '[b]luetoothd'
```
Expected: command line includes `--compat` or `-C`.

3. Verify legacy SDP UNIX socket exists (compat mode).
```bash
ls -l /var/run/sdp
```

4. Verify BR/EDR stack is enabled on controller.
```bash
btmgmt info
```
Expected: controller not in LE-only mode.

## B) Start Clean Capture Set

Open 3 terminals.

1. Terminal 1: phone logs.
```bash
adb logcat -c
adb logcat -b all | tee /tmp/aa-phone.log
```

2. Terminal 2: btmon on Pi.
```bash
sudo btmon | tee /tmp/aa-btmon.log
```

3. Terminal 3: service + daemon logs.
```bash
sudo journalctl -f -u bluetooth -u aawgd | tee /tmp/aa-daemons.log
```

Then trigger pairing/projection attempt.

## C) Verify Profile Registration Path

1. Check D-Bus profile registration events/errors.
```bash
rg -n "RegisterProfile|profile|AA Wireless|Failed to register|RFCOMM server failed" /tmp/aa-daemons.log
```

2. Optional check via D-Bus monitor during a fresh restart.
```bash
sudo dbus-monitor --system "sender='org.bluez'" | tee /tmp/aa-dbus-bluez.log
```

Decision:

- If `RegisterProfile` fails or RFCOMM server bind fails, stop and fix registration/bind conflict.
- If registration succeeds, continue to SDP discovery checks.

## D) SDP Discovery Validation (Phone-Side Truth)

Phone-side parsing result is authoritative for this failure class.

```bash
rg -n "sdp_search_callback|invalid length for discovery attribute|Record count|4de17a00-52cb-11e6-bdf4-0800200c9a66" /tmp/aa-phone.log
```

Interpretation:

- `invalid length for discovery attribute` + `Record count: 0`:
  - SDP record is malformed for Android parser.
- `Record count: 1` and no invalid-length errors:
  - SDP parsing is good; move to RFCOMM/open/channel checks.

## E) RFCOMM Open Validation

1. Phone-side connect attempt/open.
```bash
rg -n "WIRELESS_CONNECTING_RFCOMM|CONNECTED_RFCOMM|RFCOMM_CreateConnection|scn:" /tmp/aa-phone.log
```

2. Dongle-side connection callback.
```bash
rg -n "AA Wireless NewConnection|Bluetooth launch sequence completed|fd:" /tmp/aa-daemons.log
```

Interpretation:

- Phone attempts RFCOMM but no `NewConnection` on Pi:
  - channel/profile mismatch or BlueZ profile callback path issue.
- `NewConnection` present:
  - move to control-message handshake.

## F) RFCOMM Control Handshake Validation

```bash
rg -n "WifiStartRequest|WifiInfoRequest|WifiInfoResponse|Expected WifiInfoRequest|Abort" /tmp/aa-daemons.log
```

Expected order:

1. `Sending WifiStartRequest`
2. `Read WifiInfoRequest`
3. `Sending WifiInfoResponse`

Interpretation:

- If step 2 never appears:
  - phone rejected/did not accept HU bootstrap frame or disconnected early.

## G) Wi-Fi and TCP Handoff Validation

1. TCP handoff in daemon logs.
```bash
rg -n "Tcp server listening|Tcp server accepted connection|Opening usb accessory|Forwarding data between TCP and USB" /tmp/aa-daemons.log
```

2. Optional AP-side checks (on Pi).
```bash
ip -4 addr show wlan0
iw dev wlan0 info
```

Interpretation:

- RFCOMM success + no TCP accept:
  - Wi-Fi credential/join path failure.
- TCP accept + no sustained forwarding:
  - USB accessory/data-path issue.

## H) Fast Branching Logic

Use this exact branch order:

1. SDP parser errors on phone?
  - Yes: fix SDP record format first.
2. No parser errors, but no RFCOMM `NewConnection` on Pi?
  - Fix profile/channel/open path.
3. RFCOMM connected, but no WifiInfoRequest?
  - Inspect RFCOMM control frame structure and timing.
4. Control handshake done, but no TCP accept?
  - Fix Wi-Fi join credentials/AP state.
5. TCP accepted, but forwarding unstable?
  - Fix USB accessory and stream forwarding.

## I) High-Signal Greps (Copy/Paste)

Phone:
```bash
rg -n "invalid length for discovery attribute|sdp_search_callback|Record count|WIRELESS_CONNECTING_RFCOMM|CONNECTED_RFCOMM|WifiStartRequest|WifiInfoRequest|WifiInfoResponse" /tmp/aa-phone.log
```

Pi daemon:
```bash
rg -n "AA Wireless profile active|NewConnection|WifiStartRequest|WifiInfoRequest|WifiInfoResponse|Tcp server accepted connection|Forwarding data between TCP and USB|failed|Abort" /tmp/aa-daemons.log
```

btmon quick scan:
```bash
rg -n "RFCOMM|SDP|L2CAP|PSM 0x0001|PSM 0x0003|Connect Req|Config Req|SABM|UA" /tmp/aa-btmon.log
```
