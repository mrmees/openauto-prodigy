# Current Documentation Drift Remediation — Implementation Plan

Status: ACTIVE

**Design (read first):**
`docs/plans/2026-07-21-documentation-drift-remediation-design.md` — approved by
Matthew 2026-07-21.  
**Grounded against:** `origin/main` at `1157de0`.  
**Execution order:** Task 1 through Task 5, then Task 6 closure.  
**Publication:** local commit per task; push only after the complete docs series
passes its review gate.

## Goal

Reconcile current repository guidance with the shipped implementation in five
bounded subsystem passes, without changing runtime behavior or rewriting
history.

## Global Constraints

- Read root `AGENTS.md`, `docs/plans/README.md`, and the nearest contributor
  instructions before each task.
- Do not edit any existing file under `docs/archive/`.
- Do not rewrite old entries in `docs/session-handoffs.md`; append only during
  Task 6.
- Do not add product ideas to the roadmap. Preserve wishlist-then-promote.
- Do not edit executable C++, QML, shell, Python, configuration, or build files.
  Contributor `AGENTS.md` files and documentation snapshots are allowed.
- Base every correction on direct current-code/config/QML evidence. Record a
  stale, duplicate, or disproven premise rather than forcing a textual change.
- Keep public prose free of internal review provenance and sensitive
  operational details.
- Use `git add` with explicit paths. Nobody pushes mid-execution.
- A runtime behavior discrepancy is out of scope: stop and report it instead of
  changing code.

## Baseline

Run before Task 1:

```bash
python3 scripts/check-doc-links.py
git diff --check
```

Record the result for the final handoff. No compile is required for this
docs-only branch.

---

## Task 1 — Current status, roadmap, and wishlist

**Tier:** sonnet  
**Depends on:** baseline  
**Commit:** `docs: reconcile current delivery status`

### Files

- Modify: `docs/roadmap-current.md`
- Modify: `docs/wishlist.md`
- Modify: `docs/INDEX.md`
- Move to `docs/archive/plans/2026-07-08-media-player-design.md` with a
  completed header only when its shipped and bench-complete state is
  demonstrable:
  `docs/plans/2026-07-08-media-player-design.md`
- Modify only when its header/body remains stale:
  `docs/plans/2026-07-05-phase-f-light-plans.md`

**Out of scope:** changing priorities, promoting new work, editing old handoffs,
or editing archived plans.

### Work

1. Compare status prose with current plan headers, implementation presence,
   merged history, and release tags.
2. Move completed delivery statements out of “Now” and remove pending clauses
   that have already been satisfied; keep genuinely open work explicit.
3. Close or consolidate wishlist entries only where their described remedy is
   already present. Preserve still-open follow-up behavior instead of treating a
   partial ship as complete.
4. Make `docs/INDEX.md` describe only ACTIVE files as current guidance and give
   accurate per-plan status.
5. Do not touch historical handoff prose; Task 6 adds the current state.

### Acceptance

- Roadmap “Now” contains no already-shipped phase as pending work.
- Done entries do not retain contradicted pending gates.
- Wishlist entries do not ask for an already-present component or deleted test;
  unresolved behavior stays open in accurate terms.
- Plan headers, `docs/INDEX.md`, and roadmap references agree.
- `python3 scripts/check-doc-links.py` and `git diff --check` pass.

---

## Task 2 — Architecture and design guidance

**Tier:** sonnet  
**Depends on:** Task 1 committed  
**Commit:** `docs: align architecture guidance with implementation`

### Files

- Modify: `docs/architecture.md`
- Modify: `docs/design-decisions.md`
- Modify: `docs/design-philosophy.md`

### Required comparisons

- `src/main.cpp` and the service interfaces/registries for composition and
  plugin-facing service access
- `src/core/aa/`, `src/core/video/` where present, and decoder/orchestrator code
  for transport, threading, codec, fullscreen, and touch behavior
- `qml/` navigation/settings structure and widget registration code
- `web-config/` and web-widget hosting code for web-surface statements

**Out of scope:** changing architectural decisions or implementation to match an
old document.

### Acceptance

- No current architecture prose describes removed ASIO threads, deleted
  services, obsolete touch wiring, or software-only video decoding.
- AA connection defaults and constants agree with code/config.
- Fullscreen, widget seeding/catalog, service inventory, and web-surface claims
  match their current owners.
- Counts are avoided where a stable capability description is clearer; any
  retained count is proved from registration code.
- `python3 scripts/check-doc-links.py` and `git diff --check` pass.

---

## Task 3 — Reference documentation

**Tier:** sonnet  
**Depends on:** Task 2 committed  
**Commit:** `docs: refresh settings and extension references`

### Files

- Modify: `docs/reference/settings-tree.md`
- Modify: `docs/reference/config-schema.md`
- Modify: `docs/reference/plugin-api.md`
- Modify: `docs/reference/widget-developer-guide.md`
- Modify: `docs/reference/release-packaging.md`

### Required comparisons

- QML settings pages plus `test_settings_menu_structure` for menu hierarchy and
  controls
- `src/core/YamlConfig.cpp`, service accessors, shipped YAML, installer writes,
  and config coverage tests for keys/defaults/examples
- `IHostContext`, service interfaces, equalizer enums, media-status providers,
  and widget registries for extension contracts
- Top-level CMake registration for widget build instructions
- `tools/package-prebuilt-release.sh` and packaging tests for archive layout

### Acceptance

- The settings tree mirrors the shipped pages and contains no removed sidebar
  page.
- The schema example and tables cover the actual public configuration tree and
  contain no invented key or environment override.
- Theme identifiers/examples, equalizer stream names, media source values, and
  host services match code.
