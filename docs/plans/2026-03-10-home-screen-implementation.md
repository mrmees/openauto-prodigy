# Home Screen Widget System — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the placeholder home screen with a responsive 3-pane widget layout, launcher dock, widget descriptor/instance architecture, and long-press widget picker.

**Architecture:** Widget system uses a descriptor + instance split mirroring the existing plugin lifecycle. `WidgetDescriptor` holds static metadata for registry/picker. `WidgetInstanceContext` (QObject) is created per-placement and injected into a child QQmlContext. Plugins provide widgets via `widgetDescriptors()`. The home screen QML uses 3 Loaders in a responsive layout (landscape: main left + subs stacked right; portrait: main top + subs side-by-side) with a launcher dock row at the bottom.

**Tech Stack:** Qt 6.8, QML, C++17, CMake, yaml-cpp, Qt Test

**Design doc:** `docs/plans/2026-03-10-home-screen-widget-system-design.md`

---

## Task 1: Widget Data Types (WidgetDescriptor, WidgetPlacement, PageDescriptor)

**Files:**
- Create: `src/core/widget/WidgetTypes.hpp`
- Test: `tests/test_widget_types.cpp`
- Modify: `src/CMakeLists.txt` (add to openauto-core)
- Modify: `tests/CMakeLists.txt` (add test)

**Step 1: Write the failing test**

```cpp
// tests/test_widget_types.cpp
#include <QtTest/QtTest>
#include "core/widget/WidgetTypes.hpp"

class TestWidgetTypes : public QObject {
    Q_OBJECT
private slots:
    void testWidgetSizeFlags();
    void testWidgetDescriptorDefaults();
    void testWidgetPlacementCompositeKey();
    void testPageDescriptorDefaults();
};

void TestWidgetTypes::testWidgetSizeFlags() {
    using namespace oap;
    WidgetSizeFlags flags = WidgetSize::Main | WidgetSize::Sub;
    QVERIFY(flags.testFlag(WidgetSize::Main));
    QVERIFY(flags.testFlag(WidgetSize::Sub));

    WidgetSizeFlags mainOnly = WidgetSize::Main;
    QVERIFY(mainOnly.testFlag(WidgetSize::Main));
    QVERIFY(!mainOnly.testFlag(WidgetSize::Sub));
}

void TestWidgetTypes::testWidgetDescriptorDefaults() {
    oap::WidgetDescriptor desc;
    QVERIFY(desc.id.isEmpty());
    QVERIFY(desc.pluginId.isEmpty());
    QCOMPARE(desc.supportedSizes, oap::WidgetSizeFlags{});
    QVERIFY(desc.defaultConfig.isEmpty());
}

void TestWidgetTypes::testWidgetPlacementCompositeKey() {
    oap::WidgetPlacement p;
    p.pageId = "home";
    p.paneId = "main";
    QCOMPARE(p.compositeKey(), QStringLiteral("home:main"));
}

void TestWidgetTypes::testPageDescriptorDefaults() {
    oap::PageDescriptor page;
    QVERIFY(page.id.isEmpty());
    QCOMPARE(page.layoutTemplate, QStringLiteral("standard-3pane"));
    QCOMPARE(page.order, 0);
    QVERIFY(page.flags.isEmpty());
}

QTEST_GUILESS_MAIN(TestWidgetTypes)
#include "test_widget_types.moc"
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake .. && make -j$(nproc) test_widget_types 2>&1 | tail -5`
Expected: Compilation failure — `WidgetTypes.hpp` not found

**Step 3: Write minimal implementation**

```cpp
// src/core/widget/WidgetTypes.hpp
#pragma once

#include <QFlags>
#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace oap {

enum class WidgetSize {
    Main = 0x1,
    Sub  = 0x2
};
Q_DECLARE_FLAGS(WidgetSizeFlags, WidgetSize)
Q_DECLARE_OPERATORS_FOR_FLAGS(WidgetSizeFlags)

struct WidgetDescriptor {
    QString id;                         // "org.openauto.clock"
    QString displayName;                // "Clock"
    QString iconName;                   // "schedule" (Material Symbols name)
    WidgetSizeFlags supportedSizes;     // Main | Sub
    QUrl qmlComponent;                  // qrc URL to widget QML
    QString pluginId;                   // empty for standalone widgets
    QVariantMap defaultConfig;          // optional per-widget defaults
};

struct WidgetPlacement {
    QString instanceId;     // "clock-main" — unique per placement
    QString widgetId;       // "org.openauto.clock"
    QString pageId;         // "home"
    QString paneId;         // "main", "sub1", "sub2"
    QVariantMap config;     // optional per-instance config

    QString compositeKey() const { return pageId + ":" + paneId; }
};

struct PageDescriptor {
    QString id;
    QString layoutTemplate = QStringLiteral("standard-3pane");
    int order = 0;
    QVariantMap flags;
};

} // namespace oap
```

**Step 4: Add to CMake**

Add to `src/CMakeLists.txt` openauto-core sources — no .cpp needed (header-only), but add the include path if not already covered. Since `src/` is already in the include path, just creating the file is enough.

Add to `tests/CMakeLists.txt`:
```cmake
oap_add_test(test_widget_types SOURCES test_widget_types.cpp)
```

**Step 5: Build and run test**

Run: `cd build && cmake .. && make -j$(nproc) test_widget_types && ctest -R test_widget_types --output-on-failure`
Expected: All 4 tests PASS

**Step 6: Commit**

```bash
git add src/core/widget/WidgetTypes.hpp tests/test_widget_types.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(widget): add WidgetDescriptor, WidgetPlacement, and PageDescriptor types"
```

---

## Task 2: WidgetRegistry

**Files:**
- Create: `src/core/widget/WidgetRegistry.hpp`
- Create: `src/core/widget/WidgetRegistry.cpp`
- Test: `tests/test_widget_registry.cpp`
- Modify: `src/CMakeLists.txt` (add WidgetRegistry.cpp to openauto-core)
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// tests/test_widget_registry.cpp
#include <QtTest/QtTest>
#include "core/widget/WidgetRegistry.hpp"

class TestWidgetRegistry : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndLookup();
    void testDuplicateIdRejected();
    void testFilterBySize();
    void testUnregister();
    void testDescriptorCount();
};

void TestWidgetRegistry::testRegisterAndLookup() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor desc;
    desc.id = "org.test.clock";
    desc.displayName = "Clock";
    desc.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;

    QVERIFY(registry.registerWidget(desc));
    auto result = registry.descriptor("org.test.clock");
    QVERIFY(result.has_value());
    QCOMPARE(result->displayName, "Clock");
}

void TestWidgetRegistry::testDuplicateIdRejected() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor desc;
    desc.id = "org.test.dup";
    QVERIFY(registry.registerWidget(desc));
    QVERIFY(!registry.registerWidget(desc));
}

void TestWidgetRegistry::testFilterBySize() {
    oap::WidgetRegistry registry;

    oap::WidgetDescriptor mainOnly;
    mainOnly.id = "main-only";
    mainOnly.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(mainOnly);

    oap::WidgetDescriptor subOnly;
    subOnly.id = "sub-only";
    subOnly.supportedSizes = oap::WidgetSize::Sub;
    registry.registerWidget(subOnly);

    oap::WidgetDescriptor both;
    both.id = "both";
    both.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;
    registry.registerWidget(both);

    auto mainWidgets = registry.widgetsForSize(oap::WidgetSize::Main);
    QCOMPARE(mainWidgets.size(), 2); // main-only + both

    auto subWidgets = registry.widgetsForSize(oap::WidgetSize::Sub);
    QCOMPARE(subWidgets.size(), 2); // sub-only + both
}

void TestWidgetRegistry::testUnregister() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor desc;
    desc.id = "org.test.removable";
    registry.registerWidget(desc);

    registry.unregisterWidget("org.test.removable");
    QVERIFY(!registry.descriptor("org.test.removable").has_value());
}

void TestWidgetRegistry::testDescriptorCount() {
    oap::WidgetRegistry registry;
    QCOMPARE(registry.allDescriptors().size(), 0);

    oap::WidgetDescriptor d1;
    d1.id = "w1";
    registry.registerWidget(d1);

    oap::WidgetDescriptor d2;
    d2.id = "w2";
    registry.registerWidget(d2);

    QCOMPARE(registry.allDescriptors().size(), 2);
}

