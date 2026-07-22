# Wishlist

Ideas captured here. Promote to `roadmap-current.md` when ready to commit.

## Bugs / Infrastructure

- **SDP `/var/run/sdp` permissions race** — BlueZ `ExecStartPost` wait loop (5x 0.5s) loses the race; socket stays `root:root` instead of `root:bluetooth`. App-side retry (2s x 30) helps but doesn't fix root cause. Need `inotifywait` on socket creation or a longer/smarter polling loop in the systemd override. Reproduces on every fresh boot.

- **Web-config settings expose fields the app does not persist** — the service
  startup issue is fixed, but the settings form still sends
  `video_resolution`, `brightness`, and `night_mode` while
  `IpcServer::handleSetConfig()` ignores them. Remove those controls or wire
  them to the canonical configuration/theme owners with visible failures.

- **Pi user journald is not persistent** — bench 2026-07-15: `journalctl
  --user -u wireplumber` reports "No journal files were found", so
  WirePlumber/user-service logs are unavailable for forensics. Fix shape:
  `Storage=persistent` in journald.conf (or persistent /var/log/journal) on
  the Pi image + installer.

- **Boot/reboot startup reliability** — After a reboot, `graphical.target` was slow to activate (stuck on `systemd-networkd-wait-online.service` timeout). Prodigy service depends on `graphical.target` so it sat in `inactive (dead)` until the timeout passed. Need to verify clean boot sequence, measure time-to-app, and possibly mask the networkd-wait service (NetworkManager is the actual manager). Phase 4 territory.

- **Register debug logging keys in the configuration schema** — the Debug
  settings page, startup path, and IPC handlers use `logging.verbose` and
  `logging.debug_categories`, but `YamlConfig` does not define those leaves.
  Generic writes therefore reject them and startup reads cannot restore them.
  Add typed defaults plus coverage for UI/IPC writes and restart persistence.

- **Reconcile installer-only configuration keys** — the installer still writes
  display pixel dimensions that runtime does not consume, while
  `connection.bt_name` works only because load-time unknown keys are retained
  and `BluetoothManager` reads it directly. Either make the intended keys part
  of the supported schema with coverage or stop writing them.

## Deferred UI Features

- **Live Navbar viewport reconfiguration during AA** — The shipped path locks
  the Navbar-aware video margins and content-sized touch coordinate space during
  service discovery. Navbar visibility is therefore restart-required, and an
  edge change during projection does not renegotiate the phone's content region
  or rebuild the AA touch mapping. A future promoted design would need one
  authoritative live-update path for shell viewport, protocol margins/content
  config, touch content dimensions, and Navbar evdev zones. Do not base that
  design on wire ID `0x8012`: the current gold trace identifies it as the HU's
  theming-token status response to the phone's `0x8011` request. The separately
  traced runtime update-config messages must be evaluated when this idea is
  promoted.


- **Settings tile subtitles** — Live status text under each tile icon (e.g., "720p 60fps", "BT: Connected"). Removed from v0.4.3 — too small to read on 1024x600 automotive display. Revisit with larger font or alternate layout in a future milestone.

- **WiFi AP settings in UI** — Channel/band picker was in Connectivity settings, removed because WiFi AP config is set once at install via `install.sh` and doesn't need runtime changes. Could return if users need to switch channels without reinstalling.

## Candidate Ideas

- **Installer: offer example web widgets on fresh installs** — discovered on the 2026-07-08 clean reinstall: web widgets are user content (`~/.openauto/webwidgets/`), so a reflash silently loses them all and the dashboard comes up bare ("Registered 0 web widget(s)"); recovery was a manual `cp -r examples/webwidgets/hello-theme ~/.openauto/webwidgets/` + app restart. Installer (or first-run) could offer to seed `examples/webwidgets/*`. Related: user-content backup/restore (widgets + themes + config) across reinstalls.

