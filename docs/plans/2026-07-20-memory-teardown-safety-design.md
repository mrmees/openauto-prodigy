# Memory and Teardown Safety Stabilization — Design

Status: ACTIVE — approved 2026-07-20; implementation plan drafted

**Date:** 2026-07-20  
**Grounded against:** `dev` at `ade13b4`  
**Priority:** first remediation tranche before broader reliability work

## 1. Outcome

Remove three independently reachable lifetime hazards without broad subsystem
rewrites:

1. Software-decoded video frames remain safe across format changes and decoder
   destruction.
2. Weather cache eviction never returns or asynchronously updates a deleted
   `WeatherData` object.
3. `ScoNodeMonitor` always detaches from PipeWire before `AudioService` destroys
   the PipeWire loop and proxies it uses.

The work is one phase with three atomic implementation tasks in this fixed
order:

1. Video frame lifetime
2. Weather object lifetime
3. SCO/PipeWire teardown

Each task gets its own focused verification and commit. The phase receives one
full build/test/cross-build/review gate after all three tasks are locally green.

## 2. Decisions Locked

- **One phase, three atomic tasks.** The tasks share a safety outcome but do not
  share implementation commits.
- **Root-cause adjacency only.** A lower-severity issue enters this phase only
  when the same ownership contract and verifier close it.
- **Bench-gated completion.** Video and Weather may become locally green, but
  this phase remains `ACTIVE` until the SCO teardown row passes on the Pi.
- **No speculative cleanup.** Opening a file does not promote unrelated findings
  in that subsystem.
- **Evidence before closure.** Static reasoning is sufficient to promote a task,
  not to close it. Each task needs a regression test or deterministic verifier
  before its fix is accepted.
- **Local-first publication.** The design, implementation plan, and task commits
  remain local during execution and are pushed only as a completed, reviewed
  series.

## 3. Scope Boundary

### In scope

- `src/core/aa/VideoFramePool.hpp`
- `src/core/aa/VideoDecoder.hpp`
- `src/core/aa/VideoDecoder.cpp` if explicit cleanup is required
- `tests/test_video_frame_pool.cpp`
- A focused decoder-lifetime test source only if the frame-pool test cannot
  exercise the production ownership edge without duplicating product logic
- `src/core/services/WeatherService.hpp`
- `src/core/services/WeatherService.cpp`
- `tests/test_weather_service.cpp`
- `src/core/audio/ScoNodeMonitor.hpp`
- `src/core/audio/ScoNodeMonitor.cpp`
- `src/core/services/AudioService.hpp` / `.cpp` only if the selected lifetime
  contract needs an explicit pre-PipeWire-teardown notification
- `src/main.cpp`
- `tests/test_sco_node_monitor.cpp`
- `tests/CMakeLists.txt` only if a new focused test target is necessary
- Documentation that describes an ownership or shutdown contract changed by
  the implementation

### Explicitly out of scope

- Dynamic AA video renegotiation or new resolution capabilities
- Codec auto-detection across sessions
- Other AA session, touch, or socket findings
- General weather-cache redesign, persistence, or new weather features
- Other audio RT, ring-buffer, EQ, stream-error, or HFP behavior findings
- Installer, systemd-unit, night-mode, logging/config, and AVRCP repairs
- A general composition-root teardown rewrite
- Completing or expanding the wider reliability review

## 4. Task 1 — Video Frame Lifetime

### Current ownership failure

`VideoFramePool::reset()` discards free buffers and changes `bufferSize_`, but a
`RecycledVideoBuffer` already held by `latestFrame_` or the Qt render thread can
return its old allocation afterward. The return path carries neither allocation
size nor pool generation, so the old allocation enters the new free list and can
be wrapped with the new, larger format.

There is a second edge in the same ownership contract:
`RecycledVideoBuffer` retains a raw `VideoFramePool*`. A frame that outlives the
pool calls through that pointer during destruction. `VideoDecoder` currently
declares `latestFrame_` before `framePool_`, so implicit member destruction alone
does not provide a safe release order.

The existing format-change test releases its frame before reset and therefore
does not exercise either edge.

