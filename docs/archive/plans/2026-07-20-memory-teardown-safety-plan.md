# Memory and Teardown Safety Stabilization — Implementation Plan

Status: COMPLETED 2026-07-21

**Design (read first):**
`docs/archive/plans/2026-07-20-memory-teardown-safety-design.md` — approved by
Matthew 2026-07-20.  
**Grounded against:** `dev` at `ade13b4` plus the approved, uncommitted design.  
**Execution order:** Task 1 → Task 2 → Task 3 → phase gate.  
**Publication:** commit locally per task; do not push until the completed phase
passes its Pi row and review gate and Matthew approves the push.

## Goal

Close three independent lifetime hazards without expanding into adjacent video,
weather, audio, or composition-root work:

1. Old or outliving software-video frames cannot return storage through invalid
   pool state.
2. Weather cache eviction cannot return or asynchronously update a deleted
   `WeatherData`.
3. `ScoNodeMonitor` detaches before `AudioService` destroys its PipeWire loop on
   normal and early application exits.

## Global Constraints

- Read root `AGENTS.md`, `src/AGENTS.md`, and—for Task 1—
  `src/core/aa/AGENTS.md` before editing.
- Build only in `~/builds/openauto-prodigy`; never create an in-repo build
  directory on `/mnt/e`.
- Qt 6.8 system packages only. Do not add dependencies or set
  `CMAKE_PREFIX_PATH`.
- `ctest` does not compile `main.cpp`; Task 3 and the phase gate must build the
  `openauto-prodigy` target explicitly.
- Keep the Pi as HFP Hands-Free and the phone as Audio Gateway. Do not add ofono
  or change PipeWire Telephony behavior.
- For PipeWire code, acquire the PW loop lock before any service mutex. Do not
  block, allocate, or log on the PipeWire RT callback path.
- Do not edit `libs/prodigy-oaa-protocol/proto/` or change External API/proto
  surfaces.
- No exact suite counts in tracked docs or handoffs.
- Commit per implementation task with specific paths; never use `git add -A`.
- Nobody pushes mid-execution.
- If current code differs materially from the grounded symbols below, stop and
  amend this plan rather than improvising across the scope boundary.

## Pre-flight Baseline

Run before Task 1. This establishes whether failures predate the phase:

