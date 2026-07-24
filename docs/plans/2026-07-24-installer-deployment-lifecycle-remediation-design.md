# Installer and Deployment Lifecycle Remediation — Design

Status: ACTIVE
Date: 2026-07-24
Grounded against: `origin/main` at `76b3cd103170f736ae8210c95f0cff1bca62b1cf`
Publication: one standalone branch and draft pull request

## 1. Objective

Finish the audit-remediation program with one bounded installer and deployment
lifecycle wave. Source and prebuilt installs will share explicit hardware and
startup contracts, interrupted source work will not escape installer ownership,
prebuilt upgrades will be recoverable, and the Xvfb resolution helper will not
kill a process it does not own.

This wave changes installer scripts, packaged installation assets, operational
tests, and their documentation. It does not change application, QML, protocol,
API, Android Auto, Bluetooth media, or HFP behavior.

## 2. Revalidated current state

Revalidation at the grounded commit produced this bounded matrix:

| Area | Verdict | Current behavior |
|---|---|---|
| Source checkout | Confirmed | `install.sh` ignores the checkout containing the running script and assumes `~/openauto-prodigy`; execution from standard input or a copied standalone script is therefore documented as supported even though it cannot provide the complete source tree. |
| Touch properties | Confirmed | Kernel input-property bitmaps may be hexadecimal, but both installers feed them through decimal-only arithmetic and can abort or misclassify `INPUT_PROP_DIRECT`. |
| Prebuilt upgrade | Confirmed | The application, web configuration service, and system service can continue running from deleted or replaced payload paths while an upgrade mutates those paths in place. |
| Application startup | Confirmed, narrowed | `Type=notify` is already correct, but prebuilt installation does not provide the source install's bounded Wayland readiness or its Bluetooth/PipeWire ordering. |
| WiFi configuration | Confirmed | Prebuilt installation assumes 5 GHz channel 36, does not strictly validate the country code, and does not derive radio/network configuration from live adapter capability with a carrier-independent network contract. |
| Installer interruption | Confirmed | Background package/build work is not placed under an explicit process-group owner and can survive installer error, interrupt, termination, or hangup; the privileged execution path has no central cleanup boundary. |
| Xvfb lock | Confirmed, narrowed | A PID read from `.X99-lock` can be killed without establishing that the current helper launched it; exposure is one unverified PID per invocation. |

Existing installer, systemd-lifecycle, and package tests are green at the
grounded commit, but they do not pin these failure paths. The findings are not
refuted by the existing operations remediation: that earlier work repaired SDP,
optional hostapd coupling, and systemd-owned restarts, while deliberately
leaving compositor ordering and radio selection outside its scope.

## 3. Decisions

### 3.1 Source installation requires the actual complete checkout

`install.sh` derives its source root from the physical location of the running
script, resolves that location before privilege escalation or bulk mutation,
and validates checkout sentinels needed by the build and installation flow. It
does not substitute a maintainer-specific home path.

Execution from standard input cannot establish a complete checkout. A copied
standalone script likewise cannot stand in for one. Both forms fail with an
actionable message before `sudo`, package installation, service mutation, or a
build begins. Documentation removes the unsupported `curl | bash` claim and
directs users to run the installer from a complete checkout.

### 3.2 Installer-owned children have installer-owned lifetimes

Spinner-wrapped package and build commands run in process groups owned by the
current installer. One central cleanup path terminates and reaps only those
groups on normal completion, command failure, shell exit, interrupt, terminate,
or hangup. Cleanup is idempotent, preserves the primary failure status, and
cannot target an unrelated PID or process group.

If the application was active before an in-place source rebuild, the installer
stops it before replacing build/runtime material and restores its prior active
state after success or failure. An application that was inactive remains
inactive.

### 3.3 Hardware detection is a shared packaged contract

One shell library under `config/installer/` is packaged and consumed by both
installers. It provides:

- hexadecimal-safe parsing of kernel input-property bitmaps, including
  `INPUT_PROP_DIRECT`;
- strict country-code validation before configuration is generated;
- live WiFi capability probing for the selected adapter; and
- deterministic 5 GHz/VHT selection when supported, with a valid 2.4 GHz
  fallback when it is not.

Both installation modes render equivalent hostapd and systemd-networkd
configuration from this contract. Interface configuration remains usable
without carrier so the AP bootstrap does not depend on a pre-existing link.
The library has no application-runtime policy role.

### 3.4 Startup uses canonical packaged assets

The application preflight helper and application-unit template are canonical
packaged assets consumed by both installers. The preflight is installed
byte-for-byte; deployment-specific user, UID, and install-root placeholders in
the unit template are rendered deterministically, so identical inputs produce
identical installed units. The unit preserves systemd notification semantics,
orders after Bluetooth and PipeWire availability, and uses a bounded
Wayland-socket condition before process launch.

The Wayland wait is an `ExecCondition`, not a long-running application wrapper.
If the compositor socket does not appear within the bound, systemd skips the
application start without marking the unit failed and without entering a
restart loop. When the socket becomes available, a later start proceeds through
the normal service contract.

### 3.5 Prebuilt upgrades are recoverable transactions

The prebuilt installer extracts into a staging directory and validates required
payload paths, executability, and canonical assets before stopping any service.
It records which managed services were active, stops only those services before
touching their live payload, and swaps managed payload paths through a retained
rollback directory rather than deleting them in place.

