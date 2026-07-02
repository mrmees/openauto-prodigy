# Architecture Extensibility Implementation Plan
> **Status:** NOT STARTED

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete Shell migration to plugin-driven architecture, add EventBus + ActionRegistry foundation, and build notification system — enabling hudiy-style extensibility without over-engineering.

**Architecture:** Four priority tiers transform the codebase from its current partially-migrated state (hardcoded Shell + global context hacks) to a fully plugin-driven system with event-based communication. Each tier builds on the previous. All changes maintain backward compatibility with the existing 3 static plugins.

**Review:** This plan was reviewed by Codex and corrected for: QML context propagation (PluginViewHost replaces Loader), test/lifecycle ownership alignment, icon encoding, role naming, MockHostContext updates, and PhonePlugin global removal timing.

**Process:** After each commit, use the Codex MCP tool (`mcp__codex__codex`) to review the committed changes. Pass the git diff of the commit and ask Codex to verify correctness, check for issues the plan may have missed, and confirm alignment with the plan. Fix any issues Codex identifies before moving to the next task.

**Tech Stack:** Qt 6 (6.4+ compatible), C++17, QML, yaml-cpp, CMake, Boost.Log, QTest

---

## Priority 1: Shell Migration + Config-Driven Launcher

The Shell currently uses hardcoded `ApplicationTypes` enum, global `AndroidAutoService` context property for fullscreen detection, and static `Component` entries for navigation. `PluginModel` and `PluginRuntimeContext` exist but aren't wired into Shell. This priority completes that wiring.

### Task 1: Add activation/deactivation methods to PluginManager

PluginModel needs to tell PluginManager when plugins activate/deactivate so PluginRuntimeContext can be managed. Currently PluginManager has no activate/deactivate concept — only init/shutdown.

**Files:**
- Modify: `src/core/plugin/PluginManager.hpp`
- Modify: `src/core/plugin/PluginManager.cpp`
- Modify: `tests/test_plugin_manager.cpp`

**Step 1: Write the failing test**

Add to `tests/test_plugin_manager.cpp`:

```cpp
void TestPluginManager::testActivateDeactivate()
{
    // Create mock plugin
    auto* plugin = new MockPlugin("test.activate", "Test", this);
    manager_->registerStaticPlugin(plugin);
    manager_->initializeAll(&hostContext_);

    // Spy on signals (PluginManager tracks state + emits signals;
    // onActivated()/onDeactivated() are called by UI layer, not manager)
    QSignalSpy activatedSpy(manager_, &oap::PluginManager::pluginActivated);
    QSignalSpy deactivatedSpy(manager_, &oap::PluginManager::pluginDeactivated);

    // Activate
    QVERIFY(manager_->activatePlugin("test.activate"));
    QCOMPARE(manager_->activePluginId(), QString("test.activate"));
    QCOMPARE(activatedSpy.count(), 1);
    QCOMPARE(activatedSpy.first().first().toString(), QString("test.activate"));

    // Activate different plugin deactivates previous
    auto* plugin2 = new MockPlugin("test.other", "Other", this);
    manager_->registerStaticPlugin(plugin2);
    manager_->initializeAll(&hostContext_);
    QVERIFY(manager_->activatePlugin("test.other"));
    QCOMPARE(manager_->activePluginId(), QString("test.other"));
    QCOMPARE(deactivatedSpy.count(), 1);  // previous was deactivated
    QCOMPARE(activatedSpy.count(), 2);

    // Deactivate
    manager_->deactivateCurrentPlugin();
    QCOMPARE(manager_->activePluginId(), QString());
    QCOMPARE(deactivatedSpy.count(), 2);

    // Activate nonexistent plugin fails
    QVERIFY(!manager_->activatePlugin("nonexistent"));
    QCOMPARE(manager_->activePluginId(), QString());
}
```

Note: PluginManager does NOT call `onActivated()`/`onDeactivated()`. It tracks active state and emits signals. The UI layer (`PluginViewHost`/`PluginRuntimeContext`) listens to these signals and handles the QML context lifecycle.

**Step 2: Run test to verify it fails**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R test_plugin_manager -V`
Expected: Compile error — `activatePlugin`, `activePluginId`, `deactivateCurrentPlugin` don't exist.

**Step 3: Implement activation in PluginManager**

Add to `PluginManager.hpp`:

```cpp
/// Activate a plugin by ID. Deactivates any currently active plugin first.
/// Returns false if plugin not found or not initialized.
bool activatePlugin(const QString& pluginId);

/// Deactivate the currently active plugin.
void deactivateCurrentPlugin();

/// Get the currently active plugin ID (empty if none).
QString activePluginId() const;

signals:
    void pluginActivated(const QString& pluginId);
    void pluginDeactivated(const QString& pluginId);

private:
    QString activePluginId_;
```

Add to `PluginManager.cpp`:

```cpp
bool PluginManager::activatePlugin(const QString& pluginId)
{
    auto it = idIndex_.find(pluginId);
    if (it == idIndex_.end()) return false;
    auto& entry = entries_[it.value()];
    if (!entry.initialized) return false;

    if (activePluginId_ == pluginId) return true;

    // Deactivate current
    deactivateCurrentPlugin();

    activePluginId_ = pluginId;
    // onActivated() is called by the UI layer (PluginRuntimeContext) which owns the QML context
    emit pluginActivated(pluginId);
    return true;
}

void PluginManager::deactivateCurrentPlugin()
{
    if (activePluginId_.isEmpty()) return;
    QString prev = activePluginId_;
    activePluginId_.clear();
    emit pluginDeactivated(prev);
}

QString PluginManager::activePluginId() const
{
    return activePluginId_;
}
```

**Step 4: Run test to verify it passes**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R test_plugin_manager -V`
Expected: PASS

**Step 5: Commit**

```bash
git add src/core/plugin/PluginManager.hpp src/core/plugin/PluginManager.cpp tests/test_plugin_manager.cpp
git commit -m "feat: add activate/deactivate lifecycle to PluginManager"
```

---

### Task 2: Wire PluginModel to use PluginManager activation

Currently `PluginModel::setActivePlugin()` just stores a string. It needs to actually tell PluginManager to activate/deactivate, and manage `PluginRuntimeContext` for the QML context lifecycle.

**Files:**
- Modify: `src/ui/PluginModel.hpp`
- Modify: `src/ui/PluginModel.cpp`

**Step 1: Add QQmlEngine dependency and PluginRuntimeContext management**

In `PluginModel.hpp`, add:

```cpp
#include <QQmlEngine>
class PluginRuntimeContext;

// In public:
    explicit PluginModel(PluginManager* manager, QQmlEngine* engine, QObject* parent = nullptr);

// In private:
    QQmlEngine* engine_;
    PluginRuntimeContext* activeContext_ = nullptr;
```

**Step 2: Update setActivePlugin() to manage PluginRuntimeContext**

In `PluginModel.cpp`:

```cpp
PluginModel::PluginModel(PluginManager* manager, QQmlEngine* engine, QObject* parent)
    : QAbstractListModel(parent)
    , manager_(manager)
    , engine_(engine)
{
    connect(manager_, &PluginManager::pluginInitialized, this, [this]() {
        beginResetModel();
        endResetModel();
    });
}

void PluginModel::setActivePlugin(const QString& pluginId)
{
    if (activePluginId_ == pluginId) return;

    // Empty ID = go home (deactivate current, show launcher)
    if (pluginId.isEmpty()) {
        if (activeContext_) {
            activeContext_->deactivate();
            delete activeContext_;
            activeContext_ = nullptr;
        }
        activePluginId_.clear();
        manager_->deactivateCurrentPlugin();
        emit activePluginChanged();
        emit dataChanged(index(0), index(rowCount() - 1), {IsActiveRole});
        return;
    }

    // Validate: only update state if manager accepts the activation
    if (!manager_->activatePlugin(pluginId)) return;

    // Deactivate current
    if (activeContext_) {
        activeContext_->deactivate();
        delete activeContext_;
        activeContext_ = nullptr;
    }

    activePluginId_ = pluginId;

    // Activate new
    auto* plugin = manager_->plugin(pluginId);
    if (plugin && engine_) {
        activeContext_ = new PluginRuntimeContext(plugin, engine_, this);
        activeContext_->activate();
    }

    emit activePluginChanged();
    emit dataChanged(index(0), index(rowCount() - 1), {IsActiveRole});
}
```

**Step 3: Update main.cpp constructor call**

In `main.cpp`, change:
```cpp
auto pluginModel = new oap::PluginModel(&pluginManager, &app);
```
to:
```cpp
auto pluginModel = new oap::PluginModel(&pluginManager, &engine, &app);
```

Note: `pluginModel` creation must move to after `engine` construction (line 100). Move lines 98-99 to after line 100.

