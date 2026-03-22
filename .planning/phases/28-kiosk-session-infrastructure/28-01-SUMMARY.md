---
phase: 28-kiosk-session-infrastructure
plan: 01
subsystem: infra
tags: [kiosk, labwc, lightdm, xdg-session, systemd, wayland]

# Dependency graph
requires: []
provides:
  - XDG session entry (openauto-kiosk.desktop) for LightDM kiosk autologin
  - Stripped labwc kiosk config (rc.xml, autostart, environment) at /etc/openauto-kiosk/labwc/
  - LightDM drop-in (50-openauto-kiosk.conf) for autologin into kiosk session
  - AccountsService session reference file
  - Simplified systemd service template (no wf-panel-pi lifecycle management)
affects: [29-compositor-splash, 30-boot-splash, 31-exit-to-desktop, 32-installer-integration]

# Tech tracking
tech-stack:
  added: []
  patterns: [labwc -C flag for session-isolated config, LightDM conf.d drop-in override, wildcard windowRule for app_id matching]

key-files:
  created:
    - docs/pi-config/kiosk/openauto-kiosk.desktop
    - docs/pi-config/kiosk/labwc/rc.xml
    - docs/pi-config/kiosk/labwc/autostart
    - docs/pi-config/kiosk/labwc/environment
    - docs/pi-config/kiosk/50-openauto-kiosk.conf
    - docs/pi-config/kiosk/accountsservice-user
    - docs/pi-config/kiosk/README.md
  modified:
    - install.sh

key-decisions:
  - "labwc_config root element used for kiosk rc.xml (canonical labwc schema, not backward-compat openbox_config)"
  - "Wildcard identifier='openauto*' in windowRule avoids app_id mismatch without main.cpp changes"
  - "One service file for both modes with zero panel management -- kiosk provides clean canvas, desktop users accept visible panel"

patterns-established:
  - "Kiosk session isolation via labwc -C /etc/openauto-kiosk/labwc -- completely separate from user desktop config"
  - "Drop-in config strategy: 50-openauto-kiosk.conf in lightdm.conf.d/ survives raspi-config overwrites"
  - "Belt-and-suspenders session selection: both LightDM drop-in and AccountsService agree"

requirements-completed: []

# Metrics
duration: 4min
completed: 2026-03-22
---

# Phase 28 Plan 01: Kiosk Session Infrastructure Summary

**XDG session entry, stripped labwc kiosk config, LightDM drop-in, and simplified systemd service template -- foundational config files for all subsequent v0.7.0 phases**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-22T16:49:15Z
- **Completed:** 2026-03-22T16:53:08Z
- **Tasks:** 2
- **Files modified:** 8

## Accomplishments

- Created 7 kiosk session config files under `docs/pi-config/kiosk/` with deployment path documentation
- XDG session uses labwc `-C` flag for complete config isolation between kiosk and desktop sessions
- Removed wf-panel-pi kill/restore lifecycle from systemd service template in install.sh
- README with manual deployment, testing, revert, and raspi-config warning

## Task Commits

Each task was committed atomically:

1. **Task 1: Create kiosk session config files** - `407b2ce` (feat)
2. **Task 2: Simplify systemd service template and update panel messaging** - `25f052b` (feat)

## Files Created/Modified

- `docs/pi-config/kiosk/openauto-kiosk.desktop` - XDG session entry pointing to labwc -C /etc/openauto-kiosk/labwc
- `docs/pi-config/kiosk/labwc/rc.xml` - Stripped kiosk labwc config: mouseEmulation=no, auto-fullscreen wildcard rule, no decorations
- `docs/pi-config/kiosk/labwc/autostart` - Minimal autostart (splash deferred to Phase 29, app via systemd)
- `docs/pi-config/kiosk/labwc/environment` - QT_QPA_PLATFORM=wayland and XDG_CURRENT_DESKTOP=labwc
- `docs/pi-config/kiosk/50-openauto-kiosk.conf` - LightDM autologin drop-in with locked filename
- `docs/pi-config/kiosk/accountsservice-user` - AccountsService session entry reference
- `docs/pi-config/kiosk/README.md` - Deployment paths, manual testing, revert instructions, raspi-config warning
- `install.sh` - Removed 2 wf-panel-pi lines from service template, updated suppress_panel_notifications() message

## Decisions Made

- Used `<labwc_config>` root element (canonical labwc schema) instead of backward-compat `<openbox_config>` from existing desktop config
- Wildcard `identifier="openauto*"` in windowRule to avoid app_id mismatch without requiring main.cpp changes (per CONTEXT.md discretion)
- One unified service file with zero panel management -- kiosk mode provides clean canvas; desktop mode users accept panel stays visible

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `desktop-file-validate` on this VM (Ubuntu 24.04) reports `DesktopNames` as an unknown key. This is a false positive -- `DesktopNames` is a standard key per XDG Desktop Entry Spec 1.5, and Ubuntu's own wayland-session .desktop files use it. The Pi's validator (Trixie/Debian 13) should accept it.
- Build verification skipped: worktree submodule not initialized (proto headers missing). Since changes are install.sh (bash script) and docs/ (config files), this is unrelated to C++ build correctness.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All kiosk config files exist as reference configs, ready for Phase 29 (compositor splash adds swaybg line to autostart)
- Service template simplified, ready for Phase 32 (installer integration deploys these files to system paths)
- No blockers

## Self-Check: PASSED

All 8 files verified present. Both task commits (407b2ce, 25f052b) verified in git log.

---
*Phase: 28-kiosk-session-infrastructure*
*Completed: 2026-03-22*
