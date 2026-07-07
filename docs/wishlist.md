# Wishlist

Ideas captured here. Promote to `roadmap-current.md` when ready to commit.

## Bugs / Infrastructure

- **SDP `/var/run/sdp` permissions race** — BlueZ `ExecStartPost` wait loop (5x 0.5s) loses the race; socket stays `root:root` instead of `root:bluetooth`. App-side retry (2s x 30) helps but doesn't fix root cause. Need `inotifywait` on socket creation or a longer/smarter polling loop in the systemd override. Reproduces on every fresh boot.

- **Web config panel broken** — DIAGNOSED 2026-07-02: the panel code is healthy. All 5 pages + 10 API endpoints verified working, both without the Qt app (graceful "Qt app not running" degradation) and against a mock IpcServer with matching schema/framing (`{"command",...}` newline-delimited JSON, commands match `IpcServer.cpp` 1:1). Root cause is deployment: both installers `systemctl enable`d the web service but never *started* it (fixed 2026-07-02 → `enable --now` in `install.sh` + `install-prebuilt.sh`); the other candidate on old installs is missing `python3-flask` (crash-loop — check `journalctl -u openauto-prodigy-web`). On the Pi: `sudo systemctl start openauto-prodigy-web` should bring it up immediately. Minor follow-up: the settings form sends `video_resolution`/`brightness`/`night_mode`, which `handleSetConfig` silently ignores.

- **Boot/reboot startup reliability** — After a reboot, `graphical.target` was slow to activate (stuck on `systemd-networkd-wait-online.service` timeout). Prodigy service depends on `graphical.target` so it sat in `inactive (dead)` until the timeout passed. Need to verify clean boot sequence, measure time-to-app, and possibly mask the networkd-wait service (NetworkManager is the actual manager). Phase 4 territory.

## Deferred UI Features

- **Live sidebar toggle reactivity** — Toggling sidebar enable/disable mid-AA-session doesn't update AA video margins or evdev touch zones. Sidebar draws over AA content and touch passes through. Requires app restart. Needs: config change signal → margin recalculation → touch zone update pipeline for mid-session changes.


- **Settings tile subtitles** — Live status text under each tile icon (e.g., "720p 60fps", "BT: Connected"). Removed from v0.4.3 — too small to read on 1024x600 automotive display. Revisit with larger font or alternate layout in a future milestone.

- **WiFi AP settings in UI** — Channel/band picker was in Connectivity settings, removed because WiFi AP config is set once at install via `install.sh` and doesn't need runtime changes. Could return if users need to switch channels without reinstalling.

## Candidate Ideas

- **FM radio (RTL-SDR + RDS)** — paid-alternative-parity feature, explicitly deferred as a nice-to-have (Matthew, 2026-07-02). RTL-SDR dongle for tuning, RDS decode for station/track text, integrated as a media source.

