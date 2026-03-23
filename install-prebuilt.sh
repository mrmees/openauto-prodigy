#!/usr/bin/env bash
#
# OpenAuto Prodigy — Prebuilt Installer
# Targets: Raspberry Pi OS Trixie (Debian 13) on RPi 4
#
# This installer deploys a prebuilt binary and runtime payload from:
#   ./payload/
#
set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PAYLOAD_DIR="$SCRIPT_DIR/payload"
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
    echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  OpenAuto Prodigy — Prebuilt Installer${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
}

info()  { echo -e "${BLUE}[INFO]${NC} $*"; }
ok()    { echo -e "${GREEN}[OK]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
fail()  { echo -e "${RED}[FAIL]${NC} $*"; }

require_payload() {
    info "Validating payload..."

    local required=(
        "$PAYLOAD_DIR/build/src/openauto-prodigy"
        "$PAYLOAD_DIR/config/themes/default/theme.yaml"
        "$PAYLOAD_DIR/system-service/openauto_system.py"
        "$PAYLOAD_DIR/web-config/server.py"
        "$PAYLOAD_DIR/restart.sh"
    )

    for path in "${required[@]}"; do
        if [[ ! -e "$path" ]]; then
            fail "Missing payload file: $path"
            fail "Extract the prebuilt archive and run install-prebuilt.sh from its root."
            exit 1
        fi
    done

    ok "Payload looks complete"
}

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
# Step 2: Install runtime dependencies
# ────────────────────────────────────────────────────
install_dependencies() {
    info "Installing runtime dependencies..."
    sudo apt update

    local PACKAGES=(
        # Qt 6 runtime/development packages (matches current project expectations)
        qt6-base-dev qt6-declarative-dev qt6-wayland
        qt6-connectivity-dev qt6-multimedia-dev
        qml6-module-qtquick-controls qml6-module-qtquick-layouts
        qml6-module-qtquick-window qml6-module-qtqml-workerscript

        # Runtime libs
        libboost-system-dev libboost-log-dev
        libprotobuf-dev
        libssl-dev
        libavcodec-dev libavutil-dev
        libpipewire-0.3-dev libspa-0.2-dev
        libyaml-cpp-dev

        # WiFi AP / Bluetooth / proxy
        hostapd
        bluez
        redsocks
        iptables

        # Kiosk session / boot splash
        swaybg

        # Web config + system service Python deps
        python3-flask
        python3-dbus-next python3-yaml
        python3-venv
    )

    sudo apt install -y "${PACKAGES[@]}"
    ok "Dependencies installed"
}

# ────────────────────────────────────────────────────
# Step 3: Interactive hardware setup
# ────────────────────────────────────────────────────
setup_hardware() {
    echo -e "\n${CYAN}── Hardware Configuration ──${NC}\n"

    # Touch device — filter for INPUT_PROP_DIRECT (touchscreens)
    info "Detecting touch devices..."
    TOUCH_DEVS=()
    for dev in /dev/input/event*; do
        if [[ -e "$dev" ]]; then
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

        read -p "Country code (for 5GHz) [US]: " COUNTRY_CODE
        COUNTRY_CODE=${COUNTRY_CODE:-US}
    fi

    # AA settings
    read -p "Android Auto TCP port [5277]: " TCP_PORT
    TCP_PORT=${TCP_PORT:-5277}

    read -p "Video FPS (30 or 60) [30]: " VIDEO_FPS
    VIDEO_FPS=${VIDEO_FPS:-30}

    ok "Hardware configuration complete"

    echo
    read -p "Start OpenAuto Prodigy automatically on boot? [y/N] " -n 1 -r
    echo
    AUTOSTART=false
    [[ $REPLY =~ ^[Yy]$ ]] && AUTOSTART=true
}

# ────────────────────────────────────────────────────
# Step 4: Deploy prebuilt payload
# ────────────────────────────────────────────────────
deploy_payload() {
    info "Deploying prebuilt files to $INSTALL_DIR..."

    if [[ -d "$INSTALL_DIR" ]]; then
        warn "Directory $INSTALL_DIR already exists."
        read -p "Overwrite prebuilt payload in this directory? [Y/n] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Nn]$ ]]; then
            fail "Aborting to avoid modifying existing install."
            exit 1
        fi
    fi

    mkdir -p "$INSTALL_DIR"

    rm -rf \
        "$INSTALL_DIR/build" \
        "$INSTALL_DIR/config" \
        "$INSTALL_DIR/system-service" \
        "$INSTALL_DIR/web-config" \
        "$INSTALL_DIR/restart.sh"

    cp -a "$PAYLOAD_DIR/build" "$INSTALL_DIR/"
    cp -a "$PAYLOAD_DIR/config" "$INSTALL_DIR/"
    cp -a "$PAYLOAD_DIR/system-service" "$INSTALL_DIR/"
    cp -a "$PAYLOAD_DIR/web-config" "$INSTALL_DIR/"
    cp "$PAYLOAD_DIR/restart.sh" "$INSTALL_DIR/restart.sh"

    chmod +x "$INSTALL_DIR/build/src/openauto-prodigy" "$INSTALL_DIR/restart.sh"
    ok "Prebuilt payload deployed"
}

# ────────────────────────────────────────────────────
# Step 5: Generate config
# ────────────────────────────────────────────────────
generate_config() {
    info "Generating configuration..."
    mkdir -p "$CONFIG_DIR/themes/default" "$CONFIG_DIR/plugins"

    cat > "$CONFIG_DIR/config.yaml" << YAML
# OpenAuto Prodigy Configuration
# Generated by install-prebuilt.sh on $(date -Iseconds)

connection:
  wifi_ap:
    interface: "${WIFI_IFACE:-wlan0}"
    ssid: "${WIFI_SSID:-$DEVICE_NAME}"
    password: "$WIFI_PASS"
  tcp_port: $TCP_PORT
  bt_name: "$DEVICE_NAME"
  auto_connect_aa: true

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

    if [[ -f "$INSTALL_DIR/config/themes/default/theme.yaml" ]]; then
        cp "$INSTALL_DIR/config/themes/default/theme.yaml" "$CONFIG_DIR/themes/default/"
    fi

    if [[ -f "$INSTALL_DIR/config/companion-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/companion-polkit.rules" /etc/polkit-1/rules.d/50-openauto-time.rules
        ok "Companion polkit rule installed"
    fi

    if [[ -f "$INSTALL_DIR/config/bluez-agent-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/bluez-agent-polkit.rules" /etc/polkit-1/rules.d/50-openauto-bluez.rules
        ok "BlueZ agent polkit rule installed"
    fi

    sudo usermod -aG bluetooth "$USER"
    sudo usermod -aG input "$USER"

    ok "Configuration written to $CONFIG_DIR/config.yaml"
}

# ────────────────────────────────────────────────────
# Step 6: Configure WiFi AP networking
# ────────────────────────────────────────────────────
configure_network() {
    if [[ -z "$WIFI_IFACE" ]]; then
        warn "Skipping network configuration (no wireless interface)"
        return
    fi

    info "Configuring WiFi AP on $WIFI_IFACE..."

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

    if [[ -f /etc/default/hostapd ]]; then
        sudo sed -i 's|^#\?DAEMON_CONF=.*|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd
    fi

    sudo systemctl unmask hostapd 2>/dev/null || true
    sudo systemctl enable hostapd
    sudo systemctl enable systemd-networkd

    ok "WiFi AP configured: SSID=$WIFI_SSID on $WIFI_IFACE ($AP_IP)"
}

# ────────────────────────────────────────────────────
# Step 7: Configure labwc for multi-touch
# ────────────────────────────────────────────────────
configure_labwc() {
    local LABWC_DIR="$HOME/.config/labwc"
    local RC_FILE="$LABWC_DIR/rc.xml"

    info "Configuring labwc for multi-touch..."
    mkdir -p "$LABWC_DIR"

    if [[ -f "$RC_FILE" ]]; then
        if grep -q 'mouseEmulation="yes"' "$RC_FILE"; then
            sed -i 's/mouseEmulation="yes"/mouseEmulation="no"/' "$RC_FILE"
            ok "labwc: mouseEmulation set to \"no\" (was \"yes\")"
        elif grep -q 'mouseEmulation="no"' "$RC_FILE"; then
            ok "labwc: mouseEmulation already \"no\""
        else
            warn "labwc: rc.xml exists but no mouseEmulation found — check manually"
        fi
    else
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
# Step 8: Create systemd service
# ────────────────────────────────────────────────────
create_service() {
    info "Creating systemd service..."

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
        sudo systemctl enable ${SERVICE_NAME}
        ok "Service created and enabled (auto-start on boot)"
    else
        ok "Service created (manual start: sudo systemctl start ${SERVICE_NAME})"
    fi
}

# ────────────────────────────────────────────────────
# Step 9: Create web config service
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
# Step 10: Create system service (Python daemon)
# ────────────────────────────────────────────────────
create_system_service() {
    local SYS_DIR="$INSTALL_DIR/system-service"
    local VENV_DIR="$SYS_DIR/.venv"

    if [[ ! -f "$SYS_DIR/openauto_system.py" ]]; then
        warn "System service not found at $SYS_DIR — skipping"
        return
    fi

    info "Setting up system service..."

    # --- Group management ---
    if ! getent group openauto >/dev/null 2>&1; then
        sudo groupadd openauto
        ok "Created openauto group"
    fi

    local GROUP_CHANGED=false
    if ! id -nG "$USER" | grep -qw openauto; then
        sudo usermod -aG openauto "$USER"
        ok "Added $USER to openauto group"
        GROUP_CHANGED=true
    fi

    # Create dedicated redsocks system user for transparent proxy owner-based exemption
    if ! id -u redsocks &>/dev/null; then
        info "Creating redsocks system user..."
        sudo useradd --system --no-create-home --shell /usr/sbin/nologin redsocks
        info "Created redsocks system user"
    else
        info "redsocks system user already exists"
    fi

    # --- Upgrade detection and migration ---
    local UNIT_PATH="/etc/systemd/system/openauto-system.service"
    local MIGRATING=false

    if [[ -f "$UNIT_PATH" ]]; then
        # Detect old unprivileged unit (has User= line that is NOT root)
        # NOTE: Use sed, not grep -oP -- Perl regex is not available on minimal Trixie
        local OLD_USER
        OLD_USER=$(sed -n 's/^User=//p' "$UNIT_PATH" 2>/dev/null || echo "")
        if [[ -n "$OLD_USER" && "$OLD_USER" != "root" ]]; then
            MIGRATING=true
            info "Migrating system service from User=$OLD_USER to User=root"

            # Stop old service cleanly
            sudo systemctl stop openauto-system 2>/dev/null || true

            # Clean stale iptables proxy state
            sudo iptables -t nat -D OUTPUT -p tcp -j OPENAUTO_PROXY 2>/dev/null || true
            sudo iptables -t nat -F OPENAUTO_PROXY 2>/dev/null || true
            sudo iptables -t nat -X OPENAUTO_PROXY 2>/dev/null || true
            info "Cleaned stale iptables proxy state"
        fi
    fi

    # --- Python venv setup ---
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

    # --- Template rendering ---
    # Render systemd unit from template (single source of truth)
    local TEMPLATE="$SYS_DIR/openauto-system.service.in"
    if [[ ! -f "$TEMPLATE" ]]; then
        TEMPLATE="$PAYLOAD_DIR/system-service/openauto-system.service.in"
    fi

    if [[ ! -f "$TEMPLATE" ]]; then
        warn "Service template not found — using inline fallback"
        sudo tee "$UNIT_PATH" > /dev/null << SERVICE
[Unit]
Description=OpenAuto Prodigy System Manager
Before=${SERVICE_NAME}.service
After=network.target bluetooth.target

[Service]
Type=notify
User=root
ExecStart=$PYTHON_PATH $SYS_DIR/openauto_system.py
ExecStopPost=-/usr/sbin/iptables -t nat -D OUTPUT -p tcp -j OPENAUTO_PROXY
ExecStopPost=-/usr/sbin/iptables -t nat -F OPENAUTO_PROXY
ExecStopPost=-/usr/sbin/iptables -t nat -X OPENAUTO_PROXY
WorkingDirectory=$SYS_DIR
RuntimeDirectory=openauto
Restart=always
RestartSec=2
PrivateTmp=yes
RestrictSUIDSGID=yes
RestrictRealtime=yes
LockPersonality=yes

[Install]
WantedBy=multi-user.target
SERVICE
    else
        sed -e "s|@@PYTHON_PATH@@|$PYTHON_PATH|g" \
            -e "s|@@SYS_DIR@@|$SYS_DIR|g" \
            "$TEMPLATE" | sudo tee "$UNIT_PATH" > /dev/null
    fi

    # --- Reload and enable ---
    sudo systemctl daemon-reload
    sudo systemctl enable openauto-system

    # --- Post-migration notice ---
    if [[ "$MIGRATING" == "true" ]]; then
        ok "System service migrated: now runs as root with restricted IPC socket"
    fi

    if [[ "$GROUP_CHANGED" == "true" ]]; then
        warn "NOTE: You were added to the 'openauto' group."
        warn "Group membership takes effect after logging out and back in (or rebooting)."
        warn "Until then, the Qt client may not be able to connect to the system service socket."
    fi

    ok "System service installed and enabled"
}

# ────────────────────────────────────────────────────
# Step 11: Create kiosk session
# ────────────────────────────────────────────────────
create_kiosk_session() {
    info "Creating kiosk session..."

    # --- Kiosk labwc config directory ---
    sudo mkdir -p /etc/openauto-kiosk/labwc

    # --- Kiosk rc.xml ---
    sudo tee /etc/openauto-kiosk/labwc/rc.xml > /dev/null << 'RCXML'
<?xml version="1.0"?>
<labwc_config>
  <touch deviceName="" mapToOutput="" mouseEmulation="no"/>
  <core>
    <decoration>server</decoration>
    <gap>0</gap>
  </core>
  <theme>
    <cornerRadius>0</cornerRadius>
    <keepBorder>no</keepBorder>
  </theme>
  <windowRules>
    <windowRule identifier="openauto*" serverDecoration="no">
      <action name="ToggleFullscreen"/>
    </windowRule>
  </windowRules>
</labwc_config>
RCXML
    ok "Kiosk labwc rc.xml written"

    # --- Kiosk autostart ---
    sudo tee /etc/openauto-kiosk/labwc/autostart > /dev/null << 'AUTOSTART'
# Kiosk session autostart
# Splash: show branded image immediately as first Wayland surface
swaybg -i /usr/share/openauto-prodigy/splash.png -m fill &

# The app launches via systemd service (WantedBy=graphical.target), not from here.
# The app dismisses the splash after its first frame renders (pkill swaybg).
AUTOSTART
    ok "Kiosk autostart written"

    # --- Kiosk environment ---
    sudo tee /etc/openauto-kiosk/labwc/environment > /dev/null << 'KIOSKENV'
QT_QPA_PLATFORM=wayland
XDG_CURRENT_DESKTOP=labwc
OPENAUTO_KIOSK=1
KIOSKENV
    ok "Kiosk environment written"

    # --- XDG session file ---
    sudo tee /usr/share/wayland-sessions/openauto-kiosk.desktop > /dev/null << 'DESKTOP'
[Desktop Entry]
Name=OpenAuto Kiosk
Comment=OpenAuto Prodigy kiosk session
Exec=/usr/bin/labwc -C /etc/openauto-kiosk/labwc
Type=Application
DesktopNames=labwc
DESKTOP
    ok "XDG session file written"

    # --- LightDM drop-in ---
    sudo mkdir -p /etc/lightdm/lightdm.conf.d
    sudo tee /etc/lightdm/lightdm.conf.d/50-openauto-kiosk.conf > /dev/null << LIGHTDM
[Seat:*]
autologin-user=$USER
autologin-session=openauto-kiosk
LIGHTDM
    ok "LightDM kiosk autologin configured"

    # --- AccountsService session entry (belt-and-suspenders) ---
    sudo mkdir -p /var/lib/AccountsService/users
    sudo tee "/var/lib/AccountsService/users/$USER" > /dev/null << 'ACCTS'
[User]
Session=openauto-kiosk
ACCTS
    ok "AccountsService session entry written"

    # --- Deploy splash image ---
    sudo mkdir -p /usr/share/openauto-prodigy
    if [[ -f "$PAYLOAD_DIR/assets/splash.png" ]]; then
        sudo cp "$PAYLOAD_DIR/assets/splash.png" /usr/share/openauto-prodigy/splash.png
        ok "Splash image deployed to /usr/share/openauto-prodigy/"
    elif [[ -f "$SCRIPT_DIR/assets/splash.png" ]]; then
        sudo cp "$SCRIPT_DIR/assets/splash.png" /usr/share/openauto-prodigy/splash.png
        ok "Splash image deployed to /usr/share/openauto-prodigy/"
    else
        warn "splash.png not found in payload — kiosk splash will show black background"
    fi

    ok "Kiosk session created"
}

# ────────────────────────────────────────────────────
# Step 12: Configure boot splash
# ────────────────────────────────────────────────────
configure_boot_splash() {
    info "Configuring boot splash..."

    # Boot splash is nice-to-have, not load-bearing.
    # All steps are wrapped in error handling — failures log warnings and continue.

    # --- Step 1: Check if rpi-splash-screen-support is available ---
    if ! apt-cache show rpi-splash-screen-support &>/dev/null; then
        warn "rpi-splash-screen-support not available — skipping boot splash"
        warn "Boot splash requires the Raspberry Pi apt repository."
        return
    fi

    # --- Step 2: Install the package ---
    if ! sudo apt-get install -y rpi-splash-screen-support 2>/dev/null; then
        warn "Failed to install rpi-splash-screen-support — skipping boot splash"
        return
    fi
    ok "rpi-splash-screen-support installed"

    # --- Step 3: Deploy TGA to /lib/firmware/logo.tga ---
    local TGA_SRC=""
    if [[ -f "$PAYLOAD_DIR/assets/splash.tga" ]]; then
        TGA_SRC="$PAYLOAD_DIR/assets/splash.tga"
    elif [[ -f "$SCRIPT_DIR/assets/splash.tga" ]]; then
        TGA_SRC="$SCRIPT_DIR/assets/splash.tga"
    fi

    if [[ -z "$TGA_SRC" ]]; then
        warn "splash.tga not found in payload — skipping boot splash"
        return
    fi

    sudo cp "$TGA_SRC" /lib/firmware/logo.tga
    ok "Boot splash TGA deployed to /lib/firmware/logo.tga"

    # --- Step 4: Run configure-splash ---
    if command -v configure-splash &>/dev/null; then
        if sudo configure-splash /lib/firmware/logo.tga 2>/dev/null; then
            ok "configure-splash completed"
        else
            warn "configure-splash failed — attempting manual fallback"
            # Manual fallback: create initramfs hook
            sudo tee /etc/initramfs-tools/hooks/splash-screen-hook.sh > /dev/null << 'HOOK'
#!/bin/sh
if [ -f /lib/firmware/logo.tga ]; then
    mkdir -p "${DESTDIR}/lib/firmware"
    cp /lib/firmware/logo.tga "${DESTDIR}/lib/firmware/logo.tga"
fi
HOOK
            sudo chmod +x /etc/initramfs-tools/hooks/splash-screen-hook.sh
            ok "Manual initramfs hook created"
        fi
    else
        warn "configure-splash not found — creating manual initramfs hook"
        sudo tee /etc/initramfs-tools/hooks/splash-screen-hook.sh > /dev/null << 'HOOK'
#!/bin/sh
if [ -f /lib/firmware/logo.tga ]; then
    mkdir -p "${DESTDIR}/lib/firmware"
    cp /lib/firmware/logo.tga "${DESTDIR}/lib/firmware/logo.tga"
fi
HOOK
        sudo chmod +x /etc/initramfs-tools/hooks/splash-screen-hook.sh
        ok "Manual initramfs hook created"
    fi

    # --- Step 5: Repair cmdline.txt ---
    # configure-splash strips 'quiet' and 'plymouth.ignore-serial-consoles'
    # and adds its own parameters. We need to re-add quiet boot params
    # and remove the Plymouth 'splash' parameter.
    if [[ -f /boot/firmware/cmdline.txt ]]; then
        local CMDLINE
        CMDLINE=$(cat /boot/firmware/cmdline.txt)

        # Remove Plymouth 'splash' parameter
        CMDLINE=$(echo "$CMDLINE" | sed 's/ splash / /g; s/^splash //; s/ splash$//')
        # Remove plymouth.ignore-serial-consoles if present
        CMDLINE=$(echo "$CMDLINE" | sed 's/ plymouth\.ignore-serial-consoles//g')

        # Add required boot parameters (only if not already present)
        for param in "quiet" "loglevel=0" "logo.nologo"; do
            if ! echo "$CMDLINE" | grep -q "$param"; then
                CMDLINE="$CMDLINE $param"
            fi
        done

        echo "$CMDLINE" | sudo tee /boot/firmware/cmdline.txt > /dev/null
        ok "cmdline.txt repaired"
    else
        warn "cmdline.txt not found at /boot/firmware/cmdline.txt — skipping cmdline repair"
    fi

    # --- Step 6: Mask Plymouth services ---
    sudo systemctl mask plymouth-start.service 2>/dev/null || true
    sudo systemctl mask plymouth-quit.service 2>/dev/null || true
    sudo systemctl mask plymouth-quit-wait.service 2>/dev/null || true
    ok "Plymouth services masked"

    # --- Step 7: Rebuild initramfs ---
    if sudo update-initramfs -u 2>/dev/null; then
        ok "Initramfs rebuilt with boot splash"
    else
        warn "update-initramfs failed — boot splash may not appear until next kernel update"
    fi

    # --- Verification ---
    local splash_ok=true
    if [[ ! -x /etc/initramfs-tools/hooks/splash-screen-hook.sh ]]; then
        warn "Initramfs hook missing or not executable"
        splash_ok=false
    fi
    if [[ ! -f /lib/firmware/logo.tga ]]; then
        warn "TGA not found at /lib/firmware/logo.tga"
        splash_ok=false
    fi
    if [[ -f /boot/firmware/cmdline.txt ]]; then
        if ! grep -q "fullscreen_logo=1" /boot/firmware/cmdline.txt; then
            warn "cmdline.txt missing fullscreen_logo=1"
            splash_ok=false
        fi
    fi

    if [[ "$splash_ok" == "true" ]]; then
        ok "Boot splash configured successfully"
    else
        warn "Boot splash partially configured — some checks failed"
        warn "Re-run the installer to retry boot splash setup"
    fi
}

# ────────────────────────────────────────────────────
# Step 13: Diagnostics
# ────────────────────────────────────────────────────
run_diagnostics() {
    echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  System Diagnostics${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"

    if [[ -x "$INSTALL_DIR/build/src/openauto-prodigy" ]]; then
        ok "Binary: $INSTALL_DIR/build/src/openauto-prodigy"
    else
        warn "Binary missing or not executable"
    fi

    if systemctl is-enabled openauto-system &>/dev/null; then
        ok "System service: enabled"
    else
        warn "System service: not installed"
    fi

    if [[ -f "$CONFIG_DIR/config.yaml" ]]; then
        ok "Config: $CONFIG_DIR/config.yaml"
    else
        warn "Config: not found"
    fi

    echo
    echo -e "  Start:  ${BLUE}sudo systemctl start ${SERVICE_NAME}${NC}"
    echo -e "  Web:    ${BLUE}http://$(hostname -I | awk '{print $1}'):8080${NC}"
    echo -e "  Logs:   ${BLUE}journalctl -u ${SERVICE_NAME} -f${NC}"
    echo -e "  Config: ${BLUE}$CONFIG_DIR/config.yaml${NC}"
    echo
}

main() {
    print_header
    require_payload
    check_system
    install_dependencies
    setup_hardware
    deploy_payload
    generate_config
    configure_network
    configure_labwc
    create_service
    create_web_service
    create_system_service
    run_diagnostics
}

main "$@"
