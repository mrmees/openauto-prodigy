// Bus-independent safety tests. Protocol conformance is live-check territory
// (design doc §11) — do NOT fake a session bus here.
#include <QtTest/QtTest>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include "core/services/PhoneStateService.hpp"
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

bool changeCallProperties(oap::TelephonyClient& client, const QString& path,
                          const QVariantMap& changed, const QStringList& invalidated = {})
{
    const QDBusMessage message = QDBusMessage::createSignal(
        path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    return QMetaObject::invokeMethod(
        &client, "onPropertiesChanged", Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("org.pipewire.Telephony.Call1")),
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
    void testRemainingAgSelectedAfterRemoval();
    void testCachedCallAdoptedOnGatewayFailover();
    void testFailoverReplacementCancelsSettleAndRefreshesMetadata();
    void testFailoverAdoptsCachedCallBeforeActiveTransport();
    void testFailoverActiveTransportWithoutCallStaysIdle();
    void testFailoverClearsActiveScoEvidence();
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

void TestTelephonyClient::testRemainingAgSelectedAfterRemoval()
{
    oap::TelephonyClient client;
    const QString ag1 = QStringLiteral("/org/pipewire/Telephony/ag1");
    const QString ag2 = QStringLiteral("/org/pipewire/Telephony/ag2");
    const QString agIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
    const QString transportIface =
        QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1");
    QSignalSpy availableSpy(&client, &oap::TelephonyClient::availableChanged);
    QSignalSpy removedSpy(&client, &oap::TelephonyClient::transportRemoved);

    QVERIFY(addInterfaces(client, ag1, {
        {agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}}},
        {transportIface, {{QStringLiteral("State"), QStringLiteral("active")}}},
    }));
    QVERIFY(addInterfaces(client, ag2, {
        {agIface, {{QStringLiteral("Address"), QStringLiteral("CC:DD")}}},
        {transportIface, {{QStringLiteral("State"), QStringLiteral("idle")}}},
    }));
    QCOMPARE(client.agAddress(), QStringLiteral("AA:BB"));
    QCOMPARE(availableSpy.count(), 0); // serviceUp is false on this busless seam

    QVERIFY(removeInterfaces(client, ag1, {agIface}));
    // The busless seam keeps serviceUp false; selected identity and transport
    // prove failover without fabricating service ownership.
    QCOMPARE(client.agAddress(), QStringLiteral("CC:DD"));
    QCOMPARE(client.transportState(), QStringLiteral("idle"));
    QCOMPARE(removedSpy.count(), 1);
    // Availability remained true across the deterministic hand-off.
    QCOMPARE(availableSpy.count(), 0);
}

