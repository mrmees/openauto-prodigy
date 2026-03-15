# Handoff: Proto v1.1 Migration (In Progress)

## What Happened
Updated proto submodule from v1.0 to v1.1 (ed53047, origin/main). v1.1 has APK-verified Gold confidence protos across all 14 channels with many renames, retractions, and field corrections.

## Completed Fixes

### Intentional code changes (our phase 03 salvage):
1. **InputChannelHandler.cpp** — `set_scan_code()` → `set_keycode()`, added `set_meta_state(0)` (required field)
2. **MediaStatusChannelHandler** — removed retracted `sendPlaybackCommand()`, `togglePlayback()`, `playbackCommandSent` signal. MediaPlaybackCommand doesn't exist.
3. **AndroidAutoOrchestrator** — removed `sendMediaPlaybackCommand()`, `toggleMediaPlayback()`, `sendVoiceSessionRequest()`. All media/voice control is via button events on input channel.
4. **SystemSettings.qml** — replaced Voice Start/Stop buttons with Assist(219)/Voice(231) keycode buttons. All debug buttons now use `sendButtonPress()`.
5. **ServiceDiscoveryBuilder** — added keycodes 219 (ASSIST) and 231 (VOICE_ASSIST) to SDP

### Cascading proto rename fixes:
6. **ControlChannel.cpp** — `ShutdownResponse` → `ByeByeResponse` (include + usage)
7. **AASession.cpp** — `AudioFocusType::GAIN_NAVI` → `GAIN_TRANSIENT_MAY_DUCK`, `AudioFocusState::NONE` → `INVALID`
8. **NavigationChannelHandler** — removed `handleFocusIndication()` (retracted proto). Switched case to log-only for 0x8005.
9. **MessageIds.hpp** — removed `NAV_FOCUS_INDICATION`, renamed `PLAYBACK_COMMAND` → `PLAYBACK_STATUS_EVENT`
10. **ProtocolLogger.cpp** — updated NAV_FOCUS_INDICATION reference to hardcoded 0x8005
11. **AudioChannelHandler.cpp** — stubbed retracted `AudioFocusState` and `AudioStreamType` message handlers. Fixed `AVMediaAckIndication`: `set_session()` → `set_session_id()`, `set_value()` → `set_ack_count()`. Fixed `config_index` → `media_codec_type` in setup request log.
12. **VideoChannelHandler.cpp** — same ACK field renames, same config_index fix
13. **BluetoothChannelHandler.cpp** — `BluetoothPairingStatus::OK` → raw `0` (enum retracted)
14. **ServiceDiscoveryBuilder.cpp** — `Res::_720p/_1080p/_480p` → `VIDEO_1280x720/VIDEO_1920x1080/VIDEO_800x480`, `add_touch_screen_config` → `add_touch_screen_configs`, `set_ssid` → `set_bssid`

## Remaining Build Errors (last build)

1. **PhoneStatusChannelData redefinition** — `oaa/phone/PhoneStatusChannelData.pb.h` has class `PhoneStatusChannel` that conflicts with something. The proto file was deleted in v1.1 (retracted). Likely need to:
   - Check if any code includes `PhoneStatusChannelData.pb.h`
   - If so, remove the include
   - The file may still exist as a tombstone in proto/ — if it generates an empty class that conflicts, may need to add it to the CMake EXCLUDE REGEX

2. **WiFi field semantics** — Changed `set_ssid` → `set_bssid` per proto, but the VARIABLE is still `wifiSsid_`. The v1.1 proto says field 1 is `bssid` (BSSID), not SSID. Need to verify: do we actually pass the BSSID or SSID here? The WiFi AP connection likely needs SSID, but the proto field is called bssid. Check `docs/channels/wifi-projection.md` for the correct semantics.

3. **Possible more cascading errors** — each build may reveal more. Pattern: run build, fix errors, repeat.

## Proto Submodule State
- Checked out at: `ed53047` (origin/main, tag v1.1)
- In detached HEAD state (need `git checkout main` or just update submodule pointer in parent repo)

## Key Proto Learnings (update MEMORY.md)
- MediaPlaybackCommand RETRACTED — media control is via input channel button events only
- VoiceSessionRequest is Phone→HU direction (we receive it, don't send it)
- Voice activation from HU: send keycode 219 (ASSIST) or 231 (VOICE_ASSIST) as button event
- AudioStreamType, AudioFocusState messages RETRACTED
- NavigationFocusIndication RETRACTED — nav focus is on Control channel (msgs 13/14)
- AVMediaAckIndication fields: session→session_id, value→ack_count, new receive_timestamps field
- VideoResolution enum renamed: _720p→VIDEO_1280x720, etc.
- BluetoothPairingStatus enum RETRACTED — use raw int32 (0=SUCCESS)
- WiFi field 1 is bssid, not ssid
- InputChannelConfig: touch_screen_config→touch_screen_configs (plural)
