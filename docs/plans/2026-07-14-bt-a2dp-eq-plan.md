# BT A2DP Through the Equalizer (+ EQ Hygiene Riders) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Status: ACTIVE
**Design (read it first):** `docs/plans/2026-07-14-bt-a2dp-eq-design.md` — twice
Codex-reviewed (rounds 1+2 adjudicated; header records dispositions).
**Grounded against:** `dev` at `bdd5d74`.

**Goal:** BT A2DP audio flows through the app's Media-curve EQ, master volume,
and focus arbitration via an app-side loopback tap; plus riders: per-consumer
EQ engine instances, durable gains/bypass persistence, Phone→System relabel.

**Architecture:** A WirePlumber rule retargets `bluez_input.*` stream nodes to
a permanent non-autoconnect app capture stream (`openauto-bt-eq-in`); a ring
buffer feeds an activity-toggled playback stream ("BT Audio") whose existing
process path applies EQ + focus gain + master volume. Fallback on any absence
or failure is BT-direct-to-sink (today's behavior).

**Tech stack:** Qt 6.8 system packages, PipeWire 1.4 / WirePlumber 0.5,
BlueZ D-Bus, yaml-cpp, QtTest.

## Global Constraints

- Build in `~/builds/openauto-prodigy` (ext4), NEVER in-repo. Suite:
  `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest --output-on-failure`
- App target is separate from ctest — before claiming green:
  `cmake --build . --target openauto-prodigy -j$(nproc)`
- `IAudioService`/`IEqualizerService`/`IHostContext` virtual signatures MUST NOT
  change (plugin ABI). New capability goes on the concrete classes, reached via
  checked `dynamic_cast` (precedent: `MediaPlayerPlugin.cpp:48`). No
  `HOST_API_VERSION` bump is expected by this plan; if you believe you need
  one, STOP and escalate.
- StreamId values: append/alias only, never renumber (Media=0, Nav=1, System=2,
  Phone=System alias).
- Never set `node.dont-fallback` on the bluez retarget.
- The BT capture stream is non-autoconnect and never targets `inputDevice_`.
- Ring resets only with the stream inactive, under `pw_thread_loop_lock`.
- Lock order everywhere: PW loop lock FIRST, then `mutex_` (existing ABBA rule,
  `AudioService.cpp:80`).
- Workers read root `AGENTS.md` + `src/AGENTS.md` before editing. Commit per
  task; nobody pushes mid-execution (Codex gate happens at the end).
- Docs never state exact test counts.

---

### Task 1: AudioService foundations — options factories, per-handle capture, volume-at-creation, rate-match flag, activity/ring primitives

**Tier:** opus

**Files:**
- Modify: `src/core/services/AudioService.hpp`
- Modify: `src/core/services/AudioService.cpp`
- Test: `tests/test_audio_service.cpp` (extend)

**Interfaces:**
- Consumes: existing `AudioStreamHandle`, `AudioRingBuffer`, `IAudioService`
  (unchanged).
- Produces (on the **concrete** `AudioService` only — later tasks rely on these
  exact names):

```cpp
// AudioService.hpp — public, concrete-class only (NOT on IAudioService)
struct PlaybackStreamOptions {
    QString name;
    int priority = 50;
    int sampleRate = 48000;
    int channels = 2;
    QString targetDevice = QStringLiteral("auto");
    int bufferMs = 50;
    EqualizerEngine* eqEngine = nullptr;   // attached BEFORE pw_stream_connect
    bool startInactive = false;            // adds PW_STREAM_FLAG_INACTIVE
    bool disableRateMatching = false;      // skips the PI controller + set_rate
    std::function<void()> onStreamError;   // PW_STREAM_STATE_ERROR → Qt thread
};
struct CaptureStreamOptions {
    QString name;
    int sampleRate = 48000;
    int channels = 2;
    int bitDepth = 16;
    bool autoconnect = true;   // false ⇒ no AUTOCONNECT flag, inputDevice_ ignored
    IAudioService::CaptureCallback callback;  // installed BEFORE connect, immutable
};
AudioStreamHandle* createStreamWithOptions(const PlaybackStreamOptions& opts);
AudioStreamHandle* openCaptureStreamWithOptions(const CaptureStreamOptions& opts);
void closeCaptureStreamHandle(AudioStreamHandle* handle); // per-handle close
void setStreamActive(AudioStreamHandle* handle, bool active);  // loop-locked
void resetStreamRing(AudioStreamHandle* handle);  // REQUIRES inactive; loop-locked
static float cubicVolume(int masterVolume0to100);  // extracted, pure, testable
```

**What changes and why (map to design §3.1–§3.3, §4.1, §4.2):**

1. `AudioStreamHandle` gains fields:
   ```cpp
   bool isCapture = false;
   bool disableRateMatching = false;
   IAudioService::CaptureCallback captureCallback;      // immutable after connect
   std::atomic<bool> captureCallbackActive{false};      // legacy-path guard only
   std::function<void()> onStreamError;                 // playback error hook
   ```
2. **Per-handle capture.** Replace the single `CaptureState capture_` +
   `captureListener_` members with `QList<AudioStreamHandle*> captures_`; each
   capture handle carries its own `listener`/`events` (the struct already has
   them — today's capture path uses the shared `captureListener_` instead; stop
   that). `onCaptureProcess` userdata becomes the **handle** (like playback),
   not `this`. Note: `grep` proves `openCaptureStream` has **zero in-tree
   callers** — keep the three `IAudioService` capture virtuals working as thin
   wrappers (legacy `openCaptureStream` → options with `autoconnect=true`, no
   callback; `setCaptureCallback` keeps the atomic-guard semantics for
   legacy handles only and must refuse (warn) on handles created with a
   pre-connect callback; `closeCaptureStream` → `closeCaptureStreamHandle`).
3. `createStream` (legacy virtual) delegates to `createStreamWithOptions` with
   defaults — one code path.
4. **Volume-at-creation (design §4.2):** in `createStreamWithOptions`, after
   `pw_stream_connect` succeeds and still under the PW loop lock, apply
   `cubicVolume(masterVolume_)` via `pw_stream_set_control(...,
   SPA_PROP_channelVolumes, ...)` exactly as `setMasterVolume` does (read
   `masterVolume_` under `mutex_` INSIDE the PW lock — same order as
   `setMasterVolume`). Extract the cubic math into `cubicVolume()` so the test
   can assert the curve without a PipeWire daemon.
5. **Start-inactive:** when `opts.startInactive`, add `PW_STREAM_FLAG_INACTIVE`
   to the `pw_stream_connect` flags.
6. **Rate-match flag:** in `onPlaybackProcess`, wrap the entire
   "Adaptive rate matching" block (from `uint32_t avail = ...` through the
   `pw_stream_set_rate` else-branch) in `if (!handle->disableRateMatching)`.
7. **eqEngine before connect:** `createStreamWithOptions` sets
   `handle->eqEngine = opts.eqEngine` before `pw_stream_connect`. (Existing
   post-create assignment sites migrate in Task 3.)
8. **Error surfacing:** add a `state_changed` handler to the playback
   `pw_stream_events`; on `PW_STREAM_STATE_ERROR`, if `handle->onStreamError`
   is set, dispatch it to the Qt main thread with
   `QMetaObject::invokeMethod(qApp, handle->onStreamError, Qt::QueuedConnection)`
   (never run it on the PW thread).
9. **Activity/ring primitives:**
   ```cpp
   void AudioService::setStreamActive(AudioStreamHandle* h, bool active) {
       if (!h || !h->stream || !threadLoop_) return;
       pw_thread_loop_lock(threadLoop_);
       pw_stream_set_active(h->stream, active);
       pw_thread_loop_unlock(threadLoop_);
   }
   void AudioService::resetStreamRing(AudioStreamHandle* h) {
       if (!h || !h->ringBuffer) return;
       if (!threadLoop_) { h->ringBuffer->reset(); return; }
       pw_thread_loop_lock(threadLoop_);   // excludes process callbacks
       h->ringBuffer->reset();
       pw_thread_loop_unlock(threadLoop_);
   }
   ```
   Document on `resetStreamRing`: caller must have deactivated the stream
   first (the lock excludes concurrent callbacks; inactivity keeps the reset
   meaningful).
10. Destructor: tear down `captures_` the way it tears down `streams_`.

**Steps:**

- [ ] **Step 1.1 — failing tests.** In `tests/test_audio_service.cpp` add
  (PipeWire-less paths only — on a machine without a PW daemon,
  `isAvailable()` is false and the factories return nullptr; test what is
  testable headlessly):

  ```cpp
  void testCubicVolumeCurve()
  {
      QCOMPARE(oap::AudioService::cubicVolume(0),   0.0f);
      QCOMPARE(oap::AudioService::cubicVolume(100), 1.0f);
      QVERIFY(qAbs(oap::AudioService::cubicVolume(50) - 0.125f) < 1e-6f);
  }
  void testOptionsFactoriesNullWithoutPipeWire()
  {
      // Only meaningful when PW is unavailable; skip otherwise.
      oap::AudioService svc;
      if (svc.isAvailable()) QSKIP("PipeWire available; factory paths bench-verified");
      oap::AudioService::PlaybackStreamOptions po; po.name = "T";
      QCOMPARE(svc.createStreamWithOptions(po), nullptr);
      oap::AudioService::CaptureStreamOptions co; co.name = "C";
      QCOMPARE(svc.openCaptureStreamWithOptions(co), nullptr);
      svc.setStreamActive(nullptr, true);   // must not crash
      svc.resetStreamRing(nullptr);         // must not crash
  }
  ```
- [ ] **Step 1.2 — run, verify FAIL** (`cubicVolume` undefined):
  `cd ~/builds/openauto-prodigy && cmake --build . --target test_audio_service -j$(nproc)`
  Expected: compile error → that IS the failing state for API tests.
- [ ] **Step 1.3 — implement** items 1–10 above.
- [ ] **Step 1.4 — run full suite + app target, verify green:**
  `ctest --output-on-failure` and
  `cmake --build . --target openauto-prodigy -j$(nproc)`
- [ ] **Step 1.5 — commit:**
  `git commit -m "feat(audio): stream options factories, per-handle capture, volume-at-creation, rate-match opt-out, activity/ring primitives"`

**Acceptance:** new API compiles and is nullptr-safe without PipeWire; cubic
curve tested; adaptive-rate block is skippable per handle; legacy virtuals
delegate; suite + app target green.
**Out of scope:** focus changes (Task 2), EQ service (Task 3), any BT code.

---

### Task 2: Focus recency — (priority, sequence) selection + music priority unification

**Tier:** opus

**Files:**
- Modify: `src/core/services/AudioService.hpp` (handle field + selection helper)
- Modify: `src/core/services/AudioService.cpp` (`requestAudioFocus`, `applyDucking`)
- Modify: `src/plugins/media_player/PlaybackEngine.cpp:120-126` (priority 51→50 + comment)
- Test: `tests/test_audio_service.cpp` (extend)

**Interfaces:**
- Produces:
  ```cpp
  // AudioStreamHandle:
  uint64_t focusSequence = 0;
  // AudioService (public static, pure, for tests):
  static AudioStreamHandle* selectDominant(const QList<AudioStreamHandle*>& streams);
  ```
- `requestAudioFocus` stamps `handle->focusSequence = ++focusSeqCounter_`
  (a `uint64_t focusSeqCounter_ = 0;` member, guarded by `mutex_` like the
  rest of focus state).

**Design §3.5:** dominant = among `hasFocus` streams, highest `priority`;
ties broken by highest `focusSequence` (most recent request). `applyDucking`
keeps its existing mute/duck semantics, only the selection changes:

```cpp
AudioStreamHandle* AudioService::selectDominant(const QList<AudioStreamHandle*>& streams)
{
    AudioStreamHandle* dominant = nullptr;
    for (auto* s : streams) {
        if (!s->hasFocus) continue;
        if (!dominant
            || s->priority > dominant->priority
            || (s->priority == dominant->priority
                && s->focusSequence > dominant->focusSequence))
            dominant = s;
    }
    return dominant;
}
```

`PlaybackEngine::ensureStream`: priority 51 → **50**, and replace the comment
at `PlaybackEngine.cpp:122-124` with:
```cpp
// Priority 50: all music sources (AA Media, Local Media, BT Audio) share one
// priority class; focus recency (focusSequence) makes the most recently
// started source win. Nav speech (60) still ducks/mutes local playback.
```

**Steps:**

- [ ] **Step 2.1 — failing tests** (handles are plain structs — construct
  directly, no PipeWire needed):

  ```cpp
  void testFocusRecencyBreaksTies()
  {
      oap::AudioStreamHandle a, b;
      a.priority = 50; a.hasFocus = true; a.focusSequence = 1;
      b.priority = 50; b.hasFocus = true; b.focusSequence = 2;
      QCOMPARE(oap::AudioService::selectDominant({&a, &b}), &b);
      a.focusSequence = 3;                    // a re-requests focus
      QCOMPARE(oap::AudioService::selectDominant({&a, &b}), &a);
  }
  void testHigherPriorityStillWinsRegardlessOfRecency()
  {
      oap::AudioStreamHandle music, speech;
      music.priority = 50;  music.hasFocus = true; music.focusSequence = 99;
      speech.priority = 60; speech.hasFocus = true; speech.focusSequence = 1;
      QCOMPARE(oap::AudioService::selectDominant({&music, &speech}), &speech);
  }
  void testNoFocusReturnsNull()
  {
      oap::AudioStreamHandle a; a.priority = 50; a.hasFocus = false;
      QCOMPARE(oap::AudioService::selectDominant({&a}), nullptr);
  }
  ```
- [ ] **Step 2.2 — run, verify FAIL** (selectDominant undefined).
- [ ] **Step 2.3 — implement:** field + counter + stamping in
  `requestAudioFocus`; `applyDucking` calls `selectDominant(streams_)`;
  PlaybackEngine 51→50 + comment.
- [ ] **Step 2.4 — grep for stale priority claims:**
  `grep -rn "priority 51\|51," src/plugins/media_player/ docs/reference/` —
  update any doc that documents the 51 hack.
- [ ] **Step 2.5 — full suite + app target green.**
- [ ] **Step 2.6 — commit:**
  `git commit -m "feat(audio): focus recency — (priority, sequence) dominant selection; unify music at priority 50"`

**Acceptance:** the three tests pass; `test_focus_gain` untouched and green;
BT↔AA↔local takeover semantics now derivable from selection tests.
**Out of scope:** BT tap (Task 7) — it merely calls the existing
request/release API.

---

### Task 3: EqualizerService — engine fan-out, authoritative bypass, NaN boundary, consumer migration

**Tier:** opus

**Files:**
- Modify: `src/core/services/EqualizerService.hpp`
- Modify: `src/core/services/EqualizerService.cpp`
- Modify: `src/core/audio/EqualizerEngine.cpp` (`setGain`/`setAllGains` reject non-finite)
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp:332-344` (acquire + options attach)
- Modify: `src/plugins/media_player/MediaPlayerPlugin.cpp:46-49` + `PlaybackEngine.cpp:120-132` (acquire/release + options attach)
- Test: `tests/test_equalizer_service.cpp`, `tests/test_equalizer_engine.cpp` (extend)

**Interfaces:**
- Produces (concrete `EqualizerService`):
  ```cpp
  EqualizerEngine* acquireEngine(StreamId stream, float sampleRate, int channels);
  void releaseEngine(EqualizerEngine* engine);
  ```
  Each acquire returns a heap-owned dedicated instance initialized from the
  stream's current gains AND bypass; the service tracks `(StreamId, engine)`
  pairs; `setGain`/`applyPreset`/`setBypassed` fan out to all instances of the
  StreamId. All of acquire/release/fan-out run on the Qt owner thread
  (document on the methods).
- **Removes** `engineForStream` (in-tree callers migrate here; it was never on
  `IEqualizerService`).
- `StreamState` changes: drop the embedded `engine` member (constructor
  simplifies — no per-stream rate/channel args); add
  `QList<EqualizerEngine*> engines;` and `bool bypassed = false;`
  (authoritative — design §4.4/round-1 F7).
- `setBypassed` updates `bypassed`, fans out, emits, **and calls
  `scheduleSave()`** (round-1 F4 — missing today at
  `EqualizerService.cpp:90-94`).
- `isBypassed` reads `StreamState.bypassed` (not an engine).
- `setGain`/`applyPreset` keep updating `currentGains` as today; fan-out loop
  replaces the single-engine call.
- NaN boundary (design §4.5, round-2 F5): in `EqualizerService::setGain`
  reject non-finite `dB` (`if (!std::isfinite(dB)) return;`); in
  `EqualizerEngine::setGain` and `setAllGains`, treat non-finite input as 0.0f
  before clamping (defense in depth; `std::clamp(NaN)` does not sanitize).

**Consumer migration (RT ordering contract, design §4.4):**

- `AndroidAutoOrchestrator.cpp:332-344` — replace the three `createStream` +
  post-hoc `eqEngine` assignments with acquire-then-options:
  ```cpp
  if (eqService_) {
      mediaEq_  = eqService_->acquireEngine(oap::StreamId::Media, 48000.0f, 2);
      speechEq_ = eqService_->acquireEngine(oap::StreamId::Navigation, 48000.0f, 1);
      systemEq_ = eqService_->acquireEngine(oap::StreamId::System, 16000.0f, 1);
  }
  oap::AudioService::PlaybackStreamOptions mo;
  mo.name = "AA Media"; mo.priority = 50; mo.sampleRate = 48000; mo.channels = 2;
  mo.bufferMs = mediaBufMs; mo.eqEngine = mediaEq_;
  mediaStream_ = concreteAudio_->createStreamWithOptions(mo);
  // ... speech (60, 48000, 1, speechBufMs, speechEq_),
  //     system (40, 16000, 1, systemBufMs, systemEq_) same pattern.
  ```
  `concreteAudio_` = `dynamic_cast<oap::AudioService*>(audioService_)` cached
  in the constructor; if null (tests use mocks), fall back to the legacy
  `createStream` + post-assign path so `test_aa_orchestrator` still works.
  On session teardown (where `destroyStream` is called), release the three
  engines AFTER the destroys, then null the members. New members:
  `oap::EqualizerEngine* mediaEq_/speechEq_/systemEq_ = nullptr;`
- `MediaPlayerPlugin.cpp:46-49` — acquire instead of `engineForStream`:
  ```cpp
  if (auto* eqService = dynamic_cast<oap::EqualizerService*>(context->equalizerService())) {
      eqService_ = eqService;
      engine_->setEqEngine(eqService->acquireEngine(oap::StreamId::Media, 48000.0f, 2));
  }
  ```
  In `shutdown()`: destroy/stop the playback stream first (PlaybackEngine
  already destroys its stream), then `eqService_->releaseEngine(...)`.
  `PlaybackEngine::ensureStream` migrates to `createStreamWithOptions`
  (eqEngine in options, no post-assign; keep the legacy path when the
  concrete cast fails).
- Note: `StreamId::System` does not exist until Task 5 — **this task uses
  `StreamId::Phone` for the system stream** (current name) and Task 5 renames.
  (Do not reorder Tasks 3 and 5 without updating both.)

**Steps:**

- [ ] **Step 3.1 — failing tests:**
  ```cpp
  void testAcquireReturnsDistinctInitializedInstances()
  {
      oap::EqualizerService svc;
      svc.setGain(StreamId::Media, 0, 5.0f);
      auto* e1 = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
      auto* e2 = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
      QVERIFY(e1 && e2 && e1 != e2);
      QCOMPARE(e1->getGain(0), 5.0f);
      QCOMPARE(e2->getGain(0), 5.0f);
      svc.releaseEngine(e1); svc.releaseEngine(e2);
  }
  void testFanOutPropagatesToAllInstances()
  {
      oap::EqualizerService svc;
      auto* e1 = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
      auto* e2 = svc.acquireEngine(StreamId::Media, 44100.0f, 2);
      svc.applyPreset(StreamId::Media, "Rock");
      const auto* rock = oap::findBundledPreset("Rock");
      QCOMPARE(e1->getGain(0), rock->gains[0]);
      QCOMPARE(e2->getGain(0), rock->gains[0]);
      svc.setBypassed(StreamId::Media, true);
      QVERIFY(e1->isBypassed() && e2->isBypassed());
      svc.releaseEngine(e2);
      svc.setGain(StreamId::Media, 1, -3.0f);
      QCOMPARE(e1->getGain(1), -3.0f);        // still fans out to live engine
      svc.releaseEngine(e1);
      svc.setGain(StreamId::Media, 2, 4.0f);  // no engines — must not crash
  }
  void testBypassAuthoritativeWithoutEngines()
  {
      oap::EqualizerService svc;
      svc.setBypassed(StreamId::Media, true);
      QVERIFY(svc.isBypassed(StreamId::Media));              // no engine involved
      auto* e = svc.acquireEngine(StreamId::Media, 48000.0f, 2);
      QVERIFY(e->isBypassed());                              // inherited at acquire
      svc.releaseEngine(e);
  }
  void testNonFiniteGainRejected()
  {
      oap::EqualizerService svc;
      svc.setGain(StreamId::Media, 0, 5.0f);
      svc.setGain(StreamId::Media, 0, std::nanf(""));
      QCOMPARE(svc.gain(StreamId::Media, 0), 5.0f);          // unchanged
  }
  // tests/test_equalizer_engine.cpp:
  void testEngineSanitizesNonFinite()
  {
      oap::EqualizerEngine eng(48000.0f, 2);
      eng.setGain(0, std::nanf(""));
      QCOMPARE(eng.getGain(0), 0.0f);
      std::array<float, oap::kNumBands> g{}; g[3] = INFINITY;
      eng.setAllGains(g);
      QCOMPARE(eng.getGain(3), 0.0f);
  }
  ```
  Also update the existing per-instance-state expectation: process on `e1`
  with a step signal, verify `e2`'s output on the same signal is bit-identical
  to a fresh engine (states independent — reuse the pattern in
  `test_equalizer_engine.cpp`).
- [ ] **Step 3.2 — run, verify FAIL.**
- [ ] **Step 3.3 — implement** service + engine changes; migrate orchestrator,
  MediaPlayerPlugin, PlaybackEngine; delete `engineForStream` and fix any
  remaining callers (`grep -rn "engineForStream" src/ tests/`).
- [ ] **Step 3.4 — full suite + app target green** (`test_aa_orchestrator`,
  `test_playback_engine`, `test_equalizer_*` are the risk set).
- [ ] **Step 3.5 — commit:**
  `git commit -m "feat(eq): per-consumer engine instances with fan-out; authoritative bypass; finite-gain boundary"`

**Acceptance:** all new tests green; the shared-Media-engine defect is gone
(each consumer owns an instance); `engineForStream` absent from the tree;
bypass changes arm the save debounce.
**Out of scope:** disk persistence (Task 4), rename (Task 5).

---

### Task 4: Durable EQ persistence — gains/bypass round-trip to disk, validation, parent-dir fsync

**Tier:** opus

**Files:**
- Modify: `src/core/YamlConfig.hpp` / `src/core/YamlConfig.cpp`
- Modify: `src/core/services/ConfigService.hpp` / `.cpp` (`save()` returns bool)
- Modify: `src/core/services/EqualizerService.hpp` / `.cpp` (flush hook, gains/bypass IO)
- Modify: `src/main.cpp` (wire the flush hook)
- Test: `tests/test_yaml_config.cpp`, `tests/test_equalizer_service.cpp` (extend)

**Interfaces:**
- Produces (YamlConfig):
  ```cpp
  // Empty list ⇒ absent or invalid (validation below). Valid ⇒ exactly kNumBands entries.
  QList<float> eqStreamGains(const QString& streamName) const;
  void setEqStreamGains(const QString& streamName, const QList<float>& gains);
  bool eqStreamBypassed(const QString& streamName) const;   // default false
  void setEqStreamBypassed(const QString& streamName, bool bypassed);
  ```
- Produces (EqualizerService):
  ```cpp
  using FlushFn = std::function<bool()>;
  void setFlushHook(FlushFn fn);   // called by writeToConfig after mutating YamlConfig
  ```
- `ConfigService::save()` signature becomes `bool save()` (returns
  `config_->save(configPath_)` — round-2 F7 "result discarded").

**Validation (design §4.5, round-2 F5) — one shared helper in YamlConfig:**
a gains node is valid iff it is a Sequence of exactly `kNumBands` (10) scalars
that all parse as finite floats; each value clamps to ±12 dB on read. Invalid
⇒ `eqStreamGains` returns empty AND `eqUserPresets()` **drops the offending
preset** (log a warning naming it). Apply the helper in both readers.

**Durability (round-2 F7):** in `YamlConfig::save` after `::rename` succeeds,
fsync the parent directory:
```cpp
const std::string dirPath = QFileInfo(filePath).absolutePath().toStdString();
int dfd = ::open(dirPath.c_str(), O_RDONLY | O_DIRECTORY);
if (dfd >= 0) { ::fsync(dfd); ::close(dfd); }
else qWarning() << "[YamlConfig] save: cannot fsync parent dir" << QString::fromStdString(dirPath);
```

**EqualizerService IO:**
- `writeToConfig()` additionally writes, per stream (names from the existing
  `streamNames[]` array): `setEqStreamGains(name, currentGains-as-QList)` and
  `setEqStreamBypassed(name, state.bypassed)` — **always** written; then calls
  `flushFn_` if set. On `flushFn_() == false`: log
  `qCWarning(lcEq) << "EQ config flush failed; retrying"` and restart
  `saveTimer_` (dirty state retained — the next debounce retries).
- `saveNow()` unchanged flow but now actually flushes (via `writeToConfig`).
- `loadFromConfig()` per stream: if preset name non-empty → `applyPreset`
  (as today); **else** if `eqStreamGains` valid → apply raw gains via
  `setGain`-equivalent internal path that does NOT clear the (already empty)
  preset name and does NOT schedule a save; `bypassed` always restored via the
  authoritative bool + fan-out.
- `main.cpp`: after constructing `eqService`, wire
  `eqService->setFlushHook([yc = yamlConfig.get(), path = configPath]() {
  return yc->save(path); });` — locate the exact config-path variable name in
  main.cpp when implementing (it is the same path `yamlConfig->load(...)` was
  given; `grep -n "yamlConfig->load\|configPath" src/main.cpp`).

**Steps:**

- [ ] **Step 4.1 — failing tests:**
  ```cpp
  // test_yaml_config.cpp
  void testEqGainsRoundTripToDisk()
  {
      const QString path = QDir::temp().filePath("eqgains-test.yaml");
      QFile::remove(path);
      {
          oap::YamlConfig cfg; cfg.load(path);
          QList<float> g; for (int i = 0; i < 10; ++i) g << float(i) - 4.5f;
          cfg.setEqStreamGains("media", g);
          cfg.setEqStreamBypassed("media", true);
          QVERIFY(cfg.save(path));
      }
      oap::YamlConfig fresh; fresh.load(path);          // brand-new object
      auto g2 = fresh.eqStreamGains("media");
      QCOMPARE(g2.size(), 10);
      QCOMPARE(g2[0], -4.5f);
      QVERIFY(fresh.eqStreamBypassed("media"));
      QFile::remove(path);
  }
  void testEqGainsValidationRejectsMalformed()
  {
      const QString path = QDir::temp().filePath("eqbad-test.yaml");
      QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("audio:\n  equalizer:\n    streams:\n"
              "      media: { gains: [1, 2, .nan, 4, 5, 6, 7, 8, 9, 10] }\n"
              "      navigation: { gains: [1, 2, 3] }\n"
              "    user_presets:\n"
              "      - { name: Bad, gains: [.inf, 2, 3, 4, 5, 6, 7, 8, 9, 10] }\n"
              "      - { name: Good, gains: [1, 1, 1, 1, 1, 1, 1, 1, 1, 1] }\n");
      f.close();
      oap::YamlConfig cfg; cfg.load(path);
      QVERIFY(cfg.eqStreamGains("media").isEmpty());        // NaN ⇒ invalid
      QVERIFY(cfg.eqStreamGains("navigation").isEmpty());   // short ⇒ invalid
      auto presets = cfg.eqUserPresets();
      QCOMPARE(presets.size(), 1);                          // "Bad" dropped
      QCOMPARE(presets[0].name, QString("Good"));
      QFile::remove(path);
  }
  // test_equalizer_service.cpp
  void testUnsavedGainsAndBypassSurviveRestart()
  {
      const QString path = QDir::temp().filePath("eqsvc-test.yaml");
      QFile::remove(path);
      auto cfg = std::make_unique<oap::YamlConfig>(); cfg->load(path);
      {
          oap::EqualizerService svc(cfg.get());
          svc.setFlushHook([&]{ return cfg->save(path); });
          svc.setGain(StreamId::Media, 0, 7.5f);   // "Custom" — no preset saved
          svc.setBypassed(StreamId::Navigation, true);
          svc.saveNow();
      }
      auto cfg2 = std::make_unique<oap::YamlConfig>(); cfg2->load(path);
      oap::EqualizerService svc2(cfg2.get());
      QCOMPARE(svc2.gain(StreamId::Media, 0), 7.5f);
      QCOMPARE(svc2.activePreset(StreamId::Media), QString(""));  // still Custom
      QVERIFY(svc2.isBypassed(StreamId::Navigation));
      QFile::remove(path);
  }
  void testFlushFailureRearmsDebounce()
  {
      oap::YamlConfig cfg;
      oap::EqualizerService svc(&cfg);
      int calls = 0;
      svc.setFlushHook([&]{ ++calls; return false; });
      svc.setGain(StreamId::Media, 0, 1.0f);
      svc.saveNow();
      QCOMPARE(calls, 1);
      // debounce re-armed on failure: saveTimer_ active again
      QTRY_VERIFY_WITH_TIMEOUT(calls >= 2, 5000);
  }
  ```
- [ ] **Step 4.2 — run, verify FAIL.**
- [ ] **Step 4.3 — implement** (YamlConfig accessors + validation + parent
  fsync; ConfigService bool; EqualizerService write/load/flush/retry;
  main.cpp hook). Check the one existing `ConfigService::save()` caller
  compiles (`grep -rn "configService->save\|\.save()" src/ | grep -i config`).
- [ ] **Step 4.4 — full suite + app target green.**
- [ ] **Step 4.5 — commit:**
  `git commit -m "feat(eq): durable persistence — raw gains + bypass round-trip, gains validation at every ingress, fsync parent dir, surfaced save failures"`

**Acceptance:** power-cut semantics hold (fresh-object disk round-trip test);
malformed YAML can't poison the filter chain; flush failure retries.
**Out of scope:** key migration (Task 5).

---

### Task 5: Phone → System relabel + raw-YAML config migration

**Tier:** opus

**Files:**
- Modify: `src/core/services/IEqualizerService.hpp` (enum alias only)
- Modify: `src/core/services/EqualizerService.hpp` / `.cpp` (rename property/signal/keys)
- Modify: `src/core/YamlConfig.cpp` (defaults key + `migrateEqPhoneToSystem`)
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp` (StreamId::Phone → System)
- Modify: `qml/applications/settings/EqSettings.qml` (segment label + signal handler)
- Test: `tests/test_yaml_config.cpp`, `tests/test_equalizer_service.cpp` (extend)

**Interfaces:**
- `IEqualizerService.hpp`:
  ```cpp
  enum class StreamId {
      Media,
      Navigation,
      System,           // renamed from Phone — value stays 2
      Phone = System    // deprecated source-compat alias for external plugins
  };
  ```
  (Case labels: in-tree switches use `System` only — `Phone` is the same value
  and would be a duplicate case.)
- `EqualizerService`: `Q_PROPERTY systemPreset` + `systemPresetChanged()`
  replace the phone pair (declared in-tree QML break, recorded in the design
  header); `streamNames[]` arrays become `{"media", "navigation", "system"}`;
  `emitPresetSignal` case `System`.
- `YamlConfig::initDefaults()` line 111: `["phone"]` → `["system"]`.
- Migration (design §4.6, round-1 F5 — MUST run on raw YAML before merge), in
  `YamlConfig::load` between `LoadFile` and `mergeYaml`:
  ```cpp
  YAML::Node loaded = YAML::LoadFile(path);
  migrateEqPhoneToSystem(loaded);
  root_ = mergeYaml(defaults, loaded);
  ```
  ```cpp
  void YamlConfig::migrateEqPhoneToSystem(YAML::Node& loaded)
  {
      auto streams = loaded["audio"]["equalizer"]["streams"];
      if (!streams || !streams.IsMap()) return;
      if (streams["phone"]) {
          if (!streams["system"])
              streams["system"] = YAML::Clone(streams["phone"]);
          streams.remove("phone");
      }
  }
  ```
- `EqSettings.qml`: `options: ["Media", "Nav", "Phone"]` →
  `["Media", "Nav", "System"]`; `onPhonePresetChanged` →
  `onSystemPresetChanged` (same body).
- `AndroidAutoOrchestrator.cpp` system-stream acquire (Task 3 code):
  `StreamId::Phone` → `StreamId::System` — the mislabel is now honest.

**Steps:**

- [ ] **Step 5.1 — failing tests:**
  ```cpp
  // test_yaml_config.cpp
  void testPhoneToSystemMigration()
  {
      const QString path = QDir::temp().filePath("eqmig-test.yaml");
      QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("audio:\n  equalizer:\n    streams:\n"
              "      phone: { preset: Rock }\n");
      f.close();
      oap::YamlConfig cfg; cfg.load(path);
      QCOMPARE(cfg.eqStreamPreset("system"), QString("Rock"));  // migrated
      QVERIFY(cfg.save(path));
      QFile rf(path); QVERIFY(rf.open(QIODevice::ReadOnly));
      const QByteArray out = rf.readAll();
      QVERIFY(!out.contains("phone:"));                          // old key gone
      QVERIFY(out.contains("system:"));
      QFile::remove(path);
  }
  void testPhoneToSystemBothPresentKeepsSystem()
  {
      const QString path = QDir::temp().filePath("eqmig2-test.yaml");
      QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("audio:\n  equalizer:\n    streams:\n"
              "      phone: { preset: Rock }\n"
              "      system: { preset: Jazz }\n");
      f.close();
      oap::YamlConfig cfg; cfg.load(path);
      QCOMPARE(cfg.eqStreamPreset("system"), QString("Jazz"));
      QFile::remove(path);
  }
  // test_equalizer_service.cpp — legacy config end-to-end
  void testLegacyPhoneKeyRestoresSystemStream()
  {
      const QString path = QDir::temp().filePath("eqmig3-test.yaml");
      QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
      f.write("audio:\n  equalizer:\n    streams:\n"
              "      phone: { preset: Rock }\n");
      f.close();
      oap::YamlConfig cfg; cfg.load(path);
      oap::EqualizerService svc(&cfg);
      QCOMPARE(svc.activePreset(StreamId::System), QString("Rock"));
      QFile::remove(path);
  }
  ```
- [ ] **Step 5.2 — run, verify FAIL.**
- [ ] **Step 5.3 — implement.** Then sweep:
  `grep -rn "StreamId::Phone\|phonePreset\|\"phone\"" src/ qml/ tests/` —
  every hit is either the enum alias declaration or gets migrated (telephony
  code legitimately says "phone" — only EQ-context hits count; judge each).
- [ ] **Step 5.4 — full suite + app target + settings-menu structure test**
  (`test_settings_menu_structure` walks QML — it must still pass).
- [ ] **Step 5.5 — commit:**
  `git commit -m "feat(eq): Phone stream renamed System (honest labeling) with raw-YAML config migration; deprecated enum alias kept"`

**Acceptance:** legacy `phone:` configs migrate (only-phone and both-present
cases), serialized output drops the old key, UI shows "System", value 2
unchanged.
**Out of scope:** routing SCO call audio through EQ (explicitly out, design §4.6).

---

### Task 6: BtAudioPlugin — transportActiveChanged edge

**Tier:** opus

**Files:**
- Modify: `src/plugins/bt_audio/BtAudioPlugin.hpp` / `.cpp`
- Test: `tests/test_bt_audio_plugin.cpp` (extend — read it first to reuse its
  existing D-Bus-less seams)

**Interfaces:**
- Produces:
  ```cpp
  signals:
      void transportActiveChanged(bool active);
  public:
      bool transportActive() const { return transportActive_; }
  private:
      void setTransportActive(bool active);   // edge-emits only on change
      bool transportActive_ = false;
  ```

**Design §3.2 (round-2 F1):** `transportActive_` is true iff the tracked
transport's `MediaTransport1.State == "active"`. Drive it from every place
state is learned:
- `updateTransportState(state)` (`BtAudioPlugin.cpp:350`): keep the existing
  Connected/Disconnected UI mapping EXACTLY as-is; add
  `setTransportActive(state == QLatin1String("active"));`
- `onInterfacesAdded` transport branch: already reads `State` via
  `updateTransportState` — covered.
- `onInterfacesRemoved` transport branch (`:301-314`): add
  `setTransportActive(false);`
- BlueZ service disappearance: `bluezWatcher_` exists — find its
  serviceUnregistered handling in `startDBusMonitoring`/`stopDBusMonitoring`
  and add `setTransportActive(false)` there (grep
  `bluezWatcher_` call sites; if no disconnect handler exists, connect
  `QDBusServiceWatcher::serviceUnregistered` to a lambda clearing transport
  state + `setTransportActive(false)`).
- `scanExistingObjects` (GetManagedObjects init path, `:229` area): it calls
  `updateTransportState` for a found transport — covered; verify by reading.

**Steps:**

- [ ] **Step 6.1 — read `tests/test_bt_audio_plugin.cpp`** to learn its seam
  (it tests the plugin without a live bus — reuse the same pattern; if it
  exercises `updateTransportState` indirectly, make the new tests call the
  same entry points).
- [ ] **Step 6.2 — failing tests** (adapt entry points to the seam found):
  ```cpp
  void testTransportActiveEdgeFollowsState()
  {
      oap::plugins::BtAudioPlugin p;
      QSignalSpy spy(&p, &oap::plugins::BtAudioPlugin::transportActiveChanged);
      // idle → no edge (starts false)
      p.testHook_updateTransportState("idle");     // or the seam's equivalent
      QCOMPARE(spy.count(), 0);
      p.testHook_updateTransportState("active");
      QCOMPARE(spy.count(), 1);
      QCOMPARE(spy.last().at(0).toBool(), true);
      p.testHook_updateTransportState("active");   // no re-emit on same value
      QCOMPARE(spy.count(), 1);
      p.testHook_updateTransportState("pending");
      QCOMPARE(spy.count(), 2);
      QCOMPARE(spy.last().at(0).toBool(), false);
  }
  void testTransportRemovalForcesInactive()
  {
      // via the seam for onInterfacesRemoved / BlueZ loss: expect a false edge
  }
  ```
  (If the existing tests reach private methods some other way — friend class,
  test-only subclass, or driving `onPropertiesChanged` with a synthesized
  `QDBusMessage` — use THAT mechanism instead of inventing a new hook. Read
  first, then decide; the acceptance is the transition table, not the
  mechanism.)
- [ ] **Step 6.3 — run, verify FAIL.**
- [ ] **Step 6.4 — implement.**
- [ ] **Step 6.5 — full suite + app target green.**
- [ ] **Step 6.6 — commit:**
  `git commit -m "feat(bt): transportActiveChanged edge from MediaTransport1.State — interface presence is not audio activity"`

**Acceptance:** edge fires exactly on active↔non-active transitions, including
removal and BlueZ loss; UI Connected-state behavior unchanged.
**Out of scope:** the tap itself (Task 7).

---

### Task 7: BtAudioTap — the loopback tap

**Tier:** opus

**Files:**
- Create: `src/plugins/bt_audio/BtTapController.hpp` (pure state machine, header-only)
- Create: `src/plugins/bt_audio/BtAudioTap.hpp` / `.cpp`
- Modify: `src/plugins/bt_audio/BtAudioPlugin.hpp` / `.cpp` (own + wire the tap)
- Modify: `src/CMakeLists.txt` (add the new .cpp to openauto-core's plugin sources — find the bt_audio source list)
- Test: `tests/test_bt_tap_controller.cpp` (new), `tests/CMakeLists.txt` (one `oap_add_test` line)

**Interfaces:**
- `BtTapController` — pure, PipeWire-free sequencing logic (design §4.3
  "factor the lifecycle/state machine into a plain testable class"):
  ```cpp
  // Drives the §3.1/§3.2 contracts; the owner (BtAudioTap) executes effects.
  class BtTapController {
  public:
      enum class State { Stopped, Ready, Active };
      struct Effects {
          std::function<bool()> acquireEngine;     // step 1
          std::function<bool()> createPlayback;    // step 2 (inactive)
          std::function<bool()> createCapture;     // step 3 (LAST — publishes target)
          std::function<void()> destroyCapture;    // teardown FIRST
          std::function<void()> destroyPlayback;
          std::function<void()> releaseEngine;
          std::function<void()> activate;          // reset ring (inactive) → set active → request focus
          std::function<void()> deactivate;        // release focus → set inactive → reset ring
      };
      explicit BtTapController(Effects fx) : fx_(std::move(fx)) {}
      bool start();                        // Stopped → Ready; rollback on any step failure
      void stop();                         // any → Stopped (capture-first)
      void onTransportActive(bool active); // Ready ↔ Active; ignored when Stopped
      void onStreamError();                // any → Stopped (capture-first)
      State state() const { return state_; }
  private:
      Effects fx_; State state_ = State::Stopped;
  };
  ```
  `start()` semantics: run acquireEngine → createPlayback → createCapture; on
  the FIRST failure, unwind exactly the steps that succeeded, in reverse
  teardown order (capture never exists at that point by construction; playback
  destroyed before engine release), return false, state Stopped.
  `stop()` from Active first runs `deactivate`, then destroyCapture →
  destroyPlayback → releaseEngine.
- `BtAudioTap` (QObject) — owns the effects against the real services:
  ```cpp
  class BtAudioTap : public QObject {
  public:
      BtAudioTap(oap::AudioService* audio, oap::EqualizerService* eq,
                 QObject* parent = nullptr);
      ~BtAudioTap();                       // stop()
      bool start();
      void stop();
      void setTransportActive(bool active);
      bool isRunning() const;
  };
  ```
  Effect implementations (exact contracts):
  - acquireEngine: `engine_ = eq_->acquireEngine(oap::StreamId::Media, 48000.0f, 2)`
    (after Task 5 the id literal stays `Media` — BT follows the Media curve).
  - createPlayback (repo is C++17 — member-assignment, no designated init):
    ```cpp
    oap::AudioService::PlaybackStreamOptions po;
    po.name = QStringLiteral("BT Audio");
    po.priority = 50; po.sampleRate = 48000; po.channels = 2; po.bufferMs = 200;
    po.eqEngine = engine_;
    po.startInactive = true;
    po.disableRateMatching = true;
    po.onStreamError = [this]{ controller_.onStreamError(); };
    playback_ = audio_->createStreamWithOptions(po);
    ```
  - createCapture:
    ```cpp
    oap::AudioService::CaptureStreamOptions co;
    co.name = QStringLiteral("openauto-bt-eq-in");
    co.sampleRate = 48000; co.channels = 2; co.bitDepth = 16;
    co.autoconnect = false;
    co.callback = [this](const uint8_t* d, int n) {
        audio_->writeAudio(playback_, d, n);
    };
    capture_ = audio_->openCaptureStreamWithOptions(co);
    ```
    NOTE: the callback runs on the PW thread; `writeAudio` is documented
    any-thread-safe (ring write) — no Qt calls in it, ever.
  - activate: `audio_->resetStreamRing(playback_); audio_->setStreamActive(
    playback_, true); audio_->requestAudioFocus(playback_,
    oap::AudioFocusType::Gain);`
  - deactivate: `audio_->releaseAudioFocus(playback_);
    audio_->setStreamActive(playback_, false);
    audio_->resetStreamRing(playback_);`
  - destroyCapture: `audio_->closeCaptureStreamHandle(capture_)`;
    destroyPlayback: `audio_->destroyStream(playback_)`;
    releaseEngine: `eq_->releaseEngine(engine_)`.
- Plugin wiring in `BtAudioPlugin::initialize` (after existing init):
  ```cpp
  auto* audio = dynamic_cast<oap::AudioService*>(context->audioService());
  auto* eq = dynamic_cast<oap::EqualizerService*>(context->equalizerService());
  if (audio && eq && audio->isAvailable()) {
      tap_ = new BtAudioTap(audio, eq, this);
      if (tap_->start()) {
          connect(this, &BtAudioPlugin::transportActiveChanged,
                  tap_, &BtAudioTap::setTransportActive);
          if (transportActive_) tap_->setTransportActive(true);  // late-start catch-up
          hostContext_->log(LogLevel::Info, QStringLiteral("BtAudio: EQ tap running (openauto-bt-eq-in)"));
      } else {
          hostContext_->log(LogLevel::Warning, QStringLiteral("BtAudio: EQ tap failed to start — BT audio direct (un-EQ'd)"));
      }
  }
  ```
  `shutdown()`: `if (tap_) tap_->stop();` BEFORE `stopDBusMonitoring()`.

**Steps:**

- [ ] **Step 7.1 — failing controller tests** (`tests/test_bt_tap_controller.cpp`;
  register in `tests/CMakeLists.txt` beside the other plugin tests:
  `oap_add_test(test_bt_tap_controller SOURCES test_bt_tap_controller.cpp)`):
  ```cpp
  // A recording Effects fixture: each hook appends its tag to `log` and
  // returns a scripted success/failure.
  void testStartHappyPathOrder()
  {   // acquire → playback → capture, state Ready, no teardown calls
      QCOMPARE(log, QStringList({"acquire", "playback", "capture"})); }
  void testStartFailsAtPlaybackUnwindsEngineOnly()
  {   // scripted: playback=false ⇒ log ends "releaseEngine"; capture NEVER called
      QCOMPARE(log, QStringList({"acquire", "playback", "releaseEngine"})); }
  void testStartFailsAtCaptureUnwindsPlaybackThenEngine()
  {   QCOMPARE(log, QStringList({"acquire", "playback", "capture",
                                  "destroyPlayback", "releaseEngine"})); }
  void testStopFromActiveDeactivatesThenTearsDownCaptureFirst()
  {   // start → onTransportActive(true) → stop
      QCOMPARE(log.mid(3), QStringList({"activate", "deactivate",
          "destroyCapture", "destroyPlayback", "releaseEngine"})); }
  void testTransportEdgesToggleOnlyWhenReady()
  {   // edges while Stopped are ignored; true→Active, false→Ready; duplicate
      // edges don't re-run effects
  }
  void testStreamErrorTearsDownCaptureFirst()
  {   // from Active: deactivate then capture-first teardown, state Stopped
  }
  ```
- [ ] **Step 7.2 — run, verify FAIL** (controller header absent).
- [ ] **Step 7.3 — implement** controller (header-only), tap, plugin wiring,
  CMake entries.
- [ ] **Step 7.4 — full suite + app target green.** The app must boot on a
  PipeWire-less dev box exactly as before (tap simply doesn't start —
  `audio->isAvailable()` gate).
- [ ] **Step 7.5 — commit:**
  `git commit -m "feat(bt): BT A2DP loopback tap — ordered bring-up, capture-first teardown, transport-edge activity + focus"`

**Acceptance:** controller transition table fully covered incl. both partial
bring-up failures and stream error; the tap wires only when both concrete
services resolve and PipeWire is up.
**Out of scope:** the WirePlumber rule (Task 8) — without it the tap idles
harmlessly (nothing targets `openauto-bt-eq-in`).

---

### Task 8: WirePlumber rule, installers, docs, src/AGENTS.md correction

**Tier:** sonnet

**Files:**
- Create: `config/50-openauto-bt-eq.conf`
- Modify: `install.sh` (follow the clock-sync polkit pattern at `install.sh:1433-1437`)
- Modify: `install-prebuilt.sh` (same pattern — find its polkit-rules section)
- Modify: `src/AGENTS.md:19` (buffer rule correction — design §8 / round-2 F6)
- Modify: `docs/architecture.md` (audio section: tap + focus recency + EQ fan-out)
- Modify: `docs/reference/config-schema.md` — if this exact file doesn't exist,
  `grep -rl "equalizer" docs/reference/ docs/*.md` and update whichever file
  documents `audio.equalizer.*` keys (new: `gains`, `bypassed`, `system` key
  migration note)
- Test: none compiled — `bash -n install.sh install-prebuilt.sh` + the
  fresh-install bench row

**Content — `config/50-openauto-bt-eq.conf` (exact, design §3.4):**
```
# OpenAuto Prodigy: route BT A2DP input streams through the app's EQ tap.
# The target is the app's capture node "openauto-bt-eq-in". When the app is
# not running the target does not exist and WirePlumber falls back to the
# default sink — BT audio still plays, just un-EQ'd. Do NOT add
# node.dont-fallback here; the fallback IS the failure mode.
monitor.bluez.rules = [
  {
    matches = [
      {
        node.name = "~bluez_input.*"
        media.class = "Stream/Output/Audio"
      }
    ]
    actions = {
      update-props = {
        target.object = "openauto-bt-eq-in"
      }
    }
  }
]
```

**Installer snippet (both installers, adjacent to their polkit-rule steps):**
```bash
# WirePlumber rule: BT A2DP audio routes through the app EQ tap (falls back
# to direct playback when the app is down).
if [[ -f "$INSTALL_DIR/config/50-openauto-bt-eq.conf" ]]; then
    sudo mkdir -p /etc/wireplumber/wireplumber.conf.d
    sudo cp "$INSTALL_DIR/config/50-openauto-bt-eq.conf" /etc/wireplumber/wireplumber.conf.d/
    systemctl --user restart wireplumber 2>/dev/null || true
    ok "BT-EQ WirePlumber rule installed"
fi
```
(Adapt `$INSTALL_DIR` + `ok` to each installer's local conventions — read the
surrounding code; install-prebuilt.sh may stage from the tarball root instead.)

**`src/AGENTS.md` line 19 — replace:**
```
- **Playback: always output full periods.** Set `d.chunk->size = maxSize` and silence-fill any gap. ...
```
with:
```
- **Playback: always fill and publish the full requested period.** Compute `wantBytes` from `buf->requested` (bounded by `d.maxsize` — that is buffer *capacity*, not the request) and silence-fill any gap; PipeWire's resampler sets `requested` to exactly what it needs. Variable short `chunk->size` values cause tempo wobble ("skippy" audio).
```

**Steps:**

- [ ] **Step 8.1 —** write the conf file (verbatim above).
- [ ] **Step 8.2 —** patch both installers; run `bash -n install.sh` and
  `bash -n install-prebuilt.sh` (expect: no output, exit 0). Check
  `tools/package-prebuilt-release.sh` — if it enumerates `config/` files
  explicitly, add the new conf; if it copies the directory, no change
  (read it to find out).
- [ ] **Step 8.3 —** fix `src/AGENTS.md:19` (verbatim above).
- [ ] **Step 8.4 —** docs: architecture.md audio paragraph (tap data path +
  "music focus recency" + per-consumer engines); config-schema doc (new keys +
  migration); `docs/wishlist.md` § "From EQ parity audit (2026-07-14)" —
  annotate items 2–4 `SHIPPING — 2026-07-14 plan in execution` (final SHIPPED
  flip happens at ship time with the bench evidence).
- [ ] **Step 8.5 —** commit:
  `git commit -m "feat(bt-eq): WirePlumber retarget rule + installer wiring; docs; fix src/AGENTS.md period rule"`

**Acceptance:** `bash -n` clean on both installers; conf file matches the
design fragment byte-for-byte on the rule body; AGENTS.md no longer
contradicts the code.
**Out of scope:** none (last code task).

---

## Integration verification (main session, after all tasks)

- [ ] Full local build + suite + app target:
  `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest --output-on-failure && cmake --build . --target openauto-prodigy -j$(nproc)`
- [ ] Cross-build: `./cross-build.sh` (app target, ~4-6 min).
- [ ] **Codex pre-push gate:** `bash scripts/codex-review.sh` — adjudicate
  every finding per AGENTS.md; record in session-handoffs; substantial fixes
  ⇒ one gate re-run.
- [ ] Deploy to Pi (`rsync` + restart per AGENTS.md), then the **bench
  runbook** (design §7, needs Matthew + phone, one sitting):
  1. `pw-dump` the `bluez_input` node while streaming — confirm
     `media.class = "Stream/Output/Audio"`, name pattern, no dont-fallback.
     **Run BEFORE installing the rule** — if properties differ from the
     design's assumption, adjust `config/50-openauto-bt-eq.conf` (one line)
     and note it.
  2. `pw-link -l`: bluez → `openauto-bt-eq-in`; assert NO mic/source link
     into `openauto-bt-eq-in`.
  3. Audible preset swap during BT playback.
  4. HU master volume changes BT level; boot muted → BT stays muted.
  5. Focus: speech prompt ducks BT; BT↔AA takeover both directions.
  6. Failure: stop the service mid-playback → music continues direct;
     restart → tap resumes on next transport-active.
  7. Idle: no transport → playback inactive; idle CPU comparable to today.
  8. 44.1 kHz + 48 kHz sources; ≥10 min run — ring fill/drops/pitch stable.
  9. AA regression: AA media EQ'd, speech ducking intact.
  10. Fresh-install row (installer places the conf; restart order respected).
- [ ] Record RESULT rows in `docs/session-handoffs.md`; flip wishlist items to
  SHIPPED; flip design + this plan to COMPLETED and move both to
  `docs/archive/plans/` in the same commit.
- [ ] Push (with Matthew's go), then milestone: dev→main PR + next ALPHA tag +
  GitHub prerelease (Matthew declared 2026-07-14 that the milestone follows
  this work).

## Task dependencies

```
Task 1 (AudioService foundations) ──┬─→ Task 3 (EQ fan-out, uses options factory)
Task 2 (focus recency)             ─┤     └─→ Task 5 (relabel, renames Task-3 code)
                                    └─→ Task 7 (tap; also needs Tasks 2, 3, 6)
Task 4 (persistence) — after Task 3 (bypass/scheduleSave state exists)
Task 6 (transport edge) — independent
Task 8 (conf/installers/docs) — independent, but commit after Task 7 so the
                                rule never precedes the tap in history
Execution order: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8.
```
