# Current Roadmap

Governance: capture new ideas in `docs/wishlist.md`; only promoted items should appear in this roadmap.

## Done (recent)

- Wireless BT AA flow — RFCOMM server + SDP registration + TCP listener. Phone discovers, pairs, and connects without manual scripts. COMPLETE.
- Touch device auto-discovery — INPUT_PROP_DIRECT scan, axis ranges read from device at open time. COMPLETE.
- Persistent network configuration — hostapd + systemd-networkd (built-in DHCP, no dnsmasq). COMPLETE.
- Settings UI buildout — scrollable ListView, section headers, plugin settings integration. COMPLETE.
- Background system service hardening — bt.close() hang fix, layered shutdown timeouts. COMPLETE.
- Proto repo migration — standalone [open-android-auto](https://github.com/mrmees/open-android-auto) community repo. COMPLETE.
- Video ACK delta fix (PR #5) — prevents RxVid overflow on phone. COMPLETE.
- General Bluetooth cleanup — BluetoothManager with D-Bus Adapter1/Agent1, PairedDevicesModel, auto-connect retry, PairingDialog overlay, HSP/HFP profile registration, polkit rules. COMPLETE.
- Obsolete aasdk reference removal — cleaned source and active docs of stale aasdk references. COMPLETE.
- Install script overhaul — interactive installer validated on fresh Trixie: hardware detection (touch, WiFi, audio), BlueZ --compat, rfkill unblock, country code auto-detection, labwc multi-touch config, systemd services, launch option. COMPLETE.
- Prebuilt distribution workflow — `install-prebuilt.sh` + `tools/package-prebuilt-release.sh` for shipping Pi-ready release tarballs without source builds, with release naming conventions and installer mode selection (`source` vs GitHub `prebuilt`). COMPLETE.

## Now

- AA connection validation on fresh install.
  - Rationale: fresh Trixie install completes, app launches, BT pairing works, but AA projection has not been validated end-to-end on the new drive yet. SDP registration hit "Permission denied" (group membership needs re-login/reboot).
  - Outcome: full AA session (BT discovery → WiFi → TCP → video projection) working on a clean install after reboot.

- General HFP call audio handling.
  - Rationale: typical head units maintain the HFP AG profile across both Android Auto and the base hardware — e.g., if AA crashes, call audio is not lost.
  - Outcome: phone calls work with audio through the head unit speakers/mic across a closed AA stream.

- Audio equalizer.
  - Rationale: users expect the ability to swap between equalizer presets and define custom EQ profiles for their vehicle and speaker setup.
  - Outcome: audio equalizer plugin with YAML settings file, on-head-unit component for basic changes / profile swapping, web settings backend for advanced EQ setup and profile creation.

## Later

- Dynamic AA video reconfiguration for sidebar changes.
  - Rationale: toggling the sidebar on/off or changing its position (left/right/top/bottom) requires recalculating the AA video content area (margin_width/margin_height in VideoConfig). Currently this isn't handled dynamically — the video config is locked at session start.
  - Outcome: sidebar show/hide and position changes trigger AA video renegotiation or margin recalculation within the active session, so the phone renders correctly for the new layout without reconnecting.
- Reduce unnecessary logging / enable debug logging options.
  - Rationale: BtManager spams "Found N paired device(s)" on every D-Bus signal, NavStrip QML color warnings on startup. Production should be quiet by default with a debug flag or config option to enable verbose output.
  - Outcome: clean default log output, `--verbose` / config toggle for debug-level logging.
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
