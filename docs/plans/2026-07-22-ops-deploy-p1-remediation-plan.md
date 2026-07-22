# Operations Deployment Reliability Remediation — Implementation Plan

Status: ACTIVE

**Design (read first):**
`docs/plans/2026-07-22-ops-deploy-p1-remediation-design.md`
**Approved by Matthew:** 2026-07-22
**Grounded against:** `origin/main` at `2366cf88`
**Branch:** `agent/ops-deploy-p1-remediation`
**Publication:** one commit per task; no push until all gates and adjudication
are complete.

## Goal

Deliver prebuilt BlueZ SDP parity, recoverable and optional hostapd coupling,
and a systemd-owned single-instance restart contract without expanding into
other projection, Bluetooth, logging, or deployment findings.

## Global Constraints

- Read root `AGENTS.md` and the nearest nested instructions before each task.
- Preserve wireless-only AA, HFP Hands-Free role, no-ofono, the hands-off
  protocol submodule, frozen API schema, and External API rails.
- Use canonical assets under `config/systemd/`; do not introduce another
  installer-only copy of the same drop-in.
- Update behavior documentation in the commit that changes the behavior.
- One bounded task and commit at a time. Nobody pushes mid-execution.
- Do not deploy, restart Bluetooth/hostapd, or perform any other disruptive Pi
  action without Matthew's explicit approval at the live-validation gate.
- New discoveries remain out of scope and follow wishlist-then-promote.
- Do not edit archived plans or old handoff entries. Closure prepends one new
  handoff and archives this design and plan together.

---

## Task 1 — BlueZ SDP parity

**Tier:** opus
**Commit:** `fix(install): provide BlueZ SDP compatibility in every install mode`

### Files

- Add: `config/systemd/bluetooth-compat.conf`
- Modify: `install.sh`
- Modify: `install-prebuilt.sh`
- Add: `tests/test_ops_install_contracts.py`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_prebuilt_release_package.py`
- Modify: `docs/wireless-setup.md`
- Modify: `docs/aa-protocol/aa-troubleshooting-runbook.md`
- Modify: `docs/pi-config/README.md`

**Out of scope:** Bluetooth discovery retry policy, HFP profiles, hostapd
lifetime, prebuilt WiFi capability detection, and prebuilt compositor ordering.

### Acceptance

- Both installers install the exact canonical compatibility asset.
- Bluetooth compatibility setup runs independently of the no-WiFi early return.
- The prebuilt payload requires and contains the asset.
- The installed fragment resets `ExecStart`, launches the Pi's bluetoothd with
  `--compat`, and makes an existing SDP socket group-writable after startup.
- Current docs no longer claim the prebuilt installer omits SDP compatibility.
- `bash -n install.sh install-prebuilt.sh` passes.
- `python3 tests/test_ops_install_contracts.py --case bluetooth` passes.
- `python3 tests/test_prebuilt_release_package.py` passes.
- `systemd-analyze verify` checks from the focused test pass.
- `git diff --check` passes.

---

## Task 2 — Optional, recoverable hostapd lifetime

**Tier:** opus
**Depends on:** Task 1 committed
**Commit:** `fix(install): decouple application and hostapd lifetimes`

### Files

- Add: `config/systemd/openauto-prodigy-hostapd.conf`
- Add: `config/systemd/hostapd-openauto.conf`
- Modify: `install.sh`
- Modify: `install-prebuilt.sh`
- Modify: `tests/test_ops_install_contracts.py`
- Add: `tests/test_ops_systemd_lifecycle.py`
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/wireless-setup.md`
- Modify: `docs/pi-config/README.md`

**Out of scope:** hostapd radio configuration, networkd/DHCP behavior, password
rotation, application preflight parity, and WiFi reconnect policy.

### Acceptance

- Current installer/runtime assets contain neither
  `BindsTo=hostapd.service` nor reverse
  `PartOf=openauto-prodigy.service`.
- AP-configured installs add a project-owned application drop-in with optional
  `Wants`/`After` ordering and a hostapd drop-in with bounded failure recovery.
- No-AP installs remove project-owned stale coupling and generate an application
  unit with no hostapd relationship.
- A real local user-systemd harness proves that an AP crash replaces only the
  AP process, application restart leaves the AP process unchanged, and the
  no-AP application starts.
