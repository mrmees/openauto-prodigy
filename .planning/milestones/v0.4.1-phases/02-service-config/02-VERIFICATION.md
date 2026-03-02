---
phase: 02-service-config
verified: 2026-03-02T01:30:00Z
status: passed
score: 12/12 must-haves verified
re_verification: false
---

# Phase 02: Service Config Verification Report

**Phase Goal:** Users hear EQ-processed audio on all three AA streams with per-stream profiles, bundled presets, user preset save/load, and settings that survive reboot
**Verified:** 2026-03-02T01:30:00Z
**Status:** PASSED
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

All truths are drawn directly from the plan must_haves. Both plans contributed truths; all are evaluated.

**Plan 01 Truths (PRST-01 through PRST-04):**

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 1 | 9 bundled presets exist with correct names (Flat, Rock, Pop, Jazz, Classical, Bass Boost, Treble Boost, Vocal, Voice) | VERIFIED | `kBundledPresets[]` in `EqualizerPresets.hpp` lines 17-27; all 9 names present as constexpr data |
| 2 | EqualizerService manages 3 independent EqualizerEngine instances (media 48kHz/2ch, navigation 48kHz/1ch, phone 16kHz/1ch) | VERIFIED | `streams_[3]` array in `EqualizerService.hpp` lines 89-93 with exact sample rates/channels; confirmed by `testApplyPresetChangesOnlyTargetStream` test |
| 3 | Applying a preset to one stream does not affect the other streams | VERIFIED | Isolated per `streamAt()` dispatch; `test_equalizer_service` passes with stream-independence assertions |
| 4 | User presets can be saved with auto-generated names, loaded, renamed, and deleted | VERIFIED | `saveUserPreset`, `renameUserPreset`, `deleteUserPreset` all implemented in `EqualizerService.cpp`; 19 tests pass covering all CRUD paths |
| 5 | Deleting a preset assigned to a stream reverts that stream to Flat | VERIFIED | `deleteUserPreset` lines 153-158 in `EqualizerService.cpp` — loops all 3 streams, calls `applyPreset(sid, "Flat")` if active preset matches |
| 6 | Bundled presets are immutable — cannot be overwritten or deleted | VERIFIED | `isBundledName()` guard in `saveUserPreset` (returns `{}`) and `renameUserPreset` (returns false); no delete path for bundled presets |
| 7 | User preset names cannot collide with bundled preset names | VERIFIED | `isBundledName()` checked in both `saveUserPreset` and `renameUserPreset` before accepting new name |

**Plan 02 Truths (CFG-01, PRST-01, PRST-02):**

| # | Truth | Status | Evidence |
|---|-------|--------|---------|
| 8 | EQ settings persist across app restart via YAML config | VERIFIED | `loadFromConfig()` in constructor reads `eqStreamPreset` + `eqUserPresets`; `scheduleSave()`/`writeToConfig()` persist on every mutation; `saveNow()` on `aboutToQuit` |
| 9 | Per-stream preset assignments are stored and restored from config | VERIFIED | `YamlConfig::eqStreamPreset/setEqStreamPreset` write to `audio.equalizer.streams.<name>.preset`; `loadFromConfig()` reads and applies on startup; round-trip verified by 5 new `test_yaml_config` tests |
| 10 | User presets (name + 10 gains) are stored and restored from config | VERIFIED | `YamlConfig::eqUserPresets/setEqUserPresets` serialize full gain array; `loadFromConfig()` repopulates `userPresets_` before applying stream presets |
| 11 | Missing or deleted preset names in config fall back to Flat on load | VERIFIED | `applyPreset()` fallback logic — if `findPresetGains()` returns nullptr, falls back to "Flat" (lines 50-56 `EqualizerService.cpp`) |
| 12 | EQ processing runs in the PipeWire playback callback on all 3 AA streams | VERIFIED | `AudioService.cpp` lines 154-160: `handle->eqEngine->process()` called in `onPlaybackProcess` after ring buffer read; `AndroidAutoOrchestrator.cpp` lines 302-310 assign `engineForStream()` to all 3 stream handles on session start |

**Score: 12/12 truths verified**

### Required Artifacts

