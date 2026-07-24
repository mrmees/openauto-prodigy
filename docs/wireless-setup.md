# Wireless Android Auto Setup

OpenAuto Prodigy uses wireless-only Android Auto. The phone discovers the Pi via Bluetooth, receives WiFi credentials, connects to the Pi's WiFi AP, then starts the AA protocol over TCP.

## Prerequisites

- Raspberry Pi 4 with built-in WiFi + Bluetooth
- Phone with Android Auto wireless support (Android 11+ recommended)

## Automated Setup

Both the source-build and prebuilt installation paths handle WiFi AP
configuration through the same packaged hardware contract:

- Detects available wireless interfaces
- Lets you choose which interface to use for the AP
- Configures systemd-networkd for static IP + DHCP server
- Writes hostapd.conf with your SSID/password
- Reads the selected adapter's live `iw` channel capabilities
- Prefers usable 5GHz (channel 36 when available), enables 802.11ac only when
  the selected radio band advertises VHT, and falls back to a usable 2.4GHz
  channel otherwise

Run `bash install.sh` and select either installation path. Both validate and
apply a two-letter country code before probing the selected radio, then complete
channel selection and configuration rendering before changing project-managed
network files. If the adapter has no
usable AP channel (entries marked `disabled` or `no IR` do not qualify), the
installer stops with the existing network files unchanged. Both paths also
install the same BlueZ SDP compatibility drop-in. The remaining sections are
for manual configuration or troubleshooting.

When an AP interface is selected, both installers add an optional
`Wants=hostapd.service` relationship from the application and configure
bounded `Restart=on-failure` recovery for hostapd. An AP failure therefore
restarts hostapd without stopping the shell, and restarting the shell does not
restart the AP. An install with no detected AP interface removes these
project-owned drop-ins and leaves the application independent of hostapd.

## 1. WiFi Access Point (hostapd + systemd-networkd)

### Install packages

```bash
sudo apt install hostapd
```

### Configure hostapd

Create/edit `/etc/hostapd/hostapd.conf`:

```ini
interface=wlan0
driver=nl80211
ssid=OpenAutoProdigy

# 5GHz is preferred on the Pi's combo radio to reduce WiFi/Bluetooth
# coexistence interference. Both installers detect adapter capabilities;
# for a manual 2.4GHz setup, use hw_mode=g and a suitable channel such as 6.
hw_mode=a
channel=36
ieee80211n=1
ieee80211ac=1
wmm_enabled=1

# 5GHz requires a country code or hostapd will refuse to start.
# Channel 36 is non-DFS in most regions, so no radar detection needed.
country_code=US
ieee80211d=1

macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=prodigy1234
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
```

Tell hostapd to use this config — edit `/etc/default/hostapd`:

```
DAEMON_CONF="/etc/hostapd/hostapd.conf"
```

### Configure static IP

The install script creates `/etc/systemd/network/10-openauto-ap.network`:

```ini
[Match]
Name=wlan0

[Network]
Address=10.0.0.1/24
DHCPServer=yes
ConfigureWithoutCarrier=yes

[DHCPServer]
PoolOffset=10
PoolSize=40
EmitDNS=no
```

The built-in systemd-networkd DHCP server supplies addresses to phones. Enable
networkd:

```bash
sudo systemctl enable systemd-networkd
sudo systemctl start systemd-networkd
```

For manual setup without the install script, create this file and adjust the interface name and IP.

### Enable and start

The automated installers copy the application and hostapd drop-ins from
`config/systemd/openauto-prodigy-hostapd.conf` and
`config/systemd/hostapd-openauto.conf`. For a manual setup, install those
fragments as
`/etc/systemd/system/openauto-prodigy.service.d/hostapd.conf` and
`/etc/systemd/system/hostapd.service.d/openauto.conf`, then reload systemd.
The relationship is intentionally one-way and optional: do not add `BindsTo=`
or a reverse `PartOf=` relationship between these services.

