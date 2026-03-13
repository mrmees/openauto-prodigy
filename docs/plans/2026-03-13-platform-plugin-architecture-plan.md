# Platform / Plugin Architecture Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Refactor Prodigy into a platform runtime with a first-class dashboard and capability-based plugins, while keeping Android Auto the priority feature and avoiding a full rewrite.

**Architecture:** Keep the existing plugin runtime, dashboard grid, and core service layer, but formalize the seams between core platform, shell, dashboard, and feature plugins. The highest-risk boundary leaks are `main.cpp` bootstrap wiring, root-context AA/phone globals, and feature plugins that currently own platform-ish responsibilities; this plan removes those incrementally behind tested interfaces.

**Tech Stack:** Qt 6 / QML, C++17, CMake, QTest, yaml-cpp, D-Bus/BlueZ, PipeWire

---

> **Note:** This plan supersedes `docs/plans/active/2026-02-21-architecture-extensibility-plan.md` where the two conflict. Use the 2026-03-13 design doc and this plan as the source of truth for the refactor.

### Task 1: Define typed dashboard/plugin contribution metadata

**Files:**
- Create: `src/core/dashboard/ContributionTypes.hpp`
- Modify: `src/core/plugin/IPlugin.hpp`
- Modify: `src/core/widget/WidgetTypes.hpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_dashboard_contribution_types.cpp`
- Modify: `tests/test_widget_types.cpp`
- Modify: `tests/test_widget_plugin_integration.cpp`

**Step 1: Write the failing tests**

Create `tests/test_dashboard_contribution_types.cpp` to assert:

- `DashboardContributionKind` defaults distinguish `Widget` and `LiveSurfaceWidget`
- `DashboardSurfaceConstraints` default to safe shell-owned behavior
- plugin contributions can be declared without requiring a foreground view or settings page

Extend `tests/test_widget_types.cpp` to assert `WidgetDescriptor` can carry a stable contribution kind and optional live-surface constraints.

Extend `tests/test_widget_plugin_integration.cpp` so the mock plugin can declare both a lightweight widget contribution and a live-surface contribution.

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_dashboard_contribution_types|test_widget_types|test_widget_plugin_integration" --output-on-failure
```

Expected: compile failure because the new dashboard contribution types and fields do not exist yet.

**Step 3: Write the minimal implementation**

Add `src/core/dashboard/ContributionTypes.hpp` with the new dashboard-facing metadata, for example:

```cpp
enum class DashboardContributionKind {
    Widget,
    LiveSurfaceWidget,
};

struct DashboardSurfaceConstraints {
    QSize minimumCells{1, 1};
    QSize defaultCells{1, 1};
    QSize maximumCells{6, 4};
    bool consumeTouch = false;
    bool suspendWhenHidden = true;
};
```

Update `src/core/widget/WidgetTypes.hpp` so `WidgetDescriptor` includes the contribution kind and optional surface constraints. Update `src/core/plugin/IPlugin.hpp` so plugins can continue to return widget descriptors, but those descriptors now describe both lightweight and live-surface dashboard contributions.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/dashboard/ContributionTypes.hpp src/core/plugin/IPlugin.hpp src/core/widget/WidgetTypes.hpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_dashboard_contribution_types.cpp tests/test_widget_types.cpp tests/test_widget_plugin_integration.cpp
git commit -m "feat: add typed dashboard contribution metadata"
```

### Task 2: Add a dashboard contribution registry and register plugin contributions explicitly

**Files:**
- Create: `src/core/dashboard/DashboardContributionRegistry.hpp`
- Create: `src/core/dashboard/DashboardContributionRegistry.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `src/main.cpp`
- Modify: `src/ui/WidgetPickerModel.hpp`
- Modify: `src/ui/WidgetPickerModel.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_dashboard_contribution_registry.cpp`
- Modify: `tests/test_widget_plugin_integration.cpp`

**Step 1: Write the failing tests**

Create `tests/test_dashboard_contribution_registry.cpp` to assert:

- lightweight and live-surface contributions can both be registered
- duplicate IDs are rejected
- picker-visible contributions can be filtered by grid size and contribution kind
- contributions remain attributable to a source plugin ID

Add a test case to `tests/test_widget_plugin_integration.cpp` that verifies plugin-declared widget descriptors are promoted into dashboard contributions through the registry.

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_dashboard_contribution_registry|test_widget_plugin_integration" --output-on-failure
```

Expected: compile failure because the registry does not exist.

**Step 3: Write the minimal implementation**

