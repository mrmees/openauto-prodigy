#!/usr/bin/env bash
# Generates token-preview.html from the Pi's connected-device theme YAML.
# Usage: ./tools/token-preview.sh [host]
#   host defaults to matt@192.168.1.152

set -euo pipefail

HOST="${1:-matt@192.168.1.152}"
YAML_PATH="~/.openauto/themes/connected-device/theme.yaml"
OUT="$(dirname "$0")/../token-preview.html"

# Fetch YAML from Pi
yaml=$(ssh "$HOST" "cat $YAML_PATH" 2>/dev/null) || {
    echo "Error: couldn't read theme YAML from $HOST:$YAML_PATH" >&2
    exit 1
}

# Parse hyphenated token keys from day/night sections
# Returns "token-name #rrggbb" lines (strips alpha byte)
parse_section() {
    local section="$1"
    local in_section=0
    while IFS= read -r line; do
        if [[ "$line" =~ ^${section}: ]]; then
            in_section=1; continue
        fi
        if [[ $in_section -eq 1 ]]; then
            # Exit section on next top-level key
            [[ "$line" =~ ^[a-z] ]] && break
            # Only grab hyphenated keys (phone tokens), skip underscored (legacy)
            if [[ "$line" =~ ^[[:space:]]+([-a-z]+):\ \"#ff([0-9a-fA-F]{6})\" ]]; then
                local key="${BASH_REMATCH[1]}"
                local hex="${BASH_REMATCH[2]}"
                # Skip if key contains underscore (legacy format)
                [[ "$key" == *_* ]] && continue
                echo "$key #$hex"
            fi
        fi
    done <<< "$yaml"
}

# UI mapping for each token
usage_for() {
    case "$1" in
        primary)             echo "Tile bg, popup slider fill + knob" ;;
        on-surface)          echo "Power menu icons" ;;
        surface)             echo "Power menu bg, popup bg" ;;
        surface-variant)     echo "Slider track bg" ;;
        surface-container)   echo "(available — Material 3 container)" ;;
        background)          echo "Window bg, wallpaper fallback" ;;
        outline)             echo "Navbar bar bg, navbar control bg" ;;
        outline-variant)     echo "Navbar shadow, popup border" ;;
        inverse-surface)     echo "(unused)" ;;
        inverse-on-surface)  echo "Tile icon + text, navbar icons + clock" ;;
        text-primary)        echo "Power menu text, popup value label" ;;
        text-secondary)      echo "Section headers, settings subtitles" ;;
        red)                 echo "(available — errors/alerts)" ;;
        on-red)              echo "(available — text on red)" ;;
        yellow)              echo "Navbar hold-progress fill" ;;
        on-yellow)           echo "(available — text on yellow)" ;;
        *)                   echo "(unmapped)" ;;
    esac
}

# Ordered token list (controls display order)
TOKENS=(
    primary on-surface surface surface-variant surface-container background
    outline outline-variant inverse-surface inverse-on-surface
    text-primary text-secondary red on-red yellow on-yellow
)

# Parse both sections into associative arrays
declare -A day_colors night_colors
while read -r key hex; do
    day_colors["$key"]="$hex"
done < <(parse_section "day")
while read -r key hex; do
    night_colors["$key"]="$hex"
done < <(parse_section "night")

# Generate token rows HTML
gen_rows() {
    local -n colors=$1
    for token in "${TOKENS[@]}"; do
        local hex="${colors[$token]:-#000000}"
        local usage
        usage=$(usage_for "$token")
        cat <<ROW
  <div class="token-row"><div class="swatch" style="background:${hex}"></div><div class="info">
    <div class="label">${token} <span class="hex">${hex}</span></div>
    <div class="usage">${usage}</div>
  </div></div>
ROW
    done
}

# Write HTML
cat > "$OUT" <<'HEADER'
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Material You Token Preview — Current Mapping</title>
<style>
  body { font-family: 'Segoe UI', sans-serif; background: #1a1a1a; color: #eee; margin: 20px; }
  h1 { text-align: center; margin-bottom: 5px; }
  .subtitle { text-align: center; color: #888; margin-bottom: 30px; }
  .mode-group { display: inline-block; vertical-align: top; width: 48%; margin: 0 1%; }
  h2 { text-align: center; border-bottom: 1px solid #444; padding-bottom: 8px; }
  .token-row { display: flex; align-items: center; margin: 8px 0; }
  .swatch { width: 60px; height: 40px; border-radius: 6px; border: 1px solid #555; flex-shrink: 0; }
  .info { margin-left: 12px; }
  .label { font-size: 14px; font-weight: bold; }
  .hex { color: #999; font-family: monospace; font-size: 13px; }
  .usage { color: #6cf; font-size: 12px; margin-top: 2px; }
</style>
</head>
<body>
<h1>Material You Tokens — Live Mapping</h1>
HEADER

echo "<p class=\"subtitle\">Generated $(date '+%Y-%m-%d %H:%M') from ${HOST}</p>" >> "$OUT"

echo '<div class="mode-group">' >> "$OUT"
echo '  <h2>Day</h2>' >> "$OUT"
gen_rows day_colors >> "$OUT"
echo '</div>' >> "$OUT"

echo '<div class="mode-group">' >> "$OUT"
echo '  <h2>Night</h2>' >> "$OUT"
gen_rows night_colors >> "$OUT"
echo '</div>' >> "$OUT"

cat >> "$OUT" <<'FOOTER'
</body>
</html>
FOOTER

echo "Generated: $OUT"
