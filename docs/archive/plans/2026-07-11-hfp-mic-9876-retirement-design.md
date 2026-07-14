# HFP Mic Fix + Live Checks + Port-9876 Retirement — Phase Design

Status: COMPLETED 2026-07-14
Grounded on: `ae7bf8b` (dev == main, 2026-07-11)
Revised: 2026-07-11 after sol (gpt-5.6-sol) design review — verdict REWORK,
12/12 findings confirmed and incorporated (3 P1, 7 P2, 2 P3; none dismissed).
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
3. **Port-9876 retirement** — API v1 (9810/9811) and the HTTP theme endpoint
   shipped; the legacy `CompanionListenerService` survives only because the
   companion app still sends non-theme status traffic (time/GPS/battery/
   charging/proxy) over 9876. Closing the gate requires, in order: head-unit
   **inbound-state parity + consumer migration** (B0 — sol found the API path
   is not yet at semantic parity and four QML surfaces still read the legacy
   object), the companion swapping its traffic to v1 (B1, Matthew drives that
   repo via a handoff prompt authored here), a cutover validation with 9876
   **disabled**, and only then deletion (B2).

Out of scope: persistent call bar (stays on `docs/wishlist.md`), EQ parity
audit, any LC3-SWB investigation beyond the upstream bug report, A2DP support
decision.

## 2. Workstream A — HFP

### A1. Mic fix: two prepped codec interventions, one bench (top priority)

**Mechanism** (verified against PipeWire source at 1.2, 1.4 — the Pi's
series — and master, plus repo-wide code search of pipewire and wireplumber):
the HF advertises codecs via `AT+BAC` built from `device_supports_codec()`
(`spa/plugins/bluez5/backend-native.c`). mSBC **and** LC3-SWB are both gated
by the same `SPA_BT_FEATURE_MSBC` feature bit; no LC3-SWB-only toggle exists
in any branch (`bluez5.enable-swb` is an AI hallucination — zero hits
repo-wide), and `bluez5.codecs` feeds only the A2DP/media codec machinery in
`bluez5-dbus.c` — the native HFP backend never reads it (why the 2026-07-05
drop-in attempt failed live). **Consequence: mSBC-without-LC3-SWB is
unreachable by configuration; the only levers are the shared feature bit
(→ CVSD) or a source patch (→ mSBC).** Matthew wants mSBC quality, so both
interventions are prepped before the bench and tested in one sitting:

**A1a — CVSD drop-in (config, the diagnostic).** Ship
`config/50-prodigy-hfp-cvsd.conf` (sibling of the existing udev/polkit
assets), installed to `/etc/wireplumber/wireplumber.conf.d/` (directory
creation + fixed permissions) by **both** `install.sh` and
`install-prebuilt.sh` — installer wiring happens only if A1a ends up the
shipped fix. Deploy to the bench Pi manually for Stage 2.

```
monitor.bluez.properties = {
  bluez5.enable-msbc = false
}
```

Note: CVSD is encoded in the BT controller hardware, while mSBC and LC3-SWB
both use software encode over transparent eSCO — so A1a working proves the
uplink transport, but does not by itself prove A1b will work.

**A1b — patched-mSBC PipeWire build (the preferred fix).** Pull the Debian
`pipewire` source package matching the Pi (1.4.x), add a minimal quilt patch
making `device_supports_codec()` return false for `HFP_AUDIO_CODEC_LC3_SWB`
(≈3 lines — LC3-SWB drops out of `+BAC`, mSBC stays), rebuild for arm64 in
the existing Docker aarch64 infra, install on the Pi with `apt-mark hold
libspa-0.2-bluetooth` (T4 finding: that is the real Trixie/RPi-OS binary
package; the Pi runs RPi OS pipewire `1.4.2-1+rpt3`, and the kit pins the
strict `libspa-0.2-modules` dep so the lone deb installs cleanly).
Keep the patch + build script in `tools/` so the package can be rebuilt when
Debian bumps pipewire. If mSBC ships as the fix, document the hold + rebuild
procedure in `docs/architecture.md` and the installers get NO drop-in.

**Bench verification (runbook §A3), in order:**
1. Record the substrate: `pipewire --version`, `wireplumber --version`,
   `bluetoothctl --version` (expected PipeWire 1.4.2 per the D2
   implementation notes — the archived doc, not this spec, is authoritative
   for what the Pi runs).
2. Check `~/.config/wireplumber/wireplumber.conf.d/` and
   `WIREPLUMBER_CONFIG_DIR` for user-level fragments that would override the
   `/etc` drop-in.
3. Restart wireplumber, HFP-connect, place a call, read
   `busctl --user get-property org.pipewire.Telephony
   /org/pipewire/Telephony/ag1
   org.pipewire.Telephony.AudioGatewayTransport1 Codec`.

