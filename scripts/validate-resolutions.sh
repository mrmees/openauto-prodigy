#!/bin/bash
# validate-resolutions.sh -- Launch app at target resolutions for visual testing
#
# Modes:
#   ./scripts/validate-resolutions.sh              # Interactive: Xvfb + x11vnc per resolution
#   ./scripts/validate-resolutions.sh --screenshot  # Auto: Xvfb + screenshot per resolution
#   ./scripts/validate-resolutions.sh --native       # Direct launch (no Xvfb, for local display)
#
# Options:
#   -b, --binary PATH    Path to binary (default: build/src/openauto-prodigy)
#   -o, --output DIR     Screenshot output directory (default: /tmp/resolution-tests)
#   -r, --resolution WxH Test a single resolution instead of all
#   --vnc-port PORT      VNC port (default: 5900)

set -euo pipefail

BINARY="build/src/openauto-prodigy"
OUTPUT_DIR="/tmp/resolution-tests"
VNC_PORT=5900
MODE="interactive"
SINGLE_RES=""
XVFB_DISPLAY=":99"
XVFB_LOCK_FILE="${OAP_VALIDATE_XVFB_LOCK_FILE:-/tmp/.X99-lock}"
XVFB_SOCKET_FILE="${OAP_VALIDATE_XVFB_SOCKET_FILE:-/tmp/.X11-unix/X99}"
APP_PID=""
VNC_PID=""
XVFB_PID=""
CLEANUP_RUNNING=0

RESOLUTIONS=(
    "800x480:Pi Official Touchscreen"
    "1024x600:DFRobot (default)"
    "1280x720:720p"
    "1920x480:Ultrawide edge case"
    "480x800:Portrait edge case"
    "480x272:Tiny edge case"
)

usage() {
    sed -n '2,12p' "$0" | sed 's/^# \?//'
    exit 0
}

process_state() {
    local pid="$1"
    local key value

    [ -r "/proc/$pid/status" ] || return 1
    while read -r key value _; do
        if [ "$key" = "State:" ]; then
            printf '%s\n' "$value"
            return 0
        fi
    done < "/proc/$pid/status" 2>/dev/null
    return 1
}

pid_is_live() {
    local pid="$1"
    local state

    if [ -d "/proc/$pid" ]; then
        # An unreadable proc entry is still evidence of a live process. When
        # readable, zombies are stale because they cannot own an X display.
        state=$(process_state "$pid" 2>/dev/null) || return 0
        [ "$state" != "Z" ] && [ "$state" != "X" ]
        return
    fi
    kill -0 "$pid" 2>/dev/null
}

owned_child_is_running() {
    local pid="$1"
    local key value
    local parent_pid=""
    local state=""

    [ -r "/proc/$pid/status" ] || return 1
    while read -r key value _; do
        case "$key" in
            PPid:) parent_pid="$value" ;;
            State:) state="$value" ;;
        esac
    done < "/proc/$pid/status" 2>/dev/null

    [ "$parent_pid" = "$$" ] && [ "$state" != "Z" ] && [ "$state" != "X" ]
}

terminate_owned_child() {
    local pid="$1"

    [ -n "$pid" ] || return 0

    if owned_child_is_running "$pid"; then
        kill -TERM "$pid" 2>/dev/null || true
        for _ in {1..20}; do
            owned_child_is_running "$pid" || break
            sleep 0.05
        done
        if owned_child_is_running "$pid"; then
            kill -KILL "$pid" 2>/dev/null || true
            for _ in {1..20}; do
                owned_child_is_running "$pid" || break
                sleep 0.05
            done
        fi
    fi

    if owned_child_is_running "$pid"; then
        echo "Child PID $pid did not exit after TERM/KILL; leaving bounded cleanup." >&2
        return 0
    fi

    # The PID came from this shell's own `$!`, so wait can only reap our child.
    wait "$pid" 2>/dev/null || true
}

lock_pid() {
    local contents

    contents=$(<"$XVFB_LOCK_FILE") || return 1
    if [[ "$contents" =~ ^[[:space:]]*([1-9][0-9]*)[[:space:]]*$ ]]; then
        printf '%s\n' "${BASH_REMATCH[1]}"
        return 0
    fi
    return 1
}

