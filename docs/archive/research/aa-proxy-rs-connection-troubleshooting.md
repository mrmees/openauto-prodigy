# aa-proxy-rs Android Auto Connection Flow (for SDP/handshake troubleshooting)

## Scope
- Repository analyzed: `https://github.com/aa-proxy/aa-proxy-rs`
- Commit analyzed: `35b74aba48cc670a2060c0c5d4d8f82fad2d3af9` (`origin/main`)
- Goal: document the exact Bluetooth/Wi-Fi/USB connection sequence and provide a checklist for debugging `SDP record details are unparseable` issues.

## What aa-proxy-rs registers on Bluetooth
From `src/bluetooth.rs`:

- Android Auto RFCOMM profile:
  - UUID: `4de17a00-52cb-11e6-bdf4-0800200c9a66`
  - Name: `AA Wireless`
  - Channel: `8`
  - Role: `Server`
  - `require_authentication = false`
  - `require_authorization = false`
- Headset profile (used as compatibility trigger):
  - UUID: `00001108-0000-1000-8000-00805f9b34fb` (HSP HS)

Key code:
- `aa-proxy-rs-research/src/bluetooth.rs:41`
- `aa-proxy-rs-research/src/bluetooth.rs:101`
- `aa-proxy-rs-research/src/bluetooth.rs:105`
- `aa-proxy-rs-research/src/bluetooth.rs:114`

Important: aa-proxy-rs does **not** handcraft raw SDP XML in this code path. It uses `bluer` profile registration (`session.register_profile(...)`), which maps to BlueZ profile registration APIs.

## End-to-end connection sequence (actual runtime behavior)

1. Startup/runtime split:
- Main Tokio runtime handles BT/control.
- Separate `tokio_uring` runtime handles data path proxying.
- Shared `Notify` (`tcp_start`) synchronizes when phone should connect over Wi-Fi/TCP.

Key code:
- `aa-proxy-rs-research/src/main.rs:564`
- `aa-proxy-rs-research/src/main.rs:577`
- `aa-proxy-rs-research/src/main.rs:598`

2. Bluetooth adapter setup:
- Sets alias (`aa-proxy-*` by default), powered, pairable.
- If `advertise=true`, sets discoverable with no timeout.

Key code:
- `aa-proxy-rs-research/src/bluetooth.rs:84`
- `aa-proxy-rs-research/src/bluetooth.rs:93`
- `aa-proxy-rs-research/src/bluetooth.rs:96`

3. Wait for (or trigger) phone profile connection:
- Waits for incoming AA profile connection (`handle_aa.next()` with timeout).
- Optional proactive connect mode loops through paired devices and calls `connect_profile(HSP_AG_UUID)`.

Key code:
- `aa-proxy-rs-research/src/bluetooth.rs:352`
- `aa-proxy-rs-research/src/bluetooth.rs:410`
- `aa-proxy-rs-research/src/bluetooth.rs:446`

4. RFCOMM AA handshake (5 message stages):
Packet format on RFCOMM: `[len:u16_be][msg_id:u16_be][protobuf payload]`.

Message IDs:
- `1` = `WifiStartRequest`
- `2` = `WifiInfoRequest`
- `3` = `WifiInfoResponse`
- `6` = `WifiConnectStatus`
- `7` = `WifiStartResponse`

Stage flow in `send_params(...)`:
- Stage 1: send `WifiStartRequest` with server IP + TCP port (`5288` by default)
- Stage 2: read `WifiInfoRequest`
- Stage 3: send `WifiInfoResponse` with SSID/key/BSSID, `WPA2_PERSONAL`, AP type `DYNAMIC`
- Stage 4: read `WifiStartResponse`
- Stage 5: read `WifiConnectStatus`; nonzero status is treated as failure

Key code:
- `aa-proxy-rs-research/src/bluetooth.rs:169`
- `aa-proxy-rs-research/src/bluetooth.rs:469`
- `aa-proxy-rs-research/src/bluetooth.rs:481`
- `aa-proxy-rs-research/src/bluetooth.rs:493`
- `aa-proxy-rs-research/src/bluetooth.rs:496`
- `aa-proxy-rs-research/src/bluetooth.rs:505`
- `aa-proxy-rs-research/src/config.rs:16`

Proto definitions:
- `aa-proxy-rs-research/src/protos/WifiStartRequest.proto`
- `aa-proxy-rs-research/src/protos/WifiInfoResponse.proto`

