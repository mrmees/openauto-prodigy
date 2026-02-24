#!/bin/bash
# ============================================================
# WARNING: THIS SCRIPT IS BROKEN / NEEDS REWRITE
#
# Issues:
#   - PHONE_MAC is hardcoded (BT MACs randomize, can't assume)
#   - Phone WiFi disable via ADB ("svc wifi disable") is unreliable
#   - Need a better way to drop AA connection on phone side
#   - VIDEO grep validation may not match new open-androidauto log format
#
# Kept as reference for the reconnect SEQUENCE, but the specific
# commands need to be rediscovered and validated during testing.
# ============================================================
#
# OpenAuto Prodigy — AA Disconnect/Reconnect Script
#
# Sequence:
#   1. Pi: BT disconnect + Phone: WiFi off
#   2. Pi: kill app
#   3. Wait for clean state
#   4. Pi: restart app (wait for RFCOMM listener)
#   5. Pi: bluetoothctl connect with A2DP UUID (retry loop, timeout on Pi side)
#      Phone auto-discovers Pi WiFi AP via AA BT handshake
#
# GOTCHA: bluetoothctl connect on dual-mode devices randomly picks BLE.
#   Must specify A2DP UUID to force classic BR/EDR transport.
#
# Usage: ./reconnect.sh [wait_seconds]

ADB="$(dirname "$0")/../platform-tools/adb"
PI="matt@192.168.1.149"
PHONE_MAC="8C:C5:D0:DD:74:15"
A2DP="0000110a-0000-1000-8000-00805f9b34fb"
WAIT="${1:-10}"
APP_DIR="/home/matt/openauto-prodigy"
LOG="/tmp/oap.log"
MAX_ATTEMPTS=6
ATTEMPT_TIMEOUT=10
ATTEMPT_SLEEP=5

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

# SSH wrapper function — avoids variable expansion issues with timeout
pi() { ssh -o ConnectTimeout=5 -o BatchMode=yes "$PI" "$@"; }

echo -e "${YELLOW}=== OpenAuto Prodigy Reconnect ===${NC}"

# Validate wait argument
if ! [[ "$WAIT" =~ ^[0-9]+$ ]]; then
    echo -e "${RED}Invalid wait time: $WAIT (must be integer)${NC}"; exit 1
fi

# Preflight
[[ -x "$ADB" ]] || { echo -e "${RED}ADB not found at $ADB${NC}"; exit 1; }
"$ADB" devices 2>/dev/null | grep -q "device$" || { echo -e "${RED}No ADB device${NC}"; exit 1; }
pi true 2>/dev/null || { echo -e "${RED}Can't SSH to Pi${NC}"; exit 1; }

echo -e "${GREEN}[1/5]${NC} Disconnecting..."
pi "bluetoothctl disconnect ${PHONE_MAC}" 2>/dev/null
"$ADB" shell "svc wifi disable"

echo -e "${GREEN}[2/5]${NC} Killing app..."
pi "pkill -f 'build/src/openauto-prodigy' 2>/dev/null; sleep 2; pkill -9 -f 'build/src/openauto-prodigy' 2>/dev/null"

echo -e "${GREEN}[3/5]${NC} Waiting ${WAIT}s..."
sleep "${WAIT}"

echo -e "${GREEN}[4/5]${NC} Restarting app..."
# ssh -f backgrounds the SSH process itself — avoids BatchMode hang on nohup
ssh -o ConnectTimeout=5 -f "$PI" "rm -f ${LOG}; cd ${APP_DIR} && env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 ./build/src/openauto-prodigy > ${LOG} 2>&1 </dev/null &"
echo "      Waiting 8s for RFCOMM..."
sleep 8
pi "pgrep -f 'build/src/openauto-prodigy'" >/dev/null 2>&1 || { echo -e "${RED}App failed to start${NC}"; exit 1; }
echo "      App running."

MAX_TIME=$(( MAX_ATTEMPTS * (ATTEMPT_TIMEOUT + ATTEMPT_SLEEP) ))
echo -e "${GREEN}[5/5]${NC} BT connect (retrying up to ~${MAX_TIME}s)..."
for i in $(seq 1 "$MAX_ATTEMPTS"); do
    # timeout runs ON THE PI — kills bluetoothctl if it hangs
    OUTPUT=$(pi "timeout ${ATTEMPT_TIMEOUT} bluetoothctl connect ${PHONE_MAC} ${A2DP}" 2>&1)
    if echo "$OUTPUT" | grep -q "Connection successful"; then
        echo -e "      ${GREEN}Connected (attempt $i)!${NC}"
        echo -e "${YELLOW}Waiting 20s for AA session...${NC}"
        sleep 20
        VCOUNT=$(pi "grep -c VIDEO ${LOG} 2>/dev/null")
        if [ "${VCOUNT:-0}" -gt 0 ] 2>/dev/null; then
            echo -e "${GREEN}AA ACTIVE — $VCOUNT video frames.${NC}"
        else
            echo -e "${YELLOW}No video. Last log:${NC}"
            pi "tail -3 ${LOG}"
        fi
        exit 0
    fi
    echo -e "      ${YELLOW}Attempt $i/${MAX_ATTEMPTS} failed, retrying in ${ATTEMPT_SLEEP}s...${NC}"
    sleep "$ATTEMPT_SLEEP"
done
echo -e "${RED}Failed after ${MAX_ATTEMPTS} attempts.${NC}"
exit 1
