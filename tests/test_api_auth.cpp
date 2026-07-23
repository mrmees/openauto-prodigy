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
    void testSaveReturnsFalseOnOpenFailure();
    void testFailedReloadPreservesStateAndBlocksSave();
};

void TestApiAuth::testDeriveSecretDeterministic() {
    QByteArray salt("0123456789abcdef");
    QByteArray s1 = deriveSecret("ABCDEFGHIJKLMNOPQRSTUVWX", salt);
    QByteArray s2 = deriveSecret("ABCDEFGHIJKLMNOPQRSTUVWX", salt);
    QCOMPARE(s1, s2);
    QCOMPARE(s1.size(), 32);
    QCOMPARE(s1.toHex(), QByteArray("9786e3d8dd45530435cc5b09d71e93b76dccf0e3e402ae7af5bdb6400a5c1472"));
}

void TestApiAuth::testDeriveSecretSaltMatters() {
    QVERIFY(deriveSecret("ABCDEFGHIJKLMNOPQRSTUVWX", "saltA") !=
            deriveSecret("ABCDEFGHIJKLMNOPQRSTUVWX", "saltB"));
    QVERIFY(deriveSecret("ABCDEFGHIJKLMNOPQRSTUVWX", "saltA") !=
            deriveSecret("ZYXWVUTSRQPONMLKJIHGFEDC", "saltA"));
}

void TestApiAuth::testHmacProofVerifies() {
    QByteArray secret = deriveSecret("ABCDEFGHIJKLMNOPQRSTUVWX", "salt");
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
    QVERIFY(store.save());

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
    QVERIFY(store.load());
    store.upsert({"id", QByteArray(32, 'k'), "n", 0, ""});
    QVERIFY(store.save());
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

void TestApiAuth::testSaveReturnsFalseOnOpenFailure() {
    // Directory component doesn't exist -> QFile::open() fails -> save() must
    // report failure rather than silently swallowing it.
    PairedClientStore store("/nonexistent-oap-dir/clients.yaml");
    QVERIFY(store.load());
    store.upsert({"id", QByteArray(32, 'k'), "n", 0, ""});
    QVERIFY(!store.save());
}

void TestApiAuth::testFailedReloadPreservesStateAndBlocksSave() {
    const QString path = "/tmp/oap_test_api_clients_corrupt.yaml";
    QFile::remove(path);
    PairedClientStore store(path);
    QVERIFY(store.load());
    store.upsert({"kept", QByteArray(32, 'k'), "Phone", 3, "2026-07-22T00:00:00Z"});
    QVERIFY(store.save());

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write("clients: [\n"), qint64(11));
    file.close();
    const QByteArray corruptBytes = QByteArray("clients: [\n");

    QVERIFY(!store.load());
    QVERIFY(store.find("kept").has_value());
    store.upsert({"new", QByteArray(32, 'n'), "New", 3, ""});
    QVERIFY(!store.save());

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), corruptBytes);
}

QTEST_MAIN(TestApiAuth)
#include "test_api_auth.moc"