```bash
cd ~/builds/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

Record commands and pass/fail status in the eventual session handoff. A red
baseline does not authorize unrelated fixes; diagnose whether it blocks this
phase and escalate if it does.

---

## Task 1 — Video frame generation and pool lifetime

**Tier:** main  
**Depends on:** pre-flight baseline  
**Commit:** `fix(video): make recycled frames generation-safe`

### Files

- Modify: `src/core/aa/VideoFramePool.hpp`
- Modify: `src/core/aa/VideoDecoder.cpp`
- Modify: `src/AGENTS.md`
- Modify: `tests/test_video_frame_pool.cpp`

**Out of scope:** codec detection/reset between sessions, hardware DRM frames,
dynamic video renegotiation, new resolutions, touch mapping, performance tuning,
or unrelated decoder cleanup.

### Contract

Replace `RecycledVideoBuffer`'s raw `VideoFramePool*` return dependency with a
lifetime-safe return state:

1. The return state owns the mutex, current generation, current buffer size,
   free queue, and allocation/recycle counters used by the pool.
2. `VideoFramePool` owns the return state strongly. Each outstanding
   `RecycledVideoBuffer` holds it weakly plus the allocation's capacity and
   generation.
3. `reset()` advances the generation, updates the required size, resets the
   counters, and discards the current free queue under the state mutex.
4. Buffer destruction locks the weak state. If the state is gone, the
   allocation is simply freed. If present, return it only when its generation
   equals the current generation and its capacity is at least the current
   required size; otherwise free it.
5. Acquire snapshots format/generation/required size consistently and never
   wraps an allocation smaller than the advertised frame.
6. Same-format releases still recycle in steady state.

In `VideoDecoder::cleanup()`, after the worker has stopped and before resetting
the frame pool, lock `latestFrameMutex_`, clear `latestFrame_`, and clear
`hasLatestFrame_`. Then reset `framePool_`. This makes resource release explicit
even though the weak return state already prevents a UAF.

Do not solve this by storing allocation size alone while retaining the raw pool
pointer; that would leave the destruction hazard open.

### Steps

- [x] Add `testHeldOldFrameDiscardedAfterLargerReset()` to
  `tests/test_video_frame_pool.cpp`:
  - acquire and retain an 800×480 frame;
  - reset to 1920×1080;
  - release the old frame;
  - assert `freeCount() == 0` and `totalRecycled() == 0`;
  - acquire/map a 1920×1080 frame and verify its size, three planes, strides,
    and plane data sizes match YUV420P expectations.
- [x] Build and run the focused target before the fix:

  ```bash
  cd ~/builds/openauto-prodigy
  cmake --build . --target test_video_frame_pool -j$(nproc)
  ctest -R '^test_video_frame_pool$' --output-on-failure
  ```

  Expected red evidence: releasing the old held frame repopulates the free
  queue or increments recycling after reset. Do not proceed to the larger-frame
  copy through the known undersized allocation if the assertion does not stop
  the test first.
- [x] Implement the generation-aware weak return state in
  `VideoFramePool.hpp` and the explicit decoder cleanup in `VideoDecoder.cpp`.
- [x] Correct the `src/AGENTS.md` QVideoFrame rule to distinguish wrappers from
  backing storage: each decoded output gets a fresh ref-counted `QVideoFrame`
  wrapper; backing storage may be recycled only through generation-, size-, and
  lifetime-safe pool state.
- [x] Add `testFrameCanOutlivePool()`:
  - retain a `QVideoFrame` outside a scope containing the pool;
  - destroy the pool;
  - release the frame;
  - verify the test completes without touching freed state.
- [x] Preserve and run the existing allocation/recycle tests to prove
  same-format pooling still works.
- [x] Run focused green verification:

  ```bash
  cd ~/builds/openauto-prodigy
  cmake --build . --target test_video_frame_pool -j$(nproc)
  ctest -R '^test_video_frame_pool$' --output-on-failure
  cmake --build . --target openauto-prodigy -j$(nproc)
  ```

- [x] Inspect the diff for raw return dependencies:

  ```bash
  rg -n 'VideoFramePool\*|returnBuffer' src/core/aa/VideoFramePool.hpp
  ```

  Acceptance: no `RecycledVideoBuffer` member or constructor argument is a raw
  `VideoFramePool*`; every return is generation- and capacity-checked.
- [x] Commit only the four Task 1 files.

### Task 1 acceptance

- The held-frame-across-reset test is red before and green after the fix.
- Old-generation storage never enters the new generation's free queue.
- A frame can outlive the pool safely.
- Same-format recycling remains active.
- Decoder cleanup explicitly releases its latest pooled frame.
- Focused tests and the app target pass.

---

## Task 2 — Weather eviction and asynchronous target lifetime

**Tier:** opus  
**Depends on:** Task 1 committed  
**Commit:** `fix(weather): preserve live cache and reply targets`

### Files

- Modify: `src/core/services/WeatherService.hpp`
- Modify: `src/core/services/WeatherService.cpp`
- Modify: `tests/test_weather_service.cpp`

**Out of scope:** changing weather providers, rounding policy, refresh cadence,
network retry policy, persisted weather data, QML redesign, or an unbounded
general-purpose cache.

### Contract

1. `cleanupStaleEntries()` accepts a protected key and never evicts it.
2. `getWeatherData()` may insert the new entry and then enforce capacity only
   while protecting that new key. It must return an object still present in
   `cache_`.
3. Cleanup skips every actively subscribed key and evicts at most one older,
   unsubscribed candidate per call.
4. If no older candidate is eligible, capacity is a soft limit. The new object
   remains alive and cleanup is retried after a later unsubscribe or subsequent
   cache operation.
5. Weather and reverse-geocoding completion lambdas capture
   `QPointer<WeatherData>`, not raw `WeatherData*`. A null guard deletes the
   reply and exits without invoking the parsing/update handlers.
6. Eviction continues to remove cache and coordinate metadata together.
   Subscriber metadata is removed only for the evicted, unsubscribed key.

Use `QPointer` rather than inventing a parallel liveness flag: `WeatherData` is
a `QObject`, and Qt already invalidates guarded pointers at destruction.

### Steps

- [x] Replace the existing cleanup test's permissive comments/assertions with
  explicit regression slots:
  - `testNewEntrySurvivesCapacityCleanup()` seeds five older unsubscribed
    entries, assigns them valid increasing timestamps via the existing seam,
    requests a sixth, processes deferred deletes, and asserts the returned
    sixth object is non-null, cached, and subscribable while one older key is
    gone.
  - `testCapacityIsSoftWhenAllEntriesSubscribed()` subscribes five entries,
    requests and subscribes a sixth, and asserts all six remain alive. After
    unsubscribing the sixth, trigger cleanup and assert the cache returns to its
    nominal bound without removing the five subscribers.
- [x] Build and run the focused target before the fix:

  ```bash
  cd ~/builds/openauto-prodigy
  cmake --build . --target test_weather_service -j$(nproc)
  ctest -R '^test_weather_service$' --output-on-failure
  ```

  Expected red evidence: the newly requested invalid-timestamp entry is selected
  for deletion or the all-subscribed case cannot preserve the new object.
- [x] Implement protected-key cleanup, the soft-cap retry on unsubscribe, and
  `QPointer` guards for both reply paths.
- [x] Add the minimum read-only cache-membership seam needed by the regression
  tests:

  ```cpp
  bool containsCachedKeyForTest(const QString& key) const;
  ```

  Do not expose or mutate the full cache.
- [x] Change both internal completion handlers to accept a guarded
  `QPointer<WeatherData>`. Add these narrow public wrappers beside the service's
  existing `// For testing` seams; each delegates directly to the corresponding
  production handler:

  ```cpp
  void finishWeatherReplyForTest(
      QNetworkReply* reply, const QPointer<WeatherData>& target);
  void finishGeocodingReplyForTest(
      QNetworkReply* reply, const QPointer<WeatherData>& target);
  ```
