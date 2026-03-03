#!/bin/bash
# validate-resolutions.sh -- Launch app at target resolutions for visual testing
# Usage: ./scripts/validate-resolutions.sh [binary_path]

BINARY="${1:-build/src/openauto-prodigy}"

if [ ! -x "$BINARY" ]; then
    echo "Binary not found: $BINARY"
    echo "Usage: $0 [path/to/openauto-prodigy]"
    exit 1
fi

RESOLUTIONS=(
    "800x480:Pi Official Touchscreen"
    "1024x600:DFRobot (default)"
    "1280x720:720p"
    "1920x480:Ultrawide edge case"
    "480x800:Portrait edge case"
    "480x272:Tiny edge case"
)

echo "=== OpenAuto Prodigy Resolution Validation ==="
echo "Binary: $BINARY"
echo ""
echo "For each resolution:"
echo "  - Check: no overflow, clipping, or illegible text"
echo "  - Check: launcher grid adapts columns"
echo "  - Check: settings tiles fit"
echo "  - Press Ctrl+C to advance to next resolution"
echo ""

for entry in "${RESOLUTIONS[@]}"; do
    RES="${entry%%:*}"
    DESC="${entry#*:}"
    W="${RES%x*}"
    H="${RES#*x}"

    echo ">>> ${W}x${H} — ${DESC}"
    echo "    Press Ctrl+C when done inspecting..."
    "$BINARY" --geometry "${W}x${H}" 2>/dev/null
    echo ""
done

echo "=== Validation complete ==="
