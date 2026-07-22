# Android Auto Troubleshooting Runbook

Use this guide to diagnose wireless Android Auto sessions in OpenAuto Prodigy.
It covers the current Qt-based AA runtime, service logs, protocol capture,
failure isolation, and Pi deployment.

- [Phone-side debugging](aa-phone-side-debug.md) records useful Android Auto
  developer settings and logcat tags. Treat device- and release-specific
  observations there as evidence from that capture, not as current head-unit
  configuration.
- [Protocol cross-reference](android-auto-protocol-cross-reference.md) maps
  observed Android and head-unit concepts.
- Generate the untracked protocol catalog with
  `python3 tools/aa_proto_graph.py`. The implementation library is
  `libs/prodigy-oaa-protocol/`; its `proto/` directory is a hands-off community
  submodule.

## Tool inventory

### Service state and logs

Normal Pi installs run the app as `openauto-prodigy.service`. Start diagnosis
with the service and its journal rather than a detached process log:

```bash
systemctl status openauto-prodigy.service --no-pager
journalctl -u openauto-prodigy.service -n 200 --no-pager
journalctl -u openauto-prodigy.service -f
```

The installer also creates `openauto-system.service`, which owns privileged
network and Bluetooth operations. Include it when failures involve the AP,
BlueZ, or configuration application:

```bash
systemctl status openauto-system.service hostapd.service \
  systemd-networkd.service bluetooth.service --no-pager
journalctl -u openauto-system.service -n 200 --no-pager
sudo openauto-preflight --check-only
```

### Protocol capture

Protocol capture is built into the current AA runtime. Enable it in
`~/.openauto/config.yaml` before starting a new session:

```yaml
connection:
  protocol_capture:
    enabled: true
    format: jsonl
    include_media: false
    path: /tmp/oaa-protocol-capture.jsonl
```

Supported formats are `jsonl` and `tsv`. JSONL records elapsed milliseconds,
direction, channel and message identifiers, a resolved message name, and the
payload as hexadecimal. TSV uses a compact preview intended for reading in a
terminal.

Capture attaches when the TCP session is created and truncates the configured
file when a new session starts. Copy a useful capture before reconnecting.
Leave `include_media` false for handshake work; enabling it records media
payloads, grows quickly, and may retain sensitive projected content. Disable
capture again after collecting the evidence you need.

Confirm activation and inspect the configured output:

```bash
journalctl -u openauto-prodigy.service -b --no-pager | grep 'Protocol capture'
less /tmp/oaa-protocol-capture.jsonl
```

### Tests and protocol tools

Use an out-of-repository build directory:

```bash
cmake -S . -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy -j$(nproc)
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j$(nproc)
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```

Relevant coverage includes the AA orchestrator, runtime bridge, service
discovery, channel handlers, touch routing, video decode queue, and
`tests/test_oaa_integration.cpp`. Library-level session, transport, messenger,
capture, and handler tests live under `libs/prodigy-oaa-protocol/tests/` and are
registered by the normal CMake build.

For schema exploration:

```bash
python3 tools/aa_proto_graph.py
python3 -m pytest tools/
```

The graph script refreshes the tracked machine-readable graph and generates an
untracked Markdown catalog under `docs/aa-protocol/`. Proto validation tools
are described in [tools/README.md](../../tools/README.md).

## Debug workflows

### Something broke: first pass

1. Check `openauto-prodigy.service` and the current boot's journal.
2. Run `sudo openauto-preflight --check-only` to validate radio state, the
   Wayland socket, and the BlueZ SDP socket.
3. Confirm `hostapd`, `systemd-networkd`, and `bluetooth` are active.
4. Confirm the configured AA TCP listener. The default is `5277`, but
   `connection.tcp_port` is authoritative:

   ```bash
   ss -tlnp | grep 5277
   ```

5. Follow the journal while initiating a connection. A healthy wireless path
   progresses through RFCOMM/SDP discovery, WiFi credential exchange, a TCP
   connection, protocol negotiation, service discovery, and an active session.
6. If journal evidence is insufficient, enable protocol capture for one clean
   reproduction and collect phone logcat over the same interval.

### Collect a bounded reproduction

Before reproducing, note the app version and clear only the evidence stream you
control. Do not delete the phone pairing unless first-pair behavior is the
subject of the test.

```bash
adb logcat -c
journalctl -u openauto-prodigy.service -f
```

Run the manual sequence in
[Testing Android Auto Disconnect and Reconnect](../how-to/testing-reconnect.md).
That guide is the canonical reconnect procedure; keep reconnection mechanics
there instead of duplicating them in protocol investigations.