**Step 4: Build and verify existing tests still pass**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass. (PluginModel isn't unit-tested directly but nothing should break.)

**Step 5: Commit**

```bash
git add src/ui/PluginModel.hpp src/ui/PluginModel.cpp src/main.cpp
git commit -m "feat: wire PluginModel to manage PluginRuntimeContext lifecycle"
```

---

### Task 2b: Add test_plugin_model unit tests

PluginModel's constructor signature changed and its activation logic is now the critical path. Add dedicated tests.

**Files:**
- Create: `tests/test_plugin_model.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write tests**

```cpp
#include <QTest>
#include <QSignalSpy>
#include <QQmlEngine>
#include "ui/PluginModel.hpp"
#include "core/plugin/PluginManager.hpp"
#include "core/plugin/IPlugin.hpp"
#include "core/plugin/HostContext.hpp"

// MockPlugin as in test_plugin_manager.cpp

class TestPluginModel : public QObject {
    Q_OBJECT
private slots:
    void testRowCountMatchesPlugins()
    {
        oap::PluginManager manager;
        QQmlEngine engine;
        auto* p = new MockPlugin("test.a", "A", this);
        manager.registerStaticPlugin(p);
        oap::HostContext ctx;
        manager.initializeAll(&ctx);
        oap::PluginModel model(&manager, &engine);
        QCOMPARE(model.rowCount(), 1);
    }

    void testSetActivePluginValid()
    {
        oap::PluginManager manager;
        QQmlEngine engine;
        auto* p = new MockPlugin("test.a", "A", this);
        manager.registerStaticPlugin(p);
        oap::HostContext ctx;
        manager.initializeAll(&ctx);
        oap::PluginModel model(&manager, &engine);

        QSignalSpy spy(&model, &oap::PluginModel::activePluginChanged);
        model.setActivePlugin("test.a");
        QCOMPARE(model.activePluginId(), QString("test.a"));
        QCOMPARE(spy.count(), 1);
    }

    void testSetActivePluginInvalid()
    {
        oap::PluginManager manager;
        QQmlEngine engine;
        oap::PluginModel model(&manager, &engine);

        model.setActivePlugin("nonexistent");
        QCOMPARE(model.activePluginId(), QString());  // unchanged
    }

    void testSetActivePluginEmpty()
    {
        oap::PluginManager manager;
        QQmlEngine engine;
        auto* p = new MockPlugin("test.a", "A", this);
        manager.registerStaticPlugin(p);
        oap::HostContext ctx;
        manager.initializeAll(&ctx);
        oap::PluginModel model(&manager, &engine);

        model.setActivePlugin("test.a");
        model.setActivePlugin(QString());  // go home
        QCOMPARE(model.activePluginId(), QString());
    }
};

QTEST_MAIN(TestPluginModel)
#include "test_plugin_model.moc"
```

**Step 2: Add to tests/CMakeLists.txt and run**

```cmake
add_executable(test_plugin_model
    test_plugin_model.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PluginModel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PluginRuntimeContext.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginDiscovery.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/PluginManifest.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin/HostContext.cpp
)
target_include_directories(test_plugin_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_plugin_model PRIVATE
    Qt6::Core Qt6::Quick Qt6::Test yaml-cpp::yaml-cpp Boost::log)
add_test(NAME test_plugin_model COMMAND test_plugin_model)
```

**Step 3: Commit**

```bash
git add tests/test_plugin_model.cpp tests/CMakeLists.txt
git commit -m "test: add PluginModel unit tests for activation lifecycle"
```

---

### Task 2c: Create PluginViewHost for C++-managed plugin QML loading

**Critical design issue:** A QML `Loader { source: url }` loads components in the parent (root) context, NOT in the child context created by `PluginRuntimeContext`. Plugin QML views (e.g., `AndroidAutoMenu.qml`) rely on context properties like `AndroidAutoService` that are set on the child context. A `Loader` would fail to resolve them.

**Solution:** `PluginViewHost` is a C++ class that:
1. Takes ownership of a host `QQuickItem` in Shell
2. Uses `QQmlComponent::create(childContext)` to instantiate plugin QML with the correct context
3. Parents the resulting `QQuickItem` to the host item
4. Destroys the previous plugin item on deactivation before context is destroyed

**Ownership/lifecycle rules:**
- Shell owns the host `QQuickItem`
- `PluginViewHost` owns the created plugin `QQuickItem` (destroys on deactivation)
- `PluginRuntimeContext::deactivate()` is called AFTER the plugin item is destroyed
- On component creation failure: log error, show launcher, don't crash

**Files:**
- Create: `src/ui/PluginViewHost.hpp`
- Create: `src/ui/PluginViewHost.cpp`
- Modify: `src/ui/PluginModel.hpp` (add PluginViewHost dependency)
- Modify: `src/ui/PluginModel.cpp` (delegate view creation to PluginViewHost)
- Modify: `src/CMakeLists.txt`

**Step 1: Create PluginViewHost**

`src/ui/PluginViewHost.hpp`:

```cpp
#pragma once

#include <QObject>
#include <QUrl>

class QQmlEngine;
class QQmlContext;
class QQuickItem;

namespace oap {

/// Manages plugin QML view instantiation with the correct child context.
/// Shell provides a host QQuickItem; PluginViewHost creates/destroys
/// plugin views as children of that host.
class PluginViewHost : public QObject {
    Q_OBJECT
public:
    explicit PluginViewHost(QQmlEngine* engine, QObject* parent = nullptr);

    /// Set the QML host item (the container in Shell where plugin views go)
    void setHostItem(QQuickItem* host);

    /// Load a plugin's QML component into the host using the given context.
    /// Returns true on success.
    bool loadView(const QUrl& qmlUrl, QQmlContext* pluginContext);

    /// Destroy the current plugin view (must be called before context deactivation).
    void clearView();

    bool hasView() const { return activeView_ != nullptr; }

signals:
    void viewLoaded();
    void viewCleared();
    void viewLoadFailed(const QString& error);

private:
    QQmlEngine* engine_;
    QQuickItem* hostItem_ = nullptr;
    QQuickItem* activeView_ = nullptr;
};

} // namespace oap
```

`src/ui/PluginViewHost.cpp`:

```cpp
#include "PluginViewHost.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <boost/log/trivial.hpp>

namespace oap {

PluginViewHost::PluginViewHost(QQmlEngine* engine, QObject* parent)
    : QObject(parent), engine_(engine) {}

void PluginViewHost::setHostItem(QQuickItem* host)
{
    hostItem_ = host;
}

bool PluginViewHost::loadView(const QUrl& qmlUrl, QQmlContext* pluginContext)
{
    if (!hostItem_ || !engine_ || !pluginContext) return false;

    clearView();

    QQmlComponent component(engine_, qmlUrl);
    if (component.isError()) {
        auto err = component.errorString();
        BOOST_LOG_TRIVIAL(error) << "[PluginViewHost] Failed to load "
                                  << qmlUrl.toString().toStdString() << ": "
                                  << err.toStdString();
        emit viewLoadFailed(err);
        return false;
    }

    auto* obj = component.create(pluginContext);
    if (!obj) {
        emit viewLoadFailed("Component::create() returned null");
        return false;
    }

    activeView_ = qobject_cast<QQuickItem*>(obj);
    if (!activeView_) {
        delete obj;
        emit viewLoadFailed("Created object is not a QQuickItem");
        return false;
    }

    // Parent to host and fill it
    activeView_->setParentItem(hostItem_);
    activeView_->setWidth(hostItem_->width());
    activeView_->setHeight(hostItem_->height());

    // Track host resizes
    connect(hostItem_, &QQuickItem::widthChanged, activeView_, [this]() {
        if (activeView_ && hostItem_)
            activeView_->setWidth(hostItem_->width());
    });
    connect(hostItem_, &QQuickItem::heightChanged, activeView_, [this]() {
        if (activeView_ && hostItem_)
            activeView_->setHeight(hostItem_->height());
    });

    emit viewLoaded();
    return true;
}

void PluginViewHost::clearView()
{
    if (activeView_) {
        delete activeView_;
        activeView_ = nullptr;
        emit viewCleared();
    }
}

} // namespace oap
```

**Step 2: Update PluginModel to use PluginViewHost**

In `PluginModel.hpp`, add `PluginViewHost* viewHost_` member.
In `setActivePlugin()`, after `activeContext_->activate()`:

```cpp
if (viewHost_ && !pluginId.isEmpty()) {
    auto* plugin = manager_->plugin(pluginId);
    if (plugin && activeContext_) {
        if (!viewHost_->loadView(plugin->qmlComponent(), activeContext_->qmlContext())) {
            // Fallback: deactivate and go home
            activeContext_->deactivate();
            delete activeContext_;
            activeContext_ = nullptr;
            activePluginId_.clear();
            manager_->deactivateCurrentPlugin();
        }
    }
}
```

In the deactivation path (both empty-ID and switch cases), call `viewHost_->clearView()` BEFORE `activeContext_->deactivate()`.

**Step 3: Add to CMakeLists and build**

**Step 4: Commit**

```bash
git add src/ui/PluginViewHost.hpp src/ui/PluginViewHost.cpp src/ui/PluginModel.hpp src/ui/PluginModel.cpp src/CMakeLists.txt
git commit -m "feat: add PluginViewHost for context-aware plugin QML loading"
```

---

### Task 3: Add launcher config to YAML schema

Launcher tiles should come from config, not hardcoded QML. Add a `launcher` section to the YAML config with tile definitions.

**Files:**
- Modify: `src/core/YamlConfig.hpp`
- Modify: `src/core/YamlConfig.cpp`
- Modify: `tests/test_yaml_config.cpp`

**Step 1: Write the failing test**

Add to `tests/test_yaml_config.cpp`:

```cpp
void TestYamlConfig::testLauncherTiles()
{
    oap::YamlConfig config;  // defaults only

    auto tiles = config.launcherTiles();
    // Default should include at least AA and Settings
    QVERIFY(tiles.size() >= 2);

    // Each tile has required fields
    auto first = tiles.first();
    QVERIFY(!first.value("id").toString().isEmpty());
    QVERIFY(!first.value("label").toString().isEmpty());
    QVERIFY(!first.value("icon").toString().isEmpty());
    QVERIFY(!first.value("action").toString().isEmpty());
}
```

**Step 2: Run test to verify it fails**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R test_yaml_config -V`
Expected: Compile error — `launcherTiles()` doesn't exist.

**Step 3: Add launcher config to YamlConfig**

In `YamlConfig.hpp`, add:

```cpp
    // Launcher tiles — each tile is a QVariantMap with {id, label, icon, action}
    QList<QVariantMap> launcherTiles() const;
    void setLauncherTiles(const QList<QVariantMap>& tiles);
```

In `YamlConfig.cpp`, add to `initDefaults()`:

```cpp
    // Launcher tiles
    YAML::Node launcher;
    YAML::Node tiles;

    // Icons: store actual UTF-8 encoded Material Icon codepoints.
    // These match the unicode escapes used in existing QML (e.g., "\ueff7").
    // QString::fromUtf8() handles the encoding correctly.
    YAML::Node aaTile;
    aaTile["id"] = "org.openauto.android-auto";
    aaTile["label"] = "Android Auto";
    aaTile["icon"] = QString(QChar(0xeff7)).toStdString();  // directions_car
    aaTile["action"] = "plugin:org.openauto.android-auto";
    tiles.push_back(aaTile);

    YAML::Node btTile;
    btTile["id"] = "org.openauto.bt-audio";
    btTile["label"] = "Music";
    btTile["icon"] = QString(QChar(0xf01f)).toStdString();  // headphones
    btTile["action"] = "plugin:org.openauto.bt-audio";
    tiles.push_back(btTile);

    YAML::Node phoneTile;
    phoneTile["id"] = "org.openauto.phone";
    phoneTile["label"] = "Phone";
    phoneTile["icon"] = QString(QChar(0xf0d4)).toStdString();  // phone
    phoneTile["action"] = "plugin:org.openauto.phone";
    tiles.push_back(phoneTile);

    YAML::Node settingsTile;
    settingsTile["id"] = "settings";
    settingsTile["label"] = "Settings";
    settingsTile["icon"] = QString(QChar(0xe8b8)).toStdString();  // settings
    settingsTile["action"] = "navigate:settings";
    tiles.push_back(settingsTile);

    launcher["tiles"] = tiles;
    root_["launcher"] = launcher;
```

Implement `launcherTiles()`:

```cpp
QList<QVariantMap> YamlConfig::launcherTiles() const
{
    QList<QVariantMap> result;
    auto tiles = root_["launcher"]["tiles"];
    if (!tiles.IsSequence()) return result;

    for (const auto& tile : tiles) {
        QVariantMap map;
        if (tile["id"]) map["id"] = QString::fromStdString(tile["id"].as<std::string>());
        if (tile["label"]) map["label"] = QString::fromStdString(tile["label"].as<std::string>());
        if (tile["icon"]) map["icon"] = QString::fromStdString(tile["icon"].as<std::string>());
        if (tile["action"]) map["action"] = QString::fromStdString(tile["action"].as<std::string>());
        result.append(map);
    }
    return result;
}
```

**Step 4: Run test to verify it passes**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R test_yaml_config -V`
Expected: PASS

**Step 5: Commit**

```bash
git add src/core/YamlConfig.hpp src/core/YamlConfig.cpp tests/test_yaml_config.cpp
git commit -m "feat: add launcher tiles configuration to YAML schema"
```

---

### Task 4: Create LauncherModel for QML

Expose launcher tiles to QML as a list model, backed by the YAML config.

**Files:**
- Create: `src/ui/LauncherModel.hpp`
- Create: `src/ui/LauncherModel.cpp`
- Modify: `src/CMakeLists.txt`

**Step 1: Create LauncherModel**

`src/ui/LauncherModel.hpp`:

```cpp
#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

namespace oap {

class YamlConfig;

/// Exposes launcher tiles to QML. Each tile has id, label, icon, action.
/// Backed by YamlConfig::launcherTiles().
class LauncherModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        TileIdRole = Qt::UserRole + 1,
        TileLabelRole,
        TileIconRole,
        TileActionRole
    };

    explicit LauncherModel(YamlConfig* config, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Reload tiles from config.
    Q_INVOKABLE void refresh();

private:
    YamlConfig* config_;
    QList<QVariantMap> tiles_;
};

} // namespace oap
```

`src/ui/LauncherModel.cpp`:

```cpp
#include "LauncherModel.hpp"
#include "core/YamlConfig.hpp"

