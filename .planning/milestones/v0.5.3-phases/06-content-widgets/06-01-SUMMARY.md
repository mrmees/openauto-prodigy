---
phase: 06-content-widgets
plan: 01
status: complete
---

# Plan 06-01 Summary: Navigation Data Bridge

## What was built
- **NavigationDataBridge** (`src/core/aa/NavigationDataBridge.hpp/.cpp`): QObject bridge accumulating `NavigationChannelHandler` signals into QML-bindable Q_PROPERTYs (navActive, roadName, maneuverType, turnDirection, formattedDistance, distanceMeters, hasManeuverIcon, iconVersion)
- **ManeuverIconProvider** (`src/core/aa/ManeuverIconProvider.hpp`): Header-only QQuickImageProvider serving phone PNG turn icons to QML with thread-safe QMutex storage and aspect-ratio scaling
- **Orchestrator accessors**: Added `navigationHandler()` and `mediaStatusHandler()` public methods to AndroidAutoOrchestrator

## Key decisions
- **Distance formatting**: Handles all 5 AA distance units (meters, km, miles, feet, yards) with auto-km threshold at 1000m for meters
- **Single turnDataChanged signal**: All turn-related properties share one NOTIFY signal to minimize emissions
- **Header-only ManeuverIconProvider**: Simple enough to keep in-header, avoids needing a .cpp for MOC since it doesn't use Q_OBJECT
- **iconVersion counter**: Increments on each icon update for QML cache-busting via `"image://maneuver/current?" + iconVersion`

## Test coverage
- 17 tests in test_navigation_data_bridge: defaults, nav active toggle, turn event updates, icon versioning, nav deactivate clears data, icon provider wiring, all 5 distance unit formats
- 6 tests in test_maneuver_icon_provider: no-data returns null, valid PNG loads, scaling, update replaces old

## Artifacts
| File | Lines | Purpose |
|------|-------|---------|
| src/core/aa/NavigationDataBridge.hpp | 63 | Q_PROPERTY bridge header |
| src/core/aa/NavigationDataBridge.cpp | 97 | Implementation with distance formatting |
| src/core/aa/ManeuverIconProvider.hpp | 61 | QQuickImageProvider (header-only) |
| tests/test_navigation_data_bridge.cpp | 207 | Nav bridge unit tests |
| tests/test_maneuver_icon_provider.cpp | 91 | Icon provider unit tests |
