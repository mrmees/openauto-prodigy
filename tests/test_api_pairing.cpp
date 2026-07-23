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
    QVERIFY(store.load());
    PairingManager mgr(&store);
    mgr.setCodeGeneratorForTest([] { return QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWX"); });

    QVERIFY(!mgr.windowOpen());
    QCOMPARE(mgr.currentCode(), QString(""));

    QSignalSpy spy(&mgr, &PairingManager::windowChanged);

    QVERIFY(mgr.startWindow(60));
    QVERIFY(mgr.windowOpen());
    QCOMPARE(mgr.currentCode(), QString("ABCDEFGHIJKLMNOPQRSTUVWX"));
    QCOMPARE(mgr.displayCode(), QString("ABCD-EFGH-IJKL-MNOP-QRST-UVWX"));
    QVERIFY(PairingManager::isValidCode(mgr.currentCode()));
    QCOMPARE(mgr.currentSalt().size(), 16);
    QCOMPARE(spy.count(), 1);

    mgr.cancelWindow();
    QVERIFY(!mgr.windowOpen());
    QCOMPARE(mgr.currentCode(), QString(""));
    QCOMPARE(spy.count(), 2);
}

void TestApiPairing::testCompletePairingHappyPath() {
    PairedClientStore store("/tmp/oap_test_pairing_store.yaml");
    QFile::remove("/tmp/oap_test_pairing_store.yaml");
    QVERIFY(store.load());
    PairingManager mgr(&store);
    QVERIFY(mgr.startWindow(60));
    QByteArray nonce = mgr.makeNonce();
    QByteArray secret = deriveSecret(mgr.currentCode(), mgr.currentSalt());
    auto id = mgr.completePairing(nonce, hmacProof(secret, nonce), "TestPhone", 3);
    QVERIFY(id.has_value());
    QVERIFY(store.find(*id).has_value());
    QCOMPARE(store.find(*id)->secret, secret);
    QCOMPARE(store.find(*id)->credentialGeneration, kSecureCodeCredentialGeneration);
    QVERIFY(!mgr.windowOpen());
}

void TestApiPairing::testWrongPinRejected() {
    PairedClientStore store("/tmp/oap_test_pairing_wrongpin.yaml");
    QFile::remove("/tmp/oap_test_pairing_wrongpin.yaml");
    QVERIFY(store.load());
    PairingManager mgr(&store);
    QVERIFY(mgr.startWindow(60));
    QByteArray nonce = mgr.makeNonce();

    // Derive secret from a deliberately wrong code (not the real one).
    QByteArray wrongSecret = deriveSecret("ZZZZZZZZZZZZZZZZZZZZZZZZ", mgr.currentSalt());
    QByteArray badProof = hmacProof(wrongSecret, nonce);

    auto id = mgr.completePairing(nonce, badProof, "TestPhone", 3);
    QVERIFY(!id.has_value());
    // A single bad guess must NOT close the pairing window.
    QVERIFY(mgr.windowOpen());
    QVERIFY(!mgr.currentCode().isEmpty());
}

void TestApiPairing::testClosedWindowRejects() {
    PairedClientStore store("/tmp/oap_test_pairing_closed.yaml");
    QFile::remove("/tmp/oap_test_pairing_closed.yaml");
    QVERIFY(store.load());
    PairingManager mgr(&store);

    QVERIFY(!mgr.windowOpen());
    QByteArray nonce = mgr.makeNonce();
    QByteArray secret = deriveSecret("ABCDEFGHIJKLMNOPQRSTUVWX", QByteArray(16, 's'));
    auto id = mgr.completePairing(nonce, hmacProof(secret, nonce), "TestPhone", 3);
    QVERIFY(!id.has_value());
    QVERIFY(!mgr.windowOpen());
}

void TestApiPairing::testWindowExpiry() {
    PairedClientStore store("/tmp/oap_test_pairing_expiry.yaml");
    QFile::remove("/tmp/oap_test_pairing_expiry.yaml");
    QVERIFY(store.load());
    PairingManager mgr(&store);

    QVERIFY(mgr.startWindow(1));
    QVERIFY(mgr.windowOpen());
    QTest::qWait(1100);
    QVERIFY(!mgr.windowOpen());
    QCOMPARE(mgr.currentCode(), QString(""));
}

void TestApiPairing::testCompletePairingPersistFailure() {
    // Store path lives in a directory that doesn't exist, so save() will fail
    // to open the file. A correct proof must still be rejected: no client id
    // handed out, no entry left in the in-memory store, and the pairing
    // window stays open so the user can retry (mirrors a wrong-proof attempt).
    PairedClientStore store("/nonexistent-oap-dir/clients.yaml");
    QVERIFY(store.load());
    PairingManager mgr(&store);
    QVERIFY(mgr.startWindow(60));
    QByteArray nonce = mgr.makeNonce();
    QByteArray secret = deriveSecret(mgr.currentCode(), mgr.currentSalt());

    auto id = mgr.completePairing(nonce, hmacProof(secret, nonce), "TestPhone", 3);
    QVERIFY(!id.has_value());
    QVERIFY(mgr.windowOpen());
    QVERIFY(store.all().isEmpty());
}

QTEST_MAIN(TestApiPairing)
#include "test_api_pairing.moc"
