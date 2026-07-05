// Bus-independent safety tests. Protocol conformance is live-check territory
// (design doc §11) — do NOT fake a session bus here.
#include <QtTest/QtTest>
#include "core/services/TelephonyClient.hpp"

class TestTelephonyClient : public QObject {
    Q_OBJECT
private slots:
    void testConstructIsInert();
    void testStartStopSafeWithoutService();
    void testCommandsSafeWhenUnavailable();
    void testRejectScoCachedWhenUnavailable();
};

void TestTelephonyClient::testConstructIsInert() {
    oap::TelephonyClient c;               // must not touch any bus
    QVERIFY(!c.available());
    QVERIFY(c.agAddress().isEmpty());
    QVERIFY(c.transportState().isEmpty());
    QVERIFY(c.codec().isEmpty());
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

QTEST_MAIN(TestTelephonyClient)
#include "test_telephony_client.moc"
