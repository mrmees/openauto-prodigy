#include <QTest>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

class TestChannelId : public QObject {
    Q_OBJECT
private slots:
    void testChannelIdConstants() {
        QCOMPARE(oaa::ChannelId::Control,     static_cast<uint8_t>(0));
        QCOMPARE(oaa::ChannelId::Input,       static_cast<uint8_t>(1));
        QCOMPARE(oaa::ChannelId::Sensor,      static_cast<uint8_t>(2));
        QCOMPARE(oaa::ChannelId::Video,       static_cast<uint8_t>(3));
        QCOMPARE(oaa::ChannelId::MediaAudio,  static_cast<uint8_t>(4));
        QCOMPARE(oaa::ChannelId::SpeechAudio, static_cast<uint8_t>(5));
        QCOMPARE(oaa::ChannelId::SystemAudio, static_cast<uint8_t>(6));
        QCOMPARE(oaa::ChannelId::AVInput,     static_cast<uint8_t>(7));
        QCOMPARE(oaa::ChannelId::Bluetooth,   static_cast<uint8_t>(8));
        QCOMPARE(oaa::ChannelId::WiFi,        static_cast<uint8_t>(14));
    }

    void testMessageIdConstants() {
        // AV channel
        QCOMPARE(oaa::AVMessageId::SETUP_REQUEST,          static_cast<uint16_t>(0x8000));
        QCOMPARE(oaa::AVMessageId::START_INDICATION,       static_cast<uint16_t>(0x8001));
        QCOMPARE(oaa::AVMessageId::SETUP_RESPONSE,         static_cast<uint16_t>(0x8003));
        QCOMPARE(oaa::AVMessageId::ACK_INDICATION,         static_cast<uint16_t>(0x8004));
        QCOMPARE(oaa::AVMessageId::VIDEO_FOCUS_REQUEST,    static_cast<uint16_t>(0x8007));

        // Sensor
        QCOMPARE(oaa::SensorMessageId::SENSOR_START_REQUEST,    static_cast<uint16_t>(0x8001));
        QCOMPARE(oaa::SensorMessageId::SENSOR_EVENT_INDICATION, static_cast<uint16_t>(0x8003));

        // WiFi
        QCOMPARE(oaa::WiFiMessageId::CREDENTIALS_REQUEST,  static_cast<uint16_t>(0x8001));
    }
};

QTEST_MAIN(TestChannelId)
#include "test_channel_id.moc"