QTEST_GUILESS_MAIN(TestWidgetRegistry)
#include "test_widget_registry.moc"
```

**Step 2: Run test to verify it fails**

Run: `cd build && cmake .. && make -j$(nproc) test_widget_registry 2>&1 | tail -5`
Expected: Compilation failure — `WidgetRegistry.hpp` not found

**Step 3: Write implementation**

```cpp
// src/core/widget/WidgetRegistry.hpp
#pragma once

#include "core/widget/WidgetTypes.hpp"
#include <QList>
#include <QMap>
#include <optional>

namespace oap {

class WidgetRegistry {
public:
    bool registerWidget(const WidgetDescriptor& descriptor);
    void unregisterWidget(const QString& widgetId);

    std::optional<WidgetDescriptor> descriptor(const QString& widgetId) const;
    QList<WidgetDescriptor> allDescriptors() const;
    QList<WidgetDescriptor> widgetsForSize(WidgetSize size) const;

private:
    QMap<QString, WidgetDescriptor> descriptors_;
};

} // namespace oap
```

```cpp
// src/core/widget/WidgetRegistry.cpp
#include "core/widget/WidgetRegistry.hpp"

namespace oap {

bool WidgetRegistry::registerWidget(const WidgetDescriptor& descriptor) {
    if (descriptors_.contains(descriptor.id))
        return false;
    descriptors_.insert(descriptor.id, descriptor);
    return true;
}

void WidgetRegistry::unregisterWidget(const QString& widgetId) {
    descriptors_.remove(widgetId);
}

std::optional<WidgetDescriptor> WidgetRegistry::descriptor(const QString& widgetId) const {
    auto it = descriptors_.find(widgetId);
    if (it == descriptors_.end())
        return std::nullopt;
    return *it;
}

QList<WidgetDescriptor> WidgetRegistry::allDescriptors() const {
    return descriptors_.values();
}

QList<WidgetDescriptor> WidgetRegistry::widgetsForSize(WidgetSize size) const {
    QList<WidgetDescriptor> result;
    for (const auto& desc : descriptors_) {
        if (desc.supportedSizes.testFlag(size))
            result.append(desc);
    }
    return result;
}

} // namespace oap
```

**Step 4: Add to CMake**

In `src/CMakeLists.txt`, add `core/widget/WidgetRegistry.cpp` to the openauto-core STATIC sources list (after line 49, before the closing parenthesis).

In `tests/CMakeLists.txt`:
```cmake
oap_add_test(test_widget_registry SOURCES test_widget_registry.cpp)
```

**Step 5: Build and run test**

Run: `cd build && cmake .. && make -j$(nproc) test_widget_registry && ctest -R test_widget_registry --output-on-failure`
Expected: All 5 tests PASS

**Step 6: Commit**

```bash
git add src/core/widget/WidgetRegistry.hpp src/core/widget/WidgetRegistry.cpp tests/test_widget_registry.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(widget): add WidgetRegistry with registration, lookup, and size filtering"
```

---

## Task 3: WidgetInstanceContext

**Files:**
- Create: `src/ui/WidgetInstanceContext.hpp`
- Create: `src/ui/WidgetInstanceContext.cpp`
- Test: `tests/test_widget_instance_context.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// tests/test_widget_instance_context.cpp
#include <QtTest/QtTest>
#include "ui/WidgetInstanceContext.hpp"
#include "core/widget/WidgetTypes.hpp"

class TestWidgetInstanceContext : public QObject {
    Q_OBJECT
private slots:
    void testProperties();
    void testPaneSizeExposed();
};

void TestWidgetInstanceContext::testProperties() {
    oap::WidgetPlacement placement;
    placement.instanceId = "clock-main";
    placement.widgetId = "org.openauto.clock";
    placement.pageId = "home";
    placement.paneId = "main";

    oap::WidgetInstanceContext ctx(placement, oap::WidgetSize::Main, nullptr, this);

    QCOMPARE(ctx.instanceId(), "clock-main");
    QCOMPARE(ctx.widgetId(), "org.openauto.clock");
    QCOMPARE(ctx.paneSize(), oap::WidgetSize::Main);
}

void TestWidgetInstanceContext::testPaneSizeExposed() {
    oap::WidgetPlacement placement;
    placement.instanceId = "test";
    placement.paneId = "sub1";

    oap::WidgetInstanceContext ctx(placement, oap::WidgetSize::Sub, nullptr, this);

    // Verify QML-accessible paneSize property
    QCOMPARE(ctx.property("paneSize").toInt(), static_cast<int>(oap::WidgetSize::Sub));
}

QTEST_GUILESS_MAIN(TestWidgetInstanceContext)
#include "test_widget_instance_context.moc"
```

**Step 2: Run test to verify it fails**

Expected: Compilation failure — `WidgetInstanceContext.hpp` not found

**Step 3: Write implementation**

```cpp
// src/ui/WidgetInstanceContext.hpp
#pragma once

#include <QObject>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class IHostContext;

class WidgetInstanceContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(int paneSize READ paneSizeInt CONSTANT)
    Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
    Q_PROPERTY(QString widgetId READ widgetId CONSTANT)

public:
    WidgetInstanceContext(const WidgetPlacement& placement,
                          WidgetSize paneSize,
                          IHostContext* hostContext,
                          QObject* parent = nullptr);

    WidgetSize paneSize() const { return paneSize_; }
    int paneSizeInt() const { return static_cast<int>(paneSize_); }
    QString instanceId() const { return placement_.instanceId; }
    QString widgetId() const { return placement_.widgetId; }
    IHostContext* hostContext() const { return hostContext_; }
    const WidgetPlacement& placement() const { return placement_; }

private:
    WidgetPlacement placement_;
    WidgetSize paneSize_;
    IHostContext* hostContext_;
};

} // namespace oap
```

```cpp
// src/ui/WidgetInstanceContext.cpp
#include "ui/WidgetInstanceContext.hpp"

namespace oap {

WidgetInstanceContext::WidgetInstanceContext(
    const WidgetPlacement& placement,
    WidgetSize paneSize,
    IHostContext* hostContext,
    QObject* parent)
    : QObject(parent)
    , placement_(placement)
    , paneSize_(paneSize)
    , hostContext_(hostContext)
{}

} // namespace oap
```

**Step 4: Add to CMake**

In `src/CMakeLists.txt`, add `ui/WidgetInstanceContext.cpp` to openauto-core sources.

In `tests/CMakeLists.txt`:
```cmake
oap_add_test(test_widget_instance_context SOURCES test_widget_instance_context.cpp)
```

**Step 5: Build and run test**

Run: `cd build && cmake .. && make -j$(nproc) test_widget_instance_context && ctest -R test_widget_instance_context --output-on-failure`
Expected: All 2 tests PASS

**Step 6: Commit**

```bash
git add src/ui/WidgetInstanceContext.hpp src/ui/WidgetInstanceContext.cpp tests/test_widget_instance_context.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(widget): add WidgetInstanceContext for per-placement QML runtime"
```

---

## Task 4: IPlugin widgetDescriptors() Extension

**Files:**
- Modify: `src/core/plugin/IPlugin.hpp` (add virtual widgetDescriptors method)
- Test: `tests/test_widget_plugin_integration.cpp` (verify plugins return empty by default)
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// tests/test_widget_plugin_integration.cpp
#include <QtTest/QtTest>
#include "core/plugin/IPlugin.hpp"
#include "core/widget/WidgetTypes.hpp"

// Mock plugin that doesn't override widgetDescriptors
class PlainMockPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.plain"; }
    QString name() const override { return "Plain"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }
};

// Mock plugin that provides widgets
class WidgetMockPlugin : public QObject, public oap::IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)
public:
    QString id() const override { return "org.test.widgeted"; }
    QString name() const override { return "Widgeted"; }
    QString version() const override { return "1.0"; }
    int apiVersion() const override { return 1; }
    bool initialize(oap::IHostContext*) override { return true; }
    void shutdown() override {}
    QUrl qmlComponent() const override { return {}; }
    QUrl iconSource() const override { return {}; }
    QStringList requiredServices() const override { return {}; }

    QList<oap::WidgetDescriptor> widgetDescriptors() const override {
        oap::WidgetDescriptor desc;
        desc.id = "org.test.widgeted.status";
        desc.displayName = "Status";
        desc.supportedSizes = oap::WidgetSize::Sub;
        desc.pluginId = id();
        return {desc};
    }
};

class TestWidgetPluginIntegration : public QObject {
    Q_OBJECT
private slots:
    void testDefaultReturnsEmpty();
    void testPluginProvidesWidgets();
};

void TestWidgetPluginIntegration::testDefaultReturnsEmpty() {
    PlainMockPlugin plugin;
    QCOMPARE(plugin.widgetDescriptors().size(), 0);
}

void TestWidgetPluginIntegration::testPluginProvidesWidgets() {
    WidgetMockPlugin plugin;
    auto widgets = plugin.widgetDescriptors();
    QCOMPARE(widgets.size(), 1);
    QCOMPARE(widgets[0].id, "org.test.widgeted.status");
    QCOMPARE(widgets[0].pluginId, "org.test.widgeted");
}

QTEST_GUILESS_MAIN(TestWidgetPluginIntegration)
#include "test_widget_plugin_integration.moc"
```

