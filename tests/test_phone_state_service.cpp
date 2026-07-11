// tests/test_phone_state_service.cpp
#include <QtTest/QtTest>
#include "core/services/PhoneStateService.hpp"
#include "core/services/ICallStateProvider.hpp"

class TestPhoneStateService : public QObject {
    Q_OBJECT
private slots:
    void testImplementsICallStateProvider();
    void testInitialStateIsIdle();
    void testCallStateTransitions();
    void testAnswerOnlyFromRinging();
    void testHangupFromActive();
    void testHangupFromRinging();
    void testCallerInfoPreservedDuringCall();
    void testSignalEmittedOnStateChange();

    // --- D2 state machine (design doc §5 table) ---
    void testTelephonyAvailability();
    void testIncomingAnsweredCleanPath();
    void testIncomingAnsweredViaScoSettle();
    void testIncomingRejectedSettleTimeout();
    void testOutgoingFullPath();
    void testActiveEndsOnScoDropDebounced();
    void testActiveSurvivesScoBlip();
    void testActiveEndsOnTransportIdle();
    void testRecoveryScoRunningFromIdle();
    void testCallWaitingIgnoredWhileActive();
    void testAgVanishResetsToIdle();
    void testDialGuards();
    void testSetupDisconnectedEndsImmediately();

    // --- HFP hot-plug: registered map slot + adoptBluezDevice seam ---
    void adoptBluezDevice_connectedHfpPhone_adopts();
    void adoptBluezDevice_nonHfp_ignored();
    void interfacesAdded_payloadConsumed_noBusReadback();
};

void TestPhoneStateService::testImplementsICallStateProvider() {
    oap::PhoneStateService service;
    // Should be castable to ICallStateProvider
    oap::ICallStateProvider* provider = &service;
    QVERIFY(provider != nullptr);
    QCOMPARE(provider->callState(), static_cast<int>(oap::ICallStateProvider::Idle));
}

void TestPhoneStateService::testInitialStateIsIdle() {
    oap::PhoneStateService service;
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
    QVERIFY(service.callerName().isEmpty());
    QVERIFY(service.callerNumber().isEmpty());
}

void TestPhoneStateService::testCallStateTransitions() {
    oap::PhoneStateService service;

    // Simulate incoming call
    service.setIncomingCall("555-1234", "John Doe");
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));
    QCOMPARE(service.callerNumber(), "555-1234");
    QCOMPARE(service.callerName(), "John Doe");

    // Answer
    service.answer();
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Active));

    // Hangup
    service.hangup();
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
}

void TestPhoneStateService::testAnswerOnlyFromRinging() {
    oap::PhoneStateService service;

    // Answer when idle does nothing
    service.answer();
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
}

void TestPhoneStateService::testHangupFromActive() {
    oap::PhoneStateService service;
    service.setIncomingCall("555-0000", "");
    service.answer();
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Active));

    service.hangup();
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
}

void TestPhoneStateService::testHangupFromRinging() {
    oap::PhoneStateService service;
    service.setIncomingCall("555-0000", "");
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Ringing));

    service.hangup();
    QCOMPARE(service.callState(), static_cast<int>(oap::ICallStateProvider::Idle));
}

void TestPhoneStateService::testCallerInfoPreservedDuringCall() {
    oap::PhoneStateService service;
    service.setIncomingCall("555-1234", "Jane");

    // Info preserved after answer
    service.answer();
    QCOMPARE(service.callerName(), "Jane");
    QCOMPARE(service.callerNumber(), "555-1234");
}

void TestPhoneStateService::testSignalEmittedOnStateChange() {
    oap::PhoneStateService service;
    QSignalSpy spy(&service, &oap::ICallStateProvider::callStateChanged);

    service.setIncomingCall("555-0000", "Test");
    QCOMPARE(spy.count(), 1);

    service.answer();
    QCOMPARE(spy.count(), 2);

    service.hangup();
    QCOMPARE(spy.count(), 3);
}

// --- D2 state machine tests (design doc §5 table) ---

using CS = oap::ICallStateProvider;

