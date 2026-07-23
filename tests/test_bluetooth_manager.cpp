#include <QtTest>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QSignalSpy>
#include <QElapsedTimer>

#include "core/services/BluetoothManager.hpp"
#include "core/services/IConfigService.hpp"
#include "ui/PairedDevicesModel.hpp"

namespace oap {

class BluetoothManagerTestAccess {
public:
    static bool initialSnapshotApplied(const BluetoothManager& manager)
    {
        return manager.initialSnapshotApplied_;
    }

    static bool autoConnectInFlight(const BluetoothManager& manager)
    {
        return manager.autoConnectInFlight_;
    }

    static bool autoConnectTimerActive(const BluetoothManager& manager)
    {
        return manager.autoConnectTimer_ && manager.autoConnectTimer_->isActive();
    }

    static quint64 autoConnectGeneration(const BluetoothManager& manager)
    {
        return manager.autoConnectGeneration_;
    }

    static QStringList autoConnectPaths(const BluetoothManager& manager)
    {
        return manager.pairedDevicePaths_;
    }

    static QString configuredAdapterPath(const BluetoothManager& manager)
    {
        return manager.configuredAdapterPath_;
    }

    static bool hasPendingPairingDecision(const BluetoothManager& manager)
    {
        return !manager.pendingPairingDevicePath_.isEmpty()
            || manager.pendingPairingMessage_.type()
                == QDBusMessage::MethodCallMessage;
    }
};

} // namespace oap

class MockConfigService : public oap::IConfigService {
public:
    QVariant value(const QString& path) const override { return values_.value(path); }
    void setValue(const QString& path, const QVariant& value) override { values_[path] = value; }
    void save() override {}
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}

    QMap<QString, QVariant> values_;
};

using TestBluezManagedObjectMap = QMap<QDBusObjectPath, oap::BluezInterfaceMap>;
Q_DECLARE_METATYPE(TestBluezManagedObjectMap)

namespace {

class BluezObjectManagerFixture : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.ObjectManager")
public:
    TestBluezManagedObjectMap objects;
    int replyDelayMs = 200;
    int callCount = 0;
    int replyCount = 0;
    bool failNext = false;
    bool malformedNext = false;

signals:
    void replySent();
    void InterfacesAdded(const QDBusObjectPath& path,
                         const oap::BluezInterfaceMap& interfaces);
    void InterfacesRemoved(const QDBusObjectPath& path,
                           const QStringList& interfaces);

public slots:
    void GetManagedObjects()
    {
        ++callCount;
        setDelayedReply(true);
        const QDBusConnection replyBus = connection();
        QDBusMessage reply;
        if (failNext) {
            failNext = false;
            reply = message().createErrorReply(
                QStringLiteral("org.openauto.TestError"), QStringLiteral("planned failure"));
        } else if (malformedNext) {
            malformedNext = false;
            reply = message().createReply(QStringLiteral("not a managed-object map"));
        } else {
            reply = message().createReply(QVariant::fromValue(objects));
        }
        QTimer::singleShot(replyDelayMs, this, [this, replyBus, reply]() {
            replyBus.send(reply);
            ++replyCount;
            emit replySent();
        });
    }
};

class DelayedConnectFixture : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Device1")
public:
    explicit DelayedConnectFixture(const QDBusConnection& bus)
        : bus_(bus)
    {
    }

    int callCount(const QString& path) const { return callsByPath_.value(path); }

    bool replyNext()
    {
        if (pendingCalls_.isEmpty())
            return false;
        return bus_.send(pendingCalls_.takeFirst().createReply());
    }

public slots:
    void Connect()
    {
        ++callsByPath_[message().path()];
        pendingCalls_.append(message());
        setDelayedReply(true);
    }

private:
    QDBusConnection bus_;
    QMap<QString, int> callsByPath_;
    QList<QDBusMessage> pendingCalls_;
};

const QString kAdapterPath = QStringLiteral("/org/bluez/hci0");
const QString kDevicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
const QString kSecondDevicePath =
    QStringLiteral("/org/bluez/hci0/dev_11_22_33_44_55_66");
const QString kSecondAdapterPath = QStringLiteral("/org/bluez/hci1");
const QString kReplacementDevicePath =
    QStringLiteral("/org/bluez/hci1/dev_11_22_33_44_55_66");

oap::BluezManagedObjectMap adapterSnapshot()
{
    oap::BluezManagedObjectMap objects;
    objects[kAdapterPath][QStringLiteral("org.bluez.Adapter1")] = {
        {QStringLiteral("Address"), QStringLiteral("11:22:33:44:55:66")},
        {QStringLiteral("Pairable"), false},
    };
    return objects;
}

