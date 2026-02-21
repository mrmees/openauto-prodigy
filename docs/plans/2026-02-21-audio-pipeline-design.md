# Audio Pipeline Design

> Status: Approved
> Date: 2026-02-21

## Goal

Fix the broken PipeWire integration in AudioService so AA audio actually plays, add device enumeration so users can pick their output and input devices, and wire up mic capture for Google Assistant / phone calls.

## Problem

AudioService is structurally broken in four ways:

1. **PipeWire main loop never runs.** `pw_main_loop_new()` is called but `pw_main_loop_run()` never is. The event loop is dead — callbacks never fire, buffer negotiation never happens.
2. **Playback streams never connected.** `pw_stream_new()` creates stream objects but `pw_stream_connect()` is never called for output streams. They're ghosts in the audio graph.
3. **Wrong write model.** `writeAudio()` tries to push audio imperatively via `pw_stream_dequeue_buffer()`. PipeWire uses a pull model — a `process` callback fires on the RT thread when it needs data.
4. **No thread bridge.** AA audio arrives on ASIO threads. PipeWire's process callback fires on its RT thread. There's no safe handoff between them.

The AA protocol side works correctly — channel handshake completes, audio data arrives in `onAVMediaWithTimestampIndication()` — but every sample gets silently dropped at the AudioService boundary.

## Design

### Thread Model

| Thread | Owns | Communicates via |
|--------|------|-----------------|
| Qt main thread | UI, config, device list model, stream lifecycle | `pw_thread_loop_lock/unlock` |
| PipeWire RT thread | `process` callbacks, buffer fill/drain | `spa_ringbuffer` (lock-free) |
| ASIO thread(s) | AA protocol, audio data arrival | `spa_ringbuffer` (lock-free) |

### Core Change: `pw_main_loop` to `pw_thread_loop`

Replace the current `pw_main_loop` (which requires a blocking `pw_main_loop_run()` call) with `pw_thread_loop`, which runs PipeWire's event loop on its own internal thread and provides a lock/unlock API for safe cross-thread access from Qt.

```
pw_thread_loop_new() → pw_context_new() → pw_context_connect()
    → pw_thread_loop_start()
```

All PipeWire API calls from the Qt main thread (stream creation, device queries, volume changes) are bracketed with `pw_thread_loop_lock()` / `pw_thread_loop_unlock()`.

### Stream Architecture

Three separate PipeWire output streams, one per AA audio channel:

| Stream Name | AA Channel | Format | PipeWire Role |
|-------------|-----------|--------|---------------|
| `openauto-media` | `MEDIA_AUDIO` | 48 kHz stereo 16-bit PCM | Music |
| `openauto-speech` | `SPEECH_AUDIO` | 16 kHz mono 16-bit PCM | Communication |
| `openauto-system` | `SYSTEM_AUDIO` | 16 kHz mono 16-bit PCM | Notification |

One PipeWire capture stream for mic input back to AA:

| Stream Name | AA Channel | Format | PipeWire Role |
|-------------|-----------|--------|---------------|
| `openauto-mic` | `AV_INPUT` | 16 kHz mono 16-bit PCM | Capture |

PipeWire handles mixing the 3 output streams to the hardware sink natively. Separate streams give us:
- Per-stream volume control
- Proper audio role tagging (WirePlumber can apply role-based policies)
- Built-in ducking (duck music when navigation speaks)

### Data Flow: AA to Speakers

```
Phone ──TCP──> ASIO thread ──> onAVMediaIndication()
                                      │
                          spa_ringbuffer_write()  [lock-free, ASIO thread]
                                      │
                              [per-stream ring buffer, ~100ms]
                                      │
                          PipeWire process callback  [RT thread]
                                      │
                          spa_ringbuffer_read() → pw_buffer → hardware sink
```

Each stream gets its own `spa_ringbuffer` sized for ~100ms of audio data:
- Media (48kHz stereo 16-bit): 48000 * 2 * 2 * 0.1 = ~19.2 KB
- Speech/System (16kHz mono 16-bit): 16000 * 1 * 2 * 0.1 = ~3.2 KB

**Underrun policy:** Output silence (zero-fill the PipeWire buffer).
**Overrun policy:** Drop oldest data to keep latency bounded.

### Data Flow: Mic to Phone

```
Hardware source → PipeWire process callback  [RT thread]
                          │
              spa_ringbuffer_write()  [lock-free]
                          │
                  [capture ring buffer, ~100ms]
                          │
              Qt timer or ASIO poll → spa_ringbuffer_read()
                          │
              AVInputServiceChannel → TCP → Phone
```

### Device Enumeration

A `pw_registry` listener filters for nodes with `media.class` of:
- `Audio/Sink` — output devices (speakers, USB DAC, HDMI, headphone jack)
- `Audio/Source` — input devices (USB mic, line-in)
- `Audio/Duplex` — both (USB headset)

Each device entry stores:
- `node.name` — programmatic identifier, used as config value (e.g. `"alsa_output.usb-Generic_USB_Audio-00.analog-stereo"`)
- `node.description` — human-readable display name (e.g. `"USB Audio Analog Stereo"`)
- `registry_id` — PipeWire session-scoped ID for tracking adds/removes