- `bash -n install.sh install-prebuilt.sh` passes.
- `python3 tests/test_ops_install_contracts.py --case hostapd` passes.
- `python3 tests/test_ops_systemd_lifecycle.py` passes.
- `systemd-analyze verify` and `git diff --check` pass.

---

## Task 3 — systemd restart and IPC single-instance contract

**Tier:** opus
**Depends on:** Task 2 committed
**Commit:** `fix(runtime): enforce systemd-owned single-instance restarts`

### Files

- Modify: `docs/pi-config/restart.sh`
- Modify: `src/core/services/IpcServer.cpp`
- Modify: `src/core/services/IpcServer.hpp`
- Modify: `src/main.cpp`
- Add: `tests/test_ipc_single_instance.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_ops_install_contracts.py`
- Modify: `tests/test_prebuilt_release_package.py`
- Modify: `docs/pi-config/README.md`
- Modify: `docs/development.md`

**Out of scope:** IPC commands/framing, socket-path migration, web-config
behavior, generic process supervision, or unrelated shutdown ordering.

### Acceptance

- The helper contains no `pkill`, `nohup`, detached launch, fixed sleep,
  maintainer home path, or fixed runtime UID.
- Normal and forced recovery stay inside systemd's service/cgroup ownership.
- A second `IpcServer` cannot unlink or steal a live socket, and the first
  listener remains reachable.
- A genuinely stale Unix socket is removed and rebound.
- A second application process exits before Bluetooth, PipeWire, AA, or input
  resources are initialized.
- The packaged helper is the tested helper.
- `bash -n docs/pi-config/restart.sh tools/package-prebuilt-release.sh` passes.
- `python3 tests/test_ops_install_contracts.py --case restart` passes.
- `python3 tests/test_prebuilt_release_package.py` passes.
- `cmake --build . --target test_ipc_single_instance -j$(nproc)` and
  `ctest -R test_ipc_single_instance --output-on-failure` pass in
  `~/builds/openauto-prodigy`.
- `cmake --build . --target openauto-prodigy -j$(nproc)` passes.
- `git diff --check` passes.

---

## Task 4 — Full verification, live gate, and repository review

**Tier:** main
**Depends on:** Tasks 1–3 committed
**Commit:** none unless an adjudicated fix is required

### Local gate

```bash
bash -n install.sh install-prebuilt.sh docs/pi-config/restart.sh \
  tools/package-prebuilt-release.sh
python3 tests/test_ops_install_contracts.py
python3 tests/test_ops_systemd_lifecycle.py
python3 tests/test_prebuilt_release_package.py
python3 tests/test_install_list_prebuilt.py

cd ~/builds/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure

cd /mnt/e/claude/personal/openautopro/openauto-prodigy
python3 scripts/check-doc-links.py
git diff --check
bash scripts/codex-review.sh origin/main
```

Adjudicate every review finding. Confirmed findings are fixed; dismissals carry
a reason. A substantial confirmed fix triggers one review-gate rerun.

### Live gate

Stop and request Matthew's approval before cross-build, rsync, unit changes, or
service operations. After approval:

- Inspect the effective BlueZ unit and `/var/run/sdp` ownership/mode.
- Inspect the effective app/hostapd relationships and hostapd restart policy.
- Deploy the final reviewed binary and project-owned unit/helper changes.
- Run normal and forced helper restarts. After each, prove the service MainPID
  is the only matching application process and the IPC socket responds.
- Restarting real Bluetooth or hostapd is optional and requires explicit
  approval for that disruptive row.

---

## Task 5 — Closure

**Tier:** main
**Depends on:** Task 4 green and required live row complete
**Commit:** `docs: complete ops deployment reliability remediation`

### Files

- Modify: `docs/roadmap-current.md`
- Modify: `docs/session-handoffs.md` by prepending one entry
- Move this design and plan to `docs/archive/plans/` after marking both
  `COMPLETED 2026-07-22`

### Acceptance

- The handoff records changes, rationale, status, next steps, verification, Pi
  disposition, and review adjudication without exact suite counts.
- Both plans are archived together and no ACTIVE reference remains.
- Link checks, final targeted searches, `git diff --check`, and branch history
  checks pass.
- `git log --oneline --left-right origin/main...HEAD` shows one bounded series
  based on `origin/main`.
- Nothing has been pushed. Publication occurs only after Matthew's go-ahead,
  using this branch for a new draft PR targeting `main`.