namespace oap {

LauncherModel::LauncherModel(YamlConfig* config, QObject* parent)
    : QAbstractListModel(parent)
    , config_(config)
{
    refresh();
}

int LauncherModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return tiles_.size();
}

QVariant LauncherModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= tiles_.size())
        return {};

    const auto& tile = tiles_[index.row()];
    switch (role) {
    case TileIdRole:     return tile.value("id");
    case TileLabelRole:  return tile.value("label");
    case TileIconRole:   return tile.value("icon");
    case TileActionRole: return tile.value("action");
    default:             return {};
    }
}

QHash<int, QByteArray> LauncherModel::roleNames() const
{
    return {
        {TileIdRole,     "tileId"},
        {TileLabelRole,  "tileLabel"},
        {TileIconRole,   "tileIcon"},
        {TileActionRole, "tileAction"}
    };
}

void LauncherModel::refresh()
{
    beginResetModel();
    tiles_ = config_->launcherTiles();
    endResetModel();
}

} // namespace oap
```

**Step 2: Add to CMakeLists.txt**

In `src/CMakeLists.txt`, add `ui/LauncherModel.cpp` to the `qt_add_executable` source list, after `ui/PluginRuntimeContext.cpp`.

**Step 3: Build**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Compiles cleanly.

**Step 4: Commit**

```bash
git add src/ui/LauncherModel.hpp src/ui/LauncherModel.cpp src/CMakeLists.txt
git commit -m "feat: add LauncherModel for config-driven launcher tiles"
```

---

### Task 5: Rewrite Shell.qml to use PluginModel

This is the big one. Replace the hardcoded Component/StackView/enum-based navigation with a Loader driven by PluginModel, and replace BottomBar with a PluginModel-driven nav strip.

**Files:**
- Modify: `qml/components/Shell.qml`
- Create: `qml/components/NavStrip.qml`
- Modify: `src/CMakeLists.txt` (add NavStrip.qml to QML module)

**Step 1: Create NavStrip.qml**

`qml/components/NavStrip.qml`:

```qml
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: navStrip
    color: ThemeService.barBackgroundColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Repeater {
            model: PluginModel

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: navStrip.height * 1.2
                color: isActive ? ThemeService.highlightColor : "transparent"
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 2

                    MaterialIcon {
                        Layout.alignment: Qt.AlignHCenter
                        icon: pluginIcon || "\ue5c3"
                        size: 20
                        color: isActive ? ThemeService.highlightFontColor
                                        : ThemeService.normalFontColor
                    }

                    NormalText {
                        Layout.alignment: Qt.AlignHCenter
                        text: pluginName
                        font.pixelSize: 10
                        color: isActive ? ThemeService.highlightFontColor
                                        : ThemeService.normalFontColor
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: PluginModel.setActivePlugin(pluginId)
                }
            }
        }
    }
}
```

**Step 2: Rewrite Shell.qml**

Replace `qml/components/Shell.qml` entirely.

**Key design:** Plugin content is NOT loaded via QML `Loader` — that would use the root context, not the plugin's child context. Instead, `PluginViewHost` (C++) creates plugin QQuickItems with the correct context and parents them to `pluginContentHost`. Shell just provides the host Item; C++ manages the content.

```qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: shell
    anchors.fill: parent

    // Fullscreen: active plugin requested it (generic — not AA-specific)
    property bool fullscreenMode: PluginModel.activePluginFullscreen

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Status bar — ~8% height, hidden in fullscreen
        TopBar {
            Layout.fillWidth: true
            Layout.preferredHeight: shell.fullscreenMode ? 0 : shell.height * 0.08
            visible: !shell.fullscreenMode
        }

        // Content area — C++ manages plugin views via PluginViewHost
        Item {
            id: pluginContentHost
            objectName: "pluginContentHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Launcher is shown when no plugin view is loaded
            LauncherMenu {
                id: launcherView
                anchors.fill: parent
                visible: !PluginModel.activePluginId
            }
        }

        // Nav strip — plugin-model driven, hidden in fullscreen
        NavStrip {
            Layout.fillWidth: true
            Layout.preferredHeight: shell.fullscreenMode ? 0 : shell.height * 0.12
            visible: !shell.fullscreenMode
        }
    }

    // Gesture overlay (on top of everything)
    GestureOverlay {
        id: gestureOverlay
    }

    // Incoming call overlay (still uses global PhonePlugin until Priority 3)
    IncomingCallOverlay {
        id: callOverlay
    }
}
```

**In main.cpp**, after QML engine loads, find the host item and wire it:

```cpp
auto* rootObj = engine.rootObjects().first();
auto* hostItem = rootObj->findChild<QQuickItem*>("pluginContentHost");
if (hostItem)
    pluginModel->viewHost()->setHostItem(hostItem);
```

**Step 3: Register NavStrip.qml in CMakeLists.txt**

Add to `src/CMakeLists.txt` in the `set_source_files_properties` section:

```cmake
set_source_files_properties(../qml/components/NavStrip.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "NavStrip"
    QT_RESOURCE_ALIAS "NavStrip.qml"
)
```

And add `../qml/components/NavStrip.qml` to the `QML_FILES` list in `qt_add_qml_module`.

**Step 4: Build and smoke test**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Compiles. UI testing requires the Pi (QML rendering).

**Step 5: Commit**

```bash
git add qml/components/Shell.qml qml/components/NavStrip.qml src/CMakeLists.txt
git commit -m "feat: rewrite Shell to use PluginModel for nav and content loading"
```

---

### Task 6: Rewrite LauncherMenu.qml to use LauncherModel

Replace hardcoded tiles with model-driven repeater.

**Files:**
- Modify: `qml/applications/launcher/LauncherMenu.qml`
- Modify: `src/main.cpp` (expose LauncherModel to QML)

**Step 1: Wire LauncherModel in main.cpp**

After YAML config setup, before QML engine load:

```cpp
#include "ui/LauncherModel.hpp"
// ...
auto launcherModel = new oap::LauncherModel(yamlConfig.get(), &app);
// ... (after engine creation)
engine.rootContext()->setContextProperty("LauncherModel", launcherModel);
```

**Step 2: Rewrite LauncherMenu.qml**

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: launcherMenu

    GridLayout {
        anchors.centerIn: parent
        columns: 4
        rowSpacing: 20
        columnSpacing: 20

        Repeater {
            model: LauncherModel

            Tile {
                Layout.preferredWidth: launcherMenu.width * 0.18
                Layout.preferredHeight: launcherMenu.width * 0.18
                tileName: model.tileLabel
                tileIcon: model.tileIcon
                tileEnabled: true
                onClicked: {
                    var action = tileAction;
                    if (action.startsWith("plugin:")) {
                        PluginModel.setActivePlugin(action.substring(7));
                    } else if (action.startsWith("navigate:")) {
                        // Built-in navigation (settings, etc.)
                        var target = action.substring(9);
                        if (target === "settings") {
                            ApplicationController.navigateTo(6);
                        }
                    }
                }
            }
        }
    }
}
```

