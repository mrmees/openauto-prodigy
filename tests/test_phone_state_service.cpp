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

QTEST_GUILESS_MAIN(TestPhoneStateService)
#include "test_phone_state_service.moc"
