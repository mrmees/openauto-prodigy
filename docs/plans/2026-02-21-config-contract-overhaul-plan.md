# Config Contract Overhaul — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Eliminate config drift between YamlConfig, ConfigService, installer, and docs so every config key the system writes is honored at runtime.

**Architecture:** Replace ConfigService's manual 9-key mapping table with generic dot-path traversal inside YamlConfig (`valueByPath()`/`setValueByPath()`). Core AA code keeps typed accessors. Align defaults across runtime/installer/docs. Implement the stubbed IPC theme endpoint. Add coverage tests.

**Tech Stack:** C++17, Qt 6, yaml-cpp, CMake, CTest

**Review process:** After each task, submit the diff to Codex for review before proceeding.

---

### Task 1: Add valueByPath() and setValueByPath() to YamlConfig

**Files:**
- Modify: `src/core/YamlConfig.hpp`
- Modify: `src/core/YamlConfig.cpp`
- Test: `tests/test_yaml_config.cpp`

**Step 1: Write the failing tests**

Add these test methods to `tests/test_yaml_config.cpp`:

```cpp
void TestYamlConfig::testValueByPath();
void TestYamlConfig::testValueByPathNested();
void TestYamlConfig::testValueByPathMissing();
void TestYamlConfig::testSetValueByPath();
void TestYamlConfig::testSetValueByPathRejectsUnknown();
```

In the class declaration, add the slot declarations. Then implement:

```cpp
void TestYamlConfig::testValueByPath()
{
    oap::YamlConfig config;
    // Top-level key
    QCOMPARE(config.valueByPath("hardware_profile").toString(), QString("rpi4"));
    // Nested key — use CURRENT defaults (5288/60), Task 3 will change these
    QCOMPARE(config.valueByPath("connection.tcp_port").toInt(), 5288);
    QCOMPARE(config.valueByPath("video.fps").toInt(), 60);
}

void TestYamlConfig::testValueByPathNested()
{
    oap::YamlConfig config;
    // Deeply nested — use CURRENT defaults, Task 3 will change password
    QCOMPARE(config.valueByPath("connection.wifi_ap.ssid").toString(), QString("OpenAutoProdigy"));
    QCOMPARE(config.valueByPath("connection.wifi_ap.password").toString(), QString("changeme123"));
    QCOMPARE(config.valueByPath("sensors.night_mode.source").toString(), QString("time"));
    QCOMPARE(config.valueByPath("sensors.gps.enabled").toBool(), true);
}

void TestYamlConfig::testValueByPathMissing()
{
    oap::YamlConfig config;
    // Non-existent key returns invalid QVariant
    QVERIFY(!config.valueByPath("nonexistent").isValid());
    QVERIFY(!config.valueByPath("connection.nonexistent").isValid());
    QVERIFY(!config.valueByPath("").isValid());
}

void TestYamlConfig::testSetValueByPath()
{
    oap::YamlConfig config;
    QVERIFY(config.setValueByPath("connection.tcp_port", 9999));
    QCOMPARE(config.tcpPort(), static_cast<uint16_t>(9999));
    QCOMPARE(config.valueByPath("connection.tcp_port").toInt(), 9999);

    QVERIFY(config.setValueByPath("connection.wifi_ap.ssid", QString("NewSSID")));
    QCOMPARE(config.wifiSsid(), QString("NewSSID"));
}

void TestYamlConfig::testSetValueByPathRejectsUnknown()
{
    oap::YamlConfig config;
    // Unknown paths should be rejected
    QVERIFY(!config.setValueByPath("bogus.key", 42));
    QVERIFY(!config.setValueByPath("connection.bogus", 42));
    // Value should not have been created
    QVERIFY(!config.valueByPath("bogus.key").isValid());
}
```

