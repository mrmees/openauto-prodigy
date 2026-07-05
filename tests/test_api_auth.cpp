#include <QtTest>
#include "core/api/ApiAuth.hpp"

using namespace oap::api;

class TestApiAuth : public QObject {
    Q_OBJECT
private slots:
    void testDeriveSecretDeterministic();
    void testDeriveSecretSaltMatters();
    void testHmacProofVerifies();
    void testConstantTimeEquals();
    void testStoreRoundTrip();
    void testStorePermissions();
    void testUpsertReplaces();
};

void TestApiAuth::testDeriveSecretDeterministic() {
    QByteArray salt("0123456789abcdef");
    QByteArray s1 = deriveSecret("123456", salt);
    QByteArray s2 = deriveSecret("123456", salt);
    QCOMPARE(s1, s2);
    QCOMPARE(s1.size(), 32);
}

void TestApiAuth::testDeriveSecretSaltMatters() {
    QVERIFY(deriveSecret("123456", "saltA") != deriveSecret("123456", "saltB"));
    QVERIFY(deriveSecret("123456", "saltA") != deriveSecret("654321", "saltA"));
}

void TestApiAuth::testHmacProofVerifies() {
    QByteArray secret = deriveSecret("123456", "salt");
    QByteArray nonce(32, 'n');
    QByteArray proof = hmacProof(secret, nonce);
    QCOMPARE(proof.size(), 32);
    QVERIFY(constantTimeEquals(proof, hmacProof(secret, nonce)));
    QVERIFY(!constantTimeEquals(proof, hmacProof(secret, QByteArray(32, 'x'))));
}

void TestApiAuth::testConstantTimeEquals() {
    QVERIFY(constantTimeEquals("abc", "abc"));
    QVERIFY(!constantTimeEquals("abc", "abd"));
    QVERIFY(!constantTimeEquals("abc", "abcd"));
    QVERIFY(constantTimeEquals("", ""));
}

void TestApiAuth::testStoreRoundTrip() {
    QString path = "/tmp/oap_test_api_clients.yaml";
    QFile::remove(path);
    PairedClientStore store(path);
    QVERIFY(store.load());  // missing file is fine
    QVERIFY(store.all().isEmpty());

    PairedClient c{"id-1", QByteArray(32, 's'), "TestPhone", 1, "2026-07-06T00:00:00Z"};
    store.upsert(c);
    store.save();

    PairedClientStore store2(path);
    QVERIFY(store2.load());
    auto found = store2.find("id-1");
    QVERIFY(found.has_value());
    QCOMPARE(found->secret, QByteArray(32, 's'));
    QCOMPARE(found->name, QString("TestPhone"));
    QVERIFY(!store2.find("nope").has_value());
}

void TestApiAuth::testStorePermissions() {
    QString path = "/tmp/oap_test_api_clients_perm.yaml";
    QFile::remove(path);
    PairedClientStore store(path);
    store.upsert({"id", QByteArray(32, 'k'), "n", 0, ""});
    store.save();
    auto perms = QFile(path).permissions();
    QVERIFY(!(perms & QFileDevice::ReadGroup));
    QVERIFY(!(perms & QFileDevice::ReadOther));
}

void TestApiAuth::testUpsertReplaces() {
    PairedClientStore store("/tmp/oap_test_api_clients2.yaml");
    store.upsert({"id-1", QByteArray(32, 'a'), "A", 0, ""});
    store.upsert({"id-1", QByteArray(32, 'b'), "B", 0, ""});
    QCOMPARE(store.all().size(), 1);
    QCOMPARE(store.find("id-1")->name, QString("B"));
}

QTEST_MAIN(TestApiAuth)
#include "test_api_auth.moc"