### Required design properties

The implementation may choose the concrete representation, but it must satisfy
all of these properties:

1. A returned allocation carries enough identity to prove it belongs to the
   pool's current format generation and is large enough for the current frame.
2. An old-generation or undersized allocation is discarded, never placed on the
   current free list.
3. A recycled video buffer has no raw lifetime dependency on a destroyed
   `VideoFramePool`.
4. Decoder cleanup explicitly releases `latestFrame_` safely; correctness must
   not depend only on declaration order.
5. Buffer return remains safe from the Qt render thread while acquire/reset run
   on the decode worker.
6. Steady-state same-format recycling remains intact; the fix must not silently
   turn the pool into per-frame allocation.

A generation-aware return state referenced weakly by outstanding buffers is the
preferred shape because it closes both reset and pool-destruction hazards. The
implementation plan may choose an equivalent mechanism if its tests prove all
six properties.

### Acceptance

- Hold a smaller frame across `reset(largerFormat)`, release it after reset, then
  acquire/map a larger frame: the old allocation is not recycled and the mapped
  planes cover the larger format safely.
- Hold a frame beyond destruction of the `VideoFramePool`, then release it: no
  call targets freed pool state and no crash occurs.
- Decoder cleanup with an outstanding `latestFrame_` releases the frame before
  invalidating its return state.
- Same-format acquire/release still increases the recycled count without a new
  allocation.
- Focused command: `ctest -R '^test_video_frame_pool$' --output-on-failure`.
- If a decoder-specific target is introduced, the implementation plan must name
  it and give its exact focused command.

## 5. Task 2 — Weather Object Lifetime

### Current ownership failure

`getWeatherData()` inserts and starts two network requests for a new
`WeatherData`, then enforces the cache limit before the QML caller can subscribe.
The new entry has an invalid `lastUpdated()`, which makes it eligible to win the
oldest-entry selection. `cleanupStaleEntries()` can therefore remove and
`deleteLater()` the same object that `getWeatherData()` returns.

Both network completion lambdas capture a raw `WeatherData*`, so eviction also
leaves asynchronous callbacks able to update freed storage.

### Required design properties

1. The object returned by `getWeatherData()` cannot be evicted during that call.
2. Capacity enforcement prefers an older, unsubscribed entry; subscribed entries
   are never evicted.
3. If every existing entry is protected, the cache limit is a soft limit: retain
   the new entry and retry cleanup after an entry becomes eligible rather than
   returning a doomed object.
4. Network callbacks do not dereference a deleted `WeatherData`; use guarded Qt
   lifetime tracking or an equivalent cache-key lookup.
5. Eviction removes `cache_`, `coordsByKey_`, and subscriber metadata
   consistently.
6. The QML `getWeatherData()` followed by `subscribe()` contract remains intact.

### Acceptance

- Seed five older, unsubscribed entries with valid timestamps, request a sixth,
  and verify that the returned sixth object remains alive and cached while an
  older entry is evicted.
- With five subscribed entries, request and subscribe to a sixth: no subscribed
  entry or newly returned object is deleted, even if the cache temporarily
  exceeds its nominal limit.
- Complete or simulate a weather/geocoding reply after its target has been
  evicted: the callback exits safely without dereferencing the deleted object.
- Existing refresh interval and subscribed-entry preservation behavior stays
  green.
- Focused command: `ctest -R '^test_weather_service$' --output-on-failure`.

## 6. Task 3 — SCO/PipeWire Teardown

### Current ownership failure

`AudioService` and `ScoNodeMonitor` are both children of the application.
`ScoNodeMonitor` retains raw PipeWire loop, registry, and node pointers obtained
from `AudioService`, but normal shutdown does not call `scoMonitor->stop()` before
application-child destruction. Its destructor can therefore lock a freed
`pw_thread_loop` and destroy already-freed proxies.

This is not only an `app.exec()` exit problem. The monitor is created before QML
loading completes, so startup error returns after monitor creation must also
honor the same dependency order. An `aboutToQuit` connection alone is therefore
insufficient.

### Required design properties

