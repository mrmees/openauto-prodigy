---
phase: 16-routing-correctness-and-idempotency
verified: 2026-03-15T21:30:00Z
status: passed
score: 12/12 must-haves verified
re_verification: false
human_verification:
  - test: "Deploy updated installers to Pi and confirm redsocks system user is created"
    expected: "id redsocks returns a system account with no login shell and no home directory"
    why_human: "useradd runs on the Pi during install.sh execution; cannot verify hardware user creation from dev VM"
  - test: "Enable proxy route on running Pi and confirm iptables nat table shows OPENAUTO_PROXY chain"
    expected: "iptables -t nat -L OUTPUT shows one jump to OPENAUTO_PROXY; iptables -t nat -L OPENAUTO_PROXY shows owner RETURN, destination RETURN, network RETURNs, interface RETURNs, then REDIRECT"
    why_human: "iptables state is runtime on hardware; cannot be verified from unit tests alone"
  - test: "Disable proxy route and confirm all iptables state is gone"
    expected: "OPENAUTO_PROXY chain removed, no OUTPUT jump remains, DNS reverted"
    why_human: "Cleanup is runtime state on hardware"
  - test: "SIGKILL openauto-system while proxy is active, restart it, confirm iptables are clean after startup"
    expected: "cleanup_stale_state() removes orphaned state; daemon reports DISABLED on startup"
    why_human: "Startup self-heal requires hardware crash simulation"
---

# Phase 16: Routing Correctness & Idempotency Verification Report

**Phase Goal:** Proxy routing applies clean iptables rules with correct exemptions, and repeated enable/disable cycles leave no stale state
**Verified:** 2026-03-15T21:30:00Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth | Status | Evidence |
|----|-------|--------|----------|
| 1  | Phase 15 privilege model verified on Pi hardware before routing work began | VERIFIED | 16-01-SUMMARY.md: root daemon confirmed, socket 0660 root:openauto, positive+negative IPC auth, ProtectHome regression found and fixed |
| 2  | enable() installs correct iptables chain with all exemptions in the right order | VERIFIED | proxy_manager.py lines 242-310: flush/recreate sequence; test_exemption_rule_ordering passes |
| 3  | The upstream SOCKS destination is explicitly exempt from redirect | VERIFIED | proxy_manager.py lines 270-277: `-d host -p tcp --dport port -j RETURN`; test_upstream_destination_exemption passes |
| 4  | Redsocks traffic is exempt via owner-based UID match | VERIFIED | proxy_manager.py lines 262-268: `-m owner --uid-owner redsocks -j RETURN`; test_redsocks_owner_self_exemption passes |
| 5  | Network and interface exemptions prevent self-interception of AP/LAN traffic | VERIFIED | proxy_manager.py lines 279-295: four default networks + lo/eth0; test_default_network_exemptions and test_default_interface_exemptions pass |
| 6  | Calling enable 5 times produces exactly one OUTPUT jump and one clean chain per cycle | VERIFIED | test_repeated_enable_5x_only_one_jump_per_cycle passes; test_flush_recreate_idempotency_with_preexisting_chain verifies delete loop + single -I OUTPUT |
| 7  | disable() removes all routing state completely including DNS | VERIFIED | proxy_manager.py disable() lines 132-171: _iptables_flush + _dns_restore + _stop_redsocks; test_disable_sets_disabled_state passes |
| 8  | Cleanup failure sets FAILED state, not DISABLED | VERIFIED | proxy_manager.py lines 143-171: errors list, sets FAILED if any errors; test_cleanup_failure_sets_failed_state passes |
| 9  | Startup always cleans stale proxy state before IPC starts | VERIFIED | openauto_system.py lines 196-200: `await proxy.cleanup_stale_state()` before `await ipc.start()`; test_startup_cleanup_stale_state_before_ipc_start passes |
| 10 | redsocks system user created by install.sh idempotently | VERIFIED | install.sh lines 1681-1687: `id -u redsocks` guard + `useradd --system --no-create-home --shell /usr/sbin/nologin redsocks` |
| 11 | redsocks system user created by install-prebuilt.sh idempotently | VERIFIED | install-prebuilt.sh lines 512-518: identical block |
| 12 | All TS-* tests pass covering the four test requirement groups | VERIFIED | 43/43 tests pass in 10.16s |

