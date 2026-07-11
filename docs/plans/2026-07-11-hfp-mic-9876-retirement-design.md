# HFP Mic Fix + Live Checks + Port-9876 Retirement — Phase Design

Status: ACTIVE
Grounded on: `ae7bf8b` (dev == main, 2026-07-11)
Design doc lineage: extends `docs/archive/plans/2026-07-05-hfp-call-audio-design.md` (D2, esp. §11 L-results) and the 9876 gate in `docs/roadmap-current.md` (Later §, companion migration).

## 1. Why

Three items, one phase:

1. **HFP mic uplink is silent** — the far end never hears the head unit. L6
   (2026-07-05) exhaustively isolated it: mic capture good (`arecord` 19% FS),
   PipeWire capture good during a live call (18% FS), `pw-link` graph correct
   (mic → `bluez_output.<MAC>.1:input_MONO`), uplink node Running at volume
   1.00 in LC3-SWB — yet silence at the far end. Fault is in the PipeWire/BlueZ
   LC3-SWB SCO uplink encode (or the phone's decode of it), below prodigy.
   **Decision (Matthew, 2026-07-11): pin a non-SWB codec and file upstream; no
   LC3-SWB root-causing.**
2. **Deferred HFP live checks** — L3 (DTMF), L4 (`RejectSCO=true` half), L5
   (Samsung S25 Ultra / Moto G Play interop), L6 tail (volume sync, echo/levels)
   never ran.
3. **Port-9876 retirement** — every head-unit prerequisite shipped (API v1 on
   9810/9811, theme upload via web-config HTTP). The legacy
   `CompanionListenerService` survives only because the companion app still
   sends non-theme status traffic (time/GPS/battery/charging/proxy) over 9876.
   Closing the gate = companion swaps that traffic to API v1 (Matthew drives
   that repo via a handoff prompt authored here), then this repo deletes the
   service.

Out of scope: persistent call bar (stays on `docs/wishlist.md`), EQ parity
audit, any LC3-SWB investigation beyond the upstream bug report, A2DP support
decision.

## 2. Workstream A — HFP

### A1. Mic fix: force-CVSD WirePlumber pin (top priority)

**Mechanism** (verified against PipeWire 1.2 source, `spa/plugins/bluez5/
{backend-native.c,quirks.c}`): the HF advertises codecs via `AT+BAC` built from
`device_supports_codec()`. mSBC **and** LC3-SWB are both gated by the same
`SPA_BT_FEATURE_MSBC` feature bit; there is no LC3-SWB-only toggle in 1.2, and
`bluez5.codecs` is A2DP-only (why the 2026-07-05 drop-in attempt failed).
Therefore `bluez5.enable-msbc = false` removes both wideband codecs from
`+BAC` → the AG must select **CVSD** (codec id 1, classic narrowband — the
codec every BT car kit used for decades).

**Change:** ship a WirePlumber drop-in, deployed to the Pi at
`/etc/wireplumber/wireplumber.conf.d/50-prodigy-hfp-cvsd.conf`:

```
monitor.bluez.properties = {
  bluez5.enable-msbc = false
}
```

Add the file to the repo as `config/50-prodigy-hfp-cvsd.conf` (sibling of the
existing udev/polkit assets) with an `install.sh` step copying it to
`/etc/wireplumber/wireplumber.conf.d/`; deploy it to the bench Pi manually for
Stage 2. Document it in `docs/architecture.md`'s audio section.

**Bench verification (runbook §A3):** restart wireplumber → place a call →
`busctl --user get-property org.pipewire.Telephony /org/pipewire/Telephony/ag1
org.pipewire.Telephony.AudioGatewayTransport1 Codec` must return `1` → far-end
listener confirms uplink audible.

**Decision rules:**
- CVSD uplink audible → pin stays, phase item done. Draft the upstream
  PipeWire issue (LC3-SWB HFP uplink silent at far end; attach L6 + this
  bench's evidence); **Matthew reviews and approves the text before anything is
  posted externally.**
- CVSD uplink **also** silent → the codec hypothesis is dead. Stop; do not
  iterate at the bench. Record findings, regroup with
  `superpowers:systematic-debugging` as its own investigation.

### A2. Dead-slot D-Bus fixes (no bench required)

Same bug class as the USB bench saga: `a{sa{sv}}` never delivers to a
`QVariantMap` slot; the connect fails at startup ("Could not connect" in the
journal) and the slot is silently dead.

- `src/core/services/PhoneStateService.cpp` (~:319, :343): `InterfacesAdded` →
  `SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap))` — dead. Masked today
  because the initial `GetManagedObjects` scan demarshals manually and
  `PropertiesChanged` is correctly typed; the broken path is a phone that
  HFP-appears *after* startup (fresh pairing / re-pair).
- `src/plugins/bt_audio/BtAudioPlugin.cpp` (~:89, :123): same dead
  `InterfacesAdded` pattern, and its `PropertiesChanged` connect also fails per
  the startup journal (see `docs/session-handoffs.md` 2026-07-10 entry). Masked
  by agent/profile callbacks. This closes the standing "BT plugin still broken"
  note.

**Fix pattern** (copy `UsbMediaWatcher`): register
`QMap<QString,QVariantMap>` via `qDBusRegisterMetaType`, type the slots to
match the real signature. Unit tests drive the slots with synthetic payloads
(existing test seams in `tests/test_phone_state_service.cpp`); acceptance also
includes a Pi startup journal free of `Could not connect` for these two files.

### A3. Bench runbook (authored now, executed by Matthew at the bench)

One self-contained doc, `docs/plans/2026-07-11-hfp-bench-runbook.md`, ordered:

1. Mic/CVSD verification (A1) — first, it's the priority.
2. L3 — DTMF into a real IVR (`SendTones`).
3. L4 — the unexercised `RejectSCO=true` half under AA projection (Pixel 8);
   default stays `false` unless the §6 decision rule from the D2 design fires.
4. L5 — interop rows: Samsung S25 Ultra, Moto G Play 2024 (codec, ring, answer
   from head unit, outgoing dial, hangup, caller-ID).
5. L6 tail — phone volume rocker tracking, echo/level subjective check (now
   meaningful with an audible uplink).
6. Companion v1 live validation (B1's checklist tail: pairing window, reports
   visible via ApiInboundState).

Every row records its result inline in the runbook, with a summary in
`docs/session-handoffs.md` — the archived D2 doc is history and is not edited.
Interop failures on Samsung/Moto are recorded and triaged separately; they do
not block the phase.

## 3. Workstream B — Port-9876 retirement

### B1. Companion handoff prompt (authored here, run by Matthew in the companion repo)

Deliverable: `personal/openautopro/companion-9876-migration-prompt.md`
(sibling of the existing `companion-api-v1.1-handoff-prompt.md`). Contents:

- Inventory every remaining legacy-9876 sender in the companion (theme is
  already on HTTP; expected: time/clock-step, GPS, battery, charging,
  internet/proxy status).
- Map each to its API v1 `companion.proto` report (GPS/battery/connectivity/
  time already exist). Any payload with no v1 home (charging flag? proxy
  detail?) → **additive-only** proto extension per the frozen-API process,
  flagged back to this repo first — the companion never invents fields.
- Switch the runtime to the v1 transport (9810/9811) using the
  already-unit-tested codec/handshake/credential layer; preserve SOCKS5
  signaling behavior through the swap; delete or flag-off the legacy client.
- Validation checklist ending in the live pairing test (runbook §A3.6).

### B2. Head-unit teardown (gated: only after B1 validates live at the bench)

- Pre-deletion mapping (executor does this before touching code): what
  `ipcServer->setCompanionListenerService()` and
  `hostContext->setCompanionListenerService()` actually serve, and confirm the
  API-path proxy-route handling fully covers `syncProxyRoute()`
  (`src/main.cpp` ~:1025-1036). Audit `config/companion-polkit.rules`: if it
  grants the clock-step mechanism the API inbound time path also uses, it stays
  (possibly renamed); if it's listener-specific, it goes. Any consumer without
  an API-path home is a stop-and-ask.
- Delete `src/core/services/CompanionListenerService.{hpp,cpp}`, the
  `src/main.cpp` wiring (~:478-517 and ~:1025-1036), `companion.port` config
  key + schema/docs mentions, `tests/test_companion_listener.cpp`, CMake
  entries.
- Dedup the camelCase→hyphen theme-key conversion into the shared
  `ThemeService` path (`docs/wishlist.md:91`) — the IPC `install_theme` handler
  keeps a single copy.
- Close out `docs/wishlist.md` items that die with the service (RNG hygiene
  :74, dedup :91, the :53 blocker note); update `WeatherWidget.qml` comment,
  `docs/roadmap-current.md` (gate → Done), `docs/architecture.md`, and any
  `docs/INDEX.md` mentions — same commit as the deletion per repo convention.

## 4. Sequencing

| Stage | What | Needs Matthew? |
|---|---|---|
| 1 (parallel, now) | A2 dead-slot fixes; A1 drop-in prepped + deployed; A3 runbook; B1 prompt authored | No (he runs B1's prompt in the companion repo whenever) |
| 2 | Bench session: runbook top to bottom | Yes (~30-45 min, phones + Pi) |
| 3 | Post-bench: record L-results; upstream issue draft → Matthew approves; B2 teardown; codex review gate (`bash scripts/codex-review.sh`); handoff entry; ship | Approval points only |

If companion validation slips, B2 flips to `PARKED — companion not yet on v1`
and everything else ships without it.

Branch note: work happens on `worktree-hfp-mic-9876-retirement` (session
isolation); landing back onto `dev` (single-branch workflow) is decided with
Matthew at ship time.

## 5. Success criteria

- Far end hears prodigy on a live call (CVSD pinned, codec property = 1).
- L3/L4/L5 + L6-tail rows recorded; `can_send_dtmf` decision resolved by L3.
- Pi startup journal free of dead-slot `Could not connect` errors for
  `PhoneStateService` and `BtAudioPlugin`; new unit tests green.
- Companion sends all former-9876 traffic over API v1, validated live.
- `CompanionListenerService` and port 9876 gone from the binary; theme-key
  conversion single-sourced; wishlist/roadmap/docs updated in the same commits.
- Full suite + app target build green; codex review gate adjudicated; upstream
  issue text approved by Matthew before posting.

## 6. Testing strategy

- A2: TDD against the state-service test seams (synthetic `InterfacesAdded`
  payloads with the registered map type); full `ctest` + app-target build.
- B2: existing suite minus the deleted companion test; grep-level assertion
  that no `9876`/`CompanionListenerService` references survive outside
  `docs/archive/`.
- Live behavior: bench runbook is the verification instrument; no result, no
  claim (per `superpowers:verification-before-completion`).

## 7. Executor guidance (mandatory)

- HF role only (0x111e); registering 0x111f/0x1108 on the Pi = stop (root
  `AGENTS.md`).
- No ofono anywhere; telephony stays on `org.pipewire.Telephony`.
- `proto/api/` is frozen additive-only; proto gaps found during B1 come back
  here as questions, never as edits in the companion.
- Archived docs are history — L-results land in this doc, the runbook, and
  `docs/session-handoffs.md`, not by editing `docs/archive/`.
- Wishlist-then-promote: anything new found at the bench goes to
  `docs/wishlist.md`, not into this phase.
