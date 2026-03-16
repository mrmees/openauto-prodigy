# Phase 17: Status Reporting Hardening - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Make `get_proxy_status` reflect actual local pipeline health — redsocks listener alive, iptables redirect rules in place — not just cached in-memory state. Startup, apply, runtime, and cleanup failures produce honest, distinguishable states with actionable detail.

</domain>

<decisions>
## Implementation Decisions

### Verification scope
- **ACTIVE gate requires two local checks:**
  1. **Listener check:** TCP connect to `127.0.0.1:{redirect_port}` succeeds (redsocks is accepting connections)
  2. **Routing check:** OPENAUTO_PROXY chain exists, OUTPUT→OPENAUTO_PROXY jump exists, and REDIRECT `--to-ports {redirect_port}` rule exists in the chain
- **PID is diagnostic context, not a truth source** — include in logs/debug, but don't gate ACTIVE on PID alone
- **Upstream SOCKS probe is a secondary signal** — local pipeline OK + upstream OK = ACTIVE; local OK + upstream bad = DEGRADED; local bad = FAILED regardless of upstream
- Do NOT parse individual exemption rules at runtime — exemption correctness is Phase 16's responsibility and covered by tests

### State transition semantics
- **Four states only:** DISABLED, ACTIVE, DEGRADED, FAILED (no ENABLING/DISABLING)
- **ACTIVE:** local listener verified + local routing verified (+ upstream OK)
- **DEGRADED:** local pipeline intact, upstream SOCKS unreachable
- **FAILED:** local pipeline not functional (listener down, routing missing, apply failure, cleanup failure)
- **DISABLED:** cleanup fully succeeded, no active/stale routing state
- **enable() transition:** DISABLED → setup → immediate local verification → ACTIVE if verification passes, FAILED if not. Setup success alone is NOT enough for ACTIVE.
- **Runtime drift:** health loop detects local failure → immediate FAILED (no DEGRADED threshold for local checks; thresholds only make sense for noisy remote probes)
- **disable() transition:** ACTIVE/DEGRADED/FAILED → teardown → DISABLED if fully clean, FAILED if any cleanup step fails

### Response shape (get_proxy_status)
- **Single response contract for all callers:**
  ```json
  {
    "state": "active",
    "checks": {
      "listener": true,
      "iptables": true,
      "upstream": false
    },
    "error_code": null,
    "error": null,
    "verified_at": "2026-03-15T21:34:12Z",
    "live_check": false
  }
  ```
- **`checks`:** coarse boolean signals — `listener`, `iptables`, `upstream`. No sub-fields for chain/jump/redirect individually.
- **`error_code`:** stable, machine-readable. Priority order when multiple checks fail: `listener_down` > `routing_missing` > `upstream_unreachable`
- **`error`:** short human-readable summary (e.g., "redsocks is not listening on 127.0.0.1:12345")
- **`live_check`:** true if this response came from a live verification pass, false if cached
- **`verified_at`:** ISO 8601 timestamp of the last verification pass that produced this state

### Live vs cached status
- **Default:** `get_proxy_status()` returns cached state instantly (background loop keeps it fresh)
- **Optional:** `get_proxy_status({verify: true})` runs a synchronous local verification pass before responding
- **Background loop interval:** 10 seconds (local TCP connect + iptables check is cheap; 30s was too stale)
- **No auto-recovery in health loop** — detect and report only. Recovery requires explicit `enable()` from caller. This phase is about truthful status, not self-healing.

### Concurrency and race behavior
- **One `asyncio.Lock`** guards: `enable()`, `disable()`, live `verify:true`, and background health tick
- **get_proxy_status() without verify:** reads cached state immediately (no lock needed)
- **get_proxy_status({verify: true}):** acquires lock, runs live checks, returns settled result
- **Health loop:** acquires lock before probing. If enable/disable is in flight, waits or skips that cycle. Prevents the loop from observing intentionally partial state during operations.
- **Lock timeout:** 10 seconds. If exceeded, return current cached state with `error_code: "timeout"`

