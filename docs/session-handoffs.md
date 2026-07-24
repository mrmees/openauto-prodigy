# Session Handoffs

Newest entries first.

---

## 2026-07-24 — PR #36 pre-merge review

**What changed:** nothing — the review confirmed the branch as-is.

**Review adjudication:** zero findings. Verified in depth: scanner stop
correctness (generation plus worker-pointer identity reject the stale queued
completion without dereferencing the joined worker; destructor path is
emission-free; FFmpeg interrupt_callback bounds the join even mid-read on a
dying device; avformat_open_input's documented free-and-null-on-failure makes
the RAII close safe); restore-ownership completeness (the loosened
retryPendingRestore gate cannot clobber active playback because every
user-initiated playback path — transport keys, seek, and all queue
selections — clears the pending restore, and the fallback restore is always
paused); eject ordering (duplicate guard, scanner quiesced before the UDisks
request, successor scan excludes canonical aliases of the target); and the
QSKIP-to-hard-failure conversion in playback fixture tests, which is correct
because the WSL dev environment is contractually the Pi target environment.
Observation without action: purgeVolume now quiesces the scanner on yank
paths too, costing a survivor-rescan restart in exchange for guaranteed
file-handle release.

**Verification:** full local build, explicit `openauto-prodigy` target, and
`ctest --output-on-failure` (all tests) passed on the branch.

**Next 1-3 steps:** (1) merge PR #36; (2) at the next milestone tag, deploy
the accumulated PR #32-#36 fixes together.

---

## 2026-07-24 — Media lifecycle and persistence remediation COMPLETE

**What changed:** exact saved tracks now exclusively own their saved position;
an available fallback starts at zero while the raw exact restore remains
pending. Explicit transport and seek actions durably take ownership, and
shuffle, repeat, and clamped seek values persist at their user boundary.
`MediaScanner` now has explicit generation-safe quiescence, cooperative
traversal/cache/tag/artwork cancellation, and an FFmpeg interrupt boundary.
Safe eject excludes canonical target aliases, suppresses duplicate requests,
waits for scanner ownership to end, and rebuilds only surviving roots;
shutdown observes the same ownership order. Repository-owned valid fixtures
fail on backend errors, and scanner completion failures cannot access absent or
expired state. The bounded implementation is recorded in commits `65487de`
through `23e4385` after the activating documentation commit `f20ecd8`.

**Why:** restore state, user intent, scanner workers, removable-media lifetime,
and test failure paths previously had overlapping or implicit ownership. The
new boundaries keep fallback state honest, prevent late restore takeover after
user action, release files before eject/shutdown, and ensure test failures stay
visible without changing QML, media consumer units, playback arbitration,
Bluetooth/HFP, Android Auto protocol, frozen API fields, or installer behavior.

**Status:** COMPLETE on
`agent/media-lifecycle-persistence-remediation`, based on merged PR #35. The
reviewed aarch64 binary through `23e4385` is retained behind rollback snapshot
`/var/backups/openauto-prodigy/20260724T115246Z` and deployed as
`ALPHA-26-07-15-02-163-g23e4385`. One application process owns responsive IPC
with zero service restarts. The exact original media configuration was restored
byte-for-byte; the original cache directories were restored; temporary screen
captures were removed; and the Pi checkout's unrelated dirty state was not
pulled, reset, cleaned, or overwritten.

**Review gate:** the initial pass returned one P1 and three P2 findings; all
four actionable issues were confirmed and fixed. The required rerun returned
six P2 findings, all confirmed and fixed. The only dismissed residual was the
precisely bounded possibility that one indivisible kernel filesystem syscall
finishes before the next cancellation checkpoint; forced thread termination
and custom FFmpeg I/O are outside the approved contract. No finding was
silently dropped, and no third pass was run under the one-rerun policy.

**Verification:** focused media-player plugin, playback-policy, scanner,
USB-policy, tag-reader, and playback-engine tests passed. The full local build,
explicit `openauto-prodigy` target, `ctest --output-on-failure`, documentation
link/private-reference checks, and `git diff --check origin/main..HEAD` passed.
`./cross-build.sh` produced the deployed aarch64 binary. On the Pi, an exact
saved track restored paused at its saved time, a missing-current fallback
started at zero, the late USB source reclaimed the exact pending track, and a
user next/pause takeover remained durable. Shuffle and repeat persisted
immediately and survived restart. Restart during a cold multi-root scan
returned one coherent successor process; safe eject during a cold rescan
removed and powered off only the target, preserved the surviving library, and
remounted cleanly after physical reinsertion. Local playback created the
expected EQ-routed PipeWire stream and updated shared now-playing state.
Wireless H.265 projection reconnected after deployment before the native-media
matrix; the Pixel was then deliberately disconnected for that matrix and was
offline at final reconnect, while the Moto remained connected. Hostapd PID
`46989` and Bluetooth PID `672` remained unchanged with zero restarts.

