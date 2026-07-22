#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="${OAP_SERVICE_NAME:-openauto-prodigy.service}"

usage() {
  cat <<'USAGE'
Usage: restart.sh [--check|--force-kill|--help]

  --check       Report the installed service state without restarting it.
  --force-kill  Stop the unit, clean verified legacy orphans, then start once.
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

systemd_property() {
  systemctl show "$SERVICE_NAME" --property="$1" --value
}

service_executable() {
  local exec_start
  exec_start="$(systemd_property ExecStart)"
  if [[ "$exec_start" =~ path=([^\;[:space:]]+) ]]; then
    readlink -f -- "${BASH_REMATCH[1]}"
    return
  fi
  echo "ERROR: cannot determine ExecStart executable for $SERVICE_NAME" >&2
  return 1
}

service_uid() {
  local service_user
  service_user="$(systemd_property User)"
  if [[ -z "$service_user" ]]; then
    service_user=root
  fi
  id -u "$service_user"
}

is_service_cgroup_member() {
  local pid="$1"
  local hierarchy controllers cgroup_path
  [[ -n "$SERVICE_CGROUP" ]] || return 1
  while IFS=: read -r hierarchy controllers cgroup_path; do
    if [[ "$cgroup_path" == "$SERVICE_CGROUP" ||
          "$cgroup_path" == "$SERVICE_CGROUP/"* ]]; then
      return 0
    fi
  done < "/proc/$pid/cgroup" 2>/dev/null
  return 1
}

is_verified_orphan() {
  local pid="$1"
  local candidate_executable candidate_uid
  [[ "$pid" =~ ^[0-9]+$ && "$pid" != "$$" ]] || return 1
  candidate_executable="$(readlink "/proc/$pid/exe" 2>/dev/null)" || return 1
  candidate_executable="${candidate_executable% (deleted)}"
  [[ "$candidate_executable" == "$SERVICE_EXECUTABLE" ]] || return 1
  candidate_uid="$(awk '/^Uid:/{print $2; exit}' "/proc/$pid/status" 2>/dev/null)"
  [[ "$candidate_uid" == "$SERVICE_UID" ]] || return 1
  ! is_service_cgroup_member "$pid"
}

legacy_orphan_pids() {
  local proc_dir pid
  for proc_dir in /proc/[0-9]*; do
    pid="${proc_dir##*/}"
    if is_verified_orphan "$pid"; then
      printf '%s\n' "$pid"
    fi
  done
}

terminate_legacy_orphans() {
  local -a pids=()
  local pid deadline any_remaining
  mapfile -t pids < <(legacy_orphan_pids)
  ((${#pids[@]} > 0)) || return 0

  echo "Stopping ${#pids[@]} verified legacy unmanaged instance(s)..."
  for pid in "${pids[@]}"; do
    if is_verified_orphan "$pid"; then
      sudo kill --signal=TERM "$pid"
    fi
  done

  deadline=$((SECONDS + 5))
  while ((SECONDS < deadline)); do
    any_remaining=false
    for pid in "${pids[@]}"; do
      if is_verified_orphan "$pid"; then
        any_remaining=true
        break
      fi
    done
    [[ "$any_remaining" == false ]] && return 0
    sleep 0.1
  done

  for pid in "${pids[@]}"; do
    if is_verified_orphan "$pid"; then
      sudo kill --signal=KILL "$pid"
    fi
  done

  deadline=$((SECONDS + 2))
  while ((SECONDS < deadline)); do
    any_remaining=false
    for pid in "${pids[@]}"; do
      if is_verified_orphan "$pid"; then
        any_remaining=true
        break
      fi
    done
    [[ "$any_remaining" == false ]] && return 0
    sleep 0.1
  done

  echo "ERROR: verified legacy instance did not terminate" >&2
  return 1
}

if [[ "$MODE" == force-kill ]]; then
  SERVICE_EXECUTABLE="$(service_executable)"
  SERVICE_UID="$(service_uid)"
  SERVICE_CGROUP="$(systemd_property ControlGroup)"

  # Queue a stop job first so Restart=on-failure cannot race the forced kill.
  # The blocking stop then waits for that job to finish. Once the unit is down,
  # clean only upgrade-era processes whose executable and UID exactly match the
  # unit and which are not members of its cgroup.
  sudo systemctl stop --no-block "$SERVICE_NAME"
  sudo systemctl kill --kill-whom=all --signal=SIGKILL "$SERVICE_NAME" \
    2>/dev/null || true
  sudo systemctl stop "$SERVICE_NAME"
  terminate_legacy_orphans
  sudo systemctl reset-failed "$SERVICE_NAME"
  sudo systemctl start "$SERVICE_NAME"
else
  # A regular systemd restart sends the unit's configured graceful stop
  # signal and applies its normal stop timeout before starting it again.
  sudo systemctl restart "$SERVICE_NAME"
fi

sudo systemctl is-active --quiet "$SERVICE_NAME"
systemctl show "$SERVICE_NAME" --property=ActiveState,SubState,MainPID,NRestarts
