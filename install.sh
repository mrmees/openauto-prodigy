#!/usr/bin/env bash
#
# OpenAuto Prodigy — Interactive Install Script
# Targets: Raspberry Pi OS Trixie (Debian 13) on RPi 4
#
# Usage: bash install.sh
#    or: bash install.sh --mode prebuilt
#    or: bash install.sh --list-prebuilt
#
set -euo pipefail

# ERR trap is set after TUI detection in main(). Lifecycle traps are installed
# before source-mode mutation for both privileged and unprivileged execution.

# Wrap entire script in a block so bash reads it all into memory before
# executing. This prevents git pull from modifying the script mid-run.
{

print_usage() {
    cat <<'USAGE'
Usage:
  bash install.sh [options]

Options:
  -v, --verbose            Show full CMake configure output.
  --mode <source|prebuilt>  Install mode. If omitted, interactive prompt is shown.
  --list-prebuilt           List available prebuilt GitHub release assets and exit.
  --prebuilt-index <N>      Auto-select prebuilt option N (1-based, with --mode prebuilt).
  --help, -h                Show this help.

Modes:
  source   Build locally from source (existing behavior).
  prebuilt Download a precompiled release from GitHub and run install-prebuilt.sh.
USAGE
}

# Flags
VERBOSE=false
INSTALL_MODE=""
LIST_PREBUILT_ONLY=false
PREBUILT_INDEX=""
PREBUILT_ROWS=()

while [[ "${1:-}" == -* ]]; do
    case "$1" in
        -v|--verbose) VERBOSE=true; shift ;;
        --mode)
            [[ $# -ge 2 ]] || { echo "Error: --mode requires a value"; exit 1; }
            INSTALL_MODE="$2"; shift 2 ;;
        --list-prebuilt) LIST_PREBUILT_ONLY=true; shift ;;
        --prebuilt-index)
            [[ $# -ge 2 ]] || { echo "Error: --prebuilt-index requires a value"; exit 1; }
            PREBUILT_INDEX="$2"; shift 2 ;;
        --help|-h) print_usage; exit 0 ;;
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

GITHUB_RELEASES_API="${OAP_GITHUB_API_URL:-https://api.github.com/repos/mrmees/openauto-prodigy/releases?per_page=20}"
PREBUILT_ASSET_REGEX='^openauto-prodigy-prebuilt-.*-pi4-aarch64\.tar\.gz$'
LEGACY_PREBUILT_ASSET_REGEX='^openauto-prodigy-prebuilt-.*\.tar\.gz$'
INSTALL_DIR=""
CONFIG_DIR="$HOME/.openauto"
SERVICE_NAME="openauto-prodigy"

# Invocation-owned lifecycle state. An owned process-group leader remains
# unreaped until _wait_owned_command() or installer_cleanup() clears the state,
# preventing a recycled PID from becoming a cleanup target.
OWNED_GROUP_LEADER_PID=""
SUDO_KEEPALIVE_GROUP_PID=""
CLEANUP_RAN=false
APP_SERVICE_RESTORE_REQUIRED=false
APP_SERVICE_STOPPED=false

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
SCREEN_SIZE="7.0"

# Simple mode header (used when TUI is disabled)
print_header() {
    # ctest/non-interactive environments may not define TERM; avoid failing.
    if [[ -n "${TERM:-}" ]] && [ -t 1 ]; then
        clear || true
    fi
    echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  OpenAuto Prodigy — Installer${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n"
}

# ────────────────────────────────────────────────────
# TUI — Terminal User Interface
# ────────────────────────────────────────────────────

# Detect unicode support (braille spinner vs ASCII fallback)
if printf '\u2800' 2>/dev/null | grep -q '⠀' 2>/dev/null; then
    SPINNER_CHARS='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    CHECK='✓'
    CROSS='✗'
    PENDING='○'
else
    SPINNER_CHARS='|/-\'
    CHECK='+'
    CROSS='x'
    PENDING='.'
fi

# Step registry
STEP_NAMES=("System" "Dependencies" "Hardware" "Building" "Config" "Network" "Services" "Verify" "Optimize")
STEP_STATUS=(pending pending pending pending pending pending pending pending pending)
STEP_START=(0 0 0 0 0 0 0 0 0)
STEP_ELAPSED=(0 0 0 0 0 0 0 0 0)
STEP_DETAIL=("" "" "" "" "" "" "" "" "")
TOTAL_STEPS=${#STEP_NAMES[@]}
ACTIVE_STEP=-1
SPIN_TICK=0

# TUI state
TUI_MODE=false
TERM_COLS=80
TERM_ROWS=24
HEADER_ROWS=7
LOG_LINES=3
LAYOUT_MODE="small"
SPINNER_LOG=""
AGGREGATE_LOG=""
BODY_CURSOR=7

# Format elapsed seconds as M:SS
_fmt_elapsed() {
    local secs=$1
    printf '%d:%02d' $((secs / 60)) $((secs % 60))
}

# Append to aggregate debug log
_log_aggregate() {
    if [[ -n "$AGGREGATE_LOG" && -f "$AGGREGATE_LOG" ]]; then
        printf '[%s] [%s] %s\n' "$(date +%H:%M:%S)" "$1" "$2" >> "$AGGREGATE_LOG"
    fi
}

info()  { echo -e "${BLUE}[INFO]${NC} $*"; _log_aggregate "INFO" "$*"; }
ok()    { echo -e "${GREEN}[OK]${NC} $*"; _log_aggregate "OK" "$*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; _log_aggregate "WARN" "$*"; }
fail()  { echo -e "${RED}[FAIL]${NC} $*"; _log_aggregate "FAIL" "$*"; }

load_hardware_contracts() {
    local library="$INSTALL_DIR/config/installer/hardware-contracts.sh"

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

source_checkout_error() {
    fail "Source installation requires install.sh from a complete OpenAuto Prodigy git checkout."
    echo "Clone the repository, enter that checkout, and run: bash install.sh --mode source" >&2
    echo "Running from standard input or copying install.sh by itself is not supported." >&2
}

# Establish the one physical checkout that contains this running script. This
# is intentionally source-mode-only: release listing and prebuilt selection do
# not require a local source tree.
resolve_source_checkout() {
    local script_source="${BASH_SOURCE[0]:-}"
    local script_path script_dir checkout_root resolved_root

    if [[ -z "$script_source" ]]; then
        source_checkout_error
        return 1
    fi
    script_path=$(readlink -f -- "$script_source" 2>/dev/null) || {
        source_checkout_error
        return 1
    }
    [[ -f "$script_path" ]] || {
        source_checkout_error
        return 1
    }
    script_dir=$(dirname -- "$script_path")
    checkout_root=$(git -C "$script_dir" rev-parse --show-toplevel 2>/dev/null) || {
        source_checkout_error
        return 1
    }
    resolved_root=$(readlink -f -- "$checkout_root" 2>/dev/null) || {
        source_checkout_error
        return 1
    }

    if [[ "$script_path" != "$resolved_root/install.sh" ]] \
        || [[ ! -f "$resolved_root/CMakeLists.txt" ]] \
        || [[ ! -f "$resolved_root/src/main.cpp" ]] \
        || [[ ! -f "$resolved_root/src/CMakeLists.txt" ]] \
        || [[ ! -f "$resolved_root/libs/prodigy-oaa-protocol/CMakeLists.txt" ]] \
        || [[ ! -f "$resolved_root/config/systemd/bluetooth-compat.conf" ]] \
        || [[ ! -f "$resolved_root/web-config/server.py" ]]; then
        source_checkout_error
        return 1
    fi

    INSTALL_DIR="$resolved_root"
}

_start_owned_command() {
    local output_path="$1"
    shift

    if [[ -n "$OWNED_GROUP_LEADER_PID" ]]; then
        fail "Internal error: an installer-owned command is already active"
        return 1
    fi

    # Keep an invocation-owned session leader alive until the command has
    # finished and any same-session stragglers have been terminated. The
    # installer therefore never needs to signal a group after reaping its
    # ownership token.
    local supervisor='
        "$@" &
        command_pid=$!
        if wait "$command_pid"; then status=0; else status=$?; fi
        for member in $(ps -o pid= --sid "$$" 2>/dev/null); do
            [[ "$member" == "$$" ]] || kill -TERM "$member" 2>/dev/null || true
        done
        for attempt in {1..20}; do
            remaining=false
            for member in $(ps -o pid= --sid "$$" 2>/dev/null); do
                if [[ "$member" != "$$" ]]; then remaining=true; break; fi
            done
            [[ "$remaining" == false ]] && break
            sleep 0.1
        done
        for member in $(ps -o pid= --sid "$$" 2>/dev/null); do
            [[ "$member" == "$$" ]] || kill -KILL "$member" 2>/dev/null || true
        done
        exit "$status"
    '
    if [[ -n "$output_path" ]]; then
        setsid -- bash -c "$supervisor" oap-owned-command "$@" > "$output_path" 2>&1 &
    else
        setsid -- bash -c "$supervisor" oap-owned-command "$@" &
    fi
    OWNED_GROUP_LEADER_PID=$!
}

_wait_owned_command() {
    local leader_pid="$OWNED_GROUP_LEADER_PID"
    local status

    [[ -n "$leader_pid" ]] || return 0
    if wait "$leader_pid"; then
        status=0
    else
        status=$?
    fi
    # Clear immediately after the owned leader is reaped. Never retain a PID
    # that the kernel is free to recycle.
    OWNED_GROUP_LEADER_PID=""
    return "$status"
}

_terminate_owned_group() {
    local state_name="$1"
    local leader_pid="${!state_name:-}"
    local attempt

    [[ -n "$leader_pid" ]] || return 0
    if [[ "$leader_pid" =~ ^[1-9][0-9]*$ ]] && kill -0 "$leader_pid" 2>/dev/null; then
        kill -TERM -- "-$leader_pid" 2>/dev/null || true
        for attempt in {1..20}; do
            kill -0 -- "-$leader_pid" 2>/dev/null || break
            sleep 0.1
        done
        if kill -0 -- "-$leader_pid" 2>/dev/null; then
            kill -KILL -- "-$leader_pid" 2>/dev/null || true
        fi
    fi
    wait "$leader_pid" 2>/dev/null || true
    printf -v "$state_name" '%s' ""
}

restore_source_application() {
    if [[ "$APP_SERVICE_RESTORE_REQUIRED" != "true" || "$APP_SERVICE_STOPPED" != "true" ]]; then
        return 0
    fi

    APP_SERVICE_STOPPED=false
    if sudo systemctl start "${SERVICE_NAME}.service" >/dev/null 2>&1; then
        APP_SERVICE_RESTORE_REQUIRED=false
        return 0
    fi
    warn "Could not restore ${SERVICE_NAME}.service; start it manually after checking system logs."
    return 1
}

# Best-effort, idempotent cleanup. The caller's status is always returned so a
# cleanup problem cannot hide the primary command failure or signal.
installer_cleanup() {
    local primary_status="${1:-0}"

    if [[ "$CLEANUP_RAN" == "true" ]]; then
        return "$primary_status"
    fi
    CLEANUP_RAN=true

    _terminate_owned_group OWNED_GROUP_LEADER_PID
    restore_source_application || true
    _terminate_owned_group SUDO_KEEPALIVE_GROUP_PID
    rm -f "${SPINNER_LOG:-}" "${AGGREGATE_LOG:-}"
    tui_cleanup
    return "$primary_status"
}

_installer_exit_trap() {
    local status=$?
    trap - EXIT INT TERM HUP
    installer_cleanup "$status" || true
    exit "$status"
}

_installer_signal_trap() {
    local signal_number="$1"
    exit $((128 + signal_number))
}

install_lifecycle_traps() {
    trap '_installer_exit_trap' EXIT
    trap '_installer_signal_trap 2' INT
    trap '_installer_signal_trap 15' TERM
    trap '_installer_signal_trap 1' HUP
}

prepare_source_rebuild() {
    if systemctl is-active --quiet "${SERVICE_NAME}.service"; then
        info "Stopping ${SERVICE_NAME}.service for the in-place rebuild..."
        # Set restoration intent before invoking stop so an interrupt in the
        # narrow systemctl boundary still preserves the entry-active state.
        APP_SERVICE_RESTORE_REQUIRED=true
        APP_SERVICE_STOPPED=true
        sudo systemctl stop "${SERVICE_NAME}.service"
    fi
}

# Detect terminal capabilities and set layout
detect_terminal() {
    if [[ "$VERBOSE" == "true" ]] || ! [ -t 1 ] || [[ "${TERM:-dumb}" == "dumb" ]]; then
        TUI_MODE=false
        return
    fi

    TERM_COLS=$(tput cols 2>/dev/null) || { TUI_MODE=false; return; }
    TERM_ROWS=$(tput lines 2>/dev/null) || { TUI_MODE=false; return; }

    if [[ $TERM_ROWS -le 30 || $TERM_COLS -le 80 ]]; then
        LAYOUT_MODE="small"
        HEADER_ROWS=7    # 2 title + 4 steps (2-col) + 1 separator
        LOG_LINES=$(( TERM_ROWS - HEADER_ROWS - 1 ))
        if [[ $LOG_LINES -gt 4 ]]; then LOG_LINES=4; fi
        if [[ $LOG_LINES -lt 2 ]]; then LOG_LINES=2; fi
    elif [[ $TERM_ROWS -le 45 && $TERM_COLS -le 130 ]]; then
        LAYOUT_MODE="medium"
        HEADER_ROWS=11   # 2 title + 8 steps + 1 separator
        LOG_LINES=8
    else
        LAYOUT_MODE="large"
        HEADER_ROWS=11
        LOG_LINES=15
    fi

    TUI_MODE=true
    BODY_CURSOR=$HEADER_ROWS
    AGGREGATE_LOG=$(mktemp /tmp/oap-install-full-XXXXXX.log)

    tput civis 2>/dev/null || true  # hide cursor
    clear
}

# Restore terminal to normal state
tui_cleanup() {
    if [[ "$TUI_MODE" == "true" ]]; then
        printf '\033[r'                        # reset scroll region
        tput cnorm 2>/dev/null || true         # show cursor
        tput cup "$TERM_ROWS" 0 2>/dev/null || true  # cursor to bottom
    fi
}

# Draw a single step entry at current cursor position
_draw_step() {
    local idx="$1"
    local status="${STEP_STATUS[$idx]:-pending}"
    local name="${STEP_NAMES[$idx]:-}"

    case "$status" in
        pending)
            printf ' %b%s%b %-14s' "${CYAN}" "$PENDING" "${NC}" "$name"
            ;;
        active)
            local c="${SPINNER_CHARS:$((SPIN_TICK % ${#SPINNER_CHARS})):1}"
            printf ' %b%s%b %b%-14s%b' "${CYAN}" "$c" "${NC}" "${BOLD}" "$name" "${NC}"
            if [[ -n "${STEP_DETAIL[$idx]}" ]]; then
                printf ' %b%s%b' "${CYAN}" "${STEP_DETAIL[$idx]}" "${NC}"
            fi
            local elapsed=$(( SECONDS - ${STEP_START[$idx]} ))
            if [[ $elapsed -gt 2 ]]; then
                printf ' (%s)' "$(_fmt_elapsed $elapsed)"
            fi
            ;;
        done)
            local el="${STEP_ELAPSED[$idx]}"
            printf ' %b%s%b %-14s' "${GREEN}" "$CHECK" "${NC}" "$name"
            if [[ $el -gt 0 ]]; then
                printf ' %b(%s)%b' "${GREEN}" "$(_fmt_elapsed $el)" "${NC}"
            fi
            ;;
        fail)
            printf ' %b%s%b %-14s' "${RED}" "$CROSS" "${NC}" "$name"
            ;;
    esac
}

# Draw the fixed header (title + step checklist)
draw_header() {
    if [[ "$TUI_MODE" != "true" ]]; then return; fi

    # Title (rows 0-1)
    tput cup 0 0
    printf '  %b%bOpenAuto Prodigy — Installer%b' "${CYAN}" "${BOLD}" "${NC}"
    tput el
    tput cup 1 0
    printf '  %b━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%b' "${CYAN}" "${NC}"
    tput el

    if [[ "$LAYOUT_MODE" == "small" ]]; then
        # 2-column: steps 0-3 left, 4-7 right
        local col2=$((TERM_COLS / 2))
        for row in 0 1 2 3; do
            local y=$((2 + row))
            tput cup $y 1
            _draw_step $row
            tput cup $y $col2
            _draw_step $((row + 4))
            tput el
        done
        # Separator
        tput cup 6 0
        printf '  %b━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%b' "${CYAN}" "${NC}"
        tput el
    else
        # 1-column
        for i in "${!STEP_NAMES[@]}"; do
            tput cup $((2 + i)) 1
            _draw_step "$i"
            tput el
        done
        # Separator
        tput cup 10 0
        printf '  %b━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%b' "${CYAN}" "${NC}"
        tput el
    fi
}

# Draw log viewport (tail of current log file) below header
draw_log_viewport() {
    if [[ "$TUI_MODE" != "true" ]]; then return; fi

    local start_row=$BODY_CURSOR
    local available=$((TERM_ROWS - start_row - 1))
    local max_lines=$LOG_LINES
    if [[ $max_lines -gt $available ]]; then max_lines=$available; fi
    if [[ $max_lines -lt 1 ]]; then max_lines=1; fi
    local lines_drawn=0

    if [[ -n "$SPINNER_LOG" && -f "$SPINNER_LOG" ]]; then
        while IFS= read -r line; do
            tput cup $((start_row + lines_drawn)) 0
            printf '  %.*s' $((TERM_COLS - 4)) "$line"
            tput el
            lines_drawn=$((lines_drawn + 1))
        done < <(tail -n "$max_lines" "$SPINNER_LOG" 2>/dev/null)
    fi

    # Clear remaining viewport lines
    while [[ $lines_drawn -lt $max_lines ]]; do
        tput cup $((start_row + lines_drawn)) 0
        tput el
        lines_drawn=$((lines_drawn + 1))
    done
}

# Clear the body area below the header
clear_body() {
    if [[ "$TUI_MODE" != "true" ]]; then return; fi
    tput cup "$HEADER_ROWS" 0
    tput ed
    BODY_CURSOR=$HEADER_ROWS
}

# Update step status and redraw header
update_step() {
    local idx=$1
    local status=$2
    local detail="${3:-}"

    case "$status" in
        active)
            STEP_STATUS[$idx]="active"
            STEP_START[$idx]=$SECONDS
            STEP_DETAIL[$idx]="$detail"
            ACTIVE_STEP=$idx
            ;;
        done)
            STEP_STATUS[$idx]="done"
            STEP_ELAPSED[$idx]=$(( SECONDS - ${STEP_START[$idx]} ))
            STEP_DETAIL[$idx]=""
            ACTIVE_STEP=-1
            ;;
        fail)
            STEP_STATUS[$idx]="fail"
            STEP_ELAPSED[$idx]=$(( SECONDS - ${STEP_START[$idx]} ))
            STEP_DETAIL[$idx]=""
            ACTIVE_STEP=-1
            ;;
    esac

    if [[ "$TUI_MODE" == "true" ]]; then
        draw_header
        if [[ "$status" == "active" ]]; then
            clear_body
        fi
    else
        if [[ "$status" == "active" ]]; then
            echo -e "\n${CYAN}[$((idx + 1))/${TOTAL_STEPS}]${NC} ${BOLD}${STEP_NAMES[$idx]}${NC}"
        fi
    fi
}

