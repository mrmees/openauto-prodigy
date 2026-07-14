# HFP + Cutover Bench Runbook — 2026-07-11 phase

Status: ACTIVE
Design: `docs/plans/2026-07-11-hfp-mic-9876-retirement-design.md` (§A1 decision tree governs)
Executor: Matthew at the bench Pi (192.168.1.149), phones: Pixel 8 (daily), Samsung S25 Ultra, Moto G Play 2024. All `busctl`/`pw-*` commands run ON THE PI as user `matt` (session bus). Record every RESULT inline; summary goes to `docs/session-handoffs.md` afterward.

Estimated time: 45-60 min. Order matters — mic first.

## 0. Substrate recording (once, 2 min)

```
pipewire --version; wireplumber --version; bluetoothctl --version
ls ~/.config/wireplumber/wireplumber.conf.d/ 2>/dev/null; echo "WIREPLUMBER_CONFIG_DIR=$WIREPLUMBER_CONFIG_DIR"
```
Expected: PipeWire 1.4.x; no user-level fragments overriding `/etc` (if any exist, note and move them aside for the session).

> RESULT (2026-07-13): PipeWire 1.4.2, WirePlumber 0.5.8, bluetoothctl 5.82. No user fragments, `WIREPLUMBER_CONFIG_DIR` unset, `/etc/wireplumber/wireplumber.conf.d/` did not exist before deploy. Service restart (step 0 of session): both D-Bus subscription lines positive (PhoneStateService + BtAudio), zero "Could not connect"; BtAudio caught live A2DP transport + AVRCP player on hot connect. NOTE: units are SYSTEM-level (`journalctl -u`, not `--user`). New warnings wishlisted: `QDBusArgument: write from a read-only object` ×3 at startup; unregistered `QDBusRawType` reading `MediaPlayer1.Track`.

## 1. Mic — A1a CVSD attempt

Deploy (from MINIMEES, if not already deployed):
```
scp config/50-prodigy-hfp-cvsd.conf matt@192.168.1.149:/tmp/ && ssh matt@192.168.1.149 'sudo mkdir -p /etc/wireplumber/wireplumber.conf.d && sudo mv /tmp/50-prodigy-hfp-cvsd.conf /etc/wireplumber/wireplumber.conf.d/ && sudo chmod 644 /etc/wireplumber/wireplumber.conf.d/50-prodigy-hfp-cvsd.conf'
```
On the Pi: `systemctl --user restart wireplumber pipewire` → re-connect the Pixel 8 (toggle BT) → place a call → read the negotiated codec DURING the call:
```
busctl --user get-property org.pipewire.Telephony /org/pipewire/Telephony/ag1 \
  org.pipewire.Telephony.AudioGatewayTransport1 Codec
```

**Validity gate:** proceed only on `y 1` (CVSD). `y 2`/`y 3` = the pin did NOT take → debug config (user fragments, restart, reconnect), do not interpret audio. No Telephony object → fix BT connection first.

**Premise re-check during the same call (all must be green before judging silence):**
```
pw-cli ls Node | grep -A2 bluez          # SCO uplink node present + running
pw-link -l | grep -i input_MONO          # mic capture_MONO -> bluez_output...input_MONO
timeout -s INT 3 pw-record /tmp/lvl.wav; aplay /tmp/lvl.wav   # records the DEFAULT source (verified = USB mic in the wpctl step below); SIGINT finalizes the wav; expect audible speech
wpctl status | grep -A4 Sources          # default source = the USB mic, not muted
```
Far-end check: call Matthew's second phone, speak at the Pi, listen on the far phone.

> RESULT (codec observed / premises / far-end audible?) 2026-07-13: Codec `y 1` (CVSD) read DURING live call, SCO nodes running. Premise check initially FAILED — bench mic on Unitek Y-247A was analog-dead: USB capture stream Running, mixer Capture 100% +23dB unmuted, but recorded pure zeros (peak 0.1%, RMS≈0). Swapped to UACDemoV1.0 USB mic → premises green → far end hears CLEARLY. **VERDICT: audible at CVSD.** MAJOR CAVEAT: the original "far end hears nothing" symptom that produced the LC3-SWB encode-bug hypothesis was observed with this dead mic in the chain — hypothesis requires re-validation with working mic before any upstream report. Gotcha logged: Telephony `Codec` property lingers while HFP connected with NO active call — only valid alongside SCO-nodes-present check.

