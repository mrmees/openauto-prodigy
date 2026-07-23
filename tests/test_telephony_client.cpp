// Bus-independent safety tests. Protocol conformance is live-check territory
// (design doc §11) — do NOT fake a session bus here.
#include <QtTest/QtTest>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include "core/services/TelephonyClient.hpp"

namespace {

bool addInterfaces(oap::TelephonyClient& client, const QString& path,
                   const oap::InterfaceMap& interfaces)
{
    return QMetaObject::invokeMethod(
        &client, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(path)),
        Q_ARG(oap::InterfaceMap, interfaces));
}

bool removeInterfaces(oap::TelephonyClient& client, const QString& path,
                      const QStringList& interfaces)
{
    return QMetaObject::invokeMethod(
        &client, "onInterfacesRemoved", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(path)),
        Q_ARG(QStringList, interfaces));
}

bool changeProperties(oap::TelephonyClient& client, const QString& path,
                      const QVariantMap& changed, const QStringList& invalidated = {})
{
    const QDBusMessage message = QDBusMessage::createSignal(
        path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    return QMetaObject::invokeMethod(
        &client, "onPropertiesChanged", Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1")),
        Q_ARG(QVariantMap, changed), Q_ARG(QStringList, invalidated),
        Q_ARG(QDBusMessage, message));
}

} // namespace

class TestTelephonyClient : public QObject {
    Q_OBJECT
private slots:
    void testConstructIsInert();
    void testStartStopSafeWithoutService();
    void testCommandsSafeWhenUnavailable();
    void testRejectScoCachedWhenUnavailable();
    void testSelectedAgOwnsOnlySameObjectTransport();
    void testSelectedAgOwnsOnlyChildCalls();
};

void TestTelephonyClient::testConstructIsInert() {
    oap::TelephonyClient c;               // must not touch any bus
    QVERIFY(!c.available());
    QVERIFY(c.agAddress().isEmpty());
    QVERIFY(c.transportState().isEmpty());
    QVERIFY(c.codec().isEmpty());
}

void TestTelephonyClient::testSelectedAgOwnsOnlyChildCalls()
{
    oap::TelephonyClient client;
    const QString ag1 = QStringLiteral("/org/pipewire/Telephony/ag1");
    const QString ag2 = QStringLiteral("/org/pipewire/Telephony/ag2");
    const QString agIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
    const QString callIface = QStringLiteral("org.pipewire.Telephony.Call1");
    QVERIFY(addInterfaces(client, ag1,
                          {{agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}}}}));

    QSignalSpy startedSpy(&client, &oap::TelephonyClient::callSetupStarted);
    QSignalSpy endedSpy(&client, &oap::TelephonyClient::callSetupEnded);
    const QString selectedCall = ag1 + QStringLiteral("/call1");
    const QString foreignCall = ag2 + QStringLiteral("/call1");
    const QVariantMap callProps{{QStringLiteral("State"), QStringLiteral("incoming")}};

    QVERIFY(addInterfaces(client, foreignCall, {{callIface, callProps}}));
    QCOMPARE(startedSpy.count(), 0);
    QVERIFY(addInterfaces(client, selectedCall, {{callIface, callProps}}));
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(removeInterfaces(client, foreignCall, {callIface}));
    QCOMPARE(endedSpy.count(), 0);
    QVERIFY(removeInterfaces(client, selectedCall, {callIface}));
    QCOMPARE(endedSpy.count(), 1);
}

void TestTelephonyClient::testStartStopSafeWithoutService() {
    oap::TelephonyClient c;
    c.start();                            // session bus may or may not exist in CI
    c.start();                            // idempotent
    QVERIFY(!c.available());              // org.pipewire.Telephony not running here
    c.stop();
    c.stop();
    QVERIFY(!c.available());
}

void TestTelephonyClient::testCommandsSafeWhenUnavailable() {
    oap::TelephonyClient c;
    QSignalSpy failSpy(&c, &oap::TelephonyClient::commandFailed);
    c.dial("5551234");
    c.answer();
    c.hangupAll();
    c.sendTones("1");
    QCOMPARE(failSpy.count(), 4);         // each op fails fast, no crash
}

