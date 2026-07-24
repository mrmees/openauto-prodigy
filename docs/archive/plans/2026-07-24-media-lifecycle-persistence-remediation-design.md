# Media Lifecycle and Persistence Remediation — Design

Status: COMPLETED 2026-07-24
Date: 2026-07-24
Grounded against: `origin/main` at `043d00e3cf75c366bcb4f6b6895e90ebb1cb78f5`

## 1. Objective

Repair a bounded set of local-media restoration, persistence, scanner,
safe-eject, shutdown, and test-integrity defects without redesigning the media
player. The work establishes three ownership boundaries:

1. an exact saved track owns its saved position, while an available fallback
   track starts at zero;
2. explicit user transport actions take ownership away from a pending boot
   restore; and
3. `MediaScanner` must be quiescent before an eject or application shutdown can
   release its dependent resources.

This is one consolidated implementation tranche and one pull request.

## 2. Revalidated current state

Revalidation against the grounded commit confirmed these defects:

- A partial boot restore selects the first surviving queue entry when the saved
  current track is absent, then incorrectly applies the missing track's saved
  position to that fallback.
- Three valid-media playback-engine tests convert backend errors into skipped
  tests, allowing a real decode or integration regression to appear green.
- Safe eject unloads playback and purges the volume, but an in-flight scanner
  can still hold a target file when the asynchronous UDisks unmount begins.
- A restore-time unplayable edge clears `PlaybackPolicy` restoration state;
  with a partial queue still loaded, a later USB mount cannot retry the pending
  exact-track restore even though no user action took over.
- Shuffle and repeat mutations are not persisted until a later playback-state
  edge or shutdown.
- `seekTo()` neither takes ownership from a pending restore nor persists the
  requested paused position deterministically.
- Scanner teardown depends on its destructor requesting interruption and then
  waiting; directory, cache, and artwork loops do not consistently observe
  interruption, and plugin shutdown has no explicit scanner-stop boundary.
- The scanner test completion helper can continue after a timeout assertion and
  access an empty completion list.

Focused playback-engine, playback-policy, scanner, and USB-policy tests are
green at the grounded commit. The local valid-media fixture decodes with the
configured Qt Multimedia FFmpeg backend, so the skip behavior is a latent test
integrity defect rather than an unavailable local prerequisite.

## 3. Decisions

### 3.1 Saved position belongs only to the exact saved track

Restore filters the saved queue to currently available files. If the saved
current track survives, that exact entry resumes paused at the bounded saved
position. If it does not survive, the first surviving entry is adopted paused
at position zero. The raw saved queue, current path, and position remain
pending so a later USB mount can restore the exact track if the user has not
taken ownership.

Missing, invalid, negative, or out-of-range time data retains the existing
bounded zero/unknown behavior. Restore never starts audible playback by itself.

### 3.2 Pending restore has explicit ownership

The presence of a pending exact-track restore, rather than
`PlaybackPolicy::restoring()` alone, authorizes a later mount retry. A
restore-time playback error may stop the fallback, but it does not silently
become a user decision and therefore does not discard retry eligibility.

Explicit play/pause, next, previous, queue selection, and seek actions take
ownership and clear the pending restore before mutating transport state. A
later mount cannot overwrite that user choice. Existing new-queue actions keep
their current takeover semantics.

### 3.3 User mode and seek mutations persist at their boundary

Shuffle and repeat controls persist their own values immediately. They do not
serialize a partially restored queue merely because a mode changed during a
pending restore, and applying persisted modes during startup does not create a
write-back edge.

A user seek persists the clamped requested millisecond position together with
the post-takeover current queue identity. It does not depend on a later,
asynchronous `QMediaPlayer::position()` notification. The previous-track
gesture's seek-to-zero path uses the same user-seek contract. Existing periodic
playing-state persistence remains unchanged.

### 3.4 Scanner quiescence is an explicit lifecycle operation

`MediaScanner` exposes one idempotent owner-thread stop/quiesce operation. It
clears pending roots, requests interruption of the active generation, waits for
that generation to release its files, and leaves `busy()` and `scanning()`
false. Completion handling is generation- or identity-checked so a stale
queued callback cannot clear, publish, or restart newer work.

Interruption checkpoints cover root and directory traversal, cached-record
processing, tag/artwork group and member loops, and cache rewriting. A single
third-party file-open call may finish before the next checkpoint; cancellation
does not introduce custom AVIO or kill a worker thread.

### 3.5 Eject and shutdown wait for file ownership to end

Eject first marks the mount as ejecting and purges it from playback, queue, and
library state. Before requesting UDisks unmount, the plugin quiesces the current
scan so no target file remains owned. It then rebuilds scanner work from only
the surviving roots. Failure reporting and the existing asynchronous UDisks
operation remain authoritative.

