#include <QtTest/QtTest>
#include <oaa/Messenger/Cryptor.hpp>

#include <memory>

namespace {

using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PkeyContextPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;
using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;

PkeyPtr generateRsaKey()
{
    PkeyContextPtr context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr),
                           EVP_PKEY_CTX_free);
    if (!context || EVP_PKEY_keygen_init(context.get()) <= 0
        || EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 2048) <= 0) {
        return PkeyPtr(nullptr, EVP_PKEY_free);
    }

    EVP_PKEY* key = nullptr;
    if (EVP_PKEY_keygen(context.get(), &key) <= 0)
        return PkeyPtr(nullptr, EVP_PKEY_free);
    return PkeyPtr(key, EVP_PKEY_free);
}

QByteArray bioContents(BIO* bio)
{
    char* data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);
    return size > 0 ? QByteArray(data, static_cast<int>(size)) : QByteArray{};
}

struct MismatchedPemMaterial {
    QByteArray certificate;
    QByteArray privateKey;
};

MismatchedPemMaterial makeMismatchedPemMaterial()
{
    auto certificateKey = generateRsaKey();
    auto otherKey = generateRsaKey();
    X509Ptr certificate(X509_new(), X509_free);
    if (!certificateKey || !otherKey || !certificate)
        return {};

    if (X509_set_version(certificate.get(), 2) != 1
        || ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), 1) != 1
        || !X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0)
        || !X509_gmtime_adj(X509_getm_notAfter(certificate.get()), 3600)
        || X509_set_pubkey(certificate.get(), certificateKey.get()) != 1) {
        return {};
    }

    X509_NAME* name = X509_get_subject_name(certificate.get());
    if (!name
        || X509_NAME_add_entry_by_txt(
               name, "CN", MBSTRING_ASC,
               reinterpret_cast<const unsigned char*>("cryptor-test"),
               -1, -1, 0) != 1
        || X509_set_issuer_name(certificate.get(), name) != 1
        || X509_sign(certificate.get(), certificateKey.get(), EVP_sha256()) <= 0) {
        return {};
    }

    BioPtr certificateBio(BIO_new(BIO_s_mem()), BIO_free);
    BioPtr keyBio(BIO_new(BIO_s_mem()), BIO_free);
    if (!certificateBio || !keyBio
        || PEM_write_bio_X509(certificateBio.get(), certificate.get()) != 1
        || PEM_write_bio_PrivateKey(keyBio.get(), otherKey.get(), nullptr,
                                    nullptr, 0, nullptr, nullptr) != 1) {
        return {};
    }

    return {bioContents(certificateBio.get()), bioContents(keyBio.get())};
}

} // namespace

class TestCryptor : public QObject {
    Q_OBJECT
private:
    // Drive TLS handshake between client and server Cryptors.
    // Returns true when both sides are active.
    bool driveHandshake(oaa::Cryptor& client, oaa::Cryptor& server) {
        for (int i = 0; i < 20; ++i) {
            client.doHandshake();
            auto clientOut = client.readHandshakeBuffer();
            if (!clientOut.isComplete())
                return false;
            if (!clientOut.data.isEmpty()
                && !server.writeHandshakeBuffer(clientOut.data))
                return false;

            server.doHandshake();
            auto serverOut = server.readHandshakeBuffer();
            if (!serverOut.isComplete())
                return false;
            if (!serverOut.data.isEmpty()
                && !client.writeHandshakeBuffer(serverOut.data))
                return false;

            if (client.isActive() && server.isActive())
                return true;
        }
        return false;
    }

private slots:
    void testHandshakeBetweenPeers() {
        oaa::Cryptor client, server;
        QVERIFY(client.init(oaa::Cryptor::Role::Client));
        QVERIFY(server.init(oaa::Cryptor::Role::Server));

        QVERIFY(driveHandshake(client, server));
        QVERIFY(client.isActive());
        QVERIFY(server.isActive());
    }

    void testEncryptDecrypt() {
        oaa::Cryptor client, server;
        QVERIFY(client.init(oaa::Cryptor::Role::Client));
        QVERIFY(server.init(oaa::Cryptor::Role::Server));
        QVERIFY(driveHandshake(client, server));

        QByteArray plaintext("Hello AA");
        auto ciphertext = client.encrypt(plaintext);

        // Ciphertext must differ from plaintext (TLS overhead)
        QVERIFY(ciphertext.isComplete());
        QVERIFY(ciphertext.data != plaintext);
        QVERIFY(ciphertext.data.size() > plaintext.size());

        auto decrypted = server.decrypt(ciphertext.data, ciphertext.data.size());
        QVERIFY(decrypted.isComplete());
        QCOMPARE(decrypted.data, plaintext);
    }

