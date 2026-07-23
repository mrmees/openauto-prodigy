# Audio and Equalizer Real-Time Safety Remediation — Implementation Plan

Status: COMPLETED 2026-07-23
Date: 2026-07-23
Design: `docs/archive/plans/2026-07-23-audio-eq-rt-safety-remediation-design.md`
Base: `origin/main` at `1240f423a91c9a355f56e0fc9b33534157a37f8b`
Branch: `agent/audio-eq-rt-safety-remediation`

## Global constraints

- Execute one bounded task and commit at a time. Nobody pushes mid-execution.
- Preserve wireless-only AA, HFP HF role, no-ofono, frozen numerics/API, and
  the protocol-submodule boundary.
- Keep all PipeWire process paths allocation-free, lock-free, and logging-free.
- Do not edit QML, protocol files, routing rules, or logging configuration.
- Behavior documentation changes ship with the behavior they describe.
- Use private audit artifacts only for ledger state; never expose identifiers or
  evidence in tracked files.
- A task is complete only after its focused tests are green.

## Task 1 — Audio/PipeWire ownership and bounded ingestion

Tier: main

Files:

- Modify `src/core/audio/AudioRingBuffer.hpp`
- Modify `src/core/services/AudioService.hpp`
- Modify `src/core/services/AudioService.cpp`
- Modify `src/core/YamlConfig.hpp`
- Modify `src/core/YamlConfig.cpp`
- Modify `tests/test_audio_ring_buffer.cpp`
- Modify `tests/test_audio_service.cpp`
- Modify `tests/test_yaml_config.cpp`
- Modify `tests/test_config_key_coverage.cpp`
- Modify `docs/reference/config-schema.md`

Steps:

1. Add failing ring tests that run a real concurrent producer/consumer,
   validate the accepted byte stream, require `available() <= capacity()`, and
   pin short-write/drop-newest behavior.
2. Add failing AudioService tests through a private friend seam for malformed
   `pw_buffer` structures, valid period fill, safe capacity calculation, and
   unconditional queued stream-error diagnostics.
3. Add failing config tests for the effective 500 ms default and removal of the
   unused adaptive key/accessor.
4. Make the producer the sole write-index owner and the consumer the sole
   read-index owner. Update drain comments/contracts.
5. Factor and validate the playback-buffer body; ensure the callback requeues
   every dequeued buffer.
6. Replace adaptive-growth machinery with non-RT diagnostic consumption. RT
   code publishes only primitive atomics.
7. Clamp buffer targets to 500–5000 ms, validate sample rate, use bounded
   64-bit/power-of-two sizing, and convert allocation failure to clean stream
   failure.
8. Queue error logging to the application thread independently from existing
   recovery-hook dispatch.
9. Update config defaults/schema to describe static bounded buffering and remove
   the unused adaptive option.

Acceptance criteria:

- The concurrent ring test validates every byte returned as written and no
  consumed byte is replayed.
- Overflow never mutates the read index and returns fewer bytes than requested.
- Invalid PipeWire data/chunk/container pointers are not dereferenced.
- Valid buffers are filled/silenced exactly to the bounded requested period.
- Buffer sizing is total for negative, normal, and extreme inputs; no overflow,
  infinite loop, or unbounded allocation is possible.
- No string conversion, `fprintf`, Qt logging, allocation, or mutex remains in
  `onPlaybackProcess` or its buffer-filling body.
- A stream error without a recovery hook still produces a warning on the Qt
  thread; a hook still fires once when present.
- Static buffer documentation and defaults agree at 500 ms.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_audio_ring_buffer test_audio_service test_yaml_config test_config_key_coverage -j$(nproc)
ctest --output-on-failure -R 'test_(audio_ring_buffer|audio_service|yaml_config|config_key_coverage)$'
ctest --output-on-failure --repeat until-fail:50 -R 'test_audio_ring_buffer$'
```

Out of scope: focus policy, routing, live buffer resizing, caller-specific
compensating queues, QML, and equalizer coefficient publication.

Commit: `fix(audio): make PipeWire ingestion bounded and RT-safe`

## Task 2 — Equalizer service input and restore boundaries

Tier: opus

Files:

- Modify `src/core/services/IEqualizerService.hpp`
- Modify `src/core/services/EqualizerService.hpp`
- Modify `src/core/services/EqualizerService.cpp`
- Modify `tests/test_equalizer_service.cpp`

Steps:

1. Add failing invalid-`StreamId` tests for every public mutation/getter and
   engine acquisition path.
2. Add failing rename tests for empty, whitespace-only, bundled, duplicate,
   exact-no-op, and valid targets.
3. Add a config-aware test proving construction/restoration produces no timed
   flush without a user mutation, while a later mutation still flushes.
4. Guard every enum-facing entry before array access; retain the guarded int
   helpers and frozen enum values.
5. Enforce the preset-name rules without changing case sensitivity or existing
   save/overwrite behavior.
6. Add an explicit restore guard so default seeding and config application are
   save-free.
7. Update interface comments to document invalid-input and rename behavior.

Acceptance criteria:

- Invalid stream values cannot read/write outside the three stream states and
  return documented neutral results.
- A valid stream follows existing signals, fan-out, gains, bypass, and preset
  behavior unchanged.
- Empty/whitespace, bundled, and duplicate rename targets are rejected without
  changing the library or active preset references.
- Exact-name rename is a successful no-op; valid rename updates active streams.
- Construction/config load does not call the flush hook after the debounce
  interval; a subsequent user change does.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_equalizer_service -j$(nproc)
ctest --output-on-failure -R 'test_equalizer_service$'
```

