# Architecture Migration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Migrate OpenAuto Prodigy from a monolithic app to a plugin-based architecture as defined in `docs/plans/2026-02-18-architecture-and-ux-design.md`.

**Architecture:** The migration wraps existing working AA code into a plugin interface without breaking functionality. Each phase builds on the previous one, with the app compiling and working (at least in degraded mode) after each task. Service interfaces are extracted first, then the plugin boundary is defined, then existing code is wrapped as a static plugin.

**Tech Stack:** C++17, Qt 6 (6.4 dev / 6.8 target), yaml-cpp, CMake, Boost.ASIO, FFmpeg, PipeWire, protobuf

**Design reference:** `docs/plans/2026-02-18-architecture-and-ux-design.md`

---

## Phase Overview

| Phase | Tasks | Description | Dependency |
|-------|-------|-------------|------------|
| **A** | 1-3 | YAML config migration | None |
| **B** | 4-7 | Service interfaces + plugin boundary | Phase A |
| **C** | 8-10 | Plugin manager + discovery | Phase B |
| **D** | 11-13 | QML shell refactor | Phase B |
| **E** | 14-16 | AA static plugin wrapping | Phases C + D |
| **F** | 17-19 | Theme system | Phase D |
| **G** | 20-22 | Audio pipeline (PipeWire) | Phase B |
| **H** | 23-25 | BT Audio plugin | Phases C + G |
| **I** | 26-28 | Phone (HFP) plugin | Phases C + G |
| **J** | 29-31 | Web config panel | Phase A |
| **K** | 32-33 | 3-finger gesture overlay | Phase E |
| **L** | 34-35 | Install script + polish | All |

**Critical path:** A → B → C + D → E (AA must keep working throughout)

---

## Phase A: YAML Configuration Migration

### Task 1: Add yaml-cpp dependency to build system

**Files:**
- Modify: `CMakeLists.txt`

**Step 1: Verify yaml-cpp availability**

Run: `apt list --installed 2>/dev/null | grep yaml-cpp` on dev VM.
If not installed: `sudo apt install libyaml-cpp-dev`

Verify on Pi too: `ssh matt@192.168.1.149 'apt list --installed 2>/dev/null | grep yaml-cpp'`
If not installed: `ssh matt@192.168.1.149 'sudo apt install libyaml-cpp-dev'`

**Step 2: Add yaml-cpp to top-level CMakeLists.txt**

After the `pkg_check_modules(LIBAV ...)` line (line 19), add:

```cmake
find_package(yaml-cpp REQUIRED)
```

In `src/CMakeLists.txt`, add to `target_link_libraries`:

```cmake
yaml-cpp::yaml-cpp
```

**Step 3: Build to verify linkage**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && cmake --build . -j$(nproc)`
Expected: Clean compile

**Step 4: Commit**

```bash
git add CMakeLists.txt src/CMakeLists.txt
git commit -m "build: add yaml-cpp dependency"
```

---

### Task 2: Create YAML config loader alongside existing INI loader

**Files:**
- Create: `src/core/YamlConfig.hpp`
- Create: `src/core/YamlConfig.cpp`
- Create: `tests/test_yaml_config.cpp`
- Create: `tests/data/test_config.yaml`
- Modify: `tests/CMakeLists.txt`
- Modify: `src/CMakeLists.txt`

**Context:** We build the new YAML config system alongside the existing INI system. Both coexist until migration is complete. The YAML config follows the schema from the design doc Section 7.

**Step 1: Create test config YAML file**

Create `tests/data/test_config.yaml`:

```yaml
hardware_profile: rpi4

display:
  brightness: 80
  theme: default
  orientation: landscape

connection:
  auto_connect_aa: true
  bt_discoverable: true
  wifi_ap:
    ssid: "TestSSID"
    password: "TestPassword"
    channel: 36
    band: a

audio:
  master_volume: 75
  output_device: auto

video:
  fps: 60
  resolution: 720p

nav_strip:
  order:
    - org.openauto.android-auto
    - org.openauto.bt-audio
  show_labels: true

plugins:
  enabled:
    - org.openauto.android-auto
    - org.openauto.bt-audio
  disabled: []
```

**Step 2: Write the failing test**

Create `tests/test_yaml_config.cpp`:

```cpp
#include <QtTest>
#include "core/YamlConfig.hpp"

class TestYamlConfig : public QObject {
    Q_OBJECT
private slots:
    void testLoadDefaults();
    void testLoadFromFile();
    void testSaveAndReload();
    void testPluginScoping();
};

void TestYamlConfig::testLoadDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.hardwareProfile(), QString("rpi4"));
    QCOMPARE(config.displayBrightness(), 80);
    QCOMPARE(config.wifiSsid(), QString("OpenAutoProdigy"));
    QCOMPARE(config.tcpPort(), static_cast<uint16_t>(5288));
    QCOMPARE(config.videoFps(), 60);
    QCOMPARE(config.autoConnectAA(), true);
    QCOMPARE(config.masterVolume(), 80);
}

void TestYamlConfig::testLoadFromFile()
{
    oap::YamlConfig config;
    config.load(QString(TEST_DATA_DIR) + "/test_config.yaml");

    QCOMPARE(config.wifiSsid(), QString("TestSSID"));
    QCOMPARE(config.wifiPassword(), QString("TestPassword"));
    QCOMPARE(config.masterVolume(), 75);
    QCOMPARE(config.displayBrightness(), 80);

    auto enabled = config.enabledPlugins();
    QCOMPARE(enabled.size(), 2);
    QCOMPARE(enabled[0], QString("org.openauto.android-auto"));
}

