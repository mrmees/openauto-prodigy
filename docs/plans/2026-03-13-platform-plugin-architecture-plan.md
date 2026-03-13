# Platform / Plugin Architecture Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Refactor Prodigy into a platform runtime with a first-class dashboard and capability-based plugins, while keeping Android Auto the priority feature and avoiding a full rewrite.

**Architecture:** Keep the existing plugin runtime, dashboard grid, and core service layer, but formalize the seams between core platform, shell, dashboard, and feature plugins. This revision intentionally keeps the type distinction between lightweight widgets and future live-surface widgets, but defers speculative hosting/runtime infrastructure until a concrete live-surface use case exists. It also replaces monolithic wrapper ideas with narrow provider interfaces and breaks `main.cpp` cleanup into smaller extractions instead of a single `AppRuntime` jump.

**Tech Stack:** Qt 6 / QML, C++17, CMake, QTest, yaml-cpp, D-Bus/BlueZ, PipeWire

---

> **Note:** This plan supersedes `docs/plans/active/2026-02-21-architecture-extensibility-plan.md` where the two conflict. Use the 2026-03-13 design doc and this plan as the source of truth for the refactor.

> **Scope adjustment:** This plan keeps `contributionKind` in the model now, but defers `WidgetSurfaceHost` and any full live-surface runtime path until a concrete embedded AA pane or equivalent plugin requirement exists.

### Task 1: Extend the existing widget registry with typed dashboard contribution metadata

**Files:**
- Modify: `src/core/widget/WidgetTypes.hpp`
- Modify: `src/core/widget/WidgetRegistry.hpp`
- Modify: `src/core/widget/WidgetRegistry.cpp`
- Modify: `src/core/plugin/IPlugin.hpp`
- Modify: `src/ui/WidgetPickerModel.hpp`
- Modify: `src/ui/WidgetPickerModel.cpp`
- Modify: `tests/test_widget_types.cpp`
- Modify: `tests/test_widget_registry.cpp`
- Modify: `tests/test_widget_plugin_integration.cpp`

**Step 1: Write the failing tests**

Extend the existing widget tests to assert:

- `WidgetDescriptor` carries a stable `DashboardContributionKind`
- contribution metadata defaults to a lightweight dashboard widget
- `WidgetRegistry` remains the single source of truth and can filter by contribution kind
- plugin-provided widget descriptors preserve their contribution kind and source plugin ID

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_widget_types|test_widget_registry|test_widget_plugin_integration" --output-on-failure
```

Expected: compile failure because `DashboardContributionKind` and the new registry/filter fields do not exist.

**Step 3: Write the minimal implementation**

Update `src/core/widget/WidgetTypes.hpp` so `WidgetDescriptor` includes a typed contribution kind, for example:

```cpp
enum class DashboardContributionKind {
    Widget,
    LiveSurfaceWidget,
};

struct WidgetDescriptor {
    QString id;
    QString displayName;
    QString iconName;
    QUrl qmlComponent;
    QString pluginId;
    DashboardContributionKind contributionKind = DashboardContributionKind::Widget;
    QVariantMap defaultConfig;
    int minCols = 1;
    int minRows = 1;
    int maxCols = 6;
    int maxRows = 4;
    int defaultCols = 1;
    int defaultRows = 1;
};
```

Extend `WidgetRegistry` instead of creating a second contribution registry. Keep `IPlugin::widgetDescriptors()` as the plugin-facing declaration point.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/widget/WidgetTypes.hpp src/core/widget/WidgetRegistry.hpp src/core/widget/WidgetRegistry.cpp src/core/plugin/IPlugin.hpp src/ui/WidgetPickerModel.hpp src/ui/WidgetPickerModel.cpp tests/test_widget_types.cpp tests/test_widget_registry.cpp tests/test_widget_plugin_integration.cpp
git commit -m "feat: add typed dashboard contribution metadata"
```

### Task 2: Add narrow provider interfaces to the host context for shell and dashboard state