Out of scope: new preset UI/API, case-folded names, malformed-YAML retention after
a later explicit user save, and engine coefficient transport.

Commit: `fix(eq): validate service inputs and keep restore save-free`

## Task 3 — Atomic equalizer coefficient publication

Tier: main

Files:

- Modify `src/core/audio/EqualizerEngine.hpp`
- Modify `src/core/audio/EqualizerEngine.cpp`
- Modify `tests/test_equalizer_engine.cpp`

Steps:

1. Add a concurrent writer/processor stress test with gain, all-gain, and bypass
   updates; require finite in-range samples and successful completion.
2. Serialize non-RT control mutations and getters.
3. Replace recyclable object buffers with lock-free atomic primitive fields and
   an even/odd publication generation.
4. Make the RT reader take one bounded snapshot attempt and defer when a write
   overlaps it.
5. Preserve interpolation, bypass crossfade, limiter, channel state, and current
   public gain behavior.
6. Compile-time assert that published primitive atomics are always lock-free on
   supported targets.

Acceptance criteria:

- No C++ data race exists between coefficient publication and `process()`.
- `process()` performs no lock, allocation, logging, retry loop, or deletion.
- A torn snapshot is never installed; an overlapping update is picked up by a
  later callback.
- Concurrent stress and all existing response/interpolation/bypass tests pass.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_equalizer_engine -j$(nproc)
ctest --output-on-failure -R 'test_equalizer_engine$'
ctest --output-on-failure --repeat until-fail:100 -R 'test_equalizer_engine$'
```

Out of scope: filter topology, band count/frequencies, limiter tuning, sample
formats, or per-consumer ownership changes.

Commit: `fix(eq): publish coefficients with an RT-safe atomic snapshot`

## Task 4 — Integration, review, deployment, and closure

Tier: sonnet

Files:

- Modify `docs/roadmap-current.md`
- Append exactly one entry to `docs/session-handoffs.md`
- Update the private remediation ledger overlay (ignored, never tracked)
- Move this design and plan to `docs/archive/plans/` with
  `Status: COMPLETED 2026-07-23`

Steps:

1. Run focused tests and repeated concurrency stress from Tasks 1–3.
2. Run the local build, explicit application target, full suite, and whitespace
   check.
3. Run `bash scripts/codex-review.sh origin/main`; adjudicate every P1/P2/P3.
   Fix confirmed findings and re-run the gate once if a substantial fix lands.
4. Cross-build with `./cross-build.sh`.
5. Snapshot the Pi binary/config, deploy only the application binary, and
   restart only `openauto-prodigy.service`.
6. Execute the required live matrix from the design and record unavailable
   phone-dependent rows honestly.
7. Close the private batch, recompute authoritative counts, update the roadmap,
   append one handoff, mark/archive the design and plan, and commit closure.
8. Push the completed branch and open a draft PR under Matthew's standing
   approval. Nobody pushes earlier.

Acceptance criteria:

- `cmake --build . -j$(nproc)` passes.
- `cmake --build . --target openauto-prodigy -j$(nproc)` passes.
- `ctest --output-on-failure` passes.
- `git diff --check origin/main..HEAD` passes.
- Every review finding is recorded as fixed or dismissed with reason.
- The aarch64 cross-build succeeds.
- Pi has one responsive application process; configuration/unrelated checkout
  state is preserved; Bluetooth/hostapd PIDs and restart counts are unchanged;
  available connected-phone playback is normal.
- The private ledger is valid and recomputed; no private identifier/evidence is
  present in tracked files.
- The draft PR contains the complete bounded tranche.

Out of scope: milestone tag/release, merging the PR, service/daemon restart
beyond the application, re-pairing, and unrelated Pi cleanup.

Commit: `docs: close audio and equalizer RT safety remediation`