Add `DashboardContributionRegistry` as the shell-owned source of truth for dashboard-eligible plugin contributions. Keep `WidgetRegistry` for existing widget descriptors during the transition, but make `main.cpp` register plugin contributions through the new registry after plugin initialization.

Update `WidgetPickerModel` to read from the contribution registry and filter out contributions that do not have a loadable dashboard tile.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/dashboard/DashboardContributionRegistry.hpp src/core/dashboard/DashboardContributionRegistry.cpp src/CMakeLists.txt src/main.cpp src/ui/WidgetPickerModel.hpp src/ui/WidgetPickerModel.cpp tests/CMakeLists.txt tests/test_dashboard_contribution_registry.cpp tests/test_widget_plugin_integration.cpp
git commit -m "feat: add dashboard contribution registry"
```

### Task 3: Extend grid placement and YAML persistence for typed dashboard tiles

**Files:**
- Modify: `src/core/widget/WidgetTypes.hpp`
- Modify: `src/ui/WidgetGridModel.hpp`
- Modify: `src/ui/WidgetGridModel.cpp`
- Modify: `src/core/YamlConfig.hpp`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `tests/test_widget_grid_model.cpp`
- Modify: `tests/test_widget_config.cpp`
- Modify: `tests/test_widget_types.cpp`

**Step 1: Write the failing tests**

Extend `tests/test_widget_grid_model.cpp` to assert:

- grid placements store a stable dashboard contribution ID, not just a raw widget file reference
- legacy placements migrate to `Widget` contribution kind by default
- grid operations keep live-surface widgets within declared min/max bounds

Extend `tests/test_widget_config.cpp` to assert YAML round-tripping for the new placement metadata.

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_widget_grid_model|test_widget_config|test_widget_types" --output-on-failure
```

Expected: failing tests because placement metadata and YAML round-trip fields are missing.

**Step 3: Write the minimal implementation**

Update `GridPlacement` so persisted dashboard tiles carry the information needed for typed contributions, for example:

```cpp
struct GridPlacement {
    QString instanceId;
    QString contributionId;
    DashboardContributionKind kind = DashboardContributionKind::Widget;
    int col = 0;
    int row = 0;
    int colSpan = 1;
    int rowSpan = 1;
    double opacity = 0.25;
    int page = 0;
    bool visible = true;
};
```

Make `YamlConfig` read old widget placements as `Widget` contributions so existing user layouts survive the refactor.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/widget/WidgetTypes.hpp src/ui/WidgetGridModel.hpp src/ui/WidgetGridModel.cpp src/core/YamlConfig.hpp src/core/YamlConfig.cpp tests/test_widget_grid_model.cpp tests/test_widget_config.cpp tests/test_widget_types.cpp
git commit -m "feat: persist typed dashboard tile placements"
```

### Task 4: Add per-tile runtime context and a live-surface widget host

**Files:**
- Create: `src/ui/WidgetSurfaceHost.hpp`
- Create: `src/ui/WidgetSurfaceHost.cpp`
- Modify: `src/ui/WidgetInstanceContext.hpp`
- Modify: `src/ui/WidgetInstanceContext.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `qml/components/WidgetHost.qml`
- Modify: `qml/applications/home/HomeMenu.qml`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_widget_surface_host.cpp`
- Modify: `tests/test_widget_instance_context.cpp`

**Step 1: Write the failing tests**

Create `tests/test_widget_surface_host.cpp` to assert:

- a lightweight widget still loads through the existing simple loader path
- a live-surface widget gets its own runtime context and can be torn down cleanly
- host load failure clears the surface and reports an error instead of poisoning the dashboard

Extend `tests/test_widget_instance_context.cpp` to assert contribution kind and host-service accessors are exposed per tile instance.

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_widget_surface_host|test_widget_instance_context" --output-on-failure
```

Expected: compile failure because the live-surface host does not exist.

**Step 3: Write the minimal implementation**

Create `WidgetSurfaceHost` as the tile-level analogue to `PluginViewHost`. It should instantiate plugin-provided live-surface dashboard content in an isolated child context rather than through a root-context global.

Update `WidgetHost.qml` so it chooses between:

- simple `Loader` path for lightweight widgets
- `WidgetSurfaceHost` path for live-surface widgets

