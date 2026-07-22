# Wireless Android Auto Setup

OpenAuto Prodigy uses wireless-only Android Auto. The phone discovers the Pi via Bluetooth, receives WiFi credentials, connects to the Pi's WiFi AP, then starts the AA protocol over TCP.

## Prerequisites

- Raspberry Pi 4 with built-in WiFi + Bluetooth
- Phone with Android Auto wireless support (Android 11+ recommended)

## Automated Setup

The install script (`install.sh`) handles WiFi AP configuration automatically:
- Detects available wireless interfaces
- Lets you choose which interface to use for the AP
- Configures systemd-networkd for static IP + DHCP server
- Writes hostapd.conf with your SSID/password
- Prefers 5GHz (channel 36) when the selected adapter supports it and falls
  back to 2.4GHz otherwise

Run `bash install.sh` for the guided setup. The sections below are for manual configuration or troubleshooting.

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
# coexistence interference. The installer detects adapter capabilities and
# falls back to 2.4GHz when 5GHz is unavailable.
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

```bash
sudo systemctl unmask hostapd
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

## 4. Connecting

1. Start OpenAuto Prodigy on the Pi
2. On your phone: Settings > Connected devices > Connection preferences > Android Auto > Wireless Android Auto cars > Add a car
3. The phone should discover "OpenAutoProdigy" via Bluetooth
4. Pair when prompted
5. The phone receives WiFi credentials via BT, connects to the AP, then starts AA over TCP

## Troubleshooting

- **Phone doesn't see Pi:** Check `bluetoothctl show` — adapter must be powered and discoverable
- **WiFi connection fails:** Verify hostapd is running (`systemctl status hostapd`), check channel compatibility
- **TCP connection fails:** Ensure the app is running and the configured port
  is listening (`ss -tlnp | grep 5277` for the default)
- **"Incorrect credentials" error:** SSID/password in `config.yaml` must exactly match `hostapd.conf`
