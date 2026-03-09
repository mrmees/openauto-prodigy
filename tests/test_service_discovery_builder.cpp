#include <QTest>
#include "core/aa/ServiceDiscoveryBuilder.hpp"
#include "core/YamlConfig.hpp"

// oaa proto headers
#include "oaa/control/ChannelDescriptorData.pb.h"
#include "oaa/input/InputChannelData.pb.h"
#include "oaa/input/TouchConfigData.pb.h"
#include "oaa/video/AdditionalVideoConfigData.pb.h"

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
                // Night, driving, parking brake = 3 sensors (only what we can populate)
                QCOMPARE(desc.sensor_channel().sensors_size(), 3);
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

    void testNavbarBottomMargins() {
        // Bottom navbar: 1024x600 display, 56px bar, 720p video
        // Effective viewport: 1024x544
        // screenRatio = 1024/544 = 1.8824
        // videoRatio = 1280/720 = 1.7778
        // screenRatio > videoRatio -> marginH = round(720 - 1280/1.8824) = round(720 - 680) = 40
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", true);
        config.setValueByPath("navbar.edge", QString("bottom"));

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        // Find video channel (id=3) and check margins
        for (const auto& ch : sessionConfig.channels) {
            if (ch.channelId == 3) {
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(), ch.descriptor.size()));
                QVERIFY(desc.has_av_channel());
                QVERIFY(desc.av_channel().video_configs_size() > 0);
                auto& vc = desc.av_channel().video_configs(0);
                // marginW should be 0 (horizontal navbar doesn't affect width)
                QCOMPARE(vc.margin_width(), 0);
                // marginH should be positive (viewport is shorter than video aspect)
                QVERIFY2(vc.margin_height() > 0, "Bottom navbar should produce positive marginH");
                return;
            }
        }
        QFAIL("Video channel not found");
    }

    void testNavbarLeftMargins() {
        // Left navbar: 1024x600 display, 56px bar
        // Effective viewport: 968x600
        // screenRatio = 968/600 = 1.6133
        // videoRatio = 1280/720 = 1.7778
        // screenRatio < videoRatio -> marginW = round(1280 - 720*1.6133) = round(1280 - 1161.6) = 118
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", true);
        config.setValueByPath("navbar.edge", QString("left"));

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        for (const auto& ch : sessionConfig.channels) {
            if (ch.channelId == 3) {
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(), ch.descriptor.size()));
                auto& vc = desc.av_channel().video_configs(0);
                QVERIFY2(vc.margin_width() > 0, "Left navbar should produce positive marginW");
                QCOMPARE(vc.margin_height(), 0);
                return;
            }
        }
        QFAIL("Video channel not found");
    }

    void testNavbarDisabledStillHasDisplayMargins() {
        // Even without navbar, margins match display aspect ratio (1024x600)
        // so the video fills the viewport with no letterbox bars.
        // 1024/600 = 1.707 < 1280/720 = 1.778 → X margin
        // marginW = round(1280 - 720 * 1024/600) = round(1280 - 1229) = 51
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", false);
        config.setValueByPath("navbar.edge", QString("bottom"));

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        for (const auto& ch : sessionConfig.channels) {
            if (ch.channelId == 3) {
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(), ch.descriptor.size()));
                auto& vc = desc.av_channel().video_configs(0);
                QCOMPARE(vc.margin_width(), 51);
                QCOMPARE(vc.margin_height(), 0);
                return;
            }
        }
        QFAIL("Video channel not found");
    }

    void testInputDescriptorUsesNavbarDimensions() {
        // touch_screen_config dimensions should account for navbar inset
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", true);
        config.setValueByPath("navbar.edge", QString("bottom"));

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        // Find input channel (id=1)
        for (const auto& ch : sessionConfig.channels) {
            if (ch.channelId == 1) {
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(), ch.descriptor.size()));
                QVERIFY(desc.has_input_channel());
                QVERIFY(desc.input_channel().touch_screen_configs_size() > 0);
                auto& tc = desc.input_channel().touch_screen_configs(0);
                // With margins, touch dimensions should be less than full video res
                // (because margins reduce the content area)
                QVERIFY2(tc.width() <= 1280, "Touch width should be <= video width");
                QVERIFY2(tc.height() < 720, "Touch height should be < video height with bottom navbar");
                return;
            }
        }
        QFAIL("Input channel not found");
    }

    void testHiddenUiElementsWhenNavbarEnabled() {
        // Default: navbar.show_during_aa is true (or unset)
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", true);
        config.setValueByPath("navbar.edge", QString("bottom"));

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        for (const auto& ch : sessionConfig.channels) {
            if (ch.channelId == 3) {
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(), ch.descriptor.size()));
                QVERIFY(desc.has_av_channel());
                QVERIFY(desc.av_channel().video_configs_size() > 0);
                auto& vc = desc.av_channel().video_configs(0);
                QVERIFY(vc.has_additional_config());
                QCOMPARE(vc.additional_config().hidden_ui_elements_size(), 3);
                QCOMPARE(vc.additional_config().hidden_ui_elements(0),
                         oaa::proto::data::UI_ELEMENT_CLOCK);
                QCOMPARE(vc.additional_config().hidden_ui_elements(1),
                         oaa::proto::data::UI_ELEMENT_BATTERY_LEVEL);
                QCOMPARE(vc.additional_config().hidden_ui_elements(2),
                         oaa::proto::data::UI_ELEMENT_PHONE_SIGNAL);
                return;
            }
        }
        QFAIL("Video channel not found");
    }

    void testHiddenUiElementsDefaultUnset() {
        // When navbar.show_during_aa is not set, default is true
        oap::YamlConfig config;
        // Don't set navbar.show_during_aa — should default to true
        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        for (const auto& ch : sessionConfig.channels) {
            if (ch.channelId == 3) {
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(), ch.descriptor.size()));
                auto& vc = desc.av_channel().video_configs(0);
                QVERIFY2(vc.has_additional_config(),
                         "Should have hidden_ui_elements when navbar.show_during_aa is unset (default true)");
                QCOMPARE(vc.additional_config().hidden_ui_elements_size(), 3);
                return;
            }
        }
        QFAIL("Video channel not found");
    }

    void testNoHiddenUiElementsWhenNavbarDisabled() {
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", false);

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        for (const auto& ch : sessionConfig.channels) {
            if (ch.channelId == 3) {
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(), ch.descriptor.size()));
                auto& vc = desc.av_channel().video_configs(0);
                // Should NOT have additional_config when navbar is disabled
                if (vc.has_additional_config()) {
                    QCOMPARE(vc.additional_config().hidden_ui_elements_size(), 0);
                }
                return;
            }
        }
        QFAIL("Video channel not found");
    }

    void testHiddenUiElementsNoUiTheme() {
        // Verify no ui_theme field is set (critical — breaks margins)
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", true);

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        for (const auto& ch : sessionConfig.channels) {
            if (ch.channelId == 3) {
                oaa::proto::data::ChannelDescriptor desc;
                QVERIFY(desc.ParseFromArray(ch.descriptor.constData(), ch.descriptor.size()));
                auto& vc = desc.av_channel().video_configs(0);
                QVERIFY(vc.has_additional_config());
                // ui_theme should NOT be set
                QVERIFY2(!vc.additional_config().has_ui_theme(),
                         "ui_theme must NOT be set — it breaks margins");
                return;
            }
        }
        QFAIL("Video channel not found");
    }

    void testWifiChannelHasBssid() {
        // BSSID should be the wlan0 MAC address, not the SSID
        oap::aa::ServiceDiscoveryBuilder builder(nullptr, "00:00:00:00:00:00",
                                                  "TestSSID", "TestPass",
                                                  "AA:BB:CC:DD:EE:FF");
        auto config = builder.build();

        for (const auto& ch : config.channels) {
            if (ch.channelId == 14) { // WiFi
                oaa::proto::data::ChannelDescriptor desc;
                desc.ParseFromArray(ch.descriptor.constData(),
                                    ch.descriptor.size());
                QVERIFY(desc.has_wifi_channel());
                QCOMPARE(QString::fromStdString(desc.wifi_channel().bssid()),
                         QString("AA:BB:CC:DD:EE:FF"));
                return;
            }
        }
        QFAIL("WiFi channel not found");
    }
};

QTEST_MAIN(TestServiceDiscoveryBuilder)
#include "test_service_discovery_builder.moc"