```bash
sudo systemctl unmask hostapd
sudo systemctl daemon-reload
sudo systemctl enable hostapd systemd-networkd
sudo systemctl start hostapd systemd-networkd
```

### Multiple wireless interfaces

If your Pi has both built-in WiFi and a USB wireless adapter, the install script
detects all wireless interfaces and lets you choose which one runs the AP.
The selected interface is stored in `~/.openauto/config.yaml` under
`connection.wifi_ap.interface`. The app reads this to know which interface's IP
to advertise during Bluetooth discovery.

## 2. Bluetooth

The Pi's Bluetooth must be powered. OpenAuto Prodigy handles the BT RFCOMM
server and SDP registration, but BlueZ must run in compatibility mode so its
legacy SDP socket is available. Create
`/etc/systemd/system/bluetooth.service.d/override.conf`:

Both automated install paths copy this fragment from
`config/systemd/bluetooth-compat.conf`; create it manually only when configuring
the system without an installer.

```ini
[Service]
ExecStart=
ExecStart=/usr/libexec/bluetooth/bluetoothd --compat
ExecStartPost=/bin/sh -c 'for i in 1 2 3 4 5; do [ -e /var/run/sdp ] && { chgrp bluetooth /var/run/sdp; chmod g+rw /var/run/sdp; exit 0; }; sleep 0.5; done'
```

Add the account that runs OpenAuto Prodigy to the `bluetooth` group, reload the
unit, and restart BlueZ:

```bash
sudo usermod -aG bluetooth "$USER"
sudo systemctl daemon-reload
sudo systemctl enable bluetooth
sudo systemctl restart bluetooth
```

Log out and back in after changing group membership. If the adapter is not
powered after the restart:

```bash
sudo bluetoothctl power on
```

To make this persistent, add to `/etc/bluetooth/main.conf` under `[Policy]`:

```ini
AutoEnable=true
```

## 3. OpenAuto Prodigy Configuration

Edit `~/.openauto/config.yaml`:

```yaml
connection:
  wifi_ap:
    interface: "wlan0"
    ssid: "OpenAutoProdigy"
    password: "prodigy1234"
  tcp_port: 5277
```

The SSID and password here must match your hostapd configuration. At startup,
the app reads `/etc/hostapd/hostapd.conf` and synchronizes those credentials
into its configuration, making hostapd the operational source of truth.

`connection.tcp_port` accepts ports 1 through 65535 and defaults to 5277. An
invalid value falls back to that default. Integer `0` is reserved for tests: it
asks the operating system for an ephemeral port. Bluetooth discovery always
advertises the port the listener actually bound, so discovery and TCP
admission cannot diverge.

## 4. Connecting

1. Start OpenAuto Prodigy on the Pi
2. On your phone: Settings > Connected devices > Connection preferences > Android Auto > Wireless Android Auto cars > Add a car
3. The phone should discover "OpenAutoProdigy" via Bluetooth
4. Pair when prompted
5. The phone receives WiFi credentials via BT, connects to the AP, then starts AA over TCP

Only one projection client is admitted at a time. Before a session becomes
active, a newer TCP connection may replace the pending client. Once projection
is connected or backgrounded, additional connections are rejected without
disturbing the active session. Shutdown closes admission before stopping the
session.

## Troubleshooting

- **Phone doesn't see Pi:** Check `bluetoothctl show` — adapter must be powered and discoverable
- **WiFi connection fails:** Verify hostapd recovery and its final error with
  `systemctl status hostapd` and `journalctl -u hostapd`; the application
  should remain running while the AP recovers. Also check channel compatibility.
- **TCP connection fails:** Ensure the app is running and the configured port
  is listening (`ss -tlnp | grep 5277` for the default)
- **"Incorrect credentials" error:** SSID/password in `config.yaml` must exactly match `hostapd.conf`
