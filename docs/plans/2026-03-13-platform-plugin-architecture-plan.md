# Platform / Plugin Architecture Refactor Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Refactor Prodigy into a platform runtime with a first-class dashboard and capability-based plugins, while keeping Android Auto the priority feature and avoiding a full rewrite.

**Architecture:** Keep the existing plugin runtime, dashboard grid, and core service layer, but formalize the seams between core platform, shell, dashboard, and feature plugins. This revision intentionally keeps the type distinction between lightweight widgets and future live-surface widgets, but defers speculative hosting/runtime infrastructure until a concrete live-surface use case exists. It replaces monolithic wrapper ideas with narrow provider interfaces, breaks `main.cpp` cleanup into smaller extractions instead of a single `AppRuntime` jump, and sequences core service creation before root-context global removal so providers are never backed by transitional shims.

**Tech Stack:** Qt 6 / QML, C++17, CMake, QTest, yaml-cpp, D-Bus/BlueZ, PipeWire

---

> **Note:** This plan supersedes `docs/plans/active/2026-02-21-architecture-extensibility-plan.md` where the two conflict. Use the 2026-03-13 design doc and this plan as the source of truth for the refactor.

> **Scope adjustment:** This plan keeps `contributionKind` in the model now, but defers `WidgetSurfaceHost` and any full live-surface runtime path until a concrete embedded AA pane or equivalent plugin requirement exists. `WidgetPickerModel` must exclude `LiveSurfaceWidget` entries until a host path exists, so the dashboard never offers unlaunchable tiles.

> **Sequencing rationale:** Core phone/media services (Task 3) are created before root-context globals are removed (Task 4). This avoids the problem of making `MediaDataBridge` or `PhonePlugin` temporarily satisfy provider interfaces only to replace them one task later. Provider interfaces are defined in Task 2, implemented by real core services in Task 3, and consumed by QML in Task 4.

### Review Workflow

Each task follows this loop:

1. **Claude (Opus 4.6)** writes failing tests, then implementation, following the TDD steps in the task.
2. **Tests pass locally** — build + `ctest` confirms green.
3. **Codex (GPT-5.4) reviews** — before committing, dispatch a Codex agent to review the diff against this plan. The review should check:
   - Does the implementation match the task's stated intent and file list?
   - Are there architectural regressions (new root-context globals, leaked concrete types through provider interfaces, plugin code doing platform work)?
   - Are test assertions meaningful (not just "it compiles")?
   - Are there seam violations (test code reaching into private methods, hardware-coupled assertions)?
4. **Address review findings** — fix issues Codex flags before committing. If Codex and Claude disagree on a finding, surface it to Matt for a decision.
5. **Commit** — only after review is clean.

This applies to Tasks 1–6. Task 7 (docs + verification) does not need code review but does need Pi hardware verification.

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
- `WidgetPickerModel` excludes descriptors with `contributionKind == LiveSurfaceWidget`

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

Update `WidgetPickerModel` to filter out `LiveSurfaceWidget` entries from the picker. The picker should only show widget types that have a working host path. Since `WidgetSurfaceHost` is deferred, `LiveSurfaceWidget` descriptors must be excluded.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/widget/WidgetTypes.hpp src/core/widget/WidgetRegistry.hpp src/core/widget/WidgetRegistry.cpp src/core/plugin/IPlugin.hpp src/ui/WidgetPickerModel.hpp src/ui/WidgetPickerModel.cpp tests/test_widget_types.cpp tests/test_widget_registry.cpp tests/test_widget_plugin_integration.cpp
git commit -m "feat: add typed dashboard contribution metadata"
```

### Task 2: Define narrow provider interfaces and add projection status provider

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
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_projection_status_provider.cpp`

**Step 1: Write the failing tests**

Create `tests/test_projection_status_provider.cpp` to assert:

- a narrow provider can expose projection connection state without leaking the full `AndroidAutoOrchestrator`
- `HostContext` can store and return each provider type

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_projection_status_provider" --output-on-failure
```

Expected: compile failure because the provider interfaces and host context accessors do not exist.

**Step 3: Write the minimal implementation**

Define the four narrow provider interfaces:

- `IProjectionStatusProvider` — projection connection state (connected, backgrounded, disconnected), status message
- `INavigationProvider` — nav active, road name, maneuver type/direction, formatted distance, icon availability
- `IMediaStatusProvider` — title, artist, album, playback state, source, app name, playback controls (play/pause/next/previous)
- `ICallStateProvider` — call state (idle/ringing/active), caller name, caller number, answer/hangup actions

Add a small `ProjectionStatusProvider` wrapper around `AndroidAutoOrchestrator` — this is the only provider implemented in this task because AA projection has no pending core-service migration.

Add all four provider accessors to `IHostContext` / `HostContext` (returning `nullptr` for unregistered providers). Do **not** make `MediaDataBridge`, `NavigationDataBridge`, or `PhonePlugin` implement the interfaces in this task. Those providers will be backed by real core services created in Task 3.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/services/IProjectionStatusProvider.hpp src/core/services/INavigationProvider.hpp src/core/services/IMediaStatusProvider.hpp src/core/services/ICallStateProvider.hpp src/core/services/ProjectionStatusProvider.hpp src/core/services/ProjectionStatusProvider.cpp src/core/plugin/IHostContext.hpp src/core/plugin/HostContext.hpp src/core/plugin/HostContext.cpp src/main.cpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_projection_status_provider.cpp
git commit -m "feat: define provider interfaces and add projection status provider"
```

### Task 3: Promote phone, media, and navigation monitoring into core-owned services

**Files:**
- Create: `src/core/services/IPhoneStateService.hpp`
- Create: `src/core/services/PhoneStateService.hpp`
- Create: `src/core/services/PhoneStateService.cpp`
- Create: `src/core/services/IMediaStatusService.hpp`
- Create: `src/core/services/MediaStatusService.hpp`
- Create: `src/core/services/MediaStatusService.cpp`
- Modify: `src/core/aa/NavigationDataBridge.hpp`
- Modify: `src/core/aa/NavigationDataBridge.cpp`
- Modify: `src/core/plugin/IHostContext.hpp`
- Modify: `src/core/plugin/HostContext.hpp`
- Modify: `src/core/plugin/HostContext.cpp`
- Modify: `src/plugins/phone/PhonePlugin.hpp`
- Modify: `src/plugins/phone/PhonePlugin.cpp`
- Modify: `src/plugins/bt_audio/BtAudioPlugin.hpp`
- Modify: `src/plugins/bt_audio/BtAudioPlugin.cpp`
- Modify: `src/core/aa/MediaDataBridge.hpp`
- Modify: `src/core/aa/MediaDataBridge.cpp`
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_phone_state_service.cpp`
- Create: `tests/test_media_status_service.cpp`
- Modify: `tests/test_navigation_data_bridge.cpp`

**Step 1: Write the failing tests**

Create tests to assert:

- `PhoneStateService` owns the HFP D-Bus monitoring AND the full call state machine (idle → ringing → active → idle). Phone/call state survives regardless of whether the phone UI plugin is active.
- `MediaStatusService` owns the AA + BT media merging (source priority, cached per-source state). Media state survives regardless of whether the BT audio UI plugin is active.
- `NavigationDataBridge` satisfies `INavigationProvider` (single-source, no merging needed).
- Shell and dashboard providers no longer depend on plugin lifetime for state ownership.

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_phone_state_service|test_media_status_service|test_navigation_data_bridge" --output-on-failure
```

Expected: compile failure because the core phone/media services do not exist.

**Step 3: Write the minimal implementation**

**PhoneStateService** — Move from `PhonePlugin` into core:
- HFP device detection via `org.bluez.Device1` + `ObjectManager.GetManagedObjects` D-Bus scanning
- The full call state machine (idle/ringing/active transitions)
- Notification dispatch for incoming calls (via `INotificationService`)
- `answer()` and `hangup()` actions
- Implements `ICallStateProvider`