**Step 2: Run test to verify it fails**

Expected: Compilation failure — `widgetDescriptors()` not a member of `IPlugin`

**Step 3: Modify IPlugin.hpp**

Add to `src/core/plugin/IPlugin.hpp` after the `#include <QUrl>` line:
```cpp
#include <QList>
```

Add after the forward declaration of `IHostContext` (before `class IPlugin`):
```cpp
struct WidgetDescriptor;
```

Add to the `IPlugin` class, after `wantsFullscreen()` (line 47):
```cpp
    // Widgets — plugins can provide widget components for the home screen
    virtual QList<WidgetDescriptor> widgetDescriptors() const { return {}; }
```

Add the full include in the .hpp since we need `WidgetDescriptor` as a complete type for the return value:
Actually, since we return by value, we need the full type. Add `#include "core/widget/WidgetTypes.hpp"` to IPlugin.hpp — but this creates a dependency concern. Better approach: forward-declare won't work for return-by-value. Include the header.

Add at top of IPlugin.hpp after `#include <QUrl>`:
```cpp
#include "core/widget/WidgetTypes.hpp"
```

Remove the forward declaration of `WidgetDescriptor` (not needed with the include).

**Step 4: Add test to CMake**

In `tests/CMakeLists.txt`:
```cmake
oap_add_test(test_widget_plugin_integration SOURCES test_widget_plugin_integration.cpp)
```

**Step 5: Build and run tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest -R "test_widget|test_plugin" --output-on-failure`
Expected: All widget tests + existing plugin tests PASS (no regressions)

**Step 6: Commit**

```bash
git add src/core/plugin/IPlugin.hpp tests/test_widget_plugin_integration.cpp tests/CMakeLists.txt
git commit -m "feat(widget): add widgetDescriptors() to IPlugin interface"
```

---

## Task 5: Widget Config Persistence (YAML read/write)

**Files:**
- Modify: `src/core/YamlConfig.hpp` (add page/placement methods)
- Modify: `src/core/YamlConfig.cpp` (implement page/placement read/write)
- Test: `tests/test_widget_config.cpp`
- Create: `tests/data/test_widget_config.yaml` (test fixture)
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test fixture**

```yaml
# tests/data/test_widget_config.yaml
version: 1

pages:
  - id: home
    layoutTemplate: standard-3pane
    order: 0

placements:
  - instanceId: clock-main
    widgetId: org.openauto.clock
    pageId: home
    paneId: main
    config: {}

  - instanceId: aa-status-sub1
    widgetId: org.openauto.aa-status
    pageId: home
    paneId: sub1
    config: {}

  - instanceId: now-playing-sub2
    widgetId: org.openauto.bt-now-playing
    pageId: home
    paneId: sub2
    config: {}
```

**Step 2: Write the failing test**

```cpp
// tests/test_widget_config.cpp
#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "core/YamlConfig.hpp"
#include "core/widget/WidgetTypes.hpp"

class TestWidgetConfig : public QObject {
    Q_OBJECT
private slots:
    void testLoadPages();
    void testLoadPlacements();
    void testSavePlacements();
    void testDefaultsWhenEmpty();
    void testConfigVersion();
    void testPlacementValidation();
};

void TestWidgetConfig::testLoadPages() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    auto pages = config.widgetPages();
    QCOMPARE(pages.size(), 1);
    QCOMPARE(pages[0].id, "home");
    QCOMPARE(pages[0].layoutTemplate, "standard-3pane");
    QCOMPARE(pages[0].order, 0);
}

void TestWidgetConfig::testLoadPlacements() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    auto placements = config.widgetPlacements();
    QCOMPARE(placements.size(), 3);
    QCOMPARE(placements[0].instanceId, "clock-main");
    QCOMPARE(placements[0].widgetId, "org.openauto.clock");
    QCOMPARE(placements[0].pageId, "home");
    QCOMPARE(placements[0].paneId, "main");
}

void TestWidgetConfig::testSavePlacements() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    // Load, modify, save, reload
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    auto placements = config.widgetPlacements();
    placements[0].widgetId = "org.openauto.weather";
    config.setWidgetPlacements(placements);
    config.save(tmpPath);

    oap::YamlConfig reloaded;
    reloaded.load(tmpPath);
    auto reloadedPlacements = reloaded.widgetPlacements();
    QCOMPARE(reloadedPlacements[0].widgetId, "org.openauto.weather");
}

void TestWidgetConfig::testDefaultsWhenEmpty() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    tmpFile.write("# empty config\n");
    tmpFile.close();

    oap::YamlConfig config;
    config.load(tmpFile.fileName());

    auto pages = config.widgetPages();
    QCOMPARE(pages.size(), 1);
    QCOMPARE(pages[0].id, "home");

    auto placements = config.widgetPlacements();
    QCOMPARE(placements.size(), 3); // default: clock, aa-status, now-playing
}

void TestWidgetConfig::testConfigVersion() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");
    QCOMPARE(config.widgetConfigVersion(), 1);
}

void TestWidgetConfig::testPlacementValidation() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    // Try to set placements with duplicate composite key
    QList<oap::WidgetPlacement> bad;
    oap::WidgetPlacement p1;
    p1.instanceId = "a";
    p1.widgetId = "w1";
    p1.pageId = "home";
    p1.paneId = "main";
    bad.append(p1);

    oap::WidgetPlacement p2;
    p2.instanceId = "b";
    p2.widgetId = "w2";
    p2.pageId = "home";
    p2.paneId = "main"; // duplicate page:pane
    bad.append(p2);

    // Should keep only first placement per composite key
    config.setWidgetPlacements(bad);
    auto result = config.widgetPlacements();
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].instanceId, "a");
}

QTEST_GUILESS_MAIN(TestWidgetConfig)
#include "test_widget_config.moc"
```

**Step 3: Run test to verify it fails**

Expected: Compilation failure — `widgetPages()` not a member of `YamlConfig`

**Step 4: Implement config methods**

Add to `src/core/YamlConfig.hpp` (in the public section, after `setLauncherTiles`):
```cpp
    // Widget home screen config
    int widgetConfigVersion() const;
    QList<oap::PageDescriptor> widgetPages() const;
    void setWidgetPages(const QList<oap::PageDescriptor>& pages);
    QList<oap::WidgetPlacement> widgetPlacements() const;
    void setWidgetPlacements(const QList<oap::WidgetPlacement>& placements);
