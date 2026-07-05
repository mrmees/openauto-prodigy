# HFP Call Audio — Design (Phase D2)

**Date:** 2026-07-05
**Status:** DESIGNED — implementation deferred to executor (plan: `docs/superpowers/plans/2026-07-05-hfp-call-audio-implementation.md`).
**Grounded against:** `fable-design-sprint` at `875feaf` (API v1 proto FROZEN). Re-verify substrate if the branch has moved past the files cited here.
**Builds on:** `2026-07-05-hfp-call-control-decision.md` (D1 — PipeWire Telephony D-Bus API, live-verified semantics §6), `2026-07-05-extensibility-architecture-design.md` (rails R1/R4), `2026-07-06-external-api-v1-design.md` §8.4/§9.3 + frozen `proto/api/phone.proto`.
**Rails consumed:** R1 (API binds providers — `TelephonyClient` stays behind `IPhoneStateService`; no D-Bus type crosses the provider boundary), R4 (all mutation through provider invokables).

## 1. Purpose & scope

D1 decided *how* call control works (PipeWire's `org.pipewire.Telephony` D-Bus API). D2 designs the code that consumes it: the D-Bus adapter, the call state machine behind the widened `ICallStateProvider`, SCO audio routing relative to AudioService's stream model, coexistence with Android Auto, and the deletion of the dead HFP AG profile registration. Out of scope: contacts/phonebook (PBAP), call history, multi-phone, hold/swap/multiparty (not reachable in PipeWire 1.4.2 — D1 §6.4; API v1 flags are hard-false).

## 2. Substrate findings (2026-07-05 code inventory, this session)

Verified against `875feaf`; these correct or extend what D1 recorded:

1. **`PhoneStateService` already exists and is half of what we need.** `src/core/services/PhoneStateService.cpp` monitors BlueZ `Device1` for HFP-capable connected devices (UUID `0000111e`/`0000111f` in the *phone's* advertised list — that is remote-UUID detection, not a role inversion) and maintains `phoneConnected`/`deviceName` correctly. Its call-state side is a mock (`answer()`/`hangup()` flip local state; `setIncomingCall()` is a test hook). D2 keeps the BlueZ device watch, replaces the mock call machine.
2. **`PhonePlugin` duplicates the same BlueZ monitoring privately** (`PhonePlugin.hpp:107-116`) and never consults `hostContext->callStateProvider()`. Its dial/answer/hangup/sendDTMF are UI mocks (`PhonePlugin.cpp:346,398`). D2 guts the duplicate monitoring and makes the plugin a pure view over `IPhoneStateService`.
3. **Live QML bug:** `qml/applications/phone/IncomingCallOverlay.qml:11` shows the overlay when `CallStateProvider.callState === 2`, commented "(Ringing)". In `ICallStateProvider` the values are `Idle=0, Ringing=1, Active=2` — the overlay actually triggers on *Active*. The `=== 2` came from `PhonePlugin`'s private enum (`Ringing=2`). Never noticed because nothing real ever drove the provider. Fix ships with D2.
4. **The dead AG registration is deletable with zero replacement work.** D1 §5.5's indictment of `BluetoothManager::registerProfiles()` (`BluetoothManager.cpp:540-586`) holds, and the one thing wired to it — `profileNewConnection` → `cancelAutoConnect` (`:378-379`, "true success signal" comment at `:282`) — is redundant: `updateConnectedDevice()` (`:736-740`) already cancels auto-connect on any `Device1.Connected` transition, and *that* path actually fires today while the profile path never has (registration fails every boot).
5. **The "3-stream model" is AA-session-scoped.** `AndroidAutoOrchestrator.cpp:332-334` creates "AA Media"(prio 50)/"AA Speech"(60)/"AA System"(40) on session start and destroys them at `:741-743` on teardown. Note the pre-existing quirk: the *Phone* EQ engine is attached to the "AA System" stream (`:343`) — not D2's to fix, recorded for housekeeping.
6. **Audio focus/ducking and master volume are prodigy-stream-scoped.** `AudioService::applyDucking()` and `setMasterVolume()` iterate `streams_` (prodigy-created `pw_stream`s) only. SCO nodes created by PipeWire's bluez plugin are invisible to both.
7. **Bus environment is already correct.** The systemd unit (install.sh:1624-1628) runs prodigy as the user with `XDG_RUNTIME_DIR` and `DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/<uid>/bus` — `QDBusConnection::sessionBus()` reaches WirePlumber's `org.pipewire.Telephony` (session bus, D1 §6.1) with no unit changes.
8. **`AudioService` capture API (`openCaptureStream`) has zero call sites** — the AA microphone channel is unimplemented. Nothing in the call-audio path below depends on it.

## 3. SCO audio routing — who owns the phone stream

**Answer: nobody in prodigy. HFP call audio does not flow through AudioService's stream model in v1, and that is a design decision, not an omission.**

Evidence and reasoning:

- PipeWire's bluez plugin exposes the HFP audio path as *stream-class* nodes (`bluez_input.<MAC>` = far-end voice, `bluez_output.<MAC>` = uplink to phone, `api.bluez5.profile: headset-audio-gateway`). WirePlumber links them to the default sink/source exactly like it links the A2DP music stream — which is why D1's live test saw SCO go `running` **both directions with zero configuration** (D1 §6.3). The routing problem is already solved by the platform.
- The 3-stream model exists to bridge AA's ASIO-thread PCM into PipeWire. HFP has no PCM to bridge — PipeWire owns the transport end to end. Inserting prodigy loopback streams (capture SCO → replay into a prodigy "Phone" stream, capture mic → replay into uplink) would buy focus/EQ/master-volume integration at the cost of two extra ring buffers of latency on a path where latency degrades the phone's echo cancellation, plus WirePlumber suppression rules to stop the default auto-link. Bad trade for v1.
- **No-contention invariant:** AA streams exist only during an AA session (§2.5); the coexistence policy (§6) ensures SCO carries call audio primarily when AA is absent. The two audio paths structurally avoid competing, so the lack of cross-path ducking has no everyday consequence. During an A2DP+HFP call, the phone pauses its own music (standard phone behavior).

Accepted, documented consequences (v1 limitations, each with an additive escape hatch):

| Limitation | Consequence | Later fix if wanted |
|---|---|---|
| Master volume doesn't reach SCO | In-call volume is the sink volume; phone-side volume rocker syncs via HFP volume events (PipeWire implements AT+VGS handling — live check L6) | Set volume on the SCO node via a registry proxy |
| Phone EQ engine doesn't process calls | Voice calls are not EQ'd (arguably correct for SWB voice) | Loopback ownership (Option B below) |
| `audio.microphone.device` config doesn't steer the call mic | The *system default* source feeds the uplink; configure it via WirePlumber | WirePlumber rule or loopback ownership |

**Rejected alternative (Option B), recorded for the future:** prodigy-owned loopback routing — suppress WirePlumber's auto-link for `api.bluez5.profile == "headset-audio-gateway"` nodes via a WirePlumber rule drop-in, then `openCaptureStream` from `bluez_input` → `writeAudio` into a prodigy "Phone" stream (priority 70, `AudioFocusType::Gain`, phone EQ engine), and mic capture → playback targeting `bluez_output`. Full focus/EQ/volume integration, at the cost of latency, echo risk, config fragility, and ~2 new RT paths. Revisit only if v1's limitations table draws real complaints.

## 4. Component architecture

```
                    Qt main thread                          PW threads
┌──────────────────────────────────────────────────┐
│ PhonePlugin (UI only — no D-Bus)                 │
│   binds IPhoneStateService via HostContext       │
└───────────────▲──────────────────────────────────┘
                │ ICallStateProvider / IPhoneStateService
┌───────────────┴──────────────────────────────────┐
│ PhoneStateService (core)                          │
│   BlueZ Device1 watch (existing, kept)            │
│   call state machine (§5)                         │◄──── ScoNodeMonitor
│   capability truth: telephonyAvailable()          │      (PW registry watch,
│   dial/answer/hangup/sendDtmf → TelephonyClient   │       SCO node state,
└───────────────▲──────────────────────────────────┘       queued → main)
                │ typed Qt signals / slots
┌───────────────┴──────────────────────────────────┐
│ TelephonyClient (new D-Bus adapter)               │      CallAudioPolicy
│   session bus → org.pipewire.Telephony            │◄──── (IProjectionStatus-
│   ObjectManager watch: AudioGateway1 / Call1      │       Provider + config →
│   transport props (State/Codec/RejectSCO)         │       setRejectSco)
│   Dial/Answer/Hangup/SendTones (async)            │
└──────────────────────────────────────────────────┘
```

### 4.1 `TelephonyClient` (new: `src/core/services/TelephonyClient.{hpp,cpp}`)

D-Bus mechanics only — no call-state policy. Same integration pattern as the bt_audio plugin's BlueZ client, pointed at WirePlumber.

- **Bus & discovery:** `QDBusConnection::sessionBus()`. `QDBusServiceWatcher` on `org.pipewire.Telephony` (registration + unregistration). On service up (and at start if already up): `GetManagedObjects` on `/org/pipewire/Telephony` — this enumerates **AudioGateway objects only, never Call1 children** (D1 §6.4, live-verified). Subscribe `InterfacesAdded`/`InterfacesRemoved` **service-wide (empty path match)** and `PropertiesChanged` likewise — calls are tracked **exclusively** via these signals.
  > **REVISED 2026-07-05 (execution, live L2 evidence):** originally "subscribe on the root" — wrong. PipeWire puts a **second ObjectManager on each AG object** (`/org/pipewire/Telephony/ag1` carries `org.freedesktop.DBus.ObjectManager` too), and **Call1 add/remove signals are emitted by the per-AG ObjectManager**, not the root. A root-path match never sees them (bug found because the deployed head unit stayed Idle through live calls). Match rule must be service-wide.
- **Objects tracked:** first `org.pipewire.Telephony.AudioGateway1` object (path pattern `/org/pipewire/Telephony/agN`) — single-AG assumption for v1; additional AGs are logged and ignored. The transport interface (`org.pipewire.Telephony.AudioGatewayTransport1`) is expected on/adjacent to the same object — the adapter must **discover the path carrying the transport interface from the `InterfacesAdded` payload, not hardcode it** (D1 §7: discover, don't hardcode).
- **Call1 handling:** `InterfacesAdded` carrying `org.pipewire.Telephony.Call1` → emit `callSetupStarted(state, lineIdentification, name)`. `PropertiesChanged` on that path → `callSetupChanged(state)`. `InterfacesRemoved` → `callSetupEnded()`. Call1 objects are **ephemeral, setup-phase only** (D1 §6.4) — the client never assumes one exists for an active call.
- **Commands** (all async via `QDBusPendingCallWatcher`; errors logged with the D-Bus error string, surfaced as a `commandFailed(op, message)` signal): `dial(number)` → `AudioGateway1.Dial(s)`; `answer()` → `Call1.Answer()` on the current setup object (fails cleanly if none); `hangupAll()` → `AudioGateway1.HangupAll()`; `sendTones(tones)` → `AudioGateway1.SendTones(s)`.
- **RejectSCO:** `setRejectSco(bool)` → `org.freedesktop.DBus.Properties.Set` on the transport interface. Re-applied automatically whenever a new AG/transport appears (policy value cached).
- **Signals out (all main-thread):** `availableChanged(bool)` (service up ∧ AG present), `agAddressChanged(QString)`, `transportStateChanged(QString)`, `codecChanged(QString)`, `callSetupStarted/Changed/Ended`, `commandFailed`.
- **Deserialization:** `InterfacesAdded` payload is `(o, a{sa{sv}})` — extract with manual `beginMap()/endMap()` + `QDBusVariant`, exactly like `PhoneStateService::scanExistingDevices()` (`PhoneStateService.cpp:173-199`). The CLAUDE.md QVariantMap gotcha applies in full.
- **Constructor must not touch the bus**; `start()` guards on `sessionBus().isConnected()` (test environments may have no session bus — mirror `startDBusMonitoring()`'s guard style).

### 4.2 `ScoNodeMonitor` (new: `src/core/audio/ScoNodeMonitor.{hpp,cpp}`)

D1 §6.3: SCO node state `suspended → running` is **the reliable in-call signal**; transport `State` alone is not trustworthy pre-call (§6.4). So we watch the nodes.

- Owns its own `pw_registry` created from `AudioService`'s existing `pw_core` (do **not** extend `PipeWireDeviceRegistry` — it's tested, device-selection-scoped code; a second registry on the same core is cheap and isolates concerns).
- Global-added: if props contain `api.bluez5.profile == "headset-audio-gateway"` and `media.class` is an Audio node, bind the node (`pw_registry_bind` → `pw_node`), attach a `pw_node_events.info` listener, track `info->state`.
- Emits `scoRunningChanged(bool)` — true when **any** tracked SCO node is `PW_NODE_STATE_RUNNING`. All PW callbacks run on the PipeWire thread loop: marshal with `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)`; never touch Qt state on the PW thread.
- Lifecycle: started by `main.cpp` after `AudioService` init (accessor on AudioService hands over `threadLoop_`/`core_`; monitor start/stop takes the thread-loop lock like `PipeWireDeviceRegistry::start/stop`). If PipeWire is unavailable (`!audioService->isAvailable()`), the monitor stays inert and `scoRunning` is permanently false — the state machine still functions on transport-state evidence alone (degraded disambiguation, §5).

### 4.3 `PhoneStateService` (existing, extended)

Keeps: BlueZ `Device1` watch (connection + device name — works today), notification posting, `callDuration` ticking, `setIncomingCall()` as the documented test seam. Gains: the call state machine (§5), delegation of commands to `TelephonyClient`, capability truth.

- New collaborator injection: `attachTelephony(TelephonyClient*)`, `attachScoMonitor(ScoNodeMonitor*)` — wired in `main.cpp` near the existing construction site (`main.cpp:363`). The state-machine event slots are public: they are the unit-test API (tests call them directly; no bus required), same philosophy as `setIncomingCall()`.
- Command implementations (replacing the mock bodies): `answer()` → only in `Ringing`, else log+ignore; `hangup()` → any non-Idle state, `TelephonyClient::hangupAll()` (AG-level — ends the active call or rejects the ring, matching `HangupRequest` semantics in the frozen proto); new `dial(number)` → only in `Idle` with `telephonyAvailable()`; new `sendDtmf(tones)` → only in `Active`. **State transitions come only from telephony/SCO events, never optimistically from command dispatch** — the phone is the source of truth.
- Capability truth for the API serializer seam: `bool telephonyAvailable() const` + `telephonyAvailableChanged()` = (Telephony service up ∧ AG object present). This single bit drives `can_dial/can_answer/can_hangup/can_send_dtmf` (capability = "the mechanism is wired", per `phone.proto`: a command in the wrong call state returns `FAILED`, not `UNAVAILABLE`). `can_hold_swap`/`can_multiparty` remain hard-false — no code path may ever set them in v1.

### 4.4 `ICallStateProvider` / `IPhoneStateService` (widened)

```cpp
// ICallStateProvider.hpp — append-only; existing numeric values are frozen
// (QML compares numbers: IncomingCallOverlay, PhoneView)
enum CallState { Idle = 0, Ringing, Active, Dialing, Alerting, Held, Waiting };
```

- `Dialing`(3)/`Alerting`(4) are produced by v1. `Held`(5)/`Waiting`(6) are declared for enum-completeness with the frozen proto's `CallState` but **unproducible in v1** (Call1 ephemerality — D1 §6.4); the serializer mapping table covers them so nothing breaks if PipeWire grows them.
- `IPhoneStateService` gains: `Q_INVOKABLE bool dial(const QString& number)`, `Q_INVOKABLE bool sendDtmf(const QString& tones)`, `virtual bool telephonyAvailable() const`, signal `telephonyAvailableChanged()`. `ICallStateProvider::answer()/hangup()` change to return `bool`. The bool = "dispatched" (state guard passed); it is how the API bridge distinguishes `FAILED` (wrong call state) from `OK` without duplicating guard logic — QML callers ignore the return value, so the signature change is free.
- API serializer mapping (grows the table in `ApiSerializers.cpp` **iff API v1 has landed** — D2 and the API plan are order-independent): `Ringing→CALL_STATE_INCOMING`, `Dialing→CALL_STATE_DIALING`, `Alerting→CALL_STATE_ALERTING`, `Active→CALL_STATE_ACTIVE`, `Held→CALL_STATE_HELD`, `Waiting→CALL_STATE_WAITING`, `Idle→` empty `calls[]`. The API plan's Task 11 `phoneCommand()` helper swaps its unconditional-`UNAVAILABLE` body for: flag check (from `telephonyAvailable()`) → provider invokable → `OK`/`FAILED`. Capability flags and command results must never contradict (frozen contract).

### 4.5 `CallAudioPolicy` (new: `src/core/services/CallAudioPolicy.{hpp,cpp}`)

Tiny, testable policy object — mechanism lives in `TelephonyClient`, this decides *when*.

- Inputs: `IProjectionStatusProvider*` (the arbitrated AA state — rail R1; **not** EventBus topics, not the orchestrator), `YamlConfig` key `phone.reject_sco_during_aa`.
- Rule: `wantReject = config && (projectionState ∈ {Connected(3), Backgrounded(4)})`. Recomputed on `projectionStateChanged` and on config change; emitted as `rejectScoWanted(bool)`; `main.cpp` connects it to `TelephonyClient::setRejectSco`. The client re-applies on AG appearance, covering the phone-connects-mid-AA-session race (SCO only arises with calls, which cannot precede the AG object).

### 4.6 `PhonePlugin` (gutted to a view)

- Delete its private D-Bus monitoring wholesale (`startDBusMonitoring/stopDBusMonitoring/scanExistingDevices/onInterfacesAdded/onInterfacesRemoved/onPropertiesChanged`, watcher, timer duplication).
- In `initialize()`: `qobject_cast<IPhoneStateService*>(context->callStateProvider())`; bind all Q_PROPERTYs to the service; forward `dial/answer/hangup/sendDTMF` to it. Keep `dialedNumber` editing and the 1.5 s "Ended" flash as plugin-local UI state (map provider transitions Active→Idle into the plugin's `Ended` flash).
- Fix the header comment's role inversion ("The Pi acts as HFP Audio Gateway" → the Pi is the **HF**; the phone is the AG) — D1 §2.1 executor task.
- Fix `IncomingCallOverlay.qml:11`: `callState === 1` (Ringing), and correct the stale comment at `:6`.

## 5. Call state machine (normative)

Single-call model (v1). State variable is `ICallStateProvider::CallState`. Events:

| Event | Source |
|---|---|
| `E-SETUP(state, line, name)` | `TelephonyClient::callSetupStarted` |
| `E-SETUP-CHG(state)` | `callSetupChanged` (Call1 `PropertiesChanged`) |
| `E-SETUP-END` | `callSetupEnded` (Call1 `InterfacesRemoved`) |
| `E-SCO(bool)` | `ScoNodeMonitor::scoRunningChanged` |
| `E-TRANSPORT(state)` | `transportStateChanged` ("idle"/"pending"/"active" — treat as advisory; D1 §6.4: unreliable pre-call) |
| `E-GONE` | `availableChanged(false)`, or BlueZ device disconnect |

Transitions (anything not listed: log at debug, no transition):

| From | Event | To | Notes |
|---|---|---|---|
| Idle | E-SETUP("incoming") | Ringing | capture line/name; post `incoming_call` notification (existing behavior) |
| Idle | E-SETUP("dialing") | Dialing | outgoing initiated (by us or on the handset) |
| Idle | E-SETUP("alerting") | Alerting | first observation may already be alerting |
| Dialing | E-SETUP-CHG("alerting") | Alerting | |
| Ringing/Dialing/Alerting | E-SETUP-CHG("active") | Active | clean path — **L2 CONFIRMED primary** (Call1 fires State→"active" and persists through the call on Pixel 8) |
| Ringing/Dialing/Alerting | E-SETUP-CHG("disconnected") | Idle | **ADDED from L2 evidence:** reject/failure emits State→"disconnected" before InterfacesRemoved — resolve immediately, skip Settling |
| Ringing/Dialing/Alerting | E-SETUP-END | **Settling** | disambiguation below (reached only if neither "active" nor "disconnected" was seen — e.g. other phones) |
| Settling | E-SCO(true), or already true at entry | Active | answered/connected |
| Settling | grace timeout (default 2000 ms, injectable for tests) | Idle | rejected / failed / cancelled |
| Idle | E-SCO(true) ∧ AG present | Active | recovery: prodigy restarted mid-call, or user routed audio back to car; caller info = last known (may be empty) |
| Active | E-SCO(false) sustained > 1000 ms (debounce), or E-TRANSPORT("idle") while SCO not running | Idle | covers hangup and audio-moved-to-handset alike: the head unit shows what the head unit is doing |
| any | E-GONE | Idle | clear caller info, `telephonyAvailable=false` |

Implementation notes:

- **Settling** is internal (grace timer + flag), not a provider-visible state — the provider keeps reporting the prior setup state until resolution, so UI doesn't flap.
- If SCO never becomes observable (`ScoNodeMonitor` inert because PipeWire is unavailable to prodigy), Settling falls back to `E-TRANSPORT("active")` received *during* Settling as the accept signal. Transport state is only trusted inside this window — never as a call-start signal from Idle (pre-call `"active"` quirk, D1 §6.4).
- A second Call1 while Active (call-waiting) is **logged and ignored** in v1 — the provider is single-call; the frozen proto's `calls[]` gains the second element additively when the provider goes multi-call. Do not surface `Waiting` from this path without a design update.
- `callDuration` behavior unchanged: starts ticking on entry to Active (the API serializer derives `started_at_unix_ms` from it — API design §8.4; the publisher ignores `callDurationChanged`).
- Startup mid-ring is missed by design (`GetManagedObjects` never lists Call1 — D1 §6.4); the Idle+E-SCO(true) recovery row catches it at answer time.

## 6. AA coexistence — RejectSCO policy

**Mechanism (locked by D1 §5.3):** `AudioGatewayTransport1.RejectSCO` (readwrite, live in 1.4.2) toggled at runtime per AA session state; profiles are never torn down. When SCO is rejected during projection, the phone keeps call audio itself — on phones supporting *calling over Android Auto*, audio flows via the AA session; otherwise it stays on the handset.

**Policy default (D2 decision): `phone.reject_sco_during_aa: false` — HFP owns call audio always, including during AA projection.** Rationale:

- That is how every commercial AA head unit behaves: wireless AA *requires* HU HFP support; the AA session provides the in-call UI while audio flows over SCO. It is the known-good, phone-independent path.
- Call-audio-over-AA is phone-dependent (a Pixel feature flag historically; Samsung/Moto behavior unverified). Defaulting to reject would send call audio **to the handset** on non-supporting phones — the worst outcome in a car.
- The lever's real justification is empirical: Pi 4's shared 2.4 GHz WiFi/BT front-end may make SCO degrade AA video (or vice versa). Live check L4 measures exactly this; if SCO-during-AA proves unusable, flip the shipped default to `true` and record the interop caveat.

`bluez5.telephony.default-reject-sco` (WirePlumber boot default) stays **unset**: prodigy applies policy within milliseconds of AG appearance, and if prodigy isn't running, plain-HFP behavior is the right fallback.

## 7. Codec expectations

No prodigy code negotiates codecs — PipeWire does. Expectations for logging/diagnostics and the interop pass:

- **LC3-SWB** (super-wideband, 32 kHz): negotiated with zero config on Pixel 8 against the Pi 4's controller (D1 §6.3 — implies transparent eSCO works on this BT chip).
- **mSBC** (wideband, 16 kHz): expected fallback for most phones.
- **CVSD** (narrowband, 8 kHz): guaranteed floor per HFP spec.
- `TelephonyClient` logs the transport `Codec` property on change (qCInfo) — that is the interop-pass instrument. Not exposed on the provider or API in v1 (additive later if a diagnostics surface wants it).
- **REVISED (L1):** the `Codec` property is a **byte** (HFP codec ID: 1=CVSD, 2=mSBC, 3=LC3-SWB), not a string. `TelephonyClient` maps it to a name for logging. Pixel 8 negotiated `3` (LC3-SWB) live — confirms D1 §6.3.

## 8. Deletions (with proof of safety)

### 8.1 Dead HFP AG + HSP HS registration — `BluetoothManager`

Delete wholesale (D1 §5.5 indictment, re-verified at `875feaf`):

- `BluezProfile1Handler` class (`BluetoothManager.cpp:52-79` region) and its `profileFds_` store (`BluetoothManager.hpp:128`) — fds are written, never read, only closed.
- `registerProfiles()` / `unregisterProfiles()` (`BluetoothManager.cpp:540-603`), members `profileObjects_`, `registeredProfilePaths_` (`hpp:126-127`), call sites at `:352`, `:375`, `:798`.
- Signal `profileNewConnection` (`hpp:66`), its `connect` at `:378-379`, and the stale comment at `:282` ("wait for profileNewConnection (RFCOMM) as the true success signal" — it never fires; registration loses the race to PipeWire every boot).

Safety: auto-connect cancellation is already handled by `updateConnectedDevice()` (`:736-740`) on `Device1.Connected` — the only functional consumer of the deleted signal was redundant. Removing the registration also eliminates the latent boot-order race (prodigy-as-system-service winning the UUID registration and silently breaking PipeWire telephony — D1 §5.5).

### 8.2 `PhonePlugin` private D-Bus monitoring

§4.6. The plugin's copy never produced call state (only connection state, duplicating `PhoneStateService`).

## 9. Config

New keys in `YamlConfig::initDefaults()` under the existing patterns:

```yaml
phone:
  reject_sco_during_aa: false   # §6 — flip only after live check L4
  settle_grace_ms: 2000         # §5 Settling window; test seam, not user-facing
```

No installer changes: telephony service is enabled by default in Trixie's WirePlumber (D1 §6.2), the session-bus env is already in the unit (§2.7).

## 10. Testing strategy

- **State machine unit tests** (`tests/test_phone_state_service.cpp`, extended): drive the public event slots directly — no bus, no PipeWire. Cover every row of the §5 table plus: incoming-answered (SETUP→SETUP-END→SCO true), incoming-rejected (SETUP→SETUP-END→timeout), outgoing full path (dialing→alerting→active via SETUP-CHG), active-hangup (SCO false debounce), handset-route-away/route-back, mid-call restart recovery (Idle + SCO true), AG-vanish from every state, call-waiting ignored, command guards (answer only in Ringing, dial only in Idle, dtmf only in Active). Set `settle_grace_ms`/debounce low (~50 ms) via the config seam; use `QSignalSpy` + `QTRY_COMPARE`.
- **`TelephonyClient`:** constructor/start safety without a session bus (must not crash — CI has no bus); demarshaling of a synthetic `(o, a{sa{sv}})` `QDBusArgument` if practical. The real protocol conformance is live-check territory (§11) — do not fake a bus in ctest.
- **`CallAudioPolicy`:** fake `IProjectionStatusProvider` (subclass, settable state) × config on/off → `rejectScoWanted` truth table, including re-emit on AG appearance.
- **`ScoNodeMonitor`:** inert-without-PipeWire test only (start against unavailable AudioService → no crash, `scoRunning()==false`). Node-state tracking is live-check territory.
- **Regressions:** full `ctest` (88 tests) after the BluetoothManager deletion and the PhonePlugin gutting; `test_phone_state_service` existing cases must keep passing (the mock seams they use are retained).

## 11. Live verification checklist (self-contained — executor runs without the designer)

Prereqs: Pi powered, prodigy deployed, a phone paired. Run as user `matt` on the Pi (`--user` = session bus). Record every result inline in this doc or `docs/session-handoffs.md`. None of these block implementation — L2 refines a state-machine row; L4 decides a config default; the rest pin interop truth.

**L1 — Object topology (5 min, any phone).** With the phone HFP-connected:
```
busctl --user tree org.pipewire.Telephony
busctl --user introspect org.pipewire.Telephony /org/pipewire/Telephony/ag1
```
Confirm: which object carries `AudioGatewayTransport1` (same path as `AudioGateway1`?); property list matches D1 §3. Record the transport path pattern — `TelephonyClient` discovers it, but the executor should know what "right" looks like.

> **L1 RESULT (2026-07-05, Pixel 8, live):** CONFIRMED — `AudioGatewayTransport1` is on the **same object** as `AudioGateway1` (`/org/pipewire/Telephony/ag1`), alongside the ofono-compat `org.ofono.VoiceCallManager` aliases. Property list matches D1 §3 with two refinements: (1) **`Codec` is type `y` (byte)**, not string — HFP codec IDs (1=CVSD, 2=mSBC, 3=LC3-SWB); value `0` pre-call. (2) **`Address` is `const`** and readable via introspection but arrived **empty in the `InterfacesAdded` payload** — PipeWire omits it there; log-only impact in v1. `busctl tree` shows only the root (ag1 not listed as a child — consistent with the GetManagedObjects ephemerality findings). Transport `State` was `"idle"` pre-call at HFP connect — the D1 §6.4 pre-call `"active"` quirk did not reproduce here.

**L2 — Call1 State sequence (10 min, any phone).** In one terminal:
```
busctl --user monitor org.pipewire.Telephony
```
Then: (a) receive a call, answer on the head unit; (b) receive a call, reject from the phone; (c) place a call from the head unit, let it connect, hang up. Question to answer: **does `Call1.State` fire a `PropertiesChanged` to `"active"` before `InterfacesRemoved`?** If yes, the §5 clean path (`E-SETUP-CHG("active")`) is primary and Settling is rare; if no, every answered call resolves through Settling (still correct, just note it). Also record the transport `State` values seen at each step and whether the pre-call `"active"` quirk (D1 §6.4) reproduces.

> **L2 RESULT (2026-07-05, Pixel 8, live, from busctl monitor capture):**
> - **YES — `Call1.State` → `"active"` fires before `InterfacesRemoved`.** Clean path is PRIMARY. Moreover Call1 is **not destroyed at call-active on this phone** — it persists through the call, emits `State → "disconnected"` at hangup, then `InterfacesRemoved`. (Softer than D1 §6.4's ephemerality claim; the state machine handles both lifetimes.)
> - Rejected call: Call1 added → transport `pending → active → idle` → Call1 `"disconnected"` → removed — Call1 never went `"active"`. **Transport `"active"` during ring = in-band ringtone over SCO**, which explains D1's "transport active is not authoritative" quirk precisely.
> - **Design amendments applied from this evidence:** (1) `InterfacesAdded/Removed` are emitted by a per-AG ObjectManager — subscription must be service-wide (§4.1 revision; this was a live-found implementation bug). (2) New §5 row: setup + `E-SETUP-CHG("disconnected")` → Idle immediately, skipping Settling. (3) Codec property is a byte; 3=LC3-SWB observed (§7).

**L3 — SendTones DTMF (5 min, any phone).** Call an IVR (e.g. a voicemail menu). During the active call:
```
busctl --user call org.pipewire.Telephony /org/pipewire/Telephony/ag1 \
  org.pipewire.Telephony.AudioGateway1 SendTones s "1"
```
Confirm the IVR reacts. Failure here = `can_send_dtmf` must be decoupled from `telephonyAvailable()` (make it its own flag, hard-false) — flag to the designer via session handoff before shipping the capability as true.

**L4 — RejectSCO behavior under AA (15 min, Pixel 8 — THE config-default decider).** With AA projecting:
1. Baseline (`RejectSCO=false`, our default): place/receive a call. Where does audio flow (car speakers via SCO / handset)? Does AA video stutter during the call (2.4 GHz coexistence)? `pw-cli ls Node | grep bluez` — SCO node states during call.
2. Then set:
```
busctl --user set-property org.pipewire.Telephony <transport-path> \
  org.pipewire.Telephony.AudioGatewayTransport1 RejectSCO b true
```
Call again. Does call audio route through the AA session (car speakers, no SCO nodes running) or stay on the handset?
Decision rule: default stays `false` unless (1) shows unusable AA degradation AND (2) shows working call-over-AA audio. Record the verdict next to §6.

**L5 — Interop pass (10 min per phone: Samsung S25 Ultra, Moto G Play 2024).** Pair, HFP-connect, then per phone: codec (`busctl --user get-property ... AudioGatewayTransport1 Codec` during a call), incoming ring → answer from head unit, outgoing dial via `Dial()`, hangup via `HangupAll()`, caller-ID presence (`LineIdentification`/`Name` on the Call1 object during ring — from the L2 monitor).

**L6 — Volume & audio quality (in-car, 10 min).** During an active call: phone volume rocker — does downlink volume on the car speakers track it (PipeWire HFP volume sync)? Subjective mic/speaker quality both directions (echo, level). If the mic is wrong/silent: `wpctl status` → is the default source the intended mic (§3 limitations table)?

## 12. Executor Guidance (mandatory)

**Invariants — violating any is stop-and-ask:**
1. If you find yourself registering profile `0x111f` (or `0x1108`) on the Pi, you have inverted the HF/AG roles — stop (D1 §7). This design *deletes* profile registration; it must not reappear in any form.
2. `TelephonyClient` types (paths, interface names, `QDBusVariant`) never cross the `IPhoneStateService` boundary — QML, plugins, and the API see only the widened enum + invokables (rail R1).
3. `ICallStateProvider` enum values `Idle=0, Ringing=1, Active=2` are frozen (QML compares raw numbers); new states append only.
4. `can_hold_swap`/`can_multiparty` stay false — no code path sets them (frozen proto contract).
5. State transitions come from telephony/SCO events only — never optimistically from command dispatch.
6. Do not install ofono; do not enable `provide-ofono`; do not add WirePlumber config drop-ins (telephony is on by default in Trixie — D1 §6.2).
7. Never touch `libs/prodigy-oaa-protocol/`.

**Pitfalls (CLAUDE.md gotchas + new ones from this design):**
- `QDBusArgument >>` cannot extract `QVariantMap` directly — manual `beginMap()/endMap()` with `QDBusVariant` (pattern: `PhoneStateService.cpp:173-199`). Applies to `InterfacesAdded`'s `a{sa{sv}}`.
- All Qt D-Bus handlers land on the main thread — keep them light; no blocking calls in signal handlers (use async `QDBusPendingCallWatcher` for every method call; the only acceptable blocking call is the startup `GetManagedObjects` with a short timeout, matching existing style).
- PipeWire callbacks (`ScoNodeMonitor`) run on the PW thread loop — marshal to Qt via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`; take `pw_thread_loop_lock` for registry setup/teardown (pattern: `PipeWireDeviceRegistry::start/stop`).
- `QTimer` needs `#include <QTimer>` and a Qt event loop; the Settling/debounce timers live in `PhoneStateService` (main thread) — fine.
- Session bus in tests/CI may not exist — every `start()` guards `isConnected()`; constructors touch nothing.
- `IncomingCallOverlay.qml` compares raw ints — after the enum widening, re-grep QML for `callState ===` and verify each against `ICallStateProvider` numbering (the `=== 2` bug in §2.3 is exactly this class of error).
- Q_OBJECT in new headers needs the `.cpp` listed in `src/CMakeLists.txt` for MOC.

**Definition of done:** all §10 tests pass in the full suite; `./cross-build.sh` succeeds; deployed to the Pi and the L1–L6 checklist executed with results recorded; rails citations in this doc verified against the implementation; deviations recorded in `docs/session-handoffs.md`.
