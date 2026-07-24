#!/usr/bin/env bash
# Shared installer-only hardware and network configuration contracts.
# This file is sourced by install.sh and install-prebuilt.sh after their
# respective source tree or payload has been validated.

# INPUT_PROP_DIRECT is bit 1 in the kernel input-property bitmap. Kernel sysfs
# exposes the bitmap as hexadecimal; inspecting the low nibble avoids shell
# integer overflow while preserving that ABI for arbitrarily wide values.
oap_input_properties_have_direct() {
    local properties="${1:-}"
    local low_nibble

    # Trim ASCII whitespace and require exactly one hexadecimal token.
    properties="${properties#"${properties%%[!$' \t\r\n']*}"}"
    properties="${properties%"${properties##*[!$' \t\r\n']}"}"
    [[ -n "$properties" && "$properties" =~ ^[0-9A-Fa-f]+$ ]] || return 1

    low_nibble="${properties: -1}"
    case "$low_nibble" in
        2|3|6|7|a|A|b|B|e|E|f|F) return 0 ;;
        *) return 1 ;;
    esac
}

oap_normalize_country_code() {
    local country="${1:-}"

    [[ "$country" =~ ^[A-Za-z]{2}$ ]] || return 1
    LC_ALL=C tr '[:lower:]' '[:upper:]' <<< "$country" | tr -d '\n'
}

# Selected WiFi contract. Call oap_select_wifi_contract or
# oap_probe_wifi_contract before rendering hostapd configuration.
OAP_WIFI_BAND=""
OAP_WIFI_HW_MODE=""
OAP_WIFI_CHANNEL=""
OAP_WIFI_USE_VHT=false

_oap_preferred_channel() {
    local available=" $1 "
    shift
    local preferred

    for preferred in "$@"; do
        if [[ "$available" == *" $preferred "* ]]; then
            printf '%s' "$preferred"
            return 0
        fi
    done
    return 1
}

# Parse `iw phy ... info` output. A channel is usable for AP selection only
# when its frequency entry is neither disabled nor marked no-IR. Because the
# generated hostapd contract does not enable DFS/802.11h, radar-detection
# channels are also excluded from 5 GHz selection. VHT support is associated
# with the same iw band as the selected 5 GHz channel.
oap_select_wifi_contract() {
    local phy_info="${1:-}"
    local line current_band=""
    local mhz channel restrictions
    local five_entries="" two_entries=""
    local selected_entry selected_band=""
    local -A band_has_vht=()

    OAP_WIFI_BAND=""
    OAP_WIFI_HW_MODE=""
    OAP_WIFI_CHANNEL=""
    OAP_WIFI_USE_VHT=false

    while IFS= read -r line; do
        if [[ "$line" =~ Band[[:space:]]+([0-9]+): ]]; then
            current_band="${BASH_REMATCH[1]}"
            continue
        fi
        if [[ -n "$current_band" && "$line" == *"VHT Capabilities"* ]]; then
            band_has_vht["$current_band"]=true
            continue
        fi
        # Trixie's iw prints frequencies as either `5180 MHz` or
        # `5180.0 MHz`. Accept only an optional exact `.0` suffix so a
        # malformed/non-integral token cannot be truncated into another band.
        if [[ "$line" =~ \*[[:space:]]+([0-9]+)(\.0)?[[:space:]]+MHz[[:space:]]+\[([0-9]+)\](.*)$ ]]; then
            mhz="${BASH_REMATCH[1]}"
            channel="${BASH_REMATCH[3]}"
            restrictions="${BASH_REMATCH[4]}"
            if [[ "$restrictions" =~ \(disabled\) ]] \
                || [[ "$restrictions" =~ \(no[[:space:]]+IR([,\)]) ]]; then
                continue
            fi
            if (( mhz >= 4900 && mhz < 6000 )); then
                [[ "$restrictions" == *"radar detection"* ]] && continue
                five_entries+=" ${channel}:${current_band}"
            elif (( mhz >= 2400 && mhz < 2500 )); then
                two_entries+=" ${channel}:${current_band}"
            fi
        fi
    done <<< "$phy_info"

    # Prefer common non-DFS 5 GHz channels, then any remaining usable 5 GHz
    # channel. Only fall back to 2.4 GHz when no usable 5 GHz choice exists.
    local five_channels="" two_channels="" entry
    for entry in $five_entries; do five_channels+=" ${entry%%:*}"; done
    for entry in $two_entries; do two_channels+=" ${entry%%:*}"; done

    if [[ -n "$five_entries" ]]; then
        OAP_WIFI_CHANNEL=$(_oap_preferred_channel "$five_channels" \
            36 40 44 48 149 153 157 161 165) \
            || OAP_WIFI_CHANNEL="${five_entries# }"
        OAP_WIFI_CHANNEL="${OAP_WIFI_CHANNEL%%:*}"
        for entry in $five_entries; do
            if [[ "${entry%%:*}" == "$OAP_WIFI_CHANNEL" ]]; then
                selected_entry="$entry"
                break
            fi
        done
        selected_band="${selected_entry#*:}"
        OAP_WIFI_BAND="5"
        OAP_WIFI_HW_MODE="a"
        if [[ "${band_has_vht[$selected_band]:-false}" == "true" ]]; then
            OAP_WIFI_USE_VHT=true
        fi
        return 0
    fi

    if [[ -n "$two_entries" ]]; then
        OAP_WIFI_CHANNEL=$(_oap_preferred_channel "$two_channels" 6 1 11) \
            || OAP_WIFI_CHANNEL="${two_entries# }"
        OAP_WIFI_CHANNEL="${OAP_WIFI_CHANNEL%%:*}"
        OAP_WIFI_BAND="2.4"
        OAP_WIFI_HW_MODE="g"
        OAP_WIFI_USE_VHT=false
        return 0
    fi

    return 1
}

