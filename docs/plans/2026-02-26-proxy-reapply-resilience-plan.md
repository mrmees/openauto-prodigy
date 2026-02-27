# SOCKS5 Route Reapply Resilience Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Ensure companion-reported active SOCKS5 proxy state is re-applied when the system daemon reconnects, while avoiding unnecessary route reconfiguration for unchanged state.

**Architecture:** Prodigy keeps the existing Qt↔Python IPC bridge, with `CompanionListenerService` controlling when to call `set_proxy_route` and `ProxyManager` handling daemon-side state transitions. This change focuses on state reconciliation and validation.

**Tech Stack:** C++/Qt 6, QML, Python asyncio, pytest

---

### Task 1: Make companion service state reconciliation deterministic

**Files:**
- Modify: `/home/matt/claude/personal/openautopro/openauto-prodigy/src/core/services/CompanionListenerService.hpp`
- Modify: `/home/matt/claude/personal/openautopro/openauto-prodigy/src/core/services/CompanionListenerService.cpp`

**Step 1: Write/extend tests**

- Add a unit-style test in `tests/test_companion_listener.cpp` that emits a `status` update with SOCKS5 active, connects a fake `SystemServiceClient`/mock, and verifies route is requested when first status is received and when the system service reconnects after disconnect.

**Step 2: Run test to confirm failure**

Run:
```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
cmake --build build
ctest --test-dir build -R CompanionListener
```

**Step 3: Implement minimal code changes**

- Add tracking fields in `CompanionListenerService`:
  - `QString requestedProxyHost_`
  - `int requestedProxyPort_ = 0`
  - `bool proxyRouteApplied_ = false`
- On each status packet:
  - Parse/validate SOCKS5 payload
  - Compute desired host from `client_->peerAddress()`
  - Apply route only when desired host/port changes, when `internetAvailable_` flips, or when no applied state exists
  - Clear applied state on disconnect
- Add a new method/slot to resync route state when system service connects and companion status says active.
- Emit internetChanged once per state transition (`false`→`true`/`true`→`false`).

**Step 4: Re-run tests and iterate**

- Confirm route reconciliation test passes and existing companion tests still pass.

**Step 5: Commit**

```bash
git add src/core/services/CompanionListenerService.hpp src/core/services/CompanionListenerService.cpp tests/test_companion_listener.cpp
git commit -m "fix(proxy): reconcile socks5 route state on reconnect"
```

### Task 2: Strengthen daemon validation for proxy routing payload

**Files:**
- Modify: `/home/matt/claude/personal/openauto-prodigy/system-service/openauto_system.py`

**Step 1: Write test**

Add a unit test in `system-service/tests/test_openauto_system.py` for:
- missing `host`
- missing `password`
- `port` out of bounds

**Step 2: Run test to confirm failure**

Run:
```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/system-service
.venv/bin/python -m pytest tests/test_openauto_system.py -v
```

**Step 3: Implement validation in `handle_set_proxy_route`**

- Return `ok=false` with explicit error when active requests are malformed.
- Keep enable/disable state transitions unchanged.

**Step 4: Run the test suite for system-service changes**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/system-service
.venv/bin/python -m pytest tests/test_proxy_manager.py tests/test_openauto_system.py -v
```

**Step 5: Commit**

```bash
git add system-service/openauto_system.py system-service/tests/test_openauto_system.py
git commit -m "fix(proxy): validate set_proxy_route params in daemon"
```