void TestYamlConfig::testSaveAndReload()
{
    oap::YamlConfig config;
    config.setWifiSsid("NewSSID");
    config.setMasterVolume(50);

    QString tmpPath = QDir::tempPath() + "/oap_test_config.yaml";
    config.save(tmpPath);

    oap::YamlConfig loaded;
    loaded.load(tmpPath);
    QCOMPARE(loaded.wifiSsid(), QString("NewSSID"));
    QCOMPARE(loaded.masterVolume(), 50);

    QFile::remove(tmpPath);
}

void TestYamlConfig::testPluginScoping()
{
    oap::YamlConfig config;
    // Plugin configs are scoped by plugin ID
    config.setPluginValue("org.openauto.android-auto", "auto_connect", true);
    config.setPluginValue("org.openauto.android-auto", "video_fps", 60);

    QCOMPARE(config.pluginValue("org.openauto.android-auto", "auto_connect").toBool(), true);
    QCOMPARE(config.pluginValue("org.openauto.android-auto", "video_fps").toInt(), 60);
    // Different plugin returns invalid
    QVERIFY(!config.pluginValue("org.openauto.bt-audio", "auto_connect").isValid());
}

QTEST_MAIN(TestYamlConfig)
#include "test_yaml_config.moc"
```

**Step 3: Write YamlConfig header**

Create `src/core/YamlConfig.hpp`:

```cpp
#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>
#include <yaml-cpp/yaml.h>

namespace oap {

class YamlConfig {
public:
    YamlConfig();

    void load(const QString& filePath);
    void save(const QString& filePath) const;

    // Hardware profile
    QString hardwareProfile() const;
    void setHardwareProfile(const QString& v);

    // Display
    int displayBrightness() const;
    void setDisplayBrightness(int v);
    QString theme() const;
    void setTheme(const QString& v);

    // Connection
    bool autoConnectAA() const;
    void setAutoConnectAA(bool v);
    QString wifiSsid() const;
    void setWifiSsid(const QString& v);
    QString wifiPassword() const;
    void setWifiPassword(const QString& v);
    uint16_t tcpPort() const;
    void setTcpPort(uint16_t v);

    // Audio
    int masterVolume() const;
    void setMasterVolume(int v);

    // Video
    int videoFps() const;
    void setVideoFps(int v);

    // Plugins
    QStringList enabledPlugins() const;
    void setEnabledPlugins(const QStringList& plugins);

    // Plugin-scoped config
    QVariant pluginValue(const QString& pluginId, const QString& key) const;
    void setPluginValue(const QString& pluginId, const QString& key, const QVariant& value);

private:
    YAML::Node root_;
    QMap<QString, QMap<QString, QVariant>> pluginConfigs_;

