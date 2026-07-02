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

- **FM radio (RTL-SDR + RDS)** — HUDIY-parity feature, explicitly deferred as a nice-to-have (Matthew, 2026-07-02). RTL-SDR dongle for tuning, RDS decode for station/track text, integrated as a media source.

- **Companion notifications on head unit** — The companion app ([openauto-companion](https://github.com/mrmees/openauto-companion)) already does GPS/time/battery/internet sharing + theme transfer. Remaining HUDIY-parity gap: displaying phone notifications on the head unit. Depends on the head-unit notification service (extensibility plan Priority 3).

- **Key-event navigation map** — HUDIY-style keyboard/button bindings (focus movement, back, media keys, projection focus toggle) for steering-wheel buttons via GPIO/keyboard HID. Prodigy is touch-first today.

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