**Score:** 12/12 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `system-service/proxy_manager.py` | Idempotent flush/recreate routing, owner+destination exemptions, cleanup_stale_state | VERIFIED | 366 lines; contains cleanup_stale_state, _iptables_apply with flush/recreate, _iptables_flush with loop-delete; no skip_ports remnants |
| `system-service/openauto_system.py` | Startup self-heal before ipc.start(), state-aware handler, no skip_ports | VERIFIED | cleanup_stale_state() at line 197 before ipc.start() at line 200; handle_set_proxy_route checks proxy.state != ProxyState.FAILED |
| `install.sh` | redsocks system user creation | VERIFIED | Line 1683: useradd --system --no-create-home --shell /usr/sbin/nologin redsocks, idempotency check at line 1681 |
| `install-prebuilt.sh` | redsocks system user creation | VERIFIED | Line 514: identical block, idempotency check at line 512 |
| `system-service/tests/test_proxy_manager.py` | 28+ tests covering routing, idempotency, exemptions, cleanup honesty | VERIFIED | 28 tests; includes _make_flush_aware_mock helper, all new TS-* test groups |
| `system-service/tests/test_openauto_system.py` | 15 tests covering request parsing and startup ordering | VERIFIED | 15 tests; test_startup_cleanup_stale_state_before_ipc_start confirms call ordering |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `openauto_system.py` | `proxy_manager.py` | `await proxy.cleanup_stale_state()` before `ipc.start()` | WIRED | Line 197: `await proxy.cleanup_stale_state()`, line 200: `await ipc.start()` — ordering confirmed by test |
| `proxy_manager.py` | iptables | `_run_cmd` with flush/recreate sequence using OPENAUTO_PROXY chain | WIRED | _iptables_apply() and _iptables_flush() both reference OPENAUTO_PROXY throughout |
| `install.sh` | `proxy_manager.py` | redsocks user created by installer, consumed by ProxyManager `--uid-owner redsocks` | WIRED | Both sides confirmed: installer creates user, proxy_manager.py line 265 uses `self._redsocks_user` (default "redsocks") |
| `tests/test_proxy_manager.py` | `proxy_manager.py` | `from proxy_manager import ProxyManager, ProxyState` | WIRED | Line 9 of test file; 28 tests exercise real ProxyManager behavior via mocked _run_cmd |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| PX-01 | 16-02, 16-03 | set_proxy_route succeeds without privilege errors on real hardware | SATISFIED | Redsocks spawned under dedicated system user via `sudo -u redsocks`; installer creates the user; hardware verified in 16-01 |
| PX-02 | 16-02 | Expected redirect rules installed for transparent proxy path | SATISFIED | _iptables_apply() builds complete chain: loop-delete, flush, create, populate, single jump; tested |
| PX-03 | 16-02 | Upstream SOCKS destination exempt from redirect | SATISFIED | Rule at proxy_manager.py:270-277; test_upstream_destination_exemption verifies host+port specificity |
| PX-04 | 16-02 | Redsocks/control-plane traffic exempt from redirect | SATISFIED | Owner-based exemption at lines 262-268; test_redsocks_owner_self_exemption verifies |
| IC-01 | 16-02 | Repeated enable/disable cycles do not accumulate duplicate rules | SATISFIED | flush/recreate model always rebuilds; test_repeated_enable_5x_only_one_jump_per_cycle verifies |
| IC-02 | 16-02 | Disable removes routing state completely including DNS | SATISFIED | disable() calls _iptables_flush + _dns_restore; test_disable_flushes_iptables + test_disable_sets_disabled_state |
| IC-03 | 16-02 | Startup self-heals stale state before reporting success | SATISFIED | cleanup_stale_state() before ipc.start(); test_startup_cleanup_stale_state_before_ipc_start verifies ordering |
| IC-04 | 16-02 | Apply and cleanup failures surface honestly | SATISFIED | disable() sets FAILED on any error; handler checks proxy.state != ProxyState.FAILED; test_cleanup_failure_sets_failed_state |
| TS-01 | 16-04 | Tests cover permission-denied proxy-route setup failures | SATISFIED | test_permission_denied_iptables_raises_and_cleans_up + test_permission_denied_blanket_results_in_failed_state; both pass |
| TS-02 | 16-04 | Tests cover pre-existing chains/jumps and repeated enable without duplication | SATISFIED | test_flush_recreate_idempotency_with_preexisting_chain + test_repeated_enable_5x_only_one_jump_per_cycle; both pass |
| TS-03 | 16-04 | Tests cover upstream destination exemption and redsocks self-exemption | SATISFIED | test_upstream_destination_exemption + test_redsocks_owner_self_exemption + test_exemption_rule_ordering + test_default_network_exemptions + test_default_interface_exemptions; all pass |
| TS-04 | 16-04 | Tests cover cleanup failure truthfulness | SATISFIED | test_cleanup_failure_sets_failed_state + test_cleanup_full_success_sets_disabled; both pass |

