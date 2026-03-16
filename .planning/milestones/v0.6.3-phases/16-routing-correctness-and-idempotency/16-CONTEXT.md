# Phase 16: Routing Correctness & Idempotency - Context

**Gathered:** 2026-03-16
**Status:** Ready for planning

<domain>
## Phase Boundary

Make `set_proxy_route` apply clean iptables rules with correct exemptions, and make repeated enable/disable cycles leave no stale state. Startup self-heals any drift from abnormal stops. Phase 15 (root daemon, 0660 socket, peer credential auth) is the foundation — Phase 16 validates it on hardware before starting routing work.

</domain>

<decisions>
## Implementation Decisions

### Phase 15 hardware checkpoint (pre-wave gate)
- Non-autonomous checkpoint BEFORE any wave starts — Phase 16 does not begin until Phase 15 is proven on hardware
- Hybrid execution split:
  - **Matt does:** run installer, reboot/session-refresh, launch Prodigy if needed
  - **Claude does over SSH:** verify openauto-system is active and running as root, verify `/run/openauto/system.sock` exists with root:openauto and 0660, verify no permission-denied errors in recent journal
  - **Matt confirms locally:** Qt client connected cleanly, no visible permission/socket error on Prodigy side
- **If checkpoint fails:** stop immediately, classify as Phase 15 regression, fix there, rerun checkpoint. Phase 16 is NOT started.
- Interpretation boundary: socket/client failure = Phase 15 regression; socket clean + routing fails = Phase 16 territory

### Self-exemption strategy
- **Both** destination exemption AND owner-based exemption — do not rely on either mechanism alone
- **Dedicated `redsocks` system user:** create a locked-down system user (no login shell, no real home), spawn redsocks under that UID, use `-m owner --uid-owner redsocks` for process-level self-exemption. Installer creates this user.
- **Upstream SOCKS destination exemption:** exempt the exact companion host + port (not host-only). Rule targets `-d host --dport port -j RETURN`. Tightest possible — do not silently exempt other companion services.
- **Network exemptions (default_skip_networks):**
  - `127.0.0.0/8` (loopback)
  - `169.254.0.0/16` (link-local)
  - `10.0.0.0/24` (AP subnet)
  - `192.168.0.0/16` (LAN — broad to cover various home networks)
- **Interface exemptions (default_skip_interfaces):** `lo`, `eth0` only. Do NOT skip `wlan0` — redirecting AP outbound traffic to the proxy is the whole point.
- **Rule ordering in chain:** owner exemption first, then upstream destination exemption, then network exemptions, then interface exemptions, then the final REDIRECT rule

### Idempotent rule application
- **Enable sequence (deterministic, always rebuild):**
  1. Loop-delete all matching `OUTPUT → OPENAUTO_PROXY` jumps until none remain
  2. Flush and delete existing `OPENAUTO_PROXY` chain if present (ignore "not found")
  3. Create fresh `OPENAUTO_PROXY` chain
  4. Populate exemption rules (owner, upstream dest, networks, interfaces) then REDIRECT
  5. Insert exactly one fresh `OUTPUT → OPENAUTO_PROXY` jump
- Always flush/recreate the chain — never check-and-reuse. Parameters (host/port/exemptions) may change between calls, and stale rules from prior bugs must not persist.
- **Disable sequence:**
  1. Loop-delete all matching `OUTPUT → OPENAUTO_PROXY` jumps
  2. Flush `OPENAUTO_PROXY`
  3. Delete `OPENAUTO_PROXY` chain
  4. Restore DNS (`resolvectl revert wlan0`)
  5. Stop redsocks, remove config

### Cleanup failure handling
- If any disable/cleanup step fails: state = `FAILED` with detailed error info in the response
- Do NOT set `DISABLED` if stale rules may remain — that's the exact dishonesty this phase fixes
- Log exactly which cleanup steps failed
- State model (no new states):
  - `DISABLED` — cleanup fully succeeded, no active/stale routing state
  - `ACTIVE` — routing applied and healthy
  - `DEGRADED` — routing partially unhealthy but still configured
  - `FAILED` — apply or cleanup failed, state may not match intent

