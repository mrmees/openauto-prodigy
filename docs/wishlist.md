# Wishlist

Ideas captured here. Promote to `roadmap-current.md` when ready to commit.

## Bugs / Infrastructure

- **SDP `/var/run/sdp` permissions race** — BlueZ `ExecStartPost` wait loop (5x 0.5s) loses the race; socket stays `root:root` instead of `root:bluetooth`. App-side retry (2s x 30) helps but doesn't fix root cause. Need `inotifywait` on socket creation or a longer/smarter polling loop in the systemd override. Reproduces on every fresh boot.

- **Web config panel broken** — Pre-existing issue, not a phase 1 regression. The Flask web panel at `web-config/` doesn't load. Blocks testing of runtime logging controls, settings persistence, and any future web-based configuration. Needs investigation — could be a route issue, missing dependency, or socket communication failure.

- **Boot/reboot startup reliability** — After a reboot, `graphical.target` was slow to activate (stuck on `systemd-networkd-wait-online.service` timeout). Prodigy service depends on `graphical.target` so it sat in `inactive (dead)` until the timeout passed. Need to verify clean boot sequence, measure time-to-app, and possibly mask the networkd-wait service (NetworkManager is the actual manager). Phase 4 territory.

## Candidate Ideas

- **Per-connection WiFi password rotation** — Generate a fresh random WPA password each time a phone connects via BT RFCOMM, update hostapd (`hostapd_cli set wpa_passphrase` + reload), then send the new password to the phone. Eliminates any static credential. Requires coordinating hostapd reload timing with the BT handshake.

- **Three-tier community architecture** — Long-term goal is a clean separation into three public layers:
  1. **Protocol definitions** (`open-android-auto`, exists) — language-neutral `.proto` files. Anyone can generate bindings for any language.
  2. **Protocol implementations** (new repo, TBD) — community-contributed implementations in any language. Transport, framing, encryption, channel handlers. Our C++ one lives in `libs/open-androidauto/` today (needs de-Qt-ing first), but the repo would welcome Rust, Python, Go, Qt, COBOL — whatever someone wants to PR.
  3. **Application** (`openauto-prodigy`) — Qt/QML head unit that consumes the above two as submodules.

  **What needs to happen:**
  - De-Qt the protocol library: replace QObject signals/slots with `std::function` callbacks or Boost.Signals2, QByteArray/QString with STL, QTcpSocket with Boost.ASIO (already used elsewhere), QTimer with ASIO timers.
  - Split into its own public repo once Qt-free.
  - Openauto-prodigy consumes both repos as submodules, adds the Qt/QML application layer on top.

  **Open items in `open-android-auto`:**
  - `NavigationTurnEventMessage.proto` has unused imports (`ManeuverTypeEnum.proto`, `TurnSideEnum.proto`) — fields 2 and 3 should use the enum types instead of raw `int32`.
