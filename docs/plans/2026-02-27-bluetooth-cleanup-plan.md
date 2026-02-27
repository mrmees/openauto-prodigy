# Bluetooth Cleanup Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Bring the Pi's Bluetooth stack to head-unit quality with proper adapter management, pairing UX, auto-reconnect, and consolidated profile ownership.

**Architecture:** A new `BluetoothManager` core service owns all adapter-level BT concerns via BlueZ D-Bus. It implements the existing `IBluetoothService` interface, adds a pairing agent (Agent1), paired device model (QAbstractListModel), and auto-connect with backoff. A QML pairing dialog overlay handles user confirmation. Profile registration moves from Python + BluetoothDiscoveryService into BluetoothManager.

**Tech Stack:** C++17, Qt 6 (DBus, Core, Quick), BlueZ 5.x D-Bus API, QML

**Design Doc:** `docs/plans/2026-02-27-bluetooth-cleanup-design.md`

---

## Phase 1: IBluetoothService Interface Expansion + Stub

Expand the interface, create the BluetoothManager skeleton, wire it into main.cpp, add tests.

### Task 1: Expand IBluetoothService interface

**Files:**
- Modify: `src/core/services/IBluetoothService.hpp`

**Step 1: Update the interface**

Replace the full contents of `src/core/services/IBluetoothService.hpp`:

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QAbstractListModel;

namespace oap {

class IBluetoothService {
public:
    virtual ~IBluetoothService() = default;

    // --- Adapter info ---
    virtual QString adapterAddress() const = 0;
    virtual QString adapterAlias() const = 0;

    // --- Discoverability (always on; informational) ---
    virtual bool isDiscoverable() const = 0;

    // --- Pairable control ---
    virtual bool isPairable() const = 0;
    virtual void setPairable(bool enabled) = 0;

    // --- Pairing dialog state ---
    virtual bool isPairingActive() const = 0;
    virtual QString pairingDeviceName() const = 0;
    virtual QString pairingPasskey() const = 0;
    virtual void confirmPairing() = 0;
    virtual void rejectPairing() = 0;

    // --- Paired devices ---
    virtual QAbstractListModel* pairedDevicesModel() = 0;
    virtual void forgetDevice(const QString& address) = 0;

    // --- Auto-connect ---
    virtual void startAutoConnect() = 0;
    virtual void cancelAutoConnect() = 0;

    // --- Connection state ---
    virtual QString connectedDeviceName() const = 0;
    virtual QString connectedDeviceAddress() const = 0;

    // --- Lifecycle ---
    virtual void initialize() = 0;
    virtual void shutdown() = 0;
};

} // namespace oap
```

**Step 2: Verify existing code compiles**

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | head -30`
Expected: Compilation errors in test stubs that implement IBluetoothService (test_plugin_model, test_plugin_manager). We'll fix those in the next step.

**Step 3: Fix test stubs**

Search for `IBluetoothService` implementations in test files and update the mock/stub to implement all new pure virtuals with no-op returns.

Run: `cd build && cmake --build . -j$(nproc) 2>&1 | tail -20`
Expected: PASS (all new methods return defaults)

**Step 4: Commit**

```bash
git add src/core/services/IBluetoothService.hpp tests/
git commit -m "feat(bt): expand IBluetoothService interface for BT cleanup"
```

---

### Task 2: Create BluetoothManager skeleton

**Files:**
- Create: `src/core/services/BluetoothManager.hpp`
- Create: `src/core/services/BluetoothManager.cpp`
- Modify: `src/CMakeLists.txt:1-45` (add source file)

**Step 1: Create the header**

Create `src/core/services/BluetoothManager.hpp`:

```cpp
#pragma once

#include "IBluetoothService.hpp"
#include <QObject>
#include <QDBusConnection>
#include <QAbstractListModel>
#include <memory>

namespace oap {

class IConfigService;

class BluetoothManager : public QObject, public IBluetoothService {
    Q_OBJECT
    Q_PROPERTY(QString adapterAddress READ adapterAddress CONSTANT)
    Q_PROPERTY(QString adapterAlias READ adapterAlias NOTIFY adapterAliasChanged)
    Q_PROPERTY(bool discoverable READ isDiscoverable NOTIFY discoverableChanged)
    Q_PROPERTY(bool pairable READ isPairable WRITE setPairable NOTIFY pairableChanged)
    Q_PROPERTY(bool pairingActive READ isPairingActive NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString pairingDeviceName READ pairingDeviceName NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString pairingPasskey READ pairingPasskey NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY connectedDeviceChanged)

public:
    explicit BluetoothManager(IConfigService* configService, QObject* parent = nullptr);
    ~BluetoothManager() override;

    // IBluetoothService
    QString adapterAddress() const override;
    QString adapterAlias() const override;
    bool isDiscoverable() const override;
    bool isPairable() const override;
    void setPairable(bool enabled) override;
    bool isPairingActive() const override;
    QString pairingDeviceName() const override;
    QString pairingPasskey() const override;
    Q_INVOKABLE void confirmPairing() override;
    Q_INVOKABLE void rejectPairing() override;
    QAbstractListModel* pairedDevicesModel() override;
    Q_INVOKABLE void forgetDevice(const QString& address) override;
    void startAutoConnect() override;
    void cancelAutoConnect() override;
    QString connectedDeviceName() const override;
    QString connectedDeviceAddress() const override;
    void initialize() override;
    void shutdown() override;

signals:
    void adapterAliasChanged();
    void discoverableChanged();
    void pairableChanged();
    void pairingActiveChanged();
    void connectedDeviceChanged();
    void profileNewConnection();  // RFCOMM NewConnection — auto-connect stop signal

private:
    IConfigService* configService_ = nullptr;
    QString adapterAddress_;
    QString adapterAlias_;
    bool discoverable_ = false;
    bool pairable_ = false;
    bool pairingActive_ = false;
    QString pairingDeviceName_;
    QString pairingPasskey_;
    QString connectedDeviceName_;
    QString connectedDeviceAddress_;
};

} // namespace oap
```