static oap::PhoneStateService* makeFastService(QObject* parent = nullptr) {
    auto* s = new oap::PhoneStateService(parent);
    s->setSettleGraceMs(50);
    s->setScoDebounceMs(50);
    s->onTelephonyAvailable(true);
    return s;
}

void TestPhoneStateService::testTelephonyAvailability() {
    oap::PhoneStateService s;
    QVERIFY(!s.telephonyAvailable());
    QSignalSpy spy(&s, &oap::IPhoneStateService::telephonyAvailableChanged);
    s.onTelephonyAvailable(true);
    QVERIFY(s.telephonyAvailable());
    QCOMPARE(spy.count(), 1);
}

void TestPhoneStateService::testIncomingAnsweredCleanPath() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+15125551212", "Test Caller");
    QCOMPARE(s->callState(), (int)CS::Ringing);
    QCOMPARE(s->callerNumber(), QString("+15125551212"));
    QCOMPARE(s->callerName(), QString("Test Caller"));
    s->onCallSetupChanged("active");          // Call1 State → active before removal
    QCOMPARE(s->callState(), (int)CS::Active);
    s->onCallSetupEnded();                     // ephemeral object destroyed — stays Active
    QCOMPARE(s->callState(), (int)CS::Active);
}

void TestPhoneStateService::testIncomingAnsweredViaScoSettle() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+15125551212", "");
    s->onCallSetupEnded();                     // no State→active seen: settling
    QCOMPARE(s->callState(), (int)CS::Ringing); // keeps reporting prior state
    s->onScoRunningChanged(true);
    QCOMPARE(s->callState(), (int)CS::Active);
}

void TestPhoneStateService::testIncomingRejectedSettleTimeout() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+15125551212", "");
    s->onCallSetupEnded();
    QTRY_COMPARE_WITH_TIMEOUT(s->callState(), (int)CS::Idle, 500); // grace 50ms expires
    QVERIFY(s->callerNumber().isEmpty());
}

void TestPhoneStateService::testOutgoingFullPath() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("dialing", "+15125551212", "");
    QCOMPARE(s->callState(), (int)CS::Dialing);
    s->onCallSetupChanged("alerting");
    QCOMPARE(s->callState(), (int)CS::Alerting);
    s->onCallSetupChanged("active");
    QCOMPARE(s->callState(), (int)CS::Active);
    s->onCallSetupEnded();
    QCOMPARE(s->callState(), (int)CS::Active);
    s->onScoRunningChanged(true);
    s->onScoRunningChanged(false);            // hangup: SCO drops
    QTRY_COMPARE_WITH_TIMEOUT(s->callState(), (int)CS::Idle, 500);
}

void TestPhoneStateService::testActiveEndsOnScoDropDebounced() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "1", "");
    s->onCallSetupEnded();
    s->onScoRunningChanged(true);
    QCOMPARE(s->callState(), (int)CS::Active);
    s->onScoRunningChanged(false);
    QCOMPARE(s->callState(), (int)CS::Active); // debounce window — not yet
    QTRY_COMPARE_WITH_TIMEOUT(s->callState(), (int)CS::Idle, 500);
}

void TestPhoneStateService::testActiveSurvivesScoBlip() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "1", "");
    s->onCallSetupEnded();
    s->onScoRunningChanged(true);
    s->onScoRunningChanged(false);
    s->onScoRunningChanged(true);              // back within debounce
    QTest::qWait(150);
    QCOMPARE(s->callState(), (int)CS::Active); // blip did not end the call
}

void TestPhoneStateService::testActiveEndsOnTransportIdle() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "1", "");
    s->onCallSetupChanged("active");
    // SCO never observed running (monitor inert): transport idle ends it
    s->onTransportStateChanged("idle");
    QCOMPARE(s->callState(), (int)CS::Idle);
}

void TestPhoneStateService::testRecoveryScoRunningFromIdle() {
    QObject root; auto* s = makeFastService(&root);
    QCOMPARE(s->callState(), (int)CS::Idle);
    s->onScoRunningChanged(true);              // restarted mid-call / audio routed back
    QCOMPARE(s->callState(), (int)CS::Active);
}

