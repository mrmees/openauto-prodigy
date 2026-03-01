# Pitfalls Research

**Domain:** Android Auto head unit (Raspberry Pi) -- feature completion and release hardening
**Researched:** 2026-03-01
**Confidence:** MEDIUM-HIGH (most pitfalls derive from codebase analysis + verified documentation; HFP telephony API details are LOW confidence due to rapid evolution)

## Critical Pitfalls

### Pitfall 1: HFP Audio Ownership Fight Between PipeWire and Your App

**What goes wrong:**
PipeWire's bluez5 plugin already registers as the HFP Audio Gateway on Trixie (confirmed in project memory: "HFP AG owned by PipeWire's bluez5 plugin"). If you try to manage SCO audio routing yourself via BlueZ D-Bus, you will conflict with PipeWire's native HFP handling. Two components both claiming AG ownership causes silent failures: calls connect at the signaling level but no audio flows, or audio routes to the wrong sink.

**Why it happens:**
The PhonePlugin currently monitors call state via BlueZ D-Bus but delegates actual audio to PipeWire. The temptation when implementing real call audio is to grab SCO socket management yourself for control. But PipeWire already negotiates HFP codecs (mSBC/CVSD) and creates source/sink nodes automatically. Fighting this creates a tug-of-war.

**How to avoid:**
Use PipeWire as the HFP audio owner. Your app should:
1. Monitor call state via BlueZ D-Bus `org.bluez.Device1` properties (already done)
2. Let PipeWire handle SCO audio routing -- it creates `bluez_input.*` and `bluez_output.*` nodes automatically for HFP
3. Use WirePlumber policies or `pw_metadata` to route HFP audio to your app's capture/playback streams
4. For call initiation/hangup, use PipeWire's new `org.pipewire.Telephony` D-Bus API (PipeWire 1.4+, Pi has 1.4.2) rather than raw BlueZ AT commands

Do NOT: open SCO sockets directly, register your own AG profile competing with PipeWire, or bypass PipeWire for "more control."

**Warning signs:**
- Call connects but no audio heard
- `pactl list sources` shows HFP source but your app doesn't see it
- BlueZ logs show profile conflicts or "profile already registered"
- Audio works on first call but breaks on reconnect

**Phase to address:**
HFP Call Audio phase (early -- this is an architectural decision that gates all other call audio work)

---

### Pitfall 2: AA Margins Locked at Session Start -- No Dynamic Sidebar Reconfiguration

**What goes wrong:**
You implement a beautiful sidebar toggle or resize, test it in the launcher, it works great. Then you try toggling the sidebar during an active Android Auto session and either: (a) the video doesn't change because margin/touch configs are session-locked, or (b) you force a reconnect and lose the user's navigation/music mid-drive.

**Why it happens:**
The AA protocol sends `margin_width`/`margin_height` and `touch_screen_config` in `ServiceDiscoveryResponse`, which happens once at connection time. The protocol documentation (confirmed in `docs/aa-display-rendering.md`) states explicitly: "Margins are locked at session start. They cannot be changed mid-session." `AVChannelSetupRequest` is phone-to-HU only -- the head unit cannot initiate renegotiation. `VideoFocusIndication` can pause/resume but cannot change resolution or margins.

**How to avoid:**
Accept the protocol constraint and design around it:
1. Sidebar config must be set BEFORE AA connects (or on next connection)
2. Show a clear "takes effect on next connection" message in settings UI
3. Store sidebar preference in config; apply at connection time in `VideoService::fillFeatures()` and `ServiceFactory` touch setup
4. For "dynamic" feel, allow toggling sidebar VISIBILITY (show/hide the strip) without changing video margins -- the video area stays the same size but the sidebar overlays or hides. This gives instant feedback without protocol renegotiation.
5. If truly dynamic resize is required, implement a graceful AA disconnect/reconnect cycle with user confirmation

**Warning signs:**
- Touch coordinates drift after sidebar toggle during AA session
- Black bars appear/disappear without matching touch remapping
- User complaints about sidebar toggle "not working" during navigation

**Phase to address:**
Dynamic Sidebar/Video Reconfiguration phase (this shapes the entire feature design)

---

### Pitfall 3: HFP Call Audio Drops When AA Disconnects

**What goes wrong:**
User is on a phone call via HFP. AA session ends (phone moves out of WiFi range, user backgrounds AA, app restart). The call audio cuts out because the HFP audio stream was tied to the AA session lifecycle rather than being independent.