**Step 3: Build**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`
Expected: Compiles.

**Step 4: Commit**

```bash
git add qml/applications/launcher/LauncherMenu.qml src/main.cpp
git commit -m "feat: config-driven launcher tiles via LauncherModel"
```

---

### Task 7: Remove AA global context property hack from main.cpp

Now that Shell uses `PluginModel.activePluginFullscreen` instead of `AndroidAutoService.connectionState`, remove the AA transition shim. **Keep the PhonePlugin global** — `IncomingCallOverlay.qml` still depends on it until Priority 3 (Task 18).

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`

**Step 1: Remove AA global from main.cpp**

Delete only the AA line:

```cpp
// DELETE this line:
// aaPlugin->setGlobalContextProperties(engine.rootContext());

// KEEP this line until Priority 3:
engine.rootContext()->setContextProperty("PhonePlugin", phonePlugin);
```

**Step 2: Remove `setGlobalContextProperties` from AndroidAutoPlugin**

Remove the method declaration from `AndroidAutoPlugin.hpp` and the implementation from `AndroidAutoPlugin.cpp` (lines 144-153).

**Step 3: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`

Expected: Compiles and all tests pass. Shell.qml no longer references `AndroidAutoService` globally. IncomingCallOverlay still works via the PhonePlugin global (to be cleaned up in Task 18).

**Step 4: Commit**

```bash
git add src/main.cpp src/plugins/android_auto/AndroidAutoPlugin.hpp src/plugins/android_auto/AndroidAutoPlugin.cpp
git commit -m "fix: remove AA global context hack from main.cpp

Shell now uses PluginModel.activePluginFullscreen for fullscreen
detection. PhonePlugin global kept until notification system lands."
```

---

### Task 8: Update AndroidAutoPlugin to navigate via PluginModel

The AA plugin currently calls `appController_->navigateTo(ApplicationTypes::AndroidAuto)` on connect. It should activate itself via the plugin system instead.

**Files:**
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.hpp`
- Modify: `src/plugins/android_auto/AndroidAutoPlugin.cpp`

**Step 1: Replace ApplicationController with signal-based navigation**

Instead of taking `ApplicationController*` in the constructor, emit a signal that Shell/PluginModel can listen to.

In `AndroidAutoPlugin.hpp`, add:

```cpp
signals:
    void requestActivation();
    void requestDeactivation();
```

In `AndroidAutoPlugin.cpp`, replace the `connectionStateChanged` lambda (lines 43-58):

```cpp
QObject::connect(aaService_, &oap::aa::AndroidAutoService::connectionStateChanged,
                 this, [this]() {
    using CS = oap::aa::AndroidAutoService;
    if (aaService_->connectionState() == CS::Connected) {
        if (touchReader_) touchReader_->grab();
        emit requestActivation();
    } else if (aaService_->connectionState() == CS::Disconnected
               || aaService_->connectionState() == CS::WaitingForDevice) {
        if (touchReader_) touchReader_->ungrab();
        emit requestDeactivation();
    }
});
```

In `main.cpp`, after plugin registration, wire the signals:

```cpp
QObject::connect(aaPlugin, &oap::plugins::AndroidAutoPlugin::requestActivation,
                 pluginModel, [pluginModel]() {
    pluginModel->setActivePlugin("org.openauto.android-auto");
});
QObject::connect(aaPlugin, &oap::plugins::AndroidAutoPlugin::requestDeactivation,
                 pluginModel, [pluginModel]() {
    if (pluginModel->activePluginId() == "org.openauto.android-auto")
        pluginModel->setActivePlugin(QString());
});
```

**Step 2: Remove ApplicationController dependency from constructor**

Remove `appController` from the constructor signature. Remove `ApplicationTypes.hpp` include. Remove `appController_` member.

**Step 3: Build and verify**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 4: Commit**

```bash
git add src/plugins/android_auto/AndroidAutoPlugin.hpp src/plugins/android_auto/AndroidAutoPlugin.cpp src/main.cpp
git commit -m "refactor: AndroidAutoPlugin navigates via signals, not ApplicationController"
```

---

### Task 9: Deprecate ApplicationController enum-based navigation

Mark `navigateTo(int)` as deprecated. Settings navigation still uses it (from LauncherMenu action `navigate:settings`). Settings should become a plugin too, but for now keep the adapter.

**Files:**
- Modify: `src/ui/ApplicationController.hpp`

**Step 1: Add deprecation annotation**

```cpp
    /// @deprecated Use PluginModel::setActivePlugin() for plugin navigation.
    /// Kept for built-in screens (settings) that aren't yet plugins.
    Q_INVOKABLE void navigateTo(int appType);
```

No code changes — just the doc annotation. Settings migration to a plugin is a future task.

**Step 2: Commit**

```bash
git add src/ui/ApplicationController.hpp
git commit -m "docs: deprecate ApplicationController.navigateTo() in favor of PluginModel"
```

---

## Priority 2: EventBus + ActionRegistry

### Task 10: Implement concrete EventBus

**Files:**
- Create: `src/core/services/EventBus.hpp`
- Create: `src/core/services/EventBus.cpp`
- Create: `tests/test_event_bus.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing tests**

`tests/test_event_bus.cpp`:

```cpp
#include <QTest>
#include <QSignalSpy>
#include "core/services/EventBus.hpp"

class TestEventBus : public QObject {
    Q_OBJECT
private slots:
    void testSubscribeAndPublish()
    {
        oap::EventBus bus;
        QVariant received;
        int subId = bus.subscribe("test/topic", [&](const QVariant& v) { received = v; });

        bus.publish("test/topic", 42);
        // EventBus delivers via QueuedConnection — need event loop
        QCoreApplication::processEvents();

        QCOMPARE(received.toInt(), 42);
        QVERIFY(subId > 0);
    }

    void testUnsubscribe()
    {
        oap::EventBus bus;
        int count = 0;
        int subId = bus.subscribe("test/topic", [&](const QVariant&) { ++count; });

        bus.publish("test/topic");
        QCoreApplication::processEvents();
        QCOMPARE(count, 1);

        bus.unsubscribe(subId);
        bus.publish("test/topic");
        QCoreApplication::processEvents();
        QCOMPARE(count, 1);  // no change — unsubscribed
    }

    void testMultipleSubscribers()
    {
        oap::EventBus bus;
        int countA = 0, countB = 0;
        bus.subscribe("test/topic", [&](const QVariant&) { ++countA; });
        bus.subscribe("test/topic", [&](const QVariant&) { ++countB; });

        bus.publish("test/topic");
        QCoreApplication::processEvents();

        QCOMPARE(countA, 1);
        QCOMPARE(countB, 1);
    }

    void testTopicIsolation()
    {
        oap::EventBus bus;
        int count = 0;
        bus.subscribe("topic/a", [&](const QVariant&) { ++count; });

        bus.publish("topic/b");
        QCoreApplication::processEvents();
        QCOMPARE(count, 0);  // different topic — not delivered
    }
};

QTEST_MAIN(TestEventBus)
#include "test_event_bus.moc"
```

**Step 2: Run to verify it fails**

Expected: Compile error — `EventBus.hpp` doesn't exist.

**Step 3: Implement EventBus**

`src/core/services/EventBus.hpp`:

```cpp
#pragma once