**MediaStatusService** — Extract from `MediaDataBridge`:
- AA + BT source merging with priority logic (AA > BT > None)
- Per-source cached metadata state
- Playback control delegation to active source
- Implements `IMediaStatusProvider`

**NavigationDataBridge** — Make it implement `INavigationProvider` directly (it's single-source from AA, no merging needed).

Update plugins to become UI-facing wrappers:
- `PhonePlugin` reads state from `PhoneStateService`, exposes Q_PROPERTYs for its QML view, delegates `answer()`/`hangup()` to the service. No longer owns D-Bus monitoring or call state machine.
- `BtAudioPlugin` reads state from `MediaStatusService` for its QML view. No longer owns A2DP metadata/playback state (still owns A2DP transport lifecycle for connection management).

Register `PhoneStateService` as `ICallStateProvider`, `MediaStatusService` as `IMediaStatusProvider`, and `NavigationDataBridge` as `INavigationProvider` on `HostContext`.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/services/IPhoneStateService.hpp src/core/services/PhoneStateService.hpp src/core/services/PhoneStateService.cpp src/core/services/IMediaStatusService.hpp src/core/services/MediaStatusService.hpp src/core/services/MediaStatusService.cpp src/core/aa/NavigationDataBridge.hpp src/core/aa/NavigationDataBridge.cpp src/core/plugin/IHostContext.hpp src/core/plugin/HostContext.hpp src/core/plugin/HostContext.cpp src/plugins/phone/PhonePlugin.hpp src/plugins/phone/PhonePlugin.cpp src/plugins/bt_audio/BtAudioPlugin.hpp src/plugins/bt_audio/BtAudioPlugin.cpp src/core/aa/MediaDataBridge.hpp src/core/aa/MediaDataBridge.cpp src/main.cpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_phone_state_service.cpp tests/test_media_status_service.cpp tests/test_navigation_data_bridge.cpp
git commit -m "refactor: move phone, media, and navigation monitoring into core services"
```

### Task 4: Remove shell and widget root-context globals by routing through provider interfaces

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

- `WidgetInstanceContext` exposes provider-backed Q_PROPERTYs for QML consumption
- debug AA button actions route through `ActionRegistry` rather than direct `AAOrchestrator.sendButtonPress(...)` calls

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_widget_instance_context|test_action_registry|test_settings_menu_structure" --output-on-failure
```

Expected: failing tests because widgets and debug controls still depend on root-context globals.

**Step 3: Write the minimal implementation**

**WidgetInstanceContext QML-facing API changes:** Add these Q_PROPERTYs to `WidgetInstanceContext` so widget QML can resolve state through providers instead of root-context globals:

- `projectionStatus` (`IProjectionStatusProvider*`) — replaces `AAOrchestrator` in AAStatusWidget
- `navigationProvider` (`INavigationProvider*`) — replaces `NavigationBridge` in NavigationWidget
- `mediaStatus` (`IMediaStatusProvider*`) — replaces `MediaBridge` in NowPlayingWidget

These are populated from `hostContext()->projectionStatusProvider()` etc. during construction.

**Shell/overlay changes:**
- `IncomingCallOverlay.qml` — bind to a `callStateProvider` root-context property (backed by `PhoneStateService` via `ICallStateProvider`) instead of `PhonePlugin` directly. The provider exposes `callState`, `callerName`, `callerNumber`, `answer()`, `hangup()`.
- `DebugSettings.qml` — route AA button presses through `ActionRegistry.dispatch("aa.sendButton", keycode)` instead of `AAOrchestrator.sendButtonPress(keycode)`.
- `Shell.qml` — remove `PhonePlugin` dependency; call overlay binds to provider instead.

**Note on the `navicon` image provider:** The `QQuickImageProvider` registration for `image://navicon/` remains in main.cpp as QML engine infrastructure. This is not a root-context property and is unaffected by this migration. `NavigationWidget.qml` continues to use `source: "image://navicon/current?" + iconVer`.

After this task, remove these four root-context properties from `main.cpp`:
- `AAOrchestrator`
- `NavigationBridge`
- `MediaBridge`
- `PhonePlugin`

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Pi verification**

The QML binding migration (IncomingCallOverlay, widget globals) cannot be fully verified by unit tests alone. After the cross-build in Task 7, deploy to Pi and verify:
- incoming call overlay still appears when a call comes in
- AA status widget reflects connection state
- Now Playing widget shows media metadata
- Navigation widget updates during active navigation
- debug AA buttons still function in settings

**Step 6: Commit**

```bash
git add src/ui/WidgetInstanceContext.hpp src/ui/WidgetInstanceContext.cpp src/main.cpp qml/widgets/AAStatusWidget.qml qml/widgets/NavigationWidget.qml qml/widgets/NowPlayingWidget.qml qml/applications/phone/IncomingCallOverlay.qml qml/components/Shell.qml qml/applications/settings/DebugSettings.qml tests/test_widget_instance_context.cpp tests/test_action_registry.cpp tests/test_settings_menu_structure.cpp
git commit -m "refactor: remove AA and phone root-context globals"
```

### Task 5: Remove legacy `Configuration` from the entire AA stack and unify config access

> **Semantic decision:** `Configuration::wirelessEnabled()` is dead code. Prodigy is a wireless-only AA product (USB AA is a stated non-goal). The flag defaults to `true`, has no YAML key, no UI toggle, and no way for a user to change it on a fresh Trixie install. The guard at `AndroidAutoOrchestrator.cpp:101` is removed outright — if the AA plugin is loaded, BT discovery starts unconditionally.

**Files:**
- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.cpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.hpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/test_aa_orchestrator.cpp`
- Create: `tests/test_bt_discovery_service.cpp`
- Remove (if no other consumers): `src/core/Configuration.hpp`, `src/core/Configuration.cpp`, `tests/test_configuration.cpp`

**Step 1: Write the failing tests**

Extend `tests/test_aa_orchestrator.cpp` to assert:

- `AndroidAutoOrchestrator` can be constructed with `IConfigService*` instead of `std::shared_ptr<Configuration>`
- `connection.tcp_port` is read correctly from `IConfigService` (used in 3 orchestrator call sites + 2 BT discovery call sites)

Create `tests/test_bt_discovery_service.cpp` to assert:

- `BluetoothDiscoveryService` reads SSID, password, and TCP port from `IConfigService` (or constructor params) rather than `std::shared_ptr<Configuration>`
- a `WifiStartRequest` protobuf built with a given port contains that port
- a `WifiSecurityResponse` protobuf built with a given SSID and password contains those values

**Test seam:** The private methods `sendWifiStartRequest()` and `handleWifiCredentialRequest()` mix config reads, network interface probing, BSSID lookup, and socket I/O — none of which are testable without hardware. Extract two pure message-building helpers (e.g. `buildWifiStartRequest(ip, port)` returning `WifiStartRequest` and `buildWifiCredentialResponse(ssid, password, bssid)` returning `WifiSecurityResponse`) that take values in and return protobuf message objects. The existing `sendMessage()` already takes `google::protobuf::Message&`, so the private methods become: build message via helper, pass to `sendMessage()`. Tests call the builders directly and inspect fields on the returned message — no socket, no `QNetworkInterface`, no `QBluetoothServer`, no serialize/parse churn.

This covers the protocol-sensitive credential handshake path (`BluetoothDiscoveryService.cpp:305-397`) that would otherwise be migrated without test coverage.

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_aa_orchestrator|test_bt_discovery_service" --output-on-failure
```

Expected: failing tests because `AndroidAutoOrchestrator` and `BluetoothDiscoveryService` still require `std::shared_ptr<Configuration>`.

**Step 3: Write the minimal implementation**

The full `Configuration` dependency in the AA stack spans two classes:

**`AndroidAutoOrchestrator`** uses:
- `config_->tcpPort()` — 3 call sites (`start()`, `onNewConnection()`, reconnect path)
- `config_->wirelessEnabled()` — 1 call site (gates BT discovery creation) — **remove this guard entirely**

**`BluetoothDiscoveryService`** uses:
- `config_->tcpPort()` — 2 call sites (`sendWifiStartRequest()`)
- `config_->wifiSsid()` — 2 call sites (`handleWifiCredentialRequest()`)
- `config_->wifiPassword()` — 1 call site (`handleWifiCredentialRequest()`)

All of these values already exist in YAML config:
- `connection.tcp_port` (YamlConfig default: 5277)
- `connection.wifi_ap.ssid` (YamlConfig default: "OpenAutoProdigy")
- `connection.wifi_ap.password` (YamlConfig default: "prodigy")

**`main.cpp`** also uses `config->wifiSsid()` at line 308 to feed `CompanionListenerService::setWifiSsid()`. This must be rewired to `yamlConfig->wifiSsid()` or `configService->value("connection.wifi_ap.ssid")`.

Implementation:
- Replace `std::shared_ptr<Configuration>` in `AndroidAutoOrchestrator` constructor with `IConfigService*`
- Read `connection.tcp_port` from `IConfigService` where `config_->tcpPort()` was used
- Remove the `wirelessEnabled()` guard — BT discovery starts unconditionally
- Replace `std::shared_ptr<Configuration>` in `BluetoothDiscoveryService` constructor with the three values it needs (port, ssid, password) read from `IConfigService`, or pass `IConfigService*` directly
- Remove `std::shared_ptr<Configuration>` from `AndroidAutoPlugin` constructor
- Rewire `companionListener->setWifiSsid(config->wifiSsid())` in `main.cpp:308` to read from `yamlConfig` or `IConfigService`
- Remove the YAML→INI sync shim in `main.cpp:120-124` and legacy `Configuration` construction at `main.cpp:108-113`
- If `Configuration` has no remaining consumers after the above changes: delete `src/core/Configuration.hpp`, `src/core/Configuration.cpp`, and `tests/test_configuration.cpp`; remove from `src/CMakeLists.txt` and `tests/CMakeLists.txt`. If other consumers still exist, leave the class but sever the AA dependency and note the remaining consumers for a follow-up cleanup.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/aa/AndroidAutoOrchestrator.hpp src/core/aa/AndroidAutoOrchestrator.cpp src/core/aa/BluetoothDiscoveryService.hpp src/core/aa/BluetoothDiscoveryService.cpp src/plugins/android_auto/AndroidAutoPlugin.hpp src/plugins/android_auto/AndroidAutoPlugin.cpp src/main.cpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_aa_orchestrator.cpp tests/test_bt_discovery_service.cpp
git commit -m "refactor: remove legacy Configuration from AA stack"
```

### Task 6: Extract AA platform policy from plugin and extract gesture overlay controller

**Files:**
- Create: `src/core/aa/AndroidAutoRuntimeBridge.hpp`
- Create: `src/core/aa/AndroidAutoRuntimeBridge.cpp`
- Create: `src/ui/GestureOverlayController.hpp`
- Create: `src/ui/GestureOverlayController.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/main.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_android_auto_runtime_bridge.cpp`
- Create: `tests/test_gesture_overlay_controller.cpp`

**Step 1: Write the failing tests**

Create tests to assert:

- `AndroidAutoRuntimeBridge` can manage display/touch/navbar platform policy without `AndroidAutoPlugin` owning it
- `GestureOverlayController` can manage three-finger overlay zone registration and teardown independently of main.cpp

**Step 2: Run tests to verify they fail**

Run:

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R "test_android_auto_runtime_bridge|test_gesture_overlay_controller" --output-on-failure
```

Expected: compile failure because the runtime bridge and gesture overlay controller do not exist.

**Step 3: Write the minimal implementation**

**`AndroidAutoRuntimeBridge`** — Extract platform-adjacent policy **out of `AndroidAutoPlugin`**, not trivial assembler hookups out of `main.cpp`. The plugin currently owns several platform concerns that should belong to the bridge:

- Touch device auto-detection + `EvdevTouchReader` creation and lifecycle (currently `AndroidAutoPlugin.cpp:75+`)
- `EvdevCoordBridge` creation and management (currently `AndroidAutoPlugin.cpp:98+`)
- Display dimension injection into the orchestrator (currently `AndroidAutoPlugin.cpp:118+`)
- Navbar thickness calculation and injection (currently `AndroidAutoPlugin.cpp:173+`)

After extraction, `AndroidAutoPlugin` retains:
- AA orchestrator creation and lifecycle (start/stop/session management)
- QML context property exposure (`onActivated`/`onDeactivated`)
- Connection state → activation/deactivation signals
- Config change watching (video.resolution/fps)

Simple signal→slot wiring in main.cpp (requestActivation → PluginModel, coordBridge → NavbarController) stays in main.cpp — that's assembler code and reads fine there.

**`GestureOverlayController`** — Extract the ~130 lines of three-finger overlay zone registration, slider handling, and coordinate mapping currently embedded in `main.cpp`. Owns the overlay show/hide lifecycle, evdev zone priority management, and service calls for volume/brightness adjustments.

Do **not** introduce a top-level `AppRuntime` class in this task. Keep `main.cpp` as the assembler.

**Step 4: Run tests to verify they pass**

Run the command from Step 2 again.

Expected: PASS.

**Step 5: Commit**

```bash
git add src/core/aa/AndroidAutoRuntimeBridge.hpp src/core/aa/AndroidAutoRuntimeBridge.cpp src/ui/GestureOverlayController.hpp src/ui/GestureOverlayController.cpp src/plugins/android_auto/AndroidAutoPlugin.hpp src/plugins/android_auto/AndroidAutoPlugin.cpp src/main.cpp src/CMakeLists.txt tests/CMakeLists.txt tests/test_android_auto_runtime_bridge.cpp tests/test_gesture_overlay_controller.cpp
git commit -m "refactor: extract AA platform policy and gesture overlay controller"
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
- the fact that `LiveSurfaceWidget` remains a declared contribution type but its runtime host is intentionally deferred; `WidgetPickerModel` excludes it
- the provider-interface approach for shell/dashboard data
- the `WidgetInstanceContext` QML-facing provider properties (`projectionStatus`, `navigationProvider`, `mediaStatus`)
- the reduced role of standalone phone/media UI in the current product focus
- `PhoneStateService` owns HFP monitoring + call state machine; `PhonePlugin` is a UI wrapper
- `MediaStatusService` owns media source merging; `BtAudioPlugin` is a UI wrapper
- `AndroidAutoRuntimeBridge` owns touch/display/navbar platform policy; `AndroidAutoPlugin` owns AA protocol lifecycle

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

**Step 3: Pi deployment verification**

Deploy to Pi and verify the following on hardware:

- AA connection flow (BT discovery → WiFi → TCP → video projection) works
- incoming call overlay appears and functions (answer/hangup)
- AA status widget reflects connection state on home screen
- Now Playing widget shows media metadata from both AA and BT sources
- Navigation widget updates during active navigation
- 3-finger gesture overlay functions (volume/brightness sliders)
- debug AA buttons in settings function
- navbar touch zones function during AA session
- widget picker does not show any `LiveSurfaceWidget` entries

**Step 4: Record the session handoff**

Append to `docs/session-handoffs.md`:

- what changed
- why
- status
- next 1-3 steps
- verification commands/results

**Step 5: Commit**

```bash
git add docs/plugin-api.md docs/settings-tree.md docs/roadmap-current.md docs/session-handoffs.md
git commit -m "docs: record platform-plugin architecture refactor"
```
