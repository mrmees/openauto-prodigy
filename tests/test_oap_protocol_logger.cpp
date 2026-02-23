#include <QTest>
#include <QTemporaryFile>
#include <QFile>
#include "core/aa/ProtocolLogger.hpp"
#include <oaa/Channel/ChannelId.hpp>

class TestProtocolLogger : public QObject {
    Q_OBJECT
private slots:
    void testChannelNames() {
        using namespace oaa::ChannelId;
        QCOMPARE(QString::fromStdString(oap::aa::ProtocolLogger::channelName(Control)), "CONTROL");
        QCOMPARE(QString::fromStdString(oap::aa::ProtocolLogger::channelName(Video)), "VIDEO");
        QCOMPARE(QString::fromStdString(oap::aa::ProtocolLogger::channelName(MediaAudio)), "MEDIA_AUDIO");
        QCOMPARE(QString::fromStdString(oap::aa::ProtocolLogger::channelName(Input)), "INPUT");
        QCOMPARE(QString::fromStdString(oap::aa::ProtocolLogger::channelName(Sensor)), "SENSOR");
        QCOMPARE(QString::fromStdString(oap::aa::ProtocolLogger::channelName(Bluetooth)), "BLUETOOTH");
        QCOMPARE(QString::fromStdString(oap::aa::ProtocolLogger::channelName(WiFi)), "WIFI");
        QCOMPARE(QString::fromStdString(oap::aa::ProtocolLogger::channelName(AVInput)), "AV_INPUT");
        QVERIFY(oap::aa::ProtocolLogger::channelName(99).find("UNKNOWN") != std::string::npos);
    }

    void testControlMessageNames() {
        using oap::aa::ProtocolLogger;
        QCOMPARE(QString::fromStdString(ProtocolLogger::messageName(0, 0x0001)), "VERSION_REQUEST");
        QCOMPARE(QString::fromStdString(ProtocolLogger::messageName(0, 0x0006)), "SERVICE_DISCOVERY_RESPONSE");
        QCOMPARE(QString::fromStdString(ProtocolLogger::messageName(0, 0x000f)), "SHUTDOWN_REQUEST");
    }

    void testAVMessageNames() {
        using oap::aa::ProtocolLogger;
        QCOMPARE(QString::fromStdString(ProtocolLogger::messageName(oaa::ChannelId::Video, 0x0000)),
                 "AV_MEDIA_WITH_TIMESTAMP");
        QCOMPARE(QString::fromStdString(ProtocolLogger::messageName(oaa::ChannelId::MediaAudio, 0x8001)),
                 "AV_START_INDICATION");
        QCOMPARE(QString::fromStdString(ProtocolLogger::messageName(oaa::ChannelId::AVInput, 0x8005)),
                 "AV_INPUT_OPEN_REQUEST");
    }

    void testUniversalChannelOpen() {
        using oap::aa::ProtocolLogger;
        // CHANNEL_OPEN_REQUEST/RESPONSE are universal across all channels
        QCOMPARE(QString::fromStdString(ProtocolLogger::messageName(oaa::ChannelId::Input, 0x0007)),
                 "CHANNEL_OPEN_REQUEST");
        QCOMPARE(QString::fromStdString(ProtocolLogger::messageName(oaa::ChannelId::Sensor, 0x0008)),
                 "CHANNEL_OPEN_RESPONSE");
    }

    void testLogWritesToFile() {
        QTemporaryFile tmp;
        tmp.setAutoRemove(true);
        QVERIFY(tmp.open());
        QString path = tmp.fileName();
        tmp.close();

        auto& logger = oap::aa::ProtocolLogger::instance();
        logger.open(path.toStdString());

        uint8_t payload[] = {0x01, 0x02, 0x03};
        logger.log("Phone->HU", oaa::ChannelId::Control, 0x0005, payload, 3);
        logger.close();

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QString content = f.readAll();
        QVERIFY(content.contains("Phone->HU"));
        QVERIFY(content.contains("CONTROL"));
        QVERIFY(content.contains("SERVICE_DISCOVERY_REQUEST"));
        QVERIFY(content.contains("01 02 03"));
    }

    void testUnknownMessageIdFormatted() {
        using oap::aa::ProtocolLogger;
        QString name = QString::fromStdString(ProtocolLogger::messageName(oaa::ChannelId::Input, 0xFFFF));
        QCOMPARE(name, "0xffff");
    }
};

QTEST_MAIN(TestProtocolLogger)
#include "test_oap_protocol_logger.moc"
