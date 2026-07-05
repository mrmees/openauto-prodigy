# HFP Call-Control Path — Decision Record (Phase D1)

**Date:** 2026-07-05
**Status:** DECIDED (pending live confirmation on Pi — see §6; confirmation affects config, not the decision or schema shape).
**Supersedes:** the call-control claims in `docs/hfp-stack-spike.md` (2026-02-18), which contained two errors corrected in §2.

## 1. Decision

**Use PipeWire's native BlueZ5 HFP backend together with its Telephony D-Bus API (`org.pipewire.Telephony.*`) for all call control.** PhonePlugin becomes a D-Bus client of PipeWire's telephony service — the same integration pattern the BT-audio plugin already uses against BlueZ. No ofono daemon, no self-built RFCOMM AT stack.

## 2. Corrections to the 2026-02-18 spike doc

1. **Role inversion:** the spike claimed the head unit needs the HFP **AG** profile (0x111f, "Pi acts as the car's hands-free system"). Wrong: a car head unit is the **HF (Hands-Free) role, UUID 0x111e**; the *phone* is the AG. In PipeWire's Telephony API the `AudioGateway1` object represents the **remote phone** — naming that confirms the model. (`src/plugins/phone/PhonePlugin.hpp`'s header comment repeats the same inversion — executor task to fix.)
2. **"Send AT commands via BlueZ D-Bus"** — no such API exists in BlueZ core. Call control comes from whoever owns the HFP RFCOMM channel; with the native backend that is PipeWire, and PipeWire 1.4 exposes it via D-Bus (below).

## 3. Evidence

Extracted from the **exact Debian Trixie package the Pi uses** (`libspa-0.2-bluetooth` 1.4.2-1; amd64 inspected, arm64 is the same source), via `strings` on `libspa-bluez5.so` — introspection XML embedded in the binary:

- **`org.pipewire.Telephony.AudioGateway1`** (one object per connected phone/AG): `Dial(number)`, `SwapCalls()`, `ReleaseAndAnswer()`, `ReleaseAndSwap()`, `HoldAndAnswer()`, `HangupAll()`, `CreateMultiparty()`, `SendTones(tones)` (DTMF), property `Address`.
- **`org.pipewire.Telephony.AudioGatewayTransport1`**: `State`, `Codec`, **`RejectSCO` (readwrite)**, `Activate()`.
- **`org.pipewire.Telephony.Call1`** (one object per call): `Answer()`, `Hangup()`, properties `LineIdentification`, `IncomingLine`, `Name`, `Multiparty`, `State`.
- **ofono-compat aliases** (`org.ofono.Manager`, `org.ofono.VoiceCallManager`, `org.ofono.VoiceCall`) served by PipeWire itself — config `bluez5.telephony.provide-ofono`.
- Config keys present: `bluez5.telephony-dbus-service`, `bluez5.telephony.use-system-bus`, `bluez5.telephony.default-reject-sco`.
- `pipewire` 1.4.2-1 `NEWS.gz`: "A new Telephony D-BUS API compatible with ofono was added" (1.4 series); Bluetooth section: "Add a Telephony DBUS API."
- Diagnostic string "Bluetooth Telephony service **disabled by configuration**" ⇒ the service is config-gated; enablement is part of §6.

## 4. Options considered

| Option | Verdict | Why |
|---|---|---|
| **A. PipeWire native backend + Telephony D-Bus API** | **CHOSEN** | Zero protocol code on our side; SCO/codec (CVSD/mSBC/LC3-SWB) handled by PipeWire; full call-control surface incl. DTMF, hold/swap, multiparty; D-Bus client pattern already proven in bt_audio plugin; ships in the distro we target |
| B. Own RFCOMM AT stack (register HF Profile1 ourselves) | Rejected | Conflicts with PipeWire's ownership of the HF profile (one owner per RFCOMM channel — we'd have to disable PipeWire HFP and re-implement SCO routing ourselves); weeks of AT/SCO work; no upside |
| C. ofono daemon + PipeWire ofono backend | Rejected | Redundant: PipeWire provides the ofono-compatible API without the daemon; extra moving part, ModemManager conflict, the 2026-02 spike already recommended against ofono |
| D. Keep UI mock (status quo) | Rejected | dial/answer/hangup/DTMF are stubs (`PhonePlugin.cpp:346,398`); roadmap "Now" item requires real call audio |

## 5. Consequences