| Artifact | Min Lines | Actual Lines | Status | Details |
|----------|-----------|--------------|--------|---------|
| `src/core/audio/EqualizerPresets.hpp` | — | 51 | VERIFIED | 9 bundled presets, `kBundledPresetCount`, `findBundledPreset()`, default index constants |
| `src/core/services/IEqualizerService.hpp` | — | 69 | VERIFIED | Pure virtual interface with `StreamId` enum, all 11 virtual methods |
| `src/core/services/EqualizerService.hpp` | — | 103 | VERIFIED | QObject + IEqualizerService, 3 Q_PROPERTY, Q_INVOKABLE on all interface methods, `engineForStream()`, `saveNow()` |
| `src/core/services/EqualizerService.cpp` | 100 | 325 | VERIFIED | Full implementation: preset CRUD, config persistence, debounce save, stream independence |
| `tests/test_equalizer_service.cpp` | 50 | 320 | VERIFIED | 26 test cases covering CRUD, stream independence, config round-trip; all pass |
| `tests/test_equalizer_presets.cpp` | 20 | 90 | VERIFIED | 8 test cases validating preset count, gains, ranges, Flat = zeros; all pass |
| `src/core/YamlConfig.hpp` | — | — | VERIFIED | `EqUserPreset` struct, `eqStreamPreset`, `setEqStreamPreset`, `eqUserPresets`, `setEqUserPresets` present |
| `src/core/services/AudioService.hpp` | — | — | VERIFIED | `EqualizerEngine* eqEngine = nullptr` in `AudioStreamHandle` (line 51) |
| `src/core/plugin/IHostContext.hpp` | — | — | VERIFIED | `virtual IEqualizerService* equalizerService() = 0` (line 33) |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `EqualizerService.cpp` | `EqualizerEngine.hpp` | 3 `EqualizerEngine` member instances | WIRED | `streams_[3]` array in header, `engine` member of each `StreamState`; `setAllGains`, `setGain`, `setBypassed`, `process` all called |
| `EqualizerService.cpp` | `EqualizerPresets.hpp` | `kBundledPresets` lookup | WIRED | `findBundledPreset`, `isBundledName`, `bundledPresetNames` all reference `kBundledPresets` directly |
| `EqualizerService.cpp` | `YamlConfig.hpp` | `loadFromConfig`/`scheduleSave` | WIRED | `eqStreamPreset`, `setEqStreamPreset`, `eqUserPresets`, `setEqUserPresets` all called; pattern `eqStreamPreset|eqUserPresets` confirmed present |
| `AudioService.cpp` | `EqualizerEngine.hpp` | `eqEngine->process()` in `onPlaybackProcess` | WIRED | Lines 154-160 in `AudioService.cpp` — `handle->eqEngine->process()` called after ring buffer read, before silence fill |
| `AndroidAutoOrchestrator.cpp` | `EqualizerService.hpp` | `engineForStream()` assignment | WIRED | Lines 302-310 — `eqService_->engineForStream(StreamId::Media/Navigation/Phone)` assigned to `mediaStream_`, `speechStream_`, `systemStream_` `eqEngine` pointers |
| `main.cpp` | `EqualizerService` | Create, register in HostContext + QML | WIRED | `new oap::EqualizerService(yamlConfig.get(), &app)`, `hostContext->setEqualizerService(eqService)`, `setContextProperty("EqualizerService", eqService)`, `aboutToQuit` → `saveNow()` |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|---------|
| PRST-01 | 02-01, 02-02 | User can select from 8 bundled presets (Flat, Rock, Pop, Jazz, Classical, Bass Boost, Treble Boost, Vocal) | SATISFIED | 9 presets implemented (8 required + Voice for voice streams); `bundledPresetNames()` exposes all; Q_INVOKABLE for QML |
| PRST-02 | 02-01, 02-02 | User can apply different EQ profiles to media, navigation, and phone audio streams independently | SATISFIED | `applyPreset(StreamId, name)` applies to one stream; 3 separate `StreamState` objects; per-stream independence verified by tests |
| PRST-03 | 02-01 | User can save custom EQ settings as a named preset | SATISFIED | `saveUserPreset(StreamId, name)` captures gains from specified stream; auto-name generation; bundled collision guard |
| PRST-04 | 02-01 | User can load and delete user-created presets | SATISFIED | `applyPreset` works with user presets; `deleteUserPreset` with active-stream revert; `renameUserPreset` implemented |
| CFG-01 | 02-02 | User's EQ settings and presets persist across app restarts via YAML config | SATISFIED | Full persistence pipeline: `loadFromConfig()` on construction, `scheduleSave()` on mutations, `saveNow()` on shutdown; YAML round-trip verified by tests |

**Note on PRST-01 discrepancy:** REQUIREMENTS.md says "8 bundled presets" listing Flat through Vocal. The plan added "Voice" (9th preset) for navigation/phone stream defaults. This is a superset of the requirement — the 8 required presets are all present. No gap.

**Orphaned requirements check:** No additional requirement IDs mapped to Phase 2 in REQUIREMENTS.md beyond those declared in the plans.

### Anti-Patterns Found

No anti-patterns detected in phase files. All `return {}` and `return nullptr` occurrences are legitimate early-return guards (bundled name rejection, find-with-no-result), not implementation stubs. No TODO/FIXME/PLACEHOLDER comments. No console-only implementations.

### Human Verification Required

One item cannot be verified programmatically:

**1. Real-time EQ audibility on AA streams**

**Test:** Connect a phone via Android Auto wireless. Play music. Open the EQ settings (Phase 3 will expose this UI, but the service can be tested now via QML console or by hardcoding a preset in main.cpp temporarily). Switch between Flat and Bass Boost on the media stream.
**Expected:** Audible difference in bass frequencies when switching presets.
**Why human:** Verifying RT audio processing output requires actual PipeWire + AA session on the Pi. Cannot test with the VM (no PipeWire hardware, no phone connected).

**2. Persistence survives reboot**

**Test:** Apply a non-default preset (e.g., Rock for Media). Kill and restart the app. Confirm the preset shown in UI is still Rock.
**Expected:** Settings survive restart via `~/.openauto/config.yaml`.
**Why human:** Requires Pi deployment and interactive restart verification.

### Gaps Summary

No gaps. All 12 must-have truths are verified. The full implementation chain is present and wired:

- Preset data exists as constexpr (EqualizerPresets.hpp)
- Service layer manages 3 engines with CRUD preset operations (EqualizerService.cpp, 325 lines)
- Config persistence round-trips in both directions (YamlConfig EQ methods, loadFromConfig, writeToConfig)
- PipeWire RT callback calls `eqEngine->process()` on every audio chunk
- Orchestrator assigns engine pointers to stream handles on session start
- HostContext exposes the service to plugins
- QML context exposes the service to Phase 3 UI
- Shutdown flush prevents data loss

57/58 tests pass. The lone failure (`test_event_bus`) pre-dates Phase 02 (present in commit `9819121` at end of Phase 01) and is unrelated to EQ.

---

_Verified: 2026-03-02T01:30:00Z_
_Verifier: Claude (gsd-verifier)_