void TestTelephonyClient::testCachedCallAdoptedOnGatewayFailover()
{
    oap::TelephonyClient client;
    const QString ag1 = QStringLiteral("/org/pipewire/Telephony/ag1");
    const QString ag2 = QStringLiteral("/org/pipewire/Telephony/ag2");
    const QString ag1Call = ag1 + QStringLiteral("/call1");
    const QString ag2Call = ag2 + QStringLiteral("/call1");
    const QString agIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
    const QString transportIface =
        QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1");
    const QString callIface = QStringLiteral("org.pipewire.Telephony.Call1");

    QVERIFY(addInterfaces(client, ag1, {
        {agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}}},
        {transportIface, {{QStringLiteral("State"), QStringLiteral("active")}}},
    }));
    QVERIFY(addInterfaces(client, ag2, {
        {agIface, {{QStringLiteral("Address"), QStringLiteral("CC:DD")}}},
        {transportIface, {{QStringLiteral("State"), QStringLiteral("idle")}}},
    }));

    QSignalSpy startedSpy(&client, &oap::TelephonyClient::callSetupStarted);
    QSignalSpy changedSpy(&client, &oap::TelephonyClient::callSetupChanged);
    QSignalSpy endedSpy(&client, &oap::TelephonyClient::callSetupEnded);

    QVERIFY(addInterfaces(client, ag1Call, {{callIface, {
        {QStringLiteral("State"), QStringLiteral("dialing")},
        {QStringLiteral("LineIdentification"), QStringLiteral("111")},
        {QStringLiteral("Name"), QStringLiteral("Primary")},
    }}}));
    QCOMPARE(startedSpy.count(), 1);

    // AG2's child exists and changes before AG2 is selected. Its lifecycle and
    // properties are cached, but no non-selected call is published.
    QVERIFY(addInterfaces(client, ag2Call, {{callIface, {
        {QStringLiteral("State"), QStringLiteral("incoming")},
        {QStringLiteral("LineIdentification"), QStringLiteral("old-number")},
        {QStringLiteral("Name"), QStringLiteral("Moto")},
    }}}));
    QVERIFY(changeCallProperties(client, ag2Call, {
        {QStringLiteral("State"), QStringLiteral("alerting")},
        {QStringLiteral("LineIdentification"), QStringLiteral("222")},
    }));
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(endedSpy.count(), 0);

    // Removing selected AG1 ends its published call, deterministically selects
    // AG2, and adopts AG2's already-existing child with its latest properties.
    QVERIFY(removeInterfaces(client, ag1, {agIface}));
    QCOMPARE(client.agAddress(), QStringLiteral("CC:DD"));
    QCOMPARE(client.transportState(), QStringLiteral("idle"));
    QCOMPARE(endedSpy.count(), 1);
    QCOMPARE(startedSpy.count(), 2);
    QCOMPARE(startedSpy[1][0].toString(), QStringLiteral("alerting"));
    QCOMPARE(startedSpy[1][1].toString(), QStringLiteral("222"));
    QCOMPARE(startedSpy[1][2].toString(), QStringLiteral("Moto"));

    // Selected-call deltas remain edge-only. Property invalidation updates the
    // cache but is not an object-removal edge and therefore cannot end a call.
    QVERIFY(changeCallProperties(client, ag2Call,
                                 {{QStringLiteral("State"), QStringLiteral("active")}}));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedSpy[0][0].toString(), QStringLiteral("active"));
    QVERIFY(changeCallProperties(client, ag2Call,
                                 {{QStringLiteral("State"), QStringLiteral("active")}}));
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(changeCallProperties(client, ag2Call, {}, {QStringLiteral("State")}));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(endedSpy.count(), 1);
    QVERIFY(changeCallProperties(client, ag2Call,
                                 {{QStringLiteral("State"), QStringLiteral("active")}}));
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(changeCallProperties(client, ag2Call,
                                 {{QStringLiteral("State"), QStringLiteral("held")}}));
    QCOMPARE(changedSpy.count(), 2);

    QVERIFY(removeInterfaces(client, ag2Call, {callIface}));
    QCOMPARE(endedSpy.count(), 2);
    QVERIFY(changeCallProperties(client, ag2Call,
                                 {{QStringLiteral("State"), QStringLiteral("active")}}));
    QCOMPARE(changedSpy.count(), 2);

    // AG1's cached child was purged with AG1. Re-adding AG1 and failing over to
    // it later must not resurrect that stale call.
    QVERIFY(addInterfaces(client, ag1,
                          {{agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}}}}));
    QVERIFY(removeInterfaces(client, ag2, {agIface}));
    QCOMPARE(client.agAddress(), QStringLiteral("AA:BB"));
    QCOMPARE(startedSpy.count(), 2);
    QCOMPARE(endedSpy.count(), 2);
}