- [x] In `tests/test_weather_service.cpp`, add a minimal in-memory
  `QNetworkReply` fake implementing only `abort()` and `readData()`, and two
  tests that:
  - delete a guarded weather target;
  - pass the fake reply plus null `QPointer` through the real weather and
    geocoding completion handlers via the wrappers;
  - process deferred deletion and assert the reply is cleaned up without a
    target dereference.
  Do not add live-internet dependencies or a production network abstraction.
- [x] Run focused green verification:

  ```bash
  cd ~/builds/openauto-prodigy
  cmake --build . --target test_weather_service -j$(nproc)
  ctest -R '^test_weather_service$' --output-on-failure
  ```

- [x] Verify both asynchronous sites use guarded targets:

  ```bash
  rg -n 'QPointer<WeatherData>|onWeatherReplyFinished|onGeocodingReplyFinished' \
    src/core/services/WeatherService.cpp
  ```

- [x] Commit only the three Task 2 files.

### Task 2 acceptance

- The sixth returned object cannot be the cleanup victim of its own creation.
- Subscribed entries remain protected; capacity may temporarily exceed five
  when all candidates are protected.
- Unsubscribe provides a deterministic path back toward the nominal limit.
- Both asynchronous reply paths use Qt-guarded target lifetime.
- Existing refresh/subscriber interval tests and the focused target pass.

---

## Task 3 — SCO monitor detaches before PipeWire teardown

**Tier:** main  
**Depends on:** Task 2 committed  
**Commit:** `fix(audio): stop SCO monitoring before PipeWire teardown`

### Files

