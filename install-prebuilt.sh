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
PAYLOAD_DIR="${OAP_PAYLOAD_DIR:-$SCRIPT_DIR/payload}"
INSTALL_DIR="${OAP_INSTALL_DIR:-$HOME/openauto-prodigy}"
CONFIG_DIR="${OAP_CONFIG_DIR:-$HOME/.openauto}"
SYSTEMD_UNIT_DIR="${OAP_SYSTEMD_UNIT_DIR:-/etc/systemd/system}"
PREFLIGHT_DEST="${OAP_PREFLIGHT_DEST:-/usr/local/bin/openauto-preflight}"
SERVICE_NAME="openauto-prodigy"

PREBUILT_TRANSACTION_STATE="idle"
PREBUILT_TRANSACTION_ROOT=""
PREBUILT_STAGE_DIR=""
PREBUILT_ROLLBACK_DIR=""
PREBUILT_APP_WAS_ACTIVE=false
PREBUILT_WEB_WAS_ACTIVE=false
PREBUILT_SYSTEM_WAS_ACTIVE=false
PREBUILT_APP_ENABLEMENT=not-found
PREBUILT_WEB_ENABLEMENT=not-found
PREBUILT_SYSTEM_ENABLEMENT=not-found
PREBUILT_CLEANUP_RAN=false

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

load_hardware_contracts() {
    local library="$PAYLOAD_DIR/config/installer/hardware-contracts.sh"

    if [[ ! -r "$library" ]]; then
        fail "Missing installer hardware contract library: $library"
        return 1
    fi
    # shellcheck source=config/installer/hardware-contracts.sh
    if ! source "$library"; then
        fail "Could not load installer hardware contract library: $library"
        return 1
    fi
}