Two `QAbstractListModel` subclasses exposed to QML:
- `AudioOutputDeviceModel` — lists sinks + a synthetic "Default (auto)" entry at top
- `AudioInputDeviceModel` — lists sources + a synthetic "Default (auto)" entry at top

Hot-plug: `global_remove` registry event removes entries from the model. If the currently selected device disappears, trigger fallback (see below).

### Device Selection & Targeting

When creating a PipeWire stream, if the user has selected a specific device (not "auto"), set `PW_KEY_TARGET_OBJECT` to the device's `node.name`:

```cpp
if (deviceName != "auto") {
    pw_properties_set(props, PW_KEY_TARGET_OBJECT, deviceName.c_str());
}
// Otherwise: no target set, WirePlumber routes to default
```

Connect with `PW_ID_ANY` — device targeting is handled entirely by the property.

All output streams share the same target device. All capture streams share the same target device. One output selection, one input selection. Per-function routing (navigation to a different speaker than music) is not a realistic car head unit use case and is deferred indefinitely.

### Device Disconnect Fallback

When the selected device disappears at runtime:

1. Detect via `global_remove` registry event
2. Mark current streams as degraded
3. Disconnect and recreate streams with no `PW_KEY_TARGET_OBJECT` (fall back to PipeWire default)
4. Emit a signal to the UI: `deviceFallback(QString functionName, QString lostDevice, QString fallbackDevice)`
5. Log with `qWarning()`

When the device reappears: do NOT auto-switch back. The user may have adjusted. Show a notification that the preferred device is available again and let them switch manually.

### Runtime Device Switching

Device changes apply immediately without app restart:

1. `pw_thread_loop_lock()`
2. `pw_stream_disconnect()` on affected streams
3. `pw_stream_destroy()` the old streams
4. Create new streams with updated `PW_KEY_TARGET_OBJECT`
5. `pw_stream_connect()` the new streams
6. `pw_thread_loop_unlock()`

Brief audio gap (~50-100ms) is acceptable. Ring buffers are reset on stream recreation.

### Volume Control

- **Master volume:** `pw_stream_set_control(SPA_PROP_channelVolumes, ...)` applied to all 3 output streams. Driven by config value `audio.master_volume` (0-100, mapped to 0.0-1.0 cubic curve).
- **Per-stream ducking:** Existing `applyDucking()` logic applies volume multipliers when speech/system streams are active (duck media to configured level).
- **GestureOverlay:** Volume slider wires to `AudioService::setMasterVolume()`.
- **Mic gain:** Applied as a volume multiplier on the capture stream.

### Configuration

Extends existing `audio` config section. Backward-compatible with current schema:

```yaml
audio:
  master_volume: 75          # 0-100
  output_device: "auto"      # node.name string or "auto"
  input_device: "auto"       # node.name string or "auto"
  microphone:
    gain: 1.5                # multiplier applied to capture
```

The old `microphone.device: "hw:1,0"` key (ALSA format) is replaced by `input_device` at the top level using PipeWire `node.name`. Migration: if `microphone.device` exists and `input_device` doesn't, ignore the ALSA name and default to "auto".

### Settings UI

**QML AudioSettings page** (already exists, needs wiring):
- Output device: `ComboBox` populated from `AudioOutputDeviceModel`
- Input device: `ComboBox` populated from `AudioInputDeviceModel`
- Master volume: `Slider` wired to `AudioService::setMasterVolume()`
- Mic gain: `Slider` wired to config

**Web config panel** (new IPC commands):
- `get_audio_devices` — returns `{outputs: [...], inputs: [...]}` with name/description
- `set_audio_config` — sets output_device, input_device, master_volume, mic_gain
- `get_audio_config` — returns current settings

### Device Identity

Primary identifier: `node.name` (e.g. `"alsa_output.usb-Generic_USB_Audio-00.analog-stereo"`). This is stable within a PipeWire session and usually stable across reboots for the same USB port.

Secondary display: `node.description` (e.g. `"USB Audio Analog Stereo"`). Stored alongside for UI display and as a human-readable fallback if `node.name` format changes.

Known limitation: USB devices can get different `node.name` values if plugged into a different port. This is acceptable for v1 — the user just re-selects the device. A composite identity scheme (vendor ID + product ID + serial) could be added later if this becomes a real problem.

## What This Design Does NOT Include

- HFP call audio routing through PipeWire (phone plugin, separate effort)
- Per-function device routing (navigation to different speaker than music)
- Audio transition journaling / forensic logging
- Equalizer / DSP processing
- Multi-zone output (front/rear)
- Audio focus arbitration with external applications
- AA-to-native-BT handover during calls

## Testing Plan

### Unit Tests
- Ring buffer write/read correctness (fill, drain, overrun, underrun)
- Device model add/remove/lookup
- Config migration (old `microphone.device` key)
- Volume mapping (0-100 integer to 0.0-1.0 cubic float)

### Integration Tests (on Pi)
- Stream creation with real PipeWire daemon
- Device enumeration sees real hardware
- Audio plays through selected device
- Device hot-plug (plug/unplug USB audio)
- Mic capture reaches AA (Google Assistant test)

### Manual Verification
- Play music in AA, hear it from speakers
- Navigation prompt ducks music
- Google Assistant responds to voice
- Switch output device in settings, audio moves
- Unplug USB DAC mid-playback, falls back to default
- Volume slider in GestureOverlay changes actual volume