1. **Proto freeze gate (Phase B) is lifted.** The API's phone domain models: status per call (`Call1.State` semantics — incoming/dialing/alerting/active/held/waiting per HFP indicators), actions dial/answer/hangup/DTMF/hold-swap, all behind capability flags whose runtime truth comes from Telephony object discovery. Schema shape is now known.
2. **PhonePlugin architecture (D2 input):** replace mock call methods with a `TelephonyClient` D-Bus adapter (ObjectManager watch on PipeWire's telephony service; per-AG and per-call proxies). `PhoneStateService` maps `Call1` states into `ICallStateProvider` (extend enum: today's Idle/Ringing/Active is too narrow — add Dialing/Held at minimum, per §3 semantics).
3. **AA coexistence lever (D2 input):** `RejectSCO` + `bluez5.telephony.default-reject-sco` is the designed mechanism for "AA owns call audio while projecting; HFP takes over when AA is gone" — toggle per AA session state rather than tearing profiles down.
4. **D-Bus deserialization gotcha applies** (CLAUDE.md): manual `beginMap()/endMap()` for QVariantMap extraction in the new client code.
5. **Delete prodigy's HFP AG + HSP HS profile registration** (`BluetoothManager.cpp:540-586`, registrations at `:551-552`). Live evidence (Pi, 2026-07-05): the registration **fails every boot** with `"UUID already registered"` — PipeWire's native backend owns 0x111f and 0x1108 — and AA wireless discovery works regardless (its SDP record is separate, registered at boot per the same log). The `BluezProfile1Handler` fds land in `profileFds_`, which nothing ever reads (only closed in cleanup). Worse, it's a latent hazard: prodigy is a *system* service, WirePlumber a *user* service — if boot ordering ever flips, prodigy would WIN the registration race and silently break PipeWire telephony. Remove `registerProfiles()`/`unregisterProfiles()`, `BluezProfile1Handler`, and `profileFds_` wholesale.

## 6. Live verification checklist (Pi; ~15 min once Pi is powered and phone available)

Pre-req: none of this changes the decision — it pins config values and confirms interop.

1. ~~Service discovery~~ **DONE 2026-07-05 (Pi, live):** `org.pipewire.Telephony` is running on the **session bus**, owned by WirePlumber; object root `/org/pipewire/Telephony` present with no children while no phone is connected.
2. ~~Enablement key~~ **DONE 2026-07-05 (Pi, live):** enabled **by default** in Trixie's build — no telephony key exists in any shipped config and the service is up anyway. **No config drop-in needed.**
3. ~~Phone test~~ **DONE 2026-07-05 (Pixel 8, live).** Results:
   - `AudioGateway1` object `/org/pipewire/Telephony/ag1` appeared on HFP connect; codec negotiated **LC3-SWB** (super-wideband) with zero config.
   - **Outgoing via API:** `Dial("5127733773")` returned `/org/pipewire/Telephony/ag1/call1`, the phone placed the call, `HangupAll()` ended it; transport `State` went `active → idle`.
   - **Incoming via API:** during ring, `call1` carried `State: "incoming"` and `LineIdentification: "+15127733773"` (E.164 with `+`); **`Answer()` connected the call** and SCO went `running` both directions. Head-unit answer confirmed working.
   - SCO nodes: `bluez_input.<MAC>.0` / `bluez_output.<MAC>.1`, `api.bluez5.profile: headset-audio-gateway`; node state `suspended → running` is the reliable in-call signal.
4. **Observed semantics executors MUST design around:**
   - **`Call1` objects are ephemeral in 1.4.2** — they exist during call *setup* (incoming ring / outgoing dialing) and disappear once the call is active (HFP `callsetup` indicator returning to 0 appears to destroy them). Answer during ring works; mid-call per-call control does not exist.
   - **`GetManagedObjects` does not enumerate call children** (only `ag1` is listed even while a call object is introspectable). Track calls via `InterfacesAdded`/`InterfacesRemoved` signals — never by polling enumeration.
   - **Mid-call control is AG-level:** `HangupAll()` (verified) and `SendTones()` (untested) on `AudioGateway1`; "call active" UI state comes from transport `State` + SCO node state, not from a `Call1` object.
   - Transport `State: "active"` was also observed pre-call right after HFP connect — treat it as SLC/SCO-related, not authoritative call state; combine signals.
   - Consequence for `ICallStateProvider`: hold/swap/multiparty are **not reachable** in 1.4.2's surface for existing calls — capability flags for those stay false in API v1 until PipeWire grows persistent call objects.
5. Remaining (executor checklist, non-blocking): `SendTones()` DTMF during an active call; `RejectSCO` toggle behavior (the AA-coexistence lever); interop pass with Samsung S25 Ultra and Moto G Play 2024 (Pixel 8 is the daily phone and is confirmed); mic/speaker audio quality check in-car.
6. Results recorded here 2026-07-05; session handoff pending at sprint end.

## 7. Executor guidance

- Do NOT install ofono, and do not enable `provide-ofono` — we use the native `org.pipewire.Telephony` interfaces directly.
- The telephony D-Bus service may be on the session bus (app runs in the user session — fine) or system bus depending on config; discover, don't hardcode, and record the choice in config docs.
- HF vs AG: if you find yourself registering profile 0x111f on the Pi, stop — you've inverted the roles again.
- All PipeWire D-Bus signals land on whatever thread Qt's D-Bus dispatcher uses (main) — keep handlers light; heavy work via queued invokes.
