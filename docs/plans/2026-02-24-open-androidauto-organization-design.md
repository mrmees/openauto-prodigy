# open-androidauto Qt-First Organization Design

## Goal
Organize `open-androidauto` so hobbyists can quickly build their own Android Auto head unit apps on varied hardware, without changing current runtime behavior.

## Constraints
- Keep Qt in core API for now (Option 1).
- Keep a single CMake target (`open-androidauto`) for hobby-level simplicity.
- No behavior changes in this phase.
- No performance regressions on low-end hardware.

## Non-Goals
- No Qt removal / callback redesign.
- No AV pipeline rewrites.
- No new dependencies.
- No protocol behavior changes.

## Current Problem
AA protocol helpers needed by custom apps are split between:
- reusable library (`libs/open-androidauto`), and
- app-specific tree (`src/core/aa/*`).

This increases discovery cost for hobbyists and obscures what is reusable.

## Design Summary
Create a clear HU-facing organization inside `open-androidauto` while keeping one library target:

- Keep existing protocol internals:
  - `oaa/Transport/*`
  - `oaa/Messenger/*`
  - `oaa/Channel/*`
  - `oaa/Session/*`
- Add HU-oriented public modules:
  - `include/oaa/HU/Handlers/*`
  - `src/HU/Handlers/*`
  - `include/oaa/HU/Discovery/*`
  - `src/HU/Discovery/*`

## Move Candidates (No Behavior Change)
Move from app core into library:
- `src/core/aa/handlers/AudioChannelHandler.*`
- `src/core/aa/handlers/VideoChannelHandler.*`
- `src/core/aa/handlers/AVInputChannelHandler.*`
- `src/core/aa/handlers/InputChannelHandler.*`
- `src/core/aa/handlers/SensorChannelHandler.*`
- `src/core/aa/handlers/BluetoothChannelHandler.*`
- `src/core/aa/handlers/WiFiChannelHandler.*`
- `src/core/aa/ServiceDiscoveryBuilder.*`

Keep app-side (platform/policy specific):
- `src/core/aa/AndroidAutoOrchestrator.*`
- `src/core/aa/BluetoothDiscoveryService.*`
- `src/core/aa/TouchHandler.*`
- `src/core/aa/EvdevTouchReader.*`
- `src/core/aa/VideoDecoder.*`
- `src/core/aa/*NightMode*`

## API Surface (Qt-first)
Target namespaces in library:
- `oaa::hu::handlers::*`
- `oaa::hu::discovery::ServiceDiscoveryBuilder`

To keep migration low-risk, provide temporary compatibility wrappers/aliases in app namespace where needed during transition.

## Performance Guardrails
For this organization pass:
- No new copies in AV media paths.
- No added hot-path locks.
- No thread model changes.
- Keep ACK/focus/message handling behavior unchanged.
- Preserve one static library target for simple and fast builds on weak hardware.

## Migration Map
- `src/core/aa/handlers/*.hpp` -> `libs/open-androidauto/include/oaa/HU/Handlers/*.hpp`
- `src/core/aa/handlers/*.cpp` -> `libs/open-androidauto/src/HU/Handlers/*.cpp`
- `src/core/aa/ServiceDiscoveryBuilder.hpp` -> `libs/open-androidauto/include/oaa/HU/Discovery/ServiceDiscoveryBuilder.hpp`
- `src/core/aa/ServiceDiscoveryBuilder.cpp` -> `libs/open-androidauto/src/HU/Discovery/ServiceDiscoveryBuilder.cpp`

## CMake Impact
- Keep single target: `open-androidauto`.
- Update library `CMakeLists.txt` to include moved HU source/header files.
- Update app include paths and source references to library headers.

## Risks and Mitigations
- Risk: include path churn.
  - Mitigation: add transitional wrapper headers and migrate includes incrementally.
- Risk: accidental behavior drift in moved handlers.
  - Mitigation: preserve code verbatim initially; rely on existing tests and AA integration tests.
- Risk: performance regressions due to incidental refactor changes.
  - Mitigation: no algorithmic changes in this phase.

## Success Criteria
- `open-androidauto` exports protocol engine + HU helper modules in one place.
- App builds/runs with unchanged AA behavior.
- Existing handler/service discovery tests continue passing.
- Hobbyist can find reusable AA building blocks under `oaa/HU/*` without scanning app internals.

## Next Step
Write implementation plan with file-by-file move steps, compatibility shim plan, and verification checklist.