void TestTelephonyClient::testFailoverReplacementCancelsSettleAndRefreshesMetadata()
{
    oap::TelephonyClient client;
    oap::PhoneStateService phone;
    phone.setSettleGraceMs(30);
    phone.attachTelephony(&client);

    const QString ag1 = QStringLiteral("/org/pipewire/Telephony/ag1");
    const QString ag2 = QStringLiteral("/org/pipewire/Telephony/ag2");
    const QString agIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
    const QString callIface = QStringLiteral("org.pipewire.Telephony.Call1");
    QVERIFY(addInterfaces(client, ag1,
                          {{agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}}}}));
    QVERIFY(addInterfaces(client, ag2,
                          {{agIface, {{QStringLiteral("Address"), QStringLiteral("CC:DD")}}}}));

    QSignalSpy stateSpy(&phone, &oap::ICallStateProvider::callStateChanged);
    QVERIFY(addInterfaces(client, ag1 + QStringLiteral("/call1"), {{callIface, {
        {QStringLiteral("State"), QStringLiteral("incoming")},
        {QStringLiteral("LineIdentification"), QStringLiteral("111")},
        {QStringLiteral("Name"), QStringLiteral("Primary")},
    }}}));
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));
    QCOMPARE(phone.callerNumber(), QStringLiteral("111"));
    QCOMPARE(stateSpy.count(), 1);

    // The replacement call is cached silently while AG1 remains selected.
    QVERIFY(addInterfaces(client, ag2 + QStringLiteral("/call1"), {{callIface, {
        {QStringLiteral("State"), QStringLiteral("incoming")},
        {QStringLiteral("LineIdentification"), QStringLiteral("222")},
        {QStringLiteral("Name"), QStringLiteral("Replacement")},
    }}}));
    QCOMPARE(stateSpy.count(), 1);

    // AG1 removal publishes an authoritative Idle boundary, then adopts AG2's
    // call. The accepted replacement must also leave no AG1 settle timeout.
    QVERIFY(removeInterfaces(client, ag1, {agIface}));
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));
    QCOMPARE(phone.callerNumber(), QStringLiteral("222"));
    QCOMPARE(phone.callerName(), QStringLiteral("Replacement"));
    QCOMPARE(stateSpy.count(), 3);
    QTest::qWait(100);
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));
    QCOMPARE(phone.callerNumber(), QStringLiteral("222"));

    // Ordinary explicit setup removal still uses the settle window and clears
    // a rejected/unanswered call when no SCO or active transport arrives.
    QVERIFY(removeInterfaces(client, ag2 + QStringLiteral("/call1"), {callIface}));
    QTRY_COMPARE_WITH_TIMEOUT(
        phone.callState(), static_cast<int>(oap::ICallStateProvider::Idle), 500);
    QVERIFY(phone.callerNumber().isEmpty());
}

void TestTelephonyClient::testFailoverAdoptsCachedCallBeforeActiveTransport()
{
    oap::TelephonyClient client;
    oap::PhoneStateService phone;
    phone.setSettleGraceMs(30);
    phone.attachTelephony(&client);

    const QString ag1 = QStringLiteral("/org/pipewire/Telephony/ag1");
    const QString ag2 = QStringLiteral("/org/pipewire/Telephony/ag2");
    const QString agIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
    const QString transportIface =
        QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1");
    const QString callIface = QStringLiteral("org.pipewire.Telephony.Call1");
    QVERIFY(addInterfaces(client, ag1,
                          {{agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}}}}));
    QVERIFY(addInterfaces(client, ag2, {
        {agIface, {{QStringLiteral("Address"), QStringLiteral("CC:DD")}}},
        {transportIface, {{QStringLiteral("State"), QStringLiteral("active")}}},
    }));

    QList<int> observedStates;
    connect(&phone, &oap::ICallStateProvider::callStateChanged, &phone,
            [&]() { observedStates.append(phone.callState()); });
    QVERIFY(addInterfaces(client, ag1 + QStringLiteral("/call1"), {{callIface, {
        {QStringLiteral("State"), QStringLiteral("incoming")},
        {QStringLiteral("LineIdentification"), QStringLiteral("111")},
        {QStringLiteral("Name"), QStringLiteral("Primary")},
    }}}));
    QVERIFY(addInterfaces(client, ag2 + QStringLiteral("/call1"), {{callIface, {
        {QStringLiteral("State"), QStringLiteral("incoming")},
        {QStringLiteral("LineIdentification"), QStringLiteral("222")},
        {QStringLiteral("Name"), QStringLiteral("Replacement")},
    }}}));

    QVERIFY(removeInterfaces(client, ag1, {agIface}));
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));
    QCOMPARE(phone.callerNumber(), QStringLiteral("222"));
    QCOMPARE(phone.callerName(), QStringLiteral("Replacement"));
    QCOMPARE(client.transportState(), QStringLiteral("active"));
    QVERIFY(!observedStates.contains(static_cast<int>(oap::ICallStateProvider::Active)));
    QTest::qWait(100);
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));
}