**Step 2: Run tests to verify they fail**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && cmake --build . -j$(nproc) 2>&1 | tail -5`
Expected: Compile error — `valueByPath` and `setValueByPath` don't exist yet.

**Step 3: Implement valueByPath() and setValueByPath()**

In `src/core/YamlConfig.hpp`, add to the public section:

```cpp
    /// Generic dot-path config access for ConfigService.
    /// Splits key on '.' and walks the YAML tree.
    /// Returns invalid QVariant if path doesn't exist.
    QVariant valueByPath(const QString& dottedKey) const;

    /// Generic dot-path config write.
    /// Only writes to paths that exist in the DEFAULT config schema (rejects typos/unknown keys).
    /// Returns true if the path existed and was set, false otherwise.
    bool setValueByPath(const QString& dottedKey, const QVariant& value);
```

And add a private static helper:

```cpp
private:
    /// Build an immutable defaults tree for path validation.
    /// Used by setValueByPath() to reject writes to unknown paths.
    static YAML::Node buildDefaultsNode();
```

In `src/core/YamlConfig.cpp`, add a static helper and the two methods. Place them after the `setPluginValue()` method:

```cpp
// --- Generic dot-path access ---

static QVariant yamlScalarToVariant(const YAML::Node& node)
{
    if (!node.IsScalar()) return {};

    const std::string s = node.Scalar();

    // Bool detection (yaml-cpp normalizes to "true"/"false")
    if (s == "true") return QVariant(true);
    if (s == "false") return QVariant(false);

    // Int
    bool intOk = false;
    int i = QString::fromStdString(s).toInt(&intOk);
    if (intOk) return QVariant(i);

    // Double (but not if it was already a valid int)
    bool dblOk = false;
    double d = QString::fromStdString(s).toDouble(&dblOk);
    if (dblOk) return QVariant(d);

    // String fallback
    return QVariant(QString::fromStdString(s));
}

QVariant YamlConfig::valueByPath(const QString& dottedKey) const
{
    if (dottedKey.isEmpty()) return {};

    QStringList parts = dottedKey.split('.');

    // Use YAML::Clone to prevent shared-handle mutation on read
    YAML::Node node = YAML::Clone(root_);
    for (const auto& part : parts) {
        if (!node.IsMap()) return {};
        node.reset(node[part.toStdString()]);
        if (!node.IsDefined() || node.IsNull()) return {};
    }

    return yamlScalarToVariant(node);
}

// Build a fresh defaults tree for schema validation (not the merged runtime tree)
YAML::Node YamlConfig::buildDefaultsNode()
{
    YamlConfig tmp;
    return YAML::Clone(tmp.root_);
}

bool YamlConfig::setValueByPath(const QString& dottedKey, const QVariant& value)
{
    if (dottedKey.isEmpty()) return false;

    QStringList parts = dottedKey.split('.');

    // Validate against DEFAULTS tree, not merged root_ (which may contain user garbage)
    {
        YAML::Node defaults = buildDefaultsNode();
        for (const auto& part : parts) {
            if (!defaults.IsMap()) return false;
            defaults.reset(defaults[part.toStdString()]);
            if (!defaults.IsDefined()) return false;
        }
    }

    // Path exists in schema — navigate the real tree and set the value
    YAML::Node node = root_;
    for (int i = 0; i < parts.size() - 1; ++i) {
        node.reset(node[parts[i].toStdString()]);
    }

    std::string leafKey = parts.last().toStdString();
    switch (value.typeId()) {
    case QMetaType::Bool:
        node[leafKey] = value.toBool();
        break;
    case QMetaType::Int:
        node[leafKey] = value.toInt();
        break;
    case QMetaType::Double:
    case QMetaType::Float:
        node[leafKey] = value.toDouble();
        break;
    default:
        node[leafKey] = value.toString().toStdString();
        break;
    }

    return true;
}
```

**Step 4: Run tests to verify they pass**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`
Expected: All tests pass including new valueByPath tests.

**Step 5: Submit to Codex for review**

