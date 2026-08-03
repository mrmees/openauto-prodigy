# Web Widget Authoring Guide

This guide covers **HTML/JS web widgets** — dashboard tiles built from an HTML/CSS/JS package, hosted in a locked-down `WebEngineView` instead of native QML. If you're building a widget in QML instead, see [widget-developer-guide.md](widget-developer-guide.md).

Read [Known Limitations (v1)](#known-limitations-v1) before you write any code. The shim is a young, deliberately-minimal API client — several corners were cut for v1 and you need to design around them, not discover them at 2am.

---

## What a Web Widget Is

A web widget is a directory of HTML/CSS/JS served over a custom `prodigy://widgets/<id>/<entry>` scheme and rendered inside a `WebEngineView` tile on the dashboard grid — the same grid, same sizing contract, same picker as native QML widgets. Content never touches the filesystem or the network directly; it's read in-process from the package directory by a scheme handler.

Web widgets talk to the head unit exclusively through the **External API** (WebSocket, `ws://127.0.0.1:<api.ws_port>`) via an injected `window.prodigy` convenience shim — there is no QWebChannel, no direct QML access, no second RPC surface. If you want the full architecture (why `prodigy://` instead of `file://`, single-origin rationale, packaging/discovery, shim internals, security model), read the design doc (design history): `docs/archive/plans/2026-07-06-js-runtime-design.md`. This guide doesn't re-derive that — it's the practical "how do I build one" companion.

---

## Package Layout & Manifest

A web widget lives under `~/.openauto/webwidgets/<pkg>/`:

```
~/.openauto/webwidgets/
  speedo/
    widget.yaml
    index.html
    speedo.js, style.css, assets/...
```

`widget.yaml` fields (from `src/core/widget/WebWidgetManifest.hpp`):

| Field | Type | Default | Notes |
|-------|------|---------|-------|
| `id` | string | (required) | Unique; becomes the `prodigy://widgets/<id>/` URL segment. Must match `^[A-Za-z0-9][A-Za-z0-9._-]*$` — no slashes, no leading dot. |
| `name` | string | (required) | Picker display name. |
| `entry` | string | `index.html` | Entry HTML file, relative to the package dir. No leading `/`, no `..`. |
| `category` | string | `status` | Picker category: `status` / `media` / `navigation` / `launcher`. |
| `description` | string | `""` | Shown in the picker. |
| `icon` | string | `""` | Material icon codepoint, same convention as native widgets. |
| `size.minCols` / `minRows` | int | `1` / `1` | Grid span floor. |
| `size.maxCols` / `maxRows` | int | `6` / `4` | Grid span ceiling. |
| `size.defaultCols` / `defaultRows` | int | `1` / `1` | Placement default. |

Example manifest:

```yaml
id: com.example.speedo
name: "Speedometer"
entry: index.html
category: status
description: "GPS speedometer"
icon: ""
size:
  minCols: 1
  minRows: 1
  maxCols: 6
  maxRows: 4
  defaultCols: 2
  defaultRows: 2
```

There's no `config:` schema field yet — per-widget configuration UI is a v1.1 item, not implemented. Ship whatever config your widget needs baked into its own JS/localStorage for now (see the storage limitation below on why that's ephemeral).

**Getting registered:** `WebWidgetScanner` (`src/core/widget/WebWidgetScanner.cpp`) walks `~/.openauto/webwidgets/` once at startup. A subdirectory with no `widget.yaml` is skipped with a log line; an invalid manifest (bad `id`, missing `name`, inconsistent size bounds) is skipped with a warning; a duplicate `id` keeps the earlier registration and warns. **If your widget doesn't show up in the picker, `journalctl` is the first stop** — see the [Debugging Checklist](#debugging-checklist).

---

## The `prodigy` API Surface

At `DocumentCreation`, the host (`qml/widgets/WebWidgetHost.qml`) injects a chain of scripts into every widget document: a per-instance bootstrap object (`window.__prodigyBootstrap` — API URL, widget context, a themed CSS-token snapshot), the protobuf runtime + generated API types, the static shim (`resources/web/prodigy.js`) that builds `window.prodigy` from the bootstrap, and an internal host-gestures helper (edit-mode long-press). The two you interact with are the bootstrap and the shim; the rest is plumbing. First paint is already themed from the bootstrap snapshot — no flash-of-unthemed-content while the WebSocket connects.

Theme tokens land as CSS custom properties on `<html>`: token `on-primary` → `--prodigy-on-primary`. A pure-CSS widget gets full day/night theming with zero widget JS.