Update `HomeMenu.qml` to pass the correct contribution metadata into each tile host.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/ui/WidgetSurfaceHost.hpp src/ui/WidgetSurfaceHost.cpp src/ui/WidgetInstanceContext.hpp src/ui/WidgetInstanceContext.cpp src/CMakeLists.txt qml/components/WidgetHost.qml qml/applications/home/HomeMenu.qml tests/CMakeLists.txt tests/test_widget_surface_host.cpp tests/test_widget_instance_context.cpp
git commit -m "feat: add live-surface widget host"
```

### Task 5: Move phone/media monitoring into core services and stop treating them as platform-by-plugin accidents

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
- Modify: `src/core/services/BluetoothManager.hpp`
- Modify: `src/core/services/BluetoothManager.cpp`
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

Create `tests/test_phone_state_service.cpp` to assert:

- HFP/call state monitoring can exist without a phone UI plugin being active
- phone state survives UI plugin activation/deactivation
- incoming-call state can be consumed by shell overlays through a core service

Create `tests/test_media_status_service.cpp` to assert:

- Bluetooth media status can be monitored without a BT media app UI
- service state updates remain available for dashboard consumers

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_phone_state_service|test_media_status_service" --output-on-failure
```

Expected: compile failure because the new services and host context methods do not exist.

**Step 3: Write the minimal implementation**

Move the D-Bus monitoring and shared state out of `PhonePlugin` and `BtAudioPlugin` into core services. Keep any remaining phone/media plugins as thin optional UI wrappers over those services.

Update `IHostContext` / `HostContext` to expose the new services so shell/dashboard/feature plugins can consume them without root-context globals.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/services/IPhoneStateService.hpp src/core/services/PhoneStateService.hpp src/core/services/PhoneStateService.cpp src/core/services/IMediaStatusService.hpp src/core/services/MediaStatusService.hpp src/core/services/MediaStatusService.cpp src/core/plugin/IHostContext.hpp src/core/plugin/HostContext.hpp src/core/plugin/HostContext.cpp src/core/services/BluetoothManager.hpp src/core/services/BluetoothManager.cpp src/plugins/phone/PhonePlugin.hpp src/plugins/phone/PhonePlugin.cpp src/plugins/bt_audio/BtAudioPlugin.hpp src/plugins/bt_audio/BtAudioPlugin.cpp src/main.cpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_phone_state_service.cpp tests/test_media_status_service.cpp
git commit -m "refactor: move phone and media monitoring into core services"
```

### Task 6: Route AA dashboard data and shell overlays through host services instead of root-context globals

**Files:**
- Create: `src/core/services/IProjectionDashboardService.hpp`
- Create: `src/core/services/ProjectionDashboardService.hpp`
- Create: `src/core/services/ProjectionDashboardService.cpp`
- Modify: `src/core/plugin/IHostContext.hpp`
- Modify: `src/core/plugin/HostContext.hpp`
- Modify: `src/core/plugin/HostContext.cpp`
- Modify: `src/core/aa/NavigationDataBridge.hpp`
- Modify: `src/core/aa/NavigationDataBridge.cpp`
- Modify: `src/core/aa/MediaDataBridge.hpp`
- Modify: `src/core/aa/MediaDataBridge.cpp`
- Modify: `src/ui/WidgetInstanceContext.hpp`
- Modify: `src/ui/WidgetInstanceContext.cpp`
- Modify: `src/main.cpp`
- Modify: `qml/widgets/AAStatusWidget.qml`
- Modify: `qml/widgets/NavigationWidget.qml`
- Modify: `qml/widgets/NowPlayingWidget.qml`
- Modify: `qml/applications/phone/IncomingCallOverlay.qml`
- Modify: `qml/components/Shell.qml`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_projection_dashboard_service.cpp`
- Modify: `tests/test_navigation_data_bridge.cpp`
- Modify: `tests/test_media_data_bridge.cpp`

**Step 1: Write the failing tests**

Create `tests/test_projection_dashboard_service.cpp` to assert:

- AA connection status, navigation data, and media data are available through one shell-owned service
- widgets can resolve that service via `WidgetInstanceContext`
- shell overlays no longer require `AAOrchestrator`, `NavigationBridge`, `MediaBridge`, or `PhonePlugin` as root context properties

