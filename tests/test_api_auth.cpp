#include <QtTest>
#include <QTemporaryDir>
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
    void testEmptyStoreRoundTrip();
    void testLegacyNullEmptyStoreLoads();
    void testStorePermissions();
    void testUpsertReplaces();
    void testSaveReturnsFalseOnOpenFailure();
    void testAtomicSaveFailurePreservesExistingFile();
    void testFailedReloadPreservesStateAndBlocksSave();
    void testStoreRejectsInvalidDocuments_data();
    void testStoreRejectsInvalidDocuments();
    void testStoreLoadsLegacyCredentialWithoutGeneration();
};

namespace {

QByteArray validClientYaml(const QByteArray& overrides = {}) {
    if (!overrides.isEmpty())
        return overrides;
    return QByteArray(
        "clients:\n"
        "  - id: client-1\n"
        "    secret_hex: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "    name: Phone\n"
        "    kind: 1\n"
        "    paired_at: 2026-07-22T00:00:00Z\n"
        "    credential_generation: 2\n");
}

void writeFile(const QString& path, const QByteArray& contents) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), qint64(contents.size()));
}

} // namespace

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

void TestApiAuth::testEmptyStoreRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("clients.yaml");

    PairedClientStore store(path);
    QVERIFY(store.load());
    QVERIFY(store.save());

    PairedClientStore reloaded(path);
    QVERIFY(reloaded.load());
    QVERIFY(reloaded.all().isEmpty());
}

void TestApiAuth::testLegacyNullEmptyStoreLoads() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("clients.yaml");
    writeFile(path, "clients: ~\n");

    PairedClientStore store(path);
    QVERIFY(store.load());
    QVERIFY(store.loadedSuccessfully());
    QVERIFY(store.all().isEmpty());
}

void TestApiAuth::testStorePermissions() {
    QString path = "/tmp/oap_test_api_clients_perm.yaml";
    QFile::remove(path);
    PairedClientStore store(path);
    QVERIFY(store.load());
    store.upsert({"id", QByteArray(32, 'k'), "n", 0, ""});
    QVERIFY(store.save());
    auto perms = QFile(path).permissions();
    const QFileDevice::Permissions permissionMask =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
        | QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    QCOMPARE(perms & permissionMask,
             QFileDevice::Permissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
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

void TestApiAuth::testAtomicSaveFailurePreservesExistingFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("clients.yaml");

    PairedClientStore store(path);
    QVERIFY(store.load());
    store.upsert({"original", QByteArray(32, 'o'), "Original", 1, "before"});
    QVERIFY(store.save());

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray originalBytes = file.readAll();
    file.close();

    store.upsert({"replacement", QByteArray(32, 'r'), "Replacement", 1, "after"});
    const QFileDevice::Permissions readOnlyDirectory =
        QFileDevice::ReadOwner | QFileDevice::ExeOwner;
    QVERIFY(QFile::setPermissions(dir.path(), readOnlyDirectory));
    const bool saved = store.save();
    QVERIFY(QFile::setPermissions(dir.path(), QFileDevice::ReadOwner
                                  | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    QVERIFY(!saved);

    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), originalBytes);
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

void TestApiAuth::testStoreRejectsInvalidDocuments_data() {
    QTest::addColumn<QByteArray>("yaml");

    const QByteArray fields =
        "    secret_hex: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "    name: Phone\n"
        "    kind: 1\n"
        "    paired_at: 2026-07-22T00:00:00Z\n"
        "    credential_generation: 2\n";

    QTest::newRow("empty-document") << QByteArray();
    QTest::newRow("non-map-root") << QByteArray("- clients\n");
    QTest::newRow("missing-clients") << QByteArray("version: 1\n");
    QTest::newRow("clients-not-sequence") << QByteArray("clients: {}\n");
    QTest::newRow("client-not-map") << QByteArray("clients:\n  - client-1\n");
    QTest::newRow("missing-id") << QByteArray("clients:\n  - name: Phone\n") +
        "    secret_hex: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "    kind: 1\n    paired_at: now\n";
    QTest::newRow("empty-id") << QByteArray("clients:\n  - id: ''\n") + fields;
    QTest::newRow("missing-secret") << QByteArray(
        "clients:\n  - id: client-1\n    name: Phone\n    kind: 1\n    paired_at: now\n");
    QTest::newRow("short-secret") << QByteArray(
        "clients:\n  - id: client-1\n    secret_hex: 0123\n"
        "    name: Phone\n    kind: 1\n    paired_at: now\n");
    QTest::newRow("non-hex-secret") << QByteArray(
        "clients:\n  - id: client-1\n"
        "    secret_hex: g123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "    name: Phone\n    kind: 1\n    paired_at: now\n");
    QTest::newRow("missing-name") << QByteArray(
        "clients:\n  - id: client-1\n"
        "    secret_hex: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "    kind: 1\n    paired_at: now\n");
    QTest::newRow("invalid-kind") << QByteArray(
        "clients:\n  - id: client-1\n"
        "    secret_hex: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "    name: Phone\n    kind: nope\n    paired_at: now\n");
    QTest::newRow("missing-paired-at") << QByteArray(
        "clients:\n  - id: client-1\n"
        "    secret_hex: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "    name: Phone\n    kind: 1\n");
    QTest::newRow("unknown-generation") << QByteArray(
        "clients:\n  - id: client-1\n"
        "    secret_hex: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
        "    name: Phone\n    kind: 1\n    paired_at: now\n    credential_generation: 3\n");
    QTest::newRow("duplicate-id") << validClientYaml() + QByteArray(
        "  - id: client-1\n"
        "    secret_hex: fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210\n"
        "    name: Other Phone\n    kind: 1\n    paired_at: later\n"
        "    credential_generation: 1\n");
}

void TestApiAuth::testStoreRejectsInvalidDocuments() {
    QFETCH(QByteArray, yaml);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("clients.yaml");
    writeFile(path, yaml);

    PairedClientStore store(path);
    QVERIFY(!store.load());
    QVERIFY(!store.loadedSuccessfully());
    QVERIFY(store.all().isEmpty());
    QVERIFY(!store.save());

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), yaml);
}

void TestApiAuth::testStoreLoadsLegacyCredentialWithoutGeneration() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("clients.yaml");
    QByteArray yaml = validClientYaml();
    yaml.replace("    credential_generation: 2\n", "");
    writeFile(path, yaml);

    PairedClientStore store(path);
    QVERIFY(store.load());
    QCOMPARE(store.all().size(), 1);
    QCOMPARE(store.all().first().credentialGeneration, kLegacyCredentialGeneration);
}

QTEST_MAIN(TestApiAuth)
#include "test_api_auth.moc"
