# Current Roadmap

Governance: capture new ideas in `docs/wishlist.md`; only promoted items should appear in this roadmap.

## Done (recent)

- Wireless BT AA flow — already fully integrated. App starts RFCOMM server + SDP registration + TCP listener automatically. Phone discovers, pairs, and connects without manual scripts. COMPLETE.
- Touch device auto-discovery — already implemented via INPUT_PROP_DIRECT scan. Removed hardcoded evdev axis ranges; now read from device at open time. COMPLETE.
- Persistent network configuration — installer configures hostapd + systemd-networkd (built-in DHCP, no dnsmasq) for boot persistence. System service now installed and enabled by install.sh. COMPLETE.
- Settings UI buildout — scrollable ListView, section headers, plugin settings integration, subtext removal, larger touch targets. Settings tree spec at `docs/settings-tree.md`. COMPLETE.
- Settings implementation — categories rendered in UI, SettingsListItem control, PluginModel settingsQml role. COMPLETE.
- Background system service hardening — bt.close() hang fix, layered shutdown timeouts (10s overall, 5s proxy, 3s IPC). COMPLETE.
- Proto repo migration — standalone [open-android-auto](https://github.com/mrmees/open-android-auto) community repo. COMPLETE.
- Video ACK delta fix (PR #5) — prevents RxVid overflow on phone. COMPLETE.

## Now

- General Bluetooth cleanup.
  - Rationale: before adding advanced BT features like HFP call audio, the foundational BT stack needs to be solid — proper profile advertising, user-friendly pairing, and reliable auto-reconnect.
  - Outcome:
    - All available BT profiles on the Pi adapter are properly advertised (A2DP sink, AVRCP, HFP AG, etc.).
    - Users can initiate pairing from the on-screen Connection settings page (no SSH/terminal needed).
    - App auto-connects to the paired phone on startup.

- General HFP call audio handling.
  - Rationale: typical head units maintain the HFP AG profile across both Android Auto and the base hardware — e.g., if AA crashes, call audio is not lost.
  - Outcome: phone calls work with audio through the head unit speakers/mic across a closed AA stream.

- Audio equalizer.
  - Rationale: users expect the ability to swap between equalizer presets and define custom EQ profiles for their vehicle and speaker setup.
  - Outcome: audio equalizer plugin with YAML settings file, on-head-unit component for basic changes / profile swapping, web settings backend for advanced EQ setup and profile creation.

## Later

- Plugin system expansion (OBD-II, backup camera, GPIO control).
- Theme engine and user-facing theme selection.
- CI automation for builds and tests.
- Multi-display / resolution support beyond 1024x600.
- Community contribution workflow (issue templates, PR guidelines).

## Deferred

- USB Android Auto support — explicitly out of scope.
- CarPlay or non-AA projection protocols.
- Hardware support beyond Pi 4.
- Cloud services, accounts, or telemetry.