**Orphaned requirements check:** SR-01, SR-02, SR-03 map to Phase 17 (Pending). PX-05 maps to Phase 18 (Pending). TS-05 maps to Phase 15 (Complete). None orphaned for Phase 16.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| None found | — | — | — | — |

No TODOs, FIXMEs, placeholder returns, or stub implementations found in any Phase 16 modified files. The `_make_flush_aware_mock` test helper is intentional and complete — it simulates the while-loop termination behavior correctly.

### Human Verification Required

#### 1. Redsocks system user creation on hardware

**Test:** Run the updated installer on the Pi and check: `id redsocks`
**Expected:** Output shows a system UID (typically below 1000), no login shell (`/usr/sbin/nologin`), no home directory
**Why human:** `useradd` runs on Pi hardware during install; cannot verify from the dev VM

#### 2. Iptables chain structure after enable

**Test:** With the daemon running and proxy enabled, run `iptables -t nat -L OUTPUT -n` and `iptables -t nat -L OPENAUTO_PROXY -n`
**Expected:** OUTPUT shows exactly one jump to OPENAUTO_PROXY; OPENAUTO_PROXY chain shows: owner RETURN (redsocks), destination RETURN (companion host:port), four network RETURNs, lo+eth0 RETURN, then REDIRECT to port 12345
**Why human:** iptables is runtime state; unit tests mock the subprocess calls

#### 3. Disable removes all iptables state

**Test:** After running disable, check `iptables -t nat -L OPENAUTO_PROXY -n` and `iptables -t nat -L OUTPUT -n`
**Expected:** OPENAUTO_PROXY chain does not exist; OUTPUT has no jump to OPENAUTO_PROXY
**Why human:** Runtime state on hardware

#### 4. Startup self-heal after abnormal stop

**Test:** Enable proxy route, then SIGKILL the daemon (`sudo kill -9 $(pgrep -f openauto_system.py)`), then restart it, then check iptables immediately after startup
**Expected:** No OPENAUTO_PROXY chain present; daemon journal shows "Cleaning stale proxy routing state..." then "Stale proxy state cleanup complete"
**Why human:** Requires hardware crash simulation and journal inspection

## Summary

Phase 16 goal is achieved. All twelve must-haves are verified against the actual codebase.

The routing rewrite delivers exactly what was specified: the flush/recreate model always rebuilds from scratch (never check-and-reuse), owner-based and destination-based exemptions are both present in the correct order, disable() is honest about cleanup failures, and startup unconditionally cleans stale state before accepting IPC connections.

The test suite runs 43 tests in ~10 seconds with zero failures. The _make_flush_aware_mock helper correctly handles the while-loop delete pattern that would otherwise hang tests indefinitely. Both installers create the redsocks system user idempotently.

The four human verification items are all hardware runtime checks — they cannot be automated from the dev VM but the code paths they exercise are fully covered by unit tests.

---

*Verified: 2026-03-15T21:30:00Z*
*Verifier: Claude (gsd-verifier)*