# Switch to interactive mode (normal scrolling below header)
enter_interactive() {
    if [[ "$TUI_MODE" != "true" ]]; then return; fi
    # Set scroll region to body area (below any accumulated output)
    printf '\033[%d;%dr' "$((BODY_CURSOR + 1))" "$TERM_ROWS"
    # Position cursor at top of scroll region
    tput cup "$BODY_CURSOR" 0
    # Show cursor for user input
    tput cnorm 2>/dev/null || true
}

# Return from interactive mode
leave_interactive() {
    if [[ "$TUI_MODE" != "true" ]]; then return; fi
    # Reset scroll region to full screen
    printf '\033[r'
    # Hide cursor
    tput civis 2>/dev/null || true
    # Redraw header (may have been scrolled)
    draw_header
}

# Show last N lines of log on failure
show_error_tail() {
    local logfile=$1 lines=${2:-20}
    if [[ -f "$logfile" && -s "$logfile" ]]; then
        echo -e "\n${RED}── Last ${lines} lines of output ──${NC}"
        tail -n "$lines" "$logfile"
        echo -e "${RED}── End of error output ──${NC}"
    fi
}

# Save aggregate log to persistent location on failure
save_debug_log() {
    if [[ -n "$AGGREGATE_LOG" && -f "$AGGREGATE_LOG" && -s "$AGGREGATE_LOG" ]]; then
        local dest="${INSTALL_DIR}/install-debug-$(date +%Y%m%d-%H%M%S).log"
        mkdir -p "$(dirname "$dest")" 2>/dev/null || true
        cp "$AGGREGATE_LOG" "$dest" 2>/dev/null || true
        echo ""
        echo -e "${YELLOW}${BOLD}Debug log saved to:${NC} $dest"
        echo -e "${YELLOW}Share this file when reporting issues.${NC}"
    fi
}