void addDevice(oap::BluezManagedObjectMap& objects, bool paired, bool connected,
               const QString& name = QStringLiteral("Pixel"))
{
    objects[kDevicePath][QStringLiteral("org.bluez.Device1")] = {
        {QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
        {QStringLiteral("Name"), name},
        {QStringLiteral("Paired"), paired},
        {QStringLiteral("Connected"), connected},
    };
}

QDBusMessage agentCall(const QDBusConnection& serverBus, const QString& method)
{
    return QDBusMessage::createMethodCall(
        serverBus.baseService(), QStringLiteral("/org/openauto/agent"),
        QStringLiteral("org.bluez.Agent1"), method);
}

QDBusMessage awaitReply(QDBusPendingCallWatcher& watcher)
{
    if (!watcher.isFinished()) {
        QEventLoop loop;
        QObject::connect(&watcher, &QDBusPendingCallWatcher::finished,
                         &loop, &QEventLoop::quit);
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return watcher.reply();
}

} // namespace

class TestBluetoothManager : public QObject {
    Q_OBJECT

private slots:
    void testInitialState();
    void testSnapshotSeedsConnectedStateAndEdges();
    void testFirstRunAndRemovalFollowSnapshots();
    void testAdapterRemovalAndReappearanceRestartPolicyOnce();
    void testDirectAdapterReplacementClearsOldEpochState();
    void testRemoveReaddSignalStartsNewSamePathEpoch();
    void testAdapterEpochResetCancelsDelayedConfirmation();
    void testAgentMethodSurfaceAndPromptModes();
    void testAsyncRefreshCoalescesAndAppliesFinalSnapshot();
    void testValidRefreshSurvivesFailingTrailingRefresh();
    void testSustainedRefreshTrafficPublishesSnapshots();
    void testBluezLossDiscardsOldManagedObjectsReply();
    void testMalformedManagedObjectsReplyRetainsState();
    void testOverlappingAgentPromptsCancelPreviousDecision();
    void testAgentReleaseCancelsDelayedConfirmation();
    void testShutdownCancelsDelayedConfirmation();
    void testBluezLossCancelsDelayedConfirmation();
    void testPairableToggle();
    void testAutoConnectLifecycle();
};

void TestBluetoothManager::testAsyncRefreshCoalescesAndAppliesFinalSnapshot()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    qDBusRegisterMetaType<oap::BluezInterfaceMap>();
    qDBusRegisterMetaType<TestBluezManagedObjectMap>();

    BluezObjectManagerFixture fixture;
    auto staleObjects = adapterSnapshot();
    addDevice(staleObjects, true, true, QStringLiteral("Stale Pixel"));
    for (auto it = staleObjects.cbegin(); it != staleObjects.cend(); ++it)
        fixture.objects[QDBusObjectPath(it.key())] = it.value();
    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    oap::BluetoothManager mgr(&config, bus);
    QSignalSpy connectedSpy(&mgr, &oap::BluetoothManager::connectedDeviceChanged);
    int heartbeats = 0;
    QTimer heartbeat;
    heartbeat.setInterval(10);
    connect(&heartbeat, &QTimer::timeout, this, [&]() { ++heartbeats; });
    heartbeat.start();

    QElapsedTimer elapsed;
    elapsed.start();
    mgr.initialize();
    QVERIFY2(elapsed.elapsed() < 100, "initialize must not block on GetManagedObjects");
    QTRY_COMPARE(fixture.callCount, 1);

    fixture.objects[QDBusObjectPath(kDevicePath)]
        [QStringLiteral("org.bluez.Device1")][QStringLiteral("Name")] =
            QStringLiteral("Pixel");
    for (int i = 0; i < 4; ++i)
        QVERIFY(QMetaObject::invokeMethod(&mgr, "onInterfacesChanged",
                                          Qt::DirectConnection));

    QTRY_COMPARE_WITH_TIMEOUT(fixture.callCount, 2, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(mgr.connectedDeviceName(), QStringLiteral("Pixel"), 3000);
    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));
    QCOMPARE(connectedSpy.count(), 2);
    QVERIFY(heartbeats > 0);
    QCOMPARE(fixture.callCount, 2);

    mgr.shutdown();
    const QString connectedAfterShutdown = mgr.connectedDeviceName();
    QVERIFY(bus.unregisterService(QStringLiteral("org.bluez")));
    QTest::qWait(50);
    QCOMPARE(mgr.connectedDeviceName(), connectedAfterShutdown);
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBluetoothManager::testValidRefreshSurvivesFailingTrailingRefresh()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    qDBusRegisterMetaType<oap::BluezInterfaceMap>();
    qDBusRegisterMetaType<TestBluezManagedObjectMap>();

    BluezObjectManagerFixture fixture;
    auto objects = adapterSnapshot();
    addDevice(objects, true, true, QStringLiteral("First Pixel"));
    for (auto it = objects.cbegin(); it != objects.cend(); ++it)
        fixture.objects[QDBusObjectPath(it.key())] = it.value();
    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    oap::BluetoothManager mgr(&config, bus);
    mgr.initialize();
    QTRY_COMPARE(fixture.callCount, 1);

    fixture.failNext = true;
    QVERIFY(QMetaObject::invokeMethod(&mgr, "onInterfacesChanged",
                                      Qt::DirectConnection));

    QTRY_COMPARE_WITH_TIMEOUT(fixture.replyCount, 2, 3000);
    QCOMPARE(fixture.callCount, 2);
    QCOMPARE(mgr.connectedDeviceName(), QStringLiteral("First Pixel"));
    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));

    mgr.shutdown();
    QVERIFY(bus.unregisterService(QStringLiteral("org.bluez")));
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBluetoothManager::testSustainedRefreshTrafficPublishesSnapshots()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    qDBusRegisterMetaType<oap::BluezInterfaceMap>();
    qDBusRegisterMetaType<TestBluezManagedObjectMap>();

    BluezObjectManagerFixture fixture;
    fixture.replyDelayMs = 80;
    auto objects = adapterSnapshot();
    addDevice(objects, true, true, QStringLiteral("Streaming Pixel"));
    for (auto it = objects.cbegin(); it != objects.cend(); ++it)
        fixture.objects[QDBusObjectPath(it.key())] = it.value();
    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    oap::BluetoothManager mgr(&config, bus);
    mgr.initialize();
    QTRY_COMPARE(fixture.callCount, 1);

    QTimer refreshTraffic;
    refreshTraffic.setInterval(10);
    connect(&refreshTraffic, &QTimer::timeout, &mgr, [&mgr]() {
        QVERIFY(QMetaObject::invokeMethod(&mgr, "onInterfacesChanged",
                                          Qt::DirectConnection));
    });
    refreshTraffic.start();

    QTRY_COMPARE_WITH_TIMEOUT(mgr.connectedDeviceName(),
                              QStringLiteral("Streaming Pixel"), 1000);
    QVERIFY(refreshTraffic.isActive());
    refreshTraffic.stop();
    QTRY_VERIFY_WITH_TIMEOUT(fixture.callCount >= 2
                             && fixture.replyCount == fixture.callCount, 3000);
    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));

    mgr.shutdown();
    QVERIFY(bus.unregisterService(QStringLiteral("org.bluez")));
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBluetoothManager::testBluezLossDiscardsOldManagedObjectsReply()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    qDBusRegisterMetaType<oap::BluezInterfaceMap>();
    qDBusRegisterMetaType<TestBluezManagedObjectMap>();

    BluezObjectManagerFixture fixture;
    auto staleObjects = adapterSnapshot();
    addDevice(staleObjects, true, true, QStringLiteral("Stale Pixel"));
    for (auto it = staleObjects.cbegin(); it != staleObjects.cend(); ++it)
        fixture.objects[QDBusObjectPath(it.key())] = it.value();
    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    oap::BluetoothManager mgr(&config, bus);
    auto seed = adapterSnapshot();
    addDevice(seed, true, true, QStringLiteral("Seed Pixel"));
    mgr.applyManagedObjectsSnapshot(seed);
    QCOMPARE(mgr.connectedDeviceName(), QStringLiteral("Seed Pixel"));

    mgr.initialize();
    QTRY_COMPARE(fixture.callCount, 1);
    QVERIFY(bus.unregisterService(QStringLiteral("org.bluez")));
    QTRY_VERIFY(mgr.connectedDeviceName().isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(fixture.replyCount, 1, 3000);
    QTest::qWait(50);
    QCOMPARE(mgr.connectedDeviceName(), QString());
    QVERIFY(mgr.adapterAddress().isEmpty());

    mgr.shutdown();
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBluetoothManager::testMalformedManagedObjectsReplyRetainsState()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    qDBusRegisterMetaType<oap::BluezInterfaceMap>();
    qDBusRegisterMetaType<TestBluezManagedObjectMap>();

    BluezObjectManagerFixture fixture;
    auto validObjects = adapterSnapshot();
    addDevice(validObjects, true, true);
    for (auto it = validObjects.cbegin(); it != validObjects.cend(); ++it)
        fixture.objects[QDBusObjectPath(it.key())] = it.value();
    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    oap::BluetoothManager mgr(&config, bus);
    mgr.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(fixture.replyCount, 1, 3000);
    QCOMPARE(mgr.connectedDeviceName(), QStringLiteral("Pixel"));

    fixture.failNext = true;
    QVERIFY(QMetaObject::invokeMethod(&mgr, "onInterfacesChanged",
                                      Qt::DirectConnection));
    QTRY_COMPARE_WITH_TIMEOUT(fixture.replyCount, 2, 3000);
    QTest::qWait(50);
    QCOMPARE(mgr.connectedDeviceName(), QStringLiteral("Pixel"));
    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));

    fixture.malformedNext = true;
    QVERIFY(QMetaObject::invokeMethod(&mgr, "onInterfacesChanged",
                                      Qt::DirectConnection));
    QTRY_COMPARE_WITH_TIMEOUT(fixture.replyCount, 3, 3000);
    QTest::qWait(50);
    QCOMPARE(mgr.connectedDeviceName(), QStringLiteral("Pixel"));
    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));

    fixture.objects.clear();
    QVERIFY(QMetaObject::invokeMethod(&mgr, "onInterfacesChanged",
                                      Qt::DirectConnection));
    QTRY_COMPARE_WITH_TIMEOUT(fixture.replyCount, 4, 3000);
    QTRY_VERIFY(mgr.connectedDeviceName().isEmpty());
    QTRY_VERIFY(mgr.adapterAddress().isEmpty());

    mgr.shutdown();
    QVERIFY(bus.unregisterService(QStringLiteral("org.bluez")));
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBluetoothManager::testInitialState()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config, QDBusConnection::sessionBus());

    QVERIFY(mgr.adapterAddress().isEmpty());
    QVERIFY(!mgr.isPairable());
    QVERIFY(!mgr.isPairingActive());
    QVERIFY(!mgr.pairingRequiresConfirmation());
    QVERIFY(mgr.connectedDeviceName().isEmpty());
}

