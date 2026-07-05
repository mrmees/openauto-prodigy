# Unified Extensibility Architecture (Phase A Keystone)

**Date:** 2026-07-05
**Status:** APPROVED-PENDING-REVIEW — produced under the approved sprint program (`2026-07-05-fable-work-program-design.md` §5.A).
**Grounded against:** main = origin/main at `e13b591` + substrate inventory (2026-07-05).
**Audience:** future implementing agents. Read the whole doc before implementing any phase that touches the API, JS bridge, dashboards, or overlays. The **Executor Guidance** section at the end is mandatory reading.

## 1. Purpose

Four roadmap items — External API, HTML/JS runtime, multi-dashboards, overlay framework — share contracts. This doc locks the cross-cutting decisions so they compose instead of colliding. Per-item designs (Phases B, C, E) fill in detail **within these rails**; if an executor finds a rail wrong, stop and record why in the session handoff rather than silently deviating.

## 2. The Layer Model

```
┌─────────────────────────────────────────────────────────────────┐
│ CONSUMERS                                                       │
│  native QML widgets · web widgets/apps (WebEngine)              │
│  overlays · companion app (rewrite) · third-party clients       │
└───────────────┬────────────────────────────┬────────────────────┘
                │ QML bindings               │ External API
                │ (in-process)               │ (protobuf: TCP + WebSocket)
┌───────────────┴────────────────────────────┴────────────────────┐
│ CORE SERVICES (typed, main-thread, source-arbitrated)           │
│  MediaStatusService · PhoneStateService · providers             │
│  ActionRegistry · NotificationService · ThemeService            │
│  WidgetRegistry/GridModel · OverlayService (new, Phase E)       │
└───────────────┬─────────────────────────────────────────────────┘
                │ EventBus topics (aa.*) + direct signal wiring
┌───────────────┴─────────────────────────────────────────────────┐
│ SOURCES                                                         │
│  AndroidAutoOrchestrator · BlueZ D-Bus (A2DP/AVRCP/HFP)         │
│  PipeWire · companion inbound reports · plugins                 │
└─────────────────────────────────────────────────────────────────┘
```

**Rules (the rails):**

R1. **The API serves core services, never sources.** API status streams bind to `MediaStatusService`, `PhoneStateService`, `IProjectionStatusProvider`, `INavigationProvider`, etc. — the layer that already arbitrates BT-vs-AA sources — never to raw EventBus topics (free-form strings, e.g. `aa.media.metadata` from `AndroidAutoOrchestrator.cpp:491`) and never to D-Bus/protocol internals. Rationale: EventBus topics are an internal transport with no schema or stability guarantee; the services are typed and already merge sources.

R2. **One external surface.** The External API (protobuf over TCP + WebSocket) is the only externally reachable integration surface. The companion app is an API client (decided 2026-07-05: `CompanionListenerService` and port 9876 retire after companion rewrite). The web-config panel's Unix-socket IPC stays for v1 (local, root-of-trust different, ships today) but must not grow new capabilities that belong in the API; migrating it onto the API is a v2 candidate.

R3. **Web widgets are API clients.** The JS bridge is a thin bootstrap, not a parallel RPC surface (see §5). Everything a web widget can do, any API client can do. This is deliberate dogfooding: the API stays honest because our own widgets live on it.

R4. **All mutation goes through ActionRegistry or explicit API requests.** No API message reaches into a service to set state directly except through the same paths QML uses (`ActionRegistry.dispatch`, provider invokables, `NotificationService.post`). Main-thread only.

R5. **Additive-only within v1.** Once the `.proto` is committed (Phase B freeze gate), field numbers are never reused, messages are never renamed, semantics never silently change. New capability = new field/message + capability flag.

## 3. External API — locked decisions

### 3.1 Transport & framing

- **Two listeners, one protocol:** `QWebSocketServer` (binary frames, exactly one `ApiMessage` per frame) and `QTcpServer` (length-prefixed: 4-byte big-endian unsigned length, then one serialized `ApiMessage`). Both feed the same session layer. WS default port 9811, TCP default port 9810 (config: `api.tcp_port`, `api.ws_port`, chosen to avoid 9876 legacy companion port).
- **Envelope:** a single top-level `ApiMessage { uint64 request_id; oneof payload { ... } }`. Responses echo `request_id`; server-initiated stream events use `request_id = 0`.
- **Handshake:** first client message MUST be `ClientHello { requested_api_version, client_name, client_kind, auth }`. Server replies `ServerHello { api_version, capabilities, session_id }` or `AuthRequired`/`AuthReject`. No other message is accepted pre-handshake; violations disconnect.