After the reproduction:

```bash
journalctl -u openauto-prodigy.service --since '10 minutes ago' --no-pager \
  > app-journal.log
adb logcat -d > phone-logcat.log
```

Copy the configured protocol-capture file before starting another session.
Record the configuration values that affect the result, especially TCP port,
WiFi interface, video resolution/codecs, Navbar placement, touch device, and
capture media policy.

### Build and deploy a change

Run the local build, explicit app-target build, and tests first. Then build the
Pi binary through the supported Docker cross-build path:

```bash
./cross-build.sh
```

Deploy the cross-built binary. Set deployment-specific values in the shell
instead of embedding a personal account or address in commands:

```bash
PI_TARGET="user@pi-host"
PI_APP_DIR="/path/to/openauto-prodigy"
rsync -av build-pi/src/openauto-prodigy \
  "$PI_TARGET:$PI_APP_DIR/build/src/"
ssh "$PI_TARGET" 'sudo systemctl restart openauto-prodigy.service'
ssh "$PI_TARGET" 'systemctl status openauto-prodigy.service --no-pager'
```

QML is compiled into the application binary. A QML change therefore requires
the same cross-build and binary deployment; syncing QML source or pulling the
repository on the Pi does not update the running UI.

## Failure-mode playbooks

### Session never starts

Symptoms include no TCP connection, no protocol-capture entries, or an app that
remains in its waiting state.

Check the layers in order:

1. **App:** the service is active and the configured TCP port is listening.
2. **BlueZ:** `bluetoothctl show` reports a powered adapter; the journal shows
   the RFCOMM listener and successful AA SDP registration. An SDP error usually
   means BlueZ is not using compatibility mode or `/var/run/sdp` is unavailable
   to the service account.
3. **Pairing:** `bluetoothctl devices` lists the phone and
   `bluetoothctl info <phone-address>` reports the expected paired state. Use
   the actual address reported by BlueZ rather than documenting one.
4. **AP:** `hostapd` and `systemd-networkd` are active, the selected interface
   has the AP address, and the live hostapd credentials match those advertised
   by the app. The app synchronizes SSID/password from the live hostapd file at
   startup.
5. **Phone:** wireless Android Auto is enabled and the paired car appears in
   Android Auto settings.

Useful journal milestones include `RFCOMM listening`, `SDP service registered`,
`Phone connected via BT`, `Phone connected to WiFi`, `TCP listener started`,
and `Wireless AA connection`.

### Negotiation or service-discovery stall

A session that reaches TCP but never becomes active should progress through:

1. version request/response;
2. TLS handshake and authentication completion;
3. service-discovery request/response;
4. channel-open request/response for advertised services;
5. AV setup/start, input binding, and sensor requests as needed;
6. regular ping traffic once active.

Use a capture with media excluded to find the first missing response. Compare
that timestamp with phone logcat. Do not infer a certificate, consent, or
authorization defect solely from a later phone timeout; identify the last
request and response visible on both sides first.

Unhandled channel traffic is reported in the app journal with an
`[AA:unhandled]` prefix and a payload preview. Preserve that evidence and map it
against the generated protocol catalog before proposing a schema change. The
community proto submodule remains hands-off.

### Video is absent, black, or unstable

First separate channel setup from decode/display:

- No `AV_SETUP_REQUEST`, `AV_START_INDICATION`, or video media messages points
  to session/channel setup rather than FFmpeg.
- Media messages with no `First frame decoded` entry point to codec/parser or
  decoder selection.
- Decoded frames with no visible projection point to the video sink, focus, or
  QML presentation path.

The shipped service discovery advertises the configured landscape mode and
enabled codecs. The current decode path supports H.264/AVC and H.265/HEVC,
auto-detects the incoming stream, attempts configured/hardware decoders, and
falls back to software when possible. Check journal entries for the selected
decoder, codec switching, first packet, parse/send/receive errors, and first
decoded frame.

Protocol details that remain important:

- SPS/PPS configuration can arrive in `AV_MEDIA_INDICATION` without a
  timestamp and must reach the decoder.
- Encoded data already contains Annex B start codes; adding another prefix
  corrupts the stream.
- Both `AV_PIX_FMT_YUV420P` and `AV_PIX_FMT_YUVJ420P` are accepted.
- FFmpeg decode stays single-threaded to avoid latency from internal frame
  buffering.
- Video focus requests must receive a projected/unprojected response that
  matches the current UI state.

Use `video.resolution`, `video.codecs`, and `video.decoder.*` configuration to
test deliberate variants. Do not substitute the physical display dimensions
for the negotiated video mode.