void TestBluetoothManager::testSnapshotSeedsConnectedStateAndEdges()
{
    MockConfigService config;
    config.values_[QStringLiteral("connection.bt_name")] = QStringLiteral("TestProdigy");
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    oap::BluetoothManager mgr(&config, QDBusConnection::sessionBus());
    QSignalSpy connectedSpy(&mgr, &oap::BluetoothManager::connectedDeviceChanged);
    QSignalSpy addressSpy(&mgr, &oap::BluetoothManager::adapterAddressChanged);

    auto objects = adapterSnapshot();
    addDevice(objects, true, true);
    mgr.applyManagedObjectsSnapshot(objects);

    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));
    QCOMPARE(addressSpy.count(), 1);
    QCOMPARE(mgr.connectedDeviceName(), QStringLiteral("Pixel"));
    QCOMPARE(mgr.connectedDeviceAddress(), QStringLiteral("AA:BB:CC:DD:EE:FF"));
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(mgr.pairedDevicesModel()->rowCount(), 1);

    mgr.applyManagedObjectsSnapshot(objects);
    QCOMPARE(connectedSpy.count(), 1);
    QCOMPARE(addressSpy.count(), 1);

    objects[kAdapterPath][QStringLiteral("org.bluez.Adapter1")]
        .remove(QStringLiteral("Address"));
    mgr.applyManagedObjectsSnapshot(objects);
    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));
    QCOMPARE(addressSpy.count(), 1);

    addDevice(objects, true, false);
    mgr.applyManagedObjectsSnapshot(objects);
    QVERIFY(mgr.connectedDeviceName().isEmpty());
    QVERIFY(mgr.connectedDeviceAddress().isEmpty());
    QCOMPARE(connectedSpy.count(), 2);

    objects.remove(kDevicePath);
    mgr.applyManagedObjectsSnapshot(objects);
    QCOMPARE(mgr.pairedDevicesModel()->rowCount(), 0);
    QCOMPARE(connectedSpy.count(), 2);

    mgr.applyManagedObjectsSnapshot({});
    QVERIFY(mgr.adapterAddress().isEmpty());
    QCOMPARE(addressSpy.count(), 2);
}

