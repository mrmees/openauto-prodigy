#include <QtTest>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QSignalSpy>

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

namespace {

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
    void testPairableToggle();
    void testAutoConnectLifecycle();
};

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
    for (const QString& method : {QStringLiteral("RequestPinCode"),
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
