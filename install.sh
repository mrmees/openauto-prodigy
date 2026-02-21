#!/usr/bin/env bash
#
# OpenAuto Prodigy — Interactive Install Script
# Targets: Raspberry Pi OS Trixie (Debian 13) on RPi 4
#
# Usage: curl -sSL <url> | bash
#    or: bash install.sh
#
set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

REPO_URL="https://github.com/mrmees/openauto-prodigy.git"
INSTALL_DIR="$HOME/openauto-prodigy"
CONFIG_DIR="$HOME/.openauto"
SERVICE_NAME="openauto-prodigy"

print_header() {
    echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  OpenAuto Prodigy — Installer${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
}

info()  { echo -e "${BLUE}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
fail()  { echo -e "${RED}[FAIL]${NC} $*"; }

# ────────────────────────────────────────────────────
# Step 1: Check OS and architecture
# ────────────────────────────────────────────────────
check_system() {
    info "Checking system..."

    if [[ ! -f /etc/os-release ]]; then
        fail "Cannot determine OS. /etc/os-release not found."
        exit 1
    fi

    source /etc/os-release

    ARCH=$(uname -m)
    info "OS: $PRETTY_NAME"
    info "Architecture: $ARCH"
    info "Kernel: $(uname -r)"

    if [[ "$ARCH" != "aarch64" && "$ARCH" != "armv7l" ]]; then
        warn "This script targets Raspberry Pi (ARM). Detected: $ARCH"
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || exit 1
    fi

    if [[ "${VERSION_CODENAME:-}" != "trixie" ]]; then
        warn "Expected RPi OS Trixie (Debian 13). Detected: ${VERSION_CODENAME:-unknown}"
        warn "Packages may differ. Proceeding anyway."
    fi
}

# ────────────────────────────────────────────────────
# Step 2: Install apt dependencies
# ────────────────────────────────────────────────────
install_dependencies() {
    info "Installing system dependencies..."
    sudo apt update

    local PACKAGES=(
        # Build tools
        cmake g++ git pkg-config

        # Qt 6
        qt6-base-dev qt6-declarative-dev qt6-wayland
        qt6-connectivity-dev qt6-multimedia-dev
        qml6-module-qtquick-controls qml6-module-qtquick-layouts
        qml6-module-qtquick-window qml6-module-qtqml-workerscript

        # Boost
        libboost-system-dev libboost-log-dev

        # Protocol Buffers
        libprotobuf-dev protobuf-compiler

        # OpenSSL
        libssl-dev

        # FFmpeg (video decoding)
        libavcodec-dev libavutil-dev

        # PipeWire (audio)
        libpipewire-0.3-dev libspa-0.2-dev

        # YAML config
        libyaml-cpp-dev

        # WiFi AP
        hostapd

        # Web config panel
        python3-flask

        # Bluetooth
        bluez
    )

    sudo apt install -y "${PACKAGES[@]}"
    ok "Dependencies installed"
}

# ────────────────────────────────────────────────────
# Step 3: Interactive hardware setup
# ────────────────────────────────────────────────────
setup_hardware() {
    echo -e "\n${CYAN}── Hardware Configuration ──${NC}\n"

    # Touch device
    info "Detecting touch devices..."
    TOUCH_DEVICES=$(find /dev/input -name 'event*' 2>/dev/null | head -10)
    if [[ -n "$TOUCH_DEVICES" ]]; then
        echo "$TOUCH_DEVICES"
        read -p "Touch device path [/dev/input/event4]: " TOUCH_DEV
        TOUCH_DEV=${TOUCH_DEV:-/dev/input/event4}
    else
        warn "No touch devices found."
        TOUCH_DEV="/dev/input/event4"
    fi

    # WiFi AP — detect wireless interfaces
    info "Detecting wireless interfaces..."
    WIFI_INTERFACES=()
    for iface_path in /sys/class/net/*/wireless; do
        if [[ -e "$iface_path" ]]; then
            WIFI_INTERFACES+=("$(basename "$(dirname "$iface_path")")")
        fi
    done

    if [[ ${#WIFI_INTERFACES[@]} -eq 0 ]]; then
        warn "No wireless interfaces found!"
        warn "WiFi AP will not be configured. You can set this up manually later."
        WIFI_IFACE=""
    elif [[ ${#WIFI_INTERFACES[@]} -eq 1 ]]; then
        WIFI_IFACE="${WIFI_INTERFACES[0]}"
        info "Found wireless interface: $WIFI_IFACE"
    else
        echo "Multiple wireless interfaces found:"
        for i in "${!WIFI_INTERFACES[@]}"; do
            echo "  $((i+1)). ${WIFI_INTERFACES[$i]}"
        done
        read -p "Select interface for WiFi AP [1]: " WIFI_CHOICE
        WIFI_CHOICE=${WIFI_CHOICE:-1}
        WIFI_IFACE="${WIFI_INTERFACES[$((WIFI_CHOICE-1))]}"
    fi

    if [[ -n "$WIFI_IFACE" ]]; then
        read -p "WiFi hotspot SSID [OpenAutoProdigy]: " WIFI_SSID
        WIFI_SSID=${WIFI_SSID:-OpenAutoProdigy}

        read -p "WiFi hotspot password [prodigy]: " WIFI_PASS
        WIFI_PASS=${WIFI_PASS:-prodigy}

        read -p "AP static IP [10.0.0.1]: " AP_IP
        AP_IP=${AP_IP:-10.0.0.1}

        read -p "Country code (for 5GHz) [US]: " COUNTRY_CODE
        COUNTRY_CODE=${COUNTRY_CODE:-US}
    fi

    # AA settings
    read -p "Android Auto TCP port [5277]: " TCP_PORT
    TCP_PORT=${TCP_PORT:-5277}

    read -p "Video FPS (30 or 60) [30]: " VIDEO_FPS
    VIDEO_FPS=${VIDEO_FPS:-30}

    ok "Hardware configuration complete"

    # Auto-start
    echo
    read -p "Start OpenAuto Prodigy automatically on boot? [y/N] " -n 1 -r
    echo
    AUTOSTART=false
    [[ $REPLY =~ ^[Yy]$ ]] && AUTOSTART=true
}

# ────────────────────────────────────────────────────
# Step 3b: Configure WiFi AP networking
# ────────────────────────────────────────────────────
configure_network() {
    if [[ -z "$WIFI_IFACE" ]]; then
        warn "Skipping network configuration (no wireless interface)"
        return
    fi

    info "Configuring WiFi AP on $WIFI_IFACE..."

    # systemd-networkd config for static IP + built-in DHCP server
    sudo mkdir -p /etc/systemd/network
    sudo tee /etc/systemd/network/10-openauto-ap.network > /dev/null << NETCFG
[Match]
Name=$WIFI_IFACE

[Network]
Address=${AP_IP}/24
DHCPServer=yes

[DHCPServer]
PoolOffset=10
PoolSize=40
EmitDNS=no
NETCFG

    # hostapd config
    sudo tee /etc/hostapd/hostapd.conf > /dev/null << HOSTAPD
interface=$WIFI_IFACE
driver=nl80211
ssid=$WIFI_SSID
hw_mode=a
channel=36
ieee80211n=1
ieee80211ac=1
wmm_enabled=1
country_code=$COUNTRY_CODE
ieee80211d=1
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=$WIFI_PASS
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
HOSTAPD

    # Point hostapd at our config
    if [[ -f /etc/default/hostapd ]]; then
        sudo sed -i 's|^#\?DAEMON_CONF=.*|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd
    fi

    # Enable services
    sudo systemctl unmask hostapd 2>/dev/null || true
    sudo systemctl enable hostapd
    sudo systemctl enable systemd-networkd

    ok "WiFi AP configured: SSID=$WIFI_SSID on $WIFI_IFACE ($AP_IP)"
}

# ────────────────────────────────────────────────────
# Step 4: Clone and build
# ────────────────────────────────────────────────────
build_project() {
    info "Cloning OpenAuto Prodigy..."

    if [[ -d "$INSTALL_DIR" ]]; then
        warn "Directory $INSTALL_DIR already exists."
        read -p "Pull latest and rebuild? [Y/n] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            cd "$INSTALL_DIR"
            git pull --recurse-submodules
        fi
    else
        git clone --recurse-submodules "$REPO_URL" "$INSTALL_DIR"
    fi

    cd "$INSTALL_DIR"
    mkdir -p build
    cd build

    info "Building (this may take a while on RPi)..."
    cmake ..
    cmake --build . -j$(nproc)
    ok "Build complete"
}

# ────────────────────────────────────────────────────
# Step 5: Generate config
# ────────────────────────────────────────────────────
generate_config() {
    info "Generating configuration..."
    mkdir -p "$CONFIG_DIR/themes/default" "$CONFIG_DIR/plugins"

    cat > "$CONFIG_DIR/config.yaml" << YAML
# OpenAuto Prodigy Configuration
# Generated by install.sh on $(date -Iseconds)

connection:
  wifi_ap:
    interface: "${WIFI_IFACE:-wlan0}"
    ssid: "$WIFI_SSID"
    password: "$WIFI_PASS"
  tcp_port: $TCP_PORT

video:
  fps: $VIDEO_FPS
  resolution: "480p"

display:
  width: 1024
  height: 600
  brightness: 80

touch:
  device: ""
YAML

    # Install default theme
    if [[ -f "$INSTALL_DIR/tests/data/themes/default/theme.yaml" ]]; then
        cp "$INSTALL_DIR/tests/data/themes/default/theme.yaml" "$CONFIG_DIR/themes/default/"
    fi

    ok "Configuration written to $CONFIG_DIR/config.yaml"
}

# ────────────────────────────────────────────────────
# Step 6: Create systemd service
# ────────────────────────────────────────────────────
create_service() {
    info "Creating systemd service..."

    sudo tee /etc/systemd/system/${SERVICE_NAME}.service > /dev/null << SERVICE
[Unit]
Description=OpenAuto Prodigy
After=graphical.target

[Service]
Type=simple
User=$USER
Environment=XDG_RUNTIME_DIR=/run/user/$(id -u)
Environment=WAYLAND_DISPLAY=wayland-0
Environment=QT_QPA_PLATFORM=wayland
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/build/src/openauto-prodigy
Restart=on-failure
RestartSec=3

[Install]
WantedBy=graphical.target
SERVICE

    sudo systemctl daemon-reload

    if [[ "$AUTOSTART" == "true" ]]; then
        sudo systemctl enable ${SERVICE_NAME}
        ok "Service created and enabled (auto-start on boot)"
    else
        ok "Service created (manual start: sudo systemctl start ${SERVICE_NAME})"
    fi
}

# ────────────────────────────────────────────────────
# Step 7: Create web config service
# ────────────────────────────────────────────────────
create_web_service() {
    info "Creating web config panel service..."

    sudo tee /etc/systemd/system/${SERVICE_NAME}-web.service > /dev/null << SERVICE
[Unit]
Description=OpenAuto Prodigy Web Config
After=network.target ${SERVICE_NAME}.service

[Service]
Type=simple
User=$USER
WorkingDirectory=$INSTALL_DIR/web-config
ExecStart=/usr/bin/python3 $INSTALL_DIR/web-config/server.py
Restart=on-failure
RestartSec=5
Environment=OAP_WEB_HOST=0.0.0.0
Environment=OAP_WEB_PORT=8080

[Install]
WantedBy=multi-user.target
SERVICE

    sudo systemctl daemon-reload
    sudo systemctl enable ${SERVICE_NAME}-web
    ok "Web config service created (port 8080)"
}

# ────────────────────────────────────────────────────
# Step 8: Diagnostics
# ────────────────────────────────────────────────────
run_diagnostics() {
    echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  System Diagnostics${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"

    # Qt version
    if command -v qmake6 &>/dev/null; then
        ok "Qt: $(qmake6 --version 2>&1 | grep 'Qt version' || echo 'detected')"
    else
        warn "Qt: qmake6 not in PATH (may still work via cmake)"
    fi

    # PipeWire
    if systemctl --user is-active pipewire &>/dev/null; then
        ok "PipeWire: running"
    elif pgrep pipewire &>/dev/null; then
        ok "PipeWire: running (not via systemd)"
    else
        warn "PipeWire: not detected — audio may not work"
    fi

    # BlueZ
    if systemctl is-active bluetooth &>/dev/null; then
        BLUEZ_VER=$(bluetoothctl --version 2>/dev/null || echo "unknown")
        ok "BlueZ: running ($BLUEZ_VER)"
    else
        warn "BlueZ: not running — Bluetooth features disabled"
    fi

    # Touch devices
    echo
    info "Touch devices:"
    for dev in /dev/input/event*; do
        if [[ -e "$dev" ]]; then
            NAME=$(cat /sys/class/input/$(basename $dev)/device/name 2>/dev/null || echo "unknown")
            echo "  $dev — $NAME"
        fi
    done

    # Audio outputs
    echo
    info "Audio outputs:"
    if command -v pactl &>/dev/null; then
        pactl list short sinks 2>/dev/null | while read -r line; do
            echo "  $line"
        done
    elif command -v pw-cli &>/dev/null; then
        pw-cli list-objects Node 2>/dev/null | grep -i "audio.*sink" | head -5 || echo "  (use pw-cli to inspect)"
    else
        echo "  (pactl/pw-cli not found)"
    fi

    # Config
    echo
    if [[ -f "$CONFIG_DIR/config.yaml" ]]; then
        ok "Config: $CONFIG_DIR/config.yaml"
    else
        warn "Config: not found"
    fi

    # Plugins
    PLUGIN_COUNT=$(find "$CONFIG_DIR/plugins" -name "manifest.yaml" 2>/dev/null | wc -l)
    info "Plugins: $PLUGIN_COUNT dynamic plugins in $CONFIG_DIR/plugins/"
    info "  + 3 static plugins (Android Auto, Bluetooth Audio, Phone)"

    # WiFi AP
    echo
    if systemctl is-active hostapd &>/dev/null; then
        ok "WiFi AP: running (SSID: $WIFI_SSID)"
    else
        info "WiFi AP: not started (configure hostapd separately)"
    fi

    echo -e "\n${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo
    echo -e "  Start:  ${BLUE}sudo systemctl start ${SERVICE_NAME}${NC}"
    echo -e "  Web:    ${BLUE}http://$(hostname -I | awk '{print $1}'):8080${NC}"
    echo -e "  Logs:   ${BLUE}journalctl -u ${SERVICE_NAME} -f${NC}"
    echo -e "  Config: ${BLUE}$CONFIG_DIR/config.yaml${NC}"
    echo
}

# ────────────────────────────────────────────────────
# Main
# ────────────────────────────────────────────────────
main() {
    print_header
    check_system
    install_dependencies
    setup_hardware
    build_project
    generate_config
    configure_network
    create_service
    create_web_service
    run_diagnostics
}

main "$@"
