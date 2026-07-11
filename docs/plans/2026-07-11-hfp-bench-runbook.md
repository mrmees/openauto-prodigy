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

> RESULT:

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

> RESULT (codec observed / premises / far-end audible?):

- Audible → note quality subjectively; continue to §2.
- Silent with premises green → **STOP the codec track entirely** (skip §2); record and move to §3; the bug is not codec-specific.

## 2. Mic — A1b patched-mSBC attempt (only if §1 was audible)

Package facts (T4 build): the real binary package is **`libspa-0.2-bluetooth`** (RPi OS pipewire `1.4.2-1+rpt3`); the patched deb pins its `libspa-0.2-modules` dependency to the stock base version so installing the single deb removes nothing. Full install/revert detail: `tools/pipewire-msbc/README.md`.

```
ssh matt@192.168.1.149 'sudo rm /etc/wireplumber/wireplumber.conf.d/50-prodigy-hfp-cvsd.conf'
ssh matt@192.168.1.149 'apt-get -s install ~/pipewire-msbc/libspa-0.2-bluetooth_*+prodigy*_arm64.deb'   # sanity: MUST say "1 upgraded, 0 to remove"
ssh matt@192.168.1.149 'sudo apt install ~/pipewire-msbc/libspa-0.2-bluetooth_*+prodigy*_arm64.deb && sudo apt-mark hold libspa-0.2-bluetooth'
```
Restart `wireplumber pipewire`, reconnect phone, call, read Codec (expect `y 2` = mSBC — validity gate as above), premise re-check, far-end check.

> RESULT:

- Audible → **mSBC is the shipped fix.** Keep the hold; drop-in stays deleted. Quality note vs CVSD:
- Silent → revert: `sudo apt-mark unhold libspa-0.2-bluetooth && sudo apt install --reinstall libspa-0.2-bluetooth`, re-deploy the §1 drop-in. **CVSD ships.** Finding (software-encode path implicated generally) goes in the upstream report.

## 3. L3 — DTMF into a real IVR (5 min, Pixel 8)

Dial any IVR (voicemail). During the active call:
```
busctl --user call org.pipewire.Telephony /org/pipewire/Telephony/ag1 \
  org.pipewire.Telephony.AudioGateway1 SendTones s "1"
```
IVR reacts? If NO: `can_send_dtmf` must be decoupled from `telephonyAvailable()` (hard-false own flag) — flag to next session via handoff.

> RESULT:

## 4. L4 — RejectSCO=true half (15 min, Pixel 8, AA projecting)

Baseline half already passed 2026-07-05 (default `false` stands). Now the missing half — with AA projecting:
```
busctl --user set-property org.pipewire.Telephony /org/pipewire/Telephony/ag1 \
  org.pipewire.Telephony.AudioGatewayTransport1 RejectSCO b true
```
Place/receive a call. Does call audio route via the AA session (car speakers, no SCO nodes running: `pw-cli ls Node | grep bluez`) or stay on the handset? Any AA video stutter difference vs baseline?
Decision rule (D2 §6): default flips to `true` ONLY if baseline showed unusable AA degradation AND this shows working call-over-AA audio. Reset to `false` afterward.

> RESULT / verdict on default:

## 5. L5 — Interop rows (10 min per phone)

Per phone (Samsung S25 Ultra, then Moto G Play 2024): pair + HFP-connect, then record — codec during a call (`Codec` property), incoming ring → answer FROM HEAD UNIT, outgoing dial from the Phone view, hangup from head unit, caller-ID shown during ring, mic audible at far end (with whatever codec pin §1/§2 settled on).

> RESULT Samsung:
> RESULT Moto:

Failures here do NOT block the phase — record and triage separately (wishlist-then-promote).

## 6. L6 tail — volume + quality (5 min)

During an active call: phone volume rocker — does downlink volume on the car output track it? Echo/level subjective check both directions.

> RESULT:

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

> RESULT per payload:

## 8. Wrap-up

- Journal check: two positive D-Bus subscription log lines (PhoneStateService, BtAudioPlugin), no `Could not connect` for either.
- Copy all RESULT rows into `docs/session-handoffs.md`; leave this file's rows filled in place (this doc is the record; archived D2 doc is NOT edited).
- Anything new discovered → `docs/wishlist.md`, not into scope.
