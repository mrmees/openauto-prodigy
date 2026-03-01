# Testing Patterns

**Analysis Date:** 2025-03-01

## Test Framework

**Runner:**
- Qt Test framework (`QtTest`)
- Config: `tests/CMakeLists.txt` defines test setup
- Run all tests: `ctest --output-on-failure` from `build/` directory
- Run single test: `./build/test_<name>`

**Assertion Library:**
- Qt Test macros: `QCOMPARE()`, `QVERIFY()`, `QSKIP()`, `QTEST_MAIN()`

**Signals and spies:**
- `QSignalSpy` for testing signal emission and parameters
- Example: `QSignalSpy spy(&mgr, &oap::PluginManager::pluginLoaded);`

**Test execution:**
```bash
mkdir build && cd build && cmake .. && make -j$(nproc)
ctest --output-on-failure  # Run all 47 tests with failures visible
ctest --verbose            # Detailed output per test
ctest -R test_yaml_config  # Run by regex filter
```

## Test File Organization

**Location:**
- Co-located in `tests/` directory (NOT alongside source files)
- One test file per logical component

**Naming:**
- `test_<component>.cpp` pattern
- Examples: `test_yaml_config.cpp`, `test_plugin_manager.cpp`, `test_audio_service.cpp`

**Directory structure:**
```
tests/
├── CMakeLists.txt          # Test registry + oap_add_test() helper
├── test_*.cpp              # 40+ test files
├── data/                   # Test fixtures
│   ├── test_config.yaml
│   ├── test_config.ini
│   ├── test_plugin.yaml
│   └── themes/default/theme.yaml
└── test_*.py               # Python script tests (2 files)
```

## Test Structure

**Suite organization:**
Each test file is a single `QObject` subclass with private test slots:

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
    // ... more tests
};

void TestYamlConfig::testLoadDefaults()
{
    oap::YamlConfig config;
    QCOMPARE(config.hardwareProfile(), QString("rpi4"));
    QCOMPARE(config.displayBrightness(), 80);
}

QTEST_MAIN(TestYamlConfig)
#include "test_yaml_config.moc"
```

**Key patterns:**
- Test methods are **always private slots** (Qt meta-object requirement)
- **One assertion focus per test** (e.g., `testLoadDefaults` tests all defaults, `testPluginScoping` tests namespace isolation)
- **No setup/teardown** (Qt slots `initTestCase`, `cleanupTestCase` not used; state created/destroyed in each test method)
- **Inline objects** as test subjects (avoid shared state)

## Mocking

**Framework:** Hand-rolled mock classes extending QObject + target interface

**Mock plugin example** (`test_plugin_manager.cpp`):
```cpp
class MockPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)
public:
    QString id_ = "org.test.mock";
    bool initResult_ = true;
    bool initialized_ = false;
    bool shutDown_ = false;

    QString id() const override { return id_; }
    QString name() const override { return "Mock"; }
    // ... other interface methods

    bool initialize(oap::IHostContext*) override {
        initialized_ = true;
        return initResult_;
    }
    void shutdown() override { shutDown_ = true; }
};
```

**Mock host context:**
```cpp
class MockHostContext : public oap::IHostContext {
public:
    oap::IAudioService* audioService() override { return nullptr; }
    oap::IBluetoothService* bluetoothService() override { return nullptr; }
    // ... all other services return nullptr for tests that don't need them
    void log(oap::LogLevel, const QString&) override {}
};
```

**What to mock:**
- Plugin interfaces (`IPlugin`)
- Host context for service tests (provides null/stub implementations)
- External services that have side effects (e.g., D-Bus, PipeWire)

**What NOT to mock:**
- Config classes (`YamlConfig`) — test real YAML parsing with fixtures
- Model classes (`PluginModel`) — test actual QAbstractListModel behavior
- Event buses and signals — test actual signal routing with `QSignalSpy`

## Fixtures and Factories

**Test data location:** `tests/data/`

**Fixtures:**
```
tests/data/
├── test_config.yaml        # Full config with test values (SSID=TestSSID, volume=75)
├── test_config.ini         # Legacy INI format for Configuration class
├── test_plugin.yaml        # Sample plugin manifest
└── themes/
    └── default/
        └── theme.yaml      # Color palette test data