**Next 1-3 steps:** (1) publish this completed branch as a draft PR targeting
`main`; (2) review and merge it as an independent consolidated wave; (3)
revalidate the final installer/deployment lifecycle batch before activating
its bounded design and plan.

---

## 2026-07-24 — PR #35 pre-merge review

**What changed:** nothing — the review confirmed the branch as-is.

**Review adjudication:** zero findings. Verified in depth: the navbar/popup
generation plumbing (every begin synchronously unregisters old zones, evdev
callbacks capture their generation and the marshaled slot rejects mismatches,
so retired geometry cannot fire actions); the deferred plugin-view retirement
ordering (plugin hook deactivates synchronously, the child QQmlContext
outlives the dying view, `~PluginModel` drains retirements while its child
view host is still alive, and drained IDs make leftover queued callbacks
no-ops); widget-grid invariants (every mutator applies the pending remap
first, baselines commit only on user mutations with all dashboards forced to
unified grid dimensions before persisting); and the per-zone/per-slot popup
press state replacing a lambda-static shared across all zones and fingers.
Observations without action: one event-loop turn of old/new plugin view
overlap on switch (new renders on top; retained context keeps the dying
view's handlers valid by design).

**Verification:** full local build, explicit `openauto-prodigy` target, and
`ctest --output-on-failure` (all tests) passed on the branch. Physical
multi-touch, popup switching, and exact navbar-boundary touches remain
unvalidated on hardware — worth a deliberate poke on the next live drive.

**Next 1-3 steps:** (1) merge PR #35; (2) at the next milestone tag, deploy
PR #32-#35 together and physically exercise navbar edges and popup switching.

---

## 2026-07-24 — UI input and widget-grid lifecycle remediation COMPLETE

**What changed:** navbar evdev claims now come from rendered QML geometry, with
generation-safe replacement, accepted touch-slot ownership, and popup state
isolated by generation, zone, and slot. Widget remaps keep retained placements
and reserved pages reachable, apply pending dimension changes before edits,
and persist page and dimension baselines across restarts. Plugin deactivation
now precedes replacement activation while the outgoing QML view and child
context survive the current dispatch and retire in order afterward. A dedicated
owner replaces window/current-screen DPI connections. The bounded implementation
and review fixes are recorded in commits `6f28137` through `5feaa5a`.

**Why:** fixed geometry, shared or stale gesture state, unreachable widget
placements, stale remap baselines, synchronous QML destruction, late plugin
deactivation, and accumulating screen connections could make shell input,
dashboard state, or plugin transitions disagree with their rendered and
persisted owners. The remediation establishes one explicit owner at each
boundary without changing Android Auto protocol/input mapping, frozen API or
YAML fields, audio, HFP, or Bluetooth behavior.

**Status:** COMPLETE on `agent/ui-input-widget-grid-lifecycle-remediation`,
based on merged PR #34. The reviewed aarch64 binary through `5feaa5a` was
retained behind rollback snapshot
`/var/backups/openauto-prodigy/20260724T032627Z` and deployed as
`ALPHA-26-07-15-02-154-g5feaa5a`. One application process (PID `332025`) owns
responsive IPC. A native dashboard screenshot and live dashboard transition
were verified through the trusted localhost API before Android Auto re-entered
wireless H.265 projection. The exact original configuration was restored
(`57415b...`); hostapd PID `46989` and Bluetooth PID `672` remained
unchanged with zero restarts. Physical multi-touch, popup switching, and navbar
boundary rows were unavailable remotely and were not inferred.

**Review gate:** the initial repository pass returned three findings and the
required rerun returned three more. All six were confirmed and fixed; none were
dismissed or silently dropped. The fixes cover stale navbar geometry events and
generation rollback, widget baseline persistence, and ordered plugin view,
deactivation, and context retirement. No third run was performed under the
one-rerun policy.

**Verification:** focused navbar, widget-grid/remap, plugin-model/view-host, and
display tests passed. `cmake --build . -j$(nproc)`, the explicit
`openauto-prodigy` target, and the full `ctest --output-on-failure` passed in
`~/builds/openauto-prodigy`. Documentation-link validation and
`git diff --check origin/main..HEAD` passed. `./cross-build.sh` produced the
deployed aarch64 binary. Live validation covered shell/dashboard rendering and
transition, one responsive process and IPC owner, wireless H.265 re-entry,
exact configuration restoration, and unchanged hostapd/Bluetooth lifetimes.

**Next 1-3 steps:** (1) publish this completed branch as a draft PR targeting
`main`; (2) review and merge it as an independent consolidated wave; (3)
revalidate the next audit wave before activating another bounded design and
plan.

---
