---
phase: 32-installer-integration
plan: "02"
subsystem: infra
tags: [installer, kiosk, boot-splash, lightdm, labwc, swaybg, prebuilt]

# Dependency graph
requires:
  - phase: 28-kiosk-session-infrastructure
    provides: kiosk session config reference files (rc.xml, autostart, environment, desktop, lightdm)
  - phase: 29-compositor-splash-handoff
    provides: swaybg splash in kiosk autostart
  - phase: 30-rpi-boot-splash
    provides: boot splash TGA and deployment documentation
  - phase: 31-exit-to-desktop
    provides: dm-tool session switch mechanism
provides:
  - install-prebuilt.sh creates complete kiosk session with boot splash when user opts in
  - Prebuilt installer parity with source installer for kiosk features
affects: []

# Tech tracking
tech-stack:
  added: [rpi-splash-screen-support, swaybg]
  patterns: [non-fatal boot splash, kiosk opt-in prompt, idempotent config deployment]

key-files:
  created: []
  modified:
    - install-prebuilt.sh

key-decisions:
  - "Kiosk mode prompt defaults to Y (recommended) -- kiosk is the expected deployment mode"
  - "Boot splash is non-fatal -- all steps wrapped in error handling, failures warn and continue"
  - "Splash asset lookup checks both payload/assets/ and script-dir/assets/ for flexibility"
  - "Service template upgraded to match install.sh (Type=notify, WatchdogSec, BindsTo hostapd, preflight)"
  - "Combined Task 1 and Task 2 into one commit since both functions were added together"

patterns-established:
  - "Non-fatal optional feature: apt-cache check before install, each step has fallback, verification at end"
  - "Kiosk opt-in replaces basic autostart: KIOSK_MODE=true implies AUTOSTART=true"

requirements-completed: []

# Metrics
duration: 5min
completed: 2026-03-23
---

# Phase 32 Plan 02: Prebuilt Installer Kiosk Integration Summary

**install-prebuilt.sh gains create_kiosk_session() and configure_boot_splash() with kiosk mode prompt, non-fatal boot splash, and revert instructions**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-23T03:49:12Z
- **Completed:** 2026-03-23T03:53:52Z
- **Tasks:** 3
- **Files modified:** 1

## Accomplishments

- Added `create_kiosk_session()` function that deploys all kiosk infrastructure: XDG session file, labwc config directory (rc.xml, autostart, environment), LightDM drop-in, AccountsService entry, and splash image
- Added `configure_boot_splash()` function following exact steps from boot-splash.md: package install, TGA deployment, configure-splash with manual fallback, cmdline.txt repair, Plymouth masking, initramfs rebuild, verification
- Replaced basic "Start on boot?" prompt with kiosk mode prompt (default Y) with description of what kiosk mode provides
- Updated service template to match install.sh's production quality (Type=notify, WatchdogSec, BindsTo hostapd, preflight, BlueZ disconnect)
- Extended diagnostics with kiosk session health checks and installation summary with revert instructions

## Task Commits

Each task was committed atomically:

1. **Task 1+2: Add create_kiosk_session() and configure_boot_splash()** - `8a7da3f` (feat)
2. **Task 3: Wire functions into main flow, update prompts and diagnostics** - `d33fcce` (feat)

## Files Created/Modified

- `install-prebuilt.sh` -- Added ~300 lines: two new functions (create_kiosk_session, configure_boot_splash), updated kiosk prompt, upgraded service template, payload validation, diagnostics, revert instructions

## Decisions Made

- Kiosk mode prompt defaults to Y (recommended) -- this is the expected deployment mode for a car head unit
- Boot splash is completely non-fatal -- each step has error handling, failures warn and continue, the app still works without boot splash
- Combined Tasks 1 and 2 into one commit since both functions were written together as a logical unit
- Service template upgraded to match install.sh production quality rather than keeping the simpler original

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Upgraded service template to production quality**
- **Found during:** Task 3 (service update)
- **Issue:** Prebuilt installer's service template was simpler than install.sh (missing Type=notify, WatchdogSec, BindsTo hostapd, preflight, BlueZ disconnect, StartLimitBurst)
- **Fix:** Updated service template to match install.sh's battle-tested version
- **Files modified:** install-prebuilt.sh
- **Verification:** bash -n passes, service unit is valid
- **Committed in:** d33fcce (Task 3 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Service template upgrade ensures prebuilt installs get production-quality service management. No scope creep.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 32 Plan 02 complete -- prebuilt installer now has full kiosk support
- Prebuilt and source installers produce identical kiosk configurations
- Ready for Pi hardware testing (Phase 32 overall verification)

## Self-Check: PASSED

All files exist, all commits verified, all functions present, bash syntax valid.

---
*Phase: 32-installer-integration*
*Completed: 2026-03-23*