```

Add include at top of YamlConfig.hpp:
```cpp
#include "core/widget/WidgetTypes.hpp"
```

Add implementations to `src/core/YamlConfig.cpp`. Include the default placements in `initDefaults()`:
- Page: `{id: "home", layoutTemplate: "standard-3pane", order: 0}`
- Placements: clock-main, aa-status-sub1, now-playing-sub2

The `widgetPlacements()` method reads from `root_["placements"]` YAML sequence. Each entry maps to a `WidgetPlacement`. The `setWidgetPlacements()` method validates unique composite keys (pageId:paneId), discards duplicates, and writes back.

The `widgetPages()` method reads from `root_["pages"]` YAML sequence. Falls back to default if empty/missing.

**Step 5: Add test data and CMake**

Copy test fixture:
```cmake
# In tests/CMakeLists.txt
configure_file(data/test_widget_config.yaml ${CMAKE_CURRENT_BINARY_DIR}/data/test_widget_config.yaml COPYONLY)
oap_add_test(test_widget_config
    SOURCES test_widget_config.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
```

**Step 6: Build and run tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest -R test_widget_config --output-on-failure`
Expected: All 6 tests PASS

**Step 7: Run all tests for regression**

Run: `cd build && ctest --output-on-failure`
Expected: All existing tests still pass

**Step 8: Commit**

```bash
git add src/core/YamlConfig.hpp src/core/YamlConfig.cpp tests/test_widget_config.cpp tests/data/test_widget_config.yaml tests/CMakeLists.txt
git commit -m "feat(widget): add widget page and placement persistence to YamlConfig"
```

---

## Task 6: WidgetPlacementModel (QML-facing model)

**Files:**
- Create: `src/ui/WidgetPlacementModel.hpp`
- Create: `src/ui/WidgetPlacementModel.cpp`
- Test: `tests/test_widget_placement_model.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
// tests/test_widget_placement_model.cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "ui/WidgetPlacementModel.hpp"
#include "core/widget/WidgetRegistry.hpp"

class TestWidgetPlacementModel : public QObject {
    Q_OBJECT
private slots:
    void testPlacementForPane();
    void testSwapWidget();
    void testClearPane();
    void testActivePageFilter();
    void testQmlComponentUrl();
};

void TestWidgetPlacementModel::testPlacementForPane() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor clockDesc;
    clockDesc.id = "org.openauto.clock";
    clockDesc.displayName = "Clock";
    clockDesc.qmlComponent = QUrl("qrc:/widgets/ClockWidget.qml");
    clockDesc.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;
    registry.registerWidget(clockDesc);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "clock-main";
    p.widgetId = "org.openauto.clock";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    auto result = model.placementForPane("main");
    QVERIFY(result.has_value());
    QCOMPARE(result->widgetId, "org.openauto.clock");
}

void TestWidgetPlacementModel::testSwapWidget() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d1;
    d1.id = "w1";
    d1.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d1);

    oap::WidgetDescriptor d2;
    d2.id = "w2";
    d2.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d2);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    QSignalSpy spy(&model, &oap::WidgetPlacementModel::placementsChanged);
    model.swapWidget("main", "w2");

    QCOMPARE(spy.count(), 1);
    auto result = model.placementForPane("main");
    QVERIFY(result.has_value());
    QCOMPARE(result->widgetId, "w2");
}

void TestWidgetPlacementModel::testClearPane() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    model.clearPane("main");
    QVERIFY(!model.placementForPane("main").has_value());
}

void TestWidgetPlacementModel::testActivePageFilter() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;

    oap::WidgetPlacement p1;
    p1.instanceId = "w1-home";
    p1.widgetId = "w1";
    p1.pageId = "home";
    p1.paneId = "main";
    placements.append(p1);

    oap::WidgetPlacement p2;
    p2.instanceId = "w1-page2";
    p2.widgetId = "w1";
    p2.pageId = "page2";
    p2.paneId = "main";
    placements.append(p2);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);

    model.setActivePageId("home");
    QVERIFY(model.placementForPane("main").has_value());
    QCOMPARE(model.placementForPane("main")->pageId, "home");

    model.setActivePageId("page2");
    QVERIFY(model.placementForPane("main").has_value());
    QCOMPARE(model.placementForPane("main")->pageId, "page2");
}

void TestWidgetPlacementModel::testQmlComponentUrl() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.qmlComponent = QUrl("qrc:/widgets/TestWidget.qml");
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    QCOMPARE(model.qmlComponentForPane("main"), QUrl("qrc:/widgets/TestWidget.qml"));
}

QTEST_GUILESS_MAIN(TestWidgetPlacementModel)
#include "test_widget_placement_model.moc"
```

**Step 2: Run test to verify it fails**

Expected: Compilation failure — `WidgetPlacementModel.hpp` not found

**Step 3: Write implementation**

`WidgetPlacementModel` is a Q_OBJECT that:
- Holds a flat list of `WidgetPlacement` structs
- Filters by `activePageId`
- Exposes Q_INVOKABLE methods: `placementForPane(paneId)`, `swapWidget(paneId, widgetId)`, `clearPane(paneId)`, `qmlComponentForPane(paneId)`
- Emits `placementsChanged` when modifications occur
- `swapWidget` generates a new instanceId (`widgetId + "-" + paneId`)

Key: This is NOT a QAbstractListModel. It's a helper object that the QML home screen queries by pane ID. The home screen has 3 fixed Loaders, not a repeater.

```cpp
// src/ui/WidgetPlacementModel.hpp
#pragma once

#include <QObject>
#include "core/widget/WidgetTypes.hpp"
#include <optional>

namespace oap {

class WidgetRegistry;

class WidgetPlacementModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activePageId READ activePageId WRITE setActivePageId NOTIFY activePageIdChanged)

public:
    explicit WidgetPlacementModel(WidgetRegistry* registry, QObject* parent = nullptr);

    QString activePageId() const { return activePageId_; }
    void setActivePageId(const QString& pageId);

    void setPlacements(const QList<WidgetPlacement>& placements);
    QList<WidgetPlacement> allPlacements() const { return placements_; }

    Q_INVOKABLE QUrl qmlComponentForPane(const QString& paneId) const;
    Q_INVOKABLE void swapWidget(const QString& paneId, const QString& widgetId);
    Q_INVOKABLE void clearPane(const QString& paneId);

    std::optional<WidgetPlacement> placementForPane(const QString& paneId) const;

signals:
    void activePageIdChanged();
    void placementsChanged();
    void paneChanged(const QString& paneId);

private:
    WidgetRegistry* registry_;
    QList<WidgetPlacement> placements_;
    QString activePageId_ = QStringLiteral("home");
};

} // namespace oap
```

```cpp
// src/ui/WidgetPlacementModel.cpp
#include "ui/WidgetPlacementModel.hpp"
#include "core/widget/WidgetRegistry.hpp"

namespace oap {

WidgetPlacementModel::WidgetPlacementModel(WidgetRegistry* registry, QObject* parent)
    : QObject(parent), registry_(registry) {}

void WidgetPlacementModel::setActivePageId(const QString& pageId) {
    if (activePageId_ == pageId) return;
    activePageId_ = pageId;
    emit activePageIdChanged();
}

void WidgetPlacementModel::setPlacements(const QList<WidgetPlacement>& placements) {
    placements_ = placements;
    emit placementsChanged();
}

std::optional<WidgetPlacement> WidgetPlacementModel::placementForPane(const QString& paneId) const {
    for (const auto& p : placements_) {
        if (p.pageId == activePageId_ && p.paneId == paneId)
            return p;
    }
    return std::nullopt;
}

QUrl WidgetPlacementModel::qmlComponentForPane(const QString& paneId) const {
    auto placement = placementForPane(paneId);
    if (!placement) return {};
    auto desc = registry_->descriptor(placement->widgetId);
    if (!desc) return {};
    return desc->qmlComponent;
}

void WidgetPlacementModel::swapWidget(const QString& paneId, const QString& widgetId) {
    // Remove existing placement for this page+pane
    placements_.erase(
        std::remove_if(placements_.begin(), placements_.end(),
            [&](const WidgetPlacement& p) {
                return p.pageId == activePageId_ && p.paneId == paneId;
            }),
        placements_.end());

    // Add new placement
    WidgetPlacement newPlacement;
    newPlacement.instanceId = widgetId + "-" + paneId;
    newPlacement.widgetId = widgetId;
    newPlacement.pageId = activePageId_;
    newPlacement.paneId = paneId;
    placements_.append(newPlacement);

    emit placementsChanged();
    emit paneChanged(paneId);
}

void WidgetPlacementModel::clearPane(const QString& paneId) {
    placements_.erase(
        std::remove_if(placements_.begin(), placements_.end(),
            [&](const WidgetPlacement& p) {
                return p.pageId == activePageId_ && p.paneId == paneId;
            }),
        placements_.end());

    emit placementsChanged();
    emit paneChanged(paneId);
}

} // namespace oap
```

**Step 4: Add to CMake**

In `src/CMakeLists.txt`, add `ui/WidgetPlacementModel.cpp` to openauto-core.

In `tests/CMakeLists.txt`:
```cmake
oap_add_test(test_widget_placement_model SOURCES test_widget_placement_model.cpp)
```

**Step 5: Build and run test**

Run: `cd build && cmake .. && make -j$(nproc) test_widget_placement_model && ctest -R test_widget_placement_model --output-on-failure`
Expected: All 5 tests PASS

**Step 6: Commit**

```bash
git add src/ui/WidgetPlacementModel.hpp src/ui/WidgetPlacementModel.cpp tests/test_widget_placement_model.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(widget): add WidgetPlacementModel for QML pane-to-widget binding"
```

---

## Task 7: Wire Widget System into main.cpp

**Files:**
- Modify: `src/main.cpp` (create WidgetRegistry, WidgetPlacementModel, register context properties)
- No new test — integration tested via build + existing tests not regressing

**Step 1: Add includes to main.cpp**

After existing includes (around line 30):
```cpp
#include "core/widget/WidgetRegistry.hpp"
#include "core/widget/WidgetTypes.hpp"
#include "ui/WidgetPlacementModel.hpp"
```

**Step 2: Create WidgetRegistry and register built-in widgets**

After `pluginManager.initializeAll(hostContext.get());` (around line 329), add:

```cpp
    // --- Widget system ---
    auto widgetRegistry = new oap::WidgetRegistry();

    // Register built-in standalone widgets
    {
        oap::WidgetDescriptor clockDesc;
        clockDesc.id = "org.openauto.clock";
        clockDesc.displayName = "Clock";
        clockDesc.iconName = "schedule";
        clockDesc.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;
        clockDesc.qmlComponent = QUrl("qrc:/qt/qml/OpenAutoProdigy/ClockWidget.qml");
        widgetRegistry->registerWidget(clockDesc);

        oap::WidgetDescriptor aaStatusDesc;
        aaStatusDesc.id = "org.openauto.aa-status";
        aaStatusDesc.displayName = "Android Auto";
        aaStatusDesc.iconName = "directions_car";
        aaStatusDesc.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;
        aaStatusDesc.qmlComponent = QUrl("qrc:/qt/qml/OpenAutoProdigy/AAStatusWidget.qml");
        widgetRegistry->registerWidget(aaStatusDesc);
    }

    // Collect widget descriptors from plugins
    for (auto* plugin : pluginManager.plugins()) {
        for (const auto& desc : plugin->widgetDescriptors()) {
            widgetRegistry->registerWidget(desc);
        }
    }
```

**Step 3: Create WidgetPlacementModel and load config**

After the widget registry setup:

```cpp
    auto widgetPlacementModel = new oap::WidgetPlacementModel(widgetRegistry, &app);
    widgetPlacementModel->setPlacements(yamlConfig->widgetPlacements());
    widgetPlacementModel->setActivePageId("home");
```

**Step 4: Register QML context properties**

After existing context property registrations (around line 480):

```cpp
    engine.rootContext()->setContextProperty("WidgetPlacementModel", widgetPlacementModel);
    engine.rootContext()->setContextProperty("WidgetRegistry", widgetRegistry);
```

Note: `WidgetRegistry` isn't a QObject, so it won't have signals. For now, the QML side only uses `WidgetPlacementModel` (which IS a QObject). The registry reference is for the picker model (Task 9).

Actually, WidgetRegistry should be a QObject for QML access. Let's make it one — add `Q_OBJECT` and inherit from QObject in WidgetRegistry. This is a small change:

Modify `src/core/widget/WidgetRegistry.hpp`:
```cpp
class WidgetRegistry : public QObject {
    Q_OBJECT
public:
    explicit WidgetRegistry(QObject* parent = nullptr);
    // ... rest unchanged
};
```

Update constructor in `.cpp` and test to pass parent.

**Step 5: Connect placement save on change**

After creating widgetPlacementModel:
```cpp
    QObject::connect(widgetPlacementModel, &oap::WidgetPlacementModel::placementsChanged,
                     widgetPlacementModel, [yamlConfig = yamlConfig.get(), widgetPlacementModel]() {
        yamlConfig->setWidgetPlacements(widgetPlacementModel->allPlacements());
    });
```

**Step 6: Build and verify no regressions**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: All existing tests pass, build succeeds

**Step 7: Commit**

```bash
git add src/main.cpp src/core/widget/WidgetRegistry.hpp src/core/widget/WidgetRegistry.cpp
git commit -m "feat(widget): wire WidgetRegistry and WidgetPlacementModel into app startup"
```

---

## Task 8: HomeScreen QML — Pane Layout + Launcher Dock

**Files:**
- Rewrite: `qml/applications/home/HomeMenu.qml` → `HomeScreen.qml` (rename via CMake)
- Create: `qml/components/WidgetHost.qml`
- Create: `qml/components/LauncherDock.qml`
- Modify: `qml/components/Shell.qml` (replace LauncherMenu with HomeScreen)
- Modify: `src/CMakeLists.txt` (update QML module)

**Step 1: Create WidgetHost.qml**

A generic pane container that loads a widget QML component via Loader:

```qml
// qml/components/WidgetHost.qml
import QtQuick

Item {
    id: widgetHost

    property string paneId: ""
    property url widgetSource: ""
    property bool isMainPane: paneId === "main"

    // Long-press detection for widget picker
    signal longPressed()

    Loader {
        id: widgetLoader
        anchors.fill: parent
        source: widgetHost.widgetSource
        asynchronous: false

        onStatusChanged: {
            if (status === Loader.Error)
                console.warn("WidgetHost: failed to load", widgetHost.widgetSource)
        }
    }

    // Empty state
    Rectangle {
        anchors.fill: parent
        color: ThemeService.surfaceContainer
        radius: UiMetrics.radius
        visible: !widgetHost.widgetSource.toString()
        opacity: 0.5
    }

    // Long-press detector
    MouseArea {
        anchors.fill: parent
        pressAndHoldInterval: 500
        onPressAndHold: widgetHost.longPressed()
        // Pass through normal taps to widget content
        onClicked: function(mouse) { mouse.accepted = false }
        onPressed: function(mouse) { mouse.accepted = false }
        onReleased: function(mouse) { mouse.accepted = false }
    }
}
```

Note: The MouseArea long-press detection approach may need refinement — `pressAndHold` consumes the press, so passing through normal taps needs care. An alternative is using a Timer-based approach. The implementer should test this on Pi and adjust if taps stop reaching widget content. If so, switch to a transparent overlay that only activates `pressAndHold` and uses `propagateComposedEvents: true`.

**Step 2: Create LauncherDock.qml**

```qml
// qml/components/LauncherDock.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: dock
    height: UiMetrics.tileH * 0.5 + UiMetrics.spacing

    RowLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.gridGap

        Repeater {
            model: LauncherModel

            Rectangle {
                width: UiMetrics.tileW * 0.6
                height: UiMetrics.tileH * 0.45
                radius: UiMetrics.radiusSmall
                color: pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainerHigh
                property bool pressed: dockMa.pressed

                // Level 1 elevation
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: 0.4
                    shadowVerticalOffset: 2
                    shadowOpacity: 0.25
                    shadowColor: ThemeService.shadow
                }

                scale: pressed ? 0.95 : 1.0
                Behavior on scale { NumberAnimation { duration: 80 } }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: UiMetrics.spacing * 0.25

                    MaterialIcon {
                        text: model.tileIcon
                        font.pixelSize: UiMetrics.iconSmall
                        color: ThemeService.onSurface
                        Layout.alignment: Qt.AlignHCenter
                    }

                    NormalText {
                        text: model.tileLabel
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.onSurface
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                MouseArea {
                    id: dockMa
                    anchors.fill: parent
                    onClicked: {
                        var action = model.tileAction
                        if (action.startsWith("plugin:")) {
                            PluginModel.setActivePlugin(action.substring(7))
                        } else if (action.startsWith("navigate:")) {
                            var target = action.substring(9)
                            if (target === "settings") {
                                PluginModel.setActivePlugin("")
                                ApplicationController.navigateTo(6)
                            }
                        }
                    }
                }
            }
        }
    }
}
```

Note: The `MultiEffect` import and `layer.effect` syntax should be verified against the existing Tile.qml usage pattern. The implementer should reference `qml/controls/Tile.qml` for the exact import and shadow property syntax used in this codebase.

**Step 3: Rewrite HomeMenu.qml as HomeScreen**

```qml
// qml/applications/home/HomeMenu.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: homeScreen
    anchors.fill: parent

    Component.onCompleted: ApplicationController.setTitle("Home")

    property bool isLandscape: width > height

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.gap

        // Pane area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Landscape: main left, subs stacked right
            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap
                visible: homeScreen.isLandscape

                WidgetHost {
                    id: mainPaneLandscape
                    paneId: "main"
                    widgetSource: WidgetPlacementModel.qmlComponentForPane("main")
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * 0.6
                    onLongPressed: widgetPicker.openForPane("main")
                }

                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: UiMetrics.gap

                    WidgetHost {
                        id: sub1PaneLandscape
                        paneId: "sub1"
                        widgetSource: WidgetPlacementModel.qmlComponentForPane("sub1")
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onLongPressed: widgetPicker.openForPane("sub1")
                    }

                    WidgetHost {
                        id: sub2PaneLandscape
                        paneId: "sub2"
                        widgetSource: WidgetPlacementModel.qmlComponentForPane("sub2")
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onLongPressed: widgetPicker.openForPane("sub2")
                    }
                }
            }

            // Portrait: main top, subs side-by-side bottom
            ColumnLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap
                visible: !homeScreen.isLandscape

                WidgetHost {
                    id: mainPanePortrait
                    paneId: "main"
                    widgetSource: WidgetPlacementModel.qmlComponentForPane("main")
                    Layout.fillWidth: true
                    Layout.preferredHeight: parent.height * 0.6
                    onLongPressed: widgetPicker.openForPane("main")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: UiMetrics.gap

                    WidgetHost {
                        id: sub1PanePortrait
                        paneId: "sub1"
                        widgetSource: WidgetPlacementModel.qmlComponentForPane("sub1")
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        onLongPressed: widgetPicker.openForPane("sub1")
                    }

                    WidgetHost {
                        id: sub2PanePortrait
                        paneId: "sub2"
                        widgetSource: WidgetPlacementModel.qmlComponentForPane("sub2")
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        onLongPressed: widgetPicker.openForPane("sub2")
                    }
                }
            }
        }

        // Launcher dock
        LauncherDock {
            Layout.fillWidth: true
        }
    }

    // Refresh widget sources when placements change
    Connections {
        target: WidgetPlacementModel
        function onPaneChanged(paneId) {
            if (paneId === "main") {
                mainPaneLandscape.widgetSource = WidgetPlacementModel.qmlComponentForPane("main")
                mainPanePortrait.widgetSource = WidgetPlacementModel.qmlComponentForPane("main")
            } else if (paneId === "sub1") {
                sub1PaneLandscape.widgetSource = WidgetPlacementModel.qmlComponentForPane("sub1")
                sub1PanePortrait.widgetSource = WidgetPlacementModel.qmlComponentForPane("sub1")
            } else if (paneId === "sub2") {
                sub2PaneLandscape.widgetSource = WidgetPlacementModel.qmlComponentForPane("sub2")
                sub2PanePortrait.widgetSource = WidgetPlacementModel.qmlComponentForPane("sub2")
            }
        }
    }

    // Widget picker (Task 9 placeholder — just the Loader)
    Loader {
        id: widgetPicker
        anchors.fill: parent
        active: false
        property string targetPaneId: ""

        function openForPane(paneId) {
            targetPaneId = paneId
            active = true
        }
    }
}
```

**Step 4: Update Shell.qml**

Replace the `LauncherMenu` block (lines 36-41) with the home screen. Since HomeMenu.qml is the same file (just rewritten), Shell.qml's existing reference still works. But we need to update the visibility logic — the home screen should show whenever no plugin is active and we're not in settings:

The existing visibility binding `!PluginModel.activePluginId && ApplicationController.currentApplication !== 6` already does this correctly for `LauncherMenu`. Since we're rewriting HomeMenu.qml in-place (same file path, same QML type name), Shell.qml just needs to reference HomeMenu instead of LauncherMenu:

Replace lines 36-41 in Shell.qml:
```qml
            // Home screen with widget panes and launcher dock
            HomeMenu {
                id: homeView
                anchors.fill: parent
                visible: !PluginModel.activePluginId
                         && ApplicationController.currentApplication !== 6
            }
```

Remove the old LauncherMenu import if it was separate (it wasn't — it was referenced by type name through the QML module).

**Step 5: Register new QML files in CMakeLists.txt**

Add `set_source_files_properties` blocks for the new QML files and add them to `qt_add_qml_module` QML_FILES list:

```cmake
set_source_files_properties(../qml/components/WidgetHost.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "WidgetHost"
    QT_RESOURCE_ALIAS "WidgetHost.qml"
)
set_source_files_properties(../qml/components/LauncherDock.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "LauncherDock"
    QT_RESOURCE_ALIAS "LauncherDock.qml"
)
```

Add to QML_FILES list:
```
        ../qml/components/WidgetHost.qml
        ../qml/components/LauncherDock.qml
```

Note: Keep `LauncherMenu.qml` in the module for now — it's no longer used by Shell but removing it is a separate cleanup.

**Step 6: Build and test**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: Build succeeds, all tests pass

**Step 7: Commit**

```bash
git add qml/applications/home/HomeMenu.qml qml/components/WidgetHost.qml qml/components/LauncherDock.qml qml/components/Shell.qml src/CMakeLists.txt
git commit -m "feat(widget): responsive 3-pane home screen with launcher dock"
```

---

## Task 9: Widget Picker Overlay

**Files:**
- Create: `qml/components/WidgetPicker.qml`
- Modify: `qml/applications/home/HomeMenu.qml` (wire picker Loader)
- Create: `src/ui/WidgetPickerModel.hpp`
- Create: `src/ui/WidgetPickerModel.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `src/main.cpp` (register WidgetPickerModel)
- Modify: `tests/CMakeLists.txt`

**Step 1: Create WidgetPickerModel (C++ model for QML)**

This is a QAbstractListModel that wraps `WidgetRegistry::widgetsForSize()` with roles for the picker:

```cpp
// src/ui/WidgetPickerModel.hpp
#pragma once

#include <QAbstractListModel>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class WidgetRegistry;

class WidgetPickerModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        WidgetIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        IconNameRole
    };

    explicit WidgetPickerModel(WidgetRegistry* registry, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void filterForSize(int sizeFlag);

private:
    WidgetRegistry* registry_;
    QList<WidgetDescriptor> filtered_;
};

} // namespace oap
```

```cpp
// src/ui/WidgetPickerModel.cpp
#include "ui/WidgetPickerModel.hpp"
#include "core/widget/WidgetRegistry.hpp"

