#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="${OAP_SERVICE_NAME:-openauto-prodigy.service}"

usage() {
  cat <<'USAGE'
Usage: restart.sh [--check|--force-kill|--help]

  --check       Report the installed service state without restarting it.
  --force-kill  Stop the unit, kill its cgroup, then start it again.
  --help        Show this help text.
USAGE
}

MODE=restart
if [[ $# -gt 1 ]]; then
  echo "ERROR: too many arguments." >&2
  usage >&2
  exit 1
fi

if [[ $# -eq 1 ]]; then
  case "$1" in
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
      echo "ERROR: unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
fi

if [[ "$MODE" == check ]]; then
  systemctl show "$SERVICE_NAME" \
    --property=LoadState,ActiveState,SubState,MainPID,NRestarts
  exit 0
fi

if [[ "$MODE" == force-kill ]]; then
  # Queue a stop job first so Restart=on-failure cannot race the forced kill.
  # The blocking stop then waits for that job to finish before the new start;
  # systemd remains the sole process owner throughout recovery.
  sudo systemctl stop --no-block "$SERVICE_NAME"
  sudo systemctl kill --kill-whom=all --signal=SIGKILL "$SERVICE_NAME" \
    2>/dev/null || true
  sudo systemctl stop "$SERVICE_NAME"
  sudo systemctl reset-failed "$SERVICE_NAME"
  sudo systemctl start "$SERVICE_NAME"
else
  # A regular systemd restart sends the unit's configured graceful stop
  # signal and applies its normal stop timeout before starting it again.
  sudo systemctl restart "$SERVICE_NAME"
fi

sudo systemctl is-active --quiet "$SERVICE_NAME"
systemctl show "$SERVICE_NAME" --property=ActiveState,SubState,MainPID,NRestarts
