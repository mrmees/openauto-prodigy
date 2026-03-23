---
phase: 32-installer-integration
plan: 01
subsystem: infra
tags: [installer, kiosk, labwc, lightdm, boot-splash, rpi-splash-screen-support, swaybg]

# Dependency graph
requires:
  - phase: 28-kiosk-session-infrastructure
    provides: "Reference config files for kiosk session (XDG session, labwc config, LightDM drop-in)"
  - phase: 29-compositor-splash-handoff
    provides: "swaybg autostart line and splash artwork"
  - phase: 31-exit-to-desktop
    provides: "dm-tool session switch and GestureOverlay Desktop button"
provides:
  - create_kiosk_session() installer function deploying all kiosk session infrastructure
  - configure_boot_splash() installer function with rpi-splash-screen-support setup
  - Kiosk mode prompt replacing AUTOSTART prompt in setup_hardware()
  - TUI "Kiosk" step with skipped state support
  - Revert instructions and reboot prompt in finalize()
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns: [non-fatal boot splash with manual fallback, kiosk mode prompt with AUTOSTART inheritance, TUI skipped step state]

key-files:
  created:
    - .planning/phases/32-installer-integration/32-01-PLAN.md
  modified:
    - install.sh

key-decisions:
  - "rpi-splash-screen-support installed on-demand inside configure_boot_splash() rather than in main PACKAGES array -- package is RPi-specific and would fail apt-get on non-RPi systems"
  - "Kiosk mode prompt replaces AUTOSTART prompt -- kiosk implies autostart, non-kiosk falls through to legacy autostart prompt"
  - "Boot splash is entirely non-fatal -- every step has error handling and manual fallback paths"
  - "Reboot prompt instead of 'start now' when kiosk mode is enabled -- kiosk takes effect on LightDM restart"

patterns-established:
  - "Non-fatal system config: configure_boot_splash() wraps every step in error handling with manual fallback, logs warnings, continues"
  - "TUI skipped step: update_step supports 'skipped' status for conditional steps"

requirements-completed: []

# Metrics
duration: 8min
completed: 2026-03-23
---

# Phase 32 Plan 01: Installer Integration Summary

**create_kiosk_session() and configure_boot_splash() functions in install.sh, with kiosk mode prompt, non-fatal boot splash setup, TUI Kiosk step, and reboot prompt**

## Performance

- **Duration:** 8 min
- **Started:** 2026-03-23T03:48:53Z
- **Completed:** 2026-03-23T03:57:15Z
- **Tasks:** 4 (3 code, 1 verification-only)
- **Files modified:** 1

## Accomplishments

- `create_kiosk_session()` deploys all 6 kiosk artifacts: XDG session file, labwc rc.xml/autostart/environment, LightDM drop-in, AccountsService entry, splash PNG
- `configure_boot_splash()` handles rpi-splash-screen-support install, TGA deploy, cmdline.txt repair, Plymouth masking, initramfs rebuild -- all non-fatal with manual fallback
- Kiosk mode prompt in setup_hardware() replaces AUTOSTART with kiosk-first flow
- TUI gains "Kiosk" step (10 steps total, was 9) with "skipped" status for non-kiosk installs
- Finalize shows kiosk status, revert instructions, and offers reboot (kiosk) or start-now (desktop)
- All 88 C++ tests pass, bash -n syntax check passes

## Task Commits

Each task was committed atomically:

1. **Task 1: Add kiosk session and boot splash functions** - `c28af8e` (feat)
2. **Task 2: Add rpi-splash-screen-support install** - `24a7c6a` (feat)
3. **Task 3: Wire kiosk mode prompt and functions into main flow** - `6452c73` (feat)
4. **Task 4: Build verification** - (verification only, no commit -- bash -n + 88/88 tests)

## Files Created/Modified

- `install.sh` - Added create_kiosk_session(), configure_boot_splash(), KIOSK_MODE variable, kiosk prompt in setup_hardware(), Kiosk step in TUI registry, skipped step support, kiosk finalize summary with revert instructions and reboot prompt

## Decisions Made

- **rpi-splash-screen-support installed on-demand:** Package is RPi-specific. Installing it in the main PACKAGES array would fail `apt-get install` on non-RPi systems. Instead, `configure_boot_splash()` installs it separately with non-fatal error handling.
- **Kiosk implies autostart:** When `KIOSK_MODE=true`, `AUTOSTART` is set to `true` automatically. Non-kiosk users get a separate autostart prompt.
- **Reboot vs start-now:** Kiosk mode requires LightDM to restart to take effect. Finalize offers reboot instead of "start now" when kiosk is enabled.
- **All heredoc content matches reference files:** The config content in heredocs matches `docs/pi-config/kiosk/` reference files exactly (verified by grep).

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 32 Plan 01 complete -- install.sh fully integrates all v0.7.0 kiosk work
- Hardware validation needed: Matt runs the installer on the Pi to verify end-to-end kiosk boot experience
- No blockers

## Self-Check: PASSED

Modified file verified present:
- `install.sh` -- FOUND, contains create_kiosk_session and configure_boot_splash functions

All task commits verified:
- `c28af8e` -- FOUND in git log
- `24a7c6a` -- FOUND in git log
- `6452c73` -- FOUND in git log

---
*Phase: 32-installer-integration*
*Completed: 2026-03-23*