namespace oap {

WidgetPickerModel::WidgetPickerModel(WidgetRegistry* registry, QObject* parent)
    : QAbstractListModel(parent), registry_(registry) {}

int WidgetPickerModel::rowCount(const QModelIndex&) const {
    return filtered_.size();
}

QVariant WidgetPickerModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= filtered_.size())
        return {};
    const auto& desc = filtered_[index.row()];
    switch (role) {
        case WidgetIdRole: return desc.id;
        case DisplayNameRole: return desc.displayName;
        case IconNameRole: return desc.iconName;
        default: return {};
    }
}

QHash<int, QByteArray> WidgetPickerModel::roleNames() const {
    return {
        {WidgetIdRole, "widgetId"},
        {DisplayNameRole, "displayName"},
        {IconNameRole, "iconName"}
    };
}

void WidgetPickerModel::filterForSize(int sizeFlag) {
    beginResetModel();
    filtered_ = registry_->widgetsForSize(static_cast<WidgetSize>(sizeFlag));
    endResetModel();
}

} // namespace oap
```

**Step 2: Write WidgetPicker.qml**

```qml
// qml/components/WidgetPicker.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: picker
    anchors.fill: parent

    property string targetPaneId: ""
    signal widgetSelected(string widgetId)
    signal cleared()
    signal cancelled()

    // Dim background
    Rectangle {
        anchors.fill: parent
        color: ThemeService.scrim
        opacity: 0.4

        MouseArea {
            anchors.fill: parent
            onClicked: picker.cancelled()
        }
    }

    // Picker panel
    Rectangle {
        id: panel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        height: UiMetrics.tileH * 0.7
        radius: UiMetrics.radiusLarge
        color: ThemeService.surfaceContainerHighest

        // Slide up animation
        y: parent.height
        Component.onCompleted: y = parent.height - height - UiMetrics.marginPage

        Behavior on y {
            NumberAnimation { duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.spacing
            spacing: UiMetrics.spacing

            // Clear button
            Rectangle {
                width: UiMetrics.tileW * 0.4
                Layout.fillHeight: true
                radius: UiMetrics.radiusSmall
                color: clearMa.pressed ? ThemeService.errorContainer : ThemeService.surfaceContainer

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: UiMetrics.spacing * 0.25

                    MaterialIcon {
                        text: "\ue5cd"  // close
                        font.pixelSize: UiMetrics.iconSmall
                        color: ThemeService.error
                        Layout.alignment: Qt.AlignHCenter
                    }
                    NormalText {
                        text: "Clear"
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.error
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                MouseArea {
                    id: clearMa
                    anchors.fill: parent
                    onClicked: picker.cleared()
                }
            }

            // Widget options
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: ListView.Horizontal
                spacing: UiMetrics.spacing
                clip: true

                model: WidgetPickerModel

                delegate: Rectangle {
                    width: UiMetrics.tileW * 0.4
                    height: ListView.view.height
                    radius: UiMetrics.radiusSmall
                    color: optionMa.pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainer

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: UiMetrics.spacing * 0.25

                        MaterialIcon {
                            text: model.iconName
                            font.pixelSize: UiMetrics.iconSmall
                            color: ThemeService.onSurface
                            Layout.alignment: Qt.AlignHCenter
                        }
                        NormalText {
                            text: model.displayName
                            font.pixelSize: UiMetrics.fontSmall
                            color: ThemeService.onSurface
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }

                    MouseArea {
                        id: optionMa
                        anchors.fill: parent
                        onClicked: picker.widgetSelected(model.widgetId)
                    }
                }
            }
        }
    }
}
```

Note: The `MaterialIcon` `text` property uses icon codepoints in this codebase. The `iconName` from WidgetDescriptor is a Material Symbols name (e.g., "schedule"), but MaterialIcon expects codepoints (e.g., "\ue8b5"). The implementer needs to verify how MaterialIcon works in this codebase and decide whether to use codepoints or names in WidgetDescriptor. Check `qml/controls/MaterialIcon.qml` for the expected format.

**Step 3: Wire picker into HomeScreen**

Update the picker Loader in HomeMenu.qml to load WidgetPicker.qml:

```qml
    Loader {
        id: widgetPicker
        anchors.fill: parent
        active: false
        z: 50
        property string targetPaneId: ""

        function openForPane(paneId) {
            targetPaneId = paneId
            // Filter picker model for pane size
            var sizeFlag = (paneId === "main") ? 1 : 2  // WidgetSize::Main=1, Sub=2
            WidgetPickerModel.filterForSize(sizeFlag)
            active = true
        }

        sourceComponent: WidgetPicker {
            targetPaneId: widgetPicker.targetPaneId
            onWidgetSelected: function(widgetId) {
                WidgetPlacementModel.swapWidget(widgetPicker.targetPaneId, widgetId)
                widgetPicker.active = false
            }
            onCleared: {
                WidgetPlacementModel.clearPane(widgetPicker.targetPaneId)
                widgetPicker.active = false
            }
            onCancelled: widgetPicker.active = false
        }
    }