### 3.2 Auth & exposure

- **Bind:** localhost + AP interface (10.0.0.1) by default; LAN (e.g. 192.168.x) exposure is opt-in config (`api.expose_lan: false`).
- **Localhost is trusted** (no auth): the head unit is a single-user appliance; web widgets run in-process WebEngine and connect to 127.0.0.1. This keeps the JS bridge token-free.
- **Remote clients pair once:** PIN/QR pairing adapted from the proven companion flow (`CompanionListenerService.cpp:114` — secret = SHA256(PIN + salt), then HMAC challenge/response in `ClientHello.auth`). Paired client IDs persist in YAML. Pairing UI is triggered via action (`api.pairing.start`) from Settings.
- **v1 has no per-capability ACLs.** Paired = full API. Recorded as a known simplification; revisit only if third-party ecosystem materializes.

### 3.3 Threading & delivery

- The API service lives on the **Qt main thread** (Qt networking classes require an event loop; providers/EventBus already deliver there; traffic is status-change-scale, not media-scale). Protobuf serialization on main thread is acceptable at v1 volumes; if profiling ever says otherwise, move serialization — not state access — to a worker.
- **Snapshot-on-subscribe:** `Subscribe { topics[] }` immediately returns current state per topic, then deltas. Late joiners never have to reconstruct state.
- **Backpressure:** per-client outbound queue with a hard cap (config `api.max_queue_bytes`, default 1 MiB); on overflow, disconnect the client (slow consumer must not stall the head unit). Document in proto comments.

### 3.4 v1 domains (scope fence)

Status streams: `media`, `navigation`, `projection`, `phone`, `system` (day/night + theme tokens, app version, connected devices summary). Requests: action list/dispatch/register (client-registered actions auto-unregister on disconnect), notification post/dismiss, capability query. Inbound reports (companion parity): GPS fix, phone battery/charging, internet/proxy availability. **Nothing else in v1** — no EQ control, no OBD, no cover-art injection, no overlay geometry control (Phase E adds overlay visibility actions as *actions*, which the API gets for free via dispatch).

- **Phone stream caveat (freeze gate):** phone *status* is modeled on HFP-standard indicator semantics (incoming/dialing/alerting/active/held/waiting), NOT on today's UI-mock enum. Phone *actions* (dial/answer/hangup/DTMF) ship in the schema but their availability is a capability flag whose truth arrives with Phase D. The proto is not committed until D1 picks the call-control path.

## 4. Schema conventions (Phase B implements)

- proto3, package `prodigy.api.v1`, one file per domain (`api.proto` envelope + `common.proto`, `media.proto`, `navigation.proto`, `phone.proto`, `projection.proto`, `system.proto`, `actions.proto`, `notifications.proto`, `companion.proto`).
- Original schema — no HUDIY message names, field layouts, or wire compatibility (licensing stance, standing decision).
- Every message carries doc comments including delivery semantics. `reserved` ranges declared up front for future use. Enums get `_UNSPECIFIED = 0`.
- Lives in `proto/api/` in the prodigy repo (NOT in `libs/prodigy-oaa-protocol/` — that submodule is the AA wire protocol, community-managed, hands-off; the External API is prodigy-private by decision).

## 5. JS bridge — the keystone decision

**Web content talks to the API over WebSocket like any other client.** The injected `prodigy` object (via `QWebEngineScript` at document creation) is a bootstrap shim only:

- `prodigy.apiUrl` — `ws://127.0.0.1:<port>` (localhost-trusted, no token plumbing).
- `prodigy.context` — widget instance context: `instanceId`, grid span, `kind` (widget/app/overlay).
- Theme tokens delivered as **live CSS custom properties** (`--prodigy-bg`, `--prodigy-accent`, …) injected/updated by the shim from ThemeService — so pure-CSS widgets need zero JS.
- A small convenience lib (`prodigy.subscribe(topic, cb)`, `prodigy.dispatch(actionId, payload)`) that wraps the WebSocket + protobuf-JS encoding — sugar over the public API, not a second surface.