- Audible → note quality subjectively; continue to §2.
- Silent with premises green → **STOP the codec track entirely** (skip §2); record and move to §3; the bug is not codec-specific.

## 1.5. INSERTED 2026-07-13 — stock LC3-SWB retest with working mic (Matthew-approved deviation)

Rationale: dead bench mic invalidated the evidence behind the original LC3-SWB hypothesis; stock package + no pin = direct test.

> RESULT: Drop-in removed, stock libspa, BT re-toggled. During live call: Codec `y 3` (LC3-SWB), SCO nodes running, mic link = UACDemoV1.0 capture_MONO → input_MONO, capture levels live (peak 7.4%, RMS 0.8%). Far end: **SILENT**. Same mic at CVSD (§1): clearly audible. **LC3-SWB encode bug CONFIRMED with clean premises.** Upstream report can now cite a controlled A/B: identical hardware/call path, CVSD works, LC3-SWB silent. WirePlumber gotcha: pipewire restart re-evaluates default source priority — dead Unitek won until `wpctl set-default` persisted the choice (now in default-nodes state).

## 2. Mic — A1b patched-mSBC attempt (only if §1 was audible)

Package facts (T4 build): the real binary package is **`libspa-0.2-bluetooth`** (RPi OS pipewire `1.4.2-1+rpt3`); the patched deb pins its `libspa-0.2-modules` dependency to the stock base version so installing the single deb removes nothing. Full install/revert detail: `tools/pipewire-msbc/README.md`.

```
ssh matt@192.168.1.149 'sudo rm /etc/wireplumber/wireplumber.conf.d/50-prodigy-hfp-cvsd.conf'
ssh matt@192.168.1.149 'apt-get -s install ~/pipewire-msbc/libspa-0.2-bluetooth_*+prodigy*_arm64.deb'   # sanity: MUST say "1 upgraded, 0 to remove"
ssh matt@192.168.1.149 'sudo apt install ~/pipewire-msbc/libspa-0.2-bluetooth_*+prodigy*_arm64.deb && sudo apt-mark hold libspa-0.2-bluetooth'
```
Restart `wireplumber pipewire`, reconnect phone, call, read Codec (expect `y 2` = mSBC — validity gate as above), premise re-check, far-end check.

> RESULT (2026-07-13): apt -s sanity exact ("1 upgraded, 0 newly installed, 0 to remove"); installed `1.4.2-1+rpt3+prodigy1`, held (`hi`). FIRST attempt false-started: audio-stack restart raced BlueZ profile registration — `spa.bluez5.native: RegisterProfile() failed: org.bluez.Error.NotPermitted` — leaving HFP dead; phone call ran on handset ("works" was a false positive). Fix + NEW ORDERING RULE: restart `bluetooth` → `pipewire wireplumber` → `openauto-prodigy.service` (app also needs bounce: its PipeWire device enumeration dies with the daemon — settings shows empty input list; wishlisted). Clean re-run: Codec `y 2` (mSBC) live, SCO nodes running, mic link = UACDemoV1.0 → input_MONO, levels live (peak 3.5%). Far end AUDIBLE, quality good (wideband vs CVSD). **VERDICT: mSBC IS THE SHIPPED FIX.** Protocol notes: verify ag1 exists (property read, e.g. Codec → `y 0`) BEFORE dialing; `busctl tree` unreliable for this service (no children in introspection XML); `Codec` property lingers with no call — always pair with SCO-nodes check.

- Audible → **mSBC is the shipped fix.** Keep the hold; drop-in stays deleted. Quality note vs CVSD:
- Silent → revert: `sudo apt-mark unhold libspa-0.2-bluetooth && sudo apt install --reinstall libspa-0.2-bluetooth`, re-deploy the §1 drop-in. **CVSD ships.** Finding (software-encode path implicated generally) goes in the upstream report.

## 3. L3 — DTMF into a real IVR (5 min, Pixel 8)