Extend the existing navigation/media bridge tests to verify the new service wrapper forwards bridge updates correctly.

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_projection_dashboard_service|test_navigation_data_bridge|test_media_data_bridge" --output-on-failure
```

Expected: compile failure because the projection dashboard service does not exist.

**Step 3: Write the minimal implementation**

Create `ProjectionDashboardService` as the shell-owned aggregation point for AA-derived dashboard state. Update widget QML and shell overlays to pull data through instance context / host services instead of direct root-context globals set in `main.cpp`.

After this task, `main.cpp` should stop exporting:

- `AAOrchestrator`
- `NavigationBridge`
- `MediaBridge`
- `PhonePlugin`

unless a temporary compatibility shim is explicitly marked and time-boxed.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/services/IProjectionDashboardService.hpp src/core/services/ProjectionDashboardService.hpp src/core/services/ProjectionDashboardService.cpp src/core/plugin/IHostContext.hpp src/core/plugin/HostContext.hpp src/core/plugin/HostContext.cpp src/core/aa/NavigationDataBridge.hpp src/core/aa/NavigationDataBridge.cpp src/core/aa/MediaDataBridge.hpp src/core/aa/MediaDataBridge.cpp src/ui/WidgetInstanceContext.hpp src/ui/WidgetInstanceContext.cpp src/main.cpp qml/widgets/AAStatusWidget.qml qml/widgets/NavigationWidget.qml qml/widgets/NowPlayingWidget.qml qml/applications/phone/IncomingCallOverlay.qml qml/components/Shell.qml tests/CMakeLists.txt tests/test_projection_dashboard_service.cpp tests/test_navigation_data_bridge.cpp tests/test_media_data_bridge.cpp
git commit -m "refactor: expose AA dashboard state through host services"
```

### Task 7: Slim AndroidAutoPlugin and retire legacy shell/app wiring

**Files:**
- Create: `src/core/aa/AndroidAutoRuntimeBridge.hpp`
- Create: `src/core/aa/AndroidAutoRuntimeBridge.cpp`
- Create: `src/core/runtime/AppRuntime.hpp`
- Create: `src/core/runtime/AppRuntime.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/ui/ApplicationController.hpp`
- Modify: `src/ui/ApplicationController.cpp`
- Modify: `src/ui/NavbarController.hpp`
- Modify: `src/ui/NavbarController.cpp`
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `qml/components/Shell.qml`
- Modify: `qml/applications/settings/SettingsMenu.qml`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_android_auto_runtime_bridge.cpp`
- Modify: `tests/test_plugin_model.cpp`
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing tests**

Create `tests/test_android_auto_runtime_bridge.cpp` to assert:

- display/navbar/touch mapping can be configured without making `AndroidAutoPlugin` the owner of shell policy
- AA activation/deactivation signals still work when the runtime bridge owns platform-side wiring

Extend `tests/test_plugin_model.cpp` to verify the foreground plugin model still behaves correctly after the runtime split.

Extend `tests/test_settings_menu_structure.cpp` so built-in settings remain reachable without the old app-enum navigation leaking back into shell logic.

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_android_auto_runtime_bridge|test_plugin_model|test_settings_menu_structure" --output-on-failure
```

Expected: compile failure because the runtime bridge and runtime bootstrap do not exist.

**Step 3: Write the minimal implementation**

Create:

- `AndroidAutoRuntimeBridge` to own platform-adjacent AA display/touch/navbar wiring
- `AppRuntime` to replace `main.cpp` as the god-assembler

Reduce `AndroidAutoPlugin` to AA-specific orchestration and plugin contribution behavior. Keep shell-owned settings and dashboard behavior reachable without depending on the old `ApplicationTypes` enum model as the main navigation architecture.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/aa/AndroidAutoRuntimeBridge.hpp src/core/aa/AndroidAutoRuntimeBridge.cpp src/core/runtime/AppRuntime.hpp src/core/runtime/AppRuntime.cpp src/plugins/android_auto/AndroidAutoPlugin.hpp src/plugins/android_auto/AndroidAutoPlugin.cpp src/ui/ApplicationController.hpp src/ui/ApplicationController.cpp src/ui/NavbarController.hpp src/ui/NavbarController.cpp src/main.cpp src/CMakeLists.txt qml/components/Shell.qml qml/applications/settings/SettingsMenu.qml tests/CMakeLists.txt tests/test_android_auto_runtime_bridge.cpp tests/test_plugin_model.cpp tests/test_settings_menu_structure.cpp
git commit -m "refactor: separate AA plugin logic from platform runtime wiring"
```

### Task 8: Update architecture docs, verify behavior, and record handoff

**Files:**
- Modify: `docs/plugin-api.md`
- Modify: `docs/settings-tree.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/session-handoffs.md`

**Step 1: Update documentation**

Document:

- the new plugin/dashboard contribution model
- the rule that core owns singleton hardware/system policy
- the fact that dashboard widgets and live-surface widgets are distinct contribution kinds
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
