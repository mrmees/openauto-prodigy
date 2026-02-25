#include <QTest>
#include "core/aa/ServiceDiscoveryBuilder.hpp"

// oaa proto headers
#include "ChannelDescriptorData.pb.h"

class TestServiceDiscoveryBuilder : public QObject {
    Q_OBJECT
private slots:
    void testDefaultBuildProducesAllChannels() {
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();

        // Should have 12 channels: video, media, speech, system, input,
        // sensor, bluetooth, wifi, avinput, navigation, media status, phone status
        QCOMPARE(config.channels.size(), 12);
    }

    void testVideoChannelDescriptor() {
        oap::aa::ServiceDiscoveryBuilder builder;
        auto config = builder.build();

        // Find video channel (id=3)
        bool found = false;
        for (const auto& ch : config.channels) {
            if (ch.channelId == 3) {
                found = true;
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(),
                                            ch.descriptor.size()));
                QCOMPARE(desc.channel_id(), 3u);
                QVERIFY(desc.has_av_channel());
                QCOMPARE(desc.av_channel().stream_type(),
                         static_cast<int>(oaa::proto::enums::AVStreamType::VIDEO));
                break;
            }
        }
        QVERIFY(found);
    }

    void testSensorChannelHasExpectedTypes() {
        oap::aa::ServiceDiscoveryBuilder builder;
        auto config = builder.build();

        for (const auto& ch : config.channels) {
            if (ch.channelId == 2) { // Sensor
                oaa::proto::data::ChannelDescriptor desc;
                desc.ParseFromArray(ch.descriptor.constData(),
                                    ch.descriptor.size());
                QVERIFY(desc.has_sensor_channel());
                // Night, driving, location, compass, accel, gyro = 6 sensors
                QCOMPARE(desc.sensor_channel().sensors_size(), 6);
                return;
            }
        }
        QFAIL("Sensor channel not found");
    }

    void testAudioChannels() {
        oap::aa::ServiceDiscoveryBuilder builder;
        auto config = builder.build();

        int audioChannelCount = 0;
        for (const auto& ch : config.channels) {
            if (ch.channelId == 4 || ch.channelId == 5 || ch.channelId == 6) {
                oaa::proto::data::ChannelDescriptor desc;
                desc.ParseFromArray(ch.descriptor.constData(),
                                    ch.descriptor.size());
                QVERIFY(desc.has_av_channel());
                QCOMPARE(desc.av_channel().stream_type(),
                         static_cast<int>(oaa::proto::enums::AVStreamType::AUDIO));
                audioChannelCount++;
            }
        }
        QCOMPARE(audioChannelCount, 3); // media, speech, system
    }

    void testWifiChannelHasSsid() {
        oap::aa::ServiceDiscoveryBuilder builder(nullptr, "00:00:00:00:00:00",
                                                  "TestSSID", "TestPass");
        auto config = builder.build();

        for (const auto& ch : config.channels) {
            if (ch.channelId == 14) { // WiFi
                oaa::proto::data::ChannelDescriptor desc;
                desc.ParseFromArray(ch.descriptor.constData(),
                                    ch.descriptor.size());
                QVERIFY(desc.has_wifi_channel());
                QCOMPARE(QString::fromStdString(desc.wifi_channel().ssid()),
                         QString("TestSSID"));
                return;
            }
        }
        QFAIL("WiFi channel not found");
    }
};

QTEST_MAIN(TestServiceDiscoveryBuilder)
#include "test_service_discovery_builder.moc"