Ask Codex to review the valueByPath/setValueByPath implementation, specifically:
- Is the YAML::Clone approach correct for const-safety?
- Does node.reset() work as expected for traversal?
- Any edge cases with the type detection?

**Step 6: Commit**

```bash
git add src/core/YamlConfig.hpp src/core/YamlConfig.cpp tests/test_yaml_config.cpp
git commit -m "feat: add generic dot-path config access (valueByPath/setValueByPath)"
```

---

### Task 2: Replace ConfigService Manual Mapping with Generic Traversal

**Files:**
- Modify: `src/core/services/ConfigService.cpp`
- Modify: `tests/test_config_service.cpp`

**Step 1: Update test expectations**

The existing test `testReadTopLevelValues` already tests `value("display.brightness")`, `value("audio.master_volume")`, etc. These must continue to pass.

Add a new test for keys that were previously unmapped:

```cpp
void TestConfigService::testPreviouslyUnmappedKeys();
```

Add to slot declarations and implement:

```cpp
void TestConfigService::testPreviouslyUnmappedKeys()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_cs.yaml");

    // These keys were previously silently ignored
    QCOMPARE(svc.value("display.brightness").toInt(), 80);
    QCOMPARE(svc.value("connection.wifi_ap.channel").toInt(), 36);
    QCOMPARE(svc.value("sensors.night_mode.source").toString(), QString("time"));
    QCOMPARE(svc.value("identity.head_unit_name").toString(), QString("OpenAuto Prodigy"));
    QCOMPARE(svc.value("video.resolution").toString(), QString("720p"));
    QCOMPARE(svc.value("video.dpi").toInt(), 140);
}
```

**Step 2: Run the new test to verify it fails**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && QT_QPA_PLATFORM=offscreen ctest -R test_config_service --output-on-failure`
Expected: `testPreviouslyUnmappedKeys` FAILS — `connection.wifi_ap.channel` etc. return invalid QVariant.

**Step 3: Replace ConfigService implementation**

Replace `ConfigService::value()` and `ConfigService::setValue()` in `src/core/services/ConfigService.cpp`:

```cpp
QVariant ConfigService::value(const QString& key) const
{
    return config_->valueByPath(key);
}

void ConfigService::setValue(const QString& key, const QVariant& val)
{
    config_->setValueByPath(key, val);
}
```

That's it. The entire if/else chain is replaced by two one-liners.

**Step 4: Run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`
Expected: ALL tests pass — existing tests validate the same behavior, new test validates previously-broken keys.

**Step 5: Submit to Codex for review**

Ask Codex to verify the ConfigService change is backward-compatible and all existing tests still pass.

**Step 6: Commit**

```bash
git add src/core/services/ConfigService.cpp tests/test_config_service.cpp
git commit -m "refactor: replace ConfigService manual key mapping with generic traversal"
```

---

### Task 3: Align Default Values

**Files:**
- Modify: `src/core/YamlConfig.cpp` (3 lines)
- Modify: `tests/test_yaml_config.cpp` (update expected values)
- Modify: `tests/test_config_service.cpp` (update expected values)
- Modify: `install.sh` (1 line)

**Step 1: Update defaults in YamlConfig::initDefaults()**

In `src/core/YamlConfig.cpp`, change:

```cpp
// Line 30: tcp_port
root_["connection"]["tcp_port"] = 5277;  // was 5288

// Line 37: fps
root_["video"]["fps"] = 30;  // was 60

// Line 27: password
root_["connection"]["wifi_ap"]["password"] = "prodigy";  // was "changeme123"
```

**Step 2: Update tests that check default values**

In `tests/test_yaml_config.cpp`, update `testLoadDefaults()`:
```cpp
QCOMPARE(config.tcpPort(), static_cast<uint16_t>(5277));  // was 5288
QCOMPARE(config.videoFps(), 30);  // was 60
```