**Per-attempt validity gate (applies to BOTH A1a and A1b):** an intervention
is only "in effect" when the expected codec is observed — A1a expects
`Codec = 1`, A1b expects `Codec = 2`. Any other value = the intervention did
not take; debug the config/package path (user-fragment overrides, fragment
parsing, service environment, restart/reconnect, package actually installed),
do not interpret audio. Transport absent → restore the BT/HFP connection
first. And silence is only meaningful with premises re-established during the
same call — a wireplumber restart can silently change the default source,
links, or node states, so repeat the minimal L6 controls each attempt: SCO
uplink node Running; intended mic linked to `bluez_output…:input_MONO`
(`pw-link -l`); nonzero capture level from the mic path; downlink audible;
neither end muted.

**Bench order + decision tree:**
1. **A1a (CVSD) first.** Audible → uplink transport works; bug is in the
   wideband/software-encode path. Silent with premises green → codec
   hypothesis dead; skip A1b (it shares the transport), stop, record, regroup
   as its own `superpowers:systematic-debugging` investigation.
2. **A1b (mSBC) second** (only if A1a was audible). Audible → **mSBC is the
   shipped fix** (remove the A1a drop-in; hold + rebuild procedure
   documented). Silent → the bug covers software-encode/transparent-eSCO
   generally, not just LC3-SWB; **CVSD becomes the shipped fix** (installer
   wiring for the drop-in) and that finding goes in the upstream report.
