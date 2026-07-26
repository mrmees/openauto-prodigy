#include <QTest>
#include <oaa/Session/SessionConfig.hpp>

class TestSessionConfig : public QObject {
    Q_OBJECT
private slots:
    void testDefaultsHaveNoChannels() {
        oaa::SessionConfig config;
        QVERIFY(config.channels.isEmpty());
        QCOMPARE(config.protocolMajor, uint16_t{1});
        QCOMPARE(config.protocolMinor, uint16_t{7});
        QVERIFY(!config.requireExactProtocolVersion);
        QCOMPARE(config.pingInterval, 5000);
        QCOMPARE(config.pingTimeout, 15000);
    }

    void testPingCadenceAndDeadlineAreIndependentSettings() {
        oaa::SessionConfig config;
        config.pingInterval = 25;
        config.pingTimeout = 180;

        QCOMPARE(config.pingInterval, 25);
        QCOMPARE(config.pingTimeout, 180);
    }

    void testAddVideoChannel() {
        oaa::SessionConfig config;
        oaa::ChannelConfig ch;
        ch.channelId = 1; // Video
        ch.descriptor = QByteArray("fake-descriptor");
        config.channels.append(ch);
        QCOMPARE(config.channels.size(), 1);
        QCOMPARE(config.channels[0].channelId, static_cast<uint8_t>(1));
    }
};

QTEST_MAIN(TestSessionConfig)
#include "test_session_config.moc"