**Step 2: Create the implementation stub**

Create `src/core/services/BluetoothManager.cpp`:

```cpp
#include "BluetoothManager.hpp"
#include "IConfigService.hpp"
#include <QDebug>

namespace oap {

BluetoothManager::BluetoothManager(IConfigService* configService, QObject* parent)
    : QObject(parent)
    , configService_(configService)
{
}

BluetoothManager::~BluetoothManager()
{
    shutdown();
}

QString BluetoothManager::adapterAddress() const { return adapterAddress_; }
QString BluetoothManager::adapterAlias() const { return adapterAlias_; }
bool BluetoothManager::isDiscoverable() const { return discoverable_; }
bool BluetoothManager::isPairable() const { return pairable_; }

void BluetoothManager::setPairable(bool enabled)
{
    if (pairable_ == enabled) return;
    pairable_ = enabled;
    emit pairableChanged();
    qInfo() << "[BtManager] Pairable:" << enabled;
    // TODO: set Adapter1.Pairable via D-Bus
}

bool BluetoothManager::isPairingActive() const { return pairingActive_; }
QString BluetoothManager::pairingDeviceName() const { return pairingDeviceName_; }
QString BluetoothManager::pairingPasskey() const { return pairingPasskey_; }

void BluetoothManager::confirmPairing()
{
    qInfo() << "[BtManager] Pairing confirmed by user";
    // TODO: reply to pending Agent1 D-Bus method call
}

void BluetoothManager::rejectPairing()
{
    qInfo() << "[BtManager] Pairing rejected by user";
    // TODO: reply with rejection to pending Agent1 D-Bus method call
}

QAbstractListModel* BluetoothManager::pairedDevicesModel()
{
    return nullptr;  // TODO: return PairedDevicesModel
}

void BluetoothManager::forgetDevice(const QString& address)
{
    qInfo() << "[BtManager] Forget device:" << address;
    // TODO: call Adapter1.RemoveDevice via D-Bus
}

void BluetoothManager::startAutoConnect()
{
    qInfo() << "[BtManager] Starting auto-connect";
    // TODO: enumerate paired devices, start retry loop
}

void BluetoothManager::cancelAutoConnect()
{
    qInfo() << "[BtManager] Cancelling auto-connect";
    // TODO: stop retry timer
}

QString BluetoothManager::connectedDeviceName() const { return connectedDeviceName_; }
QString BluetoothManager::connectedDeviceAddress() const { return connectedDeviceAddress_; }

void BluetoothManager::initialize()
{
    qInfo() << "[BtManager] Initializing...";
    // TODO: adapter setup, agent registration, profile registration, auto-connect
}

void BluetoothManager::shutdown()
{
    qInfo() << "[BtManager] Shutting down";
    cancelAutoConnect();
    // TODO: unregister agent, profiles
}

} // namespace oap
```

**Step 3: Add to CMakeLists.txt**

Add `core/services/BluetoothManager.cpp` to `src/CMakeLists.txt` source list (after line 35, `core/services/SystemServiceClient.cpp`):

```
    core/services/BluetoothManager.cpp
```

**Step 4: Build**

Run: `cd build && cmake .. && cmake --build . -j$(nproc)`
Expected: PASS

**Step 5: Commit**

```bash
git add src/core/services/BluetoothManager.hpp src/core/services/BluetoothManager.cpp src/CMakeLists.txt
git commit -m "feat(bt): add BluetoothManager skeleton implementing IBluetoothService"
```

---

### Task 3: Wire BluetoothManager into main.cpp

**Files:**
- Modify: `src/main.cpp:93-99` (after AudioService, before PluginManager)

**Step 1: Add include and creation**

In `src/main.cpp`, add include at top (after line 22):
```cpp
#include "core/services/BluetoothManager.hpp"
```

After the AudioService volume setup (after line 91) and before plugin infrastructure (before line 93), add:
```cpp
    // --- Bluetooth manager ---
    auto* bluetoothManager = new oap::BluetoothManager(configService.get(), &app);
```

In the HostContext setup block (after line 98 `hostContext->setAudioService(audioService);`), add:
```cpp
    hostContext->setBluetoothService(bluetoothManager);
```

In the QML context property section (after line 232 `engine.rootContext()->setContextProperty("SystemService", systemClient);`), add:
```cpp
    engine.rootContext()->setContextProperty("BluetoothManager", bluetoothManager);
```

After plugin initialization (after line 159 `pluginManager.initializeAll(hostContext.get());`), add:
```cpp
    bluetoothManager->initialize();
```

Before `pluginManager.shutdownAll()` (before line 275), add:
```cpp
    bluetoothManager->shutdown();
```

**Step 2: Build**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: PASS