Dial any IVR (voicemail). During the active call:
```
busctl --user call org.pipewire.Telephony /org/pipewire/Telephony/ag1 \
  org.pipewire.Telephony.AudioGateway1 SendTones s "1"
```
IVR reacts? If NO: `can_send_dtmf` must be decoupled from `telephonyAvailable()` (hard-false own flag) — flag to next session via handoff.

> RESULT (2026-07-13): Live voicemail IVR on Pixel 8, call at mSBC (`y 2`). SendTones "1" then "8" — tones audible in call, IVR reacted correctly to both. **PASS — no decoupling work needed.** (Session interlude: bench amp died mid-session — second dead analog component of the day after the mic; USB DAC replug + amp fix restored output; software chain verified pushing 48kHz throughout.)

## 4. L4 — RejectSCO=true half (15 min, Pixel 8, AA projecting)

Baseline half already passed 2026-07-05 (default `false` stands). Now the missing half — with AA projecting:
```
busctl --user set-property org.pipewire.Telephony /org/pipewire/Telephony/ag1 \
  org.pipewire.Telephony.AudioGatewayTransport1 RejectSCO b true
```
Place/receive a call. Does call audio route via the AA session (car speakers, no SCO nodes running: `pw-cli ls Node | grep bluez`) or stay on the handset? Any AA video stutter difference vs baseline?
Decision rule (D2 §6): default flips to `true` ONLY if baseline showed unusable AA degradation AND this shows working call-over-AA audio. Reset to `false` afterward.

> RESULT / verdict on default (2026-07-13): RejectSCO=true set + confirmed; AA projecting; call placed. Zero SCO nodes during active call (reject works at transport level). Call audio does NOT route via AA — ringing AND in-call voice stay on phone handset speaker. No worse AA video stutter. Decision rule: neither condition met (baseline was usable AND no call-over-AA audio materializes — Pixel 8 just keeps audio on handset). **DEFAULT STAYS `false`.** Reset to false confirmed.

## 5. L5 — Interop rows (10 min per phone)

Per phone (Samsung S25 Ultra, then Moto G Play 2024): pair + HFP-connect, then record — codec during a call (`Codec` property), incoming ring → answer FROM HEAD UNIT, outgoing dial from the Phone view, hangup from head unit, caller-ID shown during ring, mic audible at far end (with whatever codec pin §1/§2 settled on).

> RESULT Samsung (S25 Ultra / SM-S938U, 2026-07-13): Codec mSBC (`y 2`) live-read ✓. Caller-ID number shown on ring ✓. Answer/reject FROM HEAD UNIT ✗ — popup renders during AA projection but button presses leave ZERO journal trace (touch likely falls through to AA surface); ALSO design issue: native popup shouldn't show while AA session owns call UI. WISHLISTED, does not block phase. Outgoing dial from Phone view ✓. Hangup from head unit ✓. Mic audible at far end ✓. Interop warts found: after Pixel BT-disconnect, head unit stopped advertising BT until app restart (wishlisted).
> RESULT Moto (G Play 2024 / XT2413V, 2026-07-13): Phone has NO cellular service — call-dependent items N/A (codec during call, ring/answer, outgoing dial, hangup, caller-ID, far-end mic). What was testable PASSED: pairing ✓, HFP SLC established ✓ (ag1 bound to D4:5B:51:B3:66:15, idle, RejectSCO false). One gateway flap during pairing settle (removed 18:09:39, re-established 18:09:58), then stable. Full row deferred until the Moto has service or a WiFi-calling setup.

Failures here do NOT block the phase — record and triage separately (wishlist-then-promote).

## 6. L6 tail — volume + quality (5 min)

During an active call: phone volume rocker — does downlink volume on the car output track it? Echo/level subjective check both directions.

> RESULT (2026-07-13, Pixel 8 @ mSBC): Volume rocker tracks car output ✓. Echo/levels subjectively fine both directions ✓. PASS.

## 7. Companion v1 cutover validation (with 9876 DISABLED)