- **Companion notifications on head unit** — The companion app ([openauto-companion](https://github.com/mrmees/openauto-companion)) already does GPS/time/battery/internet sharing + theme transfer. Remaining paid-alternative-parity gap: displaying phone notifications on the head unit. Depends on the head-unit notification service (extensibility plan Priority 3).

- **Key-event navigation map** — paid-alternative-style keyboard/button bindings (focus movement, back, media keys, projection focus toggle) for steering-wheel buttons via GPIO/keyboard HID. Prodigy is touch-first today.

- **Per-connection WiFi password rotation** — Generate a fresh random WPA password each time a phone connects via BT RFCOMM, update hostapd (`hostapd_cli set wpa_passphrase` + reload), then send the new password to the phone. Eliminates any static credential. Requires coordinating hostapd reload timing with the BT handshake.

- **Three-tier community architecture** — Long-term goal is a clean separation into three public layers:
  1. **Protocol definitions** (`open-android-auto`, exists) — language-neutral `.proto` files. Anyone can generate bindings for any language.
  2. **Protocol implementations** (new repo, TBD) — community-contributed implementations in any language. Transport, framing, encryption, channel handlers. Our C++ one lives in `libs/prodigy-oaa-protocol/` today (needs de-Qt-ing first), but the repo would welcome Rust, Python, Go, Qt, COBOL — whatever someone wants to PR.
  3. **Application** (`openauto-prodigy`) — Qt/QML head unit that consumes the above two as submodules.

  **What needs to happen:**
  - De-Qt the protocol library: replace QObject signals/slots with `std::function` callbacks or Boost.Signals2, QByteArray/QString with STL, QTcpSocket with Boost.ASIO (already used elsewhere), QTimer with ASIO timers.
  - Split into its own public repo once Qt-free.
  - Openauto-prodigy consumes both repos as submodules, adds the Qt/QML application layer on top.

  **Open items in `open-android-auto`:**
  - `NavigationTurnEventMessage.proto` has unused imports (`ManeuverTypeEnum.proto`, `TurnSideEnum.proto`) — fields 2 and 3 should use the enum types instead of raw `int32`.

- **Persistent in-call control (global call bar/overlay)** — Found during HFP D2 live testing (2026-07-05): after answering a call, if you're not in the Phone view (e.g. home screen or a dashboard), there's no way to hang up — the incoming-call overlay only shows during `Ringing`, and PhoneView is one plugin among many. Need a small persistent call-control affordance (an "active call" status-bar chip or a slim overlay) visible whenever `CallStateProvider.callState == Active`, with at least a hangup button (and ideally mute/DTMF). Overlay-framework (Phase E) is the natural home — this is a z-banded overlay bound to the call provider, not a change to the phone plugin. Note: during fullscreen AA the phone renders its own in-call UI, so this matters most on the prodigy shell (launcher/dashboards).

- **API: head-unit proxy-route status feedback** — From the 2026-07-05 companion gap review: the companion reports "SOCKS5 active" via `ConnectivityReport`, but the API never tells the client whether the head unit actually applied the proxy route. A v1.1+ additive status field/event would help bridge reliability debugging. (Companion UI shows local state only for now.) Related non-item from the same review: advertising the web-config settings URL over the API was considered and rejected — web-config stays the canonical HTTP channel, apps construct the URL from the host convention.

- **Web-config: theme/wallpaper upload endpoint** — PROMOTED (decided 2026-07-05): companion theme/wallpaper transfer moves off legacy port 9876 onto a new web-config HTTP upload/install endpoint (multipart upload → validate → apply via existing IPC). Blocks legacy-9876 retirement; see roadmap companion-migration item.

## From multi-dashboards execution (2026-07-06)
- **Persistent edit-mode latch across dashboard switches** — deselect-on-switch currently exits edit mode on every pill tap (correctness fix); a dedicated latch would allow browsing dashboards while editing. Prereq: the outgoing-selection clear (landed in 1762349) — the stale-latch bug it fixed is what deselect-on-switch papered over.
- **Per-widget size-bound tuning** — picker preset popup shows ~4 options for nearly every widget (none pin to a single preset); curating minCols/maxCols per widget would restore one-tap add for fixed-size widgets.
- **Overlay design doc: document that QQuickOverlay-parented modal Dialogs (config/picker/manage sheets) composite ABOVE all Shell z-bands** — or plan migrating modals into the overlay framework. Surfaced by the dashboards final review as a cross-branch semantic note.

## From companion support batch (2026-07-06)
- **Daemon-side proxy-route auto-teardown (vanished-phone net)** — Follow-up to the owner-teardown decision (2026-07-06): the API now clears the route when the owning session closes, but a phone that silently vanishes (AP association drop, no TCP FIN — the AA tcp_info gotcha) can hold a zombie session and leave the iptables/redsocks blackhole up. The Python system daemon already health-checks the route every 10s and marks DEGRADED/FAILED without tearing down; let it auto-disable after ~3 consecutive failures. Needs a small design pass: interaction with legacy `CompanionListenerService::proxyRouteApplied_` (won't re-apply after a behind-its-back disable) and flap risk on transient congestion. Caller-agnostic — protects legacy AND API paths.
- **Legacy CompanionListenerService RNG hygiene** — `QRandomGenerator::global()` at CompanionListenerService.cpp:111 (pairing PIN), :223, :281 (challenge nonce, session key) — same weakness the API hardening fixed with `::system()`. Three-line fix if the service survives another release; otherwise dies with 9876 retirement. (Final-review finding, 2026-07-06.)
- **ApiServer failed-start retry hygiene** — if both listens fail, a retry re-news QTcpServer/QWebSocketServer without deleting the failed pair AND appends a duplicate publisher set (`createPublishers()` runs before the listen calls). Unreachable in production (main.cpp calls start() once); fix shape: clean up on the failure path or make the guard require stop() first. (Final-review finding, 2026-07-06.)
- **timedatectl handlers block the main thread** — `adjustClockFromApiTimeReport` and the new timezone handler each `waitForFinished(5000)`; a hung timedatectl stalls UI for 5s. Convert BOTH to async QProcess in one pass. (Final-review finding, 2026-07-06.)

## From JS-runtime execution (2026-07-07)
- **Shim v1 hardening block (prodigy.js + WebWidgetHost)** — final-review triage bundle, none individually branch-blocking: (1) `subscribe()` validates topics with `topic in TOPIC`, which accepts prototype-chain keys (`'toString'`) and then feeds a function into the topics array — use `hasOwnProperty`; (2) after a renderer crash+reload the re-run bootstrap carries creation-time spans — call `pushContext()` in the `LoadSucceededStatus` branch; (3) requests issued during a disconnect window register a `pending` entry nothing settles (resolved `readyPromise` + silent `ws.send()` on CLOSED) — guard on `ws.readyState !== 1`; (4) design-accepted v1 simplifications worth revisiting together: readyPromise never re-pends, fire-and-forget subscribe (rejected topic invisible), unsubscribe is local-only (server streams until next rebuild), malformed `apiUrl` throw inside the reconnect setTimeout kills the loop permanently, connection-level `Error` frames with request_id 0 are silently dropped.
- **Widget-author known-limitations doc** — before third parties build against the shim: the v1 simplifications above, localStorage is off-the-record (default QML WebEngine profile — evaporates every restart; widgets must not assume persistence), widgets depend on `api.enabled: true`, and the D2 shared-origin consequences (shared localStorage across widgets).
- **`api.enabled: false` produces zombie web widgets** — scan/registration isn't gated on the API being enabled; widgets would render and spin "connecting…" on a capped reconnect loop forever. One-line qWarning when API disabled but web widgets registered (or skip registration). Default is true, so latent.
- **Web-widget field-debugging logging** — package dir without `widget.yaml` is skipped with zero log, and a zero-widgets scan logs nothing ("why doesn't my widget appear" will be this feature's #1 support question). One qInfo each.
- **Webwidget hygiene batch (next time these files are touched)** — resolver: "packages_ frozen after startup scan — synchronize before any dynamic-install feature" comment on WebWidgetContentResolver.hpp (unsynchronized QHash is a hard precondition for install-at-runtime work); WebWidgetHost: latch `widgetUrl` at first activation (transient empty effectiveConfig.url would destroy a live view, defeating D4); manifest: `entry.contains("..")` over-rejects legit names like `foo..bar.html` (resolver jail backstops real traversal); gen-proto-js.sh: comment the `-p proto` include-path rationale in-script + pin exact toolchain versions on next regen (currently major-only, regen not byte-reproducible).
- **Per-widget WebEngine profile / persistent storage decision** — if widget localStorage should survive restarts, switch the host to a named persistent profile (weigh against D2's shared-origin isolation notes and the v2 per-widget-origin hardening path).