### Audio is absent or choppy

Check PipeWire and WirePlumber before changing AA protocol behavior:

```bash
wpctl status
pw-cli ls Node
journalctl -u openauto-prodigy.service -b --no-pager | grep -E 'PipeWire|Audio|audio'
```

The advertised playback streams are media at 48 kHz stereo, speech/navigation
at 48 kHz mono, and system sounds at 16 kHz mono. The microphone input stream is
16 kHz mono. Confirm AV setup/start for the affected channel, the selected
PipeWire device, focus changes, and buffer-pressure logs.

For stutter, distinguish missing input packets from playback underruns. The
playback callback must publish the full requested PipeWire period and
silence-fill any gap; a protocol capture with media enabled can establish
whether packet delivery itself is discontinuous, but enable it only for a
short, controlled reproduction.

### Touch is absent or misaligned

The Pi touch path reads a Linux evdev multi-touch device directly. It
auto-detects a direct-input device unless `touch.device` is set. During an AA
session the device is grabbed so events route to AA; on disconnect it is
released to Wayland/libinput.

Check the journal for the detected device, raw axis range, display viewport,
advertised content dimensions, Navbar zone, and computed evdev mapping. The AA
coordinate space is the advertised `touch_screen_config` content area: the
configured video mode minus declared margins. It is not necessarily the full
encoded frame or the physical display size.

If the launcher loses touch after disconnect, check for the ungrab message. If
the Navbar is visible during AA, its touch is handled by evdev zones registered
through the runtime bridge; QML pointer handlers do not receive Pi touch while
the device is grabbed.

Useful device checks:

```bash
cat /proc/bus/input/devices
ls -l /dev/input/event*
```

The service account must belong to the `input` group.

### Crash, restart, or stale connection

Use systemd for normal restart and inspect the result immediately:

```bash
sudo systemctl restart openauto-prodigy.service
systemctl status openauto-prodigy.service --no-pager
journalctl -u openauto-prodigy.service -n 200 --no-pager
```

If the AA port cannot bind, use `ss -tlnp` to identify the owner. The current
listener sets close-on-exec and configures address reuse before bind, so a
repeatable ownership leak is a regression worth capturing rather than a reason
to normalize force-killing processes.

If the app exits under watchdog supervision, inspect the lines preceding the
restart for Qt fatal messages, decoder errors, PipeWire teardown, TCP watchdog
backoff, or session timeout. Follow the canonical reconnect guide after the
service is stable; deleting pairing data changes the failure being tested.

## Phone-side quick reference

Enable Android Auto developer settings from the Android Auto app's version
screen, then enable verbose/debug logging for the reproduction. Menu wording
and available capture toggles vary by Android Auto release.

Commonly useful logcat filters include core car protocol, wireless setup, WPP,
projection, and Bluetooth tags:

```bash
adb logcat | grep -E 'CAR\.|GH\.|WIRELESS|PROJECTION|WPP'
adb logcat -d | grep -E 'CAR\.|GH\.|WIRELESS|PROJECTION|WPP'
```

Correlate phone events with the Pi journal and protocol capture by recording a
clear test start time and one reconnect attempt. Avoid treating process names,
tag spelling, or UI labels observed on one phone release as a stable protocol
contract.

## Deployment checklist

Before a Pi validation pass:

- [ ] Local build completes in the external build directory.
- [ ] The explicit `openauto-prodigy` app target builds.
- [ ] `ctest --output-on-failure` is green; no failure is assumed or waived.
- [ ] `./cross-build.sh` produces the Pi application binary.
- [ ] The cross-built binary is synced to the selected Pi install directory.
- [ ] `openauto-prodigy.service` restarts and remains active.
- [ ] QML changes are present through the rebuilt binary, not a source sync.
- [ ] Pre-flight, AP, networkd, and Bluetooth services are healthy.
- [ ] The test configuration and capture policy are recorded.
- [ ] Reconnect follows the canonical guide and preserves pairing unless the
      test explicitly concerns pairing.

## Investigation discipline

- Keep implementation evidence, phone evidence, and hypotheses separate.
- Record the last successful protocol transition before diagnosing a later
  symptom.
- Prefer named configuration keys and observed service state over fixed ports,
  interfaces, addresses, or device identifiers.
- Never edit `libs/prodigy-oaa-protocol/proto/` in this repository. Record a
  required community protocol change for the upstream submodule instead.
- Do not turn a one-phone observation into a universal AA requirement without
  another capture or implementation-level proof.