Precondition: companion app migrated per `companion-9876-migration-prompt.md` (Matthew's other session) and installed on the Pixel 8.

1. On the Pi: set `companion.enabled: false` in the YAML config; restart `openauto-prodigy.service`.
2. Prove the legacy port is dead: `ss -ltnp | grep 9876` → empty; from the phone's network, a connect attempt to 9876 is refused.
3. Per-payload observables (companion app running, paired via API v1):
   - GPS: CompanionStatus widget shows GPS Active; goes stale ~30 s after killing the companion app's GPS.
   - Battery: widget % tracks the phone; charging toggle flips it.
   - SOCKS5: enable bridge on the phone → proxy row On + route active in SystemService; disable/disconnect → torn down.
   - Time: journal shows the controlled clock-step entry (`journalctl --user -u openauto-prodigy* | grep -i clock`).
   - Companion log: v1 transport only, zero legacy fallback attempts.
4. Disconnect the companion mid-session → GPS/battery clear (owner-disconnect clearing), widgets show offline.

> RESULT per payload (2026-07-13; companion app = migrated build, maintainer-confirmed):
> - Step 1: `companion.enabled: false` was already set in `~/.openauto/config.yaml` (line 262); service restarted several times during session — running instance has it.
> - Step 2 port dead: `ss -ltnp | grep 9876` empty on Pi; TCP connect from LAN actively REFUSED. ✓
> - GPS: live coords over v1 (29.8174, -98.0116, speed jitter) ✓; widget shows GPS Active ✓; location killed via adb → coords FREEZE at last fix (clearing is owner-disconnect-only, by design), widget flips stale within ~30 s ✓. IPC `companion_status` doesn't expose `gps_stale` (wishlisted).
> - Battery: 80% tracks phone (phone charge-limited to 80) ✓; charger plug → `charging: true` arrives over pipe ✓; CompanionSettings/StatusWidget bind `phoneCharging`; BatteryWidget has NO charging indicator by design (wishlisted).
> - SOCKS5: bridge on → `proxy: socks5://::ffff:10.0.0.21:1080` + redsocks actively relaying Pi HTTPS traffic (accepted client lines in openauto-system journal) ✓. Note: phone SOCKS server rejects redsocks' localhost self-probe ("not allowed by ruleset(2)" to 127.0.0.1:12345) — traffic unaffected.
> - Time: **FAIL — CUTOVER REGRESSION.** `ApiInboundState::setTime()` emits `timeReported` and NOTHING consumes it (grep-verified); legacy `CompanionListenerService::adjustClock()` (drift threshold, backward guard, timedatectl/polkit) is not wired to the v1 path. RTC-less Pi never steps clock with legacy off; NTP masks it on bench. **B2 BLOCKER** — wire `timeReported` → adjustClock logic, then re-validate this row.
> - Companion log: app logs nothing useful to logcat; verified ON THE WIRE instead — 90 s AF_PACKET sniff spanning a full app relaunch/connect: **0 packets to/from port 9876**. Zero legacy fallback attempts, packet-level proof. ✓
> - Step 4 mid-session disconnect (adb force-stop): full owner-disconnect clear — `connected:false`, battery `-1`, GPS zeroed, internet false, proxy cleared ✓; widgets show offline ✓; relaunch reconnects clean with fresh fix ✓.

## 8. Wrap-up

- Journal check: two positive D-Bus subscription log lines (PhoneStateService, BtAudioPlugin), no `Could not connect` for either.
- Copy all RESULT rows into `docs/session-handoffs.md`; leave this file's rows filled in place (this doc is the record; archived D2 doc is NOT edited).
- Anything new discovered → `docs/wishlist.md`, not into scope.

> RESULT (2026-07-13): Journal check passed at every app restart this session (several) — both subscription lines positive, zero "Could not connect". Summary in session-handoffs 2026-07-13 entry; new findings in wishlist "From HFP/9876 bench (2026-07-13)". Bench hardware casualties: the Unitek-attached mic was analog-dead (contaminated the ORIGINAL LC3-SWB evidence — retest vindicated the hypothesis anyway) and the bench amp died mid-session. Ops rule discovered: restart order `bluetooth` → `pipewire wireplumber` → `openauto-prodigy.service`; audio-stack restarts can race BlueZ RegisterProfile (NotPermitted → HFP silently dead) and always orphan the app's device enumeration.