`window.prodigy` (this is the whole surface — verified against `resources/web/prodigy.js`; nothing else is exposed):

| Member | Signature | Behavior |
|--------|-----------|----------|
| `prodigy.ready` | `Promise` | Resolves once, after the first `ServerHello`. See limitations below — it does not re-pend on reconnect. |
| `prodigy.context` | `{instanceId, widgetId, colSpan, rowSpan, kind}` | Current placement; updated live via `contextchange`. |
| `prodigy.apiUrl` | string | The raw WS URL, for widgets that want their own socket. |
| `prodigy.subscribe(topic, cb)` | `(string, fn) -> unsubscribe fn` | Topics: `"media"`, `"navigation"`, `"projection"`, `"phone"`, `"system"`. `cb` receives the status object each time it changes. |
| `prodigy.data.listCatalog()` | `() -> Promise<DataCatalog>` | Present only when the server advertises the external data-provider capability. Returns the current deterministic live provider/channel catalog. |
| `prodigy.data.subscribe(ref, cb)` | `({providerNamespace, channelName}, fn) -> unsubscribe fn` | Exact-channel live data. Multiple local callbacks share one server subscription; the last unsubscribe removes it server-side. |
| `prodigy.dispatch(actionId, payload)` | `(string, any?) -> Promise<boolean>` | Fires a host action; resolves to whether it was dispatched. |
| `prodigy.notify(message, {priority, ttlMs})` | `(string, object?) -> Promise<string>` | Posts a toast notification; resolves to a notification id. |
| `prodigy.request(apiMessageObject)` | `(object) -> Promise<response>` | Low-level escape hatch — build your own `ApiMessage` field for anything not covered above. |
| `prodigy.on(name, cb)` | `("themechange"\|"contextchange", fn)` | Fires on live theme flips and span/placement changes. |

For the full wire protocol behind all of this (subscription/delivery model, requests, error model), see `docs/archive/plans/2026-07-06-external-api-v1-design.md` (design history) §6 (Subscription & delivery model) and the rest of that doc.

Minimal example:

```html
<!-- index.html -->
<!DOCTYPE html>
<html>
<body style="background:transparent; color:var(--prodigy-on-surface); font-family:sans-serif;">
  <div id="speed">--</div>
  <script>
    prodigy.subscribe('navigation', function (status) {
      document.getElementById('speed').textContent =
        (status.speedKph || 0).toFixed(0) + ' km/h';
    });
  </script>
</body>
</html>
```

### Live external data

`prodigy.data` appears after `prodigy.ready` only when the connected server's
capabilities include the full data-provider bridge. It is the supported surface
for gauges and other widgets that display values supplied by an independent
backend. Names identify sources; they do not tell Prodigy whether a value came
from OBD-II, CAN, GPIO, MQTT, a simulator, or anything else.

```javascript
await prodigy.ready;
if (!prodigy.data) throw new Error('external data API unavailable');

const catalog = await prodigy.data.listCatalog();
const unsubscribe = prodigy.data.subscribe({
  providerNamespace: 'com.example.vehicle',
  channelName: 'engine.rpm'
}, event => {
  const usable = event.sample &&
    ['good', 'degraded', 'unknown'].includes(event.sample.quality) &&
    event.sample.value !== undefined;
  if (!event.available || !usable) {
    renderUnavailable(event.unavailableReason || event.sample?.quality);
    return;
  }
  renderValue(event.sample.value, event.definition.unit);
});
```

The first callback after a successful subscription is an availability boundary.
References to an absent provider or channel are accepted and wait; a later
registration/declaration activates them without another call. When a retained
sample exists, its value event follows the `available` event. Metadata changes
produce another `available` event with the new definition before later values.
Removal, provider disconnect, and widget-socket disconnect produce immediate
unavailability. Active bindings are restored automatically after reconnect.

The callback object has this stable shape:

```javascript
{
  providerNamespace: 'com.example.vehicle',
  channelName: 'engine.rpm',
  available: true,
  unavailableReason: null,       // provider_absent/channel_absent/etc. when false;
                                 // link_lost means this widget's API socket closed
  definition: {                  // present only while available
    channelName: 'engine.rpm',
    displayName: 'Engine RPM',
    valueType: 1,                // DataValueType enum
    unit: 'rpm',
    nominalIntervalMs: 100,
    staleAfterMs: 1000
  },
  sample: {
    value: 975.5,
    scalarType: 'double',        // double/signed_integer/unsigned_integer/boolean/string/enum
    timestampMs: 1722000000000,  // backend observation or Prodigy receipt wall time
    receivedAtMonotonicMs: 250,  // performance.now() at this widget's receipt
    quality: 'good',
    enumLabel: undefined         // populated for a matching current enum option
  }
}
```

