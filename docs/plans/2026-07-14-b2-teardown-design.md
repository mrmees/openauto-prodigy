# B2 Teardown — Legacy Companion Stack Retirement — Design

Status: ACTIVE
Grounded on: `6310dcc` (dev == origin/dev, 2026-07-14, post-PR #19 / ALPHA-26-07-14-01)
Design doc lineage: executes and supersedes the §B2 inventory in
`docs/plans/2026-07-11-hfp-mic-9876-retirement-design.md` (that section's line
references were verified against today's tree and updated here; where they
disagree, THIS document wins). Gate that unblocked it: §A3.6 cutover validation
passed at the 2026-07-13 bench (every payload on API v1, wire-proven zero
packets on 9876 across a 90 s sniff + app relaunch).

## 1. Why

`CompanionListenerService` (port 9876, JSON/HMAC) is fully replaced by API v1
and has been running config-disabled since the cutover bench. Every legacy
capability has a bench-validated replacement:

| Legacy capability | Replacement | Validated |
|---|---|---|
| GPS / battery / charging reports | `GpsReport`/`BatteryReport` → `ApiInboundState` (QML `CompanionState`) | 2026-07-13 bench §7 |
| Internet/SOCKS5 proxy status + route | `ConnectivityReport` + owner-tracked route plumbing | bench §7 (redsocks relaying; force-stop clear) |
| Time / timezone sync | `TimeReport` → `ClockSyncService` | live-validated ×2 incl. NTP-enabled in-car state |
| Theme/wallpaper transfer | HTTP `POST /api/theme/install` | shipped 2026-07-07 |
| Pairing PIN + QR | ApiServer PIN window + `pairingQrDataUri` (contract rev 2) | end-to-end phone scan benched |
| `vehicle_id` | deliberately dropped — API v1 identity = pairing tokens; zero consumers | n/a |

Keeping the corpse costs real things: a dead 720-line test file, dead-slot D-Bus
patterns, three wishlisted RNG-hygiene sites, a duplicated theme-key conversion,
and a settings page that had to grow a regression test just to assert the legacy
controls stay dead.

## 2. Scope

Four pieces, one branch, one Codex gate:

1. **Teardown** — delete the service and every reference (§3).
2. **Survivor rewiring** — polkit rule rename, installer config blocks (§4).
3. **Riders** (Matthew-approved 2026-07-14, both in files the teardown already
   edits): udisks polkit rule for install-prebuilt.sh; `gps_stale` in IPC
   `companion_status` (§4).
4. **Liveness expiry** — the one new behavior; the deferred PR #19 gate finding
   settles here as designed in §5 (Matthew-approved 2026-07-14).

Decisions recorded (Matthew, 2026-07-14):

- **Liveness expiry: IN scope** (was "settle at B2" from the PR #19 gate).
- **Custom-AP prompt: DROPPED** from install-prebuilt.sh; the 10.0.0.1/24
  invariant (admission + QR host) is now deliberate. Closes the 2026-07-13
  wishlist item with this decision recorded. Real custom-subnet support can be
  designed later if anyone ever asks.

## 3. Teardown inventory (verified against `6310dcc`, 2026-07-14)

Line numbers are from the grounding commit; re-verify at execution time
(verify-before-coding rule — the §B2 2026-07-11 numbers had already drifted).

- **Files deleted:** `src/core/services/CompanionListenerService.{hpp,cpp}`
  (705-line .cpp), `tests/test_companion_listener.cpp` (720 lines), their CMake
  entries (`src/CMakeLists.txt:62`, tests CMake). Dies inside: legacy
  `adjustClock` (.cpp:502), private QR renderer (.cpp:159), camelCase→hyphen
  lambda (.cpp:615), all three RNG-hygiene sites (.cpp:111/:223/:281).
- **main.cpp:** include (:43), wiring block (:404–447 — `companion.enabled`/
  `companion.port` reads, `~/.openauto/companion.key` load, setWifiSsid /
  setThemeService / setDisplaySize, `start()`, HostContext hookup),
  `ipcServer->setCompanionListenerService` (:951), `CompanionService` context
  property (:1149), stale comments (:1214–1220).
- **IHostContext/HostContext:** forward decl (`IHostContext.hpp:16`), pure
  virtual (:37), `HostContext.hpp` setter/getter/member (:17/:33/:52); mocks in
  `tests/test_plugin_manager.cpp:43` + `tests/test_plugin_model.cpp:42`. No
  plugin ever calls the accessor (verified — only main.cpp/HostContext/IpcServer
  touch it).
- **IpcServer:** `setCompanionListenerService` + `companion_` member; the
  `companion_status` legacy fallback body (.cpp:423–440) including the
  `vehicle_id` merge on the inbound-preferred branch. Post-teardown behavior:
  inbound state set → current API body (`"source":"api"`); no inbound (API
  disabled) → the existing "not available" error JSON. `vehicle_id` disappears
  from the JSON (zero consumers, verified). Rider lands here: body gains
  `gps_stale` (mirrors `CompanionState.gpsStale`; wishlist 2026-07-13).
- **`companion.*` namespace:** main.cpp reads gone; default-config YAML blocks
  removed from `install.sh` (~:1421–1423) and `install-prebuilt.sh` (~:302–304).
  Never present in the config schema (verified) — no schema change. Existing
  on-Pi configs keep a stale `companion:` block harmlessly
  (retained-but-unread; `identity.sw_version` precedent). Legacy user state
  `~/.openauto/companion.key` + `~/.openauto/vehicle.id` stays on disk, noted
  as orphaned in the docs sweep — never silently delete user state.
- **Comment sweep (same commits as the code they annotate):** `QrPng.hpp:8–9`,
  `ApiInboundState.hpp:9`/`.cpp:83`, `ApiRequestHandlers.cpp:142`/`.hpp:97`,
  `ClockSyncService.hpp:11–12` ("retired at B2" → past tense),
  `WeatherWidget.qml:29`, `ApiSettings.qml:8`,
  `test_settings_menu_structure.cpp:263–264` (assertion stays — the toggle must
  remain absent — comment updates to "namespace retired").
- **Docs sweep:** `architecture.md:18` (drop from IHostContext service list),
  `plugin-api.md:298` (section removal), `settings-tree.md:149–150`
  (companion.* language → retired + orphaned-keys note),
  `roadmap-current.md` (companion-migration gate → Done), `INDEX.md` if any
  description references the service.
- **Wishlist closeouts:** RNG hygiene (:74) — dies with the file. Theme-key
  dedup (:91) — resolved by deletion: `ThemeInstallRequest`'s copy becomes the
  only copy and is already on the shared `importCompanionTheme` path; no code
  movement. `:53` theme-upload item's "Blocks legacy-9876 retirement" note —
  retirement done. `:73` proxy-route auto-teardown — item STAYS (daemon-side
  watchdog is still separate work) but its legacy `proxyRouteApplied_`
  interaction caveat is removed, and §5's expiry now covers the app-side
  route-teardown half. Custom-AP item (:143) — closed with the §2 decision.

## 4. Survivors & riders

- **Polkit rule (survives, renamed):** `config/companion-polkit.rules` is
  already the fixed 3-action timedate1 grant (`set-time`/`set-timezone`/
  `set-ntp`, `isInGroup("bluetooth")`) that `ClockSyncService` depends on —
  the 2026-07-13 clock-sync fixes landed in this file. Rename to
  `config/clock-sync-polkit.rules`, content unchanged. **Installed filename
  stays `/etc/polkit-1/rules.d/50-openauto-time.rules`** so upgrades overwrite
  in place. Both installers update their copy lines; install.sh's "Companion
  polkit rule installed" message becomes "Clock-sync polkit rule installed".
- **Custom-AP prompt removal:** install-prebuilt.sh drops the custom static-IP
  prompt; AP address is fixed at 10.0.0.1/24, matching `ApiServer::inApSubnet`
  admission and the QR `host=` payload.
- **Rider — udisks polkit rule:** install-prebuilt.sh gains install.sh's
  udisks polkit block (wishlist 2026-07-13: prebuilt installs currently hit
  password prompts on USB mount/eject). Closes that wishlist item.
- **Rider — `gps_stale` in IPC:** see IpcServer entry in §3.

## 5. Liveness expiry (new behavior)

**Problem (PR #19 gate, deferred to B2):** `reportingSessions_` membership —
which drives `CompanionState.connected` — persists until transport close. A
phone that dies without a TCP FIN (wifi drop, walked away) leaves
`connected: true` + last battery state visible until TCP gives up (~15 min,
and only if a publisher writes). GPS already has independent 30 s staleness.

**Contract basis:** the v1.1 companion handoff instructed periodic re-reports
at ~1 Hz precisely because "the head unit does NOT yet detect silently-vanished
phones … until a daemon-side watchdog ships." The companion's half already
ships; this is our half.

**Design (all in `ApiRequestHandlers`, where the ownership model lives):**

- Track last-accepted-report time per session: one
  `QHash<ApiSession*, qint64>` updated in `handleReport` on every accepted
  report (any type — GPS, battery, connectivity active-or-not, time).
- Coarse sweep `QTimer`, 5 s tick, armed only while `reportingSessions_` is
  non-empty (same idle-cost philosophy as `ApiInboundState::staleTicker_`).
- **Threshold: 30 s** — 30 missed beats at the ~1 Hz contract cadence,
  symmetric with the GPS staleness window. Constant, not a config key (YAGNI);
  settable via test seam.
- Expiry = the `sessionClosed()` clear, extracted into a shared
  `clearReportingState(session)`: remove from `reportingSessions_` + drop the
  last-report entry, clear per-type owned state (`clearGps`, `clearBattery`,
  connectivity route teardown — the route is a blackhole once the phone is
  gone), then `recomputeOwnerPresence()`.
- **Scope guard:** expiry touches ONLY the reporting role. Registered actions,
  notification ownership, and the socket itself are untouched — a web-widget
  client that never sends reports must be completely unaffected, and a
  reporting session that expires keeps its non-reporting roles.
- **Self-healing by construction:** expiry only removes set membership. A
  wedged-but-alive session's next accepted report re-registers presence and
  ownership exactly like a first report; a true reconnect re-sends
  `ConnectivityReport` promptly per contract.

**Widget effect:** battery widget already renders nothing for `batteryLevel <
0` (PR #19 fix); GPS surfaces already handle invalid/stale. `connected` flips
within ≤35 s of the last report (30 s threshold + 5 s tick granularity).

**Tests (extend `tests/test_api_request_handlers.cpp`):** settable threshold +
directly-invokable sweep (no wall-clock sleeps). Cases: expiry flips
`connected`, clears battery/GPS, tears the proxy route, emits the change
signals; non-reporting roles survive expiry; revival on next report;
two-session independence (one expires, the other keeps presence); sweep timer
arms/disarms with set occupancy; boundary at the threshold.

## 6. Sequencing

Single branch (`dev`), teardown-first so later tasks work in the clean tree:

1. Core deletion — service, test, CMake, main.cpp, IHostContext/HostContext,
   mocks, IpcServer — suite green + **explicit app-target build** after this
   task alone (ctest never compiles main.cpp, and main.cpp carries most edits).
2. Survivor rewiring — polkit rename, installer `companion.*` blocks,
   custom-AP prompt drop, both riders.
3. Liveness expiry — TDD per §5.
4. Docs/comments/wishlist sweep — riding in the same commits as the changes
   they describe wherever possible; residue in a final docs commit.

One Codex gate (`bash scripts/codex-review.sh`) over the full range at the end;
adjudicated per AGENTS.md.

## 7. Verification

- `ctest --output-on-failure` green (suite minus the deleted test file) AND
  `cmake --build . --target openauto-prodigy` in `~/builds/openauto-prodigy`.
- `git grep -I -e 9876 -e CompanionListener` → hits only in
  `docs/archive/`, `docs/session-handoffs.md`, and plan-doc history.
- `bash -n install.sh install-prebuilt.sh`; `scripts/check-doc-links.py` → OK.
- Codex gate pass → cross-build → Pi deploy → journal check (pre-flight 4/4,
  no new warnings). On-Pi: Companion settings page sane, companion widgets
  live over API v1, port 9876 still refusing connections.
- Liveness live check at the next bench visit (airplane-mode the phone, watch
  `connected` drop within ~35 s) — NOT a merge gate; unit tests cover the
  logic and bench visits are opportunity-driven.