In `tests/test_yaml_config.cpp`, also update `testValueByPath()` and `testValueByPathNested()` (added in Task 1 with old defaults):
```cpp
// testValueByPath:
QCOMPARE(config.valueByPath("connection.tcp_port").toInt(), 5277);  // was 5288
QCOMPARE(config.valueByPath("video.fps").toInt(), 30);  // was 60

// testValueByPathNested:
QCOMPARE(config.valueByPath("connection.wifi_ap.password").toString(), QString("prodigy"));  // was "changeme123"
```

In `tests/test_config_service.cpp`, update `testReadTopLevelValues()`:
```cpp
QCOMPARE(svc.value("video.fps").toInt(), 30);  // was 60
```

And update `testPreviouslyUnmappedKeys()` if any values changed.

**Step 3: Update installer password default**

In `install.sh`, line 163, change:
```bash
read -p "WiFi hotspot password [prodigy]: " WIFI_PASS
WIFI_PASS=${WIFI_PASS:-prodigy}
```

**Step 4: Run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`
Expected: All tests pass with new default values.

**Step 5: Submit to Codex for review**

Ask Codex to verify defaults are now consistent between YamlConfig, installer, and test expectations.

**Step 6: Commit**

```bash
git add src/core/YamlConfig.cpp tests/test_yaml_config.cpp tests/test_config_service.cpp install.sh
git commit -m "fix: align config defaults (port 5277, fps 30, password prodigy)"
```

---

### Task 4: Implement IPC Theme Endpoint

**Files:**
- Modify: `src/core/services/ThemeService.hpp` (add read-only color map accessors)
- Modify: `src/core/services/IpcServer.cpp` (two methods: handleGetTheme + handleSetTheme)

**Step 0: Add read-only color map accessors to ThemeService**

In `src/core/services/ThemeService.hpp`, add to the public section:

```cpp
    /// Read-only access to color maps (for IPC export without signal side-effects)
    const QMap<QString, QColor>& dayColors() const { return dayColors_; }
    const QMap<QString, QColor>& nightColors() const { return nightColors_; }
```

This avoids the need to toggle nightMode (which emits signals) just to read colors.

**Step 1: Implement handleGetTheme()**

Replace the placeholder in `IpcServer::handleGetTheme()`:

```cpp
QByteArray IpcServer::handleGetTheme()
{
    if (!themeService_) return R"({"error":"Theme service not available"})";

    QJsonObject obj;
    obj["id"] = themeService_->currentThemeId();
    obj["font_family"] = themeService_->fontFamily();
    obj["night_mode"] = themeService_->nightMode();

    // Read color maps directly — no mode toggling, no signal emission
    QJsonObject dayObj;
    for (auto it = themeService_->dayColors().begin(); it != themeService_->dayColors().end(); ++it) {
        dayObj[it.key()] = it.value().name();
    }

    QJsonObject nightObj;
    for (auto it = themeService_->nightColors().begin(); it != themeService_->nightColors().end(); ++it) {
        nightObj[it.key()] = it.value().name();
    }

    obj["day"] = dayObj;
    obj["night"] = nightObj;

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}
```

**Step 2: Implement handleSetTheme()**

Replace the TODO stub:

```cpp
QByteArray IpcServer::handleSetTheme(const QVariantMap& data)
{
    if (!themeService_) return R"({"ok":false,"error":"Theme service not available"})";

    // Determine theme directory (with path traversal protection)
    QString themeId = data.value("id", "default").toString();
    static QRegularExpression validId("^[A-Za-z0-9._-]{1,64}$");
    if (!validId.match(themeId).hasMatch())
        return R"({"ok":false,"error":"Invalid theme ID"})";
    QString themeDir = QDir::homePath() + "/.openauto/themes/" + themeId;
    QDir().mkpath(themeDir);
    QString yamlPath = themeDir + "/theme.yaml";

    // Build YAML content
    YAML::Node root;
    root["id"] = themeId.toStdString();
    root["name"] = data.value("name", themeId).toString().toStdString();
    if (data.contains("font_family"))
        root["font_family"] = data.value("font_family").toString().toStdString();

    // Day colors
    QVariantMap dayColors = data.value("day").toMap();
    for (auto it = dayColors.begin(); it != dayColors.end(); ++it) {
        root["day"][it.key().toStdString()] = it.value().toString().toStdString();
    }

    // Night colors
    QVariantMap nightColors = data.value("night").toMap();
    for (auto it = nightColors.begin(); it != nightColors.end(); ++it) {
        root["night"][it.key().toStdString()] = it.value().toString().toStdString();
    }

    // Write YAML file
    std::ofstream fout(yamlPath.toStdString());
    if (!fout.is_open())
        return R"({"ok":false,"error":"Cannot write theme file"})";
    fout << root;
    fout.close();

    // Reload theme
    if (!themeService_->loadTheme(themeDir))
        return R"({"ok":false,"error":"Theme reload failed"})";

    return R"({"ok":true})";
}
```

**Step 3: Add required includes**

At the top of `IpcServer.cpp`, add if not already present:

```cpp
#include <yaml-cpp/yaml.h>
#include <QDir>
#include <QRegularExpression>
#include <fstream>
```

**Step 4: Verify it compiles**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Clean compile.

**Step 5: Check IpcServer's CMakeLists entry links yaml-cpp**

Verify `src/CMakeLists.txt` already links `yaml-cpp::yaml-cpp` for the main target. It should since YamlConfig already uses it.

**Step 6: Run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`
Expected: All tests pass (IPC is not unit-tested yet, but nothing should break).

