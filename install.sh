#!/usr/bin/env bash
#
# OpenAuto Prodigy — Interactive Install Script
# Targets: Raspberry Pi OS Trixie (Debian 13) on RPi 4
#
# Usage: curl -sSL <url> | bash
#    or: bash install.sh
#
set -euo pipefail

trap 'echo -e "\033[0;31m[CRASH]\033[0m Script failed at line $LINENO (exit code $?): $(sed -n "${LINENO}p" "$0" 2>/dev/null || echo "unknown")"' ERR

# Wrap entire script in a block so bash reads it all into memory before
# executing. This prevents git pull from modifying the script mid-run.
{

# Flags
VERBOSE=false
while [[ "${1:-}" == -* ]]; do
    case "$1" in
        -v|--verbose) VERBOSE=true; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

REPO_URL="https://github.com/mrmees/openauto-prodigy.git"
INSTALL_DIR="$HOME/openauto-prodigy"
CONFIG_DIR="$HOME/.openauto"
SERVICE_NAME="openauto-prodigy"

# Defaults for optional variables (may be overridden by setup_hardware)
WIFI_IFACE=""
WIFI_SSID=""
WIFI_PASS="changeme"
DEVICE_NAME="OpenAutoProdigy"
AP_IP="10.0.0.1"
COUNTRY_CODE="US"
TCP_PORT="5277"
VIDEO_FPS="30"
AUTOSTART=false