- Widget CMake instructions are internally consistent and copyable.
- The release layout includes every packager-required payload.
- Targeted search is clean in these current references:

  ```bash
  rg -n 'nav_strip|display\.(orientation|width|height)|OPENAUTO_CONFIG_PATH|5288|Video > Sidebar' docs/reference
  ```

  Acceptance: no matches, except a match explicitly proved to describe a
  different current concept.
- `python3 scripts/check-doc-links.py` and `git diff --check` pass.

---

## Task 4 — Contributor AGENTS guidance

**Tier:** sonnet  
**Depends on:** Task 3 committed  
**Commit:** `docs: correct contributor subsystem rules`

### Files

- Modify: `AGENTS.md`
- Modify: `src/core/aa/AGENTS.md`
- Modify: `qml/AGENTS.md`
- Inspect only: `src/AGENTS.md`

### Required comparisons

- `scripts/codex-review.sh` for the pinned review command
- AA service-discovery/display configuration and decoder paths for supported
  dimensions/codecs
- Current Navbar QML and evdev routing for navigation terminology
- Current `VideoFramePool` for the already-corrected backing-storage rule

### Acceptance

- The escalation example pins the model promised by its heading.
- AA guidance distinguishes configurable UI/display dimensions from negotiated
  video modes and covers both shipped decoder codecs accurately.
- QML guidance uses current Navbar terminology and preserves the evdev input
  ownership rule.
- `src/AGENTS.md` remains unchanged if its pool-lifetime wording is already
  correct; that disposition is recorded for closure.
- `rg -n 'Sidebar|ASIO|H\.264-only|1280x720.*invariant|1024x600.*invariant' AGENTS.md src/core/aa/AGENTS.md qml/AGENTS.md`
  returns no stale guidance.
- `git diff --check` passes.

---

## Task 5 — Development, setup, and troubleshooting

**Tier:** sonnet  
**Depends on:** Task 4 committed  
**Commit:** `docs: update setup and troubleshooting guides`

### Files

- Modify: `docs/development.md`
- Modify: `docs/wireless-setup.md`
- Modify: `docs/how-to/testing-reconnect.md`
- Modify: `docs/how-to/debugging-notes.md`
- Modify: `docs/pi-config/README.md`
- Modify if needed: `docs/pi-config/bluetooth-service-override.conf`
- Delete if confirmed unused: `docs/pi-config/dnsmasq.conf`
- Delete if confirmed unused: `docs/pi-config/dnsmasq-note.txt`

### Required comparisons

- `install.sh`, `CMakeLists.txt`, and systemd integration for dependencies
- `scripts/` inventory for runnable reconnect/test commands
- current hostapd/systemd-networkd and Bluetooth service templates for setup
- current connection config/defaults for ports
- evdev touch implementation for troubleshooting status

**Out of scope:** modifying `docs/pi-config/restart.sh`, service behavior,
installer behavior, or network/Bluetooth policy.

### Acceptance

- Dependency instructions include required systemd development support and do
  not install an unused DHCP daemon.
- Reconnect testing names only scripts/commands that exist now.
- Manual Bluetooth setup includes the required compatibility mode using the
  same sanitized shape as the installer.
- Connection examples and troubleshooting use the current configured port.
- Pi snapshot README accurately identifies configuration ownership and avoids a
  stale machine-specific status/IP claim.
- Obsolete touch calibration is removed or clearly historical, not ACTIVE.
- Confirmed-unused dnsmasq snapshots are removed and no current guide links to
  them.
- Targeted search is clean:

  ```bash
  rg -n '5288|dnsmasq|testing-reconnect\.sh|Touch Calibration \(ACTIVE\)' \
    docs/development.md docs/wireless-setup.md docs/how-to docs/pi-config
  ```

  Acceptance: no stale matches; any required historical/negative explanation is
  adjudicated explicitly.
- `python3 scripts/check-doc-links.py` and `git diff --check` pass.

---

## Task 6 — Adjudication, review gate, and closure

**Tier:** main  
**Depends on:** Tasks 1–5 committed  
**Commit:** `docs: complete documentation drift remediation`

### Files

- Modify: `docs/session-handoffs.md` by prepending one entry
- Move:
  `docs/plans/2026-07-21-documentation-drift-remediation-design.md` to
  `docs/archive/plans/`
- Move:
  `docs/plans/2026-07-21-documentation-drift-remediation-plan.md` to
  `docs/archive/plans/`

### Work

1. Reconcile every scoped documentation discrepancy with a disposition:
   corrected in this series, already resolved on the base, superseded
   historical record, duplicate, or disproven. Do not silently omit one.
2. Run the full current-guidance searches. Exclude `docs/archive/` explicitly;
   historical matches are not defects.
3. Run:

   ```bash
   python3 scripts/check-doc-links.py
   git diff --check
   bash scripts/codex-review.sh origin/main
   ```

4. Adjudicate every review finding. Fix confirmed docs issues; record dismissal
   reasons. Re-run once if a substantial fix changes the reviewed range.
5. Prepend the completion handoff with verification and adjudication summary.
6. Mark both plan files `COMPLETED 2026-07-21` and move them to
   `docs/archive/plans/` in this commit.
7. Confirm the final branch contains documentation changes only and remains
   independent of the runtime follow-up range.

### Final acceptance

- Every scoped discrepancy has an explicit disposition.
- No current-guide stale-name/key/port/script/menu/status search is left
  unadjudicated.
- Link checker, diff checker, and repository review gate pass.
- `git diff --name-only origin/main..HEAD` contains only approved documentation,
  contributor-guidance, and snapshot paths.
- `git log --oneline --left-right origin/main...HEAD` proves the series is based
  on `origin/main` and contains no runtime follow-up commit.
- The plans are archived as completed and a new handoff is the newest entry.