```

**How fixtures are loaded:**
CMake copies fixtures at build time via `configure_file()`:
```cmake
oap_add_test(test_yaml_config
    SOURCES test_yaml_config.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
configure_file(data/test_config.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_config.yaml COPYONLY)
```

**Test code accesses fixtures:**
```cpp
void TestYamlConfig::testLoadFromFile()
{
    oap::YamlConfig config;
    config.load(QString(TEST_DATA_DIR) + "/test_config.yaml");
    QCOMPARE(config.wifiSsid(), QString("TestSSID"));
}
```

**Fixture content examples:**

`test_config.yaml`:
```yaml
hardware_profile: rpi4
display:
  brightness: 80
audio:
  master_volume: 75
identity:
  head_unit_name: "Test Unit"
plugins:
  enabled:
    - org.openauto.android-auto
    - org.openauto.bt-audio
```

**Factories (if needed):**
Not observed in codebase. Tests create objects inline. For complex setup, use helper functions within the test class.

## Coverage

**Requirements:** Not enforced (no CI/CMake config found enforcing coverage targets)

**View coverage:** Not detected (no gcov/lcov setup in CMakeLists.txt)

**Gap estimation (by inspection):**
- Core services: ~90% covered (`ConfigService`, `YamlConfig`, `PluginManager`, `AudioService`)
- Protocol handlers: ~80% covered (input, audio, video channels tested)
- UI controllers: ~50% covered (basic QML/model tests, integration gaps)
- Error paths: ~70% covered (exception handling in plugins, null safety)

## Test Types

**Unit tests (majority: ~40/47):**
- Scope: Single class or small component group
- Mocks: External dependencies (services, filesystem, PipeWire)
- Examples: `test_yaml_config.cpp`, `test_plugin_manifest.cpp`, `test_audio_service.cpp`

**Integration tests (minority: ~5/47):**
- Scope: Multiple classes interacting
- Mocks: Only external services (PipeWire, BlueZ D-Bus)
- Examples:
  - `test_plugin_manager.cpp`: Full plugin lifecycle (register → initialize → activate)
  - `test_oaa_integration.cpp`: AA protocol handlers working together
  - `test_aa_orchestrator.cpp`: Full AA session orchestration with real YamlConfig

**GUI/QML tests (2-3):**
- Scope: QML models and controllers
- Examples: `test_plugin_model.cpp` (PluginModel QAbstractListModel behavior)
- Special setup: Use `QT_QPA_PLATFORM=offscreen` in CMake test properties (see `tests/CMakeLists.txt` line 98)

**E2E/Script tests (2):**
- Not C++ unit tests — Python validation scripts
- Examples:
  - `test_prebuilt_release_package.py`: Verify built binary works
  - `test_install_list_prebuilt.py`: Validate installer script

## Common Patterns

**Async testing (none observed):**
- No `QTest::qWait()` usage (all tests are synchronous)
- Signals tested with `QSignalSpy` for validation, not waiting

**Error testing:**
Negative case testing — verify graceful failure:

```cpp
void TestAudioService::createStreamReturnsNullWithoutDaemon()
{
    oap::AudioService service;
    if (!service.isAvailable()) {
        auto* handle = service.createStream("test", 50);
        QVERIFY(handle == nullptr);  // Should return nullptr gracefully
    } else {
        QSKIP("PipeWire daemon is running — skip no-daemon test");
    }
}

void TestAudioService::writeToNullHandleReturnsMinus1()
{
    oap::AudioService service;
    uint8_t data[] = {0, 0, 0, 0};
    QCOMPARE(service.writeAudio(nullptr, data, 4), -1);
}
```

**Signal testing with spies:**
```cpp
void TestPluginManager::testRegisterStaticPlugin()
{
    MockPlugin plugin;
    oap::PluginManager mgr;

    QSignalSpy spy(&mgr, &oap::PluginManager::pluginLoaded);
    mgr.registerStaticPlugin(&plugin);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy[0][0].toString(), QString("org.test.mock"));
}
```

**Conditional tests (QSKIP):**
Used to skip tests when external resources unavailable:

```cpp
void TestAudioService::createAndDestroyWithDaemon()
{
    oap::AudioService service;
    if (!service.isAvailable()) {
        QSKIP("PipeWire daemon not available");
    }
    // Test PipeWire-specific code only if daemon is running
}
```

**Version-gated tests:**
```cpp
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
void TestVideoFramePool::testRecycledFrameIsValid() { /* ... */ }
#endif
```

## Test Helper Macro

**`oap_add_test()` CMake function** (`tests/CMakeLists.txt` lines 4-12):

```cmake
function(oap_add_test NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;EXTRA_LIBS;DEFS" ${ARGN})
    add_executable(${NAME} ${ARG_SOURCES})
    target_link_libraries(${NAME} PRIVATE openauto-core Qt6::Test ${ARG_EXTRA_LIBS})
    if(ARG_DEFS)
        target_compile_definitions(${NAME} PRIVATE ${ARG_DEFS})
    endif()
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()
```

**Usage:**
```cmake
oap_add_test(test_yaml_config
    SOURCES test_yaml_config.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
```

Eliminates boilerplate:
- All tests link `openauto-core` (static library with all app code)
- All tests link `Qt6::Test`
- Tests can define custom compile flags/defines without repetition

## Test Organization in CMakeLists.txt

Tests are grouped by category for clarity:

```cmake
# --- Core tests ---
oap_add_test(test_configuration ...)
oap_add_test(test_yaml_config ...)

# --- Plugin tests ---
oap_add_test(test_plugin_manifest ...)
oap_add_test(test_plugin_manager ...)

# --- Service tests ---
oap_add_test(test_theme_service ...)
oap_add_test(test_audio_service ...)

# --- Device / hardware tests ---
oap_add_test(test_night_mode ...)
oap_add_test(test_input_device_scanner ...)

# --- AA protocol tests ---
oap_add_test(test_wifi_channel_handler ...)
oap_add_test(test_video_channel_handler ...)
```

---

*Testing analysis: 2025-03-01*