- **FM radio (RTL-SDR + RDS)** — paid-alternative-parity feature, explicitly deferred as a nice-to-have (Matthew, 2026-07-02). RTL-SDR dongle for tuning, RDS decode for station/track text, integrated as a media source.

- **Companion notifications on head unit** — The companion app
  ([openauto-companion](https://github.com/mrmees/openauto-companion)) already
  reports GPS/time/battery/connectivity and transfers themes. The head unit's
  `NotificationService` and overlay framework now exist; the remaining work is
  an additive companion/API notification contract plus the display policy.

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

- **WebAppHost — fullscreen streaming web apps (Spotify / YouTube / parked video)** —
  Sibling of the widget runtime with inverted policy: manifest-driven app packages
  under `~/.openauto/webapps/` surfacing as launcher tiles, persistent per-app
  WebEngine profiles (log in once), external navigation + fullscreen allowed,
  Widevine via the slice-1 CDM wiring, TV user-agents where they help (YouTube
  leanback = touch-friendly + code-pairing login). Open questions for its design
  arc: keyboard for non-code logins (Qt VirtualKeyboard vs. pair-from-phone),
  audio-focus policy for background playback under the AA view, one-app-alive
  resource cap, librespot as a Spotify Connect complement. Scoped in
  `docs/archive/plans/2026-07-07-web-surface-strategy-design.md` (Decision 3);
  not yet promoted.

## From multi-dashboards execution (2026-07-06)
- **Persistent edit-mode latch across dashboard switches** — deselect-on-switch currently exits edit mode on every pill tap (correctness fix); a dedicated latch would allow browsing dashboards while editing. Prereq: the outgoing-selection clear (landed in 1762349) — the stale-latch bug it fixed is what deselect-on-switch papered over.
- **Per-widget size-bound tuning** — picker preset popup shows ~4 options for nearly every widget (none pin to a single preset); curating minCols/maxCols per widget would restore one-tap add for fixed-size widgets.
- **Overlay design doc: document that QQuickOverlay-parented modal Dialogs (config/picker/manage sheets) composite ABOVE all Shell z-bands** — or plan migrating modals into the overlay framework. Surfaced by the dashboards final review as a cross-branch semantic note.

## From companion support batch (2026-07-06)
- **Daemon-side proxy-route auto-teardown (vanished-phone net)** — Follow-up to the owner-teardown decision (2026-07-06): the API now clears the route when the owning session closes, but a phone that silently vanishes (AP association drop, no TCP FIN — the AA tcp_info gotcha) can hold a zombie session and leave the iptables/redsocks blackhole up. The Python system daemon already health-checks the route every 10s and marks DEGRADED/FAILED without tearing down; let it auto-disable after ~3 consecutive failures. Needs a small design pass: flap risk on transient congestion. Caller-agnostic — protects legacy AND API paths. App-side note: B2's liveness expiry (2026-07-14) already tears the route when the owning session goes silent >30 s; the daemon-side auto-disable remains as defense in depth below the app.
- **ApiServer failed-start retry hygiene** — if both listens fail, a retry re-news QTcpServer/QWebSocketServer without deleting the failed pair AND appends a duplicate publisher set (`createPublishers()` runs before the listen calls). Unreachable in production (main.cpp calls start() once); fix shape: clean up on the failure path or make the guard require stop() first. (Final-review finding, 2026-07-06.)
- **timedatectl handlers block the main thread** — `ClockSyncService`'s default exec (`waitForFinished(5000)`; both time and timezone steps route through it since the 2026-07-13 extraction); a hung timedatectl stalls UI for 5s. Convert to async QProcess — the exec seam makes this a one-spot change now. (Final-review finding, 2026-07-06.)

## From JS-runtime execution (2026-07-07)
- **Shim v1 hardening block (prodigy.js + WebWidgetHost)** — final-review triage bundle, none individually branch-blocking: (1) `subscribe()` validates topics with `topic in TOPIC`, which accepts prototype-chain keys (`'toString'`) and then feeds a function into the topics array — use `hasOwnProperty`; (2) after a renderer crash+reload the re-run bootstrap carries creation-time spans — call `pushContext()` in the `LoadSucceededStatus` branch; (3) requests issued during a disconnect window register a `pending` entry nothing settles (resolved `readyPromise` + silent `ws.send()` on CLOSED) — guard on `ws.readyState !== 1`; (4) design-accepted v1 simplifications worth revisiting together: readyPromise never re-pends, fire-and-forget subscribe (rejected topic invisible), unsubscribe is local-only (server streams until next rebuild), malformed `apiUrl` throw inside the reconnect setTimeout kills the loop permanently, connection-level `Error` frames with request_id 0 are silently dropped.
- **Widget-author known-limitations doc** — before third parties build against the shim: the v1 simplifications above, localStorage is off-the-record (default QML WebEngine profile — evaporates every restart; widgets must not assume persistence), widgets depend on `api.enabled: true`, and the D2 shared-origin consequences (shared localStorage across widgets).
- **Webwidget hygiene batch (next time these files are touched)** — resolver: "packages_ frozen after startup scan — synchronize before any dynamic-install feature" comment on WebWidgetContentResolver.hpp (unsynchronized QHash is a hard precondition for install-at-runtime work); WebWidgetHost: latch `widgetUrl` at first activation (transient empty effectiveConfig.url would destroy a live view, defeating D4); manifest: `entry.contains("..")` over-rejects legit names like `foo..bar.html` (resolver jail backstops real traversal); gen-proto-js.sh: comment the `-p proto` include-path rationale in-script + pin exact toolchain versions on next regen (currently major-only, regen not byte-reproducible).
- **Per-widget WebEngine profile / persistent storage decision** — if widget localStorage should survive restarts, switch the host to a named persistent profile (weigh against D2's shared-origin isolation notes and the v2 per-widget-origin hardening path).
- **Per-page filtered widget model (instantiation hygiene)** — Found root-causing the duplicate-WebEngineView bug (2026-07-07): each SwipeView page-Loader instantiates the FULL unfiltered WidgetGridModel repeater and filters only `visible`, so every widget gets a live (invisible) delegate copy per pre-rendered page. The isCurrentPage page-ownership gate (346c33d) makes foreign copies dormant (fixes the expensive WebEngine case), but the copies still exist as QML items. Proper fix: per-page filtered proxy model (or Loader-gated delegates). Also from the fix review: web-vs-native long-press divergence when another widget is selected (TapHandler switches selection @500ms; natives deselect @800ms via interceptor) — align if it bugs anyone on-device.
- **Cross-build speed — remaining half (ext4 + ccache + orphan guard)** — Target-scoping SHIPPED 2026-07-07 (`b86ded8`: fast app-only default + `--full`; deploys now ~4 min). Still open from the original entry: relocate `build-pi/` off drvfs to ext4 (docker volume or ~/builds bind mount) + ccache in the container. NEW (found during b86ded8 verification): `docker run` through `sg docker -c` doesn't die with the client — a killed/timed-out invocation leaves an orphan container mutating the bind-mounted `build-pi/` (one truncated the app binary to 0 bytes with a fresh mtime, fooling make). Add `--name` + pre-run `docker rm -f` guard (or `--init`) to cross-build.sh.

## From theme-upload design (2026-07-07)
- **All-routes web-config auth pass** — web-config binds `0.0.0.0:8080` with ZERO auth on every route, incl. `set_config` (can change any setting) and the new `install_theme` upload. Per-endpoint auth is theater while `set_config` is open; do ONE proper auth pass across the whole panel instead. (Decided during theme-upload Q1: match web-config's no-auth for the new endpoint, fix the real hole separately. See `docs/archive/plans/2026-07-07-theme-upload-design.md` §10.)
- **Browser-facing theme/wallpaper upload UI (themes.html)** — fast-follow to the companion-only `POST /api/theme/install` endpoint: a drag-drop wallpaper/theme uploader with preview + progress, hitting the same endpoint. Deferred from the theme-upload work item (Q2 = companion-only now). Cheap add-on once the endpoint ships.
- **`/tmp/oap-theme-upload/` janitor** — Flask deletes its wallpaper temp file in a `finally`, but a Flask crash mid-request leaks a stale temp file (reboot clears it). A periodic sweep or per-request stale-file cleanup is a tidiness nicety, not a v1 need.
- **Theme-upload temp-dir agreement test** (final-review Rec 1) — `/tmp/oap-theme-upload` is a literal in BOTH `web-config/server.py` (`UPLOAD_TMP_DIR`) and `IpcServer::handleInstallTheme`. Cross-reference comments now guard against a future typo, but no test covers the agreement (the C++ IPC test only exercises the color-only path). A C++ integration slot that round-trips a real wallpaper JPEG through the handler's hardcoded dir would catch a handler-literal typo that would otherwise silently reject every wallpaper upload in production. Also strengthen the T1 unit tests: `wallpaperPathIsDirectory` should point at a real *subdir* under the allowed dir (to hit the `!isFile()` branch, not the containment check), and `wallpaperHappy` should assert byte-content not just size.
- **Theme-upload 503-vs-500 status mapping** (final-review T3a) — Flask's socket-failure→503 substring list ("not running"/"not found"/"not accepting"/"timed out") misses `ipc_request`'s `"Empty response from app"` and generic-exception strings, so those app-availability failures map to 500 instead of 503. No silent success (still `installed:false` + real error), but a companion retry policy keyed on 503 would miss them. Cleaner fix: have `ipc_request` return a structured transport-vs-app status instead of English substring-sniffing.

## From media-player bench (2026-07-08)
- **Per-AA-channel volume sliders in settings** (Matthew, on-bench) — individual user volumes for the AA audio roles: media, calls/voice, notifications+guidance. `AudioStreamHandle.baseVolume` already exists and `applyDucking()` already computes duck/mute relative to it — the missing pieces are a config key per role, a settings UI (three sliders), and plumbing base volume into the AA/local/BT stream creation sites. Cheap after the 2026-07-09 focus-gain work; pairs naturally with the EQ settings page.
- **NavigationTurnLabel UTF-8 journal spam** — during AA navigation the journal fills with `libprotobuf ERROR … NavigationTurnLabel.label contains invalid UTF-8` (~1 line/sec, drowns everything). The phone sends non-UTF-8 bytes in a `string` field; proto-side fix is `bytes` (submodule change — note for open-android-auto), or sanitize/suppress at our parse site. Log hygiene, not a functional bug.

## From bench-fix final review (2026-07-09)
- **eventBus connects accumulate across AA reconnects** (pre-existing, found adjacent to this diff) — `mediaStatusHandler_`/`navHandler_` connects inside `onNewConnection()` (AndroidAutoOrchestrator.cpp ~510-522) are in neither disconnect list, so `aa.media.state` publications duplicate per reconnect. Add to the disconnect lists.
- **Sub-500ms tracks misclassified as unplayable** — the progress high-water check trips on legit short clips, and the 500ms progress-emit throttle means a seek-near-end within the first window can spuriously count a good file as unplayable. Exempt tracks with known duration <500ms and/or emit progress unthrottled on seek.
- **Pin baseVolume≠1.0 when per-channel sliders land** — applyDucking scales off baseVolume and the RT skip-condition still engages, but nothing tests baseVolume=0.5; add the test with the sliders feature.

## From Codex pre-push gate (2026-07-09)
- **PlaybackEngine stream flush on track change/seek/stop** (gate re-run P2, deferred) — `playFile()`/`stop()`/`seek()` reuse the AudioService stream without flushing its ring buffer, so queued PCM from the previous position could be briefly audible before new audio takes over. Unverified on hardware (bench next/prev/seek rows passed without audible-artifact notes), and a proper fix needs an RT-safe `IAudioService::flushStream()` designed against the PipeWire process callback — not a push-gate patch. Bench-listen first (manual next/seek while a track plays), then design the flush if audible. `AudioRingBuffer::reset()` exists but has no safe service-level path.

## From docs-structure cleanup (2026-07-09)
- **Miata GPIO/ignition/amp-control plugin** — the OAP-era hardware behavior (power latch, ignition sense, amp switching, dimmer servo, MCP23017 toggles) is a natural Prodigy plugin. Hardware reference preserved outside this repo at `personal/miata/miata-hardware-reference.md` (moved out during needs-review triage — car wiring doesn't belong in a public repo).
- **Author `docs/reference/external-api.md`** — External API v1 is a shipped public feature but its only documentation is the archived design doc (`docs/archive/plans/2026-07-06-external-api-v1-design.md`). Distill a user-facing reference: endpoints, pairing flow, proto contract, capability flags.
- **Re-triage the PARKED config-contract overhaul** — `docs/plans/2026-02-21-config-contract-overhaul-{design,plan}.md` (approved 2026-02-21, never executed). Decide: still wanted, needs rewrite against the current config surface, or ABANDONED.
- **Secret scan + checker hardening** (from 2026-07-09 Codex gate) — run a proper secret scanner (e.g. gitleaks) over the repo/history; extend `scripts/check-doc-links.py` to also validate backticked `.md` paths in live docs (the gate caught stale backticked pointers the link syntax check can't see).

## From ALPHA versioning landing (2026-07-09)
- **Packager hardening** (Codex gate P2, pre-existing) — `tools/package-prebuilt-release.sh` defaults to a timestamp version, never verifies `--version-tag` against an annotated tag on HEAD or the binary's embedded `OAP_VERSION`, and passes unvalidated `--version-tag`/`--target-name` path components into `STAGE_DIR` which feeds `rm -rf`. Validate with strict allowlists, reject path separators, cross-check tag ↔ binary before publishing.
- **settings-tree.md structural accuracy pass** (Task-5 review finding) — the "Identity" table documents rows that actually render elsewhere (Version and Left-Hand Drive live in `SystemSettings.qml`, not `InformationSettings.qml`), and the Software section has no heading of its own. Re-map the tables to the QML files that own each row.

## From media player stage 2 gate (2026-07-10)
- **UsbMediaWatcher filesystem-mount reconciliation** — the watcher now
  handles ObjectManager events, Drive `PropertiesChanged`, mount-race replies,
  and persisted fallback state. It still does not subscribe to Filesystem
  `MountPoints` property changes, so an external unmount can leave source and
  library state stale until another reconciliation. Track those changes by
  object path and reconcile the affected volume.
- **Polkit scoping: dedicated group + removable-only constraint** (gate P2 ×2, dismissed for single-user kiosk) — udisks actions are granted to the whole `bluetooth` group across all devices. Introduce a dedicated `openauto` group in the installer and constrain via UDisks action details (`drive.removable`); also fixes the semantically misleading group name.
- **Authoritative removable/canEject model role** (gate re-run P2) — the eject button shows via path heuristics (/media, /run/media, /mnt); `ejectVolume` now validates `isKnownMount` before disrupting anything, but the button itself should come from watcher state as a FolderModel role.
- **Nested-root precedence + record-level dedupe** (gate re-run P2) — refreshSources dedupes exact canonical duplicates only; a music_dir nested INSIDE another root (or a config dir that shadows a udisks mount — key-mismatch edge, self-heals on rescan) still double-scans. Define precedence and dedupe records by canonical path.
- **Async drive-property resolution in hot-plug path** (gate P2, bounded-3s ruling stands) — resolveDrive uses a 3s-bounded synchronous GetAll on InterfacesAdded; fully async QDBusPendingCallWatcher resolution would remove the worst-case UI stall entirely.

## From HFP/9876 bench (2026-07-13)
- **Call popup answer/reject dead during AA projection** (Samsung row) — popup renders over the AA surface but button presses leave ZERO journal trace: touch likely falls through to the AA layer. Second half of the same fix: the native call popup shouldn't show at all while an AA session owns call UI (AA renders its own). Reproduced with incoming call, S25 Ultra projecting.
- **BT advertising stops after device disconnect until app restart** — hit twice back-to-back (pairing Samsung after Pixel disconnect, then Moto after Samsung). New phone can't discover the head unit; `systemctl restart openauto-prodigy` restores it. Likely the BT discovery/advertisement isn't re-armed on disconnect.
- **App PipeWire connection doesn't survive daemon restart** — after `systemctl --user restart pipewire`, the settings audio-devices list shows empty (occasionally) and device selection doesn't persist; app restart fixes. Root: the app's registry connection dies with the daemon and never re-enumerates. Related ops rule: restart order is `bluetooth` → `pipewire wireplumber` → `openauto-prodigy.service`. Same family, observed at the 2026-07-15 BT EQ bench: a stream error (e.g. wireplumber-only restart) takes the BT EQ tap down and it stays down until app restart — teardown/fallback fire correctly, but nothing re-creates the tap streams.
- **WirePlumber/BlueZ RegisterProfile restart race** (ops/installer note) — restarting the audio stack can yield `spa.bluez5.native: RegisterProfile() failed: org.bluez.Error.NotPermitted`, leaving HFP silently dead (phone connects A2DP-only, calls stay on handset). Restarting `bluetooth` first clears it. Installer/service files should encode the ordering; consider detection (ag-object absent after connect) + auto-remediation.
- **BatteryWidget: no charging indicator** — `CompanionState.phoneCharging` arrives correctly (verified end-to-end); the canvas renders only outline/fill/%. Add a bolt glyph when charging.
- **Startup QDBus warnings** — `QDBusArgument: write from a read-only object` ×3 at app start, and `QDBusRawType<0x617b73767d>* must be registered` when BtAudio reads `MediaPlayer1.Track`. Log hygiene + possible latent marshalling bugs in the same `a{sv}` family as the fixed dead-slot issues.
- **redsocks localhost self-probe rejected by phone SOCKS ruleset** — "connection not allowed by ruleset (2)" for 127.0.0.1:12345 while real traffic relays fine. Probably the app's SOCKS server policy; confirm it's intended and silence the health-probe noise if so.
- **Bench tooling: adb** — canonical adb on MINIMEES is `E:\android\sdk\platform-tools\adb.exe` (three other adb installs exist; version-mismatched clients kill each other's servers — probe with the canonical one ONLY). Pixel 8 over USB (serial 39260DLJH000LX) worked for location toggles, force-stop, logcat. Consider wireless-debug pairing for benches where the phone isn't cabled.
- **Bounded-drain session teardown for backpressured terminal frames** (2026-07-13 gate finding, dismissed as theoretical) — `flush()` before close fully drains handshake-path terminal frames (first bytes on a fresh connection, empty send buffer), but a mid-session `closeWithError` to a peer with a backlogged-yet-under-cap send buffer can still lose the frame's tail when the deferred delete destroys the socket. Fix shape: keep the transport alive until Qt reports the drain (or a bounded timeout → abort). `sendRaw` is documented best-effort; promote only if a real client ever hits it.
- **Backward clock-step guard can never fire from live traffic** — `ClockSyncService` (semantics inherited from legacy `adjustClock`) requires 3 consecutive reports agreeing on the IDENTICAL millisecond target before stepping backward >5 min, but a real phone's reports advance between sends, so the agreements never accumulate and large backward corrections are effectively impossible without a restart. Probably tolerable (backward jumps >5 min are almost always phone-side nonsense) but the guard should compare against a drift-adjusted target, not a raw timestamp. (Found during the 2026-07-13 ClockSyncService extraction.)

## From PR #19 pre-merge gate (2026-07-14)
- **Patched libspa deb has no upgrade path** (gate finding, deferred) — both installers take the idempotent return on ANY installed `+prodigy` version, so a future release bundling a rebuilt deb (`+prodigy2`, or a rebuild against a newer pipewire base) never replaces the installed one. Tolerable while exactly one patched-deb revision exists: a base upgrade breaks the held package's dependency LOUDLY (documented cue to rebuild), and a candidate built for a newer base fails the pre-install simulation on an old base anyway. Fix shape when the first revision bump ships: single-candidate selection + `dpkg --compare-versions` installed-vs-candidate, unhold → install → re-hold only when the candidate is newer. Not patched at the gate: touching the hold/upgrade logic in both installers right before a release adds untested risk to a bench-validated path.

## From bench-findings batch spec review (2026-07-15)

- **AA assistant microphone transport is unimplemented** — Codex spec-review
  discovery (verified in-tree): the protocol handler emits
  `micCaptureRequested(bool)` (`AVInputChannelHandler`) but NO production code
  consumes it; `AndroidAutoOrchestrator` owns playback handles only, and the
  sole in-tree capture consumer is `BtAudioTap`. The assistant mic has never
  worked — the 2026-07-15 "input-device selection" wishlist premise ("blocks
  AA-assistant mic config") was necessary-but-not-sufficient. Fix shape:
  connect `micCaptureRequested` to an `AudioService`
  `openCaptureStreamWithOptions` capture stream (target = configured mic
  device, mic-gain applied) feeding the AVInput channel per the AA protocol.
  Protocol-critical: `Tier: main`, own promotion cycle; bench row = assistant
  end-to-end round-trip on the Pixel.

## From bench-findings batch final review (2026-07-15)

- **Master-volume flush retry is unbounded** — final-review minor, plan-faithful
  implementation (the design mandated re-arm-on-failure with no cap). On a
  persistently unwritable config (full/read-only SD card — plausible in a car),
  one volume change starts an eternal 2 s write-attempt + warning loop until
  shutdown. Fix shape when next touched: bounded retry count (e.g. give up
  after 5 with one final warning), and the design doc gains the cap rule.

## From BT A2DP EQ bench (2026-07-15)

- **Two-minute pairing window is invisible in the UI** — diagnosed at the bench
  (btmon: kernel sends `IO Capability Negative Reply — Pairing Not Allowed (0x18)`
  before the agent is ever consulted) and Codex-confirmed by design archaeology:
  `PairableTimeout=120` + startup `Pairable=false` is DELIBERATE security design
  (2026-02-27 bluetooth-cleanup design §"auto-toggles off after 120 seconds";
  commits 617f269/a38e5b5/dc725a1/8ad2e6a) — an app crash must not leave the HU
  indefinitely pairable. Only first-run mode (zero paired devices) renews the
  window, so the trap only bites when adding a SECOND phone. Fix shape (Codex):
  rename the ConnectionSettings control to "Allow New Pairings for 2 Minutes" as
  a momentary action with "pairing window open" feedback driven by the existing
  PropertiesChanged path (do NOT zero the timeout); hardening rider:
  `setupAdapter()` assigns `pairable_ = false` without emitting `pairableChanged`,
  so the QML switch can show stale-checked after a BlueZ restart
  (`BluetoothManager.cpp:435` area). NOTE: distinct from the existing
  "BT advertising stops after device disconnect" item — discoverability never
  times out and is a separate flag; that item stands on its own.

- **Send AVRCP Pause to the BT source on music focus loss** — Matthew
  preference from the bench: when another music source takes over, pausing the
  phone beats duck-to-silence-while-it-keeps-playing (battery + "why is my
  phone playing to nobody"). Today's duck behavior is correct per design;
  this is an upgrade, gated on AVRCP control via BlueZ MediaPlayer1.Pause().
- **Tap bring-up should sweep pre-existing live bluez_input nodes** — bench
  row 4: WirePlumber applies the retarget rule only at node creation, so an
  app (re)start during live BT streaming leaves the stream direct-to-sink
  (un-EQ'd, no HU volume) until the next transport cycle. Fresh
  nodes/reconnects are tap-routed from birth. Fix shape: on tap activation,
  enumerate existing `bluez_input.*` nodes and relink (or metadata-retarget)
  them into the tap.
- **AA system-sound channel full-ducks media for the whole channel-open
  window** — every AA touch click opens AudioChannel 5 (System) for ~5 s
  (gearhead holds it) and focus ducks media to silence the entire time; a
  50 ms click costs 5 s of dead music. Fix shape: System-channel focus should
  partial-duck (or mix-over) rather than full-duck, and/or use a fast-release
  duck curve. Phone-side workaround: disable touch sounds.

## From BT A2DP EQ pre-push gate (2026-07-14)

- **Epoch-quiesced ring transitions** (gate re-run P1, dismissed with reason) — after
  the gated-writer fix, a theoretical race window remains: a capture callback
  in flight past the `captureEnabled_` gate check (~µs) could race an
  activate-side `drain()` if transitions recurred within microseconds — but
  transition cadence is bounded by BlueZ D-Bus round-trips (ms+), and the worst
  case is ONE stale 512-byte chunk (~2.7 ms of audio). Also pre-existing and
  unchanged: `AudioRingBuffer::write()`'s overflow path advances the read index
  concurrently with a live reader on ALL audio streams (AA incl.) — a design
  trait, overflow-while-reading is rare. Proper fix = consumer-owned overrun
  handling + RT epoch acknowledgment before drains, verified under TSAN — an
  AudioRingBuffer redesign touching every audio path. **Promote if the bench
  ever shows transition artifacts** (stale-audio blips on BT pause/resume).
- **Legacy capture-callback replace race** (gate re-run P2, dismissed with
  reason) — `setCaptureCallback`'s atomic flag doesn't stop an RT invocation
  already past the check, so replacing/clearing races the in-flight call.
  Pre-existing pattern from the original single-slot code; ZERO in-tree
  callers of the legacy capture API exist (the options path installs
  immutable pre-connect callbacks and is unaffected). Fix rides with the
  first real legacy consumer — or delete the legacy capture virtuals
  entirely, which is cleaner.

## From EQ parity audit (2026-07-14)

Audit verdict: on-HU and YAML legs of the original outcome statement hold (the on-HU
UI exceeds "basic changes"); the **web-config advanced-EQ leg is entirely absent**.
Coverage/labeling quirks below were confirmed against the current tree. None of these
were fixed mid-audit (wishlist-then-promote); a web EQ editor or coverage fix is a
brainstorm → plan cycle on Matthew's go-ahead.

The web EQ editor remains parked; the persistence, System-stream labeling, and
BT A2DP routing findings were promoted and shipped 2026-07-15.

- **Web-config advanced EQ editor (the parity gap)** — the original outcome statement
  promises "web settings backend for advanced EQ setup and profile creation"; nothing
  exists at any layer: no `web-config` routes/templates, no `IpcServer` EQ commands,
  no External API EQ surface. `EqualizerService` already exposes everything an editor
  needs (per-stream gains/presets/bypass, user-preset save/delete/rename — rename is
  UI-less today, a web editor would surface it). Fix shape: IPC commands
  (`get_eq_state`, `set_eq_gain`, `set_eq_preset`, preset CRUD) + a web-config page
  with per-stream band sliders; decide whether the External API also gets a (frozen
  additive) EQ surface or web/IPC stays the only remote channel.
