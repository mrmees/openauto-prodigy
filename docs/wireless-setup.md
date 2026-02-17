# Wireless Android Auto Setup

OpenAuto Prodigy uses wireless-only Android Auto. The phone discovers the Pi via Bluetooth, receives WiFi credentials, connects to the Pi's WiFi AP, then starts the AA protocol over TCP.

## Prerequisites

- Raspberry Pi 4 with built-in WiFi + Bluetooth
- Phone with Android Auto wireless support (Android 11+ recommended)

## 1. WiFi Access Point (hostapd + dnsmasq)

### Install packages

```bash
sudo apt install hostapd dnsmasq
```

### Configure hostapd

Create/edit `/etc/hostapd/hostapd.conf`:

```ini
interface=wlan0
driver=nl80211
ssid=OpenAutoProdigy
hw_mode=g
channel=6
wmm_enabled=0
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

### Configure static IP on wlan0

Create `/etc/network/interfaces.d/wlan0`:

```
auto wlan0
iface wlan0 inet static
    address 10.0.0.1
    netmask 255.255.255.0
```

Or if using NetworkManager, create `/etc/NetworkManager/conf.d/unmanage-wlan0.conf`:

```ini
[keyfile]
unmanaged-devices=interface-name:wlan0
```

Then configure with systemd-networkd or `/etc/dhcpcd.conf`:

```
interface wlan0
static ip_address=10.0.0.1/24
nohook wpa_supplicant
```

### Configure dnsmasq

Create `/etc/dnsmasq.d/openauto.conf`:

```ini
interface=wlan0
dhcp-range=10.0.0.10,10.0.0.50,255.255.255.0,24h
```

### Enable and start

```bash
sudo systemctl unmask hostapd
sudo systemctl enable hostapd dnsmasq
sudo systemctl start hostapd dnsmasq
```

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

Edit `~/.config/openauto_system.ini`:

```ini
[Wireless]
enabled=true
wifi_ssid=OpenAutoProdigy
wifi_password=prodigy1234
tcp_port=5288
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
- **"Incorrect credentials" error:** SSID/password in `openauto_system.ini` must exactly match `hostapd.conf`