print_header() {
    clear
    echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  OpenAuto Prodigy — Installer${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
}

info()  { echo -e "${BLUE}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
fail()  { echo -e "${RED}[FAIL]${NC} $*"; }

# ────────────────────────────────────────────────────
# UI Primitives — pretty output with --verbose bypass
# ────────────────────────────────────────────────────

# Detect unicode support (braille spinner vs ASCII fallback)
if printf '\u2800' 2>/dev/null | grep -q '⠀' 2>/dev/null; then
    SPINNER_CHARS='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    CHECK='✓'
    CROSS='✗'
else
    SPINNER_CHARS='|/-\'
    CHECK='+'
    CROSS='x'
fi

CURRENT_STEP=0
TOTAL_STEPS=8
SPINNER_LOG=""

# Step counter — prints "[3/9] Label"
step() {
    CURRENT_STEP=$((CURRENT_STEP + 1))
    echo -e "\n${CYAN}[${CURRENT_STEP}/${TOTAL_STEPS}]${NC} ${BOLD}$*${NC}"
}

# Format elapsed seconds as M:SS
_fmt_elapsed() {
    local secs=$1
    printf '%d:%02d' $((secs / 60)) $((secs % 60))
}

# Show last N lines of log on failure
show_error_tail() {
    local logfile=$1 lines=${2:-20}
    if [[ -f "$logfile" && -s "$logfile" ]]; then
        echo -e "\n${RED}── Last ${lines} lines of output ──${NC}"
        tail -n "$lines" "$logfile"
        echo -e "${RED}── Full log: ${logfile} ──${NC}"
    fi
}

# Run a command with an animated spinner + elapsed timer
# Usage: run_with_spinner "Installing dependencies" apt install -y ...
run_with_spinner() {
    local label="$1"; shift

    if [[ "$VERBOSE" == "true" ]]; then
        info "$label"
        "$@"
        return $?
    fi

    SPINNER_LOG=$(mktemp /tmp/oap-install-XXXXXX.log)
    local start_time=$SECONDS
    local spin_i=0
    local char_count=${#SPINNER_CHARS}

    "$@" > "$SPINNER_LOG" 2>&1 &
    local cmd_pid=$!

    # Spinner loop
    while kill -0 "$cmd_pid" 2>/dev/null; do
        local elapsed=$((SECONDS - start_time))
        local c="${SPINNER_CHARS:$((spin_i % char_count)):1}"
        printf '\r  %s %s (%s)  ' "$c" "$label" "$(_fmt_elapsed $elapsed)"
        spin_i=$((spin_i + 1))
        sleep 0.1
    done

    wait "$cmd_pid"
    local exit_code=$?
    local elapsed=$((SECONDS - start_time))

    if [[ $exit_code -eq 0 ]]; then
        echo -e "\r  ${GREEN}${CHECK}${NC} ${label} ($(_fmt_elapsed $elapsed))  "
        rm -f "$SPINNER_LOG"
    else
        echo -e "\r  ${RED}${CROSS}${NC} ${label} ($(_fmt_elapsed $elapsed))  "
        show_error_tail "$SPINNER_LOG"
        return $exit_code
    fi
}

# Run cmake --build with percentage progress + elapsed timer
# Usage: build_with_progress "Building" cmake --build . -j4
build_with_progress() {
    local label="$1"; shift

    if [[ "$VERBOSE" == "true" ]]; then
        info "$label"
        "$@"
        return $?
    fi

    SPINNER_LOG=$(mktemp /tmp/oap-build-XXXXXX.log)
    local start_time=$SECONDS
    local spin_i=0
    local char_count=${#SPINNER_CHARS}
    local last_pct=""

    # Run build in background, output to log
    "$@" > "$SPINNER_LOG" 2>&1 &
    local cmd_pid=$!

    # Spinner loop — scrape latest percentage from log file
    while kill -0 "$cmd_pid" 2>/dev/null; do
        local elapsed=$((SECONDS - start_time))
        local c="${SPINNER_CHARS:$((spin_i % char_count)):1}"

        # Grab last percentage from log (cmake outputs [  X%])
        local pct
        pct=$(sed -n 's/.*\[\s*\([0-9]*\)%\].*/\1/p' "$SPINNER_LOG" 2>/dev/null | tail -1) || true
        if [[ -n "$pct" ]]; then
            last_pct="$pct"
        fi

        if [[ -n "$last_pct" ]]; then
            printf '\r  %s %s (%s%%) (%s)  ' "$c" "$label" "$last_pct" "$(_fmt_elapsed $elapsed)"
        else
            printf '\r  %s %s (%s)  ' "$c" "$label" "$(_fmt_elapsed $elapsed)"
        fi
        spin_i=$((spin_i + 1))
        sleep 0.25
    done

    wait "$cmd_pid"
    local exit_code=$?
    local elapsed=$((SECONDS - start_time))

    if [[ $exit_code -eq 0 ]]; then
        echo -e "\r  ${GREEN}${CHECK}${NC} ${label} (100%) ($(_fmt_elapsed $elapsed))  "
        rm -f "$SPINNER_LOG"
    else
        echo -e "\r  ${RED}${CROSS}${NC} ${label} ($(_fmt_elapsed $elapsed))  "
        show_error_tail "$SPINNER_LOG"
        return $exit_code
    fi
}

# Prime sudo and keep it alive in background
prime_sudo() {
    if [[ "$EUID" -eq 0 ]]; then
        return  # Already root
    fi
    echo -e "${BLUE}[sudo]${NC} This installer needs sudo for system configuration."
    sudo -v
    # Keepalive loop — refresh sudo every 50s until script exits
    (while true; do sudo -n -v 2>/dev/null; sleep 50; done) &
    SUDO_KEEPALIVE_PID=$!
    trap 'kill $SUDO_KEEPALIVE_PID 2>/dev/null; rm -f "$SPINNER_LOG"' EXIT
}

# ────────────────────────────────────────────────────
# Step 1: Check OS and architecture
# ────────────────────────────────────────────────────
check_system() {
    step "Checking system"

    if [[ ! -f /etc/os-release ]]; then
        fail "Cannot determine OS. /etc/os-release not found."
        exit 1
    fi

    source /etc/os-release

    ARCH=$(uname -m)
    ok "OS: $PRETTY_NAME | $ARCH | $(uname -r)"

    if [[ "$ARCH" != "aarch64" && "$ARCH" != "armv7l" ]]; then
        warn "This script targets Raspberry Pi (ARM). Detected: $ARCH"
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || exit 1
    fi

    if [[ "${VERSION_CODENAME:-}" != "trixie" ]]; then
        warn "Expected RPi OS Trixie (Debian 13). Detected: ${VERSION_CODENAME:-unknown}"
    fi
}

# ────────────────────────────────────────────────────
# Step 2: Install apt dependencies
# ────────────────────────────────────────────────────
install_dependencies() {
    step "Installing dependencies"

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
        pipewire-pulse pulseaudio-utils

        # YAML config
        libyaml-cpp-dev

        # WiFi AP
        hostapd rfkill

        # Web config panel
        python3-flask

        # System service Python deps (avoids needing pip/internet later)
        python3-dbus-next python3-yaml

        # Bluetooth
        bluez libbluetooth-dev

        # Transparent SOCKS5 proxy routing (companion internet sharing)
        redsocks
        iptables

        # Python venv support (for system-service fallback)
        python3-venv
    )

    run_with_spinner "Updating package lists" sudo apt-get update -qq
    run_with_spinner "Installing ${#PACKAGES[@]} packages" sudo apt-get install -y -qq "${PACKAGES[@]}"
}

# ────────────────────────────────────────────────────
# Step 3: Interactive hardware setup
# ────────────────────────────────────────────────────
setup_hardware() {
    step "Hardware configuration"

    # Touch device — filter for INPUT_PROP_DIRECT (touchscreens)
    info "Detecting touch devices..."
    TOUCH_DEVS=()
    for dev in /dev/input/event*; do
        if [[ -e "$dev" ]]; then
            # Check for INPUT_PROP_DIRECT (bit 0x01 in properties bitmask)
            local PROPS_PATH="/sys/class/input/$(basename "$dev")/device/properties"
            if [[ -f "$PROPS_PATH" ]] && (( $(cat "$PROPS_PATH" 2>/dev/null || echo 0) & 2 )); then
                NAME=$(cat "/sys/class/input/$(basename "$dev")/device/name" 2>/dev/null || echo "unknown")
                TOUCH_DEVS+=("$dev")
                printf "  %-24s %s\n" "$dev" "$NAME"
            fi
        fi
    done
    echo
    if [[ ${#TOUCH_DEVS[@]} -eq 0 ]]; then
        warn "No touchscreen devices detected."
        ok "Touch: will auto-detect at runtime"
        TOUCH_DEV=""
    elif [[ ${#TOUCH_DEVS[@]} -eq 1 ]]; then
        TOUCH_DEV="${TOUCH_DEVS[0]}"
        NAME=$(cat "/sys/class/input/$(basename "$TOUCH_DEV")/device/name" 2>/dev/null || echo "unknown")
        ok "Touch: $TOUCH_DEV ($NAME)"
    else
        read -p "Touch device path [${TOUCH_DEVS[0]}]: " TOUCH_DEV
        TOUCH_DEV=${TOUCH_DEV:-${TOUCH_DEVS[0]}}
        ok "Touch: $TOUCH_DEV"
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
        DEVICE_NAME="Prodigy_$(od -An -tx1 -N2 /dev/urandom | tr -d ' \n')"
        echo ""
        info "This name identifies your vehicle on both WiFi and Bluetooth."
        info "The default includes a unique suffix to avoid conflicts with multiple vehicles."
        echo ""
        read -p "Device name [$DEVICE_NAME]: " USER_DEVICE_NAME
        DEVICE_NAME=${USER_DEVICE_NAME:-$DEVICE_NAME}
        WIFI_SSID="$DEVICE_NAME"

        WIFI_PASS=$(head -c 12 /dev/urandom | base64 | tr -dc 'a-zA-Z0-9' | head -c 16)
        info "Generated random WiFi password (sent to phone automatically over BT)"

        read -p "AP static IP [10.0.0.1]: " AP_IP
        AP_IP=${AP_IP:-10.0.0.1}

        # Detect country code: iw reg → IP geolocation → locale → US fallback
        # Locale is last because RPi OS defaults to en_GB regardless of user setup
        COUNTRY_CODE=""
        CC_SOURCE=""
        if command -v iw &>/dev/null; then
            COUNTRY_CODE=$(iw reg get 2>/dev/null | sed -n 's/^country \([A-Z]\{2\}\).*/\1/p' | head -1) || true
            if [[ "$COUNTRY_CODE" == "00" || "$COUNTRY_CODE" == "99" ]]; then
                COUNTRY_CODE=""
            elif [[ -n "$COUNTRY_CODE" ]]; then
                CC_SOURCE="wireless regulatory domain"
            fi
        fi
        if [[ -z "$COUNTRY_CODE" ]] && command -v curl &>/dev/null; then
            # Try IP geolocation (we have internet if we cloned from GitHub)
            COUNTRY_CODE=$(curl -fsS --max-time 3 https://ipinfo.io/country 2>/dev/null | tr -d '\r\n') || true
            if [[ ! "$COUNTRY_CODE" =~ ^[A-Z]{2}$ ]]; then
                COUNTRY_CODE=""
            else
                CC_SOURCE="IP geolocation"
            fi
        fi
        if [[ -z "$COUNTRY_CODE" ]]; then
            # Locale fallback (unreliable on RPi OS — defaults to en_GB)
            COUNTRY_CODE=$(locale 2>/dev/null | sed -n 's/.*_\([A-Z]\{2\}\)\..*/\1/p' | head -1) || true
            if [[ -n "$COUNTRY_CODE" ]]; then
                CC_SOURCE="system locale"
            fi
        fi
        if [[ -z "$COUNTRY_CODE" ]]; then
            COUNTRY_CODE="US"
            CC_SOURCE=""
            warn "Could not auto-detect country code — defaulting to US"
        else
            info "Detected country code $COUNTRY_CODE (via $CC_SOURCE)"
        fi
        read -p "Country code for 5GHz WiFi [$COUNTRY_CODE]: " USER_CC
        COUNTRY_CODE=${USER_CC:-$COUNTRY_CODE}
        COUNTRY_CODE=$(echo "$COUNTRY_CODE" | tr '[:lower:]' '[:upper:]')
        if [[ ! "$COUNTRY_CODE" =~ ^[A-Z]{2}$ ]]; then
            warn "Invalid country code '$COUNTRY_CODE' — using US"
            COUNTRY_CODE="US"
        fi

        # Set regulatory domain system-wide so hostapd can use 5GHz
        sudo iw reg set "$COUNTRY_CODE" 2>/dev/null || true
        if [[ -f /etc/default/crda ]]; then
            sudo sed -i "s/^REGDOMAIN=.*/REGDOMAIN=$COUNTRY_CODE/" /etc/default/crda
        fi
        ok "Country code: $COUNTRY_CODE (5GHz WiFi regulatory domain)"
    fi

    # Audio output device
    info "Detecting audio output devices..."
    AUDIO_SINK=""
    if command -v pactl &>/dev/null; then
        AUDIO_SINKS=()
        AUDIO_DESCS=()
        while IFS=$'\t' read -r name_field desc_field; do
            local name desc
            name=$(echo "$name_field" | sed 's/.*Name: //')
            desc=$(echo "$desc_field" | sed 's/.*Description: //')
            name=$(echo "$name" | tr -d '[:space:]')
            AUDIO_SINKS+=("$name")
            AUDIO_DESCS+=("$desc")
            printf "  %d. %s\n" "${#AUDIO_SINKS[@]}" "$desc"
        done < <(pactl list sinks 2>/dev/null | grep -E "^\s*(Name|Description):" | paste - -)

        if [[ ${#AUDIO_SINKS[@]} -gt 0 ]]; then
            echo
            read -p "Select audio output [1]: " AUDIO_CHOICE
            AUDIO_CHOICE=${AUDIO_CHOICE:-1}
            AUDIO_SINK="${AUDIO_SINKS[$((AUDIO_CHOICE-1))]}"
            ok "Audio: ${AUDIO_DESCS[$((AUDIO_CHOICE-1))]}"
        else
            warn "No audio sinks detected — will use PipeWire default"
        fi
    else
        warn "pactl not available — will use PipeWire default"
    fi

    # AA settings
    read -p "Android Auto TCP port [5277]: " TCP_PORT
    TCP_PORT=${TCP_PORT:-5277}

    ok "Hardware configuration complete"

    # Auto-start
    echo
    read -p "Start OpenAuto Prodigy automatically on boot? [Y/n] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        AUTOSTART=false
    else
        AUTOSTART=true
    fi
}

# ────────────────────────────────────────────────────
# Step 3b: Configure WiFi AP networking
# ────────────────────────────────────────────────────
configure_network() {
    step "Configuring network"

    if [[ -z "$WIFI_IFACE" ]]; then
        warn "Skipping network configuration (no wireless interface)"
        return
    fi

    # Unblock WiFi and Bluetooth radios (fresh Trixie has them soft-blocked)
    if command -v rfkill &>/dev/null; then
        sudo rfkill unblock wlan 2>/dev/null || true
        sudo rfkill unblock bluetooth 2>/dev/null || true
    fi

    # BlueZ needs --compat for SDP service registration (AA RFCOMM discovery)
    local BT_OVERRIDE="/etc/systemd/system/bluetooth.service.d"
    sudo mkdir -p "$BT_OVERRIDE"
    sudo tee "$BT_OVERRIDE/override.conf" > /dev/null << 'BTCONF'
[Service]
ExecStart=
ExecStart=/usr/libexec/bluetooth/bluetoothd --compat
ExecStartPost=/bin/sh -c 'for i in 1 2 3 4 5; do [ -e /var/run/sdp ] && { chgrp bluetooth /var/run/sdp; chmod g+rw /var/run/sdp; exit 0; }; sleep 0.5; done'
BTCONF

    run_with_spinner "Restarting BlueZ (SDP compat)" bash -c 'sudo systemctl daemon-reload && sudo systemctl restart bluetooth'

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

    # Enable and start services
    sudo systemctl unmask hostapd 2>/dev/null || true
    sudo systemctl enable --quiet hostapd
    sudo systemctl enable --quiet systemd-networkd
    run_with_spinner "Starting network services" bash -c 'sudo systemctl restart systemd-networkd 2>/dev/null; true'
    if sudo systemctl restart hostapd 2>/dev/null; then
        ok "WiFi AP: SSID=$WIFI_SSID on $WIFI_IFACE ($AP_IP)"
    else
        warn "hostapd failed to start (may need reboot). Enabled for next boot."
    fi
}

# ────────────────────────────────────────────────────
# Step 3c: Configure labwc for multi-touch
# ────────────────────────────────────────────────────
configure_labwc() {
    local LABWC_DIR="$HOME/.config/labwc"
    local RC_FILE="$LABWC_DIR/rc.xml"

    mkdir -p "$LABWC_DIR"

    if [[ -f "$RC_FILE" ]]; then
        # Replace mouseEmulation="yes" with "no" if present
        if grep -q 'mouseEmulation="yes"' "$RC_FILE"; then
            sed -i 's/mouseEmulation="yes"/mouseEmulation="no"/' "$RC_FILE"
            ok "labwc: mouseEmulation set to \"no\" (was \"yes\")"
        elif grep -q 'mouseEmulation="no"' "$RC_FILE"; then
            ok "labwc: mouseEmulation already \"no\""
        else
            warn "labwc: rc.xml exists but no mouseEmulation found — check manually"
        fi
    else
        # Create minimal rc.xml with correct touch config
        cat > "$RC_FILE" << 'LABWC'
<?xml version="1.0"?>
<openbox_config xmlns="http://openbox.org/3.4/rc">
	<touch deviceName="" mapToOutput="" mouseEmulation="no"/>
</openbox_config>
LABWC
        ok "labwc: created rc.xml with mouseEmulation=\"no\""
    fi
}

# ────────────────────────────────────────────────────
# Step 4: Clone and build
# ────────────────────────────────────────────────────
build_project() {
    step "Building from source"

    cd "$INSTALL_DIR"
    run_with_spinner "Initializing submodules" git submodule update --init --recursive

    if [[ -f "$INSTALL_DIR/build/src/openauto-prodigy" ]]; then
        warn "OpenAuto Prodigy is already built."
        read -p "Rebuild? [Y/n] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Nn]$ ]]; then
            info "Skipping build."
            return
        fi
    fi
    mkdir -p build
    cd build

    run_with_spinner "Configuring CMake" cmake .. -Wno-dev
    build_with_progress "Building" cmake --build . -j$(nproc)
}

# ────────────────────────────────────────────────────
# Step 5: Generate config
# ────────────────────────────────────────────────────
generate_config() {
    step "Generating configuration"
    mkdir -p "$CONFIG_DIR/themes/default" "$CONFIG_DIR/plugins"

    cat > "$CONFIG_DIR/config.yaml" << YAML
# OpenAuto Prodigy Configuration
# Generated by install.sh on $(date -Iseconds)

connection:
  wifi_ap:
    interface: "${WIFI_IFACE:-wlan0}"
    ssid: "${WIFI_SSID:-$DEVICE_NAME}"
    password: "$WIFI_PASS"
  tcp_port: $TCP_PORT
  bt_name: "$DEVICE_NAME"
  auto_connect_aa: true

audio:
  output_device: "${AUDIO_SINK}"

video:
  fps: $VIDEO_FPS
  resolution: "480p"

display:
  width: 1024
  height: 600
  brightness: 80

touch:
  device: ""

companion:
  enabled: true
  port: 9876
YAML

    # Install default theme
    if [[ -f "$INSTALL_DIR/config/themes/default/theme.yaml" ]]; then
        cp "$INSTALL_DIR/config/themes/default/theme.yaml" "$CONFIG_DIR/themes/default/"
    fi

    # Companion app polkit rule (allows timedatectl set-time without sudo)
    if [[ -f "$INSTALL_DIR/config/companion-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/companion-polkit.rules" /etc/polkit-1/rules.d/50-openauto-time.rules
        ok "Companion polkit rule installed"
    fi

    # BlueZ agent polkit rule (allows non-root pairing agent registration)
    if [[ -f "$INSTALL_DIR/config/bluez-agent-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/bluez-agent-polkit.rules" /etc/polkit-1/rules.d/50-openauto-bluez.rules
        ok "BlueZ agent polkit rule installed"
    fi

    # Ensure user is in required groups
    sudo usermod -aG bluetooth "$USER"  # BlueZ D-Bus access
    sudo usermod -aG input "$USER"      # evdev touch device access

    ok "Configuration written to $CONFIG_DIR/config.yaml"
}

# ────────────────────────────────────────────────────
# Step 6: Create systemd service
# ────────────────────────────────────────────────────
create_service() {
    step "Creating services"

    local USER_ID
    USER_ID=$(id -u)

    sudo tee /etc/systemd/system/${SERVICE_NAME}.service > /dev/null << SERVICE
[Unit]
Description=OpenAuto Prodigy
After=graphical.target
Wants=openauto-system.service

[Service]
Type=simple
User=$USER
Environment=XDG_RUNTIME_DIR=/run/user/$USER_ID
Environment=WAYLAND_DISPLAY=wayland-0
Environment=QT_QPA_PLATFORM=wayland
Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$USER_ID/bus
WorkingDirectory=$INSTALL_DIR
ExecStart=$INSTALL_DIR/build/src/openauto-prodigy
Restart=on-failure
RestartSec=3

[Install]
WantedBy=graphical.target
SERVICE

    sudo systemctl daemon-reload

    if [[ "$AUTOSTART" == "true" ]]; then
        sudo systemctl enable --quiet ${SERVICE_NAME}
        ok "Service created and enabled (auto-start on boot)"
    else
        ok "Service created (manual start: sudo systemctl start ${SERVICE_NAME})"
    fi
}

# ────────────────────────────────────────────────────
# Step 7: Create web config service
# ────────────────────────────────────────────────────
create_web_service() {

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
    sudo systemctl enable --quiet ${SERVICE_NAME}-web
    ok "Web config service created (port 8080)"
}

# ────────────────────────────────────────────────────
# Step 8: Create system service (Python daemon)
# ────────────────────────────────────────────────────
create_system_service() {
    local SYS_DIR="$INSTALL_DIR/system-service"
    local VENV_DIR="$SYS_DIR/.venv"

    if [[ ! -f "$SYS_DIR/openauto_system.py" ]]; then
        warn "System service not found at $SYS_DIR — skipping"
        return
    fi

    # Try venv + pip first; fall back to system Python if pip fails (no internet on AP)
    local PYTHON_PATH="/usr/bin/python3"
    if python3 -m venv "$VENV_DIR" 2>/dev/null; then
        if "$VENV_DIR/bin/pip" install --quiet -r "$SYS_DIR/requirements.txt" 2>/dev/null; then
            PYTHON_PATH="$VENV_DIR/bin/python3"
            ok "Python venv created at $VENV_DIR"
        elif "$VENV_DIR/bin/pip" install --quiet dbus-next PyYAML 2>/dev/null; then
            PYTHON_PATH="$VENV_DIR/bin/python3"
            ok "Python venv created (runtime deps only)"
        else
            warn "pip install failed (no internet?). Using system Python."
            warn "Install python3-dbus-next and python3-yaml via apt if not already present."
            rm -rf "$VENV_DIR"
        fi
    else
        warn "Could not create venv. Using system Python."
    fi

    # Install systemd service with correct paths
    sudo tee /etc/systemd/system/openauto-system.service > /dev/null << SERVICE
[Unit]
Description=OpenAuto Prodigy System Manager
Before=${SERVICE_NAME}.service
After=network.target bluetooth.target

[Service]
Type=notify
User=$USER
ExecStart=$PYTHON_PATH $SYS_DIR/openauto_system.py
ExecStopPost=-/usr/sbin/iptables -t nat -D OUTPUT -p tcp -j OPENAUTO_PROXY
ExecStopPost=-/usr/sbin/iptables -t nat -F OPENAUTO_PROXY
ExecStopPost=-/usr/sbin/iptables -t nat -X OPENAUTO_PROXY
WorkingDirectory=$SYS_DIR
RuntimeDirectory=openauto
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
SERVICE

    sudo systemctl daemon-reload
    sudo systemctl enable --quiet openauto-system
    ok "System service installed and enabled"
}

# ────────────────────────────────────────────────────
# Step 9: Diagnostics
# ────────────────────────────────────────────────────
run_diagnostics() {
    step "Verifying installation"

    local warnings=()

    # Quick checks — collect warnings, show ok for passes
    if systemctl --user is-active pipewire &>/dev/null || pgrep -x pipewire &>/dev/null; then
        ok "PipeWire running"
    else
        warnings+=("PipeWire not detected — audio may not work")
    fi

    if systemctl is-active bluetooth &>/dev/null; then
        ok "BlueZ running"
    else
        warnings+=("BlueZ not running — Bluetooth features disabled")
    fi

    local RC_FILE="$HOME/.config/labwc/rc.xml"
    if [[ -f "$RC_FILE" ]] && grep -q 'mouseEmulation="no"' "$RC_FILE"; then
        ok "labwc multi-touch configured"
    elif [[ -f "$RC_FILE" ]]; then
        warnings+=("labwc mouseEmulation may be enabled — check $RC_FILE")
    fi

    if [[ -n "$WIFI_SSID" ]] && systemctl is-active hostapd &>/dev/null; then
        ok "WiFi AP running (SSID: $WIFI_SSID)"
    elif [[ -n "$WIFI_SSID" ]]; then
        warnings+=("WiFi AP configured but hostapd not running — may need reboot")
    fi

    local needs_relogin=false
    if ! id -nG "$USER" | grep -qw bluetooth; then
        warnings+=("User not yet in bluetooth group")
        needs_relogin=true
    fi
    if ! id -nG "$USER" | grep -qw input; then
        warnings+=("User not yet in input group")
        needs_relogin=true
    fi

    # Show warnings if any
    for w in "${warnings[@]}"; do
        warn "$w"
    done

    # Verbose mode: extended diagnostics
    if [[ "$VERBOSE" == "true" ]]; then
        echo
        if command -v qmake6 &>/dev/null; then
            info "Qt: $(qmake6 --version 2>&1 | grep 'Qt version' || echo 'detected')"
        fi
        info "Touchscreen devices:"
        for dev in /dev/input/event*; do
            if [[ -e "$dev" ]]; then
                local PROPS_PATH="/sys/class/input/$(basename "$dev")/device/properties"
                if [[ -f "$PROPS_PATH" ]] && (( $(cat "$PROPS_PATH" 2>/dev/null || echo 0) & 2 )); then
                    NAME=$(cat "/sys/class/input/$(basename "$dev")/device/name" 2>/dev/null || echo "unknown")
                    printf "  %-24s %s\n" "$dev" "$NAME"
                fi
            fi
        done
        info "Audio outputs:"
        if command -v pactl &>/dev/null; then
            pactl list sinks 2>/dev/null | grep -E "^\s*Description:" | sed 's/.*Description: /    /'
        fi
    fi

    # ── Final summary box ──
    echo -e "\n${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo
    echo -e "  ${BOLD}Start:${NC}   sudo systemctl start ${SERVICE_NAME}"
    echo -e "  ${BOLD}Web:${NC}     http://$(hostname -I | awk '{print $1}'):8080"
    echo -e "  ${BOLD}Logs:${NC}    journalctl -u ${SERVICE_NAME} -f"
    echo -e "  ${BOLD}Config:${NC}  $CONFIG_DIR/config.yaml"

    if [[ "$needs_relogin" == "true" ]]; then
        echo
        echo -e "  ${YELLOW}Reboot required for group membership changes.${NC}"
    fi
    echo

    # Offer to launch immediately
    read -p "Start OpenAuto Prodigy now? [Y/n] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        # systemd resolves groups fresh from /etc/group, so this works pre-relogin
        sudo systemctl start ${SERVICE_NAME}
        sleep 2
        if systemctl is-active --quiet ${SERVICE_NAME}; then
            ok "Started! Logs: journalctl -u ${SERVICE_NAME} -f"
        else
            fail "Service failed to start. Check: journalctl -u ${SERVICE_NAME} -n 30"
            journalctl -u ${SERVICE_NAME} -n 10 --no-pager 2>/dev/null || true
        fi
    fi
}

# ────────────────────────────────────────────────────
# Main
# ────────────────────────────────────────────────────
main() {
    print_header
    prime_sudo

    check_system          # 1
    install_dependencies  # 2
    setup_hardware        # 3
    build_project         # 4
    generate_config       # 5
    configure_network     # 6
    configure_labwc       # 7 (silent — no step header)
    create_service        # 8
    create_web_service    #   (part of step 8)
    create_system_service #   (part of step 8)
    run_diagnostics       # 9
}

main "$@"
exit
}
