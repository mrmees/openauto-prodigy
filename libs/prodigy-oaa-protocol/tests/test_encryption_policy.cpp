#include <QtTest/QtTest>

#include <oaa/Messenger/EncryptionPolicy.hpp>

class TestEncryptionPolicy : public QObject {
    Q_OBJECT

private slots:
    void testPreSslAlwaysPlain()
    {
        oaa::EncryptionPolicy policy;

        // Nothing should be encrypted before SSL is active
        QVERIFY(!policy.shouldEncrypt(0, 0x0001, false)); // VERSION_REQUEST
        QVERIFY(!policy.shouldEncrypt(0, 0x0005, false)); // SERVICE_DISCOVERY
        QVERIFY(!policy.shouldEncrypt(1, 0x8001, false)); // INPUT_EVENT on ch1
        QVERIFY(!policy.shouldEncrypt(3, 0x8000, false)); // VIDEO_SETUP on ch3
        QVERIFY(!policy.shouldEncrypt(5, 0x0000, false)); // arbitrary on ch5
    }

    void testPostSslControlExceptions()
    {
        oaa::EncryptionPolicy policy;

        // These channel-0 control messages stay plaintext even after SSL
        QVERIFY(!policy.shouldEncrypt(0, 0x0001, true)); // VERSION_REQUEST
        QVERIFY(!policy.shouldEncrypt(0, 0x0002, true)); // VERSION_RESPONSE
        QVERIFY(!policy.shouldEncrypt(0, 0x0003, true)); // SSL_HANDSHAKE
        QVERIFY(!policy.shouldEncrypt(0, 0x0004, true)); // AUTH_COMPLETE
        QVERIFY(!policy.shouldEncrypt(0, 0x000b, true)); // PING_REQUEST
        QVERIFY(!policy.shouldEncrypt(0, 0x000c, true)); // PING_RESPONSE
    }

    void testPostSslNormalEncrypted()
    {
        oaa::EncryptionPolicy policy;

        // Normal messages on channel 0 should be encrypted post-SSL
        QVERIFY(policy.shouldEncrypt(0, 0x0005, true)); // SERVICE_DISCOVERY_REQUEST
        QVERIFY(policy.shouldEncrypt(0, 0x0006, true)); // SERVICE_DISCOVERY_RESPONSE
        QVERIFY(policy.shouldEncrypt(0, 0x0007, true)); // CHANNEL_OPEN_REQUEST

        // Messages on data channels should be encrypted post-SSL
        QVERIFY(policy.shouldEncrypt(3, 0x8000, true)); // VIDEO_SETUP on ch3
        QVERIFY(policy.shouldEncrypt(1, 0x8001, true)); // INPUT_EVENT on ch1
    }

    void testNonControlChannelAlwaysEncryptedPostSsl()
    {
        oaa::EncryptionPolicy policy;

        // Any messageId on channels 1-8 should be encrypted when SSL is active
        for (uint8_t ch = 1; ch <= 8; ++ch) {
            QVERIFY(policy.shouldEncrypt(ch, 0x0001, true));
            QVERIFY(policy.shouldEncrypt(ch, 0x0003, true));
            QVERIFY(policy.shouldEncrypt(ch, 0x000b, true));
            QVERIFY(policy.shouldEncrypt(ch, 0x8000, true));
        }
    }
};

QTEST_MAIN(TestEncryptionPolicy)
#include "test_encryption_policy.moc"