void TestBluetoothManager::testFirstRunAndRemovalFollowSnapshots()
{
    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    oap::BluetoothManager mgr(&config, QDBusConnection::sessionBus());
    QSignalSpy firstRunSpy(&mgr, &oap::BluetoothManager::needsFirstPairingChanged);

    auto objects = adapterSnapshot();
    mgr.applyManagedObjectsSnapshot(objects);
    QVERIFY(mgr.needsFirstPairing());
    QVERIFY(mgr.isPairable());
    QCOMPARE(firstRunSpy.count(), 1);

    mgr.applyManagedObjectsSnapshot(objects);
    QCOMPARE(firstRunSpy.count(), 1);

    addDevice(objects, true, false);
    mgr.applyManagedObjectsSnapshot(objects);
    QVERIFY(!mgr.needsFirstPairing());
    QCOMPARE(firstRunSpy.count(), 2);

    objects.remove(kDevicePath);
    mgr.applyManagedObjectsSnapshot(objects);
    QVERIFY(!mgr.needsFirstPairing());
    QCOMPARE(firstRunSpy.count(), 2);
}

void TestBluetoothManager::testAdapterRemovalAndReappearanceRestartPolicyOnce()
{
    QDBusConnection serverBus = QDBusConnection::sessionBus();
    QVERIFY(serverBus.isConnected());
    DelayedConnectFixture connectFixture(serverBus);
    QVERIFY(serverBus.registerObject(kDevicePath, &connectFixture,
                                     QDBusConnection::ExportAllSlots));
    QVERIFY(serverBus.registerService(QStringLiteral("org.bluez")));

    const QString clientName = QStringLiteral("oap-adapter-reappear-client");
    const QDBusConnection clientBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(clientBus.isConnected());

    MockConfigService config;
    oap::BluetoothManager mgr(&config, clientBus);
    auto objects = adapterSnapshot();
    addDevice(objects, true, false);
    mgr.applyManagedObjectsSnapshot(objects);
    QTRY_COMPARE(connectFixture.callCount(kDevicePath), 1);
    QVERIFY(oap::BluetoothManagerTestAccess::initialSnapshotApplied(mgr));
    QVERIFY(oap::BluetoothManagerTestAccess::autoConnectInFlight(mgr));
    const quint64 firstGeneration =
        oap::BluetoothManagerTestAccess::autoConnectGeneration(mgr);

    mgr.applyManagedObjectsSnapshot({});
    QVERIFY(!oap::BluetoothManagerTestAccess::initialSnapshotApplied(mgr));
    QVERIFY(!oap::BluetoothManagerTestAccess::autoConnectInFlight(mgr));
    QVERIFY(oap::BluetoothManagerTestAccess::autoConnectPaths(mgr).isEmpty());
    QVERIFY(mgr.adapterAddress().isEmpty());
    QVERIFY(mgr.adapterAlias().isEmpty());
    QVERIFY(!mgr.isDiscoverable());
    QVERIFY(!mgr.isPairable());
    QVERIFY(oap::BluetoothManagerTestAccess::autoConnectGeneration(mgr)
            > firstGeneration);

    mgr.applyManagedObjectsSnapshot(objects);
    QTRY_COMPARE(connectFixture.callCount(kDevicePath), 2);
    QVERIFY(oap::BluetoothManagerTestAccess::initialSnapshotApplied(mgr));
    QVERIFY(oap::BluetoothManagerTestAccess::autoConnectInFlight(mgr));
    mgr.applyManagedObjectsSnapshot(objects);
    QTest::qWait(50);
    QCOMPARE(connectFixture.callCount(kDevicePath), 2);

    // Completing the request from the removed adapter epoch must not mutate
    // the replacement epoch's in-flight state or arm its retry timer.
    QVERIFY(connectFixture.replyNext());
    QTest::qWait(50);
    QVERIFY(oap::BluetoothManagerTestAccess::autoConnectInFlight(mgr));
    QVERIFY(!oap::BluetoothManagerTestAccess::autoConnectTimerActive(mgr));
    QCOMPARE(connectFixture.callCount(kDevicePath), 2);

    mgr.shutdown();
    QVERIFY(serverBus.unregisterService(QStringLiteral("org.bluez")));
    serverBus.unregisterObject(kDevicePath);
    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testDirectAdapterReplacementClearsOldEpochState()
{
    QDBusConnection serverBus = QDBusConnection::sessionBus();
    QVERIFY(serverBus.isConnected());
    DelayedConnectFixture connectFixture(serverBus);
    QVERIFY(serverBus.registerObject(kDevicePath, &connectFixture,
                                     QDBusConnection::ExportAllSlots));
    QVERIFY(serverBus.registerObject(kReplacementDevicePath, &connectFixture,
                                     QDBusConnection::ExportAllSlots));
    QVERIFY(serverBus.registerService(QStringLiteral("org.bluez")));

    const QString clientName = QStringLiteral("oap-adapter-replace-client");
    const QDBusConnection clientBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(clientBus.isConnected());

    MockConfigService config;
    oap::BluetoothManager mgr(&config, clientBus);
    QSignalSpy addressSpy(&mgr, &oap::BluetoothManager::adapterAddressChanged);
    QSignalSpy aliasSpy(&mgr, &oap::BluetoothManager::adapterAliasChanged);
    QSignalSpy discoverableSpy(&mgr,
                               &oap::BluetoothManager::discoverableChanged);

    auto oldObjects = adapterSnapshot();
    addDevice(oldObjects, true, false);
    mgr.applyManagedObjectsSnapshot(oldObjects);
    QTRY_COMPARE(connectFixture.callCount(kDevicePath), 1);
    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));

    oap::BluezManagedObjectMap replacementObjects;
    replacementObjects[kSecondAdapterPath]
        [QStringLiteral("org.bluez.Adapter1")] = {
            {QStringLiteral("Pairable"), false},
        };
    replacementObjects[kReplacementDevicePath]
        [QStringLiteral("org.bluez.Device1")] = {
            {QStringLiteral("Address"), QStringLiteral("11:22:33:44:55:66")},
            {QStringLiteral("Name"), QStringLiteral("Moto")},
            {QStringLiteral("Paired"), true},
            {QStringLiteral("Connected"), false},
        };
    mgr.applyManagedObjectsSnapshot(replacementObjects);
    QTRY_COMPARE(connectFixture.callCount(kReplacementDevicePath), 1);

    // Address omission is tolerated only within one adapter path. The hci0
    // address must not bleed into hci1 when its bounded fallback also fails.
    QVERIFY(mgr.adapterAddress().isEmpty());
    QCOMPARE(addressSpy.count(), 2);
    QCOMPARE(oap::BluetoothManagerTestAccess::configuredAdapterPath(mgr),
             kSecondAdapterPath);
    QCOMPARE(oap::BluetoothManagerTestAccess::autoConnectPaths(mgr),
             QStringList{kReplacementDevicePath});
    QVERIFY(oap::BluetoothManagerTestAccess::initialSnapshotApplied(mgr));
    QVERIFY(oap::BluetoothManagerTestAccess::autoConnectInFlight(mgr));

    const int aliasEdges = aliasSpy.count();
    const int discoverableEdges = discoverableSpy.count();
    mgr.applyManagedObjectsSnapshot(replacementObjects);
    QTest::qWait(50);
    QCOMPARE(connectFixture.callCount(kReplacementDevicePath), 1);
    QCOMPARE(aliasSpy.count(), aliasEdges);
    QCOMPARE(discoverableSpy.count(), discoverableEdges);

    QVERIFY(connectFixture.replyNext());
    QTest::qWait(50);
    QVERIFY(oap::BluetoothManagerTestAccess::autoConnectInFlight(mgr));
    QVERIFY(!oap::BluetoothManagerTestAccess::autoConnectTimerActive(mgr));

    mgr.shutdown();
    QVERIFY(serverBus.unregisterService(QStringLiteral("org.bluez")));
    serverBus.unregisterObject(kDevicePath);
    serverBus.unregisterObject(kReplacementDevicePath);
    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testRemoveReaddSignalStartsNewSamePathEpoch()
{
    QDBusConnection serverBus = QDBusConnection::sessionBus();
    QVERIFY(serverBus.isConnected());
    qDBusRegisterMetaType<oap::BluezInterfaceMap>();
    qDBusRegisterMetaType<TestBluezManagedObjectMap>();

    BluezObjectManagerFixture objectManager;
    objectManager.replyDelayMs = 150;
    auto objects = adapterSnapshot();
    addDevice(objects, true, false);
    for (auto it = objects.cbegin(); it != objects.cend(); ++it)
        objectManager.objects[QDBusObjectPath(it.key())] = it.value();
    DelayedConnectFixture connectFixture(serverBus);
    QVERIFY(serverBus.registerObject(
        QStringLiteral("/"), &objectManager,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
    QVERIFY(serverBus.registerObject(kDevicePath, &connectFixture,
                                     QDBusConnection::ExportAllSlots));
    QVERIFY(serverBus.registerService(QStringLiteral("org.bluez")));

    const QString clientName = QStringLiteral("oap-adapter-same-path-client");
    const QDBusConnection clientBus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(clientBus.isConnected());

    MockConfigService config;
    oap::BluetoothManager mgr(&config, clientBus);
    mgr.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(connectFixture.callCount(kDevicePath), 1, 20000);
    QVERIFY(oap::BluetoothManagerTestAccess::initialSnapshotApplied(mgr));
    QCOMPARE(mgr.adapterAddress(), QStringLiteral("11:22:33:44:55:66"));

    // Keep a pre-removal snapshot in flight, then remove and re-add Adapter1
    // at the same object path before that reply completes.
    QVERIFY(QMetaObject::invokeMethod(&mgr, "onInterfacesChanged",
                                      Qt::DirectConnection));
    QTRY_COMPARE(objectManager.callCount, 2);
    const quint64 oldAutoConnectGeneration =
        oap::BluetoothManagerTestAccess::autoConnectGeneration(mgr);
    emit objectManager.InterfacesRemoved(
        QDBusObjectPath(kAdapterPath),
        QStringList{QStringLiteral("org.bluez.Adapter1")});
    emit objectManager.InterfacesAdded(
        QDBusObjectPath(kAdapterPath),
        objectManager.objects.value(QDBusObjectPath(kAdapterPath)));

    QTRY_VERIFY_WITH_TIMEOUT(
        oap::BluetoothManagerTestAccess::autoConnectGeneration(mgr)
            > oldAutoConnectGeneration,
        3000);
    QVERIFY(!oap::BluetoothManagerTestAccess::initialSnapshotApplied(mgr));
    QVERIFY(mgr.adapterAddress().isEmpty());
    QVERIFY(mgr.adapterAlias().isEmpty());
    QVERIFY(!mgr.isDiscoverable());
    QTRY_COMPARE_WITH_TIMEOUT(objectManager.callCount, 3, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(connectFixture.callCount(kDevicePath), 2, 20000);
    QVERIFY(oap::BluetoothManagerTestAccess::initialSnapshotApplied(mgr));

    // A completed same-epoch snapshot refreshes state without rerunning setup
    // or starting another auto-connect policy pass.
    emit objectManager.InterfacesAdded(
        QDBusObjectPath(kAdapterPath),
        objectManager.objects.value(QDBusObjectPath(kAdapterPath)));
    QTRY_COMPARE_WITH_TIMEOUT(objectManager.replyCount, 4, 3000);
    QCOMPARE(connectFixture.callCount(kDevicePath), 2);

    QVERIFY(connectFixture.replyNext());
    QTest::qWait(50);
    QVERIFY(oap::BluetoothManagerTestAccess::autoConnectInFlight(mgr));
    QVERIFY(!oap::BluetoothManagerTestAccess::autoConnectTimerActive(mgr));

    mgr.shutdown();
    QVERIFY(serverBus.unregisterService(QStringLiteral("org.bluez")));
    serverBus.unregisterObject(QStringLiteral("/"));
    serverBus.unregisterObject(kDevicePath);
    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testAdapterEpochResetCancelsDelayedConfirmation()
{
    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    const QDBusConnection serverBus = QDBusConnection::sessionBus();
    oap::BluetoothManager mgr(&config, serverBus);

    auto oldObjects = adapterSnapshot();
    addDevice(oldObjects, false, false);
    mgr.applyManagedObjectsSnapshot(oldObjects);

    oap::BluezManagedObjectMap replacementObjects;
    replacementObjects[kSecondAdapterPath]
        [QStringLiteral("org.bluez.Adapter1")] = {
            {QStringLiteral("Address"), QStringLiteral("66:55:44:33:22:11")},
            {QStringLiteral("Pairable"), false},
        };
    replacementObjects[kReplacementDevicePath]
        [QStringLiteral("org.bluez.Device1")] = {
            {QStringLiteral("Address"), QStringLiteral("11:22:33:44:55:66")},
            {QStringLiteral("Name"), QStringLiteral("Moto")},
            {QStringLiteral("Paired"), false},
            {QStringLiteral("Connected"), false},
        };

    const QString clientName = QStringLiteral("oap-agent-adapter-reset-client");
    const QDBusConnection client =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(client.isConnected());

    QDBusMessage replaceConfirm =
        agentCall(serverBus, QStringLiteral("RequestConfirmation"));
    replaceConfirm << QVariant::fromValue(QDBusObjectPath(kDevicePath))
                   << QVariant::fromValue(quint32(112233));
    QDBusPendingCallWatcher replaceWatcher(
        client.asyncCall(replaceConfirm, 5000));
    QTRY_VERIFY(mgr.isPairingActive());
    mgr.applyManagedObjectsSnapshot(replacementObjects);
    const QDBusMessage replaceReply = awaitReply(replaceWatcher);
    QCOMPARE(replaceReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(replaceReply.errorName(), QStringLiteral("org.bluez.Error.Canceled"));
    QVERIFY(!mgr.isPairingActive());
    QVERIFY(!oap::BluetoothManagerTestAccess::hasPendingPairingDecision(mgr));
    mgr.confirmPairing();
    QVERIFY(!mgr.isPairingActive());

    QDBusMessage removeConfirm =
        agentCall(serverBus, QStringLiteral("RequestConfirmation"));
    removeConfirm << QVariant::fromValue(QDBusObjectPath(kReplacementDevicePath))
                  << QVariant::fromValue(quint32(445566));
    QDBusPendingCallWatcher removeWatcher(
        client.asyncCall(removeConfirm, 5000));
    QTRY_VERIFY(mgr.isPairingActive());
    mgr.applyManagedObjectsSnapshot({});
    const QDBusMessage removeReply = awaitReply(removeWatcher);
    QCOMPARE(removeReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(removeReply.errorName(), QStringLiteral("org.bluez.Error.Canceled"));
    QVERIFY(!mgr.isPairingActive());
    QVERIFY(!oap::BluetoothManagerTestAccess::hasPendingPairingDecision(mgr));
    mgr.confirmPairing();
    QVERIFY(!mgr.isPairingActive());

    mgr.shutdown();
    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testAgentMethodSurfaceAndPromptModes()
{
    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    const QDBusConnection serverBus = QDBusConnection::sessionBus();
    oap::BluetoothManager mgr(&config, serverBus);
    auto objects = adapterSnapshot();
    addDevice(objects, false, false);
    mgr.applyManagedObjectsSnapshot(objects);

    const QString clientName = QStringLiteral("oap-agent-test-client");
    const QDBusConnection client =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(client.isConnected());

    QDBusMessage introspect = QDBusMessage::createMethodCall(
        serverBus.baseService(), QStringLiteral("/org/openauto/agent"),
        QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
    QDBusPendingCallWatcher introspectWatcher(client.asyncCall(introspect));
    const QDBusMessage introspectReply = awaitReply(introspectWatcher);
    QCOMPARE(introspectReply.type(), QDBusMessage::ReplyMessage);
    const QString xml = introspectReply.arguments().constFirst().toString();
    for (const QString& method : {QStringLiteral("Release"),
                                  QStringLiteral("RequestPinCode"),
                                  QStringLiteral("RequestPasskey"),
                                  QStringLiteral("DisplayPinCode"),
                                  QStringLiteral("DisplayPasskey"),
                                  QStringLiteral("RequestConfirmation"),
                                  QStringLiteral("RequestAuthorization"),
                                  QStringLiteral("AuthorizeService"),
                                  QStringLiteral("Cancel")}) {
        QVERIFY2(xml.contains(QStringLiteral("name=\"") + method + QLatin1Char('"')),
                 qPrintable(method));
    }

    QDBusMessage display = agentCall(serverBus, QStringLiteral("DisplayPasskey"));
    display << QVariant::fromValue(QDBusObjectPath(kDevicePath))
            << QVariant::fromValue(quint32(123456)) << QVariant::fromValue(quint16(0));
    QDBusPendingCallWatcher displayWatcher(client.asyncCall(display));
    QCOMPARE(awaitReply(displayWatcher).type(), QDBusMessage::ReplyMessage);
    QVERIFY(mgr.isPairingActive());
    QVERIFY(!mgr.pairingRequiresConfirmation());
    QCOMPARE(mgr.pairingDeviceName(), QStringLiteral("Pixel"));
    QCOMPARE(mgr.pairingPasskey(), QStringLiteral("123456"));
    QCOMPARE(mgr.pairingEntered(), 0);

    QSignalSpy progressSpy(&mgr, &oap::BluetoothManager::pairingActiveChanged);
    QDBusMessage progress = agentCall(serverBus, QStringLiteral("DisplayPasskey"));
    progress << QVariant::fromValue(QDBusObjectPath(kDevicePath))
             << QVariant::fromValue(quint32(123456)) << QVariant::fromValue(quint16(2));
    QDBusPendingCallWatcher progressWatcher(client.asyncCall(progress));
    QCOMPARE(awaitReply(progressWatcher).type(), QDBusMessage::ReplyMessage);
    QCOMPARE(mgr.pairingEntered(), 2);
    QCOMPARE(progressSpy.count(), 1);
    mgr.confirmPairing();
    QVERIFY(!mgr.isPairingActive());

    QDBusMessage pin = agentCall(serverBus, QStringLiteral("RequestPinCode"));
    pin << QVariant::fromValue(QDBusObjectPath(kDevicePath));
    QDBusPendingCallWatcher pinWatcher(client.asyncCall(pin));
    const QDBusMessage pinReply = awaitReply(pinWatcher);
    QCOMPARE(pinReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(pinReply.errorName(), QStringLiteral("org.bluez.Error.Rejected"));

    QDBusMessage passkey = agentCall(serverBus, QStringLiteral("RequestPasskey"));
    passkey << QVariant::fromValue(QDBusObjectPath(kDevicePath));
    QDBusPendingCallWatcher passkeyWatcher(client.asyncCall(passkey));
    const QDBusMessage passkeyReply = awaitReply(passkeyWatcher);
    QCOMPARE(passkeyReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(passkeyReply.errorName(), QStringLiteral("org.bluez.Error.Rejected"));

    QDBusMessage confirm = agentCall(serverBus, QStringLiteral("RequestConfirmation"));
    confirm << QVariant::fromValue(QDBusObjectPath(kDevicePath))
            << QVariant::fromValue(quint32(654321));
    QDBusPendingCallWatcher confirmWatcher(client.asyncCall(confirm, 5000));
    QTRY_VERIFY(mgr.isPairingActive());
    QVERIFY(mgr.pairingRequiresConfirmation());
    QCOMPARE(mgr.pairingPasskey(), QStringLiteral("654321"));
    mgr.confirmPairing();
    QCOMPARE(awaitReply(confirmWatcher).type(), QDBusMessage::ReplyMessage);

    QDBusMessage authorize = agentCall(serverBus, QStringLiteral("RequestAuthorization"));
    authorize << QVariant::fromValue(QDBusObjectPath(kDevicePath));
    QDBusPendingCallWatcher authorizeWatcher(client.asyncCall(authorize, 5000));
    QTRY_VERIFY(mgr.isPairingActive());
    QVERIFY(mgr.pairingRequiresConfirmation());
    QVERIFY(mgr.pairingPasskey().isEmpty());
    mgr.rejectPairing();
    const QDBusMessage authorizeReply = awaitReply(authorizeWatcher);
    QCOMPARE(authorizeReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(authorizeReply.errorName(), QStringLiteral("org.bluez.Error.Rejected"));

    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testOverlappingAgentPromptsCancelPreviousDecision()
{
    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    const QDBusConnection serverBus = QDBusConnection::sessionBus();
    oap::BluetoothManager mgr(&config, serverBus);
    auto objects = adapterSnapshot();
    addDevice(objects, false, false);
    objects[kSecondDevicePath][QStringLiteral("org.bluez.Device1")] = {
        {QStringLiteral("Address"), QStringLiteral("11:22:33:44:55:66")},
        {QStringLiteral("Name"), QStringLiteral("Moto")},
        {QStringLiteral("Paired"), false},
        {QStringLiteral("Connected"), false},
    };
    mgr.applyManagedObjectsSnapshot(objects);

    const QString clientName = QStringLiteral("oap-agent-overlap-client");
    const QDBusConnection client =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(client.isConnected());

    QDBusMessage firstConfirm =
        agentCall(serverBus, QStringLiteral("RequestConfirmation"));
    firstConfirm << QVariant::fromValue(QDBusObjectPath(kDevicePath))
                 << QVariant::fromValue(quint32(111222));
    QDBusPendingCallWatcher firstConfirmWatcher(
        client.asyncCall(firstConfirm, 5000));
    QTRY_COMPARE(mgr.pairingDeviceName(), QStringLiteral("Pixel"));

    QDBusMessage authorization =
        agentCall(serverBus, QStringLiteral("RequestAuthorization"));
    authorization << QVariant::fromValue(QDBusObjectPath(kSecondDevicePath));
    QDBusPendingCallWatcher authorizationWatcher(
        client.asyncCall(authorization, 5000));
    const QDBusMessage firstConfirmReply = awaitReply(firstConfirmWatcher);
    QCOMPARE(firstConfirmReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(firstConfirmReply.errorName(),
             QStringLiteral("org.bluez.Error.Canceled"));
    QTRY_COMPARE(mgr.pairingDeviceName(), QStringLiteral("Moto"));
    QVERIFY(mgr.pairingRequiresConfirmation());
    QVERIFY(mgr.pairingPasskey().isEmpty());
    mgr.confirmPairing();
    QCOMPARE(awaitReply(authorizationWatcher).type(), QDBusMessage::ReplyMessage);

    QDBusMessage secondConfirm =
        agentCall(serverBus, QStringLiteral("RequestConfirmation"));
    secondConfirm << QVariant::fromValue(QDBusObjectPath(kDevicePath))
                  << QVariant::fromValue(quint32(333444));
    QDBusPendingCallWatcher secondConfirmWatcher(
        client.asyncCall(secondConfirm, 5000));
    QTRY_COMPARE(mgr.pairingPasskey(), QStringLiteral("333444"));

    QDBusMessage display = agentCall(serverBus, QStringLiteral("DisplayPasskey"));
    display << QVariant::fromValue(QDBusObjectPath(kSecondDevicePath))
            << QVariant::fromValue(quint32(654321))
            << QVariant::fromValue(quint16(3));
    QDBusPendingCallWatcher displayWatcher(client.asyncCall(display, 5000));
    QCOMPARE(awaitReply(displayWatcher).type(), QDBusMessage::ReplyMessage);
    const QDBusMessage secondConfirmReply = awaitReply(secondConfirmWatcher);
    QCOMPARE(secondConfirmReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(secondConfirmReply.errorName(),
             QStringLiteral("org.bluez.Error.Canceled"));
    QVERIFY(mgr.isPairingActive());
    QVERIFY(!mgr.pairingRequiresConfirmation());
    QCOMPARE(mgr.pairingDeviceName(), QStringLiteral("Moto"));
    QCOMPARE(mgr.pairingPasskey(), QStringLiteral("654321"));
    QCOMPARE(mgr.pairingEntered(), 3);

    mgr.confirmPairing();
    QVERIFY(!mgr.isPairingActive());
    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testAgentReleaseCancelsDelayedConfirmation()
{
    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    const QDBusConnection serverBus = QDBusConnection::sessionBus();
    oap::BluetoothManager mgr(&config, serverBus);
    auto objects = adapterSnapshot();
    addDevice(objects, false, false);
    mgr.applyManagedObjectsSnapshot(objects);

    const QString clientName = QStringLiteral("oap-agent-release-client");
    const QDBusConnection client =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(client.isConnected());

    QDBusMessage confirm = agentCall(serverBus, QStringLiteral("RequestConfirmation"));
    confirm << QVariant::fromValue(QDBusObjectPath(kDevicePath))
            << QVariant::fromValue(quint32(111222));
    QDBusPendingCallWatcher confirmWatcher(client.asyncCall(confirm, 5000));
    QTRY_VERIFY(mgr.isPairingActive());

    QDBusPendingCallWatcher releaseWatcher(
        client.asyncCall(agentCall(serverBus, QStringLiteral("Release")), 5000));
    QCOMPARE(awaitReply(releaseWatcher).type(), QDBusMessage::ReplyMessage);
    const QDBusMessage confirmReply = awaitReply(confirmWatcher);
    QCOMPARE(confirmReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(confirmReply.errorName(), QStringLiteral("org.bluez.Error.Canceled"));
    QVERIFY(!mgr.isPairingActive());
    mgr.confirmPairing();
    QVERIFY(!mgr.isPairingActive());

    mgr.shutdown();
    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testShutdownCancelsDelayedConfirmation()
{
    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    const QDBusConnection serverBus = QDBusConnection::sessionBus();
    oap::BluetoothManager mgr(&config, serverBus);
    auto objects = adapterSnapshot();
    addDevice(objects, false, false);
    mgr.applyManagedObjectsSnapshot(objects);

    const QString clientName = QStringLiteral("oap-agent-shutdown-client");
    const QDBusConnection client =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(client.isConnected());

    QDBusMessage confirm = agentCall(serverBus, QStringLiteral("RequestConfirmation"));
    confirm << QVariant::fromValue(QDBusObjectPath(kDevicePath))
            << QVariant::fromValue(quint32(333444));
    QDBusPendingCallWatcher confirmWatcher(client.asyncCall(confirm, 5000));
    QTRY_VERIFY(mgr.isPairingActive());

    mgr.shutdown();
    const QDBusMessage confirmReply = awaitReply(confirmWatcher);
    QCOMPARE(confirmReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(confirmReply.errorName(), QStringLiteral("org.bluez.Error.Canceled"));
    QVERIFY(!mgr.isPairingActive());
    mgr.confirmPairing();
    QVERIFY(!mgr.isPairingActive());

    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testBluezLossCancelsDelayedConfirmation()
{
    QDBusConnection serverBus = QDBusConnection::sessionBus();
    QVERIFY(serverBus.isConnected());
    qDBusRegisterMetaType<oap::BluezInterfaceMap>();
    qDBusRegisterMetaType<TestBluezManagedObjectMap>();

    BluezObjectManagerFixture fixture;
    auto validObjects = adapterSnapshot();
    addDevice(validObjects, false, false);
    for (auto it = validObjects.cbegin(); it != validObjects.cend(); ++it)
        fixture.objects[QDBusObjectPath(it.key())] = it.value();
    QVERIFY(serverBus.registerObject(QStringLiteral("/"), &fixture,
                                     QDBusConnection::ExportAllSlots));
    QVERIFY(serverBus.registerService(QStringLiteral("org.bluez")));

    MockConfigService config;
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    oap::BluetoothManager mgr(&config, serverBus);
    mgr.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(fixture.replyCount, 1, 3000);

    const QString clientName = QStringLiteral("oap-agent-loss-client");
    const QDBusConnection client =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, clientName);
    QVERIFY(client.isConnected());
    QDBusMessage confirm = agentCall(serverBus, QStringLiteral("RequestConfirmation"));
    confirm << QVariant::fromValue(QDBusObjectPath(kDevicePath))
            << QVariant::fromValue(quint32(555666));
    QDBusPendingCallWatcher confirmWatcher(client.asyncCall(confirm, 5000));
    QTRY_VERIFY(mgr.isPairingActive());

    QVERIFY(serverBus.unregisterService(QStringLiteral("org.bluez")));
    const QDBusMessage confirmReply = awaitReply(confirmWatcher);
    QCOMPARE(confirmReply.type(), QDBusMessage::ErrorMessage);
    QCOMPARE(confirmReply.errorName(), QStringLiteral("org.bluez.Error.Canceled"));
    QVERIFY(!mgr.isPairingActive());
    mgr.confirmPairing();
    QVERIFY(!mgr.isPairingActive());

    mgr.shutdown();
    serverBus.unregisterObject(QStringLiteral("/"));
    QDBusConnection::disconnectFromBus(clientName);
}

void TestBluetoothManager::testPairableToggle()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config, QDBusConnection::sessionBus());
    QSignalSpy spy(&mgr, &oap::BluetoothManager::pairableChanged);

    mgr.setPairable(true);
    QVERIFY(mgr.isPairable());
    QCOMPARE(spy.count(), 1);
    mgr.setPairable(true);
    QCOMPARE(spy.count(), 1);
    mgr.setPairable(false);
    QVERIFY(!mgr.isPairable());
    QCOMPARE(spy.count(), 2);
}

void TestBluetoothManager::testAutoConnectLifecycle()
{
    MockConfigService config;
    oap::BluetoothManager mgr(&config, QDBusConnection::sessionBus());

    mgr.startAutoConnect();
    mgr.cancelAutoConnect();
    mgr.cancelAutoConnect();
    config.values_[QStringLiteral("connection.auto_connect_aa")] = false;
    mgr.startAutoConnect();
}

QTEST_MAIN(TestBluetoothManager)
#include "test_bluetooth_manager.moc"