**Files:**
- Create: `src/core/services/IProjectionStatusProvider.hpp`
- Create: `src/core/services/INavigationProvider.hpp`
- Create: `src/core/services/IMediaStatusProvider.hpp`
- Create: `src/core/services/ICallStateProvider.hpp`
- Create: `src/core/services/ProjectionStatusProvider.hpp`
- Create: `src/core/services/ProjectionStatusProvider.cpp`
- Modify: `src/core/plugin/IHostContext.hpp`
- Modify: `src/core/plugin/HostContext.hpp`
- Modify: `src/core/plugin/HostContext.cpp`
- Modify: `src/core/aa/NavigationDataBridge.hpp`
- Modify: `src/core/aa/NavigationDataBridge.cpp`
- Modify: `src/core/aa/MediaDataBridge.hpp`
- Modify: `src/core/aa/MediaDataBridge.cpp`
- Modify: `src/plugins/phone/PhonePlugin.hpp`
- Modify: `src/plugins/phone/PhonePlugin.cpp`
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_projection_status_provider.cpp`
- Modify: `tests/test_navigation_data_bridge.cpp`
- Modify: `tests/test_media_data_bridge.cpp`

**Step 1: Write the failing tests**

Create `tests/test_projection_status_provider.cpp` to assert:

- a narrow provider can expose projection connection state without leaking the full `AndroidAutoOrchestrator`
- `HostContext` can store and return a projection status provider

Extend the existing navigation/media bridge tests to assert:

- `NavigationDataBridge` satisfies `INavigationProvider`
- `MediaDataBridge` satisfies `IMediaStatusProvider`

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_projection_status_provider|test_navigation_data_bridge|test_media_data_bridge" --output-on-failure
```

Expected: compile failure because the provider interfaces and host context accessors do not exist.

**Step 3: Write the minimal implementation**

Add narrow provider interfaces so shell/dashboard code consumes queryable state rather than root-context globals or AA-shaped classes:

- `IProjectionStatusProvider`
- `INavigationProvider`
- `IMediaStatusProvider`
- `ICallStateProvider`

Make `NavigationDataBridge` and `MediaDataBridge` satisfy the provider interfaces directly. Add a small `ProjectionStatusProvider` wrapper around `AndroidAutoOrchestrator`. Temporarily let `PhonePlugin` satisfy `ICallStateProvider` until later tasks move ownership into core services.

Add these providers to `IHostContext` / `HostContext`.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/services/IProjectionStatusProvider.hpp src/core/services/INavigationProvider.hpp src/core/services/IMediaStatusProvider.hpp src/core/services/ICallStateProvider.hpp src/core/services/ProjectionStatusProvider.hpp src/core/services/ProjectionStatusProvider.cpp src/core/plugin/IHostContext.hpp src/core/plugin/HostContext.hpp src/core/plugin/HostContext.cpp src/core/aa/NavigationDataBridge.hpp src/core/aa/NavigationDataBridge.cpp src/core/aa/MediaDataBridge.hpp src/core/aa/MediaDataBridge.cpp src/plugins/phone/PhonePlugin.hpp src/plugins/phone/PhonePlugin.cpp src/main.cpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_projection_status_provider.cpp tests/test_navigation_data_bridge.cpp tests/test_media_data_bridge.cpp
git commit -m "feat: add host provider interfaces for shell and dashboard state"
```

### Task 3: Remove shell and widget root-context globals by routing through provider interfaces

**Files:**
- Modify: `src/ui/WidgetInstanceContext.hpp`
- Modify: `src/ui/WidgetInstanceContext.cpp`
- Modify: `src/main.cpp`
- Modify: `qml/widgets/AAStatusWidget.qml`
- Modify: `qml/widgets/NavigationWidget.qml`
- Modify: `qml/widgets/NowPlayingWidget.qml`
- Modify: `qml/applications/phone/IncomingCallOverlay.qml`
- Modify: `qml/components/Shell.qml`
- Modify: `qml/applications/settings/DebugSettings.qml`
- Modify: `tests/test_widget_instance_context.cpp`
- Modify: `tests/test_action_registry.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing tests**

Extend tests to assert:

- `WidgetInstanceContext` exposes provider-backed access to host services used by dashboard widgets
- shell overlays no longer require `PhonePlugin` as a root-context property
- debug AA button actions do not require `AAOrchestrator` as a root-context property

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_widget_instance_context|test_action_registry|test_settings_menu_structure" --output-on-failure
```

Expected: failing tests because widgets, overlays, and debug controls still depend on root-context globals.

**Step 3: Write the minimal implementation**

Update widget QML and shell overlay QML to resolve state through `WidgetInstanceContext` and host providers instead of:

- `AAOrchestrator`
- `NavigationBridge`
- `MediaBridge`
- `PhonePlugin`

Route debug AA button presses through `ActionRegistry` rather than direct `AAOrchestrator.sendButtonPress(...)` calls from QML.

After this task, remove those four root-context properties from `main.cpp`.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/ui/WidgetInstanceContext.hpp src/ui/WidgetInstanceContext.cpp src/main.cpp qml/widgets/AAStatusWidget.qml qml/widgets/NavigationWidget.qml qml/widgets/NowPlayingWidget.qml qml/applications/phone/IncomingCallOverlay.qml qml/components/Shell.qml qml/applications/settings/DebugSettings.qml tests/test_widget_instance_context.cpp tests/test_action_registry.cpp tests/test_settings_menu_structure.cpp
git commit -m "refactor: remove AA and phone root-context globals"
```