1. There is an explicit, mechanically enforced lifetime edge: monitor stop
   finishes before `AudioService` begins destroying its PipeWire resources.
2. The edge covers normal event-loop exit, early returns after monitor creation,
   and application-child destruction.
3. `stop()` remains idempotent and safe when PipeWire was unavailable or start
   was partial.
4. Registry/node listeners and proxies are removed while the thread loop is
   valid and under the required loop lock.
5. Queued Qt delivery cannot emit from or dereference a destroyed monitor after
   stop.
6. The fix does not change the Pi/phone HFP roles, PipeWire Telephony path, SCO
   detection predicate, or call-state semantics.

The implementation mechanism is deliberately left open for the implementation
plan: an AudioService pre-teardown notification, a composition-root scope guard,
or another explicit owner may satisfy the contract. Destructor ordering by
accident and an `aboutToQuit`-only hook do not.

### Local acceptance

- Null-loop/null-core start remains a guarded no-op.
- Stop before start, repeated stop, and stop after a partial/failed start are
  safe and leave `scoRunning() == false`.
- The chosen lifetime coordinator has deterministic coverage for normal and
  early-exit cleanup where practical without requiring a live PipeWire daemon.
- Focused command: `ctest -R '^test_sco_node_monitor$' --output-on-failure`.

### Required Pi row

After cross-build and deployment:

1. Confirm the service starts healthy and the SCO monitor registers normally.
2. Perform repeated clean `systemctl restart openauto-prodigy.service` cycles
   with no teardown crash, hang, watchdog kill, or unexpected `NRestarts` growth.
3. Establish one HFP call so a real SCO node is tracked, end or retain the call as
   the runbook specifies, then cleanly restart the service.
4. Confirm the old registry/node proxies are gone, the new process becomes
   healthy, and a subsequent call can drive SCO state normally.

The implementation plan must turn these into exact commands, evidence to
capture, and pass/fail fields before execution.

## 7. Verification and Closure

### Per-task loop

1. Re-read the current code and turn the claimed failure into a red test or
   deterministic verifier.
2. Record any deviation before changing production code.
3. Apply the smallest root-cause fix.
4. Run the focused target.
5. Commit the task atomically with its regression coverage.

### Phase-level local gate

From the Linux-filesystem build directory:

```bash
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
```

Then cross-build the Pi application:

```bash
./cross-build.sh
```

The cross-build command runs from the repository root.

### Completion gate

The phase stays `ACTIVE` until all of the following hold:

- All three task-level acceptance sets pass.
- The full local build, explicit app-target build, and test suite pass.
- The cross-build passes.
- The SCO Pi row passes.
- `bash scripts/codex-review.sh <base-ref>` completes and every finding is
  explicitly confirmed/fixed or dismissed with a reason.
- Behavior documentation and `docs/session-handoffs.md` are updated with the
  actual verification commands and results.
- The design and implementation plan are marked `COMPLETED 2026-07-20` (or the
  actual completion date) and moved to `docs/archive/plans/` in the same commit.

## 8. Publication and Traceability Contract

- This tracked design is self-contained and references only tracked project
  sources.
- Do not add backlinks to local-only review material, private evidence, or
  opaque internal identifiers.
- Detailed working notes and disposition history remain local-only.
- Public commits, plans, and handoffs describe the repaired ownership contracts
  and their verification without publishing unrelated unresolved defects.
- Do not push the active plan or partial task series. Push only after the phase
  completion gate and explicit user approval.

## 9. Implementation-Plan Handoff

The implementation plan must satisfy the repository Definition of Ready for
each of the three tasks:

- exact files named;
- testable acceptance criteria derived from §§4–6;
- explicit out-of-scope line;
- exact focused test command plus phase-level gates;
- no open design questions.

Expected tiering:

- Video frame lifetime: `Tier: main` (cross-thread buffer ownership)
- Weather object lifetime: `Tier: opus`
- SCO/PipeWire teardown: `Tier: main` (PipeWire lifetime and teardown ordering)
- Mechanical documentation/ledger updates: `Tier: sonnet`

No implementation begins until the implementation plan has passed its
verification loop and is approved.