Plugin shutdown stops new volume callbacks, quiesces the scanner explicitly,
and only then tears down playback/audio and media objects. Repeated stop and
shutdown calls are safe and cannot restart queued scanner work.

### 3.6 Valid fixtures must fail honestly

Tests using repository-owned valid media treat any backend error as a failure
with useful diagnostics. Skips remain appropriate only for an explicitly
optional external prerequisite, not for a configured project fixture. Scanner
wait helpers terminate the current test path safely on timeout and never read
an absent result.

## 4. Acceptance contracts

- An exact saved current track restores paused at its bounded saved position.
- A surviving fallback for a missing saved current track restores paused at
  zero, never at the missing track's position.
- A pending exact-track restore remains retryable after a restore-time
  unplayable edge when no user action has taken ownership.
- Play/pause, next, previous, queue selection, and seek clear pending restore
  ownership before changing transport state.
- Shuffle and repeat persist on their user mutation without serializing a
  partial restore queue or writing back during startup restoration.
- A seek persists the clamped requested millisecond position even while paused,
  and a later mount cannot overwrite it.
- Scanner quiescence releases active files, cancels pending work, emits coherent
  observable state, and cannot publish a stale generation.
- UDisks unmount is not requested until scanner file ownership has ended; a
  surviving-root rescan cannot reopen the ejecting mount.
- Shutdown during traversal, cache processing, or artwork extraction completes
  without a hang, use-after-free, stale result, or restarted scan.
- Valid playback fixtures report backend/decode errors as failures, not skips.
- Scanner completion timeouts fail safely without empty-result access.
- Existing playback arbitration, metadata, controls, EQ tap, audio focus,
  progress units, shared now-playing state, External API reporting, USB watcher
  eligibility, queue ordering, and UDisks error reporting remain unchanged.

## 5. Dynamic verification

Focused tests drive the real media-plugin restore/persistence boundary,
playback policy, scanner worker lifecycle, USB policy, and playback engine. The
final repository gate is:

1. focused target builds and tests;
2. full local build;
3. explicit `openauto-prodigy` target build;
4. full `ctest --output-on-failure`;
5. `git diff --check` and current-document link/private-reference checks;
6. repository review gate with every finding adjudicated;
7. aarch64 cross-build; and
8. binary-only Pi deployment and live media-lifecycle checks.

## 6. Pi/live acceptance matrix

### Required after deployment

- Preserve a rollback copy of the application binary and the exact media
  configuration before any bounded mutation.
- Deploy only the cross-built application binary and restart only the
  application unless a listed scenario requires another already-authorized
  Pi operation.
- Exactly one application process owns responsive IPC and reports the deployed
  version; normal local playback, time/progress, shared media state, EQ routing,
  and audio focus remain functional.
- An available saved track restores paused at its saved position. A deliberately
  missing saved-current path falls back paused at zero.
- Reconnecting the saved USB source retries the exact pending track only when
  no user action took ownership; a user seek or transport action prevents that
  later takeover.
- Shuffle and repeat survive an application restart, including when playback is
  paused and no additional playback-state edge occurs.
- Eject during an active cold/rescan releases scanner access before UDisks
  unmount, leaves coherent queue/library state, and does not reopen the target
  through a survivor rescan.
- Restarting the application during an active scan exits and returns without a
  stop timeout, hang, duplicate process, or stale completion.
- Hostapd and Bluetooth retain their PIDs and restart counts. Restore the exact
  original configuration and preserve unrelated Pi checkout state.

### Optional

- Repeat cancellation with a large cold artwork cache.
- Exercise pause/resume and track changes around the pending-restore boundary.
- Repeat eject and survivor-rescan behavior with a second removable volume.

### Not required

- Bluetooth daemon restart, re-pairing, HFP call testing, Android Auto protocol
  capture, companion-app work, unrelated QML inspection, or destructive media
  removal.

Matthew's standing authorization covers application deployment, restart, and
the Pi operations needed by this matrix. It also covers pushing the completed
wave and opening its draft pull request. It does not authorize merging that
pull request. Avoid unrelated service disruption.

## 7. Out of scope

- QML layout or interaction redesign, new library features, new persistence
  fields, or new queue/repeat semantics.
- Playback ring-buffer flushing, decoder replacement, custom FFmpeg I/O, or
  forced worker termination.
- Bluetooth/AVRCP, HFP, Android Auto protocol, video, wireless transport,
  PipeWire routing, or equalizer behavior changes.
- `proto/api/`, External API semantics, JS-shim capabilities, frozen numerics,
  or YAML placement fields.
- UDisks eligibility/polkit redesign, daemon restart, re-pairing, or installer
  and deployment lifecycle changes.
- Logging-configuration or companion-repository changes.

All project rails in `AGENTS.md` and `src/AGENTS.md` remain binding during
implementation. No QML edit is planned; `qml/AGENTS.md` becomes required only
if an approved scope change introduces one.
