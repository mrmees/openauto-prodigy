#!/usr/bin/env bash
set -euo pipefail

APP_DIR=/home/matt/openauto-prodigy
APP_BIN="$APP_DIR/build/src/openauto-prodigy"
LOG_FILE=/tmp/oap.log
WAYLAND_DISPLAY=wayland-0
XDG_RUNTIME_DIR=/run/user/1000

usage() {
  cat <<'USAGE'
Usage: restart.sh [--check|--force-kill|--help]

  --check       Validate runtime binary/path/log state without restarting the app.
  --force-kill  Restart app with SIGKILL for existing matches (use carefully).
  --help        Show this help text.
USAGE
}

MODE="restart"
if [[ $# -gt 1 ]]; then
  echo "ERROR: too many arguments."
  usage
  exit 1
fi

if [[ $# -eq 1 ]]; then
  case "${1:-}" in
    --check)
      MODE=check
      ;;
    --force-kill)
      MODE=force-kill
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      usage
      exit 1
      ;;
  esac
fi

if [[ "$MODE" == "check" ]]; then
  echo "Restart script dry-run mode: validation only."
  if [[ ! -d "$APP_DIR" ]]; then
    echo "ERROR: APP_DIR missing: $APP_DIR"
    exit 1
  fi
  if [[ ! -x "$APP_BIN" ]]; then
    echo "ERROR: binary missing or not executable: $APP_BIN"
    exit 1
  fi
  if ! touch "$LOG_FILE" 2>/dev/null; then
    echo "ERROR: cannot write log file path: $LOG_FILE"
    exit 1
  fi
  if pgrep -f "$APP_BIN" >/dev/null 2>&1; then
    echo "running processes:"
    pgrep -af "$APP_BIN"
  else
    echo "no matching openauto-prodigy process is currently running"
  fi
  echo "config check: OK"
  exit 0
fi

# Stop previous instance if present.
if [[ "$MODE" == "force-kill" ]]; then
  pkill -9 -f "$APP_BIN" 2>/dev/null || true
else
  pkill -f "$APP_BIN" 2>/dev/null || true
fi

sleep 1

# Clear log
truncate -s 0 "$LOG_FILE"

export WAYLAND_DISPLAY XDG_RUNTIME_DIR

# Launch
cd "$APP_DIR"
nohup "$APP_BIN" > "$LOG_FILE" 2>&1 &
APP_PID=$!
disown "$APP_PID" || true

sleep 2
if kill -0 "$APP_PID" 2>/dev/null; then
  echo "App running (PID $APP_PID)"
else
  echo "FAILED to start — check $LOG_FILE"
fi
