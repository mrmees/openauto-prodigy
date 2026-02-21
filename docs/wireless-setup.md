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
- Sets up 5GHz (channel 36) with your country code

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

# 5GHz is MANDATORY for wireless Android Auto on Pi 4/5.
# The CYW43455 combo chip shares one antenna for WiFi and Bluetooth.
# Running on 2.4GHz causes severe BT coexistence interference — the phone
# can't maintain BT RFCOMM and WiFi simultaneously on the same radio band.
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

[DHCPServer]
PoolOffset=10
PoolSize=40
EmitDNS=no
```

This replaces dnsmasq — systemd-networkd has a built-in DHCP server. Enable it:

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

The Pi's Bluetooth must be discoverable. OpenAuto Prodigy handles the BT RFCOMM server and SDP registration automatically — no manual bluetoothctl setup needed beyond making sure the adapter is powered on.

```bash
sudo systemctl enable bluetooth
sudo systemctl start bluetooth
```

If the adapter isn't powering on automatically:

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
  tcp_port: 5288
```

The SSID and password here **must match** your hostapd configuration.

## 4. Connecting

1. Start OpenAuto Prodigy on the Pi
2. On your phone: Settings > Connected devices > Connection preferences > Android Auto > Wireless Android Auto cars > Add a car
3. The phone should discover "OpenAutoProdigy" via Bluetooth
4. Pair when prompted
5. The phone receives WiFi credentials via BT, connects to the AP, then starts AA over TCP

## Troubleshooting

- **Phone doesn't see Pi:** Check `bluetoothctl show` — adapter must be powered and discoverable
- **WiFi connection fails:** Verify hostapd is running (`systemctl status hostapd`), check channel compatibility
- **TCP connection fails:** Ensure the app is running and port 5288 is not blocked (`ss -tlnp | grep 5288`)
- **"Incorrect credentials" error:** SSID/password in `config.yaml` must exactly match `hostapd.conf`