    void testMaximumFramePayloadAndOversizedInput() {
        oaa::Cryptor client, server;
        QVERIFY(client.init(oaa::Cryptor::Role::Client));
        QVERIFY(server.init(oaa::Cryptor::Role::Server));
        QVERIFY(driveHandshake(client, server));

        QByteArray payload(oaa::FRAME_MAX_PAYLOAD, '\0');
        for (int i = 0; i < payload.size(); ++i)
            payload[i] = static_cast<char>(i & 0xFF);

        auto ciphertext = client.encrypt(payload);
        QVERIFY(ciphertext.isComplete());
        QVERIFY(!ciphertext.data.isEmpty());

        auto decrypted = server.decrypt(ciphertext.data, ciphertext.data.size());
        QVERIFY(decrypted.isComplete());
        QCOMPARE(decrypted.data, payload);

        auto oversized = client.encrypt(
            QByteArray(oaa::FRAME_MAX_PAYLOAD + 1, 'X'));
        QVERIFY(!oversized.isComplete());
        QVERIFY(oversized.data.isEmpty());
        QVERIFY(!oversized.error.isEmpty());
    }

    void testMultipleMessages() {
        oaa::Cryptor client, server;
        QVERIFY(client.init(oaa::Cryptor::Role::Client));
        QVERIFY(server.init(oaa::Cryptor::Role::Server));
        QVERIFY(driveHandshake(client, server));

        QByteArray msg1("First message");
        QByteArray msg2("Second message with more data");
        QByteArray msg3("Third");

        // Encrypt and decrypt each independently, preserving order
        auto ct1 = client.encrypt(msg1);
        auto dec1 = server.decrypt(ct1.data, ct1.data.size());
        QVERIFY(ct1.isComplete());
        QVERIFY(dec1.isComplete());
        QCOMPARE(dec1.data, msg1);

        auto ct2 = client.encrypt(msg2);
        auto dec2 = server.decrypt(ct2.data, ct2.data.size());
        QVERIFY(ct2.isComplete());
        QVERIFY(dec2.isComplete());
        QCOMPARE(dec2.data, msg2);

        auto ct3 = client.encrypt(msg3);
        auto dec3 = server.decrypt(ct3.data, ct3.data.size());
        QVERIFY(ct3.isComplete());
        QVERIFY(dec3.isComplete());
        QCOMPARE(dec3.data, msg3);
    }

    void testDeinit() {
        oaa::Cryptor cryptor;
        QVERIFY(cryptor.init(oaa::Cryptor::Role::Client));
        QVERIFY(!cryptor.isActive());
        cryptor.deinit();
        QVERIFY(!cryptor.isActive());
        // Should be safe to call deinit again
        cryptor.deinit();
    }

    void testHandshakeDistinguishesWantIoFromFatalInput() {
        oaa::Cryptor client;
        QVERIFY(client.init(oaa::Cryptor::Role::Client));

        QCOMPARE(client.doHandshake(), oaa::Cryptor::HandshakeResult::WantIo);
        QVERIFY(client.readHandshakeBuffer().isComplete());

        QVERIFY(client.writeHandshakeBuffer(QByteArray(64, 'X')));
        QCOMPARE(client.doHandshake(), oaa::Cryptor::HandshakeResult::Failed);
        QVERIFY(!client.lastHandshakeError().isEmpty());
        QVERIFY(!client.isActive());
    }

    void testInvalidCredentialMaterialFailsTransactionallyAndCanRetry() {
        oaa::Cryptor cryptor;

        QVERIFY(!cryptor.init(oaa::Cryptor::Role::Client,
                              QByteArrayLiteral("not a certificate"),
                              QByteArrayLiteral("not a key")));
        QVERIFY(!cryptor.isActive());
        QVERIFY(!cryptor.lastError().isEmpty());
        QCOMPARE(cryptor.doHandshake(), oaa::Cryptor::HandshakeResult::Failed);

        QVERIFY(cryptor.init(oaa::Cryptor::Role::Client));
        QVERIFY(cryptor.lastError().isEmpty());
        QCOMPARE(cryptor.doHandshake(), oaa::Cryptor::HandshakeResult::WantIo);
    }

