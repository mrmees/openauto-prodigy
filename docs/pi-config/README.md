# Pi System Configuration Snapshot

**Date:** 2026-02-24
**Pi IP:** 192.168.1.152 (eth0), 10.0.0.1 (wlan0 AP)
**OS:** RPi OS Trixie (Debian 13)
**Commit:** dfc42a7 (main)

## Files

| File | Source on Pi | Purpose |
|------|-------------|---------|
| `hostapd.conf` | `/etc/hostapd/hostapd.conf` | WiFi AP (SSID: OpenAutoProdigy, 5GHz ch36, WPA2) |
| `10-openauto-ap.network` | `/etc/systemd/network/10-openauto-ap.network` | Static IP 10.0.0.1/24 on wlan0, built-in DHCP server |
| `bluetooth-service-override.conf` | `/etc/systemd/system/bluetooth.service.d/*.conf` | BlueZ `--compat -P *` flags |
| `labwc-rc.xml` | `~/.config/labwc/rc.xml` | Wayland compositor config (mouseEmulation, keybinds) |
| `cmdline.txt` | `/boot/firmware/cmdline.txt` | Kernel boot params (regulatory domain) |
| `config.txt` | `/boot/firmware/config.txt` | Pi firmware config (GPU mem, display, etc.) |
| `udev-rules.txt` | `/etc/udev/rules.d/*.rules` | Custom udev rules (USB BT dongle disable, etc.) |
| `restart.sh` | `~/openauto-prodigy/restart.sh` | App kill/restart script |
| `openauto-prodigy.desktop` | `~/Desktop/openauto-prodigy.desktop` | Desktop shortcut |

## Enabled Services

- `bluetooth.service` (enabled)
- `hostapd.service` (enabled)
- `dnsmasq.service` (enabled — but DHCP is via networkd, not dnsmasq)

## Notes

- No `~/.openauto/openauto.yaml` exists — app runs on defaults
- Theme dir `~/.openauto/themes/default/` exists but is empty
- `wlan0` IP is persistent via networkd (was previously a known issue)
- `/var/run/sdp` permissions (`chmod 777`) don't survive reboot — needs udev rule