**Why it happens:**
In the current architecture, the AndroidAutoPlugin manages connection state and triggers grab/ungrab of touch + audio focus. If HFP audio routing is wired through the AA orchestrator's lifecycle, disconnecting AA tears down the audio path. The phone call is still active at the Bluetooth/cellular level, but the head unit stops routing audio.

**How to avoid:**
HFP audio must be architecturally independent of the AA session:
1. PhonePlugin owns HFP audio lifecycle, NOT AndroidAutoPlugin
2. HFP PipeWire streams (capture + playback) live in PhonePlugin, created on call start, destroyed on call end -- completely independent of AA state
3. Audio focus interaction: when a call is active AND AA is connected, use `AudioFocusType::GainTransientMayDuck` to duck AA media while routing call audio. When AA disconnects, call audio continues undisturbed.
4. Test matrix: call starts before AA, call starts during AA, AA disconnects during call, call ends after AA disconnects

**Warning signs:**
- Audio cuts out when switching away from AA view during a call
- PhonePlugin's audio streams are created/destroyed in AndroidAutoPlugin callbacks
- No unit test for "call survives AA disconnect" scenario

**Phase to address:**
HFP Call Audio phase (core architectural requirement)

---

### Pitfall 4: PipeWire Filter-Chain EQ Adds Latency to AA Audio

**What goes wrong:**
You insert a PipeWire filter-chain (biquad EQ) into the audio path. It works for music playback, but AA navigation prompts and phone call audio now have perceptible latency. Navigation says "turn right" 100-200ms after the visual prompt changes. Call audio feels laggy like a satellite phone.

**Why it happens:**
PipeWire filter-chain operates with its own buffer quantum. Each filter node adds at least one quantum of latency (typically 1024 samples at 48kHz = ~21ms). Multiple filter stages or large convolutions compound this. AA audio already has ~100ms of transport + decode latency from the wireless link. Adding filter-chain latency pushes total latency past the perceptibility threshold for speech/navigation.