```

**Step 4: Register in main.cpp**

After creating WidgetPlacementModel:
```cpp
    auto widgetPickerModel = new oap::WidgetPickerModel(widgetRegistry, &app);
    engine.rootContext()->setContextProperty("WidgetPickerModel", widgetPickerModel);
```

**Step 5: Register QML file in CMake**

```cmake
set_source_files_properties(../qml/components/WidgetPicker.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "WidgetPicker"
    QT_RESOURCE_ALIAS "WidgetPicker.qml"
)
```

Add to QML_FILES list.

**Step 6: Add to CMake (C++ sources)**

Add `ui/WidgetPickerModel.cpp` to openauto-core in `src/CMakeLists.txt`.

**Step 7: Build and test**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: Build succeeds, all tests pass

**Step 8: Commit**

```bash
git add qml/components/WidgetPicker.qml qml/applications/home/HomeMenu.qml src/ui/WidgetPickerModel.hpp src/ui/WidgetPickerModel.cpp src/main.cpp src/CMakeLists.txt
git commit -m "feat(widget): add widget picker overlay with size-filtered selection"
```

---

## Task 10: Clock Widget (First Standalone Widget)

**Files:**
- Create: `qml/widgets/ClockWidget.qml`
- Modify: `src/CMakeLists.txt` (register QML file)

**Step 1: Create ClockWidget.qml**

```qml
// qml/widgets/ClockWidget.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: clockWidget

    // WidgetInstanceContext is injected into our QQmlContext
    // Access paneSize to adapt layout
    property bool isMainPane: typeof widgetContext !== "undefined"
                              ? widgetContext.paneSize === 1 : true  // 1 = Main

    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainer

        ColumnLayout {
            anchors.centerIn: parent
            spacing: isMainPane ? UiMetrics.spacing : UiMetrics.spacing * 0.5

            NormalText {
                id: timeText
                font.pixelSize: isMainPane ? UiMetrics.fontHeading * 2.5 : UiMetrics.fontHeading * 1.5
                font.weight: Font.Light
                color: ThemeService.onSurface
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                id: dateText
                font.pixelSize: isMainPane ? UiMetrics.fontTitle : UiMetrics.fontBody
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                id: dayText
                font.pixelSize: isMainPane ? UiMetrics.fontBody : UiMetrics.fontSmall
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
                visible: isMainPane
            }
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            timeText.text = now.toLocaleTimeString(Qt.locale(), "h:mm")
            dateText.text = now.toLocaleDateString(Qt.locale(), "MMMM d, yyyy")
            dayText.text = now.toLocaleDateString(Qt.locale(), "dddd")
        }
    }
}
```

**Step 2: Register in CMake**

```cmake
set_source_files_properties(../qml/widgets/ClockWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ClockWidget"
    QT_RESOURCE_ALIAS "ClockWidget.qml"
)
```

Add to QML_FILES list in `qt_add_qml_module`.

**Step 3: Build**

Run: `cd build && cmake .. && make -j$(nproc)`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add qml/widgets/ClockWidget.qml src/CMakeLists.txt
git commit -m "feat(widget): add Clock widget with adaptive main/sub layout"
```