Any failure after the stop boundary restores the prior managed payload and unit
assets, reloads systemd when necessary, and restores only the services that were
active at entry. Success removes the superseded rollback material only after the
new payload and units are installed and readiness has been checked. A service
that was initially inactive is never started as a side effect.

This is a managed-payload transaction, not a machine-wide transaction: package
manager state and arbitrary global network changes are not rolled back.

### 3.6 Xvfb cleanup follows launch ownership

`scripts/validate-resolutions.sh` treats `.X99-lock` as evidence to inspect, not
authority to kill. A malformed or genuinely stale lock can be removed safely;
a live occupied display is refused. The helper records the PID of an Xvfb
instance it launches and cleanup may terminate only that recorded child.

No PID is killed merely because it appears in `.X99-lock`, and an unrelated
process cannot become a cleanup target through lock-file contents.

## 4. Acceptance contracts

- A source checkout in any path is used as the source root; stdin and standalone
  copied execution fail before privileged or bulk mutation.
- Documentation contains no supported `curl | bash` installation path.
- All spinner/build process groups owned by an installer are reaped on success,
  failure, `INT`, `TERM`, and `HUP`; unrelated processes are untouched.
- Source rebuild stops and restores only an application that was active at
  entry, including failure cleanup.
- Hexadecimal input-property bitmaps correctly identify direct-touch devices in
  both installers without arithmetic errors.
- Country validation rejects malformed values, and live capability probing
  chooses valid 5 GHz/VHT or 2.4 GHz hostapd configuration consistently in both
  installation modes.
- Generated systemd-networkd configuration brings the selected AP interface up
  independently of carrier state.
- Both installers consume the identical canonical application preflight and
  unit template, render equivalent units for identical deployment inputs, and
  preserve Bluetooth/PipeWire ordering.
- Missing Wayland causes a bounded, successful `ExecCondition` skip with no app
  process and no restart loop; an available socket permits normal readiness.
- Prebuilt validation fails before service stop. Post-stop failure restores the
  exact prior managed payload and the prior active/inactive service set.
- A successful prebuilt upgrade starts only previously active services from the
  new payload and leaves no live process mapped to the retired payload.
- Resolution validation never signals a lock-file PID unless that process was
  launched by the current helper invocation.
- Existing BlueZ SDP compatibility, optional hostapd lifetime, systemd-owned
  restart, IPC single-instance, package contents, and installer selection remain
  unchanged.

## 5. Dynamic verification

Focused shell/Python tests exercise source-root rejection, child cleanup,
application-state restoration, hardware parsing/rendering, payload transaction
rollback, canonical unit/preflight installation, real temporary systemd units,
and Xvfb lock ownership. The final local gate includes:

1. focused installer, package, systemd, and resolution tests;
2. `bash -n` for every changed shell entry point and error-level ShellCheck;
3. full local build;
4. explicit `openauto-prodigy` target build;
5. full `ctest --output-on-failure`;
6. documentation-link, public-reference, and `git diff --check` checks;
7. repository review with every P1/P2/P3 finding adjudicated; and
8. an aarch64 cross-build before live validation.

## 6. Pi/live acceptance matrix

### Required

- Read the live input-device property bitmaps and prove the shared parser makes
  the same direct-touch decision without decimal-only assumptions.
- Probe the Pi's active WiFi adapter and prove the shared contract derives its
  current valid 5 GHz/VHT configuration; pin a synthetic capability fixture
  that derives the valid 2.4 GHz fallback.
- Exercise staged payload validation, successful swap, forced rollback, and
  prior active-state restoration using temporary install roots and temporary
  systemd units. Do not overwrite the production checkout.
- With a temporary application unit, prove absent Wayland does not launch an
  application process, fail the unit, or enter a restart loop; creating the
  expected socket permits normal readiness.
- Preserve the production checkout and configuration. At exit, exactly one
  production application process owns responsive IPC, and hostapd and Bluetooth
  retain their original PIDs and restart counts.

### Optional

- Repeat the transaction test with the web configuration and system service
  initially inactive in different combinations.
- Exercise both an explicit country code and an environment-derived valid
  country code on the live adapter.
- Repeat an application start after removing and recreating the temporary
  Wayland socket.

### Approval and authorization

Matthew's standing authorization covers the Pi inspection, temporary roots and
units, application/service operations required by this matrix, and cleanup. It
also covers pushing the completed branch and opening its draft pull request.
The authorization does not cover merging the pull request.

### Not required

- A full installer overwrite of the production checkout.
- Bluetooth or hostapd daemon restart, re-pairing, HFP call testing, Android
  Auto protocol capture, or unrelated QML inspection.
- A milestone tag, packaged release publication, or companion-repository work.

## 7. Out of scope

- Application C++ behavior, QML, protobuf/API/JS-shim behavior, Android Auto,
  wireless transport, Bluetooth media, HFP, telephony, audio, or equalizer
  behavior.
- Full production-checkout overwrite during live validation.
- Package-manager/apt rollback or general machine-wide transaction support.
- Rollback of arbitrary global network state beyond project-managed files.
- Configuration-schema migration or new application configuration fields.
- Companion-repository work, milestone tags, release packaging/publication, or
  pull-request merge.
- New installer features unrelated to the revalidated matrix.

All project rails in `AGENTS.md` remain binding. New ideas discovered during
execution follow wishlist-then-promote and do not enlarge this wave.