**How to avoid:**
1. Apply EQ only to the media/music stream, NOT to navigation or phone call audio streams. The current AudioService already creates separate streams for media, navigation, and phone -- use this separation.
2. Use PipeWire's parametric-equalizer module (more efficient than manually chained biquads) for the EQ implementation
3. Keep the filter-chain quantum matched to the graph quantum (don't use a larger processing block)
4. Benchmark on Pi 4: a 10-band parametric EQ via biquad should use <1% CPU at 48kHz stereo. If CPU usage is higher, check for unnecessary resampling or mismatched sample rates.
5. Consider implementing EQ in the ring buffer write path (apply biquads inline in `AudioService::writeAudio()`) instead of via PipeWire filter-chain -- eliminates the extra graph hop entirely. Simple biquad math is ~100 multiplies per sample, trivial on Pi 4.

**Warning signs:**
- Navigation audio feels "late" compared to visual prompts
- Users report echo on phone calls
- `pw-top` shows extra latency on filtered streams
- CPU usage spikes when EQ is enabled (suggests resampling or format conversion in the chain)

**Phase to address:**
Audio EQ phase (design decision: inline vs filter-chain)

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Inline biquad EQ in writeAudio() | Zero latency, no PipeWire dependency | Tightly coupled to AudioService, harder to add convolution/advanced DSP later | Acceptable for v1.0 -- 10-band parametric is the scope |
| Sidebar visibility toggle instead of true dynamic resize | Instant UX, no protocol issues | Users expect "real" resize; overlay sidebar wastes screen space | Acceptable for v1.0 -- protocol makes true resize impossible mid-session |
| Single hardcoded theme directory | Simpler code path | Adding theme marketplace/sharing later requires refactor | Acceptable for v1.0 if theme YAML schema is well-defined |
| stderr/printf logging in RT audio callbacks | Easy debugging | Blocks RT thread if stderr is slow (pipe, file) | Never in release builds -- use atomic flag + periodic poll from non-RT thread (already partially done for rate diagnostics) |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| PipeWire HFP audio | Opening SCO sockets directly, bypassing PipeWire | Let PipeWire own SCO; route via `pw_metadata` or WirePlumber policies |
| PipeWire filter-chain | Inserting filter between app stream and sink (requires WirePlumber scripting) | Use `target.object` on filter-chain capture to grab your app's output; or apply EQ inline in ring buffer |
| BlueZ D-Bus telephony | Using `org.bluez.Network1` or deprecated oFono for call control | Use PipeWire's `org.pipewire.Telephony` API (1.4+) for Dial/Answer/Hangup; monitor state via BlueZ D-Bus |
| ThemeService live reload | Emitting `colorsChanged()` and expecting all QML to update | QML bindings to `ThemeService.color("key")` are NOT automatically re-evaluated on signal. Must use Q_PROPERTY bindings or explicit Connections {} blocks per color |
| systemd watchdog | Setting `WatchdogSec` in unit file but not petting from app | App must call `sd_notify("WATCHDOG=1")` periodically. Pi 4 BCM watchdog has 15s max timeout -- set `RuntimeWatchdogSec=10s` and pet every 5s |
| AA video decoder reconfiguration | Destroying and recreating FFmpeg codec context mid-stream | Flush decoder with `avcodec_flush_buffers()` before parameter change. SPS/PPS must be re-sent after flush. |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| EQ filter-chain with sample rate mismatch | Extra resampler nodes appear in `pw-top`, CPU jumps 5-10% | Ensure EQ filter-chain sample rate matches AA audio (48kHz). Avoid format negotiation mismatches. | Immediately on mismatch -- constant overhead |
| Theme color lookup per frame | QML `color: ThemeService.color("bg")` called every frame due to animation | Cache color values in QML properties; update only on `colorsChanged` signal | Visible at >30 animated elements with per-frame color lookups |
| Wallpaper image not pre-scaled | 4K wallpaper decoded and scaled every time view loads | Pre-scale wallpaper to display resolution (1024x600) on selection, cache scaled version | With images >2MP on Pi 4 -- noticeable 200-500ms stutter on view load |
| fprintf in RT audio callback | Audio glitches when stderr is piped to journald or slow storage | Use atomic counters in RT callback, read + log from timer on main thread (pattern already in codebase for rate diagnostics) | When journald is under pressure or SD card is busy |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Leaving PipeWire telephony API accessible to all D-Bus users | Any process can initiate/answer/hangup phone calls | Restrict `org.pipewire.Telephony` access via D-Bus policy; or use PipeWire's built-in access control |
| Web config panel Unix socket world-readable | Any local process can change head unit config | Set socket permissions to user-only (0600) in IpcServer; already using `/tmp/openauto-prodigy.sock` -- verify umask |
| Theme YAML with unchecked paths | Path traversal via `icon_path: "../../../etc/shadow"` in malicious theme | Validate all paths in theme YAML are relative and within theme directory; reject `..` components |
| Installer running as root without input validation | Command injection via WiFi SSID/password with special characters | Already addressed somewhat; double-check all `hostapd.conf` values are sanitized |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| EQ changes require app restart | User tunes EQ, has to restart to hear changes -- feels broken | Apply EQ changes in real-time. If using inline biquads, update coefficients atomically. If using filter-chain, reload config via `pw-cli`. |
| Theme preview shows colors but not actual car-mounted appearance | User picks theme that looks good on phone/desktop but is unreadable in bright sunlight | Show preview ON the Pi display. Provide "sunlight" and "night" preview modes. Include high-contrast themes as defaults. |
| Sidebar toggle "doesn't work" during AA | User expects instant sidebar; protocol prevents it | Clear UI messaging: "Sidebar changes take effect on next Android Auto connection." Disable the toggle control (greyed out) while AA is active. |
| Phone call notification during AA navigation | Full-screen call overlay blocks navigation view | Use the existing notification system (priority-based overlay) for incoming calls. Show minimal caller ID bar, NOT full-screen takeover. Allow answer/reject from the bar. |
| EQ presets named for headphones not car speakers | Imported AutoEQ profiles meaningless in car context | Ship presets named for car audio scenarios: "Bass Boost", "Flat", "Vocal Clarity", "Road Noise Compensation". Don't expose raw frequency numbers in main UI. |

## "Looks Done But Isn't" Checklist

- [ ] **HFP Call Audio:** Often missing microphone routing -- calls work outbound (you hear them) but they can't hear you. Verify bidirectional audio: PipeWire capture stream from BT SCO source routed to PhonePlugin, AND playback stream from PhonePlugin to BT SCO sink.
- [ ] **HFP Call Audio:** Often missing codec negotiation handling -- mSBC (wideband) vs CVSD (narrowband) produce different stream parameters. Verify both codecs work, especially when phone doesn't support mSBC.
- [ ] **Audio EQ:** Often missing bypass/disable -- user has no way to turn EQ completely off without manually removing config. Ship with EQ defaulting to OFF (flat).
- [ ] **Theme Selection:** Often missing validation of incomplete themes -- user-created theme YAML missing required color keys causes transparent/invisible UI elements. Validate all required keys present; fall back to default theme colors for missing keys.
- [ ] **Theme Selection:** Often missing font fallback -- custom theme specifies font not installed on Pi. Always chain to a guaranteed-present fallback font (Lato is already bundled).
- [ ] **Dynamic Sidebar:** Often missing evdev touch zone recalculation -- sidebar visibility changes but EvdevTouchReader still uses old hit zones. Touch zones must update when sidebar state changes.
- [ ] **Release Hardening:** Often missing graceful degradation when PipeWire is unavailable -- app crashes instead of running without audio. AudioService already handles this (`isAvailable()` checks), but verify ALL callers handle nullptr returns from `createStream()`.
- [ ] **Release Hardening:** Often missing log rotation -- app runs for weeks, fills SD card with logs. Ensure journald has size limits and app's own logging (if any file logging) is bounded.
- [ ] **Release Hardening:** Often missing clean shutdown on SIGTERM -- systemd sends SIGTERM before SIGKILL. App must flush config, disconnect AA gracefully, and release EVIOCGRAB within the systemd timeout (default 90s, but the app's `restart.sh` uses shorter windows).

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| HFP audio ownership conflict | LOW | Disable app's SCO handling, restart PipeWire (`systemctl --user restart pipewire`), let PipeWire re-register AG |
| AA margins wrong after sidebar change | LOW | Restart app or trigger AA reconnect; margins recalculated on next `ServiceDiscoveryResponse` |
| EQ causes audio glitches | LOW | Disable EQ (set flat/bypass), remove filter-chain config, restart PipeWire if needed |
| Theme causes unreadable UI | LOW | Fall back to default theme via config edit or web panel; ThemeService already falls back to day colors when night keys missing |
| Watchdog false-positive reboots | MEDIUM | Increase `RuntimeWatchdogSec`, add more frequent petting, check for main thread blocks (long FFmpeg init, D-Bus timeouts) |
| Call audio drops on AA disconnect | HIGH (if architectural) | Requires decoupling PhonePlugin audio from AA lifecycle -- architectural change if not designed correctly upfront |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| HFP audio ownership fight | HFP Call Audio | PipeWire owns SCO; `pw-dump` shows HFP nodes; no BlueZ profile conflicts in `journalctl -u bluetooth` |
| AA margins locked mid-session | Dynamic Sidebar | UI shows "next connection" message; toggle greyed during AA; no touch coordinate drift |
| HFP audio independent of AA | HFP Call Audio | Test: start call, connect AA, disconnect AA, verify call audio continues |
| EQ latency on navigation/call | Audio EQ | Measure end-to-end latency with EQ enabled; navigation audio within 50ms of visual prompt |
| Theme live reload | Theme Selection | Change theme, verify all QML elements update without app restart; no transparent/missing elements |
| Release watchdog | Release Hardening | Kill -STOP the app process; verify Pi reboots within 15s; verify clean restart |
| Log rotation | Release Hardening | Run app for 48h; verify log storage bounded; `journalctl --disk-usage` under 50MB |
| Clean shutdown | Release Hardening | `systemctl stop openauto-prodigy`; verify EVIOCGRAB released (touch works in launcher), BT ungrabbed, config flushed |
| Wallpaper pre-scaling | Theme Selection | Set 4K wallpaper; measure view load time; must be <100ms |

## Sources

- [PipeWire Bluetooth Telephony Support (George Kiagiadakis, Feb 2025)](https://gkiagia.gr/2025-02-20-pipewire-telephony/) -- MEDIUM confidence (new API, may evolve)
- [PipeWire Filter-Chain Documentation](https://docs.pipewire.org/page_module_filter_chain.html) -- HIGH confidence (official docs)
- [PipeWire Parametric Equalizer Documentation](https://docs.pipewire.org/page_module_parametric_equalizer.html) -- HIGH confidence (official docs)
- [WirePlumber Bluetooth Configuration](https://pipewire.pages.freedesktop.org/wireplumber/daemon/configuration/bluetooth.html) -- HIGH confidence (official docs)
- [Raspberry Pi Watchdog Forums](https://forums.raspberrypi.com/viewtopic.php?t=326779) -- MEDIUM confidence (community, but hardware-specific)
- [Dries Buytaert - Keeping Pi Online with Watchdogs](https://dri.es/keeping-your-raspberry-pi-online-with-watchdogs) -- MEDIUM confidence (practical guide)
- Project codebase: `docs/aa-display-rendering.md`, `PhonePlugin.cpp`, `AudioService.cpp`, `ThemeService.cpp` -- HIGH confidence (primary source)
- Project memory: HFP AG ownership confirmed on fresh Trixie install -- HIGH confidence (tested)

---
*Pitfalls research for: OpenAuto Prodigy feature completion and release hardening*
*Researched: 2026-03-01*