### Task 4: Promote phone and media monitoring into core-owned services while preserving plugin UI wrappers

**Files:**
- Create: `src/core/services/IPhoneStateService.hpp`
- Create: `src/core/services/PhoneStateService.hpp`
- Create: `src/core/services/PhoneStateService.cpp`
- Create: `src/core/services/IMediaStatusService.hpp`
- Create: `src/core/services/MediaStatusService.hpp`
- Create: `src/core/services/MediaStatusService.cpp`
- Modify: `src/core/plugin/IHostContext.hpp`
- Modify: `src/core/plugin/HostContext.hpp`
- Modify: `src/core/plugin/HostContext.cpp`
- Modify: `src/plugins/phone/PhonePlugin.hpp`
- Modify: `src/plugins/phone/PhonePlugin.cpp`
- Modify: `src/plugins/bt_audio/BtAudioPlugin.hpp`
- Modify: `src/plugins/bt_audio/BtAudioPlugin.cpp`
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_phone_state_service.cpp`
- Create: `tests/test_media_status_service.cpp`

**Step 1: Write the failing tests**

Create tests to assert:

- phone/call monitoring survives regardless of whether the phone UI plugin is active
- media monitoring survives regardless of whether the BT audio UI plugin is active
- shell and dashboard providers no longer depend on plugin lifetime for state ownership

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_phone_state_service|test_media_status_service" --output-on-failure
```

Expected: compile failure because the core phone/media services do not exist.

**Step 3: Write the minimal implementation**

Move the actual D-Bus monitoring/state ownership into core services:

- `PhoneStateService`
- `MediaStatusService`

Update `PhonePlugin` and `BtAudioPlugin` to become UI-facing wrappers that consume those services for:

- QML-facing properties
- activation lifecycle
- nav-strip presence
- optional standalone views

Rebind the `ICallStateProvider` / `IMediaStatusProvider` host interfaces to the new core services so the provider split introduced in Task 2 becomes the permanent architecture rather than a transitional plugin-backed shim.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/services/IPhoneStateService.hpp src/core/services/PhoneStateService.hpp src/core/services/PhoneStateService.cpp src/core/services/IMediaStatusService.hpp src/core/services/MediaStatusService.hpp src/core/services/MediaStatusService.cpp src/core/plugin/IHostContext.hpp src/core/plugin/HostContext.hpp src/core/plugin/HostContext.cpp src/plugins/phone/PhonePlugin.hpp src/plugins/phone/PhonePlugin.cpp src/plugins/bt_audio/BtAudioPlugin.hpp src/plugins/bt_audio/BtAudioPlugin.cpp src/main.cpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_phone_state_service.cpp tests/test_media_status_service.cpp
git commit -m "refactor: move phone and media monitoring into core services"
```

### Task 5: Remove legacy `Configuration` usage from Android Auto and unify config access

**Files:**
- Create: `src/core/aa/AndroidAutoConfigSnapshot.hpp`
- Create: `src/core/aa/AndroidAutoConfigSnapshot.cpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_aa_orchestrator.cpp`

**Step 1: Write the failing tests**

Extend `tests/test_aa_orchestrator.cpp` to assert:

- AA runtime configuration can be derived from the active config service without `Configuration`
- config-driven AA settings still behave correctly after removing the legacy constructor dependency

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_aa_orchestrator" --output-on-failure
```

Expected: failing tests because `AndroidAutoPlugin` / `AndroidAutoOrchestrator` still depend on legacy `Configuration`.

**Step 3: Write the minimal implementation**

Add a small `AndroidAutoConfigSnapshot` or equivalent helper that reads the needed AA runtime settings from `ConfigService` / YAML-backed configuration. Remove `std::shared_ptr<Configuration>` from `AndroidAutoPlugin` construction and stop using it as a second config channel.