**Step 7: Submit to Codex for review**

Ask Codex to review the handleSetTheme implementation for correctness and security (path traversal in themeId, file write safety).

**Step 8: Commit**

```bash
git add src/core/services/ThemeService.hpp src/core/services/IpcServer.cpp
git commit -m "feat: implement IPC theme get/set endpoints"
```

---

### Task 5: Fix Installer Diagnostics + Headless Tests

**Files:**
- Modify: `install.sh` (1 line)
- Modify: `tests/CMakeLists.txt` (2 lines)

**Step 1: Fix plugin manifest filename in installer**

In `install.sh`, line 448, change:
```bash
PLUGIN_COUNT=$(find "$CONFIG_DIR/plugins" -name "plugin.yaml" 2>/dev/null | wc -l)
```

**Step 2: Add headless test environment for GUI tests**

In `tests/CMakeLists.txt`, after the `add_test()` calls at the bottom, add:

```cmake
# GUI/Quick tests need offscreen platform when no display is available
set_tests_properties(test_configuration test_theme_service test_plugin_model
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

**Step 3: Verify headless tests work**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && ctest --output-on-failure`
Expected: All tests pass WITHOUT needing to manually set QT_QPA_PLATFORM.

**Step 4: Commit**

```bash
git add install.sh tests/CMakeLists.txt
git commit -m "fix: installer plugin diagnostics filename, headless test env"
```

---

### Task 6: Config Key Coverage Test

**Files:**
- Create: `tests/test_config_key_coverage.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the coverage test**

Create `tests/test_config_key_coverage.cpp`:

```cpp
#include <QtTest>
#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"

/// This test verifies that every config key used in the codebase
/// is readable through ConfigService with a valid default value.
class TestConfigKeyCoverage : public QObject {
    Q_OBJECT
private slots:
    void testAllInstallerKeys();
    void testAllRuntimeKeys();
    void testPluginConsumedKeys();
};