**Step 3: Run tests**

Run: `cd build && ctest --output-on-failure`
Expected: All existing tests pass (BluetoothManager has no test-breaking changes)

**Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat(bt): wire BluetoothManager into main.cpp startup/shutdown"
```

---

### Task 4: Write BluetoothManager unit test

**Files:**
- Create: `tests/test_bluetooth_manager.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test**

Create `tests/test_bluetooth_manager.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "core/services/BluetoothManager.hpp"
#include "core/services/IConfigService.hpp"

// Minimal mock ConfigService for tests
class MockConfigService : public oap::IConfigService {
public:
    QVariant value(const QString& path) const override {
        return values_.value(path);
    }
    void setValue(const QString& path, const QVariant& value) override {
        values_[path] = value;
    }
    void save() override {}
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}

    QMap<QString, QVariant> values_;
};

class TestBluetoothManager : public QObject {
    Q_OBJECT

private slots:
    void testInitialState();
    void testPairableToggle();
    void testPairingConfirmReject();
    void testConnectedDeviceState();
};

void TestBluetoothManager::testInitialState()
{
    MockConfigService config;
    config.values_["connection.bt_name"] = "TestProdigy";
    oap::BluetoothManager mgr(&config);

    QVERIFY(mgr.adapterAddress().isEmpty());  // No adapter in test env
    QVERIFY(!mgr.isPairable());
    QVERIFY(!mgr.isPairingActive());
    QVERIFY(mgr.connectedDeviceName().isEmpty());
}

void TestBluetoothManager::testPairableToggle()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config);

    QSignalSpy spy(&mgr, &oap::BluetoothManager::pairableChanged);
    QVERIFY(!mgr.isPairable());

    mgr.setPairable(true);
    QVERIFY(mgr.isPairable());
    QCOMPARE(spy.count(), 1);

    // Setting same value doesn't emit
    mgr.setPairable(true);
    QCOMPARE(spy.count(), 1);

    mgr.setPairable(false);
    QVERIFY(!mgr.isPairable());
    QCOMPARE(spy.count(), 2);
}

void TestBluetoothManager::testPairingConfirmReject()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config);

    // These should not crash when no pending request
    mgr.confirmPairing();
    mgr.rejectPairing();
    QVERIFY(!mgr.isPairingActive());
}

void TestBluetoothManager::testConnectedDeviceState()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config);

    QVERIFY(mgr.connectedDeviceName().isEmpty());
    QVERIFY(mgr.connectedDeviceAddress().isEmpty());
}

QTEST_MAIN(TestBluetoothManager)
#include "test_bluetooth_manager.moc"
```

**Step 2: Add test to CMakeLists.txt**

Add to `tests/CMakeLists.txt` (before the final `set_tests_properties` block):

```cmake
add_executable(test_bluetooth_manager
    test_bluetooth_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/services/BluetoothManager.cpp
)
target_include_directories(test_bluetooth_manager PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_bluetooth_manager PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_test(NAME test_bluetooth_manager COMMAND test_bluetooth_manager)
```

**Step 3: Run the test**

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest -R test_bluetooth_manager --output-on-failure`
Expected: PASS (4 test functions)

**Step 4: Run all tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests pass

**Step 5: Commit**

```bash
git add tests/test_bluetooth_manager.cpp tests/CMakeLists.txt
git commit -m "test(bt): add BluetoothManager unit tests"
```

---

## Phase 2: Adapter Management via D-Bus

Implement the BlueZ D-Bus adapter setup — power on, set alias, discoverable, service watcher.

### Task 5: Implement adapter setup (D-Bus Adapter1 properties)

**Files:**
- Modify: `src/core/services/BluetoothManager.hpp`
- Modify: `src/core/services/BluetoothManager.cpp`

**Step 1: Add adapter D-Bus methods**

Add private methods and members to `BluetoothManager.hpp`:

```cpp
private:
    // D-Bus helpers
    void setupAdapter();
    void setAdapterProperty(const QString& property, const QVariant& value);
    QVariant getAdapterProperty(const QString& property);
    QString findAdapterPath();

    // BlueZ D-Bus
    QString adapterPath_;  // e.g. "/org/bluez/hci0"
```

**Step 2: Implement adapter setup**

In `BluetoothManager.cpp`, implement `initialize()` to:
1. Find adapter path via `GetManagedObjects` (filter for `org.bluez.Adapter1` interface)
2. Read `Address` property → `adapterAddress_`
3. Set `Powered=true`
4. Set `Alias` from `connection.bt_name` config (fallback: `OpenAutoProdigy`)
5. Set `DiscoverableTimeout=0`, `Discoverable=true`
6. Set `PairableTimeout=120`, `Pairable=false`

Use `QDBusConnection::systemBus()` for all BlueZ calls.

**Step 3: Add QDBusServiceWatcher for BlueZ restart recovery**

In `initialize()`, create a `QDBusServiceWatcher` monitoring `org.bluez`:
```cpp
auto* watcher = new QDBusServiceWatcher("org.bluez",
    QDBusConnection::systemBus(),
    QDBusServiceWatcher::WatchForRegistration, this);
