#!/bin/bash
# OpenAuto Prodigy — AA Protocol Capture Script
#
# Wraps reconnect.sh, then collects Pi-side protocol log + phone-side logcat.
# Usage: ./capture.sh [capture_name] [wait_seconds]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ADB="${SCRIPT_DIR}/../platform-tools/adb"
PI="matt@192.168.1.149"
CAPTURE_NAME="${1:-baseline}"
WAIT="${2:-10}"
CAPTURE_DIR="${SCRIPT_DIR}/captures/${CAPTURE_NAME}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
pi() { ssh -o ConnectTimeout=5 -o BatchMode=yes "$PI" "$@"; }

echo -e "${YELLOW}=== Protocol Capture: ${CAPTURE_NAME} ===${NC}"

# Create capture directory
mkdir -p "${CAPTURE_DIR}"

# Clear phone logcat before reconnect
"$ADB" logcat -c 2>/dev/null

# Run reconnect
echo -e "${GREEN}[1/4]${NC} Running reconnect..."
"${SCRIPT_DIR}/reconnect.sh" "${WAIT}"
RC=$?
if [ "$RC" -ne 0 ]; then
    echo -e "${RED}Reconnect failed (exit $RC). Aborting capture.${NC}"
    exit 1
fi

# Give user time to interact with AA
echo -e "${GREEN}[2/4]${NC} AA session active."
echo -e "${YELLOW}Interact with AA now. Press Enter when done.${NC}"
read -r

# Collect Pi-side protocol log
echo -e "${GREEN}[3/4]${NC} Collecting Pi protocol log..."
scp "${PI}:/tmp/oap-protocol.log" "${CAPTURE_DIR}/pi-protocol.log" 2>/dev/null
if [ -f "${CAPTURE_DIR}/pi-protocol.log" ]; then
    LINES=$(wc -l < "${CAPTURE_DIR}/pi-protocol.log")
    echo -e "      ${GREEN}${LINES} lines captured.${NC}"
else
    echo -e "      ${YELLOW}No protocol log found on Pi.${NC}"
fi

# Collect phone logcat
echo -e "${GREEN}[4/4]${NC} Collecting phone logcat..."
"$ADB" logcat -d > "${CAPTURE_DIR}/phone-logcat-raw.log" 2>/dev/null
grep -E 'CAR\.|GH\.|WIRELESS|PROJECTION|WPP' "${CAPTURE_DIR}/phone-logcat-raw.log" \
    > "${CAPTURE_DIR}/phone-logcat.log"
PHONE_LINES=$(wc -l < "${CAPTURE_DIR}/phone-logcat.log")
echo -e "      ${GREEN}${PHONE_LINES} AA-filtered lines.${NC}"

# Summary
echo ""
echo -e "${GREEN}Capture complete: ${CAPTURE_DIR}/${NC}"
ls -la "${CAPTURE_DIR}/"