    void initDefaults();
};

} // namespace oap
```

**Step 4: Write YamlConfig implementation**

Create `src/core/YamlConfig.cpp` — implements load/save using yaml-cpp, getters/setters read from/write to the `root_` YAML node. Plugin-scoped config stored in `pluginConfigs_` map.

The implementation should:
- `initDefaults()` populates `root_` with the default config schema
- `load()` parses YAML file, merges over defaults
- `save()` writes `root_` to file
- Getters navigate the YAML tree: `root_["connection"]["wifi_ap"]["ssid"]`
- Plugin config stored separately in `pluginConfigs_` and serialized under `plugin_config:` key

**Step 5: Add to build system**

In `src/CMakeLists.txt`, add `core/YamlConfig.cpp` to the `qt_add_executable` source list.

In `tests/CMakeLists.txt`, add:

```cmake
add_executable(test_yaml_config
    test_yaml_config.cpp
    ${CMAKE_SOURCE_DIR}/src/core/YamlConfig.cpp
)
target_include_directories(test_yaml_config PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(test_yaml_config PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
target_link_libraries(test_yaml_config PRIVATE
    Qt6::Core
    Qt6::Test
    yaml-cpp::yaml-cpp
)
configure_file(data/test_config.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_config.yaml COPYONLY)
add_test(NAME test_yaml_config COMMAND test_yaml_config)
```

**Step 6: Build and run tests**

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass including new `test_yaml_config`

**Step 7: Commit**

```bash
git add src/core/YamlConfig.hpp src/core/YamlConfig.cpp tests/test_yaml_config.cpp tests/data/test_config.yaml src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add YAML configuration system (YamlConfig)"
```

---

### Task 3: Wire YamlConfig into main.cpp alongside INI config

**Files:**
- Modify: `src/main.cpp`

**Context:** Use YamlConfig as the primary config source, falling back to INI if no YAML file exists. This lets existing users migrate gradually.

**Step 1: Update main.cpp to try YAML first, fall back to INI**

```cpp
// Try YAML config first, fall back to INI
QString yamlPath = QDir::homePath() + "/.openauto/config.yaml";
QString iniPath = QDir::homePath() + "/.openauto/openauto_system.ini";

auto yamlConfig = std::make_shared<oap::YamlConfig>();
if (QFile::exists(yamlPath)) {
    yamlConfig->load(yamlPath);
} else if (QFile::exists(iniPath)) {
    // Legacy INI — load old config, migrate values to YamlConfig
    auto legacyConfig = std::make_shared<oap::Configuration>();
    legacyConfig->load(iniPath);
    yamlConfig->setWifiSsid(legacyConfig->wifiSsid());
    yamlConfig->setWifiPassword(legacyConfig->wifiPassword());
    yamlConfig->setTcpPort(legacyConfig->tcpPort());
    yamlConfig->setVideoFps(legacyConfig->videoFps());
    // Save as YAML for next boot
    QDir().mkpath(QDir::homePath() + "/.openauto");
    yamlConfig->save(yamlPath);
}
```

For now, the rest of main.cpp continues using the old `Configuration` object for AA service wiring. We'll fully switch over when the plugin system is in place. The goal here is just to establish the YAML config path and auto-migrate.

**Step 2: Build and verify**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: Clean compile

**Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: wire YAML config with INI fallback and auto-migration"
```

---

## Phase B: Service Interfaces + Plugin Boundary

### Task 4: Define core service interfaces

**Files:**
- Create: `src/core/services/IAudioService.hpp`
- Create: `src/core/services/IBluetoothService.hpp`
- Create: `src/core/services/IConfigService.hpp`
- Create: `src/core/services/IThemeService.hpp`
- Create: `src/core/services/IDisplayService.hpp`
- Create: `src/core/services/IEventBus.hpp`

**Context:** Pure abstract interfaces that define the service boundary between core and plugins. These are header-only — no implementation yet. Design doc Section 4 defines the service list.

**Step 1: Create the services directory and headers**

Each header is a pure abstract class (all virtual methods = 0). No implementation, no dependencies beyond Qt types.

Key interfaces to define:

- `IAudioService`: `createStream()`, `destroyStream()`, `setMasterVolume()`, `requestAudioFocus()`, `releaseAudioFocus()`
- `IBluetoothService`: `adapterAddress()`, `isDiscoverable()`, `setDiscoverable()`, `startAdvertising()`, `stopAdvertising()`
- `IConfigService`: `value()`, `setValue()`, `pluginValue()`, `setPluginValue()`, `save()`
- `IThemeService`: `color()`, `fontFamily()`, `iconPath()`, `currentThemeId()`, signals for theme changes
- `IDisplayService`: `screenSize()`, `brightness()`, `setBrightness()`, `orientation()`
- `IEventBus`: `publish()`, `subscribe()`, `unsubscribe()` — string-keyed events with QVariant payloads

**Step 2: Build to verify headers compile**

Include them from a test or main.cpp to verify no syntax errors.

Run: `cd build && cmake --build . -j$(nproc)`

**Step 3: Commit**

```bash
git add src/core/services/
git commit -m "feat: define core service interfaces (IAudioService, IBluetoothService, etc.)"
```

---

### Task 5: Define IPlugin and IHostContext interfaces

**Files:**
- Create: `src/core/plugin/IPlugin.hpp`
- Create: `src/core/plugin/IHostContext.hpp`

**Context:** The plugin boundary as defined in design doc Section 4. `IPlugin` is what plugins implement. `IHostContext` is what the core provides to plugins.

**Step 1: Create IPlugin.hpp**

```cpp
#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace oap {

class IHostContext;

class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Identity
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual int apiVersion() const = 0;

    // Lifecycle
    virtual bool initialize(IHostContext* context) = 0;
    virtual void shutdown() = 0;

    // UI
    virtual QUrl qmlComponent() const = 0;
    virtual QUrl iconSource() const = 0;
    virtual QUrl settingsComponent() const { return {}; }

    // Capabilities
    virtual QStringList requiredServices() const = 0;
};

} // namespace oap

#define OAP_PLUGIN_IID "org.openauto.PluginInterface/1.0"
Q_DECLARE_INTERFACE(oap::IPlugin, OAP_PLUGIN_IID)
```

**Step 2: Create IHostContext.hpp**

```cpp
#pragma once

#include <QString>

namespace oap {

class IAudioService;
class IBluetoothService;
class IConfigService;
class IThemeService;
class IDisplayService;
class IEventBus;

enum class LogLevel { Debug, Info, Warning, Error };

class IHostContext {
public:
    virtual ~IHostContext() = default;

    virtual IAudioService* audioService() = 0;
    virtual IBluetoothService* bluetoothService() = 0;
    virtual IConfigService* configService() = 0;
    virtual IThemeService* themeService() = 0;
    virtual IDisplayService* displayService() = 0;
    virtual IEventBus* eventBus() = 0;

    virtual void log(LogLevel level, const QString& message) = 0;
};

} // namespace oap
```

**Step 3: Build to verify**

Run: `cd build && cmake --build . -j$(nproc)`

**Step 4: Commit**

```bash
git add src/core/plugin/
git commit -m "feat: define IPlugin and IHostContext plugin boundary interfaces"
```

---

### Task 6: Create HostContext concrete implementation

**Files:**
- Create: `src/core/plugin/HostContext.hpp`
- Create: `src/core/plugin/HostContext.cpp`
- Modify: `src/CMakeLists.txt`

**Context:** The concrete `HostContext` holds pointers to all service implementations and is passed to plugins during init. For now, most services are nullptr stubs — they'll be implemented in later phases.

**Step 1: Implement HostContext**

```cpp
// HostContext.hpp
#pragma once

#include "IHostContext.hpp"
#include <boost/log/trivial.hpp>

namespace oap {

class HostContext : public IHostContext {
public:
    void setAudioService(IAudioService* svc) { audio_ = svc; }
    void setBluetoothService(IBluetoothService* svc) { bt_ = svc; }
    void setConfigService(IConfigService* svc) { config_ = svc; }
    void setThemeService(IThemeService* svc) { theme_ = svc; }
    void setDisplayService(IDisplayService* svc) { display_ = svc; }
    void setEventBus(IEventBus* bus) { eventBus_ = bus; }

    IAudioService* audioService() override { return audio_; }
    IBluetoothService* bluetoothService() override { return bt_; }
    IConfigService* configService() override { return config_; }
    IThemeService* themeService() override { return theme_; }
    IDisplayService* displayService() override { return display_; }
    IEventBus* eventBus() override { return eventBus_; }

    void log(LogLevel level, const QString& message) override;

private:
    IAudioService* audio_ = nullptr;
    IBluetoothService* bt_ = nullptr;
    IConfigService* config_ = nullptr;
    IThemeService* theme_ = nullptr;
    IDisplayService* display_ = nullptr;
    IEventBus* eventBus_ = nullptr;
};

} // namespace oap
```

**Step 2: Add to CMakeLists, build, commit**

```bash
git add src/core/plugin/HostContext.hpp src/core/plugin/HostContext.cpp src/CMakeLists.txt
git commit -m "feat: implement HostContext concrete service registry"
```

---

### Task 7: Implement ConfigService wrapping YamlConfig

**Files:**
- Create: `src/core/services/ConfigService.hpp`
- Create: `src/core/services/ConfigService.cpp`
- Create: `tests/test_config_service.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Context:** First concrete service implementation. Wraps `YamlConfig` behind the `IConfigService` interface with plugin-scoped access. The config file is owned by ConfigService (single writer rule from design doc Section 7).

**Step 1: Write failing test**

Test that ConfigService scopes plugin config correctly and only one writer exists.

**Step 2: Implement ConfigService**

- Constructor takes `YamlConfig*`
- `value(key)` reads from main config
- `pluginValue(pluginId, key)` reads from plugin-scoped section
- `setPluginValue(pluginId, key, value)` writes to plugin-scoped section
- `save()` flushes to disk

**Step 3: Build, test, commit**

```bash
git commit -m "feat: implement ConfigService wrapping YamlConfig"
```

---

## Phase C: Plugin Manager + Discovery

### Task 8: Create PluginManifest parser

**Files:**
- Create: `src/core/plugin/PluginManifest.hpp`
- Create: `src/core/plugin/PluginManifest.cpp`
- Create: `tests/test_plugin_manifest.cpp`
- Create: `tests/data/test_plugin.yaml`

**Context:** Parses `plugin.yaml` files from plugin directories. Design doc Section 4 defines the manifest schema.

**Step 1: Create test plugin manifest**

```yaml
# tests/data/test_plugin.yaml
id: org.test.example
name: "Test Plugin"
version: "1.0.0"
api_version: 1
type: full
description: "A test plugin"
author: "Test"
icon: icons/test.svg
requires:
  services:
    - AudioService >= 1.0
settings:
  - key: enabled
    type: bool
    default: true
    label: "Enabled"
nav_strip:
  order: 1
  visible: true
```

**Step 2: Write failing test, implement parser, test, commit**

PluginManifest stores: id, name, version, apiVersion, type (full/qml-only), requiredServices, settings schema, nav strip config.

```bash
git commit -m "feat: implement PluginManifest YAML parser"
```

---

### Task 9: Create PluginManager

**Files:**
- Create: `src/core/plugin/PluginManager.hpp`
- Create: `src/core/plugin/PluginManager.cpp`
- Create: `tests/test_plugin_manager.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Context:** Discovers, validates, loads, and manages plugin lifecycle. Design doc Section 4 defines the lifecycle: Discovered → Validated → Loaded → Initialized → Running → Shutdown.

**Responsibilities:**
- Scan `~/.openauto/plugins/` for directories containing `plugin.yaml`
- Parse manifests, check API version compatibility
- For full plugins: load `.so` via `QPluginLoader`
- For QML-only plugins: locate QML component path
- Call `initialize(IHostContext*)` on each loaded plugin
- Expose list of loaded plugins (for nav strip population)
- Handle failures gracefully (log and disable failed plugins)

**Step 1: Write failing tests**

Test discovery (finds plugins in a temp directory), validation (rejects wrong API version), loading (loads a test manifest), failure handling (bad manifest doesn't crash).

**Step 2: Implement PluginManager**

Key methods:
- `discoverPlugins(const QString& pluginsDir)`
- `loadPlugins(IHostContext* context)`
- `shutdownPlugins()`
- `QList<PluginManifest> loadedPlugins() const`
- `IPlugin* plugin(const QString& id) const`

**Step 3: Register static plugins**

Add a method `registerStaticPlugin(IPlugin* plugin)` for core plugins that are compiled into the binary. These skip the `.so` loading step but go through the same validation and initialization.

**Step 4: Build, test, commit**

```bash
git commit -m "feat: implement PluginManager with discovery and lifecycle"
```

---

### Task 10: Wire PluginManager into main.cpp

**Files:**
- Modify: `src/main.cpp`

**Context:** Create PluginManager, HostContext, and wire them together in main. For now, no plugins are registered yet — this just establishes the infrastructure.

**Step 1: Add PluginManager creation after config loading**

```cpp
auto hostContext = std::make_unique<oap::HostContext>();
auto configService = std::make_unique<oap::ConfigService>(yamlConfig.get());
hostContext->setConfigService(configService.get());

auto pluginManager = std::make_unique<oap::PluginManager>();
pluginManager->discoverPlugins(QDir::homePath() + "/.openauto/plugins");
pluginManager->loadPlugins(hostContext.get());
```

**Step 2: Build, verify, commit**

```bash
git commit -m "feat: wire PluginManager and HostContext into main"
```

---

## Phase D: QML Shell Refactor

### Task 11: Create QML shell layout (status bar + content area + nav strip)

**Files:**
- Create: `qml/Shell.qml`
- Modify: `qml/main.qml`

**Context:** Split the monolithic `main.qml` into a shell that provides the status bar, navigation strip, and a content area where plugin views load. The existing TopBar, BottomBar, and content switching logic move into the shell.

**Step 1: Create Shell.qml**

Shell.qml provides the three-zone layout from design doc Section 3:
- StatusBar (top 5-8%) — clock, BT/WiFi status, connection indicators
- ContentArea (middle 75-80%) — `Loader` that loads the active plugin's QML component
- NavStrip (bottom 12-15%) — `Row`/`ListView` of plugin tiles from PluginManager

The content area uses a `Loader` whose `source` property is bound to the active plugin's `qmlComponent()` URL.

**Step 2: Refactor main.qml**

Reduce main.qml to window setup + Shell loading. Move all layout logic into Shell.qml.

**Step 3: Build, verify UI looks the same, commit**

```bash
git commit -m "refactor: extract QML Shell layout from monolithic main.qml"
```

---

### Task 12: Create PluginModel for nav strip

**Files:**
- Create: `src/ui/PluginModel.hpp`
- Create: `src/ui/PluginModel.cpp`
- Modify: `src/CMakeLists.txt`

**Context:** A `QAbstractListModel` that exposes loaded plugins to QML for the nav strip. Roles: id, name, iconSource, qmlComponent, isActive.

**Step 1: Implement PluginModel**

- Backed by `PluginManager::loadedPlugins()`
- Roles: `PluginId`, `PluginName`, `PluginIcon`, `PluginQml`, `IsActive`
- Method `setActivePlugin(const QString& id)` — sets the active plugin, emits dataChanged

**Step 2: Build, commit**

```bash
git commit -m "feat: add PluginModel for QML nav strip binding"
```

---

### Task 13: Wire PluginModel to Shell nav strip

**Files:**
- Modify: `qml/Shell.qml`
- Modify: `src/main.cpp`

**Context:** The nav strip uses a `ListView` or `Repeater` bound to PluginModel. Tapping a tile sets the active plugin and updates the content Loader.

**Step 1: Expose PluginModel to QML context**

In main.cpp:
```cpp
auto pluginModel = new oap::PluginModel(pluginManager.get(), &app);
engine.rootContext()->setContextProperty("PluginModel", pluginModel);
```

**Step 2: Update Shell.qml nav strip**

```qml
// Nav strip
Row {
    Repeater {
        model: PluginModel
        delegate: Tile {
            iconSource: model.pluginIcon
            label: model.pluginName
            isActive: model.isActive
            onClicked: PluginModel.setActivePlugin(model.pluginId)
        }
    }
}

// Content area
Loader {
    id: contentLoader
    source: PluginModel.activePluginQml
}
```

**Step 3: Build, verify navigation works, commit**

```bash
git commit -m "feat: wire PluginModel to Shell nav strip for plugin navigation"
```

---

## Phase E: Wrap AA as Static Plugin

### Task 14: Create AndroidAutoPlugin implementing IPlugin

**Files:**
- Create: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Create: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/CMakeLists.txt`

**Context:** This is the critical migration step. The existing AA code (`AndroidAutoService`, `VideoService`, `VideoDecoder`, `EvdevTouchReader`, `TouchHandler`, `ServiceFactory`, `AndroidAutoEntity`, `BluetoothDiscoveryService`) gets wrapped inside an `AndroidAutoPlugin` that implements `IPlugin`.

**Step 1: Create AndroidAutoPlugin**

```cpp
class AndroidAutoPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)

public:
    QString id() const override { return "org.openauto.android-auto"; }
    QString name() const override { return "Android Auto"; }
    QString version() const override { return "1.0.0"; }
    int apiVersion() const override { return 1; }

    bool initialize(IHostContext* context) override;
    void shutdown() override;

    QUrl qmlComponent() const override;
    QUrl iconSource() const override;
    QStringList requiredServices() const override;

private:
    IHostContext* context_ = nullptr;
    std::unique_ptr<oap::aa::AndroidAutoService> aaService_;
    oap::aa::EvdevTouchReader* touchReader_ = nullptr;
};
```

**Step 2: Move AA initialization from main.cpp into plugin's initialize()**

The `initialize()` method does everything that main.cpp currently does:
- Creates `AndroidAutoService` (using config from `context->configService()`)
- Creates `EvdevTouchReader` if touch device exists
- Connects state change signals
- Starts the AA service

**Step 3: Move AA cleanup into plugin's shutdown()**

**Step 4: Build, verify AA still works, commit**

```bash
git commit -m "feat: wrap Android Auto code as static plugin (AndroidAutoPlugin)"
```

---

### Task 15: Register AndroidAutoPlugin as static plugin

**Files:**
- Modify: `src/main.cpp`

**Context:** Remove all AA-specific code from main.cpp. Register `AndroidAutoPlugin` with the PluginManager as a static plugin.

**Step 1: Clean up main.cpp**

Remove:
- `#include "core/aa/AndroidAutoService.hpp"`
- `#include "core/aa/EvdevTouchReader.hpp"`
- All AA-specific wiring (aaService creation, touchReader, connect signals)
- Context properties for VideoDecoder, TouchHandler

Replace with:
```cpp
auto aaPlugin = new oap::plugins::AndroidAutoPlugin(&app);
pluginManager->registerStaticPlugin(aaPlugin);
pluginManager->loadPlugins(hostContext.get());
```

**Step 2: Expose AA-specific context properties through the plugin**

The AndroidAutoPlugin needs to expose VideoDecoder and TouchHandler to its QML view. This goes through the plugin's QML context, not the global context. The Shell's content Loader should set up a plugin-specific context.

**Step 3: Build, verify AA still connects and displays video, commit**

```bash
git commit -m "refactor: remove AA-specific code from main.cpp, use static plugin"
```

---

### Task 16: Create AndroidAutoMenu.qml as plugin's QML view

**Files:**
- Move: `qml/applications/android_auto/AndroidAutoMenu.qml` → plugin's QML directory
- Modify: Plugin's `qmlComponent()` to return the new path

**Context:** The AA QML view belongs to the plugin, not the shell. When the plugin architecture is formalized, this file will live in the plugin's resource bundle.

**Step 1: Update qmlComponent() to return the AA view URL**

**Step 2: Verify AA fullscreen mode still works**

When AA is active, it should still take over the full screen (hiding status bar + nav strip). The Shell needs to respect the "dominant plugin" pattern from the design doc.

**Step 3: Build, verify, commit**

```bash
git commit -m "refactor: move AndroidAutoMenu.qml under AA plugin ownership"
```

---

## Phase F: Theme System

### Task 17: Implement ThemeService

**Files:**
- Create: `src/core/services/ThemeService.hpp`
- Create: `src/core/services/ThemeService.cpp`
- Create: `tests/test_theme_service.cpp`
- Create: `tests/data/themes/default/theme.yaml`
- Create: `tests/data/themes/default/colors.yaml`

**Context:** Loads theme directories (YAML color definitions, font config), exposes theme values as Q_PROPERTY for QML binding. Design doc Section 5.

**Step 1: Create default theme YAML files matching design doc color schema**

**Step 2: Write failing tests — load theme, read colors, verify inheritance**

**Step 3: Implement ThemeService as QObject with Q_PROPERTY for each color group**

**Step 4: Build, test, commit**

```bash
git commit -m "feat: implement ThemeService with YAML theme loading"
```

---

### Task 18: Wire ThemeService into HostContext and QML

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/core/plugin/HostContext.cpp`

**Step 1: Create ThemeService, register with HostContext, expose to QML**

**Step 2: Build, commit**

```bash
git commit -m "feat: wire ThemeService into HostContext and QML context"
```

---

### Task 19: Migrate QML from old ThemeController to new ThemeService

**Files:**
- Modify: All QML files that reference `ThemeController`

**Context:** Replace all `ThemeController.backgroundColor` etc. with `ThemeService.background.primary` (new theme property structure).

**Step 1: Search and replace theme bindings across QML files**

**Step 2: Remove old ThemeController once all references are updated**

**Step 3: Build, verify UI looks correct, commit**

```bash
git commit -m "refactor: migrate QML from ThemeController to ThemeService"
```

---

## Phase G: Audio Pipeline

### Task 20: Implement AudioService wrapping PipeWire

**Files:**
- Create: `src/core/services/AudioService.hpp`
- Create: `src/core/services/AudioService.cpp`
- Create: `tests/test_audio_service.cpp`
- Modify: `src/CMakeLists.txt`

**Context:** PipeWire owns mixing and routing. AudioService is a thin helper that creates PipeWire stream nodes for plugins. Design doc Section 6.

**Dependencies:** `libpipewire-0.3-dev` (apt install on both dev VM and Pi)

**Step 1: Add PipeWire dependency to CMake**

```cmake
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
```

**Step 2: Implement AudioService**

- `createStream()` creates a PipeWire stream node
- `destroyStream()` tears it down
- `setMasterVolume()` controls the PipeWire sink volume
- `requestAudioFocus()` / `releaseAudioFocus()` configure ducking policy

**Step 3: Build, test (at minimum: create/destroy stream without crash), commit**

```bash
git commit -m "feat: implement AudioService wrapping PipeWire"
```

---

### Task 21: Wire AA audio channels through AudioService

**Files:**
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Modify: `src/core/aa/ServiceFactory.cpp`

**Context:** The AA protocol has 3 audio channels: MEDIA_AUDIO (4), SPEECH_AUDIO (5), SYSTEM_AUDIO (6). Each becomes a PipeWire stream via AudioService.

**Step 1: In AndroidAutoPlugin::initialize(), create audio streams**

```cpp
auto* audio = context_->audioService();
mediaStream_ = audio->createStream("AA Media", 50, audioFormat);
navStream_ = audio->createStream("AA Navigation", 80, audioFormat);
phoneStream_ = audio->createStream("AA Phone", 90, audioFormat);
```

**Step 2: In ServiceFactory, wire audio data callbacks to write to PipeWire streams**

The audio service stubs currently discard audio data. Replace with actual PCM forwarding to the PipeWire streams.

**Step 3: Build, deploy to Pi, test with AA connected — verify audio plays**

```bash
git commit -m "feat: wire AA audio channels through PipeWire AudioService"
```

---

### Task 22: Implement audio focus routing

**Files:**
- Modify: `src/core/aa/AndroidAutoEntity.cpp`
- Modify: `src/core/services/AudioService.cpp`

**Context:** When AA requests audio focus (GAIN, GAIN_TRANSIENT, etc.), AudioService ducks or mutes lower-priority streams. Fix the always-GAIN response from protocol audit task 8.

**Step 1: Implement focus request handling in AudioService**

**Step 2: Fix AudioFocusRequest response in AndroidAutoEntity (mirror request type)**

**Step 3: Build, test, commit**

```bash
git commit -m "feat: implement audio focus routing with PipeWire ducking"
```

---

## Phase H: Bluetooth Audio Plugin

### Task 23: Create BtAudioPlugin skeleton

**Files:**
- Create: `src/plugins/bt_audio/BtAudioPlugin.hpp`
- Create: `src/plugins/bt_audio/BtAudioPlugin.cpp`
- Create: `src/plugins/bt_audio/qml/BtAudioView.qml`
- Modify: `src/CMakeLists.txt`

**Context:** A2DP sink — the Pi acts as a Bluetooth speaker. Uses BlueZ D-Bus API for A2DP profile registration and PipeWire for audio output.

**Step 1: Create plugin skeleton implementing IPlugin**

**Step 2: Register as static plugin in main.cpp**

**Step 3: Create basic QML view (album art placeholder, play/pause, track info)**

**Step 4: Build, commit**

```bash
git commit -m "feat: add BtAudioPlugin skeleton (A2DP sink)"
```

---

### Task 24: Implement A2DP sink via BlueZ D-Bus

**Files:**
- Modify: `src/plugins/bt_audio/BtAudioPlugin.cpp`

**Context:** Register as A2DP sink with BlueZ via D-Bus. Receive audio stream, forward to PipeWire via AudioService.

**Step 1: Register A2DP Sink endpoint with BlueZ via D-Bus**

**Step 2: Handle incoming audio data, decode SBC/AAC, write to PipeWire stream**

**Step 3: Test with phone playing music over Bluetooth**

```bash
git commit -m "feat: implement A2DP sink via BlueZ D-Bus"
```

---

### Task 25: Implement BT audio metadata (AVRCP)

**Files:**
- Modify: `src/plugins/bt_audio/BtAudioPlugin.cpp`
- Modify: `src/plugins/bt_audio/qml/BtAudioView.qml`

**Context:** AVRCP profile provides track metadata (title, artist, album art) and playback control (play, pause, next, prev).

**Step 1: Subscribe to AVRCP metadata via BlueZ D-Bus**

**Step 2: Expose metadata to QML, update BtAudioView**

**Step 3: Implement playback controls (play/pause/next/prev via AVRCP commands)**

```bash
git commit -m "feat: add AVRCP metadata and playback controls to BT Audio plugin"
```

---

## Phase I: Phone Plugin

### Task 26: Create PhonePlugin skeleton

**Files:**
- Create: `src/plugins/phone/PhonePlugin.hpp`
- Create: `src/plugins/phone/PhonePlugin.cpp`
- Create: `src/plugins/phone/qml/PhoneView.qml`
- Modify: `src/CMakeLists.txt`

**Context:** BT HFP (Hands-Free Profile) for making and receiving calls. Uses ofono or BlueZ directly.

**Step 1: Create plugin skeleton, register as static plugin**

**Step 2: Create dialer QML view (number pad, call/hangup buttons, contact list placeholder)**

```bash
git commit -m "feat: add PhonePlugin skeleton (HFP dialer)"
```

---

### Task 27: Implement HFP profile via ofono

**Files:**
- Modify: `src/plugins/phone/PhonePlugin.cpp`

**Context:** ofono manages HFP connections. We interact via D-Bus to make/receive calls, send/receive audio.

**Step 1: Connect to ofono D-Bus service**

**Step 2: Implement call lifecycle (dial, answer, hangup)**

**Step 3: Route call audio through PipeWire (high priority — mutes all other audio)**

```bash
git commit -m "feat: implement HFP phone calls via ofono"
```

---

### Task 28: Implement incoming call notification

**Files:**
- Modify: `src/plugins/phone/PhonePlugin.cpp`
- Create: `src/plugins/phone/qml/IncomingCallOverlay.qml`

**Context:** When a call comes in, show an overlay regardless of which plugin is active (including during AA). Uses EventBus to signal the shell.

**Step 1: Publish incoming call event via EventBus**

**Step 2: Shell listens for incoming call, shows overlay**

**Step 3: Accept/reject from overlay, route to PhonePlugin**

```bash
git commit -m "feat: add incoming call overlay notification"
```

---

## Phase J: Web Config Panel

### Task 29: Create web config server skeleton

**Files:**
- Create: `web-config/` directory
- Create: `web-config/server.py` (Flask-based)
- Create: `web-config/requirements.txt`
- Create: `web-config/templates/index.html`

**Context:** Lightweight web server accessible at `http://10.0.0.1:8080`. Communicates with the Qt app via Unix socket IPC. Design doc Section 9.

**Step 1: Set up Flask app with basic routes**

Routes: `/` (dashboard), `/settings` (main config), `/themes` (theme editor), `/plugins` (plugin management)

**Step 2: Create IPC client for communication with Qt app**

The web server sends JSON change requests to a Unix socket. The Qt app validates and writes config (single writer rule).

**Step 3: Test locally, commit**

```bash
git commit -m "feat: add web config panel skeleton (Flask)"
```

---

### Task 30: Add Unix socket IPC to main app

**Files:**
- Create: `src/core/services/IpcServer.hpp`
- Create: `src/core/services/IpcServer.cpp`

**Context:** Qt app listens on a Unix domain socket for config change requests from the web server.

**Step 1: Implement QLocalServer-based IPC**

**Step 2: Handle incoming JSON requests (get config, set config, list plugins, etc.)**

**Step 3: Build, test with web server, commit**

```bash
git commit -m "feat: add Unix socket IPC server for web config panel"
```

---

### Task 31: Build web config UI

**Files:**
- Modify: `web-config/templates/` (HTML/CSS/JS)

**Context:** Settings forms, theme color editor, plugin enable/disable toggles. Auto-generates plugin settings from manifest schemas.

**Step 1: Implement settings page with forms for all config.yaml sections**

**Step 2: Implement theme editor with color pickers**

**Step 3: Implement plugin management page**

**Step 4: Add QR code display on touchscreen for easy phone access**

```bash
git commit -m "feat: build web config panel UI"
```

---

## Phase K: 3-Finger Gesture Overlay

### Task 32: Add gesture detection to EvdevTouchReader

**Files:**
- Modify: `src/core/aa/EvdevTouchReader.cpp`
- Modify: `src/core/aa/EvdevTouchReader.hpp`

**Context:** Detect 3 simultaneous touch-down events within a 200ms window. These touches are NOT forwarded to AA. Instead, emit a signal to show the overlay. Design doc Section 8.

**Step 1: Track simultaneous touch count in slot state machine**

**Step 2: When 3 fingers detected within window, emit gestureDetected signal**

**Step 3: Suppress those touches from being forwarded to AA**

```bash
git commit -m "feat: detect 3-finger tap gesture in EvdevTouchReader"
```

---

### Task 33: Create gesture overlay QML component

**Files:**
- Create: `qml/components/GestureOverlay.qml`
- Modify: `qml/Shell.qml`

**Context:** Translucent overlay with volume, brightness, home, and dismiss controls. Shown over AA video when 3-finger tap detected. Design doc Section 8.

**Step 1: Create overlay QML with controls**

**Step 2: Wire gesture signal from EvdevTouchReader to Shell overlay visibility**

**Step 3: Implement auto-dismiss (5 second timeout or tap outside)**

**Step 4: Build, deploy, test on Pi with AA connected**

```bash
git commit -m "feat: add 3-finger tap gesture overlay for AA quick controls"
```

---

## Phase L: Install Script + Polish

### Task 34: Create interactive install script

**Files:**
- Create: `install.sh`

**Context:** Runs on stock RPi OS Trixie. Installs dependencies, builds from source, configures system services, generates initial config. Design doc Section 10.

**Step 1: Write install script**

Script flow:
1. Check RPi OS version and architecture
2. Install apt dependencies
3. Interactive hardware setup (display, touch, WiFi AP, audio)
4. Clone and build from source
5. Create systemd service
6. Configure labwc
7. Generate `~/.openauto/config.yaml`
8. Print completion summary

**Step 2: Test on fresh RPi OS Trixie install**

```bash
git commit -m "feat: add interactive install script for RPi OS Trixie"
```

---

### Task 35: Final integration testing and CLAUDE.md update

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/development.md`

**Context:** Update all project documentation to reflect the new architecture. Test the full lifecycle: install script → boot → BT discovery → AA connect → 3-finger overlay → settings → theme change → BT audio → phone call.

**Step 1: Update CLAUDE.md with new architecture, file paths, build commands**

**Step 2: Update development.md with new dependency list and project structure**

**Step 3: Full integration test on Pi hardware**

**Step 4: Tag v0.2.0 release (architecture milestone)**

```bash
git commit -m "docs: update project documentation for plugin architecture"
git tag -a v0.2.0 -m "Architecture migration: plugin system, themes, audio, BT, phone"
```

---

## Task Dependency Graph

```
Phase A (YAML Config)
  └─→ Phase B (Service Interfaces)
        ├─→ Phase C (Plugin Manager) ─────────┐
        ├─→ Phase D (QML Shell) ──────────────┤
        │                                      ├─→ Phase E (AA Plugin)
        └─→ Phase G (Audio/PipeWire)           │     └─→ Phase K (Gestures)
              ├─→ Phase H (BT Audio) ◄─── Phase C
              └─→ Phase I (Phone) ◄──── Phase C
  └─→ Phase J (Web Config) ← independent after Phase A
                                               └─→ Phase L (Install + Polish)
```

**Estimated total: ~35 tasks across 12 phases.**

Phases A-E are the critical path (config + interfaces + plugins + shell + AA wrapping). This is where the most risk lives. Phases F-K can be parallelized once the plugin infrastructure is in place. Phase L is the capstone.