connect(watcher, &QDBusServiceWatcher::serviceRegistered,
        this, [this]() {
    qInfo() << "[BtManager] BlueZ restarted — re-initializing";
    setupAdapter();
    // TODO: re-register agent + profiles after adapter setup
});
```

**Step 4: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass. BluetoothManager logs "No BlueZ adapter found" on dev VM (no hci0).

**Step 5: Commit**

```bash
git add src/core/services/BluetoothManager.hpp src/core/services/BluetoothManager.cpp
git commit -m "feat(bt): implement adapter setup via BlueZ D-Bus"
```

---

### Task 6: Implement Agent1 for pairing confirmation

**Files:**
- Modify: `src/core/services/BluetoothManager.hpp`
- Modify: `src/core/services/BluetoothManager.cpp`

**Step 1: Add Agent1 D-Bus adaptor**

BlueZ calls methods on our agent object. We need to:
1. Register a QObject at `/org/openauto/agent` on system bus
2. Expose `RequestConfirmation(QDBusObjectPath device, uint32_t passkey)` — this is the pairing confirmation method
3. Expose `Cancel()` — BlueZ calls this to abort pending requests
4. Expose `Release()` — cleanup
5. Expose `AuthorizeService(QDBusObjectPath device, QString uuid)` — auto-accept known profiles

Use `QDBusConnection::registerObject()` with `QDBusConnection::ExportScriptableSlots`.

The `RequestConfirmation` method must be handled as a **delayed D-Bus reply**: store the `QDBusMessage` from the method call, don't return immediately. When the user taps Confirm/Reject in QML, reply to the stored message.

Key implementation detail:
```cpp
// In the slot called by D-Bus:
void agentRequestConfirmation(const QDBusObjectPath& device, uint32_t passkey,
                               const QDBusMessage& msg)
{
    // Tell D-Bus we're not replying yet
    msg.setDelayedReply(true);
    pendingPairingMessage_ = msg;

    // Extract device name from device path
    pairingDeviceName_ = deviceNameFromPath(device.path());
    pairingPasskey_ = QString("%1").arg(passkey, 6, 10, QChar('0'));
    pairingActive_ = true;
    emit pairingActiveChanged();
}

void confirmPairing() override
{
    if (!pairingActive_) return;
    auto reply = pendingPairingMessage_.createReply();
    QDBusConnection::systemBus().send(reply);
    pairingActive_ = false;
    emit pairingActiveChanged();
}

void rejectPairing() override
{
    if (!pairingActive_) return;
    auto reply = pendingPairingMessage_.createErrorReply(
        "org.bluez.Error.Rejected", "User rejected pairing");
    QDBusConnection::systemBus().send(reply);
    pairingActive_ = false;
    emit pairingActiveChanged();
}
```

**Step 2: Register agent with AgentManager1**

After registering the D-Bus object, call:
- `RegisterAgent("/org/openauto/agent", "DisplayYesNo")`
- `RequestDefaultAgent("/org/openauto/agent")`

**Step 3: Handle Cancel from BlueZ**

When BlueZ calls `Cancel()`:
```cpp
void agentCancel()
{
    if (pairingActive_) {
        pairingActive_ = false;
        emit pairingActiveChanged();
        qInfo() << "[BtManager] BlueZ cancelled pairing request";
    }
}
```

**Step 4: Set Trusted=true on successful pairing**

After `confirmPairing()`, set `Trusted=true` on the device:
```cpp
setDeviceProperty(devicePath, "Trusted", true);
```

**Step 5: Update tests**

Add test for confirm/reject state transitions with signal spy on `pairingActiveChanged`.

**Step 6: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass

**Step 7: Commit**

```bash
git add src/core/services/BluetoothManager.hpp src/core/services/BluetoothManager.cpp tests/test_bluetooth_manager.cpp
git commit -m "feat(bt): implement BlueZ Agent1 for pairing confirmation"
```

---

## Phase 3: PairedDevicesModel + Paired Device Management

### Task 7: Create PairedDevicesModel

**Files:**
- Create: `src/ui/PairedDevicesModel.hpp`
- Create: `src/ui/PairedDevicesModel.cpp`
- Modify: `src/CMakeLists.txt` (add source)
- Create: `tests/test_paired_devices_model.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the model header**

Create `src/ui/PairedDevicesModel.hpp`:

```cpp
#pragma once

#include <QAbstractListModel>
#include <QList>

namespace oap {

struct PairedDeviceInfo {
    QString address;
    QString name;
    bool connected = false;
};

class PairedDevicesModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        AddressRole = Qt::UserRole + 1,
        NameRole,
        ConnectedRole
    };

    explicit PairedDevicesModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDevices(const QList<PairedDeviceInfo>& devices);
    void updateConnectionState(const QString& address, bool connected);

signals:
    void countChanged();

private:
    QList<PairedDeviceInfo> devices_;
};

} // namespace oap
```

**Step 2: Write the model implementation**

Create `src/ui/PairedDevicesModel.cpp` following the `AudioDeviceModel` pattern:
- `roleNames()` maps `AddressRole→"address"`, `NameRole→"name"`, `ConnectedRole→"connected"`
- `setDevices()` does `beginResetModel()` / `endResetModel()`
- `updateConnectionState()` updates in-place with `dataChanged()` signal

**Step 3: Write tests**

Create `tests/test_paired_devices_model.cpp`:
- Test empty model has rowCount 0
- Test setDevices populates model, rowCount matches
- Test data() returns correct values for each role
- Test updateConnectionState changes ConnectedRole

**Step 4: Add to CMakeLists**

