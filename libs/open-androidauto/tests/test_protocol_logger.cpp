#include <QtTest/QtTest>
#include <oaa/Messenger/ProtocolLogger.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <fstream>
#include <string>

class TestProtocolLogger : public QObject {
    Q_OBJECT

private slots:
    void testChannelNames()
    {
        QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::Control), std::string("CONTROL"));
        QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::Video), std::string("VIDEO"));
        QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::MediaAudio), std::string("MEDIA_AUDIO"));
        QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::Navigation), std::string("NAVIGATION"));
        QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::MediaStatus), std::string("MEDIA_STATUS"));
        QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::PhoneStatus), std::string("PHONE_STATUS"));
        QCOMPARE(oaa::ProtocolLogger::channelName(oaa::ChannelId::WiFi), std::string("WIFI"));
        QCOMPARE(oaa::ProtocolLogger::channelName(99), std::string("UNKNOWN(99)"));
    }

    void testMessageNames()
    {
        // Control channel
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::Control, 0x0001),
                 std::string("VERSION_REQUEST"));
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::Control, 0x000f),
                 std::string("SHUTDOWN_REQUEST"));

        // Universal channel open
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::Video, 0x0007),
                 std::string("CHANNEL_OPEN_REQUEST"));

        // AV channel
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::Video, oaa::AVMessageId::SETUP_REQUEST),
                 std::string("AV_SETUP_REQUEST"));

        // Sensor channel
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::Sensor, oaa::SensorMessageId::SENSOR_START_REQUEST),
                 std::string("SENSOR_START_REQUEST"));

        // Navigation/media/phone status channels
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::Navigation, oaa::NavigationMessageId::NAV_STEP),
                 std::string("NAVIGATION_NOTIFICATION"));
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::Navigation, oaa::NavigationMessageId::NAV_DISTANCE),
                 std::string("NAVIGATION_DISTANCE"));
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::MediaStatus, oaa::MediaStatusMessageId::PLAYBACK_STATUS),
                 std::string("MEDIA_PLAYBACK_STATUS"));
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::MediaStatus, oaa::MediaStatusMessageId::PLAYBACK_METADATA),
                 std::string("MEDIA_PLAYBACK_METADATA"));
        QCOMPARE(oaa::ProtocolLogger::messageName(oaa::ChannelId::PhoneStatus, oaa::PhoneStatusMessageId::PHONE_STATUS),
                 std::string("PHONE_STATUS_UPDATE"));

        // Unknown
        auto name = oaa::ProtocolLogger::messageName(oaa::ChannelId::Input, 0xFFFF);
        QVERIFY(name.find("0xffff") != std::string::npos);
    }

    void testFileOutput()
    {
        std::string path = "/tmp/test_protocol_logger.tsv";

        oaa::ProtocolLogger logger;
        logger.open(path);
        QVERIFY(logger.isOpen());

        uint8_t payload[] = {0x01, 0x02, 0x03};
        logger.log("HU->Phone", oaa::ChannelId::Control, 0x0006,
                   payload, sizeof(payload));

        logger.close();
        QVERIFY(!logger.isOpen());

        // Read back and verify
        std::ifstream f(path);
        QVERIFY(f.is_open());

        std::string headerLine, dataLine;
        std::getline(f, headerLine);
        std::getline(f, dataLine);

        QVERIFY(headerLine.find("TIME") != std::string::npos);
        QVERIFY(dataLine.find("HU->Phone") != std::string::npos);
        QVERIFY(dataLine.find("CONTROL") != std::string::npos);
        QVERIFY(dataLine.find("SERVICE_DISCOVERY_RESPONSE") != std::string::npos);
        QVERIFY(dataLine.find("01 02 03") != std::string::npos);

        std::remove(path.c_str());
    }

    void testDataMessageSuppression()
    {
        std::string path = "/tmp/test_protocol_logger_data.tsv";

        oaa::ProtocolLogger logger;
        logger.open(path);

        // Video data message should show "[video data]" not hex
        uint8_t payload[100] = {};
        logger.log("Phone->HU", oaa::ChannelId::Video,
                   oaa::AVMessageId::AV_MEDIA_WITH_TIMESTAMP,
                   payload, sizeof(payload));

        logger.close();

        std::ifstream f(path);
        std::string headerLine, dataLine;
        std::getline(f, headerLine);
        std::getline(f, dataLine);

        QVERIFY(dataLine.find("[video data]") != std::string::npos);

        std::remove(path.c_str());
    }

    void testJsonlOutput()
    {
        std::string path = "/tmp/test_protocol_logger.jsonl";

        oaa::ProtocolLogger logger;
        logger.setFormat(oaa::ProtocolLogger::OutputFormat::Jsonl);
        logger.setIncludeMedia(false);
        logger.open(path);

        uint8_t payload[] = {0x08, 0x01};
        logger.log("Phone->HU", oaa::ChannelId::Control, 0x000B, payload, sizeof(payload));
        logger.close();

        std::ifstream f(path);
        QVERIFY(f.is_open());

        std::string line;
        QVERIFY(std::getline(f, line));
        QVERIFY(line.find("\"direction\":\"Phone->HU\"") != std::string::npos);
        QVERIFY(line.find("\"channel_id\":0") != std::string::npos);
        QVERIFY(line.find("\"message_id\":11") != std::string::npos);
        QVERIFY(line.find("\"message_name\":\"PING_REQUEST\"") != std::string::npos);
        QVERIFY(line.find("\"payload_hex\":\"0801\"") != std::string::npos);

        std::string extra;
        QVERIFY(!std::getline(f, extra));

        std::remove(path.c_str());
    }

    void testJsonlSkipsMediaWhenDisabled()
    {
        std::string path = "/tmp/test_protocol_logger_jsonl_media.jsonl";

        oaa::ProtocolLogger logger;
        logger.setFormat(oaa::ProtocolLogger::OutputFormat::Jsonl);
        logger.setIncludeMedia(false);
        logger.open(path);

        uint8_t payload[] = {0x00, 0x01, 0x02};
        logger.log("Phone->HU", oaa::ChannelId::Video, oaa::AVMessageId::AV_MEDIA_INDICATION,
                   payload, sizeof(payload));
        logger.close();

        std::ifstream f(path);
        QVERIFY(f.is_open());

        std::string line;
        QVERIFY(!std::getline(f, line));

        std::remove(path.c_str());
    }
};

QTEST_MAIN(TestProtocolLogger)
#include "test_protocol_logger.moc"
