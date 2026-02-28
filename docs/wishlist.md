# Wishlist

Ideas captured here. Promote to `roadmap-current.md` when ready to commit.

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