#include "IEventBus.hpp"
#include <QObject>
#include <QMutex>
#include <QHash>
#include <QMultiHash>

namespace oap {

class EventBus : public QObject, public IEventBus {
    Q_OBJECT
public:
    explicit EventBus(QObject* parent = nullptr);

    int subscribe(const QString& topic, Callback callback) override;
    void unsubscribe(int subscriptionId) override;
    void publish(const QString& topic, const QVariant& payload = {}) override;

private:
    struct Subscription {
        QString topic;
        Callback callback;
    };

    QMutex mutex_;
    int nextId_ = 1;
    QHash<int, Subscription> subscriptions_;
    QMultiHash<QString, int> topicIndex_;
};

} // namespace oap
```

`src/core/services/EventBus.cpp`:

```cpp
#include "EventBus.hpp"
#include <QMetaObject>

namespace oap {

EventBus::EventBus(QObject* parent) : QObject(parent) {}

int EventBus::subscribe(const QString& topic, Callback callback)
{
    QMutexLocker lock(&mutex_);
    int id = nextId_++;
    subscriptions_[id] = {topic, std::move(callback)};
    topicIndex_.insert(topic, id);
    return id;
}

void EventBus::unsubscribe(int subscriptionId)
{
    QMutexLocker lock(&mutex_);
    auto it = subscriptions_.find(subscriptionId);
    if (it == subscriptions_.end()) return;
    topicIndex_.remove(it->topic, subscriptionId);
    subscriptions_.erase(it);
}

void EventBus::publish(const QString& topic, const QVariant& payload)
{
    QMutexLocker lock(&mutex_);
    auto ids = topicIndex_.values(topic);
    for (int id : ids) {
        auto it = subscriptions_.find(id);
        if (it == subscriptions_.end()) continue;
        auto cb = it->callback;  // copy callback while holding lock
        // Deliver on main thread via QueuedConnection
        QMetaObject::invokeMethod(this, [cb, payload]() {
            cb(payload);
        }, Qt::QueuedConnection);
    }
}

} // namespace oap
```

**Step 4: Add to CMakeLists**

In `src/CMakeLists.txt`, add `core/services/EventBus.cpp` to source list.

In `tests/CMakeLists.txt`, add:

```cmake
add_executable(test_event_bus
    test_event_bus.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/EventBus.cpp
)
target_include_directories(test_event_bus PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_event_bus PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_event_bus COMMAND test_event_bus)
```

**Step 5: Run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R test_event_bus -V`
Expected: All 4 tests PASS.

**Step 6: Commit**

```bash
git add src/core/services/EventBus.hpp src/core/services/EventBus.cpp tests/test_event_bus.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: implement concrete EventBus with thread-safe pub/sub"
```

---

### Task 11: Wire EventBus into HostContext and main.cpp

**Files:**
- Modify: `src/main.cpp`

**Step 1: Create and inject EventBus**

In `main.cpp`, after audio service setup, before plugin infrastructure:

```cpp
#include "core/services/EventBus.hpp"
// ...
auto eventBus = new oap::EventBus(&app);
// ... (in HostContext setup)
hostContext->setEventBus(eventBus);
```

**Step 2: Build and run all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All pass.

**Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: inject EventBus into HostContext for plugin access"
```

---

### Task 12: Implement ActionRegistry

**Files:**
- Create: `src/core/services/ActionRegistry.hpp`
- Create: `src/core/services/ActionRegistry.cpp`
- Create: `tests/test_action_registry.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing tests**

`tests/test_action_registry.cpp`:

```cpp
#include <QTest>
#include "core/services/ActionRegistry.hpp"

class TestActionRegistry : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndDispatch()
    {
        oap::ActionRegistry registry;
        bool called = false;
        registry.registerAction("app.quit", [&](const QVariant&) { called = true; });

        bool ok = registry.dispatch("app.quit");
        QVERIFY(ok);
        QVERIFY(called);
    }

    void testDispatchUnknownAction()
    {
        oap::ActionRegistry registry;
        bool ok = registry.dispatch("nonexistent");
        QVERIFY(!ok);
    }

    void testDispatchWithPayload()
    {
        oap::ActionRegistry registry;
        QVariant received;
        registry.registerAction("volume.set", [&](const QVariant& v) { received = v; });

        registry.dispatch("volume.set", 75);
        QCOMPARE(received.toInt(), 75);
    }

    void testUnregister()
    {
        oap::ActionRegistry registry;
        int count = 0;
        registry.registerAction("test", [&](const QVariant&) { ++count; });

        registry.dispatch("test");
        QCOMPARE(count, 1);

        registry.unregisterAction("test");
        bool ok = registry.dispatch("test");
        QVERIFY(!ok);
        QCOMPARE(count, 1);
    }

    void testListActions()
    {
        oap::ActionRegistry registry;
        registry.registerAction("a", [](const QVariant&) {});
        registry.registerAction("b", [](const QVariant&) {});

        auto actions = registry.registeredActions();
        QCOMPARE(actions.size(), 2);
        QVERIFY(actions.contains("a"));
        QVERIFY(actions.contains("b"));
    }

    void testDuplicateRegistrationOverwrites()
    {
        oap::ActionRegistry registry;
        int version = 0;
        registry.registerAction("test", [&](const QVariant&) { version = 1; });
        registry.registerAction("test", [&](const QVariant&) { version = 2; });

        registry.dispatch("test");
        QCOMPARE(version, 2);  // last-write-wins
        QCOMPARE(registry.registeredActions().size(), 1);
    }
};

QTEST_MAIN(TestActionRegistry)
#include "test_action_registry.moc"
```

**Step 2: Implement ActionRegistry**

`src/core/services/ActionRegistry.hpp`:

```cpp
#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <functional>

namespace oap {

/// Registry for named actions. Actions are synchronous command handlers
/// (as opposed to EventBus topics which are async notifications).
/// Thread-safe for registration; dispatch must happen on the main thread.
class ActionRegistry : public QObject {
    Q_OBJECT
public:
    using Handler = std::function<void(const QVariant& payload)>;

    explicit ActionRegistry(QObject* parent = nullptr);

    void registerAction(const QString& actionId, Handler handler);
    void unregisterAction(const QString& actionId);
    bool dispatch(const QString& actionId, const QVariant& payload = {});
    QStringList registeredActions() const;

signals:
    void actionDispatched(const QString& actionId);

private:
    QHash<QString, Handler> handlers_;
};

} // namespace oap
```

`src/core/services/ActionRegistry.cpp`:

```cpp
#include "ActionRegistry.hpp"
#include <boost/log/trivial.hpp>

namespace oap {

ActionRegistry::ActionRegistry(QObject* parent) : QObject(parent) {}

void ActionRegistry::registerAction(const QString& actionId, Handler handler)
{
    handlers_[actionId] = std::move(handler);
}

void ActionRegistry::unregisterAction(const QString& actionId)
{
    handlers_.remove(actionId);
}

bool ActionRegistry::dispatch(const QString& actionId, const QVariant& payload)
{
    auto it = handlers_.find(actionId);
    if (it == handlers_.end()) {
        BOOST_LOG_TRIVIAL(debug) << "ActionRegistry: unknown action '"
                                  << actionId.toStdString() << "'";
        return false;
    }
    it.value()(payload);
    emit actionDispatched(actionId);
    return true;
}

QStringList ActionRegistry::registeredActions() const
{
    return handlers_.keys();
}

} // namespace oap
```

**Step 3: Add to CMakeLists**

In `src/CMakeLists.txt`, add `core/services/ActionRegistry.cpp`.

In `tests/CMakeLists.txt`:

```cmake
add_executable(test_action_registry
    test_action_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/ActionRegistry.cpp
)
target_include_directories(test_action_registry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_action_registry PRIVATE Qt6::Core Qt6::Test Boost::log)
add_test(NAME test_action_registry COMMAND test_action_registry)
```

**Step 4: Run tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest -R test_action_registry -V`
Expected: All 5 tests PASS.

**Step 5: Commit**

```bash
git add src/core/services/ActionRegistry.hpp src/core/services/ActionRegistry.cpp tests/test_action_registry.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add ActionRegistry for named command dispatch"
```

---

### Task 13: Wire ActionRegistry into main.cpp with initial actions

Register built-in actions and make ActionRegistry accessible to plugins via IHostContext.

**Files:**
- Modify: `src/core/plugin/IHostContext.hpp`
- Modify: `src/core/plugin/HostContext.hpp`
- Modify: `src/main.cpp`
- Modify: `tests/test_plugin_manager.cpp` — update `MockHostContext` to implement new `actionRegistry()` pure virtual (return `nullptr`)

**Step 1: Add ActionRegistry to IHostContext**

In `IHostContext.hpp`:

```cpp
class ActionRegistry;
// In class:
    virtual ActionRegistry* actionRegistry() = 0;
```

In `HostContext.hpp`:

```cpp
    void setActionRegistry(ActionRegistry* reg) { actions_ = reg; }
    ActionRegistry* actionRegistry() override { return actions_; }
// In private:
    ActionRegistry* actions_ = nullptr;
```

**Step 2: Wire in main.cpp**

```cpp
#include "core/services/ActionRegistry.hpp"
// After EventBus creation:
auto actionRegistry = new oap::ActionRegistry(&app);
hostContext->setActionRegistry(actionRegistry);

// Register built-in actions:
actionRegistry->registerAction("app.quit", [](const QVariant&) {
    QGuiApplication::quit();
});
actionRegistry->registerAction("app.home", [pluginModel](const QVariant&) {
    pluginModel->setActivePlugin(QString());
});
actionRegistry->registerAction("theme.toggle", [themeService](const QVariant&) {
    themeService->toggleMode();
});
```

**Step 3: Build and verify all tests**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc) && ctest --output-on-failure`

**Step 4: Commit**

```bash
git add src/core/plugin/IHostContext.hpp src/core/plugin/HostContext.hpp src/main.cpp
git commit -m "feat: wire ActionRegistry into HostContext with built-in actions"
```

---

### Task 14: Update GestureOverlay to use ActionRegistry

Replace the hardcoded `ApplicationController.navigateTo(0)` and `ThemeService.toggleMode()` calls with action dispatch.

**Files:**
- Modify: `qml/components/GestureOverlay.qml`
- Modify: `src/main.cpp` (expose ActionRegistry to QML)

**Step 1: Expose ActionRegistry to QML**

In `main.cpp`, after engine context property setup:

```cpp
engine.rootContext()->setContextProperty("ActionRegistry", actionRegistry);
```

**Step 2: Add Q_INVOKABLE dispatch to ActionRegistry**

The `dispatch()` method is already public but needs `Q_INVOKABLE` for QML:

In `ActionRegistry.hpp`, change:

```cpp
    Q_INVOKABLE bool dispatch(const QString& actionId, const QVariant& payload = {});
```

**Step 3: Update GestureOverlay.qml**

Replace the Home button click handler:
```qml
onClicked: {
    ActionRegistry.dispatch("app.home")
    overlay.dismiss()
}
```

Replace the Day/Night toggle button:
```qml
onClicked: {
    ActionRegistry.dispatch("theme.toggle")
    dismissTimer.restart()
}
```

**Step 4: Build**

Run: `cd /home/matt/claude/personal/openautopro/openauto-prodigy/build && cmake --build . -j$(nproc)`

**Step 5: Commit**

```bash
git add qml/components/GestureOverlay.qml src/main.cpp src/core/services/ActionRegistry.hpp
git commit -m "refactor: GestureOverlay dispatches actions instead of direct calls"
```

---

## Priority 3: Notification MVP

### Task 15: Create INotificationService and NotificationModel

**Files:**
- Create: `src/core/services/INotificationService.hpp`
- Create: `src/core/services/NotificationService.hpp`
- Create: `src/core/services/NotificationService.cpp`
- Create: `src/ui/NotificationModel.hpp`
- Create: `src/ui/NotificationModel.cpp`
- Create: `tests/test_notification_service.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing tests**

```cpp
#include <QTest>
#include "core/services/NotificationService.hpp"
#include "ui/NotificationModel.hpp"

class TestNotificationService : public QObject {
    Q_OBJECT
private slots:
    void testPostNotification()
    {
        oap::NotificationService svc;
        oap::NotificationModel model(&svc);

        QCOMPARE(model.rowCount(), 0);

        svc.post({
            {"kind", "toast"},
            {"message", "Hello"},
            {"sourcePluginId", "test"},
            {"priority", 50},
            {"ttlMs", 5000}
        });

        QCOMPARE(model.rowCount(), 1);
    }

    void testDismiss()
    {
        oap::NotificationService svc;
        oap::NotificationModel model(&svc);

        svc.post({{"kind", "toast"}, {"message", "Test"}});
        QCOMPARE(model.rowCount(), 1);

        auto id = model.data(model.index(0, 0), oap::NotificationModel::NotificationIdRole).toString();
        svc.dismiss(id);
        QCOMPARE(model.rowCount(), 0);
    }

    void testTtlExpiry()
    {
        oap::NotificationService svc;
        oap::NotificationModel model(&svc);

        svc.post({{"kind", "toast"}, {"message", "Ephemeral"}, {"ttlMs", 50}});
        QCOMPARE(model.rowCount(), 1);

        QTest::qWait(100);
        QCOMPARE(model.rowCount(), 0);  // expired
    }
};

QTEST_MAIN(TestNotificationService)
#include "test_notification_service.moc"
```

**Step 2: Implement**

`src/core/services/INotificationService.hpp`:

```cpp
#pragma once

#include <QVariantMap>
#include <QString>

namespace oap {

class INotificationService {
public:
    virtual ~INotificationService() = default;

    /// Post a notification. Required fields: kind, message.
    /// Optional: sourcePluginId, priority (0-100), ttlMs (0 = persistent).
    /// Returns notification ID.
    virtual QString post(const QVariantMap& notification) = 0;

    /// Dismiss a notification by ID.
    virtual void dismiss(const QString& notificationId) = 0;
};

} // namespace oap
```

`src/core/services/NotificationService.hpp`:

```cpp
#pragma once

#include "INotificationService.hpp"
#include <QObject>
#include <QList>
#include <QTimer>
#include <QUuid>

namespace oap {

struct Notification {
    QString id;
    QString kind;       // "toast", "incoming_call", "status_icon"
    QString message;
    QString sourcePluginId;
    int priority = 50;
    int ttlMs = 0;      // 0 = persistent until dismissed
    QVariantMap extra;   // additional payload
};

class NotificationService : public QObject, public INotificationService {
    Q_OBJECT
public:
    explicit NotificationService(QObject* parent = nullptr);

    QString post(const QVariantMap& notification) override;
    void dismiss(const QString& notificationId) override;

    QList<Notification> active() const { return notifications_; }

signals:
    void notificationAdded(const oap::Notification& n);
    void notificationRemoved(const QString& id);

private:
    QList<Notification> notifications_;
};

} // namespace oap
```

`src/core/services/NotificationService.cpp`:

```cpp
#include "NotificationService.hpp"