require_payload() {
    info "Validating payload..."

    local required=(
        "$PAYLOAD_DIR/build/src/openauto-prodigy"
        "$PAYLOAD_DIR/config/themes/default/theme.yaml"
        "$PAYLOAD_DIR/config/systemd/bluetooth-compat.conf"
        "$PAYLOAD_DIR/config/systemd/openauto-prodigy-hostapd.conf"
        "$PAYLOAD_DIR/config/systemd/hostapd-openauto.conf"
        "$PAYLOAD_DIR/config/systemd/openauto-prodigy.service.in"
        "$PAYLOAD_DIR/config/installer/hardware-contracts.sh"
        "$PAYLOAD_DIR/config/installer/openauto-preflight"
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

    if [[ ! -x "$PAYLOAD_DIR/build/src/openauto-prodigy" \
        || ! -x "$PAYLOAD_DIR/restart.sh" \
        || ! -x "$PAYLOAD_DIR/config/installer/openauto-preflight" ]]; then
        fail "Payload executables have invalid permissions."
        exit 1
    fi

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
        qt6-connectivity-dev qt6-multimedia-dev qt6-websockets-dev
        qml6-module-qtquick-controls qml6-module-qtquick-layouts
        qml6-module-qtquick-window qml6-module-qtqml-workerscript
        qml6-module-qtwebengine

        # Runtime libs
        libboost-system-dev libboost-log-dev
        libprotobuf-dev
        libssl-dev
        libavcodec-dev libavutil-dev
        libpipewire-0.3-dev libspa-0.2-dev
        libyaml-cpp-dev

        # WiFi AP / Bluetooth / proxy
        hostapd
        rfkill
        bluez
        redsocks
        iptables

        # Web config + system service Python deps
        python3-flask
        python3-dbus-next python3-yaml
        python3-venv

        # Bounded, zero-CPU Wayland socket condition
        inotify-tools
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
            local properties=""
            if [[ -f "$PROPS_PATH" ]]; then
                properties=$(cat "$PROPS_PATH" 2>/dev/null || true)
            fi
            if oap_input_properties_have_direct "$properties"; then
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

        read -p "Country code (for 5GHz) [US]: " COUNTRY_CODE
        COUNTRY_CODE=${COUNTRY_CODE:-US}
        local normalized_country
        if ! normalized_country=$(oap_normalize_country_code "$COUNTRY_CODE"); then
            fail "Invalid country code '$COUNTRY_CODE'; enter exactly two ASCII letters."
            return 1
        fi
        COUNTRY_CODE="$normalized_country"
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
validate_prebuilt_paths() {
    [[ -n "$INSTALL_DIR" && "$INSTALL_DIR" == /* && "$INSTALL_DIR" != "/" ]] || {
        fail "Refusing unsafe install root: $INSTALL_DIR"
        return 1
    }
    [[ "$SYSTEMD_UNIT_DIR" == /* && "$PREFLIGHT_DEST" == /* ]] || {
        fail "Managed unit and preflight destinations must be absolute paths."
        return 1
    }
}

confirm_payload_replacement() {
    if [[ -d "$INSTALL_DIR" && "${OAP_PREBUILT_ASSUME_YES:-false}" != "true" ]]; then
        warn "Directory $INSTALL_DIR already exists."
        read -p "Overwrite prebuilt payload in this directory? [Y/n] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Nn]$ ]]; then
            fail "Aborting to avoid modifying existing install."
            exit 1
        fi
    fi
}

prebuilt_transaction_checkpoint() {
    local name="$1"
    if [[ "${OAP_PREBUILT_FAIL_AT:-}" == "$name" ]]; then
        fail "Injected prebuilt transaction failure at $name"
        return 97
    fi
}

stage_prebuilt_payload() {
    local parent
    validate_prebuilt_paths
    parent=$(dirname "$INSTALL_DIR")
    mkdir -p "$parent"
    PREBUILT_TRANSACTION_ROOT=$(mktemp -d "$parent/.oap-upgrade.XXXXXX")
    PREBUILT_STAGE_DIR="$PREBUILT_TRANSACTION_ROOT/stage"
    PREBUILT_ROLLBACK_DIR="$PREBUILT_TRANSACTION_ROOT/rollback"
    PREBUILT_TRANSACTION_STATE="staging"
    mkdir -p "$PREBUILT_STAGE_DIR" "$PREBUILT_ROLLBACK_DIR/payload" \
        "$PREBUILT_ROLLBACK_DIR/external"

    cp -a "$PAYLOAD_DIR/build" "$PREBUILT_STAGE_DIR/"
    cp -a "$PAYLOAD_DIR/config" "$PREBUILT_STAGE_DIR/"
    cp -a "$PAYLOAD_DIR/system-service" "$PREBUILT_STAGE_DIR/"
    cp -a "$PAYLOAD_DIR/web-config" "$PREBUILT_STAGE_DIR/"
    cp -a "$PAYLOAD_DIR/restart.sh" "$PREBUILT_STAGE_DIR/restart.sh"

    local required
    for required in \
        build/src/openauto-prodigy \
        config/installer/hardware-contracts.sh \
        config/installer/openauto-preflight \
        config/systemd/openauto-prodigy.service.in \
        system-service/openauto_system.py \
        web-config/server.py \
        restart.sh; do
        if [[ ! -e "$PREBUILT_STAGE_DIR/$required" ]]; then
            fail "Staged payload validation failed: missing $required"
            return 1
        fi
    done
    [[ -x "$PREBUILT_STAGE_DIR/build/src/openauto-prodigy" \
        && -x "$PREBUILT_STAGE_DIR/config/installer/openauto-preflight" \
        && -x "$PREBUILT_STAGE_DIR/restart.sh" ]] || {
        fail "Staged payload validation failed: invalid executable permissions"
        return 1
    }
    PREBUILT_TRANSACTION_STATE="staged"
}

backup_external_file() {
    local source="$1"
    local label="$2"
    local backup="$PREBUILT_ROLLBACK_DIR/external/$label"

    if sudo test -e "$source"; then
        sudo cp -a "$source" "$backup"
        touch "$backup.present"
    else
        touch "$backup.absent"
    fi
}

capture_prebuilt_service_state() {
    local state

    sudo systemctl is-active --quiet "${SERVICE_NAME}.service" \
        && PREBUILT_APP_WAS_ACTIVE=true || PREBUILT_APP_WAS_ACTIVE=false
    sudo systemctl is-active --quiet "${SERVICE_NAME}-web.service" \
        && PREBUILT_WEB_WAS_ACTIVE=true || PREBUILT_WEB_WAS_ACTIVE=false
    sudo systemctl is-active --quiet "openauto-system.service" \
        && PREBUILT_SYSTEM_WAS_ACTIVE=true || PREBUILT_SYSTEM_WAS_ACTIVE=false

    state=$(sudo systemctl is-enabled "${SERVICE_NAME}.service" 2>/dev/null || true)
    PREBUILT_APP_ENABLEMENT="${state:-not-found}"
    state=$(sudo systemctl is-enabled "${SERVICE_NAME}-web.service" 2>/dev/null || true)
    PREBUILT_WEB_ENABLEMENT="${state:-not-found}"
    state=$(sudo systemctl is-enabled "openauto-system.service" 2>/dev/null || true)
    PREBUILT_SYSTEM_ENABLEMENT="${state:-not-found}"
}

begin_prebuilt_transaction() {
    [[ "$PREBUILT_TRANSACTION_STATE" == "staged" ]] || {
        fail "Prebuilt payload was not staged before the stop boundary."
        return 1
    }

    capture_prebuilt_service_state
    backup_external_file "$SYSTEMD_UNIT_DIR/${SERVICE_NAME}.service" application-unit
    backup_external_file "$SYSTEMD_UNIT_DIR/${SERVICE_NAME}-web.service" web-unit
    backup_external_file "$SYSTEMD_UNIT_DIR/openauto-system.service" system-unit
    backup_external_file "$PREFLIGHT_DEST" preflight

    PREBUILT_TRANSACTION_STATE="active"
    [[ "$PREBUILT_WEB_WAS_ACTIVE" == "false" ]] \
        || sudo systemctl stop "${SERVICE_NAME}-web.service"
    [[ "$PREBUILT_APP_WAS_ACTIVE" == "false" ]] \
        || sudo systemctl stop "${SERVICE_NAME}.service"
    [[ "$PREBUILT_SYSTEM_WAS_ACTIVE" == "false" ]] \
        || sudo systemctl stop "openauto-system.service"
    prebuilt_transaction_checkpoint after-services-stopped
}

deploy_payload() {
    local relative live backup
    info "Deploying staged prebuilt files to $INSTALL_DIR..."
    [[ "$PREBUILT_TRANSACTION_STATE" == "active" ]] || return 1

    sudo mkdir -p "$INSTALL_DIR"
    for relative in build config system-service web-config restart.sh; do
        live="$INSTALL_DIR/$relative"
        backup="$PREBUILT_ROLLBACK_DIR/payload/$relative"
        if sudo test -e "$live" || sudo test -L "$live"; then
            mkdir -p "$(dirname "$backup")"
            touch "$backup.present"
            sudo mv "$live" "$backup"
        else
            touch "$backup.absent"
        fi
    done
    prebuilt_transaction_checkpoint after-live-retired

    for relative in build config system-service web-config restart.sh; do
        sudo mv "$PREBUILT_STAGE_DIR/$relative" "$INSTALL_DIR/$relative"
    done
    prebuilt_transaction_checkpoint after-new-payload

    sudo chmod +x "$INSTALL_DIR/build/src/openauto-prodigy" "$INSTALL_DIR/restart.sh"
    ok "Prebuilt payload deployed"
}

restore_external_file() {
    local destination="$1"
    local label="$2"
    local backup="$PREBUILT_ROLLBACK_DIR/external/$label"

    sudo rm -f "$destination"
    if [[ -f "$backup.present" ]]; then
        sudo mkdir -p "$(dirname "$destination")"
        sudo cp -a "$backup" "$destination"
    fi
}

restore_prebuilt_service_state() {
    if [[ "$PREBUILT_SYSTEM_WAS_ACTIVE" == "true" ]]; then
        sudo systemctl start "openauto-system.service"
    fi
    if [[ "$PREBUILT_APP_WAS_ACTIVE" == "true" ]]; then
        sudo systemctl start "${SERVICE_NAME}.service"
    fi
    if [[ "$PREBUILT_WEB_WAS_ACTIVE" == "true" ]]; then
        sudo systemctl start "${SERVICE_NAME}-web.service"
    fi

    # Wants= may activate a dependency that was inactive at entry. Restore the
    # exact observable set after starting the services that were active.
    if [[ "$PREBUILT_WEB_WAS_ACTIVE" == "false" ]] \
        && sudo systemctl is-active --quiet "${SERVICE_NAME}-web.service"; then
        sudo systemctl stop "${SERVICE_NAME}-web.service"
    fi
    if [[ "$PREBUILT_APP_WAS_ACTIVE" == "false" ]] \
        && sudo systemctl is-active --quiet "${SERVICE_NAME}.service"; then
        sudo systemctl stop "${SERVICE_NAME}.service"
    fi
    if [[ "$PREBUILT_SYSTEM_WAS_ACTIVE" == "false" ]] \
        && sudo systemctl is-active --quiet "openauto-system.service"; then
        sudo systemctl stop "openauto-system.service"
    fi
}

verify_prebuilt_service_state() {
    local service expected
    while read -r service expected; do
        if [[ "$expected" == "true" ]]; then
            if ! sudo systemctl is-active --quiet "$service"; then
                fail "Managed service did not regain its active state: $service"
                return 1
            fi
        elif sudo systemctl is-active --quiet "$service"; then
            fail "Managed service was unexpectedly activated: $service"
            return 1
        fi
    done <<EOF
openauto-system.service $PREBUILT_SYSTEM_WAS_ACTIVE
${SERVICE_NAME}.service $PREBUILT_APP_WAS_ACTIVE
${SERVICE_NAME}-web.service $PREBUILT_WEB_WAS_ACTIVE
EOF
}

clear_prebuilt_service_enablement() {
    local service
    for service in "openauto-system.service" "${SERVICE_NAME}.service" \
        "${SERVICE_NAME}-web.service"; do
        # Unit installation may replace a /dev/null mask, while enable writes
        # wants links outside the files backed up by this transaction. Clear
        # both persistent/runtime policy before restarting the prior active
        # set. Final masks are reconstructed only after those starts, which
        # also preserves the valid active-but-masked entry combination.
        sudo systemctl unmask --runtime "$service" >/dev/null 2>&1 || true
        sudo systemctl unmask "$service" >/dev/null 2>&1 || true
        sudo systemctl disable "$service" >/dev/null 2>&1 || true
    done
}

restore_prebuilt_service_enablement() {
    local service expected
    while read -r service expected; do
        case "$expected" in
            enabled)
                sudo systemctl enable "$service"
                ;;
            enabled-runtime)
                sudo systemctl enable --runtime "$service"
                ;;
            masked)
                sudo systemctl mask "$service"
                ;;
            masked-runtime)
                sudo systemctl mask --runtime "$service"
                ;;
            disabled|static|indirect|generated|transient|alias|not-found)
                ;;
            *)
                fail "Unsupported captured enablement state for $service: $expected"
                return 1
                ;;
        esac
    done <<EOF
openauto-system.service $PREBUILT_SYSTEM_ENABLEMENT
${SERVICE_NAME}.service $PREBUILT_APP_ENABLEMENT
${SERVICE_NAME}-web.service $PREBUILT_WEB_ENABLEMENT
EOF
}

verify_prebuilt_service_enablement() {
    local service expected actual
    while read -r service expected; do
        actual=$(sudo systemctl is-enabled "$service" 2>/dev/null || true)
        actual="${actual:-not-found}"
        if [[ "$actual" != "$expected" ]]; then
            fail "Managed service enablement changed: $service (expected $expected, got $actual)"
            return 1
        fi
    done <<EOF
openauto-system.service $PREBUILT_SYSTEM_ENABLEMENT
${SERVICE_NAME}.service $PREBUILT_APP_ENABLEMENT
${SERVICE_NAME}-web.service $PREBUILT_WEB_ENABLEMENT
EOF
}

rollback_prebuilt_transaction() {
    local relative live backup rollback_failed=false
    [[ "$PREBUILT_TRANSACTION_STATE" == "active" ]] || return 0
    warn "Rolling back the interrupted prebuilt upgrade..."

    for service in "${SERVICE_NAME}-web.service" "${SERVICE_NAME}.service" \
        "openauto-system.service"; do
        if sudo systemctl is-active --quiet "$service"; then
            sudo systemctl stop "$service" || rollback_failed=true
        fi
    done

    for relative in build config system-service web-config restart.sh; do
        live="$INSTALL_DIR/$relative"
        backup="$PREBUILT_ROLLBACK_DIR/payload/$relative"
        if [[ -f "$backup.present" && ( -e "$backup" || -L "$backup" ) ]]; then
            sudo rm -rf "$live" || rollback_failed=true
            sudo mkdir -p "$(dirname "$live")" || rollback_failed=true
            sudo mv "$backup" "$live" || rollback_failed=true
        elif [[ -f "$backup.present" && ! -e "$live" && ! -L "$live" ]]; then
            rollback_failed=true
        elif [[ -f "$backup.absent" ]]; then
            sudo rm -rf "$live" || rollback_failed=true
        fi
    done

    restore_external_file "$SYSTEMD_UNIT_DIR/${SERVICE_NAME}.service" application-unit \
        || rollback_failed=true
    restore_external_file "$SYSTEMD_UNIT_DIR/${SERVICE_NAME}-web.service" web-unit \
        || rollback_failed=true
    restore_external_file "$SYSTEMD_UNIT_DIR/openauto-system.service" system-unit \
        || rollback_failed=true
    restore_external_file "$PREFLIGHT_DEST" preflight || rollback_failed=true
    sudo systemctl daemon-reload || rollback_failed=true
    clear_prebuilt_service_enablement || rollback_failed=true
    restore_prebuilt_service_state || rollback_failed=true
    restore_prebuilt_service_enablement || rollback_failed=true
    verify_prebuilt_service_state || rollback_failed=true
    verify_prebuilt_service_enablement || rollback_failed=true

    if [[ "$rollback_failed" == "true" ]]; then
        PREBUILT_TRANSACTION_STATE="rollback-failed"
        warn "Rollback was incomplete; retained recovery material at $PREBUILT_TRANSACTION_ROOT"
        return 1
    fi

    PREBUILT_TRANSACTION_STATE="rolled-back"
    sudo rm -rf "$PREBUILT_TRANSACTION_ROOT"
    PREBUILT_TRANSACTION_ROOT=""
    warn "Previous managed payload and service state restored."
}

commit_prebuilt_transaction() {
    [[ "$PREBUILT_TRANSACTION_STATE" == "active" ]] || return 1
    restore_prebuilt_service_state
    prebuilt_transaction_checkpoint after-services-restored
    verify_prebuilt_service_state
    prebuilt_transaction_checkpoint after-readiness

    PREBUILT_TRANSACTION_STATE="committed"
    if ! sudo rm -rf "$PREBUILT_TRANSACTION_ROOT"; then
        warn "Upgrade committed, but temporary rollback material remains at $PREBUILT_TRANSACTION_ROOT"
    fi
    PREBUILT_TRANSACTION_ROOT=""
    ok "Prebuilt payload transaction committed"
}

prebuilt_installer_cleanup() {
    local primary_status="${1:-0}"
    if [[ "$PREBUILT_CLEANUP_RAN" == "true" ]]; then
        return "$primary_status"
    fi
    PREBUILT_CLEANUP_RAN=true

    if [[ "$PREBUILT_TRANSACTION_STATE" == "active" ]]; then
        rollback_prebuilt_transaction || true
    elif [[ ( "$PREBUILT_TRANSACTION_STATE" == "staging" \
        || "$PREBUILT_TRANSACTION_STATE" == "staged" ) \
        && -n "$PREBUILT_TRANSACTION_ROOT" ]]; then
        sudo rm -rf "$PREBUILT_TRANSACTION_ROOT" || true
        PREBUILT_TRANSACTION_ROOT=""
    fi
    return "$primary_status"
}

_prebuilt_exit_trap() {
    local status=$?
    trap - EXIT INT TERM HUP
    prebuilt_installer_cleanup "$status" || true
    exit "$status"
}

_prebuilt_signal_trap() {
    local signal_number="$1"
    exit $((128 + signal_number))
}

install_prebuilt_lifecycle_traps() {
    trap '_prebuilt_exit_trap' EXIT
    trap '_prebuilt_signal_trap 2' INT
    trap '_prebuilt_signal_trap 15' TERM
    trap '_prebuilt_signal_trap 1' HUP
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
YAML

    if [[ -f "$INSTALL_DIR/config/themes/default/theme.yaml" ]]; then
        cp "$INSTALL_DIR/config/themes/default/theme.yaml" "$CONFIG_DIR/themes/default/"
    fi

    if [[ -f "$INSTALL_DIR/config/clock-sync-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/clock-sync-polkit.rules" /etc/polkit-1/rules.d/50-openauto-time.rules
        ok "Clock-sync polkit rule installed"
    fi

    if [[ -f "$INSTALL_DIR/config/bluez-agent-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/bluez-agent-polkit.rules" /etc/polkit-1/rules.d/50-openauto-bluez.rules
        ok "BlueZ agent polkit rule installed"
    fi

    # udisks2 polkit rule (allows the service user to mount/unmount and
    # power off USB media without a password prompt)
    if [[ -f "$INSTALL_DIR/config/udisks-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/udisks-polkit.rules" /etc/polkit-1/rules.d/50-openauto-udisks.rules
        ok "udisks2 polkit rule installed"
    fi

    # WirePlumber rule: BT A2DP audio routes through the app EQ tap (falls
    # back to direct playback when the app is down).
    if [[ -f "$INSTALL_DIR/config/50-openauto-bt-eq.conf" ]]; then
        sudo mkdir -p /etc/wireplumber/wireplumber.conf.d
        sudo cp "$INSTALL_DIR/config/50-openauto-bt-eq.conf" /etc/wireplumber/wireplumber.conf.d/
        systemctl --user try-restart wireplumber 2>/dev/null || true
        ok "BT-EQ WirePlumber rule installed"
    fi

    sudo usermod -aG bluetooth "$USER"
    sudo usermod -aG input "$USER"

    ok "Configuration written to $CONFIG_DIR/config.yaml"
}

# ────────────────────────────────────────────────────
# Step 5b: HFP mSBC codec fix (patched libspa-0.2-bluetooth)
# ────────────────────────────────────────────────────
# PipeWire's HFP LC3-SWB uplink encode is silent at the far end of a call;
# the patched deb drops LC3-SWB from the +BAC advertisement so the phone
# negotiates mSBC (bench-validated 2026-07-13). The deb is version-locked
# to the installed pipewire base and is NOT in git — build it with
# tools/pipewire-msbc/build.sh (see that README for the upgrade caveat:
# the hold survives 'apt upgrade'; a pipewire base upgrade breaks the
# held package's dependency LOUDLY, which is the cue to rebuild).
install_msbc_codec_fix() {
    # Release payloads carry the deb under payload/pipewire-msbc (staged by
    # tools/package-prebuilt-release.sh); the other paths cover a repo build
    # output or a hand-staged copy. FIRST match wins — the shipped payload
    # must not be shadowed by a stale hand-staged deb in $HOME.
    local deb="" d
    for d in "$PAYLOAD_DIR/pipewire-msbc"/libspa-0.2-bluetooth_*+prodigy*_arm64.deb \
             "$INSTALL_DIR/tools/pipewire-msbc/out"/libspa-0.2-bluetooth_*+prodigy*_arm64.deb \
             "$HOME/pipewire-msbc"/libspa-0.2-bluetooth_*+prodigy*_arm64.deb; do
        if [[ -f "$d" ]]; then deb="$d"; break; fi
    done

    if dpkg -s libspa-0.2-bluetooth 2>/dev/null | grep -q '+prodigy'; then
        sudo apt-mark hold libspa-0.2-bluetooth >/dev/null
        ok "HFP mSBC codec fix already installed; apt hold confirmed"
        return 0
    fi

    if [[ -z "$deb" ]]; then
        warn "HFP mSBC codec fix: no patched libspa-0.2-bluetooth deb found."
        warn "  Without it the phone can negotiate LC3-SWB and the far end hears"
        warn "  NOTHING during calls. Build it: tools/pipewire-msbc/README.md"
        return 0
    fi

    # The deb pins Depends: libspa-0.2-modules (= stock base). Simulate first:
    # a base mismatch (Pi upgraded pipewire since the deb was built) must fail
    # loudly here instead of mixing plugin/module ABIs or removing the stack.
    # (|| true: a failed simulation is the expected probe result on mismatch —
    # without it, set -e would kill the installer instead of reaching the warn.)
    local sim
    sim=$(apt-get -s install "$deb" 2>/dev/null || true)
    if ! grep -q '^Inst libspa-0.2-bluetooth' <<<"$sim" || grep -q '^Remv' <<<"$sim"; then
        warn "HFP mSBC codec fix: $(basename "$deb") does not install cleanly"
        warn "  against the current pipewire base — rebuild it first"
        warn "  (tools/pipewire-msbc/README.md). Skipping; calls may negotiate"
        warn "  LC3-SWB (silent far-end mic)."
        return 0
    fi

    sudo apt install -y "$deb"
    sudo apt-mark hold libspa-0.2-bluetooth

    # Activate: a running audio stack keeps the OLD plugin loaded until
    # PipeWire restarts. Restart bluetooth first — audio restarts can race
    # BlueZ profile registration (RegisterProfile NotPermitted leaves HFP
    # silently dead). Best-effort: a reboot also activates it.
    sudo systemctl try-restart bluetooth 2>/dev/null || true
    systemctl --user try-restart pipewire pipewire-pulse wireplumber 2>/dev/null || true
    ok "HFP mSBC codec fix installed + held ($(basename "$deb"))"
    ok "  (active after the audio-stack restart just attempted, or after reboot)"
}

# ────────────────────────────────────────────────────
# Step 5c: Configure Bluetooth compatibility
# ────────────────────────────────────────────────────
configure_bluetooth() {
    local source="$PAYLOAD_DIR/config/systemd/bluetooth-compat.conf"
    local destination="/etc/systemd/system/bluetooth.service.d/override.conf"

    if [[ ! -f "$source" ]]; then
        fail "Missing BlueZ compatibility asset: $source"
        return 1
    fi

    sudo install -D -m 0644 "$source" "$destination"
    sudo systemctl daemon-reload
    sudo systemctl restart bluetooth
    ok "BlueZ SDP compatibility enabled"
}

# ────────────────────────────────────────────────────
# Step 6: Configure WiFi AP networking
# ────────────────────────────────────────────────────
configure_hostapd_lifecycle() {
    local app_source="$PAYLOAD_DIR/config/systemd/openauto-prodigy-hostapd.conf"
    local hostapd_source="$PAYLOAD_DIR/config/systemd/hostapd-openauto.conf"
    local app_destination="/etc/systemd/system/${SERVICE_NAME}.service.d/hostapd.conf"
    local hostapd_destination="/etc/systemd/system/hostapd.service.d/openauto.conf"

    if [[ -z "$WIFI_IFACE" ]]; then
        sudo rm -f "$app_destination" "$hostapd_destination"
        sudo systemctl daemon-reload
        return
    fi

    if [[ ! -f "$app_source" || ! -f "$hostapd_source" ]]; then
        fail "Missing hostapd lifecycle assets under $PAYLOAD_DIR/config/systemd"
        return 1
    fi

    sudo install -D -m 0644 "$app_source" "$app_destination"
    sudo install -D -m 0644 "$hostapd_source" "$hostapd_destination"
    sudo systemctl daemon-reload
}

configure_network() {
    if [[ -z "$WIFI_IFACE" ]]; then
        configure_hostapd_lifecycle
        warn "Skipping network configuration (no wireless interface)"
        return
    fi

    info "Configuring WiFi AP on $WIFI_IFACE..."

    local normalized_country
    if ! normalized_country=$(oap_normalize_country_code "$COUNTRY_CODE"); then
        fail "Invalid country code '$COUNTRY_CODE'; network configuration was not changed."
        return 1
    fi
    COUNTRY_CODE="$normalized_country"

    if ! sudo "${OAP_IW_BIN:-iw}" reg set "$COUNTRY_CODE" 2>/dev/null; then
        warn "Could not apply wireless regulatory domain $COUNTRY_CODE; probing current channel permissions."
    fi
    if ! oap_probe_wifi_contract "$WIFI_IFACE"; then
        fail "No usable WiFi AP channel was reported for $WIFI_IFACE; network configuration was not changed."
        return 1
    fi

    local network_config hostapd_config
    network_config=$(oap_render_networkd_config "$WIFI_IFACE" "$AP_IP") || {
        fail "Could not render network configuration; no managed network file was changed."
        return 1
    }
    hostapd_config=$(oap_render_hostapd_config \
        "$WIFI_IFACE" "$WIFI_SSID" "$WIFI_PASS" "$COUNTRY_CODE") || {
        fail "Could not render hostapd configuration; no managed network file was changed."
        return 1
    }

    configure_hostapd_lifecycle
    local network_tmp hostapd_tmp install_status=0
    network_tmp=$(mktemp)
    hostapd_tmp=$(mktemp)
    printf '%s\n' "$network_config" > "$network_tmp"
    printf '%s\n' "$hostapd_config" > "$hostapd_tmp"
    sudo install -D -m 0644 "$network_tmp" \
        /etc/systemd/network/10-openauto-ap.network || install_status=$?
    if [[ $install_status -eq 0 ]]; then
        sudo install -D -m 0644 "$hostapd_tmp" \
            /etc/hostapd/hostapd.conf || install_status=$?
    fi
    rm -f "$network_tmp" "$hostapd_tmp"
    [[ $install_status -eq 0 ]] || return "$install_status"

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
create_preflight_script() {
    local source="$INSTALL_DIR/config/installer/openauto-preflight"

    if [[ ! -f "$source" ]]; then
        fail "Canonical application preflight not found: $source"
        return 1
    fi
    sudo install -D -m 0755 "$source" "$PREFLIGHT_DEST"
    ok "Pre-flight script installed at $PREFLIGHT_DEST"
}

systemd_escape_absolute_path() {
    local path="$1"

    [[ -n "$path" && "$path" == /* \
        && "$path" != *$'\n'* && "$path" != *$'\r'* ]] || return 1
    if LC_ALL=C printf '%s' "$path" | grep -q '[[:cntrl:]]'; then
        return 1
    fi

    printf '%s' "$path" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' \
        -e 's/%/%%/g' -e 's/\$/$$/g'
}

systemd_escape_path_field() {
    local path="$1"

    [[ -n "$path" && "$path" == /* \
        && "$path" != *$'\n'* && "$path" != *$'\r'* ]] || return 1
    if LC_ALL=C printf '%s' "$path" | grep -q '[[:cntrl:]]'; then
        return 1
    fi

    printf '%s' "$path" | sed -e 's/%/%%/g'
}

render_application_unit() {
    local template_path="$1"
    local service_user="$2"
    local user_id="$3"
    local install_root="$4"
    local escaped_user systemd_root systemd_working_root
    local escaped_root escaped_working_root

    [[ -f "$template_path" ]] || return 1
    [[ -n "$service_user" && "$service_user" != *$'\n'* ]] || return 1
    [[ "$user_id" =~ ^[0-9]+$ ]] || return 1
    [[ -n "$install_root" && "$install_root" == /* && "$install_root" != *$'\n'* ]] || return 1

    systemd_root=$(systemd_escape_absolute_path "$install_root") || return 1
    systemd_working_root=$(systemd_escape_path_field "$install_root") || return 1
    escaped_user=$(printf '%s' "$service_user" | sed 's/[\\&|]/\\&/g')
    escaped_root=$(printf '%s' "$systemd_root" | sed 's/[\\&|]/\\&/g')
    escaped_working_root=$(printf '%s' "$systemd_working_root" | sed 's/[\\&|]/\\&/g')
    sed -e "s|@@USER@@|$escaped_user|g" \
        -e "s|@@USER_ID@@|$user_id|g" \
        -e "s|@@INSTALL_DIR@@|$escaped_root|g" \
        -e "s|@@INSTALL_WORKING_DIR@@|$escaped_working_root|g" \
        "$template_path"
}

render_system_service_unit() {
    local template_path="$1"
    local python_path="$2"
    local system_service_root="$3"
    local systemd_python systemd_root systemd_working_root
    local sed_python sed_root sed_working_root

    [[ -f "$template_path" ]] || return 1
    systemd_python=$(systemd_escape_absolute_path "$python_path") || return 1
    systemd_root=$(systemd_escape_absolute_path "$system_service_root") || return 1
    systemd_working_root=$(systemd_escape_path_field "$system_service_root") || return 1
    sed_python=$(printf '%s' "$systemd_python" | sed 's/[\\&|]/\\&/g')
    sed_root=$(printf '%s' "$systemd_root" | sed 's/[\\&|]/\\&/g')
    sed_working_root=$(printf '%s' "$systemd_working_root" | sed 's/[\\&|]/\\&/g')
    sed -e "s|@@PYTHON_PATH@@|$sed_python|g" \
        -e "s|@@SYS_DIR@@|$sed_root|g" \
        -e "s|@@SYS_WORKING_DIR@@|$sed_working_root|g" \
        "$template_path"
}

create_service() {
    info "Creating systemd service..."

    local USER_ID template_path rendered_unit
    USER_ID=$(id -u)
    template_path="$INSTALL_DIR/config/systemd/openauto-prodigy.service.in"
    rendered_unit=$(mktemp)
    if ! render_application_unit "$template_path" "$USER" "$USER_ID" "$INSTALL_DIR" \
        > "$rendered_unit"; then
        rm -f "$rendered_unit"
        fail "Could not render canonical application service template: $template_path"
        return 1
    fi
    sudo install -D -m 0644 "$rendered_unit" \
        "$SYSTEMD_UNIT_DIR/${SERVICE_NAME}.service"
    rm -f "$rendered_unit"

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

    local systemd_root systemd_working_root
    systemd_root=$(systemd_escape_absolute_path "$INSTALL_DIR") || {
        fail "Install path cannot be represented safely in the web service unit: $INSTALL_DIR"
        return 1
    }
    systemd_working_root=$(systemd_escape_path_field "$INSTALL_DIR") || return 1

    sudo tee "$SYSTEMD_UNIT_DIR/${SERVICE_NAME}-web.service" > /dev/null << SERVICE
[Unit]
Description=OpenAuto Prodigy Web Config
After=network.target ${SERVICE_NAME}.service

[Service]
Type=simple
User=$USER
WorkingDirectory=$systemd_working_root/web-config
ExecStart=/usr/bin/python3 "$systemd_root/web-config/server.py"
Restart=on-failure
RestartSec=5
Environment=OAP_WEB_HOST=0.0.0.0
Environment=OAP_WEB_PORT=8080

[Install]
WantedBy=multi-user.target
SERVICE

    sudo systemctl daemon-reload
    sudo systemctl enable ${SERVICE_NAME}-web
    ok "Web config service created (port 8080; prior active state preserved)"
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
    local UNIT_PATH="$SYSTEMD_UNIT_DIR/openauto-system.service"
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

    local systemd_python systemd_sys_dir systemd_working_sys_dir
    systemd_python=$(systemd_escape_absolute_path "$PYTHON_PATH") || {
        fail "Python path cannot be represented safely in the system service unit: $PYTHON_PATH"
        return 1
    }
    systemd_sys_dir=$(systemd_escape_absolute_path "$SYS_DIR") || {
        fail "Install path cannot be represented safely in the system service unit: $SYS_DIR"
        return 1
    }
    systemd_working_sys_dir=$(systemd_escape_path_field "$SYS_DIR") || return 1

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
ExecStart="$systemd_python" "$systemd_sys_dir/openauto_system.py"
ExecStopPost=-/usr/sbin/iptables -t nat -D OUTPUT -p tcp -j OPENAUTO_PROXY
ExecStopPost=-/usr/sbin/iptables -t nat -F OPENAUTO_PROXY
ExecStopPost=-/usr/sbin/iptables -t nat -X OPENAUTO_PROXY
WorkingDirectory=$systemd_working_sys_dir
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
        render_system_service_unit "$TEMPLATE" "$PYTHON_PATH" "$SYS_DIR" \
            | sudo tee "$UNIT_PATH" > /dev/null
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
# Step 11: Diagnostics
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
    load_hardware_contracts
    install_prebuilt_lifecycle_traps
    check_system
    install_dependencies
    setup_hardware
    validate_prebuilt_paths
    confirm_payload_replacement
    stage_prebuilt_payload
    begin_prebuilt_transaction
    deploy_payload
    generate_config
    install_msbc_codec_fix
    configure_bluetooth
    configure_network
    configure_labwc
    create_preflight_script
    prebuilt_transaction_checkpoint after-preflight
    create_service
    prebuilt_transaction_checkpoint after-application-unit
    create_web_service
    prebuilt_transaction_checkpoint after-web-unit
    create_system_service
    prebuilt_transaction_checkpoint after-system-unit
    sudo systemctl daemon-reload
    prebuilt_transaction_checkpoint after-daemon-reload
    commit_prebuilt_transaction
    run_diagnostics
}

run_prebuilt_transaction_test_action() {
    local assets_dir="${OAP_PREBUILT_TEST_ASSETS_DIR:-}"
    [[ -n "$assets_dir" ]] || {
        fail "OAP_PREBUILT_TEST_ASSETS_DIR is required for the transaction harness"
        return 2
    }

    install_prebuilt_lifecycle_traps
    require_payload
    validate_prebuilt_paths
    stage_prebuilt_payload
    begin_prebuilt_transaction
    deploy_payload

    sudo install -D -m 0755 "$assets_dir/preflight" "$PREFLIGHT_DEST"
    prebuilt_transaction_checkpoint after-preflight
    sudo install -D -m 0644 "$assets_dir/application-unit" \
        "$SYSTEMD_UNIT_DIR/${SERVICE_NAME}.service"
    if [[ "${OAP_PREBUILT_TEST_AUTOSTART:-false}" == "true" ]]; then
        sudo systemctl enable "${SERVICE_NAME}.service"
    fi
    prebuilt_transaction_checkpoint after-application-unit
    sudo install -D -m 0644 "$assets_dir/web-unit" \
        "$SYSTEMD_UNIT_DIR/${SERVICE_NAME}-web.service"
    sudo systemctl enable "${SERVICE_NAME}-web.service"
    prebuilt_transaction_checkpoint after-web-unit
    sudo install -D -m 0644 "$assets_dir/system-unit" \
        "$SYSTEMD_UNIT_DIR/openauto-system.service"
    sudo systemctl enable "openauto-system.service"
    prebuilt_transaction_checkpoint after-system-unit
    sudo systemctl daemon-reload
    prebuilt_transaction_checkpoint after-daemon-reload
    commit_prebuilt_transaction
}

if [[ "${OAP_PREBUILT_TEST_ACTION:-}" == "transaction" ]]; then
    run_prebuilt_transaction_test_action "$@"
else
    main "$@"
fi
