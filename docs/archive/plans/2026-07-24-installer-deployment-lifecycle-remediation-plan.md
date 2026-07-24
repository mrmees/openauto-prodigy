# Installer and Deployment Lifecycle Remediation — Implementation Plan

Status: COMPLETED 2026-07-24
Date: 2026-07-24
Design: `docs/plans/2026-07-24-installer-deployment-lifecycle-remediation-design.md`
Base: `origin/main` at `76b3cd103170f736ae8210c95f0cff1bca62b1cf`
Branch: `agent/installer-deployment-lifecycle-remediation`

## Global constraints

- Execute one bounded task and commit at a time. Nobody pushes mid-execution.
- Read root `AGENTS.md` before every task and any nearer nested instructions
  before changing their subsystem.
- Preserve wireless-only Android Auto, the HFP Hands-Free role, no-ofono, the
  hands-off protocol submodule, frozen API/numerics, and External API rails.
- Source execution must fail closed before privilege escalation or bulk
  mutation when no complete checkout can be established.
- Child cleanup and payload rollback may affect only resources demonstrably
  owned by the current installer invocation.
- Shared hardware, preflight, and application-unit assets are authoritative;
  installers consume them rather than retaining divergent inline copies.
- Behavior documentation changes ship in the commit that changes behavior.
- Public files never expose private audit identifiers, evidence, or backlinks.
- New discoveries follow wishlist-then-promote; this plan does not grow.
- A task is complete only when its focused tests and listed static checks pass.

## Task 1 — Activate the final installer lifecycle wave

Tier: sonnet

Files:

- Add `docs/plans/2026-07-24-installer-deployment-lifecycle-remediation-design.md`
- Add `docs/plans/2026-07-24-installer-deployment-lifecycle-remediation-plan.md`
- Modify `docs/roadmap-current.md`
- Modify `docs/INDEX.md`

Steps:

1. Record the approved source-root, child-ownership, shared-hardware,
   canonical-startup, transactional-upgrade, and Xvfb-ownership contracts.
2. Include the revalidated current-state and required/optional/not-required Pi
   matrix without private identifiers, evidence, or artifact backlinks.
3. Make this the only active implementation wave in the roadmap and index.

Acceptance criteria:

- Both new documents are `Status: ACTIVE`, agree on the grounded commit,
  branch, scope, acceptance contracts, and verification boundary.
- Every task has a `Tier:` field, exact tracked files, testable acceptance
  criteria, a test command, and an explicit out-of-scope line.
- Roadmap and index identify this as the only active wave.
- Changed public documents contain no private audit identifiers, evidence, or
  private-artifact backlinks.

Test command:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git diff --check
python3 scripts/check-doc-links.py
```

Out of scope: installer implementation, tests, Pi operations, private-ledger
mutation, application runtime behavior, or publication.

Commit: `docs: activate installer lifecycle remediation`

## Task 2 — Own source execution and child-process lifetimes

Tier: opus (Matthew's approved worker override: 5.6-terra)

Files:

- Modify `install.sh`
- Add `tests/test_install_source_lifecycle.py`
- Modify `tests/CMakeLists.txt`
- Modify `README.md`
- Modify `docs/development.md`

Steps:

1. Add a shell-harness seam that runs source-root discovery and child cleanup
   against temporary checkouts and fake commands without package mutation.
2. Resolve the running script's physical directory and validate complete
   checkout sentinels before `sudo`, package installation, service changes, or
   build work. Reject stdin and standalone copied execution with actionable
   output.
3. Run spinner/package/build children in installer-owned process groups and
   centralize idempotent cleanup for normal exit, failure, `INT`, `TERM`, and
   `HUP`, preserving the primary status.
4. Record whether `openauto-prodigy.service` was active; stop it before an
   in-place source rebuild and restore only that prior active state on success
   or failure.
5. Remove the unsupported `curl | bash` path and document checkout-relative
   execution and interruption behavior.

Acceptance criteria:

- Running a complete checkout from a path other than the user's home selects
  that physical checkout as the only source/build root.
- Standard-input and standalone-copy execution exit nonzero before any fake
  sudo/package/service/build mutation is observed.
- Success, ordinary failure, `INT`, `TERM`, and `HUP` reap all installer-owned
  child process groups and leave an unrelated sentinel process alive.
- Cleanup is idempotent and returns the initiating command/signal status.
- An initially active application is stopped before mutation and restored on
  success and failure; an initially inactive application stays inactive.
- Current docs contain no supported `curl | bash` source-install command.

Test command:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
bash -n install.sh
shellcheck -S error install.sh
python3 tests/test_install_source_lifecycle.py
git diff --check
```

