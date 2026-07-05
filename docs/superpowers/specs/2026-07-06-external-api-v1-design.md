# External API v1 — Design (Phase B)

**Date:** 2026-07-06
**Status:** FROZEN — Codex review 2026-07-06 verdict "freeze with fixes"; all four findings applied (notification `priority` made optional; phone commands strictly capability-gated so contract and behavior cannot contradict; `ConnectivityReport.socks5_password` added for companion proxy parity; version-minor lives in ServerHello only). `proto/api/` is now ADDITIVE-ONLY (rail R5).
**Grounded against:** `fable-design-sprint` at `a748121` + two code-level substrate scouts (2026-07-05/06). If the substrate has moved since, re-verify the mapping tables in §8 before implementing.
**Rails consumed:** `2026-07-05-extensibility-architecture-design.md` §2 (R1–R5), §3 (transport/auth/threading/backpressure — locked), §4 (schema conventions — locked), §8 (invariants). Phone semantics: `2026-07-05-hfp-call-control-decision.md` §5–§6 (live-verified). WS-first-class rationale: `2026-07-05-webengine-spike-results.md` (GO; web widgets are WebSocket API clients).
**Audience:** the implementing agent. Read this whole doc plus the Executor Guidance (§17) before writing any code. The companion implementation plan is `docs/superpowers/plans/2026-07-06-external-api-v1-implementation.md`.

## 1. Purpose

One protobuf API over TCP + WebSocket is the single external integration surface for OpenAuto Prodigy (rail R2): companion app (rewrite), web widgets (in-process WebEngine, connecting to 127.0.0.1), and third-party clients. This doc fixes the session layer, auth, delivery model, per-domain semantics, and test plan. The wire contract itself lives in `proto/api/` (authored with this doc; FROZEN 2026-07-06 after the Codex review gate — additive-only from here).

## 2. Scope (v1 fence) and two flagged fence deltas

In scope, per the architecture doc §3.4 — status streams `media`, `navigation`, `projection`, `phone`, `system`; requests: action list/dispatch/register, notification post/dismiss, capability query, phone commands (dial/answer/hangup/DTMF, capability-gated); inbound reports: GPS, battery/charging, connectivity/proxy.

Explicitly out: EQ control, OBD, cover art, overlay geometry, notification *streams* (post/dismiss only), theme/wallpaper push from companion (v1.1 candidate), rebroadcast of companion GPS as a `location` status topic (v1.1 candidate — a speedometer web widget will want it, but the fence says nothing else in v1).

