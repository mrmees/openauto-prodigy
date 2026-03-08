#include <QtTest/QtTest>
#include <oaa/Messenger/Cryptor.hpp>

class TestCryptor : public QObject {
    Q_OBJECT
private:
    // Drive TLS handshake between client and server Cryptors.
    // Returns true when both sides are active.
    bool driveHandshake(oaa::Cryptor& client, oaa::Cryptor& server) {
        for (int i = 0; i < 20; ++i) {
            client.doHandshake();
            QByteArray clientOut = client.readHandshakeBuffer();
            if (!clientOut.isEmpty())
                server.writeHandshakeBuffer(clientOut);

            server.doHandshake();
            QByteArray serverOut = server.readHandshakeBuffer();
            if (!serverOut.isEmpty())
                client.writeHandshakeBuffer(serverOut);

            if (client.isActive() && server.isActive())
                return true;
        }
        return false;
    }

private slots:
    void testHandshakeBetweenPeers() {
        oaa::Cryptor client, server;
        client.init(oaa::Cryptor::Role::Client);
        server.init(oaa::Cryptor::Role::Server);

        QVERIFY(driveHandshake(client, server));
        QVERIFY(client.isActive());
        QVERIFY(server.isActive());
    }

    void testEncryptDecrypt() {
        oaa::Cryptor client, server;
        client.init(oaa::Cryptor::Role::Client);
        server.init(oaa::Cryptor::Role::Server);
        QVERIFY(driveHandshake(client, server));

        QByteArray plaintext("Hello AA");
        QByteArray ciphertext = client.encrypt(plaintext);

        // Ciphertext must differ from plaintext (TLS overhead)
        QVERIFY(ciphertext != plaintext);
        QVERIFY(ciphertext.size() > plaintext.size());

        QByteArray decrypted = server.decrypt(ciphertext, ciphertext.size());
        QCOMPARE(decrypted, plaintext);
    }

    void testLargePayload() {
        oaa::Cryptor client, server;
        client.init(oaa::Cryptor::Role::Client);
        server.init(oaa::Cryptor::Role::Server);
        QVERIFY(driveHandshake(client, server));

        // 50000-byte payload
        QByteArray payload(50000, '\0');
        for (int i = 0; i < payload.size(); ++i)
            payload[i] = static_cast<char>(i & 0xFF);

        QByteArray ciphertext = client.encrypt(payload);
        QVERIFY(!ciphertext.isEmpty());

        QByteArray decrypted = server.decrypt(ciphertext, ciphertext.size());
        QCOMPARE(decrypted, payload);
    }

    void testMultipleMessages() {
        oaa::Cryptor client, server;
        client.init(oaa::Cryptor::Role::Client);
        server.init(oaa::Cryptor::Role::Server);
        QVERIFY(driveHandshake(client, server));

        QByteArray msg1("First message");
        QByteArray msg2("Second message with more data");
        QByteArray msg3("Third");

        // Encrypt and decrypt each independently, preserving order
        QByteArray ct1 = client.encrypt(msg1);
        QByteArray dec1 = server.decrypt(ct1, ct1.size());
        QCOMPARE(dec1, msg1);

        QByteArray ct2 = client.encrypt(msg2);
        QByteArray dec2 = server.decrypt(ct2, ct2.size());
        QCOMPARE(dec2, msg2);

        QByteArray ct3 = client.encrypt(msg3);
        QByteArray dec3 = server.decrypt(ct3, ct3.size());
        QCOMPARE(dec3, msg3);
    }

    void testDeinit() {
        oaa::Cryptor cryptor;
        cryptor.init(oaa::Cryptor::Role::Client);
        QVERIFY(!cryptor.isActive());
        cryptor.deinit();
        QVERIFY(!cryptor.isActive());
        // Should be safe to call deinit again
        cryptor.deinit();
    }
};

QTEST_MAIN(TestCryptor)
#include "test_cryptor.moc"
