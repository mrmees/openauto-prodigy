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

cleanup() {
    # Kill any processes we started
    if [ -n "${APP_PID:-}" ] && kill -0 "$APP_PID" 2>/dev/null; then
        kill "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
    fi
    if [ -n "${VNC_PID:-}" ] && kill -0 "$VNC_PID" 2>/dev/null; then
        kill "$VNC_PID" 2>/dev/null || true
    fi
    if [ -n "${XVFB_PID:-}" ] && kill -0 "$XVFB_PID" 2>/dev/null; then
        kill "$XVFB_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

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
    # Kill any existing Xvfb on our display
    if [ -f "/tmp/.X99-lock" ]; then
        kill "$(cat /tmp/.X99-lock 2>/dev/null)" 2>/dev/null || true
        rm -f "/tmp/.X99-lock"
        sleep 0.5
    fi
    Xvfb "$XVFB_DISPLAY" -screen 0 "${w}x${h}x24" &
    XVFB_PID=$!
    sleep 1
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
    if [ -n "${APP_PID:-}" ] && kill -0 "$APP_PID" 2>/dev/null; then
        kill "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
    fi
    APP_PID=""
}

stop_xvfb() {
    if [ -n "${VNC_PID:-}" ] && kill -0 "$VNC_PID" 2>/dev/null; then
        kill "$VNC_PID" 2>/dev/null || true
        VNC_PID=""
    fi
    if [ -n "${XVFB_PID:-}" ] && kill -0 "$XVFB_PID" 2>/dev/null; then
        kill "$XVFB_PID" 2>/dev/null || true
        XVFB_PID=""
    fi
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