### Observability and testability
- **Logging policy:**
  - Healthy steady-state: no output
  - Any failed check on a tick: WARNING with check results
  - State transition (ACTIVE→FAILED etc.): WARNING with failed checks and error_code
  - Repeated identical failures: suppress if unchanged
  - Recovery (FAILED/DEGRADED→ACTIVE): INFO
  - Full per-tick detail: available at DEBUG
- **Injectable verification seam:** verification logic (TCP connect, iptables parsing) must be mockable. Split into small async methods or a verifier object returning normalized results (`listener_ok`, `routing_ok`, `upstream_ok`, `error_code`, `error`). Tests mock this boundary, not half of asyncio + subprocess.

### Claude's Discretion
- Exact iptables output parsing for chain/jump/REDIRECT detection
- Internal structure of the verifier (methods on ProxyManager vs separate class)
- Exact log message formatting
- Test structure and granularity beyond the injectable seam requirement
- Whether to suppress repeated failures via counter or timestamp

### Not At Claude's Discretion
- TCP connect is the listener truth gate (not PID alone)
- ACTIVE requires post-setup verification (not optimistic)
- Local failure = immediate FAILED (no threshold)
- Upstream failure with local OK = DEGRADED (not FAILED)
- One asyncio.Lock for all state-mutating operations
- Health loop respects the operation lock
- No auto-recovery in health loop
- Response shape: state + checks + error_code + error + verified_at + live_check

</decisions>

<specifics>
## Specific Ideas

- "ACTIVE means 'we proved the local pipeline is alive right now'" — the core contract for SR-01
- "If the Pi's own redirect pipeline is broken, calling it DEGRADED muddies the meaning of the state" — local failure is always FAILED
- "upstream is a different failure domain" — DEGRADED reserves meaning for secondary signals the Pi can't fix
- "thresholds make sense for noisy remote dependencies, not for 'does the local listener exist'" — no retry/threshold before FAILED on local checks
- "auto-recovery will blur whether the pipeline is actually stable" — detect and report, don't silently fix
- "parsing human text is garbage" — error_code for machine consumers, error for humans, checks for the full picture
- "health loop probing during enable() observes intentionally partial state — that's bullshit signal" — lock all state-affecting paths

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ProxyManager` (`proxy_manager.py`): enable/disable/cleanup_stale_state, _health_loop, _probe_once. Needs verification logic added and health loop redesigned.
- `ProxyState` enum: DISABLED, ACTIVE, DEGRADED, FAILED — already exists, no changes needed
- `_run_cmd()` async wrapper: used for subprocess calls, established mock pattern
- `_set_state()` with callback: state notification pattern, can carry error context

### Established Patterns
- `_run_cmd = AsyncMock(return_value=(0, ""))` for mocking subprocess calls in tests
- State callback pattern for external notification
- JSON-RPC style IPC (newline-delimited JSON over Unix socket)

### Integration Points
- `handle_get_proxy_status` in `openauto_system.py` (line 186): currently returns `{state: proxy.state.value}` — needs expanded response
- `handle_set_proxy_route` in `openauto_system.py`: enable() call site — no change needed (enable() internally verifies now)
- `SystemServiceClient.cpp`: Qt client that polls status — may need minor update to parse new response fields
- `test_proxy_manager.py`: 14 existing tests, needs extension for verification and health check behavior

</code_context>

<deferred>
## Deferred Ideas

- Auto-recovery for crashed redsocks or missing iptables rules — separate phase with explicit policy and tests
- `get_proxy_debug` introspection endpoint with full rule details — evaluate after Phase 17 if journal logs are insufficient
- End-to-end traffic validation on hardware — Phase 18 (PX-05)

</deferred>

---

*Phase: 17-status-reporting-hardening*
*Context gathered: 2026-03-15*