void TestPhoneStateService::testCallWaitingIgnoredWhileActive() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+1111", "First");
    s->onCallSetupChanged("active");
    s->onCallSetupStarted("incoming", "+2222", "Second"); // call-waiting: ignored in v1
    QCOMPARE(s->callState(), (int)CS::Active);
    QCOMPARE(s->callerNumber(), QString("+1111"));
    s->onCallSetupEnded();                     // waiting call resolved — no transition
    QCOMPARE(s->callState(), (int)CS::Active);
}

void TestPhoneStateService::testAgVanishResetsToIdle() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "1", "");
    s->onCallSetupChanged("active");
    s->onTelephonyAvailable(false);
    QCOMPARE(s->callState(), (int)CS::Idle);
    QVERIFY(!s->telephonyAvailable());
}

void TestPhoneStateService::testDialGuards() {
    QObject root; auto* s = makeFastService(&root);
    // telephony marked available but no TelephonyClient attached →
    // mock-mode dial: transitions locally so the dev VM UI still works
    QVERIFY(s->dial("5551234"));
    QCOMPARE(s->callState(), (int)CS::Dialing);
    QVERIFY(!s->dial("5551234"));              // not Idle → rejected
    QVERIFY(!s->sendDtmf("1"));                // not Active → rejected
}

void TestPhoneStateService::testSetupDisconnectedEndsImmediately() {
    // Live-verified (L2, Pixel 8): a rejected/failed call emits Call1
    // State→"disconnected" before InterfacesRemoved — end immediately,
    // no Settling wait.
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+15125551212", "");
    QCOMPARE(s->callState(), (int)CS::Ringing);
    s->onCallSetupChanged("disconnected");
    QCOMPARE(s->callState(), (int)CS::Idle);   // immediate, not via grace timeout
    QVERIFY(s->callerNumber().isEmpty());
    s->onCallSetupEnded();                      // subsequent removal: no-op
    QCOMPARE(s->callState(), (int)CS::Idle);
}

// --- HFP hot-plug: registered map slot + adoptBluezDevice seam ---

void TestPhoneStateService::adoptBluezDevice_connectedHfpPhone_adopts()
{
    oap::PhoneStateService svc;
    QSignalSpy spy(&svc, &oap::PhoneStateService::connectionChanged);
    QVariantMap props{
        {QStringLiteral("Connected"), true},
        {QStringLiteral("Alias"), QStringLiteral("Pixel 8")},
        {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000111e-0000-1000-8000-00805f9b34fb")}},
    };
    svc.adoptBluezDevice(QStringLiteral("/org/bluez/hci0/dev_AA_BB"), props);
    QCOMPARE(spy.count(), 1);
    QVERIFY(svc.phoneConnected());
    QCOMPARE(svc.deviceName(), QStringLiteral("Pixel 8"));
}

void TestPhoneStateService::adoptBluezDevice_nonHfp_ignored()
{
    oap::PhoneStateService svc;
    QSignalSpy spy(&svc, &oap::PhoneStateService::connectionChanged);
    QVariantMap props{
        {QStringLiteral("Connected"), true},
        {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")}}, // A2DP sink only
    };
    svc.adoptBluezDevice(QStringLiteral("/org/bluez/hci0/dev_CC_DD"), props);
    QCOMPARE(spy.count(), 0);
    QVERIFY(!svc.phoneConnected());
}

void TestPhoneStateService::interfacesAdded_payloadConsumed_noBusReadback()
{
    oap::PhoneStateService svc;
    QSignalSpy spy(&svc, &oap::PhoneStateService::connectionChanged);
    oap::BluezInterfaceMap ifaces;
    ifaces.insert(QStringLiteral("org.bluez.Device1"), QVariantMap{
        {QStringLiteral("Connected"), true},
        {QStringLiteral("Alias"), QStringLiteral("Pixel 8")},
        {QStringLiteral("UUIDs"), QStringList{QStringLiteral("0000111e-0000-1000-8000-00805f9b34fb")}},
    });
    // private slot → invoke through the meta-object (also proves the
    // signature is invokable with the registered type)
    QMetaObject::invokeMethod(&svc, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_AA_BB"))),
        Q_ARG(oap::BluezInterfaceMap, ifaces));
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(TestPhoneStateService)
#include "test_phone_state_service.moc"
