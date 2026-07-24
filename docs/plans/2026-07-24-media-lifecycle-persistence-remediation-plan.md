# Media Lifecycle and Persistence Remediation — Implementation Plan

Status: ACTIVE
Date: 2026-07-24
Design: `docs/plans/2026-07-24-media-lifecycle-persistence-remediation-design.md`
Base: `origin/main` at `043d00e3cf75c366bcb4f6b6895e90ebb1cb78f5`
Branch: `agent/media-lifecycle-persistence-remediation`

## Global constraints

- Execute one bounded task and commit at a time. Nobody pushes mid-execution.
- Read `src/AGENTS.md` before implementation. Read `qml/AGENTS.md` only if an
  approved scope change introduces a QML edit.
- Preserve wireless-only AA, the HFP HF role, no-ofono, frozen numerics/API,
  frozen YAML fields, and the protocol-submodule boundary.
- Fix ownership and persistence at the media-player/scanner boundary; do not add
  compensating behavior in QML, shared widgets, or External API consumers.
- An exact saved track alone owns its saved position. Explicit user transport
  actions alone abandon a pending exact-track restore.
- Eject and shutdown never outlive scanner file ownership.
- Behavior documentation changes ship in the same commit as the behavior.
- Use private audit artifacts only for ledger state; never expose identifiers,
  evidence, links, or backlinks in tracked files.
- A task is complete only after its focused tests are green.

## Task 1 — Activate the media lifecycle remediation wave

Tier: sonnet

Files:

- Add `docs/plans/2026-07-24-media-lifecycle-persistence-remediation-design.md`
- Add `docs/plans/2026-07-24-media-lifecycle-persistence-remediation-plan.md`
- Modify `docs/roadmap-current.md`
- Modify `docs/INDEX.md`

Steps:

1. Record the approved restore, persistence, scanner, eject, shutdown, and test
   integrity contracts without private finding identifiers or evidence.
2. Mark this consolidated wave active in the roadmap and documentation index.
3. Validate current-document links and whitespace.

Acceptance criteria:

- Both new documents are `Status: ACTIVE`, cite the grounded commit and branch,
  and agree on scope and verification.
- Every executable task names exact files, testable acceptance criteria, a test
  command, and an explicit out-of-scope boundary.
- The roadmap and index identify this as the only active implementation wave.
- No tracked document exposes a private identifier, evidence reference, or
  private-artifact backlink.

Test command:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git diff --check
python3 - <<'PY'
from pathlib import Path
for path in (Path("docs/INDEX.md"), Path("docs/roadmap-current.md")):
    text = path.read_text()
    assert "2026-07-24-media-lifecycle-persistence-remediation-design.md" in text
    assert "2026-07-24-media-lifecycle-persistence-remediation-plan.md" in text
PY
```

Out of scope: source, test, QML, Pi, private-ledger, or runtime behavior changes.

Commit: `docs: activate media lifecycle remediation wave`

## Task 2 — Correct restore ownership and user persistence

Tier: main

Files:

- Modify `src/plugins/media_player/MediaPlayerPlugin.hpp`
- Modify `src/plugins/media_player/MediaPlayerPlugin.cpp`
- Add `tests/test_media_player_plugin.cpp`
- Modify `tests/CMakeLists.txt`
- Modify `docs/design-decisions.md`

Steps:

1. Add a focused plugin test seam that drives the production restore and user
   persistence boundary with fake host/config dependencies and deterministic
   media paths.
2. Lock exact-current restore at the saved bounded position and missing-current
   fallback at zero while retaining the raw pending exact-track state.
3. Authorize a late mount retry from pending restore ownership even after a
   restore-time unplayable edge.
4. Clear pending restore before play/pause, next, previous, queue-selection, or
   seek actions take transport ownership.
5. Persist shuffle and repeat directly from their user controls without
   serializing a partial restore queue or writing modes back during startup.
6. Persist a seek from its clamped requested millisecond value and route the
   previous-track seek-to-zero case through the same contract.
7. Document restore ownership and persistence boundaries.

Acceptance criteria:

- The exact saved current track restores paused at its bounded saved position;
  a surviving fallback for a missing current track restores paused at zero.
- A restore-time error leaves the pending exact-track restore retryable until a
  real user transport action takes ownership.
- Play/pause, next, previous, folder/library queue selection, and seek prevent a
  later mount from overwriting the user's state.
- Shuffle and repeat each persist immediately without a later playback edge,
  without a startup write-back, and without writing a partial restore queue.
- A paused or playing seek persists the clamped requested millisecond position
  deterministically.
- Missing/invalid time data retains bounded zero/unknown behavior.
- Existing playback arbitration, metadata, progress, queue ordering, EQ tap,
  audio focus, shared media state, and External API consumption are unchanged.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_media_player_plugin test_media_playback_policy -j$(nproc)
ctest --output-on-failure -R 'test_media_(player_plugin|playback_policy)$'
```