void TestTelephonyClient::testFailoverActiveTransportWithoutCallStaysIdle()
{
    oap::TelephonyClient client;
    oap::PhoneStateService phone;
    phone.setSettleGraceMs(30);
    phone.attachTelephony(&client);

    const QString ag1 = QStringLiteral("/org/pipewire/Telephony/ag1");
    const QString ag2 = QStringLiteral("/org/pipewire/Telephony/ag2");
    const QString agIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
    const QString transportIface =
        QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1");
    const QString callIface = QStringLiteral("org.pipewire.Telephony.Call1");
    QVERIFY(addInterfaces(client, ag1,
                          {{agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}}}}));
    QVERIFY(addInterfaces(client, ag2, {
        {agIface, {{QStringLiteral("Address"), QStringLiteral("CC:DD")}}},
        {transportIface, {{QStringLiteral("State"), QStringLiteral("active")}}},
    }));

    QList<int> observedStates;
    connect(&phone, &oap::ICallStateProvider::callStateChanged, &phone,
            [&]() { observedStates.append(phone.callState()); });
    QVERIFY(addInterfaces(client, ag1 + QStringLiteral("/call1"), {{callIface, {
        {QStringLiteral("State"), QStringLiteral("incoming")},
        {QStringLiteral("LineIdentification"), QStringLiteral("111")},
        {QStringLiteral("Name"), QStringLiteral("Primary")},
    }}}));

    QVERIFY(removeInterfaces(client, ag1, {agIface}));
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
    QVERIFY(phone.callerNumber().isEmpty());
    QVERIFY(phone.callerName().isEmpty());
    QCOMPARE(client.transportState(), QStringLiteral("active"));
    QVERIFY(!observedStates.contains(static_cast<int>(oap::ICallStateProvider::Active)));
    QTest::qWait(100);
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
}

void TestTelephonyClient::testFailoverClearsActiveScoEvidence()
{
    oap::TelephonyClient client;
    oap::PhoneStateService phone;
    phone.setSettleGraceMs(30);
    phone.attachTelephony(&client);

    const QString ag1 = QStringLiteral("/org/pipewire/Telephony/ag1");
    const QString ag2 = QStringLiteral("/org/pipewire/Telephony/ag2");
    const QString ag1Call = ag1 + QStringLiteral("/call1");
    const QString ag2Call = ag2 + QStringLiteral("/call1");
    const QString agIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
    const QString transportIface =
        QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1");
    const QString callIface = QStringLiteral("org.pipewire.Telephony.Call1");
    QVERIFY(addInterfaces(client, ag1,
                          {{agIface, {{QStringLiteral("Address"), QStringLiteral("AA:BB")}}}}));
    QVERIFY(addInterfaces(client, ag2, {
        {agIface, {{QStringLiteral("Address"), QStringLiteral("CC:DD")}}},
        {transportIface, {{QStringLiteral("State"), QStringLiteral("active")}}},
    }));
    QVERIFY(addInterfaces(client, ag1Call, {{callIface, {
        {QStringLiteral("State"), QStringLiteral("incoming")},
        {QStringLiteral("LineIdentification"), QStringLiteral("111")},
        {QStringLiteral("Name"), QStringLiteral("Primary")},
    }}}));
    QVERIFY(changeCallProperties(client, ag1Call,
                                 {{QStringLiteral("State"), QStringLiteral("active")}}));
    phone.onScoRunningChanged(true);
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Active));

    QVERIFY(removeInterfaces(client, ag1, {agIface}));
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
    QVERIFY(phone.callerNumber().isEmpty());
    QVERIFY(phone.callerName().isEmpty());

    // A replacement setup must not inherit AG1's SCO=true observation. Its
    // removal therefore settles to Idle instead of immediately becoming Active.
    QVERIFY(addInterfaces(client, ag2Call, {{callIface, {
        {QStringLiteral("State"), QStringLiteral("incoming")},
        {QStringLiteral("LineIdentification"), QStringLiteral("222")},
    }}}));
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));
    QVERIFY(removeInterfaces(client, ag2Call, {callIface}));
    QCOMPARE(phone.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));
    QTRY_COMPARE_WITH_TIMEOUT(
        phone.callState(), static_cast<int>(oap::ICallStateProvider::Idle), 500);
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
