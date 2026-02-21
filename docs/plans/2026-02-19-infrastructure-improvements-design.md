# Infrastructure Improvements Design
> **Status:** COMPLETED

## Goal

Make OpenAuto Prodigy reliable across diverse hardware by auto-detecting touch input, persisting network AP configuration, and enabling boot-to-app via systemd.

## 1. Touch Auto-Discovery

**Problem:** Touch device hardcoded to `/dev/input/event4`. Any USB reorder or different hardware breaks touch input.

**Solution:** Scan `/dev/input/event*` at startup for devices with `INPUT_PROP_DIRECT` capability (identifies touchscreens). Use the first match. Allow config override.

**New file:** `src/core/InputDeviceScanner.cpp/.hpp`
- `findTouchDevice()` — iterates event0–event31, checks `EVIOCGPROP` for `INPUT_PROP_DIRECT`, returns first match path
- Also reads device name via `EVIOCGNAME` for logging

**Modified file:** `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Replace hardcoded path with: read `touch.device` from config → if empty, call `findTouchDevice()`
- Read `display.width` / `display.height` from config (default 1024x600)
- AA resolution stays hardcoded to match video resolution enum (1280x720)

**Config:**
```yaml
touch:
  device: ""  # empty = auto-detect
display:
  width: 1024
  height: 600
```

## 2. Network AP Configuration

**Problem:** `wlan0` static IP doesn't survive reboot. Network setup is fully manual per `wireless-setup.md`. Users with multiple wireless interfaces have no way to choose.

**Solution:** Install script detects wireless interfaces, lets user pick, writes systemd-networkd + hostapd configs.

**Key insight:** systemd-networkd has a built-in DHCP server, eliminating the dnsmasq dependency.

**Install script changes:**
- Detect wireless interfaces via `/sys/class/net/*/wireless`
- Let user pick interface if multiple found
- Write `/etc/systemd/network/10-openauto-ap.network` (static IP + DHCP server)
- Write `/etc/hostapd/hostapd.conf` from user inputs (interface, SSID, password, country code)
- Set `DAEMON_CONF` in `/etc/default/hostapd`
- Enable hostapd + systemd-networkd
- Store `wifi.interface` in config.yaml

**App-side:** `BluetoothDiscoveryService` reads `wifi.interface` from config to look up the correct IP to advertise. Falls back to first wireless interface if not set.

## 3. Boot-to-App

**Problem:** App must be launched manually via SSH every boot.

**Solution:** Clean up existing systemd service in install script. Remove unnecessary `Wants=` dependencies. Add `WorkingDirectory`. Make auto-start opt-in during install.

**Service file:**
```ini
[Unit]
Description=OpenAuto Prodigy
After=graphical.target

[Service]
Type=simple
User=$USER
Environment=XDG_RUNTIME_DIR=/run/user/$UID
Environment=WAYLAND_DISPLAY=wayland-0
Environment=QT_QPA_PLATFORM=wayland
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/build/src/openauto-prodigy
Restart=on-failure
RestartSec=3

[Install]
WantedBy=graphical.target
```

Install script asks "Enable auto-start on boot?" — only enables if yes.