Out of scope: QML changes, new persistence keys, new repeat/queue behavior,
decoder or ring-buffer changes, scanner cancellation, and UDisks policy.

Commit: `fix(media): own restores and persist user transport state`

## Task 3 — Quiesce scanning before eject and shutdown

Tier: main

Files:

- Modify `src/plugins/media_player/MediaScanner.hpp`
- Modify `src/plugins/media_player/MediaScanner.cpp`
- Modify `src/plugins/media_player/MediaPlayerPlugin.hpp`
- Modify `src/plugins/media_player/MediaPlayerPlugin.cpp`
- Modify `tests/test_media_scanner.cpp`
- Modify `tests/test_media_player_plugin.cpp`
- Modify `tests/test_media_usb_policy.cpp`
- Modify `docs/architecture.md`

Steps:

1. Add scanner tests for idempotent quiescence, pending-root cancellation,
   interruption during cache/artwork work, stale completion suppression, clean
   state signals, restart-after-stop, and destruction after explicit stop.
2. Add an owner-thread stop/quiesce operation with identity-safe completion and
   interruption checkpoints across traversal, cached records, artwork, and
   cache rewriting.
3. Add plugin-level ordering tests proving scanner quiescence completes before
   UDisks unmount and before dependent shutdown, and that survivor roots exclude
   the ejecting mount.
4. Order eject as mark/purge, quiesce, request unmount, and survivor rescan
   without reopening the target.
5. Stop new watcher work and quiesce the scanner explicitly during plugin
   shutdown before releasing media/audio dependencies.
6. Document scanner, eject, and shutdown ownership.

Acceptance criteria:

- Stop clears pending work, requests active interruption, releases the worker,
  and returns with `busy()==false` and `scanning()==false`.
- Stop is idempotent; one current scan generation produces coherent state
  notifications, and stale callbacks cannot publish, clear, or restart newer
  work.
- Long root, cache, tag, artwork, and rewrite loops observe interruption at
  bounded checkpoints without forced thread termination.
- Eject does not request UDisks unmount until scanner file ownership has ended;
  survivor scanning cannot reopen the ejecting mount.
- Shutdown during a scan cannot hang, publish through destroyed state, restart
  pending work, or release playback/audio dependencies ahead of the scanner.
- Existing UDisks eligibility, purge/recovery policy, queue ordering, library
  results, cache format, progress reporting, and eject-error reporting remain
  unchanged.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_media_scanner test_media_player_plugin test_media_usb_policy -j$(nproc)
