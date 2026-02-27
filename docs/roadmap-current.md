# Current Roadmap

Governance: capture new ideas in `docs/wishlist.md`; only promoted items should appear in this roadmap.

## Done (recent)

- Touch device auto-discovery — already implemented via INPUT_PROP_DIRECT scan. Removed hardcoded evdev axis ranges; now read from device at open time. COMPLETE.
- Persistent network configuration — installer configures hostapd + systemd-networkd (built-in DHCP, no dnsmasq) for boot persistence. System service now installed and enabled by install.sh. COMPLETE.
- Settings UI buildout — scrollable ListView, section headers, plugin settings integration, subtext removal, larger touch targets. Settings tree spec at `docs/settings-tree.md`. COMPLETE.
- Settings implementation — categories rendered in UI, SettingsListItem control, PluginModel settingsQml role. COMPLETE.
- Background system service hardening — bt.close() hang fix, layered shutdown timeouts (10s overall, 5s proxy, 3s IPC). COMPLETE.
- Proto repo migration — standalone [open-android-auto](https://github.com/mrmees/open-android-auto) community repo. COMPLETE.
- Video ACK delta fix (PR #5) — prevents RxVid overflow on phone. COMPLETE.

## Now

- Integrate wireless BT AA flow into the main application.
  - Rationale: wireless AA works via standalone test scripts (sdp_clean, HFP AG, hostapd, TCP handoff). Needs to be wired into the app's connection manager. Needs investigation to confirm whether this is already present.
  - Outcome: phone pairs and connects wirelessly through the app UI without manual script execution.

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
