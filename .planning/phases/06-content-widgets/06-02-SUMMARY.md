---
phase: 06-content-widgets
plan: 02
status: complete
---

# Plan 06-02 Summary: Media Data Bridge

## What was built
- **MediaDataBridge** (`src/core/aa/MediaDataBridge.hpp/.cpp`): Unified QObject bridge merging AA and BT A2DP metadata with deterministic source priority (AA > BT > None)

## Key decisions
- **Source enum**: None=0, Bluetooth=1, AndroidAuto=2 — Q_ENUM for QML access
- **Separate caches**: aaTitle_/btTitle_ etc. stored independently so source switching reads correct cache without stale data flash
- **Instant source switch**: AA connect immediately sets source=AA (even before metadata arrives), AA disconnect falls back to BT (if connected) with immediate readBtState()
- **BT code #ifdef HAS_BLUETOOTH guarded**: Compiles cleanly on dev VM without qt6-connectivity-dev
- **albumArt stored but not exposed**: QByteArray stored from AA metadata, reserved for future album art widget polish
- **Control delegation keycodes**: playPause=85, next=87, previous=88 (AA InputEventIndication button keycodes)

## Test coverage
- 13 tests in test_media_data_bridge: default state, source enum, null safety, controls when None, AA connect/disconnect source switching, AA metadata/playback updates, appName updates, hasMedia reflects content
- BT-specific interaction tests are #ifdef HAS_BLUETOOTH guarded (only run on Pi with full BT support)

## Artifacts
| File | Lines | Purpose |
|------|-------|---------|
| src/core/aa/MediaDataBridge.hpp | 104 | Unified media bridge header |
| src/core/aa/MediaDataBridge.cpp | 284 | Implementation with source priority |
| tests/test_media_data_bridge.cpp | ~180 | Media bridge unit tests |