---

## Task 11: AA Status Widget (Second Standalone Widget)

**Files:**
- Create: `qml/widgets/AAStatusWidget.qml`
- Modify: `src/CMakeLists.txt`

**Step 1: Create AAStatusWidget.qml**

```qml
// qml/widgets/AAStatusWidget.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: aaStatusWidget

    property bool isMainPane: typeof widgetContext !== "undefined"
                              ? widgetContext.paneSize === 1 : false
    property bool connected: typeof AAOrchestrator !== "undefined"
                             && AAOrchestrator.sessionActive

    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainer

        ColumnLayout {
            anchors.centerIn: parent
            spacing: UiMetrics.spacing

            MaterialIcon {
                text: connected ? "\ue531" : "\ue55d"  // phonelink / phonelink_off
                font.pixelSize: isMainPane ? UiMetrics.iconSize * 2 : UiMetrics.iconSize
                color: connected ? ThemeService.primary : ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                text: connected ? "Connected" : "Tap to connect"
                font.pixelSize: isMainPane ? UiMetrics.fontTitle : UiMetrics.fontBody
                color: ThemeService.onSurface
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                text: "Android Auto"
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
                visible: isMainPane
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (!connected)
                    PluginModel.setActivePlugin("org.openauto.android-auto")
            }
        }
    }
}
```

