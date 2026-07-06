#include <QtTest>
#include "core/api/PairingManager.hpp"
#include "core/api/ApiAuth.hpp"

using namespace oap::api;

class TestApiPairing : public QObject {
    Q_OBJECT
private slots:
    void testWindowLifecycle();
    void testCompletePairingHappyPath();
    void testWrongPinRejected();
    void testClosedWindowRejects();
    void testWindowExpiry();
    void testCompletePairingPersistFailure();
};

void TestApiPairing::testWindowLifecycle() {
    PairedClientStore store("/tmp/oap_test_pairing_lifecycle.yaml");
    QFile::remove("/tmp/oap_test_pairing_lifecycle.yaml");
    PairingManager mgr(&store);

    QVERIFY(!mgr.windowOpen());
    QCOMPARE(mgr.currentPin(), QString(""));

    QSignalSpy spy(&mgr, &PairingManager::windowChanged);

    mgr.startWindow(60);
    QVERIFY(mgr.windowOpen());
    QCOMPARE(mgr.currentPin().length(), 6);
    bool allDigits = true;
    for (const QChar& ch : mgr.currentPin()) {
        if (!ch.isDigit()) allDigits = false;
    }
    QVERIFY(allDigits);
    QCOMPARE(mgr.currentSalt().size(), 16);
    QCOMPARE(spy.count(), 1);

    mgr.cancelWindow();
    QVERIFY(!mgr.windowOpen());
    QCOMPARE(mgr.currentPin(), QString(""));
    QCOMPARE(spy.count(), 2);
}

void TestApiPairing::testCompletePairingHappyPath() {
    PairedClientStore store("/tmp/oap_test_pairing_store.yaml");
    QFile::remove("/tmp/oap_test_pairing_store.yaml");
    PairingManager mgr(&store);
    mgr.startWindow(60);
    QByteArray nonce = mgr.makeNonce();
    QByteArray secret = deriveSecret(mgr.currentPin(), mgr.currentSalt());
    auto id = mgr.completePairing(nonce, hmacProof(secret, nonce), "TestPhone", 3);
    QVERIFY(id.has_value());
    QVERIFY(store.find(*id).has_value());
    QCOMPARE(store.find(*id)->secret, secret);
    QVERIFY(!mgr.windowOpen());
}

void TestApiPairing::testWrongPinRejected() {
    PairedClientStore store("/tmp/oap_test_pairing_wrongpin.yaml");
    QFile::remove("/tmp/oap_test_pairing_wrongpin.yaml");
    PairingManager mgr(&store);
    mgr.startWindow(60);
    QByteArray nonce = mgr.makeNonce();

    // Derive secret from a deliberately wrong pin (not the real one).
    QByteArray wrongSecret = deriveSecret("000000", mgr.currentSalt());
    QByteArray badProof = hmacProof(wrongSecret, nonce);

    auto id = mgr.completePairing(nonce, badProof, "TestPhone", 3);
    QVERIFY(!id.has_value());
    // A single bad guess must NOT close the pairing window.
    QVERIFY(mgr.windowOpen());
    QVERIFY(!mgr.currentPin().isEmpty());
}

void TestApiPairing::testClosedWindowRejects() {
    PairedClientStore store("/tmp/oap_test_pairing_closed.yaml");
    QFile::remove("/tmp/oap_test_pairing_closed.yaml");
    PairingManager mgr(&store);

    QVERIFY(!mgr.windowOpen());
    QByteArray nonce = mgr.makeNonce();
    QByteArray secret = deriveSecret("123456", QByteArray(16, 's'));
    auto id = mgr.completePairing(nonce, hmacProof(secret, nonce), "TestPhone", 3);
    QVERIFY(!id.has_value());
    QVERIFY(!mgr.windowOpen());
}

void TestApiPairing::testWindowExpiry() {
    PairedClientStore store("/tmp/oap_test_pairing_expiry.yaml");
    QFile::remove("/tmp/oap_test_pairing_expiry.yaml");
    PairingManager mgr(&store);

    mgr.startWindow(1);
    QVERIFY(mgr.windowOpen());
    QTest::qWait(1100);
    QVERIFY(!mgr.windowOpen());
    QCOMPARE(mgr.currentPin(), QString(""));
}

void TestApiPairing::testCompletePairingPersistFailure() {
    // Store path lives in a directory that doesn't exist, so save() will fail
    // to open the file. A correct proof must still be rejected: no client id
    // handed out, no entry left in the in-memory store, and the pairing
    // window stays open so the user can retry (mirrors a wrong-proof attempt).
    PairedClientStore store("/nonexistent-oap-dir/clients.yaml");
    PairingManager mgr(&store);
    mgr.startWindow(60);
    QByteArray nonce = mgr.makeNonce();
    QByteArray secret = deriveSecret(mgr.currentPin(), mgr.currentSalt());

    auto id = mgr.completePairing(nonce, hmacProof(secret, nonce), "TestPhone", 3);
    QVERIFY(!id.has_value());
    QVERIFY(mgr.windowOpen());
    QVERIFY(store.all().isEmpty());
}

QTEST_MAIN(TestApiPairing)
#include "test_api_pairing.moc"