void TestTelephonyClient::testRejectScoCachedWhenUnavailable() {
    oap::TelephonyClient c;
    c.setRejectSco(true);                 // no transport — must only cache
    QVERIFY(!c.available());
}

void TestTelephonyClient::testSelectedAgOwnsOnlySameObjectTransport()
{
    oap::TelephonyClient client;
    QSignalSpy stateSpy(&client, &oap::TelephonyClient::transportStateChanged);
    QSignalSpy codecSpy(&client, &oap::TelephonyClient::codecChanged);
    const QString ag1 = QStringLiteral("/org/pipewire/Telephony/ag1");
    const QString ag2 = QStringLiteral("/org/pipewire/Telephony/ag2");
    const QString agIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
    const QString transportIface =
        QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1");

    oap::InterfaceMap selected;
    selected.insert(agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}});
    selected.insert(transportIface, {
        {QStringLiteral("State"), QStringLiteral("active")},
        {QStringLiteral("Codec"), QVariant::fromValue(quint8(2))},
    });
    QVERIFY(addInterfaces(client, ag1, selected));
    QCOMPARE(client.agAddress(), QStringLiteral("AA:BB"));
    QCOMPARE(client.transportState(), QStringLiteral("active"));
    QCOMPARE(client.codec(), QStringLiteral("mSBC"));

    const int selectedStateEdges = stateSpy.count();
    const int selectedCodecEdges = codecSpy.count();
    oap::InterfaceMap foreign;
    foreign.insert(agIface, {{QStringLiteral("Address"), QStringLiteral("CC:DD")}});
    foreign.insert(transportIface, {
        {QStringLiteral("State"), QStringLiteral("idle")},
        {QStringLiteral("Codec"), QVariant::fromValue(quint8(1))},
    });
    QVERIFY(addInterfaces(client, ag2, foreign));
    QCOMPARE(client.agAddress(), QStringLiteral("AA:BB"));
    QCOMPARE(client.transportState(), QStringLiteral("active"));
    QCOMPARE(client.codec(), QStringLiteral("mSBC"));
    QCOMPARE(stateSpy.count(), selectedStateEdges);
    QCOMPARE(codecSpy.count(), selectedCodecEdges);

    QVERIFY(changeProperties(client, ag2, {
        {QStringLiteral("State"), QStringLiteral("idle")},
        {QStringLiteral("Codec"), QVariant::fromValue(quint8(1))},
    }));
    QVERIFY(removeInterfaces(client, ag2, {transportIface}));
    QCOMPARE(client.transportState(), QStringLiteral("active"));
    QCOMPARE(client.codec(), QStringLiteral("mSBC"));
    QCOMPARE(stateSpy.count(), selectedStateEdges);
    QCOMPARE(codecSpy.count(), selectedCodecEdges);

    // Same-value delivery is edge-only; invalidation returns the observable
    // selected transport properties to unknown exactly once.
    QVERIFY(changeProperties(client, ag1,
                             {{QStringLiteral("State"), QStringLiteral("active")}}));
    QCOMPARE(stateSpy.count(), selectedStateEdges);
    QVERIFY(changeProperties(client, ag1, {}, {QStringLiteral("State")}));
    QVERIFY(client.transportState().isEmpty());
    QCOMPARE(stateSpy.count(), selectedStateEdges + 1);
    QVERIFY(changeProperties(client, ag1, {}, {QStringLiteral("Codec")}));
    QVERIFY(client.codec().isEmpty());
    QCOMPARE(codecSpy.count(), selectedCodecEdges + 1);

    // Re-adopt state, then removal clears only the selected transport.
    QVERIFY(changeProperties(client, ag1, {
        {QStringLiteral("State"), QStringLiteral("active")},
        {QStringLiteral("Codec"), QVariant::fromValue(quint8(2))},
    }));
    const int beforeRemovalState = stateSpy.count();
    const int beforeRemovalCodec = codecSpy.count();
    QVERIFY(removeInterfaces(client, ag1, {transportIface}));
    QVERIFY(client.transportState().isEmpty());
    QVERIFY(client.codec().isEmpty());
    QCOMPARE(stateSpy.count(), beforeRemovalState + 1);
    QCOMPARE(codecSpy.count(), beforeRemovalCodec + 1);
}

QTEST_MAIN(TestTelephonyClient)
#include "test_telephony_client.moc"