# TUI-aware error handler
handle_error() {
    local line=$1 code=$2

    # Mark active step as failed
    if [[ $ACTIVE_STEP -ge 0 ]]; then
        update_step $ACTIVE_STEP fail
    fi

    # Exit TUI mode
    tui_cleanup

    echo -e "\n${RED}[CRASH]${NC} Script failed at line $line (exit code $code)"

    if [[ -n "$SPINNER_LOG" && -f "$SPINNER_LOG" ]]; then
        show_error_tail "$SPINNER_LOG"
    fi

    save_debug_log
}

# Run a command with spinner + log viewport
# Usage: run_with_spinner "Installing dependencies" apt install -y ...
run_with_spinner() {
    local label="$1"; shift

    if [[ "$VERBOSE" == "true" ]]; then
        info "$label"
        _start_owned_command "" "$@"
        if _wait_owned_command; then
            return 0
        else
            return $?
        fi
    fi

    SPINNER_LOG=$(mktemp /tmp/oap-install-XXXXXX.log)
    local start_time=$SECONDS
    local char_count=${#SPINNER_CHARS}

    _start_owned_command "$SPINNER_LOG" "$@"
    local cmd_pid="$OWNED_GROUP_LEADER_PID"

    if [[ "$TUI_MODE" == "true" ]]; then
        tput civis 2>/dev/null || true
        while kill -0 "$cmd_pid" 2>/dev/null; do
            SPIN_TICK=$((SPIN_TICK + 1))
            local elapsed=$((SECONDS - start_time))
            local c="${SPINNER_CHARS:$((SPIN_TICK % char_count)):1}"
            draw_header
            # Show current task label with spinner on the line below completed items
            tput cup "$BODY_CURSOR" 0
            printf '  %s %s (%s)' "$c" "$label" "$(_fmt_elapsed $elapsed)"
            tput el
            # Show log viewport below the label line
            local saved_cursor=$BODY_CURSOR
            BODY_CURSOR=$((BODY_CURSOR + 1))
            draw_log_viewport
            BODY_CURSOR=$saved_cursor
            sleep 0.15
        done
    else
        local spin_i=0
        while kill -0 "$cmd_pid" 2>/dev/null; do
            local elapsed=$((SECONDS - start_time))
            local c="${SPINNER_CHARS:$((spin_i % char_count)):1}"
            printf '\r  %s %s (%s)  ' "$c" "$label" "$(_fmt_elapsed $elapsed)"
            spin_i=$((spin_i + 1))
            sleep 0.1
        done
    fi

    local exit_code
    if _wait_owned_command; then
        exit_code=0
    else
        exit_code=$?
    fi
    local elapsed=$((SECONDS - start_time))

    # Append to aggregate log
    if [[ -n "$AGGREGATE_LOG" && -f "$SPINNER_LOG" ]]; then
        printf '\n=== %s (exit %d, %s) ===\n' "$label" "$exit_code" "$(_fmt_elapsed $elapsed)" >> "$AGGREGATE_LOG"
        cat "$SPINNER_LOG" >> "$AGGREGATE_LOG" 2>/dev/null || true
    fi

    if [[ $exit_code -eq 0 ]]; then
        if [[ "$TUI_MODE" == "true" ]]; then
            # Clear spinner viewport, print completion line, advance cursor
            tput cup "$BODY_CURSOR" 0; tput ed
            printf '  %b%s%b %s (%s)\n' "${GREEN}" "$CHECK" "${NC}" "$label" "$(_fmt_elapsed $elapsed)"
            BODY_CURSOR=$((BODY_CURSOR + 1))
            tput cup "$BODY_CURSOR" 0
        else
            echo -e "\r  ${GREEN}${CHECK}${NC} ${label} ($(_fmt_elapsed $elapsed))  "
        fi
        rm -f "$SPINNER_LOG"
        SPINNER_LOG=""
    else
        if [[ "$TUI_MODE" != "true" ]]; then
            echo -e "\r  ${RED}${CROSS}${NC} ${label} ($(_fmt_elapsed $elapsed))  "
        fi
        tui_cleanup
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
        _start_owned_command "" "$@"
        if _wait_owned_command; then
            return 0
        else
            return $?
        fi
    fi

    SPINNER_LOG=$(mktemp /tmp/oap-build-XXXXXX.log)
    local start_time=$SECONDS
    local char_count=${#SPINNER_CHARS}
    local last_pct=""

    _start_owned_command "$SPINNER_LOG" "$@"
    local cmd_pid="$OWNED_GROUP_LEADER_PID"

    if [[ "$TUI_MODE" == "true" ]]; then
        tput civis 2>/dev/null || true
        while kill -0 "$cmd_pid" 2>/dev/null; do
            SPIN_TICK=$((SPIN_TICK + 1))
            # Scrape cmake percentage from log
            local pct
            pct=$(sed -n 's/.*\[\s*\([0-9]*\)%\].*/\1/p' "$SPINNER_LOG" 2>/dev/null | tail -1) || true
            if [[ -n "$pct" ]]; then
                last_pct="$pct"
                if [[ $ACTIVE_STEP -ge 0 ]]; then
                    STEP_DETAIL[$ACTIVE_STEP]="${pct}%"
                fi
            fi
            draw_header
            draw_log_viewport
            sleep 0.25
        done
    else
        local spin_i=0
        while kill -0 "$cmd_pid" 2>/dev/null; do
            local elapsed=$((SECONDS - start_time))
            local c="${SPINNER_CHARS:$((spin_i % char_count)):1}"
            local pct
            pct=$(sed -n 's/.*\[\s*\([0-9]*\)%\].*/\1/p' "$SPINNER_LOG" 2>/dev/null | tail -1) || true
            if [[ -n "$pct" ]]; then last_pct="$pct"; fi
            if [[ -n "$last_pct" ]]; then
                printf '\r  %s %s (%s%%) (%s)  ' "$c" "$label" "$last_pct" "$(_fmt_elapsed $elapsed)"
            else
                printf '\r  %s %s (%s)  ' "$c" "$label" "$(_fmt_elapsed $elapsed)"
            fi
            spin_i=$((spin_i + 1))
            sleep 0.25
        done
    fi

    local exit_code
    if _wait_owned_command; then
        exit_code=0
    else
        exit_code=$?
    fi
    local elapsed=$((SECONDS - start_time))

    # Append to aggregate log
    if [[ -n "$AGGREGATE_LOG" && -f "$SPINNER_LOG" ]]; then
        printf '\n=== %s (exit %d, %s) ===\n' "$label" "$exit_code" "$(_fmt_elapsed $elapsed)" >> "$AGGREGATE_LOG"
        cat "$SPINNER_LOG" >> "$AGGREGATE_LOG" 2>/dev/null || true
    fi

    if [[ $exit_code -eq 0 ]]; then
        if [[ "$TUI_MODE" == "true" ]]; then
            tput cup "$BODY_CURSOR" 0; tput ed
            printf '  %b%s%b %s (100%%) (%s)\n' "${GREEN}" "$CHECK" "${NC}" "$label" "$(_fmt_elapsed $elapsed)"
            BODY_CURSOR=$((BODY_CURSOR + 1))
            tput cup "$BODY_CURSOR" 0
        else
            echo -e "\r  ${GREEN}${CHECK}${NC} ${label} (100%) ($(_fmt_elapsed $elapsed))  "
        fi
        rm -f "$SPINNER_LOG"
        SPINNER_LOG=""
    else
        if [[ "$TUI_MODE" != "true" ]]; then
            echo -e "\r  ${RED}${CROSS}${NC} ${label} ($(_fmt_elapsed $elapsed))  "
        fi
        tui_cleanup
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
    setsid -- bash -c 'while true; do sudo -n -v 2>/dev/null; sleep 50; done' &
    SUDO_KEEPALIVE_GROUP_PID=$!
}

fetch_prebuilt_release_rows() {
    local json_path="$1"
    python3 - "$json_path" "$PREBUILT_ASSET_REGEX" "$LEGACY_PREBUILT_ASSET_REGEX" <<'PY'
import json
import re
import sys

path = sys.argv[1]
pattern_new = re.compile(sys.argv[2])
pattern_legacy = re.compile(sys.argv[3])

with open(path, "r", encoding="utf-8") as f:
    releases = json.load(f)

for release in releases:
    if release.get("draft"):
        continue
    release_label = release.get("tag_name") or release.get("name") or "untagged"
    published = release.get("published_at") or ""
    for asset in release.get("assets", []):
        name = asset.get("name") or ""
        url = asset.get("browser_download_url") or ""
        if not url:
            continue
        if pattern_new.match(name) or pattern_legacy.match(name):
            print("\t".join([release_label, name, url, published]))
PY
}

load_prebuilt_rows() {
    local tmp_json
    tmp_json="$(mktemp -t oap-releases-XXXXXX.json)"
    if ! curl -fsSL "$GITHUB_RELEASES_API" -o "$tmp_json"; then
        rm -f "$tmp_json"
        fail "Failed to fetch release list from: $GITHUB_RELEASES_API"
        exit 1
    fi
    mapfile -t PREBUILT_ROWS < <(fetch_prebuilt_release_rows "$tmp_json")
    rm -f "$tmp_json"

    if [[ ${#PREBUILT_ROWS[@]} -eq 0 ]]; then
        fail "No prebuilt release assets found (expected pattern: openauto-prodigy-prebuilt-<version>-pi4-aarch64.tar.gz)."
        exit 1
    fi
}

print_prebuilt_rows() {
    echo -e "\n${CYAN}Available prebuilt releases${NC}\n"
    local i row tag asset url published published_date
    for i in "${!PREBUILT_ROWS[@]}"; do
        row="${PREBUILT_ROWS[$i]}"
        IFS=$'\t' read -r tag asset url published <<< "$row"
        published_date="${published%%T*}"
        [[ -n "$published_date" ]] || published_date="unknown-date"
        echo "  $((i+1))) $tag — $asset ($published_date)"
    done
    echo
}

install_from_github_prebuilt() {
    info "Preparing prebuilt install from GitHub releases..."
    load_prebuilt_rows
    print_prebuilt_rows

    local choice selected row tag asset url published
    if [[ -n "$PREBUILT_INDEX" ]]; then
        choice="$PREBUILT_INDEX"
    else
        read -p "Select prebuilt release [1]: " choice
        choice="${choice:-1}"
    fi

    if ! [[ "$choice" =~ ^[0-9]+$ ]]; then
        fail "Invalid selection: $choice"
        exit 1
    fi
    if (( choice < 1 || choice > ${#PREBUILT_ROWS[@]} )); then
        fail "Selection out of range: $choice"
        exit 1
    fi

    selected=$((choice - 1))
    row="${PREBUILT_ROWS[$selected]}"
    IFS=$'\t' read -r tag asset url published <<< "$row"

    local tmp_dir archive_path installer_path installer_root keep_tmp
    tmp_dir="$(mktemp -d -t oap-prebuilt-XXXXXX)"
    archive_path="$tmp_dir/$asset"

    info "Downloading $asset..."
    curl -fL "$url" -o "$archive_path"
    info "Extracting release package..."
    tar -xzf "$archive_path" -C "$tmp_dir"

    installer_path="$(find "$tmp_dir" -maxdepth 3 -type f -name install-prebuilt.sh | head -n1)"
    if [[ -z "$installer_path" ]]; then
        fail "Downloaded archive does not contain install-prebuilt.sh"
        warn "Temporary files kept at: $tmp_dir"
        exit 1
    fi

    installer_root="$(dirname "$installer_path")"
    info "Running prebuilt installer from: $installer_root"
    if (cd "$installer_root" && bash ./install-prebuilt.sh); then
        read -p "Keep downloaded release files at $tmp_dir? [y/N] " -n 1 -r keep_tmp
        echo
        if [[ $keep_tmp =~ ^[Yy]$ ]]; then
            info "Kept temporary files: $tmp_dir"
        else
            rm -rf "$tmp_dir"
            ok "Removed temporary files"
        fi
    else
        warn "Prebuilt install failed. Temporary files kept at: $tmp_dir"
        exit 1
    fi
}

choose_install_mode() {
    if [[ "$INSTALL_MODE" == "source" || "$INSTALL_MODE" == "prebuilt" ]]; then
        return
    fi
    if [[ -n "$INSTALL_MODE" ]]; then
        fail "Invalid --mode value: $INSTALL_MODE (expected source or prebuilt)"
        exit 1
    fi

    echo -e "\n${CYAN}── Installation Mode ──${NC}\n"
    echo "  1. Build locally from source (slower, best for development)"
    echo "  2. Download precompiled release from GitHub (recommended for users)"
    echo "  3. Exit"
    echo
    read -p "Select mode [1]: " MODE_CHOICE
    MODE_CHOICE="${MODE_CHOICE:-1}"

    case "$MODE_CHOICE" in
        1) INSTALL_MODE="source" ;;
        2) INSTALL_MODE="prebuilt" ;;
        3) info "Exiting."; exit 0 ;;
        *) fail "Invalid mode selection: $MODE_CHOICE"; exit 1 ;;
    esac
}

# ────────────────────────────────────────────────────
# Step 1: Check OS and architecture
# ────────────────────────────────────────────────────
check_system() {
    update_step 0 active
    enter_interactive

    if [[ ! -f /etc/os-release ]]; then
        fail "Cannot determine OS. /etc/os-release not found."
        exit 1
    fi

    source /etc/os-release

    ARCH=$(uname -m)
    MODEL=""
    if [[ -r /proc/device-tree/model ]]; then
        MODEL="$(tr -d '\0' < /proc/device-tree/model)"
    fi

    ok "OS: $PRETTY_NAME | $ARCH | $(uname -r)"
    if [[ -n "$MODEL" ]]; then
        info "Hardware: $MODEL"
    fi

    if [[ "${ID:-}" != "debian" && "${ID:-}" != "raspbian" ]]; then
        warn "Expected Debian/RPi OS family. Detected ID=${ID:-unknown}"
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || exit 1
    fi

    if [[ "$ARCH" != "aarch64" && "$ARCH" != "armv7l" ]]; then
        warn "This script targets Raspberry Pi (ARM). Detected: $ARCH"
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || exit 1
    fi

    if [[ -n "$MODEL" ]] && [[ "$MODEL" != *"Raspberry Pi 4"* ]]; then
        warn "Expected Raspberry Pi 4 hardware. Detected: $MODEL"
        read -p "Continue anyway? [y/N] " -n 1 -r
        echo
        [[ $REPLY =~ ^[Yy]$ ]] || exit 1
    fi

    if [[ "${VERSION_CODENAME:-}" != "trixie" ]]; then
        warn "Expected RPi OS Trixie (Debian 13). Detected: ${VERSION_CODENAME:-unknown}"
    fi

    leave_interactive
    update_step 0 done
}

# ────────────────────────────────────────────────────
# Step 2: Install apt dependencies
# ────────────────────────────────────────────────────
install_dependencies() {
    update_step 1 active

    local PACKAGES=(
        # Build tools
        cmake g++ git pkg-config ccache

        # Qt 6
        qt6-base-dev qt6-declarative-dev qt6-wayland
        qt6-connectivity-dev qt6-multimedia-dev qt6-websockets-dev
        qml6-module-qtquick-controls qml6-module-qtquick-layouts
        qml6-module-qtquick-window qml6-module-qtqml-workerscript
        qt6-webengine-dev qml6-module-qtwebengine

        # Boost
        libboost-system-dev libboost-log-dev

        # Protocol Buffers
        libprotobuf-dev protobuf-compiler

        # OpenSSL
        libssl-dev

        # FFmpeg (video decoding)
        libavformat-dev libavcodec-dev libavutil-dev

        # PipeWire (audio)
        libpipewire-0.3-dev libspa-0.2-dev
        pipewire-pulse pulseaudio-utils

        # YAML config
        libyaml-cpp-dev

        # Systemd integration (sd_notify)
        libsystemd-dev

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

        # Wayland socket wait (preflight uses inotifywait for zero-CPU blocking)
        inotify-tools

        # Splash screen (displays logo while app initializes)
        swaybg

        # USB media auto-mount (UsbMediaWatcher D-Bus client, runtime only —
        # no -dev headers needed)
        udisks2
    )

    run_with_spinner "Updating package lists" sudo apt-get update -q
    run_with_spinner "Installing ${#PACKAGES[@]} packages" sudo apt-get install -y -q "${PACKAGES[@]}"

    # Widevine CDM — enables DRM (EME) playback in the web runtime (spec
    # 2026-07-07-web-surface-strategy §Slice 1.1). Present in RPi OS repos,
    # absent on plain Debian: best-effort, never a hard dependency. Desktop
    # Chromium (if installed) is deliberately left untouched.
    if apt-cache show libwidevinecdm0 >/dev/null 2>&1; then
        run_with_spinner "Installing Widevine CDM (libwidevinecdm0)" \
            sudo apt-get install -y -q libwidevinecdm0 \
            || warn "libwidevinecdm0 install failed — DRM (Widevine) web content will not play"
    else
        warn "libwidevinecdm0 not found in APT repos — DRM (Widevine) web content will not play"
    fi

    update_step 1 done
}

# ────────────────────────────────────────────────────
# EDID screen size detection
# ────────────────────────────────────────────────────
# Probe EDID for physical screen size.
# Returns: diagonal inches (e.g. "7.0") on stdout, or empty string if unavailable.
probe_screen_size() {
    local edid_path=""

    # Find first connected display's EDID
    for card_dir in /sys/class/drm/card*-*/; do
        if [[ -f "${card_dir}status" ]]; then
            local status
            status=$(cat "${card_dir}status" 2>/dev/null || echo "")
            if [[ "$status" == "connected" ]] && [[ -f "${card_dir}edid" ]] && [[ -s "${card_dir}edid" ]]; then
                edid_path="${card_dir}edid"
                break
            fi
        fi
    done

    if [[ -z "$edid_path" ]]; then
        return
    fi

    # Try detailed timing descriptor first (mm precision).
    # First descriptor starts at byte 54. Check pixel clock (bytes 54-55)
    # is non-zero to confirm it's a timing descriptor, not a monitor descriptor.
    local pclk_lo pclk_hi
    pclk_lo=$(od -An -tu1 -j54 -N1 "$edid_path" 2>/dev/null | tr -d ' ')
    pclk_hi=$(od -An -tu1 -j55 -N1 "$edid_path" 2>/dev/null | tr -d ' ')

    if [[ -n "$pclk_lo" ]] && [[ -n "$pclk_hi" ]]; then
        local pclk=$(( (pclk_hi << 8) | pclk_lo ))
        if [[ $pclk -gt 0 ]]; then
            # Physical size at bytes 66-68 within first descriptor
            local b66 b67 b68
            b66=$(od -An -tu1 -j66 -N1 "$edid_path" 2>/dev/null | tr -d ' ')
            b67=$(od -An -tu1 -j67 -N1 "$edid_path" 2>/dev/null | tr -d ' ')
            b68=$(od -An -tu1 -j68 -N1 "$edid_path" 2>/dev/null | tr -d ' ')

            if [[ -n "$b66" ]] && [[ -n "$b67" ]] && [[ -n "$b68" ]]; then
                local w_mm=$(( ((b68 >> 4) << 8) | b66 ))
                local h_mm=$(( ((b68 & 0x0F) << 8) | b67 ))

                if [[ $w_mm -gt 0 ]] && [[ $h_mm -gt 0 ]]; then
                    awk "BEGIN {printf \"%.1f\", sqrt($w_mm*$w_mm + $h_mm*$h_mm) / 25.4}"
                    return
                fi
            fi
        fi
    fi

    # Fall back to basic display params (cm precision, bytes 21-22)
    local w_cm h_cm
    w_cm=$(od -An -tu1 -j21 -N1 "$edid_path" 2>/dev/null | tr -d ' ')
    h_cm=$(od -An -tu1 -j22 -N1 "$edid_path" 2>/dev/null | tr -d ' ')

    if [[ -n "$w_cm" ]] && [[ -n "$h_cm" ]] && [[ $w_cm -gt 0 ]] && [[ $h_cm -gt 0 ]]; then
        local w_mm=$((w_cm * 10))
        local h_mm=$((h_cm * 10))
        awk "BEGIN {printf \"%.1f\", sqrt($w_mm*$w_mm + $h_mm*$h_mm) / 25.4}"
        return
    fi
}

# ────────────────────────────────────────────────────
# Step 3: Interactive hardware setup
# ────────────────────────────────────────────────────
setup_hardware() {
    update_step 2 active
    enter_interactive

    # ── Touch device ──
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
        read -p "Press Enter to continue..." -r
    elif [[ ${#TOUCH_DEVS[@]} -eq 1 ]]; then
        TOUCH_DEV="${TOUCH_DEVS[0]}"
        NAME=$(cat "/sys/class/input/$(basename "$TOUCH_DEV")/device/name" 2>/dev/null || echo "unknown")
        ok "Touch: $TOUCH_DEV ($NAME)"
        read -p "Press Enter to continue..." -r
    else
        read -p "Touch device path [${TOUCH_DEVS[0]}]: " TOUCH_DEV
        TOUCH_DEV=${TOUCH_DEV:-${TOUCH_DEVS[0]}}
        ok "Touch: $TOUCH_DEV"
    fi

    # ── Screen size ──
    clear_body
    tput cup "$HEADER_ROWS" 0
    info "Detecting display..."
    DETECTED_SIZE=$(probe_screen_size)

    if [[ -n "$DETECTED_SIZE" ]]; then
        ok "Detected screen: ${DETECTED_SIZE}\""
        echo ""
        read -p "Press Enter to accept, or type a different size in inches: " USER_SIZE
        if [[ -n "$USER_SIZE" ]]; then
            if [[ "$USER_SIZE" =~ ^[0-9]+\.?[0-9]*$ ]] && awk "BEGIN {exit !($USER_SIZE > 0)}"; then
                SCREEN_SIZE="$USER_SIZE"
            else
                warn "Invalid input, using detected size"
                SCREEN_SIZE="$DETECTED_SIZE"
            fi
        else
            SCREEN_SIZE="$DETECTED_SIZE"
        fi
    else
        info "Could not detect screen size from EDID"
        echo ""
        read -p "Enter screen diagonal in inches [7.0]: " USER_SIZE
        if [[ -n "$USER_SIZE" ]]; then
            if [[ "$USER_SIZE" =~ ^[0-9]+\.?[0-9]*$ ]] && awk "BEGIN {exit !($USER_SIZE > 0)}"; then
                SCREEN_SIZE="$USER_SIZE"
            else
                warn "Invalid input, using default 7.0\""
                SCREEN_SIZE="7.0"
            fi
        fi
        # else SCREEN_SIZE stays at default "7.0"
    fi
    ok "Screen size: ${SCREEN_SIZE}\""

    # ── WiFi AP ──
    clear_body
    tput cup "$HEADER_ROWS" 0
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
        read -p "Press Enter to continue..." -r
    elif [[ ${#WIFI_INTERFACES[@]} -eq 1 ]]; then
        WIFI_IFACE="${WIFI_INTERFACES[0]}"
        ok "WiFi interface: $WIFI_IFACE"
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
        while true; do
            read -p "Device name [$DEVICE_NAME]: " USER_DEVICE_NAME
            USER_DEVICE_NAME=${USER_DEVICE_NAME:-$DEVICE_NAME}
            if [[ ${#USER_DEVICE_NAME} -gt 32 ]]; then
                warn "Name too long (${#USER_DEVICE_NAME} chars, max 32 for WiFi SSID)"
            elif [[ ! "$USER_DEVICE_NAME" =~ ^[a-zA-Z0-9_.-]+$ ]]; then
                warn "Name must be alphanumeric (a-z, 0-9, _ . - only)"
            else
                DEVICE_NAME="$USER_DEVICE_NAME"
                break
            fi
        done
        WIFI_SSID="$DEVICE_NAME"

        WIFI_PASS=$(head -c 12 /dev/urandom | base64 | tr -dc 'a-zA-Z0-9' | head -c 16)
        info "Generated random WiFi password (sent to phone automatically over BT)"

        AP_IP="10.0.0.1"

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
            COUNTRY_CODE=$(curl -fsS --max-time 3 https://ipinfo.io/country 2>/dev/null | tr -d '\r\n') || true
            if [[ ! "$COUNTRY_CODE" =~ ^[A-Z]{2}$ ]]; then
                COUNTRY_CODE=""
            else
                CC_SOURCE="IP geolocation"
            fi
        fi
        if [[ -z "$COUNTRY_CODE" ]]; then
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
        local normalized_country
        if ! normalized_country=$(oap_normalize_country_code "$COUNTRY_CODE"); then
            fail "Invalid country code '$COUNTRY_CODE'; enter exactly two ASCII letters."
            return 1
        fi
        COUNTRY_CODE="$normalized_country"

        sudo iw reg set "$COUNTRY_CODE" 2>/dev/null || true
        if [[ -f /etc/default/crda ]]; then
            sudo sed -i "s/^REGDOMAIN=.*/REGDOMAIN=$COUNTRY_CODE/" /etc/default/crda
        fi
        ok "Country code: $COUNTRY_CODE (5GHz WiFi regulatory domain)"
    fi

    # ── Audio output ──
    clear_body
    tput cup "$HEADER_ROWS" 0
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

        if [[ ${#AUDIO_SINKS[@]} -eq 1 ]]; then
            AUDIO_SINK="${AUDIO_SINKS[0]}"
            ok "Audio: ${AUDIO_DESCS[0]}"
            read -p "Press Enter to continue..." -r
        elif [[ ${#AUDIO_SINKS[@]} -gt 1 ]]; then
            echo
            read -p "Select audio output [1]: " AUDIO_CHOICE
            AUDIO_CHOICE=${AUDIO_CHOICE:-1}
            AUDIO_SINK="${AUDIO_SINKS[$((AUDIO_CHOICE-1))]}"
            ok "Audio: ${AUDIO_DESCS[$((AUDIO_CHOICE-1))]}"
        else
            warn "No audio sinks detected — will use PipeWire default"
            read -p "Press Enter to continue..." -r
        fi
    else
        warn "pactl not available — will use PipeWire default"
        read -p "Press Enter to continue..." -r
    fi

    # ── General settings ──
    clear_body
    tput cup "$HEADER_ROWS" 0
    TCP_PORT=5277

    echo
    read -p "Start OpenAuto Prodigy automatically on boot? [Y/n] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        AUTOSTART=false
    else
        AUTOSTART=true
    fi

    leave_interactive
    update_step 2 done
}

# ────────────────────────────────────────────────────
# Step 3b: Configure Bluetooth compatibility
# ────────────────────────────────────────────────────
configure_bluetooth() {
    local source="$INSTALL_DIR/config/systemd/bluetooth-compat.conf"
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
# Step 3c: Configure WiFi AP networking
# ────────────────────────────────────────────────────
configure_hostapd_lifecycle() {
    local app_source="$INSTALL_DIR/config/systemd/openauto-prodigy-hostapd.conf"
    local hostapd_source="$INSTALL_DIR/config/systemd/hostapd-openauto.conf"
    local app_destination="/etc/systemd/system/${SERVICE_NAME}.service.d/hostapd.conf"
    local hostapd_destination="/etc/systemd/system/hostapd.service.d/openauto.conf"

    if [[ -z "$WIFI_IFACE" ]]; then
        sudo rm -f "$app_destination" "$hostapd_destination"
        sudo systemctl daemon-reload
        return
    fi

    if [[ ! -f "$app_source" || ! -f "$hostapd_source" ]]; then
        fail "Missing hostapd lifecycle assets under $INSTALL_DIR/config/systemd"
        return 1
    fi

    sudo install -D -m 0644 "$app_source" "$app_destination"
    sudo install -D -m 0644 "$hostapd_source" "$hostapd_destination"
    sudo systemctl daemon-reload
}

configure_network() {
    enter_interactive

    if [[ -z "$WIFI_IFACE" ]]; then
        configure_hostapd_lifecycle
        warn "Skipping network configuration (no wireless interface)"
        leave_interactive
        return
    fi

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

    if [[ "$OAP_WIFI_BAND" == "5" ]]; then
        local wifi_standard="802.11a/n"
        [[ "$OAP_WIFI_USE_VHT" == "true" ]] && wifi_standard+="/ac"
        ok "WiFi: 5GHz ($wifi_standard), channel $OAP_WIFI_CHANNEL"
    else
        ok "WiFi: 2.4GHz (802.11g/n), channel $OAP_WIFI_CHANNEL"
    fi

    # Complete validation and rendering before the first managed network write.
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

    leave_interactive
}

# ────────────────────────────────────────────────────
# Step 3d: Configure labwc for multi-touch
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

# Step 3e: Suppress wf-panel-pi Bluetooth popups
# The panel's BT plugin shows "Connection successful" dialogs that draw over
# the app.  The systemd service kills the panel on start and restores it on
# clean stop, so no config changes are needed here — just log it.
suppress_panel_notifications() {
    ok "wf-panel-pi will be suspended while Prodigy runs (systemd unit handles lifecycle)"
}

# ────────────────────────────────────────────────────
# Step 4: Clone and build
# ────────────────────────────────────────────────────
build_project() {
    update_step 3 active

    cd "$INSTALL_DIR"
    # Clone proto submodule from dist branch (lightweight, proto definitions only)
    # The dist branch excludes research archive (~1600 files), keeping clone small
    if [[ ! -d "$INSTALL_DIR/libs/prodigy-oaa-protocol/proto/.git" ]]; then
        run_with_spinner "Initializing submodules" git clone --depth 1 -b dist \
            https://github.com/mrmees/open-android-auto.git \
            "$INSTALL_DIR/libs/prodigy-oaa-protocol/proto"
    else
        run_with_spinner "Updating submodules" git submodule update --init --recursive
    fi

    if [[ -f "$INSTALL_DIR/build/src/openauto-prodigy" ]]; then
        enter_interactive
        warn "OpenAuto Prodigy is already built."
        read -p "Rebuild? [Y/n] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Nn]$ ]]; then
            info "Skipping build."
            leave_interactive
            update_step 3 done
            return
        fi
        leave_interactive
    fi

    # Preserve the entry state across configure/link mutation. Cleanup holds
    # the service stopped until the installer finishes and restores it on both
    # success and every later failure path.
    prepare_source_rebuild
    mkdir -p build
    cd build

    run_with_spinner "Configuring CMake" cmake .. -Wno-dev \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DBUILD_TESTING=OFF -DOAA_BUILD_TESTS=OFF
    build_with_progress "Building" cmake --build . -j$(nproc)

    update_step 3 done
}

# ────────────────────────────────────────────────────
# Step 5: Generate config
# ────────────────────────────────────────────────────
generate_config() {
    enter_interactive
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
  screen_size: ${SCREEN_SIZE}
  width: 1024
  height: 600
  brightness: 80

touch:
  device: ""
YAML

    # Install default theme
    if [[ -f "$INSTALL_DIR/config/themes/default/theme.yaml" ]]; then
        cp "$INSTALL_DIR/config/themes/default/theme.yaml" "$CONFIG_DIR/themes/default/"
    fi

    # Install sample wallpapers (user can add/remove from ~/.openauto/wallpapers/)
    mkdir -p "$CONFIG_DIR/wallpapers"
    if [[ -d "$INSTALL_DIR/config/wallpapers" ]]; then
        cp -n "$INSTALL_DIR/config/wallpapers/"*.jpg "$CONFIG_DIR/wallpapers/" 2>/dev/null || true
    fi

    # Clock-sync polkit rule (allows timedatectl set-time/set-timezone/set-ntp
    # without sudo, for ClockSyncService's TimeReport clock stepping)
    if [[ -f "$INSTALL_DIR/config/clock-sync-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/clock-sync-polkit.rules" /etc/polkit-1/rules.d/50-openauto-time.rules
        ok "Clock-sync polkit rule installed"
    fi

    # BlueZ agent polkit rule (allows non-root pairing agent registration)
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

    # Ensure user is in required groups
    sudo usermod -aG bluetooth "$USER"  # BlueZ D-Bus access
    sudo usermod -aG input "$USER"      # evdev touch device access

    ok "Configuration written to $CONFIG_DIR/config.yaml"

    leave_interactive
}

# ────────────────────────────────────────────────────
# HFP mSBC codec fix (patched libspa-0.2-bluetooth)
# ────────────────────────────────────────────────────
# PipeWire's HFP LC3-SWB uplink encode is silent at the far end of a call;
# the patched deb drops LC3-SWB from the +BAC advertisement so the phone
# negotiates mSBC (bench-validated 2026-07-13). The deb is version-locked
# to the installed pipewire base and is NOT in git — build it with
# tools/pipewire-msbc/build.sh (see that README for the upgrade caveat:
# the hold survives 'apt upgrade'; a pipewire base upgrade breaks the
# held package's dependency LOUDLY, which is the cue to rebuild).
install_msbc_codec_fix() {
    # FIRST match wins — the repo build output must not be shadowed by a
    # stale hand-staged deb in $HOME.
    local deb="" d
    for d in "$INSTALL_DIR/tools/pipewire-msbc/out"/libspa-0.2-bluetooth_*+prodigy*_arm64.deb \
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
# ────────────────────────────────────────────────────
# Clear paired phones from BlueZ (stale pairings from previous installs)
# Identifies phones by Bluetooth Class of Device major class 0x02 (Phone).
# Leaves non-phone devices (keyboards, speakers, etc.) untouched.
# ────────────────────────────────────────────────────
clear_paired_phones() {
    local BT_DIR="/var/lib/bluetooth"
    if [[ ! -d "$BT_DIR" ]]; then return; fi

    # Find all paired phones
    local phones=()
    local phone_names=()
    while IFS= read -r info_file; do
        local class_hex
        class_hex=$(sed -n 's/^Class=0x\([0-9a-fA-F]*\)/\1/p' "$info_file" 2>/dev/null) || continue
        if [[ -z "$class_hex" ]]; then continue; fi
        # Major device class is bits 12-8: (class >> 8) & 0x1F
        local major=$(( (16#$class_hex >> 8) & 0x1F ))
        if [[ $major -eq 2 ]]; then
            local dev_dir
            dev_dir=$(dirname "$info_file")
            local dev_name
            dev_name=$(sed -n 's/^Name=//p' "$info_file" 2>/dev/null)
            if [[ -z "$dev_name" ]]; then dev_name="Unknown device"; fi
            phones+=("$dev_dir")
            phone_names+=("$dev_name")
        fi
    done < <(sudo find "$BT_DIR" -mindepth 3 -maxdepth 3 -name "info" 2>/dev/null)

    if [[ ${#phones[@]} -eq 0 ]]; then return; fi

    enter_interactive
    echo
    warn "Found ${#phones[@]} paired phone(s) from a previous installation:"
    for i in "${!phone_names[@]}"; do
        echo "    • ${phone_names[$i]}"
    done
    echo
    echo "  Stale pairings can prevent the first-run pairing flow from appearing."
    echo "  Non-phone devices (speakers, keyboards, etc.) will not be affected."
    echo
    read -p "  Remove paired phone(s)? [Y/n] " -n 1 -r
    echo
    if [[ ! "$REPLY" =~ ^[Nn]$ ]]; then
        for dev_dir in "${phones[@]}"; do
            sudo rm -rf "$dev_dir"
        done
        # Restart BlueZ to pick up the change
        sudo systemctl restart bluetooth 2>/dev/null || true
        ok "Removed ${#phones[@]} phone pairing(s)"
    else
        info "Keeping existing phone pairings"
    fi
    leave_interactive
}

# Step 6: Create pre-flight script and systemd service
# ────────────────────────────────────────────────────
create_preflight_script() {
    local source="$INSTALL_DIR/config/installer/openauto-preflight"

    if [[ ! -f "$source" ]]; then
        fail "Canonical application preflight not found: $source"
        return 1
    fi
    sudo install -D -m 0755 "$source" /usr/local/bin/openauto-preflight
    ok "Pre-flight script installed at /usr/local/bin/openauto-preflight"
}

# ────────────────────────────────────────────────────
install_restart_helper() {
    local SOURCE="$INSTALL_DIR/docs/pi-config/restart.sh"
    local DESTINATION="$INSTALL_DIR/restart.sh"

    if [[ ! -f "$SOURCE" ]]; then
        fail "Canonical restart helper not found: $SOURCE"
        return 1
    fi

    install -m 0755 "$SOURCE" "$DESTINATION"
    ok "Restart helper installed at $DESTINATION"
}

# ────────────────────────────────────────────────────
render_application_unit() {
    local template_path="$1"
    local service_user="$2"
    local user_id="$3"
    local install_root="$4"
    local escaped_user escaped_root

    [[ -f "$template_path" ]] || return 1
    [[ -n "$service_user" && "$service_user" != *$'\n'* ]] || return 1
    [[ "$user_id" =~ ^[0-9]+$ ]] || return 1
    [[ -n "$install_root" && "$install_root" == /* && "$install_root" != *$'\n'* ]] || return 1

    escaped_user=$(printf '%s' "$service_user" | sed 's/[\\&|]/\\&/g')
    escaped_root=$(printf '%s' "$install_root" | sed 's/[\\&|]/\\&/g')
    sed -e "s|@@USER@@|$escaped_user|g" \
        -e "s|@@USER_ID@@|$user_id|g" \
        -e "s|@@INSTALL_DIR@@|$escaped_root|g" \
        "$template_path"
}

create_service() {
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
        "/etc/systemd/system/${SERVICE_NAME}.service"
    rm -f "$rendered_unit"

    sudo systemctl daemon-reload

    if [[ "$AUTOSTART" == "true" ]]; then
        sudo systemctl enable --quiet ${SERVICE_NAME}
        ok "Service created and enabled (auto-start on boot)"
    else
        ok "Service created (manual start: sudo systemctl start ${SERVICE_NAME})"
    fi
}

# ────────────────────────────────────────────────────
# Step 6b: Create web config service
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
    sudo systemctl enable --now --quiet ${SERVICE_NAME}-web
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

    # --- Template rendering ---
    # Render systemd unit from template (single source of truth)
    local TEMPLATE="$SYS_DIR/../system-service/openauto-system.service.in"
    if [[ ! -f "$TEMPLATE" ]]; then
        TEMPLATE="$SYS_DIR/openauto-system.service.in"
    fi
    if [[ ! -f "$TEMPLATE" ]]; then
        TEMPLATE="$(dirname "$0")/system-service/openauto-system.service.in"
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
    sudo systemctl enable --quiet openauto-system

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
# Step 9: Diagnostics
# ────────────────────────────────────────────────────
run_diagnostics() {
    update_step 7 active
    enter_interactive

    warnings=()

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

    needs_relogin=false
    if ! id -nG "$USER" | grep -qw bluetooth; then
        warnings+=("User not yet in bluetooth group")
        needs_relogin=true
    fi
    if ! id -nG "$USER" | grep -qw input; then
        warnings+=("User not yet in input group")
        needs_relogin=true
    fi

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
                local properties=""
                if [[ -f "$PROPS_PATH" ]]; then
                    properties=$(cat "$PROPS_PATH" 2>/dev/null || true)
                fi
                if oap_input_properties_have_direct "$properties"; then
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

    leave_interactive
    update_step 7 done
}

# ────────────────────────────────────────────────────
# Step 10: System optimization
# ────────────────────────────────────────────────────
optimize_system() {
    update_step 8 active

    # Disable wait-online services — Pi typically boots without internet,
    # these just add 10-14s of timeout waiting for a connection that won't come
    run_with_spinner "Disabling network wait-online services" bash -c \
        'sudo systemctl disable systemd-networkd-wait-online.service 2>/dev/null; \
         sudo systemctl disable NetworkManager-wait-online.service 2>/dev/null; true'

    # Disable cloud-init — cloud VM provisioning, useless on a Pi
    run_with_spinner "Disabling cloud-init services" bash -c \
        'sudo systemctl disable cloud-init-local.service cloud-init-main.service \
         cloud-init-network.service cloud-final.service cloud-config.service 2>/dev/null; \
         sudo touch /etc/cloud/cloud-init.disabled 2>/dev/null; true'

    # Disable ModemManager — no cellular modem on a Pi head unit
    run_with_spinner "Disabling ModemManager" bash -c \
        'sudo systemctl disable ModemManager.service 2>/dev/null; true'

    update_step 8 done
}

finalize() {
    # Exit TUI mode for final summary
    tui_cleanup
    clear

    # ── Final summary ──
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo

    # Configuration summary
    echo -e "  ${BOLD}${CYAN}Configuration${NC}"
    echo -e "  ${BOLD}Device name:${NC}    $DEVICE_NAME  ${CYAN}(WiFi SSID + Bluetooth)${NC}"
    if [[ -n "$WIFI_IFACE" ]]; then
        echo -e "  ${BOLD}WiFi interface:${NC} $WIFI_IFACE"
        echo -e "  ${BOLD}AP IP:${NC}          $AP_IP"
        echo -e "  ${BOLD}Country code:${NC}   $COUNTRY_CODE"
    else
        echo -e "  ${BOLD}WiFi AP:${NC}        ${YELLOW}not configured${NC}"
    fi
    echo -e "  ${BOLD}TCP port:${NC}       $TCP_PORT"
    echo -e "  ${BOLD}Video:${NC}          480p @ ${VIDEO_FPS}fps"
    if [[ -n "${AUDIO_SINK:-}" ]]; then
        echo -e "  ${BOLD}Audio output:${NC}   $AUDIO_SINK"
    else
        echo -e "  ${BOLD}Audio output:${NC}   PipeWire default"
    fi
    if [[ -n "${TOUCH_DEV:-}" ]]; then
        echo -e "  ${BOLD}Touch device:${NC}   $TOUCH_DEV"
    else
        echo -e "  ${BOLD}Touch device:${NC}   auto-detect"
    fi
    echo -e "  ${BOLD}Auto-start:${NC}     $(if [[ "$AUTOSTART" == "true" ]]; then echo "yes"; else echo "no"; fi)"
    echo

    # Diagnostics warnings
    if [[ ${#warnings[@]} -gt 0 ]]; then
        for w in "${warnings[@]}"; do
            warn "$w"
        done
        echo
    fi

    # Quick reference
    echo -e "  ${BOLD}${CYAN}Quick Reference${NC}"
    echo -e "  ${BOLD}Start:${NC}    sudo systemctl start ${SERVICE_NAME}"
    echo -e "  ${BOLD}Stop:${NC}     sudo systemctl stop ${SERVICE_NAME}"
    echo -e "  ${BOLD}Logs:${NC}     journalctl -u ${SERVICE_NAME} -f"
    echo -e "  ${BOLD}Web:${NC}      http://$(hostname -I | awk '{print $1}'):8080"
    echo -e "  ${BOLD}Config:${NC}   $CONFIG_DIR/config.yaml"

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
    # Handle --list-prebuilt early (no TUI needed)
    if [[ "$LIST_PREBUILT_ONLY" == "true" ]]; then
        print_header
        load_prebuilt_rows
        print_prebuilt_rows
        exit 0
    fi

    # Choose install mode before TUI setup (prebuilt bypasses source flow entirely)
    print_header
    choose_install_mode

    if [[ "$INSTALL_MODE" == "prebuilt" ]]; then
        check_system
        install_from_github_prebuilt
        exit 0
    fi

    # Source build flow — full TUI
    resolve_source_checkout
    load_hardware_contracts
    install_lifecycle_traps
    detect_terminal
    trap 'handle_error $LINENO $?' ERR

    if [[ "$TUI_MODE" == "true" ]]; then
        draw_header
        enter_interactive
    fi

    prime_sudo

    if [[ "$TUI_MODE" == "true" ]]; then
        leave_interactive
    fi

    check_system          # Step 0: System
    install_dependencies  # Step 1: Dependencies
    setup_hardware        # Step 2: Hardware
    build_project         # Step 3: Building

    # Step 4: Config
    update_step 4 active
    generate_config
    install_msbc_codec_fix
    update_step 4 done

    # Step 5: Network
    update_step 5 active
    configure_bluetooth
    configure_network
    configure_labwc
    suppress_panel_notifications
    update_step 5 done

    # Step 6: Services
    update_step 6 active
    enter_interactive
    install_restart_helper
    create_preflight_script
    create_service
    create_web_service
    create_system_service
    leave_interactive
    update_step 6 done

    # Clean up stale phone pairings (survived from previous install)
    clear_paired_phones

    run_diagnostics       # Step 7: Verify
    optimize_system       # Step 8: Optimize
    finalize              # Final summary

    # Clean up aggregate log on success
    rm -f "$AGGREGATE_LOG"
}

run_source_lifecycle_test_action() {
    local action="$1"
    shift

    case "$action" in
        resolve-source)
            resolve_source_checkout
            printf 'INSTALL_DIR=%s\n' "$INSTALL_DIR"
            ;;
        owned-command)
            install_lifecycle_traps
            VERBOSE=true
            run_with_spinner "Lifecycle test command" "$@"
            ;;
        cleanup-idempotence)
            local requested_status="$1"
            install_lifecycle_traps
            installer_cleanup "$requested_status" || true
            installer_cleanup "$requested_status" || true
            return "$requested_status"
            ;;
        service-rebuild)
            install_lifecycle_traps
            prepare_source_rebuild
            "$@"
            ;;
        service-skip)
            install_lifecycle_traps
            ;;
        *)
            echo "Unknown source lifecycle test action: $action" >&2
            return 2
            ;;
    esac
}

if [[ -n "${OAP_INSTALL_TEST_ACTION:-}" ]]; then
    run_source_lifecycle_test_action "$OAP_INSTALL_TEST_ACTION" "$@"
else
    main "$@"
fi
exit
}