**Fence deltas discovered during substrate scouting — Matthew must see these (both are small, neither touches the fence's spirit):**

1. **`TimeReport` added as a fourth inbound report.** The live companion protocol carries `time_ms` on every status message and drives `adjustClock` — the Pi has no RTC, so clock sync is functional parity, not a nice-to-have. Omitting it would silently regress the companion rewrite. One message, inbound-only.
2. **Three built-in actions `media.playPause` / `media.next` / `media.previous` get registered in `main.cpp`.** ActionRegistry today has no media transport actions (the 25 built-ins are app/navbar/aa/theme); `MediaStatusService` has the invokables but nothing exposes them outside QML. Without this, API clients cannot control playback at all. This is an ActionRegistry addition, not an API schema addition — the API gets it for free via dispatch, exactly the mechanism the architecture doc §3.4 prescribes for overlay actions later.

## 3. Component architecture

All new code in `src/core/api/` (the directory already exists, empty with `.gitkeep`). Everything runs on the **Qt main thread** (arch §3.3 — locked; Qt networking needs the event loop, providers/EventBus deliver there, traffic is status-scale).

```
                    ┌────────────────────────────────────────────┐
                    │ ApiServer (QObject, owns everything below) │
                    └──┬─────────────┬───────────────┬───────────┘
   QTcpServer :9810 ───┤             │               │
   QWebSocketServer    │      ┌──────┴──────┐  ┌─────┴──────────────┐
   :9811 ──────────────┤      │ PairingMgr  │  │ Topic publishers   │
                       │      │ + client    │  │ (one per domain)   │
                ┌──────┴────┐ │ store       │  │ bind to services,  │
                │ ApiSession│ └─────────────┘  │ serialize once,    │
                │ (1/conn)  │                  │ fan out to sessions│
                └──────┬────┘                  └─────┬──────────────┘
                       │ IApiTransport               │
                ┌──────┴──────────┐        ┌─────────┴──────────────┐
                │ TcpApiTransport │        │ Request bridges:       │
                │ (length-prefix  │        │ actions, notifications,│
                │  framing)       │        │ phone, companion-ingest│
                │ WsApiTransport  │        └────────────────────────┘
                │ (1 msg/frame)   │
                └─────────────────┘
```

### 3.1 File layout

| File | Contents |
|---|---|
| `src/core/api/ApiServer.{hpp,cpp}` | Listeners, session registry, publisher wiring, config read, lifecycle |
| `src/core/api/ApiSession.{hpp,cpp}` | Per-connection state machine, subscriptions, outbound queue cap, request routing |
| `src/core/api/ApiTransport.{hpp,cpp}` | `IApiTransport` + `TcpApiTransport` (4-byte BE length framing, partial-read reassembly) + `WsApiTransport` (binary frame = one message) |
| `src/core/api/ApiAuth.{hpp,cpp}` | `PairingManager` (PIN window, challenge/response verify) + `PairedClientStore` (`~/.openauto/api_clients.yaml`) |
| `src/core/api/ApiPublishers.{hpp,cpp}` | `TopicPublisher` base (coalescing, snapshot interface) + the five domain publishers |
| `src/core/api/ApiSerializers.{hpp,cpp}` | Pure functions: service state → proto message, including all normalization tables in §8. Unit-testable without sockets |
| `src/core/api/ApiRequestHandlers.{hpp,cpp}` | Action bridge (incl. client-registered action lifetime), notification bridge (ownership rule), phone command bridge, companion ingest |
| `src/core/api/ApiInboundState.{hpp,cpp}` | Q_PROPERTY holder for companion-report data (GPS/battery/connectivity/time) — the migration target for `CompanionListenerService`'s consumers |
| `proto/api/*.proto` | The contract (see §11 and the files themselves) |

### 3.2 Service binding — how the API reaches the substrate

The API server is **not a plugin**. It is instantiated in `main.cpp` after all services exist and receives a plain struct of pointers:

```cpp
struct ApiServiceRefs {
    oap::IMediaStatusProvider* media = nullptr;        // MediaStatusService
    oap::INavigationProvider* navigation = nullptr;    // NavigationDataBridge (may be null — no AA orchestrator)
    oap::IProjectionStatusProvider* projection = nullptr; // may be null
    oap::IPhoneStateService* phone = nullptr;          // PhoneStateService — the WIDE interface, deliberately
    oap::ThemeService* theme = nullptr;                // concrete: needs the full color Q_PROPERTY surface
    oap::INotificationService* notifications = nullptr;
    oap::ActionRegistry* actions = nullptr;
    oap::IConfigService* config = nullptr;
    oap::BluetoothManager* bluetooth = nullptr;        // optional; system stream device summary
};
```

Rationale (this is a deliberate deviation-with-reasons, record it): `IHostContext::callStateProvider()` returns only the narrow `ICallStateProvider` — `phoneConnected()`/`deviceName()`/`callDuration()` live on `IPhoneStateService` and are unreachable through the plugin context. Binding through `main.cpp` wiring (the same way QML context properties get concrete instances) keeps rail R1 intact — these are all core services/providers, never EventBus topics, never D-Bus/AA internals — without widening `IHostContext` for a non-plugin consumer.

Null providers are legal (navigation/projection are only created when an AA orchestrator exists). A null provider's topic is simply absent from `ServerHello.capabilities.supported_topics`, and `Subscribe` for it returns `accepted=false, reason="topic unavailable"`.

## 4. Connection lifecycle & session state machine

```
        connect            ClientHello ok
 ┌────────┐   ┌──────────────┐   (localhost)   ┌───────┐
 │ SOCKET │──▶│ EXPECT_HELLO │────────────────▶│ READY │
 └────────┘   └──────┬───────┘                 └───┬───┘
                     │ ClientHello, remote         │ any violation,
                     ▼                             │ queue overflow,
              ┌──────────────┐  AuthResponse ok    │ disconnect
              │ AUTH_PENDING │────────────────▶READY
              └──────┬───────┘
                     │ ClientHello.auth.pairing_request
                     │ && pairing window open
                     ▼
              ┌─────────────────┐ PairingResponse ok → persist client → READY
              │ PAIRING_PENDING │
              └─────────────────┘
```

Rules (violations → best-effort `Error` then disconnect):

- First message MUST be `ClientHello`, within `api.handshake_timeout_ms` (default 5000) of connect. Anything else, or timeout → disconnect. (Locked, arch §3.1.)
- `requested_api_version_major != 1` → `Error{UNSUPPORTED_VERSION}` + disconnect.
- **Localhost is trusted:** `peerAddress().isLoopback()` → straight to READY, `ServerHello` sent. This is what keeps the JS bridge token-free (rail, arch §3.2).
- Remote + known `client_id` in `ClientHello.auth` → server sends `AuthRequired{nonce}` → client sends `AuthResponse{client_id, hmac}` → verify → `ServerHello` or `AuthReject` + disconnect.
- Remote + `pairing_request` → only honored while the pairing window is open (§5), else `AuthReject{PAIRING_WINDOW_CLOSED}`.
- In READY: requests are answered with responses echoing `request_id`; stream events carry `request_id = 0`. Pre-READY, only handshake messages are legal.
- `Ping`/`Pong` (both directions) are legal in READY; server always answers `Ping` with `Pong` echoing `request_id`.

Disconnect cleanup (one path, `ApiSession::teardown()`): unregister client-registered actions, drop subscriptions, remove from session registry, deleteLater. Dead-peer note: there is no server-initiated liveness probe in v1 — a vanished subscriber is reaped by the queue cap as deltas accumulate (status topics are chatty enough), and the CLAUDE.md AP-interface TCP gotcha applies to *idle* connections only, which cost one fd. Accepted for v1; recorded here so nobody adds keepalive machinery without data.

### 4.1 Bind policy

Listen on `QHostAddress::Any`, enforce exposure per-connection by peer address (accept loopback always; accept AP subnet `10.0.0.0/24` always; accept anything else only if `api.expose_lan: true`). Rationale: binding to `10.0.0.1` literally would race wlan0 AP bring-up at boot (address not yet assigned → bind fails); bind-any + peer filter has no ordering dependency. The filter runs before any protocol processing — a rejected peer gets a TCP close, not an `Error` message.

## 5. Auth & pairing

Adapted from the proven companion flow (`CompanionListenerService.cpp:109-147, 219-262`), with three deliberate changes: per-pairing **random salt** (companion used a fixed literal salt), **per-client persistent secrets** (companion had one global secret), and **no per-message MACs** (companion HMAC'd every JSON line; the API authenticates the connection, then trusts it — the link is WPA2-encrypted on the AP, TLS is the honest v2 upgrade if a real threat appears, and hand-rolled per-message MACs on a length-prefixed binary protocol buy complexity, not security).

**Pairing (once per client):**

1. Matthew opens Settings → triggers action `api.pairing.start` → `PairingManager` generates a 6-digit PIN (`QRandomGenerator::global()->bounded(100000, 999999)`) and a 16-byte random `salt`, displays PIN (+ QR: `prodigy://pair?host=10.0.0.1&tcp=9810&ws=9811`) in Settings QML, opens the window for `api.pairing_timeout_s` (default 120).
2. Client connects, sends `ClientHello{auth: {pairing_request: true}, client_name, client_kind}`.
3. Server → `PairingChallenge{nonce (32 random bytes), salt}`.
4. User types the PIN into the client. Client computes `secret = SHA256(pin_utf8 + salt)`, replies `PairingResponse{proof: HMAC_SHA256(key=secret, data=nonce)}`.
5. Server verifies (it knows PIN + salt), generates `client_id` (UUID), persists `{client_id, secret_hex, client_name, client_kind, paired_at}` to `~/.openauto/api_clients.yaml`, replies `ServerHello{...,  granted_client_id}`. Client stores `client_id` + `secret` for reconnects. Window closes on first success or timeout, whichever first.

**Reconnect (every subsequent remote connection):** `ClientHello{auth:{client_id}}` → `AuthRequired{nonce}` → `AuthResponse{client_id, proof: HMAC_SHA256(key=stored secret, data=nonce)}` → constant-time compare (`QMessageAuthenticationCode`, same as companion) → `ServerHello` or `AuthReject`.

Storage is a **separate file**, not `config.yaml` — Matthew pastes `config.yaml` into bug reports; client secrets must not ride along. File mode `0600` like `companion.key`. Revocation: delete the entry (Settings UI for this is Phase E-adjacent; file edit suffices for v1).

**v1 has no per-capability ACLs** — paired = full API (locked, arch §3.2; recorded simplification).

## 6. Subscription & delivery model

- `SubscribeRequest{topics[]}` → `SubscribeResponse{results[]: {topic, accepted, reason}}` → for each accepted topic, the current full snapshot is sent immediately as a normal stream event (`request_id = 0`), then deltas follow. Late joiners never reconstruct state (locked, arch §3.3).
- **Every stream event is a full snapshot of its domain status message.** No field-diff protocol: messages are tens-to-hundreds of bytes, and full-state semantics make client code trivial (`latest wins`, no merge logic, no ordering hazards beyond per-session TCP/WS ordering). "Delta" in this design means "a fresh full snapshot when something changed," and the proto comments say so per message.
- **Coalescing:** each publisher debounces with a 0-ms single-shot QTimer — a burst of service signals within one event-loop turn (e.g. `mediaStatusChanged` firing for metadata and playback state back-to-back) serializes once. Serialization happens once per event; the resulting `QByteArray` is shared across all subscribed sessions (serialize-once, enqueue-per-session).
- **Unsubscribe:** `UnsubscribeRequest{topics[]}` → `Ack`. Idempotent.
- **Backpressure / slow consumer (load-bearing, arch §8):** before each send, if `socket->bytesToWrite() > api.max_queue_bytes` (default 1 MiB), the session is disconnected immediately — no waiting, no partial skips. A stalled WebSocket client must never stall the head unit. This is tested explicitly in the loopback suite (§15) by not draining a client while forcing high-rate publishes.
- Inbound frame sanity cap: `api.max_frame_bytes` (default 256 KiB, not user-config-documented) — a length prefix above the cap disconnects (protects against garbage/hostile frames; nothing legitimate in v1 approaches it).

## 7. Handshake surface (api.proto)

`ClientHello{requested_api_version_major/minor, client_name, client_kind, auth}` / `ServerHello{api_version_major=1, api_version_minor, server_name, app_version, session_id, granted_client_id?, capabilities}`.

**Capability discovery:** `ServerHello.capabilities` = `Capabilities{supported_topics[], phone: PhoneCapabilities}` — the static shape at connect time (the API minor version lives in `ServerHello` only; it cannot change mid-session, so `Capabilities` does not repeat it). `GetCapabilitiesRequest` → `CapabilitiesResponse{capabilities}` re-queries it. *Dynamic* capability truth (is dial possible *right now*) additionally lives in each `PhoneStatus` snapshot (§8.4), because it changes with phone connect/disconnect and clients already handle "no phone connected" — same mechanism, per the program doc's de-risking principle.

## 8. Domain semantics — service → proto mapping (normative)

The serializers in `ApiSerializers.cpp` are the **only** place normalization lives. Raw service ints never cross the wire.

### 8.1 `media` ← `IMediaStatusProvider` (signal: `mediaStatusChanged`)

| proto field | source | normalization |
|---|---|---|
| `has_media` | `hasMedia()` | — |
| `title/artist/album` | getters | — |
| `source` | `source()` string | `""`→`MEDIA_SOURCE_NONE`, `"Bluetooth"`→`_BLUETOOTH`, `"AndroidAuto"`→`_ANDROID_AUTO` |
| `playback_state` | `playbackState()` int | **source-dependent** (scout finding): if source is Bluetooth, `0/1/2 = STOPPED/PLAYING/PAUSED` (`BtAudioPlugin.hpp:50-53`); if AndroidAuto, the int comes from the oaa `MediaStatusChannelHandler` — executor MUST verify the AA enum values against the oaa protocol headers (read-only look into the submodule is fine) and encode the mapping as a table with unknown→`PLAYBACK_STATE_UNSPECIFIED` |
| `app_name` | `appName()` | AA-only field; empty otherwise (its getter ignores active source — serialize as-is) |

No position/duration in v1 — the service has none (scout-verified). Adding them later is the additive path working as intended.

### 8.2 `navigation` ← `INavigationProvider` (signals: `navActiveChanged`, `turnDataChanged`, `distanceChanged` — all three feed one publisher)

| proto field | source | normalization |
|---|---|---|
| `nav_active` | `navActive()` | — |
| `road_name` | `roadName()` | — |
| `maneuver` | `maneuverType()` int | raw AA maneuver code — **must not leak** (rail R1 spirit). Map to `prodigy.api.v1.ManeuverType` via table; unmapped → `MANEUVER_TYPE_OTHER`. Executor builds the table from the oaa protocol's maneuver enum (read-only) |
| `turn_side` | `turnDirection()` int | same treatment → `TurnSide{UNSPECIFIED/LEFT/RIGHT/UNSPECIFIED_SIDE}` mapping table |
| `distance_meters` | not on the interface — `distanceMeters` is a `NavigationDataBridge` extra | **v1 binds the interface only**: omit until the getter is promoted to `INavigationProvider` (tiny executor task, listed in the plan) — after which it populates. Field exists in the proto from day one |
| `formatted_distance` | `formattedDistance()` | display-ready string, pass through |

No ETA/destination/total-remaining — the provider has none (scout-verified). Fields deliberately absent; additive later.

### 8.3 `projection` ← `IProjectionStatusProvider` (signals: `projectionStateChanged`, `statusMessageChanged`)

`Disconnected/WaitingForDevice/Connecting/Connected/Backgrounded (0–4)` → `PROJECTION_STATE_{DISCONNECTED, WAITING_FOR_DEVICE, CONNECTING, PROJECTING, BACKGROUNDED}` by explicit switch (never `static_cast` — the provider reads a dynamic property off the orchestrator and the coupling is stringly; a switch with default→`UNSPECIFIED` is the safe shape). Plus `status_message` passthrough.

### 8.4 `phone` ← `IPhoneStateService` (signals: `callStateChanged`, `connectionChanged`; **ignore** `callDurationChanged` — see below)

Modeled on the **live-verified PipeWire Telephony semantics** (HFP decision §6), not today's UI-mock enum:

- `PhoneStatus{hfp_connected, device_name, calls[], capabilities}`.
- `calls[]` is `repeated Call{state, line_identification, display_name, started_at_unix_ms}` — **repeated** even though today's provider is single-call, because HFP is multi-call and repeated-with-one-element today costs nothing while a singular field would need deprecation later. No per-call id in v1: `Call1` objects are ephemeral (exist during setup only), `GetManagedObjects` never enumerates them, and mid-call control is AG-level (`HangupAll`/`SendTones`) — there is nothing for an id to address. Added additively if PipeWire ever grows persistent call objects.
- `CallState` enum: `UNSPECIFIED/INCOMING/DIALING/ALERTING/ACTIVE/HELD/WAITING` (HFP indicator semantics). Today's mapping: provider `Ringing`→`INCOMING`, `Active`→`ACTIVE`, `Idle`→empty `calls[]`. When D2's `TelephonyClient` widens the provider enum (Dialing/Held at minimum, per HFP decision §5.2), only the serializer table grows.
- **`started_at_unix_ms` instead of a duration field.** The provider's `callDuration()` ticks at 1 Hz; publishing it would re-emit `PhoneStatus` every second to every subscriber for zero information. The serializer computes `now − callDuration()·1000` once on the transition into `ACTIVE`, holds it for the call's lifetime, and clients render their own ticking timer. The publisher deliberately does **not** connect `callDurationChanged`.
- `PhoneCapabilities{can_dial, can_answer, can_hangup, can_send_dtmf, can_hold_swap, can_multiparty}` — **`can_hold_swap` and `can_multiparty` are hard-false in v1** (HFP decision §6.4: not reachable in PipeWire 1.4.2's surface for existing calls). The others are false today (mock) and become true when D2's TelephonyClient lands and reports Telephony object discovery. Capability truth is runtime state, not schema state.

### 8.5 `system` ← `ThemeService` + config + `BluetoothManager` (signals: `modeChanged`, `colorsChanged`, `currentThemeIdChanged`, BT `connectedDeviceName` change)

`SystemStatus{night_mode, theme_id, theme_tokens: map<string,string>, app_version, bluetooth: BtDeviceSummary{connected, device_name}}`.

- `night_mode` = `realNightMode()` (the effective value after forceDarkMode), `theme_id` = `currentThemeId()`.
- `theme_tokens`: the ~45 color Q_PROPERTYs serialized as `#RRGGBB` hex, keyed by **hyphenated token names** (`primary`, `on-primary`, `surface-container-high`, …) — the same names ThemeService's YAML uses and the same vocabulary the JS bridge will inject as `--prodigy-*` CSS custom properties (arch §5). One naming system end to end. Emitted on `colorsChanged`/`modeChanged` — theme switches are rare; full-map resend is fine.
- `app_version`: `identity.sw_version` from config + `OAP_GIT_HASH` compile define (scout: no version string exists anywhere today — this is its birth).

## 9. Requests

### 9.1 Actions (`actions.proto`)

- `ListActionsRequest` → `ListActionsResponse{actions[]: ActionInfo{id, label, client_owned}}`. Known limitation, recorded: ActionRegistry stores **no metadata** (bare `QHash<QString, Handler>` — scout-verified), so `label` is empty for built-ins; for client-registered actions the bridge stores the label the client supplied. When ActionRegistry grows metadata, labels fill in — no schema change.
- `DispatchActionRequest{action_id, payload_json?}` → `DispatchActionResponse{dispatched}` (`false` = unknown action, mirrors `ActionRegistry::dispatch`). Payload is optional JSON, converted `QJsonValue`→`QVariant` — covers today's real payloads (int keycode for `aa.sendButton`, string plugin id for `app.launchPlugin`) without a typed-variant zoo in the proto.
- `RegisterActionsRequest{actions[]: ActionSpec{id, label}}` → `RegisterActionsResponse{results[]: {id, accepted, reason}}`. Rules: reject ids already registered; reject ids under reserved prefixes (`app.`, `aa.`, `navbar.`, `theme.`, `media.`, `phone.`, `system.`, `overlay.`, `api.`); recommend reverse-dns or app-prefix naming in the proto comment. Dispatching a client action delivers `ActionInvokedEvent{action_id, payload_json}` to the owning session (`request_id = 0`); the in-registry handler is a forwarder the bridge installs. **All of a session's registrations auto-unregister on disconnect** (single teardown path, §4).
- `UnregisterActionsRequest{ids[]}` → `Ack` (own actions only; others' ids are silently skipped and reported in `Ack`-adjacent… no — silently skipped, and the proto comment says so. Keep it boring).

### 9.2 Notifications (`notifications.proto`)

- `PostNotificationRequest{message, kind = TOAST, priority (optional, 0–100; omitted → head-unit default 50, explicit 0 honored), ttl_ms (0 = persistent)}` → `PostNotificationResponse{notification_id}`. Maps onto `INotificationService::post()`'s QVariantMap with `sourcePluginId = "api:" + client_id` (or `"api:localhost"`). `kind` is an enum with only `NOTIFICATION_KIND_TOAST` beyond `_UNSPECIFIED` — the service's other kinds (`incoming_call`, `status_icon`) have no UI path today (scout) and are system-internal; not exposed.
- `DismissNotificationRequest{notification_id}` → `Ack` or `Error{NOT_FOUND}`. **Ownership rule:** a session may dismiss only notifications it posted (tracked per session) — a web widget must not nuke system toasts. Enforced in the bridge, stated in the proto comment.
- Note: the service's `post()` parses only `kind/message/sourcePluginId/priority/ttlMs` and there is no title/body split (single `message` string, scout-verified) — the proto mirrors reality instead of inventing fields the renderer would drop.

### 9.3 Phone commands (`phone.proto`)

Typed requests — not action-dispatch strings — because they carry payloads, are capability-gated, and are the API's most safety-relevant surface: `DialRequest{number}`, `AnswerCallRequest{}`, `HangupRequest{}` (AG-level semantics — ends the call/rejects the ring, per `HangupAll`), `SendDtmfRequest{tones}` (AG-level `SendTones`). Each → `PhoneCommandResponse{result: OK/UNAVAILABLE/FAILED, detail}`. **Capability flags and command results must never contradict** (Codex review, contract-level): `UNAVAILABLE` iff the corresponding capability flag is false, and in v1 all four flags are false — so all four commands return `UNAVAILABLE` until D2's `TelephonyClient` lands real call control and flips the flags from Telephony object discovery. The mock provider's `answer()`/`hangup()` are NOT exposed through the API: they only flip local UI state, and reporting `can_answer=true` for that would lie to remote clients. The bridge calls `IPhoneStateService` invokables — never PipeWire/BlueZ directly (rail R1; the D-Bus client is D2's `TelephonyClient` behind the provider).

### 9.4 Capability query — §7. Ping/Pong — §4.

## 10. Inbound reports (`companion.proto`) — companion parity

Fire-and-forget client→server messages (`request_id = 0`, no response; a malformed report is logged and dropped, never disconnects — GPS at 1 Hz must not be fragile):

- `GpsReport{latitude, longitude, speed_mps, bearing_deg, accuracy_m, age_ms, altitude_m?}` — field set mirrors today's live JSON (`lat/lon/speed/accuracy/bearing/age_ms`, staleness by `age_ms` not timestamp, >30 s = stale) plus optional altitude for the rewrite (greenfield contract, harmless now, awkward to want later).
- `BatteryReport{percent, charging}`.
- `ConnectivityReport{internet_available, socks5_active, socks5_port, socks5_password?}` — proxy host comes from the socket peer address, same as today (`CompanionListenerService.cpp:427`); on receipt the ingest forwards route + password to `SystemServiceClient::setProxyRoute()` exactly as the legacy service does. The password travels in-band (Codex review: the legacy flow derived it from the shared companion secret, `CompanionListenerService.cpp:496-500` — that derivation dies with the single global secret; the rewritten companion generates/sends its proxy password explicitly, which is simpler and avoids coupling both ends to a derivation rule).
- `TimeReport{unix_time_ms}` — fence delta #1 (§2): drives clock adjust on the RTC-less Pi.

Reports land in `ApiInboundState` (Q_PROPERTYs + signals mirroring `CompanionListenerService`'s surface: GPS fix, `phoneBattery`, `phoneCharging`, `internetAvailable`, `proxyAddress`) so QML (`CompanionService` context property) and `SystemServiceClient` consumers migrate by swapping one object. Actually retiring `CompanionListenerService`/port 9876 is the companion-rewrite executor phase, **not** Phase B (arch §5.A) — v1 ships the ingest path dormant-but-tested alongside the legacy service.

## 11. Envelope & field-number plan (`api.proto`)

`ApiMessage{uint64 request_id = 1; oneof payload{...}}` — exactly one per WS binary frame; length-prefixed on TCP (4-byte BE, then bytes). Oneof field numbers are allocated in blocks so domains grow without collisions; unused numbers in each block are `reserved` in-file from day one:

| Range | Domain |
|---|---|
| 2–9 | cross-cutting: `error=2, ack=3, ping=4, pong=5` (6–9 reserved) |
| 10–19 | handshake/auth/pairing: `client_hello=10, server_hello=11, auth_required=12, auth_response=13, auth_reject=14, pairing_challenge=15, pairing_response=16` (17–19 reserved) |
| 20–29 | subscription + capabilities: `subscribe_request=20, subscribe_response=21, unsubscribe_request=22, get_capabilities_request=25, capabilities_response=26` |
| 30–39 | status streams: `media_status=30, navigation_status=31, projection_status=32, phone_status=33, system_status=34` (35–39 reserved — `location` is the anticipated 35) |
| 40–49 | actions |
| 50–59 | notifications |
| 60–69 | phone commands |
| 70–79 | inbound reports |
| 80–99 | reserved for future domains |

Files per the locked convention (arch §4): `api.proto` (envelope + handshake + subscription + capabilities), `common.proto` (`Error`, `Ack`, `Ping`, `Pong`, `Topic`), one file per domain. Package `prodigy.api.v1`, proto3, `optional` for presence-relevant scalars, every enum's zero value `*_UNSPECIFIED`, every message doc-commented with delivery semantics. **Original schema — no HUDIY names, layouts, or wire compatibility** (standing licensing decision).

## 12. Config & persistence

New top-level `api` namespace in `YamlConfig::initDefaults()` (pattern: every existing block):

```yaml
api:
  enabled: true
  tcp_port: 9810        # avoids 9876 legacy companion port
  ws_port: 9811
  expose_lan: false     # opt-in; loopback + 10.0.0.0/24 always allowed
  max_queue_bytes: 1048576
  pairing_timeout_s: 120
  handshake_timeout_ms: 5000
```

Paired clients: `~/.openauto/api_clients.yaml` (mode 0600, separate from `config.yaml` — bug-report hygiene, §5). Web-config exposure of `api.*` settings is **out of v1** (IpcServer marshals fixed key subsets; adding `api.*` there is an executor follow-up, not load-bearing).

## 13. Error model

`Error{code, message}` echoing the failing `request_id` (or 0 for connection-level faults sent just before disconnect). Codes: `UNKNOWN, INVALID_REQUEST, UNSUPPORTED_VERSION, NOT_AUTHENTICATED, AUTH_FAILED, PAIRING_WINDOW_CLOSED, NOT_FOUND, UNKNOWN_ACTION, REJECTED, UNAVAILABLE, INTERNAL`. Request-scoped errors never disconnect; handshake-phase errors and framing violations always do (§4, §6).

## 14. Versioning & evolution

- Package stays `prodigy.api.v1` for all additive growth; `api_version_minor` bumps per additive change batch; clients feature-detect via capabilities, never version-sniff where a capability exists.
- Additive-only after freeze (rail R5): field numbers never reused, messages never renamed, semantics never silently changed; new capability = new field/message + capability flag. The `reserved` blocks in §11 are the growth room.
- A `prodigy.api.v2` package is the only escape hatch for breaking change, and it would run beside v1 behind `ClientHello.requested_api_version_major` — not planned, documented so nobody invents a fourth version mechanism.

## 15. Test plan

**Unit (serializers — no sockets):** `test_api_serializers` — per-domain: given service state (real service instances driven directly: `MediaStatusService::updateBtMetadata(...)` etc.), assert exact proto snapshot, including every normalization table in §8 (BT playback ints, projection switch, call-state mapping, `started_at` synthesis, theme token naming, `MEDIA_SOURCE_*` strings).

**Unit (framing):** `test_api_framing` — TCP length-prefix reassembly: partial reads, coalesced frames, oversized-length disconnect, zero-length rejection.

**Unit (auth):** `test_api_pairing` — PIN→salt→secret derivation vectors, challenge/response accept/reject, window expiry, store round-trip (0600 mode), duplicate-pairing replaces.

**Loopback integration** (`test_api_loopback`, pattern: `tests/test_companion_listener.cpp` — real server on `127.0.0.1` high port, `QTcpSocket`/`QWebSocket` clients, `processEvents()` pumping; `QT_QPA_PLATFORM=offscreen` env like existing socket tests). Mandatory cases (arch §8 test strategy, all six):

1. handshake happy path (TCP and WS; localhost-trust path)
2. auth-reject (bad HMAC; unknown client; pairing window closed) — *loopback peers are trusted, so auth paths are exercised by constructing sessions with a forced-remote test seam (`ApiSession::setPeerTrustOverrideForTest`)*
3. snapshot-on-subscribe (subscribe → immediate current state per topic; unavailable topic rejected)
4. delta delivery (mutate service → subscribed clients get fresh snapshot; unsubscribed topic silent; coalescing collapses same-turn bursts)
5. slow-consumer disconnect (client stops reading, publisher floods until `bytesToWrite` cap → session dropped, other clients unaffected, main thread never blocks)
6. client-action lifecycle (register → visible in list → dispatch routes `ActionInvokedEvent` to owner → disconnect auto-unregisters → dispatch now returns `dispatched=false`)

Plus: notification ownership rule; phone command `UNAVAILABLE` when capabilities false; inbound-report ingest updates `ApiInboundState` and forwards proxy route (mock `SystemServiceClient` seam).

Build gates: `cmake .. && make -j$(nproc)` (WSL2 Trixie), `ctest --output-on-failure`, `./cross-build.sh` — all green before any task is "done".

## 16. Deliberately open / deferred

| Item | Where it lands |
|---|---|
| `location` status topic (rebroadcast companion GPS) | v1.1 — reserved slot 35 |
| Theme/wallpaper push from companion | companion-rewrite phase |
| Notification event stream | v1.1 if a client needs it |
| Web-config panel `api.*` settings page + paired-client management UI | executor follow-up post-v1 |
| Per-capability ACLs, TLS | v2, if a third-party ecosystem materializes |
| `CompanionListenerService`/port 9876 retirement | companion-rewrite executor phase (arch §5.A) |

## 17. Executor Guidance (mandatory)

**Invariants — violating any is stop-and-ask (inherits arch §8 verbatim, plus API-specific):**
1. Serializers are the only normalization site; no raw service ints/strings on the wire (§8).
2. All API code on the Qt main thread; no worker threads for handlers (locked). If profiling ever hurts, move *serialization* to a worker — never state access.
3. `bytesToWrite()` cap check before every send; disconnect on breach — never queue-and-hope (§6).
4. One session teardown path; client actions must not survive their session (§9.1).
5. Proto changes after freeze: additive only, and the §11 block plan is honored.
6. Never touch `libs/prodigy-oaa-protocol/` (read-only reference for AA enum mapping tables is allowed and expected).
7. Phone bridge calls `IPhoneStateService` only — if you find yourself importing anything PipeWire/BlueZ in `src/core/api/`, stop.

**Pitfalls (CLAUDE.md gotchas that WILL bite here + new ones from this design):**
- Qt6::WebSockets is **net-new to the build** (scout-verified: zero references today) — `find_package` component + link + `qt6-websockets-dev` in `docs/development.md`, `install.sh`, and the cross-build Docker image. Cross-build failure here is expected on first try; fix the image, don't #ifdef the feature away.
- `QWebSocket::sendBinaryMessage` + `bytesToWrite()` is the WS backpressure pair; on TCP it's `QTcpSocket::bytesToWrite()`. Both exist on the main thread only in this design — don't move sockets across threads.
- proto3 `optional` requires protoc ≥ 3.15 — Trixie's is fine; do not add a version pin ritual, `find_package(Protobuf REQUIRED)` matches the existing pattern.
- The oaa CMake pattern (custom `add_custom_command` + `protobuf::protoc`, `libs/prodigy-oaa-protocol/CMakeLists.txt:8-49`) is the template for `proto/api/` codegen — `protobuf_generate_cpp` mangles subdirectory layouts (documented in `docs/session-handoffs.md:550`). Generate with `--proto_path=proto` so includes are `api/xxx.pb.h`.
- `QTimer` needs the event loop — publishers' 0-ms coalescing timers are main-thread objects; fine here, but don't instantiate publishers before `QCoreApplication`.
- Media `playbackState` int semantics differ by source (§8.1) — a test that only covers BT will pass while AA values render garbage. Cover both in `test_api_serializers`.
- `IPhoneStateService`'s `phoneConnected/deviceName/callDuration` are NOT Q_PROPERTYs (scout) — connect to `connectionChanged`/`callStateChanged` signals and call getters; property-binding idioms won't compile.
- `NotificationService::post()` ignores the struct's `extra` map (scout) — don't route anything through `extra` expecting delivery.
- High-port collisions in loopback tests: follow `test_companion_listener.cpp`'s distinct-port-per-test convention (19876–19893 taken; use 19900+), or ephemeral ports per the oaa transport test.

**Definition of done (per arch §8):** design-doc rails cited in the plan; all §15 tests green locally (`ctest`) and cross-build clean; deviations recorded in `docs/session-handoffs.md`; proto untouched after freeze except additively.