x_socket_exists() {
    [ -e "$XVFB_SOCKET_FILE" ] || [ -S "$XVFB_SOCKET_FILE" ]
}

remove_owned_xvfb_lock() {
    local recorded_pid="$1"
    local recorded_lock_pid

    [ -e "$XVFB_LOCK_FILE" ] || return 0
    recorded_lock_pid=$(lock_pid 2>/dev/null) || return 0
    if [ "$recorded_lock_pid" = "$recorded_pid" ] && ! kill -0 "$recorded_pid" 2>/dev/null; then
        # Cleanup is best-effort: a lock-removal failure must not replace the
        # command status or signal that initiated cleanup.
        rm -f -- "$XVFB_LOCK_FILE" || true
    fi
}

cleanup() {
    [ "$CLEANUP_RUNNING" -eq 0 ] || return 0
    CLEANUP_RUNNING=1

    terminate_owned_child "$APP_PID"
    APP_PID=""
    terminate_owned_child "$VNC_PID"
    VNC_PID=""
    terminate_owned_child "$XVFB_PID"
    remove_owned_xvfb_lock "$XVFB_PID"
    XVFB_PID=""
}

on_exit() {
    local status="$1"

    trap - EXIT INT TERM HUP
    cleanup
    exit "$status"
}

on_signal() {
    local signal="$1"
    local fallback_status="$2"

    trap - EXIT INT TERM HUP
    cleanup
    kill -s "$signal" "$$" 2>/dev/null || exit "$fallback_status"
}

trap 'on_exit "$?"' EXIT
trap 'on_signal INT 130' INT
trap 'on_signal TERM 143' TERM
trap 'on_signal HUP 129' HUP

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help) usage ;;
        -b|--binary) BINARY="$2"; shift 2 ;;
        -o|--output) OUTPUT_DIR="$2"; shift 2 ;;
        -r|--resolution) SINGLE_RES="$2"; shift 2 ;;
        --vnc-port) VNC_PORT="$2"; shift 2 ;;
        --screenshot) MODE="screenshot"; shift ;;
        --native) MODE="native"; shift ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

if [ ! -x "$BINARY" ]; then
    echo "Binary not found: $BINARY"
    echo "Build first: cd build && cmake --build . -j\$(nproc)"
    exit 1
fi

# If single resolution, override the list
if [ -n "$SINGLE_RES" ]; then
    RESOLUTIONS=("${SINGLE_RES}:Custom")
fi

# Check dependencies for Xvfb modes
if [ "$MODE" != "native" ]; then
    MISSING=""
    command -v Xvfb >/dev/null 2>&1 || MISSING="xvfb"
    if [ "$MODE" = "interactive" ]; then
        command -v x11vnc >/dev/null 2>&1 || MISSING="${MISSING:+$MISSING }x11vnc"
    fi
    if [ "$MODE" = "screenshot" ]; then
        command -v xwd >/dev/null 2>&1 || MISSING="${MISSING:+$MISSING }x11-apps"
        command -v convert >/dev/null 2>&1 || MISSING="${MISSING:+$MISSING }imagemagick"
    fi
    if [ -n "$MISSING" ]; then
        echo "Missing packages: $MISSING"
        echo "Install with: sudo apt install $MISSING"
        exit 1
    fi
fi