- Modify: `src/core/services/AudioService.hpp`
- Modify: `src/core/services/AudioService.cpp`
- Modify: `src/core/audio/ScoNodeMonitor.hpp`
- Modify: `src/core/audio/ScoNodeMonitor.cpp`
- Modify: `src/main.cpp`
- Modify: `src/AGENTS.md`
- Modify: `tests/test_sco_node_monitor.cpp`

**Out of scope:** changing the `headset-audio-gateway` match, HFP call-state
semantics, audio stream teardown, PipeWire Telephony, SCO routing, codec policy,
ring-buffer behavior, or broader application-child ownership.

### Contract

Use an explicit pre-teardown signal from the resource owner:

1. Add the concrete `AudioService` signal `aboutToDestroyPipeWire()`; do not add
   it to `IAudioService` or change plugin ABI.
2. Emit it synchronously at the beginning of `AudioService::~AudioService()`,
   before acquiring/destroying any PipeWire resource.
3. In `main.cpp`, connect that signal to `ScoNodeMonitor::stop()` with
   `Qt::DirectConnection` immediately after constructing the monitor and before
   calling `start()`.
4. This owner-driven signal covers normal `app.exec()` return, startup error
   returns after monitor creation, and either application-child deletion order:
   - if AudioService dies first, it stops the live monitor before freeing PW;
   - if the monitor dies first, its destructor stops while AudioService is live
     and Qt automatically removes the later signal connection.
5. Make `ScoNodeMonitor::stop()` normalize every partial state, including
   `threadLoop_ != nullptr` with `registry_ == nullptr`, and remain idempotent.
6. Add an atomic active/epoch guard so a queued `recomputeRunning()` delivery
   captured before stop cannot emit stale SCO state afterward.
7. Listener/proxy destruction remains under the valid PW loop lock. Preserve PW
   lock-before-other-lock ordering.

Do not use an `aboutToQuit`-only connection or rely on QObject child order; both
leave an early-return or ordering hole.

### Steps

- [x] Extend `tests/test_sco_node_monitor.cpp` with
  `testAudioOwnerStopsMonitorBeforeTeardown()`:
  - allocate `AudioService` while a `ScoNodeMonitor` remains alive;
  - connect `aboutToDestroyPipeWire` to `stop()` with `Qt::DirectConnection`;
  - if the service is available, start the monitor with its real loop/core;
  - delete the AudioService;
  - call `monitor.stop()` again and assert it is safe and
    `scoRunning() == false`.
  This runs meaningfully with or without a local PipeWire daemon.
- [x] Add a signal-order assertion in the same test: a direct observer of
  `aboutToDestroyPipeWire` must run before the `AudioService` destructor
  completes. Keep the observer external to the deleted service.
- [x] Build and run the focused target before the fix:

  ```bash
  cd ~/builds/openauto-prodigy
  cmake --build . --target test_sco_node_monitor -j$(nproc)
  ```

  Expected red evidence: compilation fails because the owner signal does not
  exist. That is the red state for this lifetime API.
- [x] Add the AudioService signal/emission, main composition-root connection,
  normalized stop path, and queued-delivery active/epoch guard.
- [x] Add the PipeWire ownership rule to `src/AGENTS.md`: any auxiliary object
  retaining raw `AudioService` PW loop/core/proxy handles must connect a direct
  pre-teardown stop edge before it starts; QObject child order and
  `aboutToQuit` are not sufficient ownership mechanisms.
- [x] Run focused green verification and build the real app target:

  ```bash
  cd ~/builds/openauto-prodigy
  cmake --build . --target test_sco_node_monitor -j$(nproc)
  ctest -R '^test_sco_node_monitor$' --output-on-failure
  cmake --build . --target openauto-prodigy -j$(nproc)
  ```

- [x] Inspect the ownership edge and lock placement:

  ```bash
  rg -n 'aboutToDestroyPipeWire|ScoNodeMonitor::stop|Qt::DirectConnection' \
    src/core/services/AudioService.* src/core/audio/ScoNodeMonitor.* src/main.cpp
  ```

  Acceptance: the signal emission precedes the first PW teardown/lock in the
  AudioService destructor; main connects before monitor start; stop removes PW
  objects only while the loop is valid.
