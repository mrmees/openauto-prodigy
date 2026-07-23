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

const QString kAdapterPath = QStringLiteral("/org/bluez/hci0");
const QString kDevicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");

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
    void testAgentMethodSurfaceAndPromptModes();
    void testAsyncRefreshCoalescesAndAppliesFinalSnapshot();
    void testBluezLossDiscardsOldManagedObjectsReply();
    void testMalformedManagedObjectsReplyRetainsState();
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
    QCOMPARE(connectedSpy.count(), 1);
    QVERIFY(heartbeats > 0);
    QCOMPARE(fixture.callCount, 2);

    mgr.shutdown();
    const QString connectedAfterShutdown = mgr.connectedDeviceName();
    QVERIFY(bus.unregisterService(QStringLiteral("org.bluez")));
    QTest::qWait(50);
    QCOMPARE(mgr.connectedDeviceName(), connectedAfterShutdown);
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