launch_xvfb() {
    local w="$1" h="$2"
    local existing_pid=""
    local recorded_lock_pid=""
    local ready=0
    local ready_observations=0

    # The socket is independent ownership evidence. Never start against an
    # existing endpoint, even when its lock is absent or malformed.
    if x_socket_exists; then
        echo "Display $XVFB_DISPLAY already has an X socket; refusing to replace it." >&2
        return 1
    fi

    # A lock owned by any live process makes the display unavailable. The lock
    # is evidence only: never signal a PID merely because it appears here.
    if [ -e "$XVFB_LOCK_FILE" ]; then
        existing_pid=$(lock_pid 2>/dev/null) || true
        if [ -n "$existing_pid" ] && pid_is_live "$existing_pid"; then
            echo "Display $XVFB_DISPLAY is occupied by live PID $existing_pid; refusing to replace it." >&2
            return 1
        fi
        rm -f -- "$XVFB_LOCK_FILE"
    fi

    Xvfb "$XVFB_DISPLAY" -screen 0 "${w}x${h}x24" &
    XVFB_PID=$!

    # Do not expose or render through the display until the exact child this
    # invocation launched is alive and owns both readiness artifacts.
    for _ in {1..40}; do
        if owned_child_is_running "$XVFB_PID"; then
            recorded_lock_pid=$(lock_pid 2>/dev/null) || recorded_lock_pid=""
            if [ "$recorded_lock_pid" = "$XVFB_PID" ] && x_socket_exists; then
                ready_observations=$((ready_observations + 1))
                if [ "$ready_observations" -ge 2 ]; then
                    ready=1
                    break
                fi
            else
                ready_observations=0
            fi
        elif ! kill -0 "$XVFB_PID" 2>/dev/null; then
            break
        fi
        sleep 0.05
    done
    if [ "$ready" -ne 1 ]; then
        echo "Xvfb failed to establish an owned $XVFB_DISPLAY display; refusing to launch the app." >&2
        return 1
    fi
}

launch_app() {
    local w="$1" h="$2"
    if [ "$MODE" = "native" ]; then
        "$BINARY" --geometry "${w}x${h}" 2>/dev/null &
    else
        QT_QPA_PLATFORM=xcb DISPLAY="$XVFB_DISPLAY" "$BINARY" --geometry "${w}x${h}" 2>/dev/null &
    fi
    APP_PID=$!
    sleep 3  # Let QML fully render
}

stop_app() {
    terminate_owned_child "$APP_PID"
    APP_PID=""
}

stop_xvfb() {
    terminate_owned_child "$VNC_PID"
    VNC_PID=""
    terminate_owned_child "$XVFB_PID"
    remove_owned_xvfb_lock "$XVFB_PID"
    XVFB_PID=""
    # Ensure port is released before next iteration
    sleep 0.5
}

echo "=== OpenAuto Prodigy Resolution Validation ==="
echo "Binary: $BINARY"
echo "Mode:   $MODE"
echo ""

if [ "$MODE" = "screenshot" ]; then
    mkdir -p "$OUTPUT_DIR"
    echo "Screenshots will be saved to: $OUTPUT_DIR"
    echo ""
fi

for entry in "${RESOLUTIONS[@]}"; do
    RES="${entry%%:*}"
    DESC="${entry#*:}"
    W="${RES%x*}"
    H="${RES#*x}"

    echo ">>> ${W}x${H} — ${DESC}"

    case "$MODE" in
        native)
            echo "    Press Ctrl+C when done inspecting..."
            "$BINARY" --geometry "${W}x${H}" 2>/dev/null
            ;;

        interactive)
            launch_xvfb "$W" "$H"
            launch_app "$W" "$H"
            x11vnc -display "$XVFB_DISPLAY" -nopw -forever -quiet -rfbport "$VNC_PORT" &
            VNC_PID=$!
            echo "    VNC ready — connect to $(hostname -I | awk '{print $1}'):${VNC_PORT}"
            echo "    Press Enter when done inspecting..."
            read -r
            stop_app
            stop_xvfb
            ;;

        screenshot)
            launch_xvfb "$W" "$H"
            launch_app "$W" "$H"
            OUTFILE="${OUTPUT_DIR}/test-${W}x${H}.png"
            xwd -display "$XVFB_DISPLAY" -root -silent | convert xwd:- "$OUTFILE"
            echo "    Saved: $OUTFILE"
            stop_app
            stop_xvfb
            ;;
    esac

    echo ""
done

echo "=== Validation complete ==="

if [ "$MODE" = "screenshot" ]; then
    echo "Screenshots in: $OUTPUT_DIR"
    ls -lh "$OUTPUT_DIR"/test-*.png 2>/dev/null
fi