**Explicitly rejected: QWebChannel** as the widget RPC mechanism. It would create a second, Qt-private RPC surface with its own lifetime and marshaling semantics, and web widgets built on it would not run anywhere except inside our WebEngine. With the WebSocket approach, the same widget HTML can run in a desktop browser against a paired LAN API — which is also the widget developer story (develop on your PC, deploy to the car).

Consequence for sequencing: **the JS runtime (Phase C proper) depends on API v1 existing.** The spike does not — it measures WebEngine itself.

## 6. Dashboards & overlays — contracts Phase E must honor

- **Dashboards:** multiple named dashboards = multiple `WidgetGridModel` instances keyed by dashboard id; YAML `widget_grid` (v3) grows to `dashboards[]` with a documented v3→v4 migration (v3 config becomes `dashboards[0]`, name "Home"). Widget *sizing* already exists (col/row spans + min/max in `WidgetDescriptor`) — HUDIY-style size options are picker UX over existing spans, not a new model.
- **Web widgets** enter as `WidgetDescriptor { qmlComponent: WebWidgetHost.qml, defaultConfig: { url, … } }` — one host component, config-schema-driven, `DashboardContributionKind` gains `WebWidget`. Gated on the Phase C spike verdict.
- **Overlays:** generalize the hardcoded Shell.qml sibling stack (NotificationArea / dim / GestureOverlay / PairingDialog / IncomingCallOverlay, `Shell.qml:73-100`) into an `OverlayService` (C++ registry: id, source plugin, geometry, z-band, visibility) + `OverlayHost` (QML Repeater over a model). Visibility/position mutations are **actions** (`overlay.<id>.show/hide/toggle/move`) so QML, key bindings, and the API all drive overlays through one path (rail R4). Existing overlays migrate incrementally — new framework first, migration as separate tasks, `NotificationService`'s unrendered kinds (`incoming_call`, `status_icon` — `NotificationArea.qml:33` renders toasts only) become overlay-framework consumers.
- **Z-order contract:** fixed bands — content < notifications < overlays(user) < pairing/call(system-modal) < gesture. Within a band, registration order. No per-overlay arbitrary z.

## 7. What stays open (deliberately)

| Question | Owner | Why deferred |
|---|---|---|
| Exact proto field layout | Phase B | Needs D1 verdict for phone actions |
| WebEngine go/no-go, process/lifecycle model, widget packaging | Phase C | Empirical |
| Dashboard switching UX, overlay drag-to-move | Phase E | UX detail, no cross-cutting impact |
| Web-config IPC migration onto API | v2 | Ships today; don't churn |
| Per-capability ACLs | v2+ | No third-party ecosystem yet |

## 8. Executor Guidance (mandatory)

**Invariants — violating any of these is a stop-and-ask:**
1. Never expose EventBus topics, D-Bus paths, or AA protocol internals through the API (rail R1/R4).
2. Never edit `libs/prodigy-oaa-protocol/` (community submodule; the API proto lives in `proto/api/`).
3. All service access from API handlers happens on the Qt main thread. Qt networking already delivers there; do not introduce worker threads for handlers.
4. Additive-only proto evolution after the freeze (rail R5).
5. The `prodigy` JS shim gets no capability the public API lacks (rail R3).

**Pitfalls already known (from CLAUDE.md gotchas + this design):**
- QTimer needs a Qt event loop; anything ASIO-side must marshal via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
- `QDBusArgument >>` can't extract `QVariantMap` directly (matters if any handler touches BlueZ-adjacent state).
- Boost.ASIO sockets don't set `SOCK_CLOEXEC` — but the API uses Qt networking, so do not copy the AA transport's socket code.
- The slow-consumer disconnect (§3.3) is load-bearing: a stalled WebSocket client must never block the main thread. Test it explicitly.

**Test strategy:** unit tests per domain serializer (service state → proto snapshot); a loopback integration test (QTcpSocket + QWebSocket client in ctest) covering handshake, auth-reject, snapshot-on-subscribe, delta delivery, slow-consumer disconnect, and client-action lifecycle (register → dispatch → disconnect-unregisters). Follow existing test layout in `tests/` (47→88-test suite conventions).

**Definition of done for any phase built on this doc:** its design doc cites which rails it consumed; deviations recorded in `docs/session-handoffs.md`; full suite + cross-build pass per project workflow.