    void testValidButMismatchedCredentialMaterialFailsTransactionally() {
        const auto material = makeMismatchedPemMaterial();
        QVERIFY(!material.certificate.isEmpty());
        QVERIFY(!material.privateKey.isEmpty());

        oaa::Cryptor cryptor;
        QVERIFY(!cryptor.init(oaa::Cryptor::Role::Client,
                              material.certificate, material.privateKey));
        QVERIFY(!cryptor.isActive());
        QVERIFY(!cryptor.lastError().isEmpty());
        QVERIFY(cryptor.lastError().contains(QStringLiteral("mismatch")));

        QVERIFY(cryptor.init(oaa::Cryptor::Role::Client));
        QCOMPARE(cryptor.doHandshake(), oaa::Cryptor::HandshakeResult::WantIo);
    }

    void testIncompleteAndFatalTlsRecordsFailWithoutPlaintext() {
        oaa::Cryptor client, server;
        QVERIFY(client.init(oaa::Cryptor::Role::Client));
        QVERIFY(server.init(oaa::Cryptor::Role::Server));
        QVERIFY(driveHandshake(client, server));

        auto encrypted = client.encrypt(QByteArrayLiteral("complete record"));
        QVERIFY(encrypted.isComplete());

        auto incomplete = server.decrypt(encrypted.data.left(encrypted.data.size() / 2),
                                         encrypted.data.size() / 2);
        QVERIFY(!incomplete.isComplete());
        QVERIFY(incomplete.data.isEmpty());
        QVERIFY(!incomplete.error.isEmpty());

        // Use a fresh peer because the incomplete record has intentionally
        // left the previous TLS stream unusable.
        oaa::Cryptor freshClient, freshServer;
        QVERIFY(freshClient.init(oaa::Cryptor::Role::Client));
        QVERIFY(freshServer.init(oaa::Cryptor::Role::Server));
        QVERIFY(driveHandshake(freshClient, freshServer));
        auto corrupted = freshClient.encrypt(QByteArrayLiteral("bad record"));
        QVERIFY(corrupted.isComplete());
        corrupted.data[corrupted.data.size() - 1] ^= char(0x01);

        auto fatal = freshServer.decrypt(corrupted.data, corrupted.data.size());
        QVERIFY(!fatal.isComplete());
        QVERIFY(fatal.data.isEmpty());
        QVERIFY(!fatal.error.isEmpty());
    }

    void testDecryptConsumesEveryCompleteRecordAndRejectsTrailingPartialRecord() {
        oaa::Cryptor client, server;
        QVERIFY(client.init(oaa::Cryptor::Role::Client));
        QVERIFY(server.init(oaa::Cryptor::Role::Server));
        QVERIFY(driveHandshake(client, server));

        const auto first = client.encrypt(QByteArrayLiteral("first"));
        const auto second = client.encrypt(QByteArrayLiteral("second"));
        QVERIFY(first.isComplete());
        QVERIFY(second.isComplete());

        const QByteArray completeRecords = first.data + second.data;
        const auto combined = server.decrypt(completeRecords,
                                             completeRecords.size());
        QVERIFY(combined.isComplete());
        QCOMPARE(combined.data, QByteArrayLiteral("firstsecond"));

        oaa::Cryptor freshClient, freshServer;
        QVERIFY(freshClient.init(oaa::Cryptor::Role::Client));
        QVERIFY(freshServer.init(oaa::Cryptor::Role::Server));
        QVERIFY(driveHandshake(freshClient, freshServer));

        const auto freshFirst = freshClient.encrypt(QByteArrayLiteral("accepted"));
        const auto freshSecond = freshClient.encrypt(QByteArrayLiteral("truncated"));
        QVERIFY(freshFirst.isComplete());
        QVERIFY(freshSecond.isComplete());
        QByteArray trailingPartial = freshFirst.data + freshSecond.data;
        trailingPartial.chop(1);

        const auto rejected = freshServer.decrypt(trailingPartial,
                                                  trailingPartial.size());
        QVERIFY(!rejected.isComplete());
        QVERIFY(rejected.data.isEmpty());
        QVERIFY(!rejected.error.isEmpty());
    }

    void testUninitializedRuntimeIoFailsClosed() {
        oaa::Cryptor cryptor;
        auto encrypted = cryptor.encrypt(QByteArrayLiteral("plaintext"));
        auto decrypted = cryptor.decrypt(QByteArrayLiteral("ciphertext"), 10);

        QVERIFY(!encrypted.isComplete());
        QVERIFY(encrypted.data.isEmpty());
        QVERIFY(!encrypted.error.isEmpty());
        QVERIFY(!decrypted.isComplete());
        QVERIFY(decrypted.data.isEmpty());
        QVERIFY(!decrypted.error.isEmpty());
    }
};

QTEST_MAIN(TestCryptor)
#include "test_cryptor.moc"