ctest --output-on-failure -R 'test_media_(scanner|player_plugin|usb_policy)$'
```

Out of scope: custom FFmpeg I/O, forced worker termination, UDisks/polkit
redesign, new library features, QML changes, and unrelated plugin lifecycles.

Commit: `fix(media): quiesce scans before eject and shutdown`

## Task 4 — Make media test failures deterministic

Tier: sonnet

Files:

- Modify `tests/test_playback_engine.cpp`
- Modify `tests/test_media_scanner.cpp`

Steps:

1. Replace valid-fixture backend-error skips with failures that retain the
   backend error and fixture context.
2. Preserve skips only for explicitly optional external prerequisites; the
   repository-owned media fixtures are required.
3. Replace the scanner completion helper's assertion-in-expression path with a
   failure-safe wait/result path that cannot access an empty list after timeout.
4. Exercise both success and timeout behavior without increasing production
   timeouts.

Acceptance criteria:

- Every repository-owned valid playback fixture either plays/seeks/stops as
  asserted or fails the test with useful diagnostics; no backend/decode error
  becomes a skip.
- Scanner completion timeout reports a test failure and returns safely without
  empty-result access, assertion-dependent control flow, or undefined behavior.
- Existing playback-engine transport assertions, invalid-file coverage,
  scanner coalescing, cache, artwork, and progress coverage remain unchanged.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_playback_engine test_media_scanner -j$(nproc)
ctest --output-on-failure -R 'test_(playback_engine|media_scanner)$'
```

Out of scope: production playback/scanner behavior, fixture replacement,
backend selection, longer production timeouts, and unrelated tests.

Commit: `test(media): fail deterministically on fixture errors`

## Task 5 — Integration, review, deployment, and closure

Tier: main

Files:

- Modify `docs/roadmap-current.md`
- Modify `docs/INDEX.md`
- Append exactly one entry to `docs/session-handoffs.md`
- Update the ignored private remediation ledger overlay
- Move `docs/plans/2026-07-24-media-lifecycle-persistence-remediation-design.md`
  to `docs/archive/plans/` with `Status: COMPLETED 2026-07-24`
- Move `docs/plans/2026-07-24-media-lifecycle-persistence-remediation-plan.md`
  to `docs/archive/plans/` with `Status: COMPLETED 2026-07-24`

Steps:

1. Run the focused tests from Tasks 2–4.
2. Run the full local build, explicit application target, full suite,
   current-document link/private-reference checks, and whitespace check.
3. Run `bash scripts/codex-review.sh origin/main`; adjudicate every P1/P2/P3.
   Fix confirmed findings and rerun the gate once if a substantial fix lands.
4. Cross-build with `./cross-build.sh`.
5. Preserve Pi rollback material and exact configuration, deploy only the
   application binary, and run every required live row from the design matrix.
6. Restore the exact original Pi configuration and verify one process,
   responsive IPC, normal local media behavior, and unchanged hostapd/Bluetooth
   service lifetimes.
7. Reconcile and recompute the private overlay, update the roadmap/index,
   append one handoff, mark/archive the design and plan, and commit closure.
8. Under Matthew's standing authorization, push the completed branch and open
   a draft pull request. Do not merge it.

Acceptance criteria:

- `cmake --build . -j$(nproc)` passes.
- `cmake --build . --target openauto-prodigy -j$(nproc)` passes.
- `ctest --output-on-failure` passes without an unexpected skip.
- `git diff --check origin/main..HEAD` passes.
- Public documentation contains no private identifier/evidence reference and
  all current-document links resolve.
- Every repository-review finding is fixed or dismissed with a bounded reason.
- `./cross-build.sh` succeeds.
- Every required Pi row in the design passes: exact/fallback restore, pending
  retry and user takeover, seek/mode persistence, scan-time eject, scan-time
  restart, normal media behavior, one responsive process, and unchanged
  hostapd/Bluetooth PIDs and restart counts. Exact configuration and unrelated
  checkout state are preserved.
- The private overlay validates and recomputes; the draft pull request contains
  the whole bounded tranche and remains unmerged.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git diff --check origin/main..HEAD
bash scripts/codex-review.sh origin/main
./cross-build.sh
```

Out of scope: milestone tag/release, merging the pull request, daemon restarts,
re-pairing, HFP/AA protocol capture, companion work, installer/deployment
lifecycle remediation, unrelated Pi cleanup, and optional rows that lack their
required removable-media setup.

Commit: `docs: close media lifecycle and persistence remediation`