### Startup self-heal
- **Always clean on startup** — run full proxy cleanup sequence before reporting healthy, before `ipc.start()`
- No detection logic — just clean. If nothing exists, deletes fail silently and it's a no-op.
- Clean **both** iptables AND DNS (resolvectl revert) — full proxy side effects, not just firewall half
- **Implementation:** explicit `async def cleanup_stale_state()` method on ProxyManager, called with `await proxy.cleanup_stale_state()` from `main()` before `ipc.start()`. Not in `__init__`.
- Division of labor:
  - `ExecStopPost` — cheap crash insurance for iptables only (belt-and-suspenders)
  - `cleanup_stale_state()` — full authoritative startup cleanup of all proxy side effects
  - `disable()` — full runtime teardown path

### Response design
- `set_proxy_route` success responses stay simple: `{ok: true, state: "active"}`. Failure: `{ok: false, error: "...", state: "failed"}`.
- Rule/exemption detail lives in journal logs or a separate debug/introspection endpoint, NOT in the control response.
- `get_proxy_status` stays simple: `{state: "..."}` (Phase 17 adds truthfulness to this)

### Claude's Discretion
- Exact iptables command sequencing and error message formatting
- Whether to add a `get_proxy_debug` method or just rely on journal logs for diagnostics
- Test structure and granularity for the new exemption/idempotency cases
- How to spawn redsocks under the dedicated user (setuid before exec, `su -s`, or `subprocess` with `user=` param)

### Not At Claude's Discretion
- The exemption strategy: both owner-based AND destination-based, always
- The dedicated `redsocks` user (not root, not app user)
- The flush/recreate model (not check-and-reuse)
- Cleanup failure = FAILED state (not DISABLED)
- Startup always cleans (not conditional)
- Phase 15 checkpoint is a pre-wave gate (not wave 1)

</decisions>

<specifics>
## Specific Ideas

- "The whole point of this phase is deterministic, boring routing state" — flush/recreate is preferred over clever incremental patching
- "If you exempt root, you punch a giant hole in the redirect policy" — redsocks must have its own UID
- "Disable is not 'successful' if stale redirect state may still be present" — honest state reporting
- "Every boot starts from clean proxy-routing state. If routing later exists, this process created it." — startup invariant
- Phase 15 checkpoint: "Do not waste Phase 16 debugging routing when the root daemon / socket path is still busted"

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `proxy_manager.py`: ProxyManager class with enable/disable, _iptables_apply/_iptables_flush, _dns_enable/_dns_restore, redsocks lifecycle, health loop. Needs rework but structure is sound.
- `openauto_system.py`: handle_set_proxy_route/handle_get_proxy_status handlers, parse_set_proxy_route_request validator. IPC dispatch is clean.
- `health_monitor.py`: HealthMonitor with service checks, recovery, functional validation. No proxy-specific checks yet.
- `test_proxy_manager.py`: 14 tests covering enable/disable/config/iptables/health. Need extension for exemption and idempotency cases.

### Established Patterns
- `_run_cmd()` async subprocess wrapper used throughout ProxyManager and HealthMonitor
- State callback pattern: `_set_state()` triggers `_state_callback` for external notification
- Test pattern: `_run_cmd = AsyncMock(return_value=(0, ""))` for mocking subprocess calls
- ProxyState enum (DISABLED, ACTIVE, DEGRADED, FAILED) already exists

### Integration Points
- `openauto_system.py main()`: startup sequence — proxy.cleanup_stale_state() goes before ipc.start()
- `install.sh` / `install-prebuilt.sh`: need `redsocks` system user creation alongside `openauto` group
- `openauto-system.service.in`: ExecStopPost already covers iptables cleanup (belt-and-suspenders)
- `CompanionListenerService.cpp`: calls setProxyRoute() — no changes needed

</code_context>

<deferred>
## Deferred Ideas

- Status reporting truthfulness (ACTIVE only when verified alive) — Phase 17 (SR-01, SR-02, SR-03)
- End-to-end traffic validation on hardware — Phase 18 (PX-05)
- `get_proxy_debug` introspection endpoint — evaluate after Phase 17 if journal logs are insufficient

</deferred>

---

*Phase: 16-routing-correctness-and-idempotency*
*Context gathered: 2026-03-16*
