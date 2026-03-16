---
phase: 18-hardware-validation
plan: 01
subsystem: infra
tags: [socks5, redsocks, iptables, proxy, hardware-validation, tcpdump, companion]

# Dependency graph
requires:
  - phase: 16-routing-correctness-and-idempotency
    provides: "iptables REDIRECT chain, redsocks lifecycle, proxy_manager enable/disable"
  - phase: 17-status-reporting-hardening
    provides: "get_proxy_status with live verification, health loop"
provides:
  - "End-to-end proof that TCP traffic traverses companion SOCKS bridge on real Pi hardware"
  - "Two proxy_manager bug fixes (skip_interfaces, config permissions)"
affects: [companion-app, proxy-routing]

# Tech tracking
tech-stack:
  added: [tcpdump, dnsutils (on Pi)]
  patterns: [four-point proof chain for proxy validation]

key-files:
  created:
    - docs/validation/phase18-hardware-validation-20260316-093435.log
  modified:
    - system-service/proxy_manager.py
    - system-service/tests/test_proxy_manager.py

key-decisions:
  - "eth0 removed from default skip_interfaces -- network-based 192.168.0.0/16 covers LAN, interface-based eth0 exemption bypassed all internet REDIRECT"
  - "Config permissions changed from 0600 to 0640 with root:redsocks ownership so redsocks user can read its own config"
  - "Companion SOCKS5 per-connection logging gap documented as known limitation (origin IP comparison used as alternative proof)"

patterns-established:
  - "Four-point proof chain: loopback redirect + outbound bypass + companion receipt + request success"
  - "Origin IP comparison as companion receipt proof when per-connection logging unavailable"

requirements-completed: [PX-05]

# Metrics
duration: 12min
completed: 2026-03-16
---

# Phase 18 Plan 01: Hardware Validation Summary

**End-to-end four-point proof that HTTP and HTTPS traffic traverses companion SOCKS5 bridge on real Pi hardware, with two proxy_manager bug fixes**

## Performance

- **Duration:** 12 min
- **Started:** 2026-03-16T14:34:31Z
- **Completed:** 2026-03-16T14:46:50Z
- **Tasks:** 2 (1 checkpoint + 1 auto)
- **Files modified:** 3

## Accomplishments
- All 10/10 proof points pass: negative baseline (2), HTTP positive (4), HTTPS positive (4)
- Origin IP from httpbin.org/ip (166.205.190.58) matches phone's cellular IP, proving traffic egressed through companion SOCKS5 proxy
- Fixed eth0 skip_interfaces bug that bypassed all internet traffic REDIRECT
- Fixed redsocks config permission bug (0600 root-owned -> 0640 root:redsocks)

## Task Commits

Each task was committed atomically:

1. **Task 1: Physical setup** - checkpoint (human-action, completed by Matt)
2. **Task 2: Four-point proof chain + bug fixes** - `631c1e9` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `docs/validation/phase18-hardware-validation-20260316-093435.log` - 428-line timestamped validation evidence log with all proof points and verdict
- `system-service/proxy_manager.py` - Fixed default skip_interfaces (removed eth0) and config permissions (0640 + chown)
- `system-service/tests/test_proxy_manager.py` - Updated tests to expect lo-only default interface exemption

## Decisions Made
- Removed eth0 from default skip_interfaces: the 192.168.0.0/16 network exemption already covers LAN destinations, so an interface-level eth0 exemption is both redundant and harmful (it bypasses REDIRECT for all internet traffic since the default route goes via eth0)
- Changed redsocks config from 0600 to 0640 with root:redsocks group ownership: the config is written by the root-running daemon but read by the redsocks user via sudo
- Used origin IP comparison as companion receipt proof since the Socks5Server has no per-connection logging: when httpbin.org/ip returns the phone's cellular IP (166.205.190.58) instead of the Pi's LAN IP, that's conclusive proof the request traversed the companion's cellular-bound SOCKS proxy

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Installed tcpdump and dnsutils on Pi**
- **Found during:** Task 2, Step 3 (first HTTP proof attempt)
- **Issue:** Pi minimal Trixie install lacked tcpdump and dig commands
- **Fix:** `sudo apt-get install -y tcpdump dnsutils`
- **Verification:** Both commands available, used successfully in proof chain
- **Committed in:** 631c1e9

**2. [Rule 1 - Bug] eth0 in default skip_interfaces bypasses all internet REDIRECT**
- **Found during:** Task 2, Step 3 (HTTP PROOF 2 failed: 10 packets leaked to httpbin on eth0)
- **Issue:** Default `skip_interfaces=["lo", "eth0"]` adds `-o eth0 -j RETURN` rule that matches all internet traffic (default route goes via eth0), preventing any REDIRECT
- **Fix:** Changed default to `["lo"]` only; 192.168.0.0/16 network exemption already covers LAN
- **Files modified:** system-service/proxy_manager.py, system-service/tests/test_proxy_manager.py
- **Verification:** 62/62 proxy_manager tests pass; hardware proof shows 0 outbound packets after fix
- **Committed in:** 631c1e9

**3. [Rule 1 - Bug] redsocks config written with 0600 root-owned permissions**
- **Found during:** Task 2, pre-flight (journalctl showed "Unable to open config file: Permission denied")
- **Issue:** proxy_manager writes config as root with 0600, then spawns redsocks as `redsocks` user who cannot read the file
- **Fix:** Changed to 0640 with `os.chown(path, 0, redsocks_gid)` so redsocks group can read
- **Files modified:** system-service/proxy_manager.py
- **Verification:** Manual workaround confirmed the fix approach works; redsocks reads config successfully
- **Committed in:** 631c1e9

---

**Total deviations:** 3 auto-fixed (1 blocking, 2 bugs)
**Impact on plan:** Bug fixes were essential for correct proxy operation. Without them, the pipeline silently fails (redsocks can't start, or all traffic bypasses REDIRECT).

## Issues Encountered
- System redsocks.service (using /etc/redsocks.conf pointing to 127.0.0.1:1080) was interfering with the proxy_manager's intended redsocks instance. Resolved by stopping system service and running manually with corrected config during validation.
- Companion app Socks5Server has no per-connection SOCKS logging -- successful relays produce zero logcat output. Used origin IP comparison as alternative proof (stronger evidence anyway).

## Companion App Enhancement Needed
The Socks5Server lacks per-connection logging. Adding a `Log.i(TAG, "CONNECT $destHost:$destPort")` line in `handleClient()` after successful auth would make future validation self-documenting. Filed as known gap, not blocking.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- PX-05 satisfied: representative TCP traffic traverses companion SOCKS bridge
- v0.6.3 milestone acceptance gate passed
- Two bug fixes should be deployed to Pi before production use

---
*Phase: 18-hardware-validation*
*Completed: 2026-03-16*