void TestConfigKeyCoverage::testAllInstallerKeys()
{
    // Keys that install.sh writes to config.yaml
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_coverage.yaml");

    // These MUST return valid values — the installer generates them
    QVERIFY(svc.value("connection.wifi_ap.interface").isValid());
    QVERIFY(svc.value("connection.wifi_ap.ssid").isValid());
    QVERIFY(svc.value("connection.wifi_ap.password").isValid());
    QVERIFY(svc.value("connection.tcp_port").isValid());
    QVERIFY(svc.value("video.fps").isValid());
    QVERIFY(svc.value("video.resolution").isValid());
    QVERIFY(svc.value("display.brightness").isValid());
}

void TestConfigKeyCoverage::testAllRuntimeKeys()
{
    // All keys that exist in YamlConfig::initDefaults()
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_coverage.yaml");

    static const QStringList keys = {
        "hardware_profile",
        "display.brightness",
        "display.theme",
        "display.orientation",
        "connection.auto_connect_aa",
        "connection.bt_discoverable",
        "connection.wifi_ap.ssid",
        "connection.wifi_ap.password",
        "connection.wifi_ap.channel",
        "connection.wifi_ap.band",
        "connection.tcp_port",
        "audio.master_volume",
        "audio.output_device",
        "audio.microphone.device",
        "audio.microphone.gain",
        "video.fps",
        "video.resolution",
        "video.dpi",
        "identity.head_unit_name",
        "identity.manufacturer",
        "identity.model",
        "identity.sw_version",
        "identity.car_model",
        "identity.car_year",
        "identity.left_hand_drive",
        "sensors.night_mode.source",
        "sensors.night_mode.day_start",
        "sensors.night_mode.night_start",
        "sensors.night_mode.gpio_pin",
        "sensors.night_mode.gpio_active_high",
        "sensors.gps.enabled",
        "sensors.gps.source",
        "nav_strip.show_labels",
    };

    for (const auto& key : keys) {
        QVERIFY2(svc.value(key).isValid(),
                 qPrintable(QString("Key '%1' returned invalid QVariant").arg(key)));
    }
}

void TestConfigKeyCoverage::testPluginConsumedKeys()
{
    // Keys that AndroidAutoPlugin reads via configService()->value()
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_coverage.yaml");

    // touch.device may be empty string but must be valid QVariant
    // Note: touch section must exist in defaults for this to work
    // display.width and display.height must be readable
    // These are the keys that were previously silently dropped
}

QTEST_MAIN(TestConfigKeyCoverage)
#include "test_config_key_coverage.moc"
```

**Important note on touch.device and display.width/height:** These keys are written by the installer but do NOT exist in `YamlConfig::initDefaults()`. The `setValueByPath()` method rejects unknown paths. We need to add these to initDefaults() for the coverage test to pass. Add to `YamlConfig::initDefaults()`:

```cpp
root_["display"]["width"] = 1024;
root_["display"]["height"] = 600;
root_["touch"]["device"] = "";
```

**Step 2: Add test target to CMakeLists.txt**

Add to `tests/CMakeLists.txt` before `enable_testing()`:

```cmake
add_executable(test_config_key_coverage
    test_config_key_coverage.cpp
    ${CMAKE_SOURCE_DIR}/src/core/YamlConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/ConfigService.cpp
)
target_include_directories(test_config_key_coverage PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_config_key_coverage PRIVATE
    Qt6::Core
    Qt6::Test
    yaml-cpp::yaml-cpp
)
```

And add the test registration:

```cmake
add_test(NAME test_config_key_coverage COMMAND test_config_key_coverage)
```

**Step 3: Add display.width, display.height, and touch.device to YamlConfig defaults**

In `src/core/YamlConfig.cpp`, in `initDefaults()`, add after the display.orientation line:

```cpp
root_["display"]["width"] = 1024;
root_["display"]["height"] = 600;
```

And add a touch section:

```cpp
root_["touch"]["device"] = "";
```

**Step 4: Add typed accessors for display width/height and touch device**

In `src/core/YamlConfig.hpp`, add to the Display section:

```cpp
    int displayWidth() const;
    void setDisplayWidth(int v);
    int displayHeight() const;
    void setDisplayHeight(int v);
