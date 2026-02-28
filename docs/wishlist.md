# Wishlist

Ideas captured here. Promote to `roadmap-current.md` when ready to commit.

## Candidate Ideas

- **Per-connection WiFi password rotation** — Generate a fresh random WPA password each time a phone connects via BT RFCOMM, update hostapd (`hostapd_cli set wpa_passphrase` + reload), then send the new password to the phone. Eliminates any static credential. Requires coordinating hostapd reload timing with the BT handshake.

- **De-Qt the protocol library (`libs/open-androidauto/`) and split into its own repo** — Replace Qt dependencies (QObject signals/slots, QByteArray, QTcpSocket, QTimer) with standard C++ / Boost equivalents. Makes the library usable by any C++ project, not just Qt apps. Pair with the `open-android-auto` proto repo as a community AA protocol stack. Signals/slots → `std::function` callbacks or Boost.Signals2; networking → Boost.ASIO (already used elsewhere); containers → STL.