namespace oap {

NotificationService::NotificationService(QObject* parent) : QObject(parent) {}

QString NotificationService::post(const QVariantMap& data)
{
    Notification n;
    n.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    n.kind = data.value("kind").toString();
    n.message = data.value("message").toString();
    n.sourcePluginId = data.value("sourcePluginId").toString();
    n.priority = data.value("priority", 50).toInt();
    n.ttlMs = data.value("ttlMs", 0).toInt();

    notifications_.append(n);
    emit notificationAdded(n);

    if (n.ttlMs > 0) {
        QString id = n.id;
        QTimer::singleShot(n.ttlMs, this, [this, id]() { dismiss(id); });
    }

    return n.id;
}

void NotificationService::dismiss(const QString& notificationId)
{
    for (int i = 0; i < notifications_.size(); ++i) {
        if (notifications_[i].id == notificationId) {
            notifications_.removeAt(i);
            emit notificationRemoved(notificationId);
            return;
        }
    }
}

} // namespace oap
```

`src/ui/NotificationModel.hpp`:

```cpp
#pragma once

#include <QAbstractListModel>

namespace oap {

class NotificationService;

class NotificationModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        NotificationIdRole = Qt::UserRole + 1,
        KindRole,
        MessageRole,
        SourcePluginRole,
        PriorityRole
    };

    explicit NotificationModel(NotificationService* service, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    NotificationService* service_;
};

} // namespace oap
```

`src/ui/NotificationModel.cpp`:

```cpp
#include "NotificationModel.hpp"
#include "core/services/NotificationService.hpp"

namespace oap {

NotificationModel::NotificationModel(NotificationService* service, QObject* parent)
    : QAbstractListModel(parent), service_(service)
{
    connect(service_, &NotificationService::notificationAdded, this, [this]() {
        beginResetModel(); endResetModel(); emit countChanged();
    });
    connect(service_, &NotificationService::notificationRemoved, this, [this]() {
        beginResetModel(); endResetModel(); emit countChanged();
    });
}

int NotificationModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return service_->active().size();
}

QVariant NotificationModel::data(const QModelIndex& index, int role) const
{
    auto notifications = service_->active();
    if (index.row() < 0 || index.row() >= notifications.size()) return {};
    const auto& n = notifications[index.row()];
    switch (role) {
    case NotificationIdRole:   return n.id;
    case KindRole:             return n.kind;
    case MessageRole:          return n.message;
    case SourcePluginRole:     return n.sourcePluginId;
    case PriorityRole:         return n.priority;
    default:                   return {};
    }
}

QHash<int, QByteArray> NotificationModel::roleNames() const
{
    return {
        {NotificationIdRole, "notificationId"},
        {KindRole,           "kind"},
        {MessageRole,        "message"},
        {SourcePluginRole,   "sourcePlugin"},
        {PriorityRole,       "priority"}
    };
}

} // namespace oap
```

**Step 3: Add to CMakeLists and run tests**

**Step 4: Commit**

```bash
git commit -m "feat: add NotificationService and NotificationModel"
```

---

### Task 16: Add NotificationService to IHostContext

**Files:**
- Modify: `src/core/plugin/IHostContext.hpp`
- Modify: `src/core/plugin/HostContext.hpp`
- Modify: `src/main.cpp`

Same pattern as EventBus/ActionRegistry: add `virtual INotificationService* notificationService() = 0;` to IHostContext, wire in HostContext and main.cpp. **Also update `MockHostContext` in `tests/test_plugin_manager.cpp`** to implement `notificationService()` (return `nullptr`).

**Commit:**
```bash
git commit -m "feat: wire NotificationService into HostContext"
```

---

### Task 17: Create NotificationArea QML component and wire into Shell

**Files:**
- Create: `qml/components/NotificationArea.qml`
- Modify: `qml/components/Shell.qml`
- Modify: `src/CMakeLists.txt`

Toast notifications appear at the top of the screen with auto-dismiss. Incoming call notifications use the existing `IncomingCallOverlay` layout but driven by the notification model instead of the global `PhonePlugin` property.

**Commit:**
```bash
git commit -m "feat: add NotificationArea component to Shell"
```

---

### Task 18: Migrate PhonePlugin to use NotificationService

Replace the global `PhonePlugin` context property hack with posting an `incoming_call` notification.

**Files:**
- Modify: `src/plugins/phone/PhonePlugin.cpp`
- Modify: `qml/applications/phone/IncomingCallOverlay.qml`

In `PhonePlugin::initialize()`, when an incoming call is detected, post:

```cpp
if (hostContext_->notificationService()) {
    hostContext_->notificationService()->post({
        {"kind", "incoming_call"},
        {"message", callerName.isEmpty() ? callerNumber : callerName},
        {"sourcePluginId", id()},
        {"priority", 90}
    });
}
```

**Commit:**
```bash
git commit -m "refactor: PhonePlugin uses NotificationService for incoming calls"
```

---

## Priority 4: Pre-SDK Contract Documentation

### Task 19: Document interface contracts

**Files:**
- Create: `docs/plugin-api.md`
- Create: `docs/config-schema.md`

Document for each service interface:
- Thread affinity (UI thread only? background allowed?)
- Ownership rules (who deletes returned objects?)
- Error handling (return values, not exceptions)
- Stability guarantees (stable, experimental, internal)

Document for config:
- Full YAML schema with types and defaults
- Plugin config namespace rules
- Migration policy (additive only for minor versions)

**Commit:**
```bash
git commit -m "docs: add plugin API contracts and config schema documentation"
```

---

---

## Deferred Items (documented for future)

- **InputFocusManager** — formalize input scope ownership (`native`/`projection`/`overlay`). Not urgent until overlays contend for input beyond the current AA grab/ungrab pattern. Design sketch: `InputFocusManager` with `requestFocus(scope, requesterId)` / `releaseFocus(requesterId)`, wired to EvdevTouchReader and GestureOverlay.
- **External API version handshake** — evolve IpcServer to versioned protobuf/WebSocket when a second consumer exists.
- **Theme extension points** — action-specific assets, icon font packs. No consumer yet.
- **Chromium/HTML widget runtime** — too heavy for now. Prefer native QML + API-first.
- **Settings as a plugin** — currently `SettingsMenu.qml` is navigated via `ApplicationController.navigateTo(6)`. Should become a plugin with its own QML component.

---

## Review Checkpoint

After completing Priority 1 (Tasks 1-2c, 3-9), deploy to Pi and smoke test:
- [ ] Launcher shows config-driven tiles
- [ ] Tapping a tile activates the plugin via PluginModel
- [ ] Nav strip shows plugins from PluginModel
- [ ] AA fullscreen hides nav/status bar (via `PluginModel.activePluginFullscreen`)
- [ ] AA connect auto-activates AA plugin
- [ ] AA disconnect returns to launcher
- [ ] No global `AndroidAutoService` in root QML context
- [ ] Existing gesture overlay still works
- [ ] Incoming call overlay still works (PhonePlugin global kept until Priority 3)

After Priority 2 (Tasks 10-14), verify:
- [ ] EventBus pub/sub works in test
- [ ] ActionRegistry dispatches built-in actions
- [ ] GestureOverlay home/theme-toggle work through ActionRegistry
- [ ] All existing tests still pass

After Priority 3 (Tasks 15-18), verify:
- [ ] Phone incoming call shows via NotificationService
- [ ] No global `PhonePlugin` context property (removed from main.cpp)
- [ ] Toast notifications auto-dismiss
- [ ] All existing tests still pass + new tests (event_bus, action_registry, notification_service, plugin_model)