```

And add a Touch section:

```cpp
    // Touch
    QString touchDevice() const;
    void setTouchDevice(const QString& v);
```

In `src/core/YamlConfig.cpp`, implement:

```cpp
int YamlConfig::displayWidth() const
{
    return root_["display"]["width"].as<int>(1024);
}

void YamlConfig::setDisplayWidth(int v)
{
    root_["display"]["width"] = v;
}

int YamlConfig::displayHeight() const
{
    return root_["display"]["height"].as<int>(600);
}

void YamlConfig::setDisplayHeight(int v)
{
    root_["display"]["height"] = v;
}

QString YamlConfig::touchDevice() const
{
    return QString::fromStdString(root_["touch"]["device"].as<std::string>(""));
}

void YamlConfig::setTouchDevice(const QString& v)
{
    root_["touch"]["device"] = v.toStdString();
}
```

**Step 5: Update testPluginConsumedKeys()**

Now that the keys exist in defaults, fill in the test:

```cpp
void TestConfigKeyCoverage::testPluginConsumedKeys()
{
    oap::YamlConfig yaml;
    oap::ConfigService svc(&yaml, "/tmp/oap_test_coverage.yaml");

    // Keys that AndroidAutoPlugin reads
    QVERIFY(svc.value("touch.device").isValid());
    QVERIFY(svc.value("display.width").isValid());
    QVERIFY(svc.value("display.height").isValid());

    // Verify sensible defaults
    QCOMPARE(svc.value("display.width").toInt(), 1024);
    QCOMPARE(svc.value("display.height").toInt(), 600);
    QCOMPARE(svc.value("touch.device").toString(), QString(""));
}
```

**Step 6: Run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && cmake --build . -j$(nproc) && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`
Expected: All tests pass including the new coverage test.

**Step 7: Submit to Codex for review**

Ask Codex to verify the coverage test is comprehensive and the new defaults are sensible.

**Step 8: Commit**

```bash
git add src/core/YamlConfig.hpp src/core/YamlConfig.cpp tests/test_config_key_coverage.cpp tests/CMakeLists.txt
git commit -m "feat: add config key coverage test, add missing display/touch defaults"
```

---

### Task 7: Rewrite Config Schema Documentation

**Files:**
- Modify: `docs/config-schema.md`

**Step 1: Rewrite from scratch**

Replace the entire contents of `docs/config-schema.md` with documentation generated from `YamlConfig::initDefaults()`. Document every key with its type, default value, and which component reads it.

The document should include:
- Full YAML example with all keys and their defaults
- Table of every key: path, type, default, description, consumer
- Plugin config namespace documentation (using `pluginValue()`/`setPluginValue()`)
- Migration policy (carried forward from existing doc)
- Theme config reference (separate file, existing format)

Use the actual code as the source of truth — do not guess or copy from the old doc.

**Step 2: Submit to Codex for review**

Ask Codex to cross-reference the new docs against `YamlConfig::initDefaults()` and `ConfigService` to verify completeness.

**Step 3: Commit**

```bash
git add docs/config-schema.md
git commit -m "docs: rewrite config-schema.md from actual YamlConfig code"
```

---

### Task 8: Final Verification

**Step 1: Run full test suite**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake .. && cmake --build . -j$(nproc) && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`
Expected: All tests pass (should be 16+ tests now).

**Step 2: Also verify ctest works WITHOUT manual QT_QPA_PLATFORM**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && ctest --output-on-failure`
Expected: All tests pass — the CMake `set_tests_properties` handles it.

**Step 3: Submit full diff to Codex for final review**

Ask Codex to review the complete set of changes across all tasks for consistency, regressions, and anything missed from the original stability report.

**Step 4: Update CLAUDE.md if needed**

If any key gotchas or architectural decisions should be captured for future sessions.
