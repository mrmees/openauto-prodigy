# Configuration Startup Contract Remediation — Implementation Plan

Date: 2026-07-22  
Status: ACTIVE  
Design: `docs/plans/2026-07-22-config-startup-contract-remediation-design.md`  
Base: `origin/main` at `45f6684e39beb649a1df046f64337a98e8023ab9`

## Execution Rules

- Execute one task and one commit at a time; nobody pushes mid-execution.
- `Tier: opus` tasks use `gpt-5.6-terra` for this plan.
- Preserve the frozen API/protocol, wireless-only, HFP-role, no-ofono, and
  submodule rails in the repository instructions.
- Do not add QML changes. If implementation evidence makes QML necessary, stop
  and return to Matthew with a design amendment first.
- New feature ideas go to the wishlist and do not expand this plan.

## Task 1: Activate the approved tranche

**Tier:** main

**Files:**

- Add: `docs/plans/2026-07-22-config-startup-contract-remediation-design.md`
- Add: `docs/plans/2026-07-22-config-startup-contract-remediation-plan.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`

**Acceptance criteria:**

- The design and plan are ACTIVE and grounded on the current `origin/main`.
- The roadmap promotes only this bounded tranche.
- The index lists both active documents.

**Verification:** `git diff --check`

**Out of scope:** Any behavior or test change.

## Task 2: Enforce the YAML and typed logging configuration boundary

**Tier:** opus

**Files:**

- Modify: `src/core/YamlMerge.hpp`
- Modify: `src/core/YamlConfig.hpp`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `tests/test_yaml_config.cpp`
- Modify: `tests/test_config_service.cpp`
- Modify: `tests/test_config_key_coverage.cpp`
- Modify: `docs/reference/config-schema.md`

**Acceptance criteria:**

- `logging.verbose` defaults to `false`, is scalar-writable, and persists.
- `logging.debug_categories` defaults to an empty sequence and round-trips
  through dedicated typed accessors without changing generic scalar semantics.
- A known mapping replaced by a scalar is quarantined during `load()` and all
  subsequent typed reads use defaults without throwing.
- Valid nested overlays and unknown-key retention remain unchanged.
- ConfigService emits its existing change signal only for accepted scalar
  writes.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_yaml_config test_config_service test_config_key_coverage -j$(nproc)
ctest --output-on-failure -R 'test_(yaml_config|config_service|config_key_coverage)$'
```

**Out of scope:** Generalized sequence writes, logging runtime policy, IPC, QML,
or brightness behavior.

## Task 3: Unify startup, settings, and IPC logging behavior

**Tier:** opus

**Files:**

- Modify: `src/main.cpp`
- Modify: `src/core/services/IpcServer.cpp`
- Add: `tests/test_ipc_logging.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/reference/settings-tree.md`
- Modify: `docs/wishlist.md`

**Acceptance criteria:**

- Startup uses typed logging configuration and preserves the CLI verbose
  override.
- Turning verbose off applies the persisted selective list rather than losing
  it until restart.
- Setting categories through the real IPC socket selects non-verbose mode,
  applies the exact list live, persists it, and returns it through `get_logging`.
- Reloading the written file recreates the same effective configuration.
- A schema-write or save failure returns `ok: false` with a useful error.
- Existing logging filter behavior and category names remain unchanged.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_logging test_config_service test_ipc_logging openauto-prodigy -j$(nproc)
ctest --output-on-failure -R 'test_(logging|config_service|ipc_logging)$'
```

**Out of scope:** Logging formatting/destinations, new categories, web/QML
layout, or unrelated IPC handlers.

## Task 4: Apply the default-valued initial brightness once

**Tier:** opus

**Files:**

- Modify: `src/core/services/DisplayService.hpp`
- Modify: `src/core/services/DisplayService.cpp`
- Modify: `tests/test_display_service.cpp`
- Modify: `docs/reference/config-schema.md`

**Acceptance criteria:**

- The first assignment of 80 reaches the selected backend exactly once.
- That default-valued initialization emits no `brightnessChanged` signal.
- A repeated 80 is suppressed; a changed value applies once and emits once.
- Existing clamp, dim-overlay, hardware detection, and property defaults remain
  unchanged.

**Test command:**

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_display_service openauto-prodigy -j$(nproc)
ctest --output-on-failure -R '^test_display_service$'
```

**Out of scope:** Backend discovery, asynchronous DDC result reporting, QML, or
display-settings redesign.

## Task 5: Complete local gates and repository review

**Tier:** main

**Files:** No planned changes. Confirmed review findings may produce bounded
fix commits with their own focused verification.

**Acceptance criteria:**

- `cmake --build . -j$(nproc)` passes in `~/builds/openauto-prodigy`.
- The explicit `openauto-prodigy` target passes.
- `ctest --output-on-failure` passes.
- `git diff --check` and documentation-link checks pass.
- `bash scripts/codex-review.sh origin/main` completes, and every P1/P2/P3
  finding is either fixed or dismissed with a recorded reason.

**Out of scope:** Unrelated cleanup identified during review.

## Task 6: Cross-build and approval-gated Pi validation

**Tier:** main

**Files:** No planned tracked changes.

**Acceptance criteria:**

- `./cross-build.sh` succeeds.
- No Pi mutation occurs without Matthew's separate approval.
- If deployment is approved, execute the required live matrix from the design,
  restore the original Pi configuration, and preserve unrelated dirty files.

**Out of scope:** Daemon restart/re-pairing, HFP calls, AA protocol capture, or
unrelated service changes.

## Task 7: Close and archive

**Tier:** sonnet

**Files:**

- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move: the design and plan from `docs/plans/` to `docs/archive/plans/`

**Acceptance criteria:**

- Closure reflects only verified results and includes review adjudication.
- Exactly one session handoff is appended.
- Both plan documents are marked `COMPLETED 2026-07-22` and archived in the
  same commit.
- Internal remediation state is reconciled separately without a public
  backlink.

**Verification:** `git diff --check` plus documentation-link checks.

**Out of scope:** Publishing, tagging, or release creation.