5. Wi-Fi/TCP + USB forwarding phase:
- After BT handshake success, `tcp_start.notify_one()` is fired.
- I/O loop then accepts phone TCP connection on `0.0.0.0:5288`.
- Opens USB accessory endpoint and starts bidirectional proxy between phone (TCP) and head unit (USB accessory).

Key code:
- `aa-proxy-rs-research/src/bluetooth.rs:531`
- `aa-proxy-rs-research/src/io_uring.rs:330`
- `aa-proxy-rs-research/src/io_uring.rs:376`
- `aa-proxy-rs-research/src/io_uring.rs:382`
- `aa-proxy-rs-research/src/io_uring.rs:407`
- `aa-proxy-rs-research/src/io_uring.rs:427`

6. Post-handshake BT behavior:
- Default: disconnects BT profile so phone can connect to car HFP path.
- If `quick_reconnect=true`, keeps profile stream alive for fast restart.

Key code:
- `aa-proxy-rs-research/src/bluetooth.rs:533`
- `aa-proxy-rs-research/src/bluetooth.rs:575`

## Services this design expects in practice
For wireless AA setup path:
- Required: AA RFCOMM service on UUID `4de17a00-52cb-11e6-bdf4-0800200c9a66`
- Expected channel in this implementation: RFCOMM channel `8`
- Auxiliary compatibility profile: HSP HS UUID `00001108-0000-1000-8000-00805f9b34fb`

For BLE sidecar/web control (not AA transport):
- BLE service UUID: `9b3f6c10-a4d2-418e-a2b9-0700300de8f4`

Key code:
- `aa-proxy-rs-research/src/bluetooth.rs:41`
- `aa-proxy-rs-research/src/bluetooth.rs:42`
- `aa-proxy-rs-research/src/bluetooth.rs:43`
- `aa-proxy-rs-research/src/btle.rs:80`

## Troubleshooting checklist for "SDP record unparseable"
Use this as a strict compare-against-aa-proxy-rs baseline.

1. Confirm your AA service registration path
- aa-proxy-rs uses BlueZ profile registration via `bluer` (not custom XML blobs).
- If your implementation uses custom SDP XML, first validate against minimal profile registration semantics.

2. Confirm UUID and channel match
- UUID must be exactly `4de17a00-52cb-11e6-bdf4-0800200c9a66`.
- Channel should be a valid RFCOMM server channel; aa-proxy-rs uses `8`.

3. Validate that phone can complete RFCOMM open after SDP
- If SDP matches but RFCOMM open never happens, trace `createRfcommSocketToServiceRecord(...)` attempts from phone logs.

4. Capture Bluetooth signaling while pairing/connecting
- Use `btmon` during pairing and first wireless launch attempt.
- Verify:
  - SDP Service Search Attribute request for AA UUID
  - non-error SDP response
  - RFCOMM SABM/UA on expected server channel

5. Compare against aa-proxy-rs on same stack if possible
- Same BlueZ/kernel/hardware, run aa-proxy-rs unchanged.
- If aa-proxy-rs works and your build fails, diff only BT profile registration and SDP attributes first.

## Practical log markers to align with
When healthy, aa-proxy-rs logs should show this order:
- `AA Wireless Profile: registered`
- `Waiting for phone to connect via bluetooth...`
- `AA Wireless Profile: connect from ...`
- `Sending Host IP Address ...`
- `Sending Host SSID and Password ...`
- `Bluetooth launch sequence completed`
- `MD TCP server: listening for phone connection...`
- `Starting to proxy data between HU and MD...`

(These strings come from `src/bluetooth.rs` and `src/io_uring.rs`.)

## Why this matters for your current error
Your phone-side error (`invalid length for discovery attribute`) happens before RFCOMM connect, during SDP attribute parsing.

Given aa-proxy-rs behavior, the safest baseline is:
- register AA service through BlueZ profile API (not custom malformed XML),
- expose UUID `4de17a00-52cb-11e6-bdf4-0800200c9a66`,
- bind a concrete RFCOMM channel (aa-proxy-rs uses `8`),
- verify SDP response bytes on-air with `btmon`.

If you want, I can produce a second document that translates this into a BlueZ D-Bus + `sdptool`/`btmon` command-by-command validation script for your Raspberry Pi test setup.