Out of scope: prebuilt payload swaps, hardware detection, systemd unit design,
package-manager rollback, application code, or service behavior beyond
preserving the source rebuild's prior application active state.

Commit: `fix(install): own source execution and child lifetimes`

## Task 3 — Share hardware detection and network contracts

Tier: opus (Matthew's approved worker override: 5.6-terra)

Files:

- Add `config/installer/hardware-contracts.sh`
- Modify `install.sh`
- Modify `install-prebuilt.sh`
- Modify `tools/package-prebuilt-release.sh`
- Add `tests/test_installer_hardware_contracts.py`
- Modify `tests/test_ops_install_contracts.py`
- Modify `tests/test_prebuilt_release_package.py`
- Modify `tests/CMakeLists.txt`
- Modify `docs/wireless-setup.md`
- Modify `docs/pi-config/README.md`

Steps:

1. Add a sourceable, packaged shell library for numeric bitmap parsing,
   country validation, WiFi capability classification, hostapd rendering, and
   carrier-independent systemd-networkd rendering.
2. Interpret kernel property bitmaps as hexadecimal bit sets and test
   `INPUT_PROP_DIRECT` across zero, decimal-looking, alphabetic-hex, multiword,
   whitespace, malformed, and missing inputs.
3. Strictly validate normalized two-letter country codes before configuration
   rendering.
4. Probe the selected adapter's live capabilities and choose deterministic
   supported 5 GHz/VHT settings or a valid 2.4 GHz fallback.
5. Make both installers use the shared functions and render equivalent
   hostapd/networkd contracts. Include the library in prebuilt packages.

Acceptance criteria:

- Hexadecimal property bitmaps containing the direct-touch bit are recognized
  without arithmetic exceptions; absent, malformed, and unset bits fail safely.
- Both installers produce the same touch classification for identical kernel
  data and contain no decimal-only duplicate parser.
- Invalid country inputs are rejected before managed network files change.
- Synthetic supported-capability fixtures select valid 5 GHz/VHT settings;
  unsupported fixtures select valid 2.4 GHz settings and never hardcode an
  unsupported band/channel.
- Generated hostapd and `.network` files are equivalent between installation
  modes, select the requested interface, and configure it without carrier.
- The prebuilt archive requires and contains the exact shared library.

Test command:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
bash -n config/installer/hardware-contracts.sh install.sh install-prebuilt.sh \
  tools/package-prebuilt-release.sh
shellcheck -S error config/installer/hardware-contracts.sh install.sh \
  install-prebuilt.sh tools/package-prebuilt-release.sh
python3 tests/test_installer_hardware_contracts.py
python3 tests/test_ops_install_contracts.py
python3 tests/test_prebuilt_release_package.py
git diff --check
```

Out of scope: dynamic runtime WiFi policy, AP password rotation, arbitrary
network-manager migration, Bluetooth restart, configuration-schema fields,
application touch handling, or package-manager rollback.

Commit: `fix(install): share hardware and network contracts`

## Task 4 — Transact prebuilt upgrades and canonicalize startup

Tier: main

Files:

- Add `config/installer/openauto-preflight`
- Add `config/systemd/openauto-prodigy.service.in`
- Modify `install.sh`
- Modify `install-prebuilt.sh`
- Modify `tools/package-prebuilt-release.sh`
- Add `tests/test_prebuilt_upgrade_transaction.py`
- Modify `tests/test_ops_install_contracts.py`
- Modify `tests/test_ops_systemd_lifecycle.py`
- Modify `tests/test_prebuilt_release_package.py`
- Modify `tests/CMakeLists.txt`
- Modify `docs/development.md`
- Modify `docs/pi-config/README.md`
- Modify `docs/reference/release-packaging.md`

Steps:

1. Move the application preflight helper and unit template into canonical
   package assets. Install the preflight byte-for-byte and render the template
   through the same deployment-specific substitutions in both modes.
2. Give the unit Bluetooth/PipeWire ordering and a bounded Wayland
   `ExecCondition`. Preserve `Type=notify`; missing Wayland skips application
   launch without failure or restart-loop semantics.
3. Add a prebuilt transaction harness with fake managed services, live payload,
   staged payload, injected failure points, and readiness probes.
4. Stage and validate all required payload/assets before stopping services.
   Capture the initial active state, then stop only active managed services.
5. Swap live managed paths through a recoverable rollback directory. On any
   post-stop failure, restore the prior payload/units, daemon-reload as needed,
   and restore only the original active service set.
6. On success, start only previously active services from the new payload,
   check readiness, and retire rollback material only after the transaction is
   committed.

Acceptance criteria:

- Both installers install the byte-identical canonical preflight and consume
  the same unit template; identical deployment inputs render identical unit
  bytes, and inline divergent copies are absent.
- Unit verification confirms Bluetooth/PipeWire ordering, `Type=notify`, and a
  bounded Wayland `ExecCondition` before `ExecStart`.
- A real temporary-systemd harness proves absent Wayland produces no
  application process, no failed unit, and no restart loop; creating the socket
  permits a later start and readiness notification.
- Invalid or incomplete staging fails before any managed service stop.
- Injected failures at every post-stop boundary restore byte-identical prior
  managed payload/unit content and the exact prior active/inactive service set.
- Success runs previously active services from the new managed payload; no
  process remains mapped to a retired payload, and initially inactive services
  remain inactive.
- Existing SDP, hostapd lifetime, restart helper, IPC singleton, and package
  selection tests remain green.

Test command:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
bash -n config/installer/openauto-preflight install.sh install-prebuilt.sh \
  tools/package-prebuilt-release.sh
shellcheck -S error config/installer/openauto-preflight install.sh \
  install-prebuilt.sh tools/package-prebuilt-release.sh
python3 tests/test_prebuilt_upgrade_transaction.py
python3 tests/test_ops_install_contracts.py
python3 tests/test_ops_systemd_lifecycle.py
python3 tests/test_prebuilt_release_package.py
git diff --check
```

Out of scope: apt/package rollback, arbitrary global network rollback,
production checkout overwrite, application readiness protocol redesign,
hostapd/Bluetooth daemon restart, tagging, or release publication.

Commit: `fix(install): transact upgrades and canonicalize startup`

## Task 5 — Make Xvfb resolution validation ownership-safe

Tier: sonnet

Files:

- Modify `scripts/validate-resolutions.sh`
- Add `tests/test_validate_resolutions.py`
- Modify `tests/CMakeLists.txt`
- Modify `scripts/README.md`

Steps:

1. Add a fake-Xvfb harness covering absent, malformed, stale, unrelated-live,
   and current-invocation lock ownership.
2. Treat a live `.X99-lock` as an occupied display and refuse it without
   sending a signal. Remove only a demonstrably stale or malformed lock.
3. Record the Xvfb child launched by the current invocation and restrict exit
   cleanup to that child.
4. Document display-lock refusal and cleanup ownership.

Acceptance criteria:

- No PID is signaled merely because its text appears in `.X99-lock`.
- A live unrelated lock causes a bounded nonzero refusal and leaves the process
  and lock intact.
- Malformed and stale locks are handled without signaling an unrelated process.
- Normal completion, failure, `INT`, `TERM`, and `HUP` terminate and reap only
  the Xvfb child launched by the current invocation.
- Existing resolution matrix and renderer validation behavior is unchanged.

Test command:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
bash -n scripts/validate-resolutions.sh
shellcheck -S error scripts/validate-resolutions.sh
python3 tests/test_validate_resolutions.py
git diff --check
```

Out of scope: compositor architecture, production display selection, renderer
behavior, application/QML changes, generic PID-lock frameworks, or killing an
occupied display owner.

Commit: `fix(validation): respect Xvfb process ownership`

## Task 6 — Gate, live-validate, close, and publish

Tier: main

Tracked files:

- Modify `docs/roadmap-current.md`
- Modify `docs/INDEX.md`
- Modify `docs/session-handoffs.md` by prepending exactly one entry
- Move `docs/plans/2026-07-24-installer-deployment-lifecycle-remediation-design.md`
  to `docs/archive/plans/2026-07-24-installer-deployment-lifecycle-remediation-design.md`
  with `Status: COMPLETED 2026-07-24`
- Move `docs/plans/2026-07-24-installer-deployment-lifecycle-remediation-plan.md`
  to `docs/archive/plans/2026-07-24-installer-deployment-lifecycle-remediation-plan.md`

Private state: update only the ignored remediation ledger overlay after all
acceptance gates; do not modify immutable private report/evidence inputs and do
not name or link private artifacts from tracked files.

Steps:

1. Run Tasks 2–5 focused checks, shell syntax checks, and error-level
   ShellCheck.
2. Run the full local build, explicit application target, full suite,
   documentation-link/public-reference checks, and whitespace checks.
3. Run `bash scripts/codex-review.sh origin/main`; adjudicate every P1/P2/P3.
   Fix confirmed findings and rerun the gate once after any substantial fix.
4. Cross-build with `./cross-build.sh`.
5. Capture the Pi's production service PIDs/restart counts, responsive IPC,
   exact configuration, and checkout state. Run every required row in the
   design's live matrix using temporary roots/units and no production-checkout
   overwrite.
6. Remove temporary live artifacts and prove one responsive production app
   process, exact configuration preservation, and unchanged hostapd/Bluetooth
   lifetimes.
7. Reconcile the private overlay by immutable array position, recompute its
   counts, update public closure documents, prepend one handoff, and archive the
   approved design/plan together.
8. Under Matthew's standing authorization, push the completed branch and open
   a draft pull request against `main`. Do not merge it.

Acceptance criteria:

- Every focused command from Tasks 2–5 passes.
- `cmake --build . -j$(nproc)` passes.
- `cmake --build . --target openauto-prodigy -j$(nproc)` passes.
- `ctest --output-on-failure` passes.
- `git diff --check origin/main..HEAD`, current-document link checks, and the
  changed-public-document private-reference scan pass.
- Every repository-review finding is fixed or dismissed with a bounded reason
  recorded in the handoff; substantial fixes receive the required rerun.
- `./cross-build.sh` succeeds.
- Live input and WiFi probing, synthetic 2.4 GHz fallback, staged swap,
  injected rollback, prior-service restoration, missing/present Wayland, and
  production-process/IPC rows all pass without production-checkout overwrite.
- Production configuration and unrelated checkout state are preserved;
  hostapd and Bluetooth retain their PIDs and restart counts.
- The ignored overlay validates and recomputes, the design and plan archive
  together, exactly one handoff is added, and the draft pull request remains
  unmerged.

Test command:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
bash -n install.sh install-prebuilt.sh config/installer/hardware-contracts.sh \
  config/installer/openauto-preflight scripts/validate-resolutions.sh
shellcheck -S error install.sh install-prebuilt.sh \
  config/installer/hardware-contracts.sh config/installer/openauto-preflight \
  scripts/validate-resolutions.sh
python3 tests/test_install_source_lifecycle.py
python3 tests/test_installer_hardware_contracts.py
python3 tests/test_prebuilt_upgrade_transaction.py
python3 tests/test_validate_resolutions.py
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
git diff --check origin/main..HEAD
bash scripts/codex-review.sh origin/main
./cross-build.sh
```

Out of scope: application/QML/proto/API/AA/Bluetooth/HFP behavior, full
production-checkout overwrite, apt or arbitrary global-network rollback,
configuration-schema migration, companion-repository work, milestone tags,
release publication, and pull-request merge.

Commit: `docs: complete installer lifecycle remediation`