After this task, `AndroidAutoPlugin` should no longer be constructed with the legacy config object in `main.cpp`.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/aa/AndroidAutoConfigSnapshot.hpp src/core/aa/AndroidAutoConfigSnapshot.cpp src/core/aa/AndroidAutoOrchestrator.hpp src/core/aa/AndroidAutoOrchestrator.cpp src/plugins/android_auto/AndroidAutoPlugin.hpp src/plugins/android_auto/AndroidAutoPlugin.cpp src/main.cpp tests/test_aa_orchestrator.cpp
git commit -m "refactor: remove legacy configuration from AA runtime"
```

### Task 6: Separate AA platform wiring from plugin behavior and shrink `main.cpp` incrementally

**Files:**
- Create: `src/core/aa/AndroidAutoRuntimeBridge.hpp`
- Create: `src/core/aa/AndroidAutoRuntimeBridge.cpp`
- Create: `src/ui/GestureOverlayController.hpp`
- Create: `src/ui/GestureOverlayController.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/ui/ApplicationController.hpp`
- Modify: `src/ui/ApplicationController.cpp`
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_android_auto_runtime_bridge.cpp`
- Create: `tests/test_gesture_overlay_controller.cpp`
- Modify: `tests/test_plugin_model.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing tests**

Create tests to assert:

- navbar/display/touch wiring can be configured without making `AndroidAutoPlugin` the owner of shell policy
- gesture overlay zone registration can be exercised without keeping the whole lambda in `main.cpp`
- settings/home navigation still work while reducing dependence on legacy enum-style app switching

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_android_auto_runtime_bridge|test_gesture_overlay_controller|test_plugin_model|test_settings_menu_structure" --output-on-failure
```

Expected: compile failure because the runtime bridge and gesture overlay controller do not exist.

**Step 3: Write the minimal implementation**

Create:

- `AndroidAutoRuntimeBridge` to own platform-adjacent AA display/touch/navbar wiring
- `GestureOverlayController` to own the three-finger overlay zone registration and teardown currently embedded in `main.cpp`

Refactor `main.cpp` by extracting these specific concerns first. Do **not** introduce a top-level `AppRuntime` class in this task. Keep `main.cpp` as the assembler, but make it substantially smaller and less policy-heavy.

Update `ApplicationController` only as far as needed to keep built-in shell-owned screens reachable without reinforcing the old enum-driven architecture.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/aa/AndroidAutoRuntimeBridge.hpp src/core/aa/AndroidAutoRuntimeBridge.cpp src/ui/GestureOverlayController.hpp src/ui/GestureOverlayController.cpp src/plugins/android_auto/AndroidAutoPlugin.hpp src/plugins/android_auto/AndroidAutoPlugin.cpp src/ui/ApplicationController.hpp src/ui/ApplicationController.cpp src/main.cpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_android_auto_runtime_bridge.cpp tests/test_gesture_overlay_controller.cpp tests/test_plugin_model.cpp tests/test_settings_menu_structure.cpp
git commit -m "refactor: extract AA runtime wiring from main bootstrap"
```

### Task 7: Update architecture docs, verify behavior, and record handoff

**Files:**
- Modify: `docs/plugin-api.md`
- Modify: `docs/settings-tree.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/session-handoffs.md`

**Step 1: Update documentation**

Document:

- the typed dashboard contribution model in `WidgetRegistry`
- the rule that core owns singleton hardware/system policy
- the fact that `LiveSurfaceWidget` remains a declared contribution type but its runtime host is intentionally deferred
- the provider-interface approach for shell/dashboard data
- the reduced role of standalone phone/media UI in the current product focus

**Step 2: Run full verification**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && ctest --output-on-failure
cd /home/matt/claude/personal/openautopro/openauto-prodigy && ./cross-build.sh
```

Expected:

- local build succeeds
- full test suite passes
- cross-build completes successfully

If the refactor touches Pi deployment behavior, deploy and verify on hardware before closing the milestone.

**Step 3: Record the session handoff**

Append to `docs/session-handoffs.md`:

- what changed
- why
- status
- next 1-3 steps
- verification commands/results

**Step 4: Commit**

```bash
git add docs/plugin-api.md docs/settings-tree.md docs/roadmap-current.md docs/session-handoffs.md
git commit -m "docs: record platform-plugin architecture refactor"
```