Scalar conversion is fixed: doubles are JavaScript `number`; signed integers,
unsigned integers, and enum values are exact `bigint`; booleans and strings
retain their native types. Do not pass an integer through `Number(...)` unless
your renderer has first proved it is inside JavaScript's safe-integer range.
`timestampMs` is deliberately a number because Unix epoch milliseconds are
safe, but it is provenance/display data—not a freshness clock.

Samples whose quality is `stale`, `invalid`, or `unavailable` may omit the
scalar value. Treat quality and value presence as usability gates; availability
alone says the channel definition is live, not that the current sample is fit
to display.

Every live gauge must own a finite positive stale timeout. Schedule it from
`receivedAtMonotonicMs` and compare against `performance.now()`; never compare
`timestampMs` with `Date.now()`. A vehicle Pi may boot without correct RTC/NTP
time or step its wall clock later. The server does not request, throttle,
coalesce, or infer a provider cadence, and it does not probe idle providers in
v1.2, so the widget's monotonic stale policy is the defense against a half-open
provider connection.

---

## Known Limitations (v1)

Read this section before you build anything you plan to rely on. Every item below reflects a real, verified behavior in the shipped code — not a hypothetical.

### Requires `api.enabled: true`

The shim is nothing more than an External API WebSocket client. If the External API is disabled in config, your widget will render (HTML/CSS load fine — the scheme handler doesn't care about the API) but `prodigy.ready` never resolves and every `subscribe`/`dispatch`/`notify`/`request` call sits there or rejects — the widget is stuck showing "connecting…" forever behind a capped reconnect loop. As of the current build the app logs this for you: if web widgets are registered but `api.enabled` is false, startup emits a warning — *"Web widgets are registered but api.enabled is false — they will render and spin 'connecting…' forever (web widgets require the External API; set api.enabled: true)"* (`src/main.cpp`, web widget registration block). There's no code-side fix on the widget's end — tell your users to check `api.enabled: true` in config, and don't assume the API is up just because your widget loaded.

### localStorage / sessionStorage / IndexedDB are ephemeral & off-the-record

`WebWidgetHost.qml` uses the **default QML WebEngine profile**, which runs off-the-record. Anything you write to `localStorage`, `sessionStorage`, or IndexedDB inside a widget evaporates the next time the app restarts (reboot, service restart, crash). Do not use browser storage APIs for anything you need to survive a restart — keep durable state server-side via the API (`prodigy.request`/`dispatch` against something the host persists), or design your widget to be fine re-deriving its state from scratch every session. Persistent per-widget storage is a plausible future change (tracked in `docs/wishlist.md`), but it isn't there today.

### Shared origin across widgets (D2)

All web widgets in v1 share **one** browser origin — there is no per-widget isolation. From the design doc (`docs/archive/plans/2026-07-06-js-runtime-design.md`, design history, §3, "Single origin (D2)"):

> **Single origin (D2):** all widgets share the `prodigy://widgets` origin, so all views share one renderer process — the spike's measured cheap case. Consequence, accepted for v1: widgets share localStorage and could read each other's DOM-visible state if they embedded each other (they can't — see navigation policy §5). Web widgets are user-installed local content with the same trust level as native QML widgets (which already have full QML runtime access), so isolation between them buys little today. Per-widget origins (`prodigy://<id>`) are the recorded v2 hardening path if third-party widget distribution ever materializes; the spike says even hostile per-origin isolation (~60–100 MB/origin) stays in budget.

Practically: if two installed widgets both write `localStorage.setItem('config', ...)`, they collide — there's no per-widget bucket. **Namespace your storage keys** (e.g. prefix with your `id` from the manifest) if you use `localStorage` at all — see the ephemerality caveat above for why you shouldn't lean on it heavily regardless. Per-widget origins are a wishlisted v2 hardening path, not something you can opt into today.

### v1 shim simplifications

The shim (`resources/web/prodigy.js`) trades a few edge-case guarantees for simplicity in v1 (from the JS-runtime execution ledger's final-review triage). None of these are bugs to work around with hacks — they're documented behavior to design against:

- **`prodigy.ready` resolves once and never re-pends.** It flips to resolved after the first `ServerHello` and stays that way for the life of the page. If the connection drops and reconnects later, `ready` doesn't go back to pending — but any `request()`/`dispatch()`/`notify()` call issued **during** that reconnect gap will **reject** with `"prodigy: not connected"` rather than queueing (this reflects the just-landed guard on `ws.readyState !== 1` in `request()` — previously such calls silently black-holed forever instead of rejecting). **Your widget must catch rejections and retry**, not assume a resolved `ready` means "connected right now."
- **`subscribe()` is best-effort.** An unknown topic name throws synchronously, client-side, so you'll catch that immediately during development. But if the *server* rejects a subscription for some other reason, there's no ack in v1 — you won't be told; your callback simply never fires for that topic.
- **`unsubscribe()` is local-only.** Calling the function `subscribe()` returns stops your callback from firing immediately, but the server keeps streaming that topic to your connection until the next reconnect/resubscribe cycle rebuilds the topic list. Harmless (wasted bandwidth, not a leak you can observe), but don't assume unsubscribing stops server-side work instantly.
- **Connection-level errors aren't surfaced to widget code.** An `Error` response with no correlating `request_id` (i.e., not a reply to something you called) is handled internally via the reconnect path — your widget never sees it directly. Watch for reconnect behavior (widget going blank/stale) rather than expecting an error callback.

### Locked-down sandbox

`WebWidgetHost.qml` runs each widget under Chromium defaults plus explicit lockdown (verified against `qml/widgets/WebWidgetHost.qml`):

- `settings.javascriptCanOpenWindows: false`, `settings.fullScreenSupportEnabled: false` — no `window.open()`, no fullscreen requests.
- `settings.localContentCanAccessFileUrls: false` — no `file://` access; your widget can only read its own package directory through the `prodigy://` scheme handler.
- `settings.localContentCanAccessRemoteUrls: true` — **https subresources are allowed** (fetch a weather API, load a remote font, etc.), but top-level navigation is restricted to same-origin `prodigy://widgets/` URLs (`onNavigationRequested` rejects anything else). There's no way to navigate your widget's top-level document off to an external site, and no external browser handoff.

Design this in from the start: a widget that expects to pop a window, go fullscreen, or link out to a browser simply won't — those requests are silently denied, not errored back to your JS.

### Crash recovery (D5)

If the widget's renderer process dies (a rendering bug, an OOM, whatever), `WebWidgetHost.qml` auto-reloads it — 3 attempts with 2s/4s/8s backoff, then an error card ("tap to retry") if all three fail. On every successful reload (including after a crash), the host re-pushes the live widget context (span/placement) via `pushContext()` — that's a just-landed fix; the reload reuses the same `WebEngineView`, so without it a crash-reloaded widget would be stuck reading whatever `colSpan`/`rowSpan` it had at original construction time, not its current placement.

**Write your widget's init code to be idempotent** — it will run again from scratch on a fresh document load after any crash, with no guarantee prior JS state (in-memory variables, timers, DOM state) survived. Don't assume `index.html` only ever executes once per widget lifetime.

---

## Debugging Checklist

**My widget doesn't appear in the picker:**

1. Check `journalctl -u openauto-prodigy` (or the console, on a dev box) for `WebWidgetScanner` lines around startup:
   - `WebWidgetScanner: skipping <dir> — no widget.yaml` — your package directory is missing the manifest, or it's misnamed.
   - `WebWidgetScanner: skipping invalid package <path>` — the manifest parsed but failed validation (bad `id`, missing `name`, inconsistent size bounds — see the manifest table above).
   - `WebWidgetScanner: duplicate widget id <id> — keeping the earlier registration` — two packages share an `id`; only the first-scanned one wins.
   - `Registered N web widget(s) from <path>` — always logged, even when `N` is 0. If you see `Registered 0 web widget(s)`, the scanner ran but found nothing valid.
2. Confirm `api.enabled: true` in config — the widget can still be *registered* and appear in the picker with the API off, but it'll be functionally dead (see the limitation above). If nothing ever connects, check for the `api.enabled is false` warning at startup.
3. Confirm your manifest's `entry:` field matches the actual HTML filename in the package directory, and that the file exists.

---

## See Also

- `docs/archive/plans/2026-07-06-js-runtime-design.md` (design history) — full architecture: `prodigy://` scheme, packaging/discovery, `WebWidgetHost.qml` lifecycle, shim internals, security model, deferred v2 items.
- `docs/archive/plans/2026-07-06-external-api-v1-design.md` (design history) — the External API wire protocol the shim rides on top of.
- [widget-developer-guide.md](widget-developer-guide.md) — native QML widget development (the other widget path).
- `docs/engineering-backlog.md` — residual shim-contract and package/runtime
  findings that require fresh research before promotion.
- `docs/wishlist.md` — the user-facing persistent isolated widget-storage
  capability.