Add source to `src/CMakeLists.txt`, test to `tests/CMakeLists.txt`.

**Step 5: Build and test**

Run: `cd build && cmake .. && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass

**Step 6: Commit**

```bash
git add src/ui/PairedDevicesModel.hpp src/ui/PairedDevicesModel.cpp src/CMakeLists.txt tests/test_paired_devices_model.cpp tests/CMakeLists.txt
git commit -m "feat(bt): add PairedDevicesModel for QML paired device list"
```

---

### Task 8: Wire PairedDevicesModel into BluetoothManager

**Files:**
- Modify: `src/core/services/BluetoothManager.hpp`
- Modify: `src/core/services/BluetoothManager.cpp`

**Step 1: Add model ownership**

Add `#include "ui/PairedDevicesModel.hpp"` to BluetoothManager.hpp.
Add private member: `PairedDevicesModel* pairedDevicesModel_ = nullptr;`

In constructor, create the model: `pairedDevicesModel_ = new PairedDevicesModel(this);`

Implement `pairedDevicesModel()` override to return `pairedDevicesModel_`.

**Step 2: Add paired device enumeration from BlueZ**

Add private method `refreshPairedDevices()`:
- Call `GetManagedObjects` on `org.bluez` ObjectManager
- Filter for objects with `org.bluez.Device1` interface where `Paired=true`
- Extract `Address`, `Name`, `Connected` from each
- Call `pairedDevicesModel_->setDevices(devices)`

Call `refreshPairedDevices()` at end of `setupAdapter()`.

**Step 3: Watch for device changes**

Connect to `org.freedesktop.DBus.ObjectManager.InterfacesAdded` and `InterfacesRemoved` signals on `/` to detect new/removed devices.

Connect to `org.freedesktop.DBus.Properties.PropertiesChanged` for `org.bluez.Device1` to detect connection state changes → call `updateConnectionState()`.

**Step 4: Implement forgetDevice()**

`forgetDevice(address)`:
- Find device object path from address
- Call `Adapter1.RemoveDevice(devicePath)` via D-Bus
- `refreshPairedDevices()` after removal

**Step 5: Expose model to QML**

In `main.cpp`, after creating BluetoothManager, add:
```cpp
engine.rootContext()->setContextProperty("PairedDevicesModel", bluetoothManager->pairedDevicesModel());
```

**Step 6: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass

**Step 7: Commit**

```bash
git add src/core/services/BluetoothManager.hpp src/core/services/BluetoothManager.cpp src/main.cpp
git commit -m "feat(bt): wire PairedDevicesModel into BluetoothManager with D-Bus enumeration"
```

---

## Phase 4: Auto-Connect with Backoff

### Task 9: Implement auto-connect retry loop

**Files:**
- Modify: `src/core/services/BluetoothManager.hpp`
- Modify: `src/core/services/BluetoothManager.cpp`
- Modify: `tests/test_bluetooth_manager.cpp`

**Step 1: Add auto-connect state**

Add to header:
```cpp
private:
    // Auto-connect
    QTimer* autoConnectTimer_ = nullptr;
    int autoConnectAttempt_ = 0;
    int autoConnectDeviceIndex_ = 0;
    bool autoConnectInFlight_ = false;
    QStringList pairedDevicePaths_;

    void attemptConnect();
    int nextRetryInterval() const;
    static constexpr int MAX_ATTEMPTS = 13;  // 6 + 4 + 3
```

**Step 2: Implement the retry loop**

In `startAutoConnect()`:
1. Read `connection.auto_connect_aa` from config — if false, return
2. Enumerate paired device D-Bus paths (reuse from `refreshPairedDevices`)
3. If none, return
4. Create `autoConnectTimer_` (singleShot)
5. Connect timer to `attemptConnect()`
6. Fire first attempt immediately

In `attemptConnect()`:
1. If `autoConnectInFlight_`, return (previous call still pending)
2. Pick next device path (round-robin: `pairedDevicePaths_[autoConnectDeviceIndex_ % count]`)
3. Async D-Bus call: `Device1.Connect()` on that path
4. Set `autoConnectInFlight_ = true`
5. On reply (success or error): set `autoConnectInFlight_ = false`, increment attempt, schedule next

In `nextRetryInterval()`:
- Attempts 0-5: return 5000 (5s)
- Attempts 6-9: return 30000 (30s)
- Attempts 10-12: return 60000 (60s)
- Attempts >= 13: return -1 (stop)

In `cancelAutoConnect()`:
- Stop timer, reset state

**Step 3: Connect RFCOMM NewConnection as stop signal**

When BluetoothDiscoveryService's `BluezProfile1Adaptor::NewConnection()` fires, it means a phone connected on the right profile. BluetoothManager needs to know about this.

For now, add `profileNewConnection()` signal to BluetoothManager. In `main.cpp`, we'll connect BluetoothDiscoveryService's signal to this later (Task 12).

In BluetoothManager, connect to own `profileNewConnection`:
```cpp
connect(this, &BluetoothManager::profileNewConnection,
        this, &BluetoothManager::cancelAutoConnect);
```

**Step 4: Add test for retry interval schedule**

Test `nextRetryInterval()` returns correct values for attempt counts 0-13.

**Step 5: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass

**Step 6: Commit**