3. Either way: draft the upstream PipeWire issue (LC3-SWB HFP uplink silent
   at far end, plus the A1b datapoint; L6 + this bench's evidence). **Matthew
   reviews and approves the text before anything is posted externally.**

### A2. Dead-slot D-Bus fixes (no bench required)

Same bug family as the USB bench saga, but sol's review sharpened the
diagnosis — the two files fail differently:

**`PhoneStateService`** (`src/core/services/PhoneStateService.cpp` ~:319,
:343): `InterfacesAdded` connected to a `QVariantMap` slot — the `a{sa{sv}}`
type mismatch; connect fails at startup. Masked because the initial
`GetManagedObjects` scan demarshals manually and `PropertiesChanged` is
correctly typed; the broken path is a phone that HFP-appears *after* startup
(fresh pairing / re-pair). Additional defect (sol P2.4): the current
`onInterfacesAdded` discards its payload (`Q_UNUSED`) and performs a second,
racy live D-Bus read — and no test seam can exercise it.
**Fix:** register `QMap<QString,QVariantMap>` (the `UsbMediaWatcher`
pattern), retype the slot, and refactor to a testable
`adoptBluezDevice(path, propertyMap)` helper consumed by BOTH the initial
scan and the signal handler — the handler uses the delivered `Device1`
property map instead of re-reading the bus. Unit tests drive
`adoptBluezDevice` and the retyped slot with synthetic payloads (extend
`tests/test_phone_state_service.cpp` with this new seam — it does not exist
yet).

**`BtAudioPlugin`** (`src/plugins/bt_audio/BtAudioPlugin.cpp` ~:83-106,
disconnects ~:119-133): all three handlers (`onInterfacesAdded`,
`onInterfacesRemoved`, `onPropertiesChanged`) are declared under plain
`private:` — **not slots at all** (`BtAudioPlugin.hpp` ~:109-116), so all
three string-based connects fail regardless of argument types.
**Fix:** move all three under `private slots:`; retype **only**
`InterfacesAdded` to the registered map alias (`InterfacesRemoved`'s
`(o, as)` and `PropertiesChanged`'s `(s, a{sv}, as)` signatures are already
correct as written); add a trailing `QDBusMessage` parameter to
`onPropertiesChanged` and filter by sender path against
`transportPath_`/`playerPath_` (sol P2.5 — once the connect works, every
BlueZ object's updates would otherwise stomp the selected transport/player);
capture and log every `QDBusConnection::connect()` return value (the
`TelephonyClient`/`UsbMediaWatcher` pattern); dedicated test seam + tests,
including one asserting updates from an unrelated object path are ignored.

**Acceptance for both:** unit tests green; **positive** startup logging shows
every D-Bus subscription returned true on the Pi (not merely the absence of
"Could not connect").

### A3. Bench runbook (authored now, executed by Matthew at the bench)

One self-contained doc, `docs/plans/2026-07-11-hfp-bench-runbook.md`, ordered:

1. Substrate recording + override check + the A1a→A1b codec sequence per
   A1's decision tree — first, it's the priority.
2. L3 — DTMF into a real IVR (`SendTones`); resolves the `can_send_dtmf`
   coupling question.
3. L4 — the unexercised `RejectSCO=true` half under AA projection (Pixel 8);
   default stays `false` unless the D2 §6 decision rule fires.
4. L5 — interop rows: Samsung S25 Ultra, Moto G Play 2024 (codec, ring,
   answer from head unit, outgoing dial, hangup, caller-ID).
5. L6 tail — phone volume rocker tracking, echo/level subjective check (now
   meaningful with an audible uplink).
6. Companion v1 cutover validation — **with the legacy listener disabled**
   (`companion.enabled: false`, restart; `ss -ltnp` shows nothing on 9876 and
   a connection attempt is refused). Per-payload observable checks (§5).

Every row records its result inline in the runbook, with a summary in
`docs/session-handoffs.md` — the archived D2 doc is history and is not edited.
Interop failures on Samsung/Moto are recorded and triaged separately; they do
not block the phase.

## 3. Workstream B — Port-9876 retirement

### B0. Head-unit inbound-state parity + consumer migration (NEW — before any cutover)

Sol's review (P2.6/P2.7) established that proto coverage is **complete** — no
additive fields needed — but `ApiInboundState` is not at semantic parity with
the legacy service, and four QML surfaces plus IPC still read the legacy
object. Deleting the service without this work breaks live UI. Tasks:

- **Report semantics parity** (`src/core/api/ApiRequestHandlers.cpp`,
  `ApiInboundState.{hpp,cpp}`): consume and expose GPS `age_ms`, accuracy,
  bearing (currently dropped, handler forwards only lat/lon/speed); define
  GPS staleness (report age + local elapsed time — `gpsValid` currently never
  goes stale); track last-writer ownership **per report type** and clear that
  report's state when its owner disconnects (legacy cleared GPS/battery/
  connectivity on disconnect; API today clears only proxy ownership).
- **"Companion connected" semantics:** define as report ownership/freshness
  (an authenticated API session alone is not evidence — web widgets and other
  clients connect too), or delete the UI concept. Default: freshness-based.
- **Consumer migration** (while legacy still runs — dual-read window is fine,
  dual-write is not): `qml/widgets/BatteryWidget.qml`,
  `qml/widgets/WeatherWidget.qml`, `qml/widgets/CompanionStatusWidget.qml`,
  `qml/applications/settings/CompanionSettings.qml` (decide: migrate to API
  pairing/state vs. merge into API settings surface — executor proposes,
  Matthew picks), the `CompanionService` root-context exposure
  (`src/main.cpp` ~:1223), and `IpcServer` `companion_status`
  (`src/core/services/IpcServer.cpp` ~:400-415) — rebind to inbound state or
  delete with its callers.
- Tests: disconnect/reset clearing, stale-GPS transition, and the migrated
  QML settings-menu structure (`tests/test_settings_menu_structure.cpp`).

### B1. Companion handoff prompt (authored here, run by Matthew in the companion repo)

Deliverable: `personal/openautopro/companion-9876-migration-prompt.md`
(sibling of the existing `companion-api-v1.1-handoff-prompt.md`). Contents:

- Inventory every remaining legacy-9876 sender in the companion (theme is
  already on HTTP; expected: time/clock-step, GPS, battery, charging,
  internet/proxy status).
- **Coverage is confirmed complete** against `proto/api/companion.proto`
  (GPS incl. age/accuracy/bearing :22-43; battery + charging :46-53;
  internet/SOCKS5 state/port/password :55-73; time/timezone :75-86). No proto
  changes. If the companion session believes otherwise, it flags back here —
  it never edits proto.
- Switch the runtime to the v1 transport (9810/9811) using the
  already-unit-tested codec/handshake/credential layer; **transports are
  mutually exclusive during cutover** — no dual publishing, in particular no
  simultaneous legacy+v1 proxy or time publishers (either path's disconnect
  tears down system state the other just applied); preserve SOCKS5 signaling
  behavior through the swap; delete or flag-off the legacy client.
- Validation checklist ending in the live cutover test (runbook §A3.6, legacy
  listener disabled) and a companion-log check: v1 transport only, zero
  legacy fallback attempts.

### B2. Head-unit teardown (gated: only after the §A3.6 cutover validation passes)

Full inventory (sol P2.6/P2.9/P3.11 — the pre-review spec missed most of it):

- Delete `src/core/services/CompanionListenerService.{hpp,cpp}`, `main.cpp`
  wiring (~:478-517, ~:1025-1036), `tests/test_companion_listener.cpp`, CMake
  entries.
- Remove the service from `IHostContext`/`HostContext`
  (`src/core/plugin/IHostContext.hpp` :16,:37; `HostContext.hpp`) and update
  the mocks in `tests/test_plugin_model.cpp` / `tests/test_plugin_manager.cpp`.
- Retire the **whole `companion.*` config namespace** (`companion.enabled` +
  `companion.port`), in `main.cpp`, both installers' default-config blocks
  (`install.sh` ~:1421, `install-prebuilt.sh` ~:302), and
  `docs/reference/settings-tree.md`.
- Audit `config/companion-polkit.rules`: if it grants the clock-step
  mechanism the API inbound time path also uses, it stays (possibly renamed);
  if listener-specific, it goes. Any consumer without an API-path home is a
  stop-and-ask.
- Dedup the camelCase→hyphen theme-key conversion into the shared
  `ThemeService` path (`docs/wishlist.md:91`) — the IPC `install_theme`
  handler keeps a single copy.
- Legacy user state (`~/.openauto/companion.key`, `~/.openauto/vehicle.id`):
  **retained harmlessly** — never silently delete user state; note them as
  orphaned in the upgrade docs.
- Sweep stale references: comments in `ThemeInstallRequest.cpp`,
  `ApiInboundState.cpp`, `ApiRequestHandlers.*`;
  `docs/reference/plugin-api.md:298`; `WeatherWidget.qml` comment.
- Close out `docs/wishlist.md` items that die with the service (RNG hygiene
  :74, dedup :91, the :53 blocker note); update `docs/roadmap-current.md`
  (gate → Done), `docs/architecture.md`, `docs/INDEX.md` — same commit as the
  deletion per repo convention.

## 4. Sequencing — staged cutover

| Stage | What | Needs Matthew? |
|---|---|---|
| 1 (parallel, now) | A2 dead-slot fixes; A1a drop-in + A1b patched deb both prepped (deb staged on the Pi, not installed); A3 runbook; **B0 parity + consumer migration (deployed to Pi)**; B1 prompt authored | No (he runs B1's prompt in the companion repo whenever) |
| 2 | Bench session, runbook top to bottom; §A3.6 runs with `companion.enabled: false` — companion proven on v1 exclusively | Yes (~45-60 min, phones + Pi) |
| 3 | Post-bench: record L-results; upstream issue draft → Matthew approves; B2 teardown; codex review gate (`bash scripts/codex-review.sh`); handoff entry; ship | Approval points only |

If companion validation slips, B2 flips to `PARKED — companion not yet on v1`;
A-workstream items and B0 ship regardless (B0 is pure improvement — the legacy
service keeps working beside it during the dual-read window).

Branch note: work happens on `worktree-hfp-mic-9876-retirement` (session
isolation); landing back onto `dev` (single-branch workflow) is decided with
Matthew at ship time.

## 5. Success criteria

- Far end hears prodigy on a live call — expected codec property observed
  for the attempt (1 for CVSD, 2 for the mSBC patch), minimal L6 controls
  green during the same call, then audibility confirmed; shipped fix is mSBC
  if A1b passes, else CVSD.
- L3/L4/L5 + L6-tail rows recorded; `can_send_dtmf` decision resolved by L3.
- Positive startup logging: every PhoneStateService/BtAudioPlugin D-Bus
  subscription logged true on the Pi; new unit tests (incl. sender-path
  filtering and `adoptBluezDevice`) green.
- Per-payload cutover observables, legacy listener disabled: GPS position +
  staleness visible in migrated UI; battery %/charging toggles track the
  phone; SOCKS route goes active in SystemService and tears down on
  disable/disconnect; time report produces its controlled journal entry;
  `ss -ltnp` shows no 9876 listener and connection attempts are refused;
  companion log shows v1 only.
- `CompanionListenerService`, `companion.*` config, and the 9876 listener
  gone from the binary (`git grep -I` over tracked files, `docs/archive/`
  excluded); theme-key conversion single-sourced; wishlist/roadmap/docs
  updated in the same commits.
- Full suite + app target build green; codex review gate adjudicated; upstream
  issue text approved by Matthew before posting.

## 6. Testing strategy

- A2/B0: TDD via the new seams (`adoptBluezDevice`, BtAudio test target,
  inbound-state disconnect/staleness tests); meta-object signature tests for
  all repaired subscriptions; full `ctest` + app-target build per repo rule.
- B2: suite minus deleted tests; `git grep -I -e 9876 -e CompanionListener`
  over tracked files with `docs/archive/` excluded must return only
  intentional history.
- Live behavior: bench runbook is the verification instrument; no result, no
  claim (per `superpowers:verification-before-completion`).

## 7. Executor guidance (mandatory)

- HF role only (0x111e); registering 0x111f/0x1108 on the Pi = stop (root
  `AGENTS.md`).
- No ofono anywhere; telephony stays on `org.pipewire.Telephony`.
- `proto/api/` is frozen additive-only; coverage is confirmed complete for
  this phase — any perceived gap comes back as a question, never an edit.
- Never silently delete user state (`companion.key`, `vehicle.id`).
- Archived docs are history — new results land in this doc, the runbook, and
  `docs/session-handoffs.md`, not in `docs/archive/`.
- Wishlist-then-promote: anything new found at the bench goes to
  `docs/wishlist.md`, not into this phase.