Note: The implementer should verify `AAOrchestrator.sessionActive` is the correct property name for AA connection state. Check `src/core/aa/AndroidAutoOrchestrator.cpp` for the actual Q_PROPERTY exposed. Adjust the binding if the property name differs.

**Step 2: Register in CMake**

Same pattern as ClockWidget — add `set_source_files_properties` and to QML_FILES.

**Step 3: Build and commit**

```bash
git add qml/widgets/AAStatusWidget.qml src/CMakeLists.txt
git commit -m "feat(widget): add AA Status widget with connection state and tap-to-connect"
```

---

## Task 12: BtAudio Now Playing Widget (Plugin-Provided)

**Files:**
- Create: `qml/widgets/NowPlayingWidget.qml`
- Modify: `src/plugins/bt_audio/BtAudioPlugin.cpp` (add widgetDescriptors override)
- Modify: `src/plugins/bt_audio/BtAudioPlugin.hpp` (if exists, or .cpp header section)
- Modify: `src/CMakeLists.txt`

**Step 1: Add widgetDescriptors() to BtAudioPlugin**

The implementer should read `src/plugins/bt_audio/BtAudioPlugin.hpp` and `BtAudioPlugin.cpp` first to understand the current class structure. Add:

```cpp
QList<oap::WidgetDescriptor> widgetDescriptors() const override {
    oap::WidgetDescriptor desc;
    desc.id = "org.openauto.bt-now-playing";
    desc.displayName = "Now Playing";
    desc.iconName = "music_note";  // or codepoint — check MaterialIcon format
    desc.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;
    desc.qmlComponent = QUrl("qrc:/qt/qml/OpenAutoProdigy/NowPlayingWidget.qml");
    desc.pluginId = id();
    return {desc};
}
```

**Step 2: Create NowPlayingWidget.qml**

The implementer should read `qml/applications/bt_audio/BtAudioView.qml` first to understand which properties the BtAudioPlugin exposes to QML (track title, artist, album art, playback state, AVRCP commands). The widget is a compact version of the full player view.

```qml
// qml/widgets/NowPlayingWidget.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: nowPlayingWidget

    property bool isMainPane: typeof widgetContext !== "undefined"
                              ? widgetContext.paneSize === 1 : false

    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainer

        // Main pane: album art + track info + controls
        // Sub pane: compact strip — icon + track/artist + play/pause

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.spacing
            spacing: UiMetrics.spacing * 0.5
            visible: isMainPane

            // TODO: implementer must check BtAudioPlugin QML context properties
            // Expected: trackTitle, artistName, albumArt, isPlaying, etc.
            // Wire to actual plugin properties

            NormalText {
                text: "No music playing"
                font.pixelSize: UiMetrics.fontTitle
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.spacing
            spacing: UiMetrics.spacing
            visible: !isMainPane

            MaterialIcon {
                text: "\ue405"  // music_note
                font.pixelSize: UiMetrics.iconSmall
                color: ThemeService.onSurfaceVariant
            }

            NormalText {
                text: "No music playing"
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.onSurfaceVariant
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
    }
}
```

**Important:** This is a scaffold. The implementer MUST read `BtAudioView.qml` and `BtAudioPlugin.cpp` to discover the actual QML context properties for track metadata and playback controls, then wire them into this widget. The exact property names (e.g., `btAudioTrack`, `btAudioArtist`, `btAudioPlaying`, `btAudioAlbumArt`) must come from the plugin's `onActivated()` context injection.

**Challenge:** Plugin QML context properties are only injected during `onActivated()` (when the plugin view is displayed). Widgets need access to this data WITHOUT the plugin being the active fullscreen view. The implementer will likely need to:
1. Move shared state (track metadata, playback state) to a service or singleton exposed globally, OR
2. Have the plugin expose a shared controller QObject via `createWidgetContext()`, OR
3. Expose the plugin's D-Bus monitor state through a global context property

This is the most architecturally interesting task. The implementer should decide the best approach based on how BtAudioPlugin currently manages its D-Bus connections and state.

**Step 3: Register in CMake and build**

**Step 4: Commit**

```bash
git add qml/widgets/NowPlayingWidget.qml src/plugins/bt_audio/BtAudioPlugin.hpp src/plugins/bt_audio/BtAudioPlugin.cpp src/CMakeLists.txt
git commit -m "feat(widget): add Now Playing widget from BtAudio plugin"
```

---

## Task 13: Home Screen Settings Page

**Files:**
- Create: `qml/applications/settings/HomeSettings.qml`
- Modify: `qml/applications/settings/SettingsMenu.qml` (add Home tile)
- Modify: `src/CMakeLists.txt`

This provides an alternative to long-press for configuring widget placements — a settings page where users can see their current layout and swap widgets via a picker.

**This task is optional for initial release.** The long-press picker (Task 9) is the primary UX. Defer this if the initial implementation is already large enough.

**Step 1: Create HomeSettings.qml**

A simple settings page showing the 3 pane slots with their current widget and a "Change" button for each.

**Step 2: Add tile to SettingsMenu.qml**

Add a "Home Screen" tile to the settings grid, following the existing pattern (AA, Display, Audio, Bluetooth, Companion, System).

**Step 3: Register in CMake, build, commit**

---

## Task 14: Integration Testing & Polish

**Files:**
- Various QML and C++ files as needed

**Step 1: Build on dev VM**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Verify: All tests pass, no warnings

**Step 2: Test on Pi**

Push to branch, pull on Pi, build and run:
```bash
git push origin dev/0.5.2
ssh matt@192.168.1.152 'cd ~/openauto-prodigy && git pull && cd build && cmake --build . -j3'
ssh matt@192.168.1.152 '~/openauto-prodigy/restart.sh'
```

Verify:
- Home screen shows 3-pane layout with clock + AA status + now playing
- Launcher dock renders at bottom with 4 items
- Tapping dock items launches plugins/settings
- Long-press a pane opens widget picker
- Selecting a widget swaps it
- Layout persists after restart

**Step 3: Polish pass**

- Adjust pane proportions if 60/40 doesn't look right on 1024x600
- Tune dock tile sizing for touch targets
- Verify theme colors apply correctly (day/night)
- Fix any MultiEffect shadow issues on Pi (Pi GPU may need different shadow params)

**Step 4: Commit any polish fixes**

```bash
git add -A
git commit -m "fix(widget): home screen polish and Pi compatibility adjustments"
```

---

## Execution Notes

### Task Dependencies

```
Task 1 (types) → Task 2 (registry) → Task 3 (instance context)
Task 1 (types) → Task 4 (IPlugin extension)
Task 1 (types) → Task 5 (config persistence)
Tasks 2,3,5 → Task 6 (placement model)
Tasks 2,6 → Task 7 (main.cpp wiring)
Task 7 → Task 8 (home screen QML)
Tasks 7,8 → Task 9 (widget picker)
Task 7 → Task 10 (clock widget)
Task 7 → Task 11 (AA status widget)
Tasks 7,10 → Task 12 (now playing widget — hardest task)
Task 8 → Task 13 (settings page — optional)
All → Task 14 (integration)
```

### Parallel Opportunities

- Tasks 1-5 are mostly independent (only Task 2 depends on Task 1 types)
- Tasks 10, 11, 12 can be done in parallel after Task 7
- Task 13 is optional and independent of widget QML files

### Risk Areas

- **Task 12 (Now Playing Widget):** Requires exposing BtAudio D-Bus state globally, not just in the plugin's QML context. This may need architectural discussion.
- **Task 8 (WidgetHost long-press):** MouseArea pressAndHold may interfere with widget content taps. May need a Timer-based approach instead.
- **Task 9 (Picker icon format):** MaterialIcon codepoints vs named icons — verify the existing pattern before using icon names in descriptors.