oap_probe_wifi_contract() {
    local interface="${1:-}"
    local sys_class_net="${OAP_SYS_CLASS_NET_ROOT:-/sys/class/net}"
    local iw_bin="${OAP_IW_BIN:-iw}"
    local phy_path phy_name phy_info

    [[ -n "$interface" && "$interface" != *$'\n'* ]] || return 1
    phy_path="$sys_class_net/$interface/phy80211/name"
    [[ -r "$phy_path" ]] || return 1
    IFS= read -r phy_name < "$phy_path" || return 1
    [[ -n "$phy_name" && "$phy_name" != *[[:space:]]* ]] || return 1
    command -v "$iw_bin" >/dev/null 2>&1 || return 1
    phy_info=$("$iw_bin" phy "$phy_name" info 2>/dev/null) || return 1
    oap_select_wifi_contract "$phy_info"
}

oap_render_networkd_config() {
    local interface="${1:-}"
    local ap_ip="${2:-}"

    [[ -n "$interface" && "$interface" != *$'\n'* ]] || return 1
    [[ "$ap_ip" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || return 1
    cat <<EOF
[Match]
Name=$interface

[Network]
Address=${ap_ip}/24
DHCPServer=yes
ConfigureWithoutCarrier=yes

[DHCPServer]
PoolOffset=10
PoolSize=40
EmitDNS=no
EOF
}

oap_render_hostapd_config() {
    local interface="${1:-}"
    local ssid="${2:-}"
    local passphrase="${3:-}"
    local country="${4:-}"

    [[ -n "$interface" && "$interface" != *$'\n'* ]] || return 1
    [[ -n "$ssid" && "$ssid" != *$'\n'* ]] || return 1
    [[ ${#passphrase} -ge 8 && ${#passphrase} -le 63 && "$passphrase" != *$'\n'* ]] \
        || return 1
    country=$(oap_normalize_country_code "$country") || return 1
    [[ -n "$OAP_WIFI_HW_MODE" && -n "$OAP_WIFI_CHANNEL" ]] || return 1

    cat <<EOF
interface=$interface
driver=nl80211
ssid=$ssid
hw_mode=$OAP_WIFI_HW_MODE
channel=$OAP_WIFI_CHANNEL
ieee80211n=1
EOF
    if [[ "$OAP_WIFI_USE_VHT" == "true" ]]; then
        printf '%s\n' 'ieee80211ac=1'
    fi
    cat <<EOF
wmm_enabled=1
country_code=$country
ieee80211d=1
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=$passphrase
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
EOF
}