- [x] Commit only the seven Task 3 files.

### Task 3 local acceptance

- The new test is red at compile time before the signal exists and green after.
- Owner teardown synchronously stops the monitor.
- Null, partial, repeated, and destructor-driven stop paths are safe.
- Queued state delivery cannot resurrect `scoRunning` after stop.
- The focused test and real application target pass.

---

## Phase-Level Local Gate

Run after all three task commits:

```bash
cd ~/builds/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

Then cross-build from the repository root:

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
./cross-build.sh
```

If any failure is caused by this phase, fix it in the owning task's scope and
record the deviation. Do not make unrelated tests green opportunistically.

## Pi Checkpoint — Required Before Completion

Deploy the cross-built binary using the repository-standard path:

```bash
rsync -av build-pi/src/openauto-prodigy \
  matt@192.168.1.149:~/openauto-prodigy/build/src/
ssh matt@192.168.1.149 'sudo systemctl restart openauto-prodigy.service'
```

Capture a baseline:

```bash
ssh matt@192.168.1.149 \
  'systemctl show openauto-prodigy.service -p ActiveState -p SubState -p MainPID -p NRestarts'
```

### Row S1 — repeated clean shutdown

- Perform at least five clean `systemctl restart openauto-prodigy.service`
  cycles.
- After each, require `ActiveState=active`, `SubState=running`, a new healthy
  `MainPID`, and no unexpected `NRestarts` increase.
- Inspect the journal since deployment for segmentation faults, watchdog kills,
  invalid PipeWire proxy/loop access, or shutdown hangs:

  ```bash
  ssh matt@192.168.1.149 \
    'journalctl -u openauto-prodigy.service --since "10 minutes ago" --no-pager'
  ```

### Row S2 — live SCO teardown and recovery

- Matthew establishes one HFP call and confirms a real SCO node is being
  tracked/running.
- Cleanly restart the service while the runbook's chosen call state is active or
  immediately after ending the call; record which was used.
- Require a clean old-process exit and healthy replacement process.
- Place a subsequent call and verify SCO state can transition normally again.

Record both rows as explicit PASS/FAIL in `docs/session-handoffs.md`. A local
green build with either row unrun is `bench-pending`, not phase completion.

## Review Gate and Adjudication

After the Pi rows pass, determine the base ref immediately before the design and
task commits, then run:

```bash
bash scripts/codex-review.sh <base-ref>
```

- Adjudicate every P1/P2/P3 finding.
- Confirmed small findings may be fixed inline; substantial findings return to a
  bounded task and trigger one review re-run.
- Dismissed findings require a recorded reason.
- Exit 2/4 degrades to a main-session-only review and must be noted in the
  handoff; it never silently passes.

## Closeout — Main Session, Not a Fourth Implementation Task

After all gates pass:

1. Update behavior docs only where the implemented contracts changed.
2. Update `docs/roadmap-current.md` with the actual completion/bench state.
3. Update `docs/INDEX.md` as the design and plan move to archive.
4. Append `docs/session-handoffs.md` with changes, rationale, status, next
   steps, verification commands/results, and review adjudication.
5. Mark this plan and its design `COMPLETED <date>` and move both to
   `docs/archive/plans/` in the same commit.
6. Update the local-only remediation ledger from `reconfirmed_static` through
   the evidence states reached, including task commit hashes and review/bench
   results. Tracked closeout files must not link back to that ledger.
7. Stage only the named closeout files and commit. Do not push without Matthew's
   explicit go-ahead.

## Definition of Done

- Task 1, Task 2, and Task 3 each have one atomic local commit and synthesized
  verification results.
- Every task meets its acceptance criteria without adjacent scope growth.
- Full local build, explicit app-target build, and `ctest` pass.
- `./cross-build.sh` passes.
- Pi rows S1 and S2 pass.
- The review gate is fully adjudicated.
- Roadmap, index, handoff, design, plan, and private ledger reflect the actual
  result.
- Nothing is pushed until Matthew approves.