```bash
git add src/core/services/BluetoothManager.hpp src/core/services/BluetoothManager.cpp tests/test_bluetooth_manager.cpp
git commit -m "feat(bt): implement auto-connect with backoff retry loop"
```

---

## Phase 5: Profile Consolidation

### Task 10: Move HFP/HSP profile registration to BluetoothManager

**Files:**
- Modify: `src/core/services/BluetoothManager.hpp`
- Modify: `src/core/services/BluetoothManager.cpp`

**Step 1: Add profile registration methods**

Add private methods:
```cpp
void registerProfiles();
void unregisterProfiles();
```

Add members:
```cpp
QStringList registeredProfilePaths_;
std::vector<std::unique_ptr<QObject>> profileObjects_;
std::vector<int> profileFds_;
```

**Step 2: Implement profile registration**

Port the profile registration logic from `BluetoothDiscoveryService::registerBluetoothProfiles()` (lines 266-329) into `BluetoothManager::registerProfiles()`.

Key changes from the original:
- Remove `AutoConnect` option (it's for client role)
- Add `RequireAuthentication: false`, `RequireAuthorization: false`
- Register HFP AG (`0000111f-...`) and HSP HS (`00001108-...`)
- Keep the `BluezProfile1Adaptor` pattern for fd-holding

Create a simple adaptor class inside the BluetoothManager .cpp file (or reuse from BluetoothDiscoveryService — it will be removed from there in Task 12).

**Step 3: Call from initialize()**

After `setupAdapter()`, call `registerProfiles()`.
In `shutdown()`, call `unregisterProfiles()`.
In BlueZ restart handler, call both `setupAdapter()` and `registerProfiles()`.

**Step 4: Emit profileNewConnection when NewConnection fires**

When `BluezProfile1Adaptor::NewConnection()` fires, emit `profileNewConnection()` on the BluetoothManager so auto-connect cancels.

**Step 5: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass

**Step 6: Commit**

```bash
git add src/core/services/BluetoothManager.hpp src/core/services/BluetoothManager.cpp
git commit -m "feat(bt): move HFP/HSP profile registration to BluetoothManager"
```

---

### Task 11: Remove profile registration from BluetoothDiscoveryService

**Files:**
- Modify: `src/core/aa/BluetoothDiscoveryService.hpp`
- Modify: `src/core/aa/BluetoothDiscoveryService.cpp`

**Step 1: Remove profile methods and members**

Remove from `.hpp`:
- `registerBluetoothProfiles()` / `unregisterBluetoothProfiles()` declarations
- `BluezProfile1Adaptor` class (or move if not yet duplicated)
- `profileObjects_`, `profileFds_`, `registeredProfilePaths_` members

Remove from `.cpp`:
- `BluezProfile1Adaptor` implementation (lines 234-262)
- `registerBluetoothProfiles()` (lines 266-329)
- `unregisterBluetoothProfiles()` (lines 331-353)
- Call to `registerBluetoothProfiles()` in `start()` (line 89)
- Call to `unregisterBluetoothProfiles()` in `stop()` (line 94)

**Step 2: Remove D-Bus includes no longer needed**

Remove `QDBusVariant`, `QDBusUnixFileDescriptor` includes if only used by profile code.

**Step 3: Build and test**

Run: `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
Expected: All tests pass

**Step 4: Commit**

```bash
git add src/core/aa/BluetoothDiscoveryService.hpp src/core/aa/BluetoothDiscoveryService.cpp
git commit -m "refactor(bt): remove profile registration from BluetoothDiscoveryService"
```

---

## Phase 6: QML — Pairing Dialog + Connection Settings

### Task 12: Create PairingDialog QML overlay

**Files:**
- Create: `qml/components/PairingDialog.qml`
- Modify: `qml/components/Shell.qml`
- Modify: `src/CMakeLists.txt` (QML resources)

**Step 1: Create PairingDialog.qml**

Follow the GestureOverlay pattern (z:998, anchors.fill parent, translucent background):

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: overlay
    anchors.fill: parent
    color: "#CC000000"
    visible: BluetoothManager ? BluetoothManager.pairingActive : false
    z: 998

    // Block touch passthrough
    MouseArea {
        anchors.fill: parent
        onClicked: {} // absorb
    }

    // Center panel
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, 500)
        height: col.implicitHeight + UiMetrics.marginPage * 2
        radius: 12
        color: ThemeService.backgroundColor

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: UiMetrics.marginPage
            spacing: UiMetrics.gap

            Text {
                text: "Pair with device?"
                font.pixelSize: UiMetrics.fontHeader
                color: ThemeService.normalFontColor
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: BluetoothManager ? BluetoothManager.pairingDeviceName : ""
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.descriptionFontColor
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: BluetoothManager ? BluetoothManager.pairingPasskey : ""
                font.pixelSize: UiMetrics.fontHeader * 1.8
                font.weight: Font.Bold
                font.letterSpacing: 8
                color: ThemeService.normalFontColor
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: UiMetrics.gap
                Layout.bottomMargin: UiMetrics.gap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: UiMetrics.gap * 2

                Rectangle {
                    width: 140; height: UiMetrics.rowH
                    radius: 8
                    color: "#cc4444"
                    Text {
                        anchors.centerIn: parent
                        text: "Reject"
                        font.pixelSize: UiMetrics.fontBody
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: if (BluetoothManager) BluetoothManager.rejectPairing()
                    }
                }

                Rectangle {
                    width: 140; height: UiMetrics.rowH
                    radius: 8
                    color: "#44aa44"
                    Text {
                        anchors.centerIn: parent
                        text: "Confirm"
                        font.pixelSize: UiMetrics.fontBody
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: if (BluetoothManager) BluetoothManager.confirmPairing()
                    }
                }
            }
        }
    }
}
```

**Step 2: Add to Shell.qml**

In `qml/components/Shell.qml`, after the GestureOverlay line (line 64), add:
```qml
    // Bluetooth pairing confirmation dialog
    PairingDialog {
        id: pairingDialog
    }
```

**Step 3: Register QML file in CMakeLists**

Add `../qml/components/PairingDialog.qml` to the `qt_add_qml_module` QML_FILES list in `src/CMakeLists.txt`.

**Step 4: Build**

Run: `cd build && cmake .. && cmake --build . -j$(nproc)`
Expected: PASS

**Step 5: Commit**

```bash
git add qml/components/PairingDialog.qml qml/components/Shell.qml src/CMakeLists.txt
git commit -m "feat(bt): add PairingDialog QML overlay for pairing confirmation"
```

---

### Task 13: Update ConnectionSettings.qml

**Files:**
- Modify: `qml/applications/settings/ConnectionSettings.qml`

**Step 1: Rewrite Bluetooth section**

Replace lines 48-54 (current Bluetooth section) with:

```qml
        SectionHeader { text: "Bluetooth" }

        ReadOnlyField {
            label: "Device Name"
            configPath: "connection.bt_name"
            placeholder: "OpenAutoProdigy"
        }

        // Repurposed: controls Pairable (not Discoverable)
        Item {
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage
                spacing: UiMetrics.gap
                MaterialIcon { icon: "\ue1b7"; size: UiMetrics.iconSize; color: ThemeService.normalFontColor }
                Text {
                    text: "Accept New Pairings"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }
                Switch {
                    id: pairableSwitch
                    checked: BluetoothManager ? BluetoothManager.pairable : false
                    onToggled: {
                        if (BluetoothManager) BluetoothManager.setPairable(checked)
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left; anchors.right: parent.right
                anchors.leftMargin: UiMetrics.marginPage; anchors.rightMargin: UiMetrics.marginPage
                height: 1; color: ThemeService.descriptionFontColor; opacity: 0.15
            }
        }

        // Paired devices list
        Repeater {
            model: PairedDevicesModel
            delegate: Item {
                Layout.fillWidth: true
                implicitHeight: UiMetrics.rowH
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    spacing: UiMetrics.gap
                    MaterialIcon {
                        icon: model.connected ? "\ue1ba" : "\ue1b9"
                        size: UiMetrics.iconSize
                        color: model.connected ? "#44aa44" : ThemeService.descriptionFontColor
                    }
                    Text {
                        text: model.name || model.address
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.normalFontColor
                        Layout.fillWidth: true
                    }
                    Text {
                        text: "Forget"
                        font.pixelSize: UiMetrics.fontSmall
                        color: "#cc4444"
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -8
                            onClicked: {
                                if (BluetoothManager) BluetoothManager.forgetDevice(model.address)
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.leftMargin: UiMetrics.marginPage; anchors.rightMargin: UiMetrics.marginPage
                    height: 1; color: ThemeService.descriptionFontColor; opacity: 0.15
                }
            }
        }
```

**Step 2: Build**

Run: `cd build && cmake --build . -j$(nproc)`
Expected: PASS

**Step 3: Commit**

```bash
git add qml/applications/settings/ConnectionSettings.qml
git commit -m "feat(bt): update ConnectionSettings with pairable toggle and paired devices list"
```

---

## Phase 7: Install Script & Config

### Task 14: Update install.sh for unified device name

**Files:**
- Modify: `install.sh:164-172` (device name generation)
- Modify: `install.sh:297-323` (config heredoc)

**Step 1: Replace VEHICLE_UUID with DEVICE_NAME**

In `install.sh`, replace lines 164-172:
```bash
        DEVICE_NAME="Prodigy_$(head -c 4 /dev/urandom | xxd -p)"
        echo ""
        info "This name identifies your vehicle on both WiFi and Bluetooth."
        info "The default includes a unique suffix to avoid conflicts with multiple vehicles."
        echo ""
        read -p "Device name [$DEVICE_NAME]: " USER_DEVICE_NAME
        DEVICE_NAME=${USER_DEVICE_NAME:-$DEVICE_NAME}
```

Update `WIFI_SSID` references to use `$DEVICE_NAME` instead.

**Step 2: Update config heredoc**

In the `generate_config()` function (line 297), update the heredoc to include new BT keys:
```yaml
connection:
  wifi_ap:
    interface: "${WIFI_IFACE:-wlan0}"
    ssid: "$DEVICE_NAME"
    password: "$WIFI_PASS"
  tcp_port: $TCP_PORT
  bt_name: "$DEVICE_NAME"
  auto_connect_aa: true
```

**Step 3: Commit**

```bash
git add install.sh
git commit -m "feat(bt): unified device name for WiFi SSID and BT adapter alias"
```

---

### Task 15: Add BlueZ polkit rule

**Files:**
- Create: `config/bluez-agent-polkit.rules`
- Modify: `install.sh` (polkit install section)

**Step 1: Create polkit rule**

Create `config/bluez-agent-polkit.rules`:
```javascript
polkit.addRule(function(action, subject) {
    if (action.id === "org.bluez.request-default-agent" &&
        subject.isInGroup("bluetooth")) {
        return polkit.Result.YES;
    }
});
```

Note: Uses group-based check (`bluetooth` group) rather than hardcoded username. The install script should add the user to the `bluetooth` group.

**Step 2: Update install.sh**

In `generate_config()` (after the companion polkit rule block, around line 334), add:
```bash
    # BlueZ agent polkit rule (allows non-root pairing agent registration)
    if [[ -f "$INSTALL_DIR/config/bluez-agent-polkit.rules" ]]; then
        sudo cp "$INSTALL_DIR/config/bluez-agent-polkit.rules" /etc/polkit-1/rules.d/50-openauto-bluez.rules
        ok "BlueZ agent polkit rule installed"
    fi
```

Also ensure user is in bluetooth group (in package install section):
```bash
sudo usermod -aG bluetooth "$USER"
```

**Step 3: Commit**

```bash
git add config/bluez-agent-polkit.rules install.sh
git commit -m "feat(bt): add BlueZ agent polkit rule and bluetooth group membership"
```

---

## Phase 8: Python System Service Cleanup

### Task 16: Remove Python profile registration (after Pi validation)

**IMPORTANT:** This task should only be done AFTER validating on Pi that the C++ BluetoothManager registers profiles correctly (see Validation Strategy in design doc).

**Files:**
- Delete: `system-service/bt_profiles.py`
- Modify: `system-service/openauto_system.py`
- Modify: `system-service/health_monitor.py`

**Step 1: Remove bt_profiles.py**

```bash
rm system-service/bt_profiles.py
```

**Step 2: Remove BtProfileManager from openauto_system.py**

Remove:
- Import of `BtProfileManager`
- Instantiation of `BtProfileManager()`
- `bt.register_all()` call
- `await bt.close()` in shutdown
- `health.set_bt_restart_callback(bt.register_all)`

**Step 3: Remove bt_restart_callback from health_monitor.py**

Remove:
- `set_bt_restart_callback()` method
- Calls to the callback in BT recovery

Keep: BT adapter health check (`hciconfig hci0` up/down detection), `/var/run/sdp` permission fixes. These are infrastructure-level, not profile-level.

**Step 4: Commit**

```bash
git add -A system-service/
git commit -m "refactor(bt): remove Python profile registration — C++ BluetoothManager owns profiles"
```

---

## Phase 9: Pi Validation

### Task 17: Cross-compile and deploy

**Step 1: Cross-compile**

```bash
./cross-build.sh
```

**Step 2: Deploy to Pi**

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
```

**Step 3: Restart app on Pi**

```bash
ssh matt@192.168.1.152 '~/openauto-prodigy/restart.sh'
```

**Step 4: Verify adapter setup**

```bash
ssh matt@192.168.1.152 'bluetoothctl show' | grep -E 'Name|Discoverable|Pairable|Powered'
```
Expected: Name matches config bt_name, Discoverable=yes, Pairable=no, Powered=yes

**Step 5: Verify profile registration**

```bash
ssh matt@192.168.1.152 'bluetoothctl show' | grep UUID
```
Expected: Shows HFP AG and HSP HS UUIDs

**Step 6: Test pairing**

On phone: Bluetooth settings → scan → find the Pi device → tap to pair.
Expected: Pi shows pairing dialog with 6-digit code. Confirm on both sides → device paired.

**Step 7: Test auto-connect**

Restart the app on Pi. Check logs for auto-connect attempts.

```bash
ssh matt@192.168.1.152 'journalctl -u openauto-prodigy -f' | grep BtManager
```

Expected: Auto-connect retry attempts with backoff intervals.

**Step 8: Verify Connection settings UI**

Navigate to Settings → Connection on the Pi touchscreen.
Expected: Device name shown, "Accept New Pairings" toggle, paired device(s) listed with connected indicator.

---

### Task 18: Disable Python profile registration and verify

**Step 1: Comment out Python profiles**

```bash
ssh matt@192.168.1.152 'cd ~/openauto-prodigy/system-service && sed -i "s/await bt.register_all/#await bt.register_all/" openauto_system.py'
```

**Step 2: Restart both services**

```bash
ssh matt@192.168.1.152 'sudo systemctl restart openauto-system && ~/openauto-prodigy/restart.sh'
```

**Step 3: Verify phone connects**

Check that HFP/HSP profile connections are handled by the C++ `BluezProfile1Adaptor` (not Python). Look for `[BtManager] Profile NewConnection` in logs.

**Step 4: If successful, proceed with Task 16 (Python cleanup)**

---

## Summary

| Phase | Tasks | What it delivers |
|-------|-------|-----------------|
| 1 | 1-4 | Interface, skeleton, main.cpp wiring, tests |
| 2 | 5-6 | Adapter D-Bus setup, Agent1 pairing |
| 3 | 7-8 | PairedDevicesModel, device enumeration |
| 4 | 9 | Auto-connect with backoff |
| 5 | 10-11 | Profile consolidation (move to C++, remove from BtDiscovery) |
| 6 | 12-13 | PairingDialog QML, ConnectionSettings update |
| 7 | 14-15 | install.sh unified naming, polkit rule |
| 8 | 16 | Python cleanup (after Pi validation) |
| 9 | 17-18 | Pi validation, full integration test |
