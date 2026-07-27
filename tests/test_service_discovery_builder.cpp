#include <QTest>
#include <QTemporaryFile>
#include "core/aa/ServiceDiscoveryBuilder.hpp"
#include "core/aa/ProjectedDisplayConfig.hpp"
#include "core/YamlConfig.hpp"

// oaa proto headers
#include "oaa/control/ChannelDescriptorData.pb.h"
#include "oaa/input/InputChannelData.pb.h"
#include "oaa/input/TouchConfigData.pb.h"
#include "oaa/video/AdditionalVideoConfigData.pb.h"
#include "oaa/video/VideoConfigData.pb.h"
#include "oaa/video/DisplayTypeEnum.pb.h"
#include "oaa/video/VideoFPSEnum.pb.h"
#include "oaa/video/VideoResolutionEnum.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"

namespace {

const oaa::ChannelConfig* channelById(const oaa::SessionConfig& config,
                                      uint8_t channelId)
{
    for (const auto& channel : config.channels) {
        if (channel.channelId == channelId)
            return &channel;
    }
    return nullptr;
}

oaa::proto::data::ChannelDescriptor descriptorById(
    const oaa::SessionConfig& config, uint8_t channelId)
{
    const auto* channel = channelById(config, channelId);
    if (!channel)
        return {};

    oaa::proto::data::ChannelDescriptor descriptor;
    descriptor.ParseFromArray(channel->descriptor.constData(),
                              channel->descriptor.size());
    return descriptor;
}

QList<uint8_t> channelIds(const oaa::SessionConfig& config)
{
    QList<uint8_t> ids;
    for (const auto& channel : config.channels)
        ids.append(channel.channelId);
    return ids;
}

void verifyCompanionInsets(
    const oaa::proto::data::AdditionalVideoConfig& additional,
    uint32_t marginWidth,
    uint32_t marginHeight)
{
    QVERIFY(additional.has_display_insets());
    const auto& insets = additional.display_insets();
    QVERIFY(insets.has_top());
    QVERIFY(insets.has_bottom());
    QVERIFY(insets.has_left());
    QVERIFY(insets.has_right());
    QCOMPARE(insets.left(), marginWidth / 2);
    QCOMPARE(insets.right(), marginWidth - marginWidth / 2);
    QCOMPARE(insets.top(), marginHeight / 2);
    QCOMPARE(insets.bottom(), marginHeight - marginHeight / 2);
    QCOMPARE(insets.left() + insets.right(), marginWidth);
    QCOMPARE(insets.top() + insets.bottom(), marginHeight);
    QVERIFY(!additional.has_field_2_insets());
    QVERIFY(!additional.has_field_3_insets());
    QVERIFY(!additional.has_ui_theme());
    QCOMPARE(additional.resize_actions_size(), 0);
    QCOMPARE(additional.margin_configs_size(), 0);
    QVERIFY(!additional.has_blended_ui_config());
}

QByteArray baseVideoConfigBytes(const oaa::proto::data::VideoConfig& source)
{
    oaa::proto::data::VideoConfig base = source;
    base.clear_additional_config();
    const std::string serialized = base.SerializeAsString();
    return QByteArray(serialized.data(), static_cast<qsizetype>(serialized.size()));
}

} // namespace

class TestServiceDiscoveryBuilder : public QObject {
    Q_OBJECT
private slots:
    void testVideoConfigCountMatchesDescriptor_data() {
        QTest::addColumn<QString>("codecYaml");
        QTest::addColumn<int>("expectedCount");
        QTest::newRow("empty-defaults") << QStringLiteral("[]") << 2;
        QTest::newRow("unknown-fallback") << QStringLiteral("[bogus]") << 1;
        QTest::newRow("one") << QStringLiteral("[h264]") << 1;
        QTest::newRow("two") << QStringLiteral("[h264, h265]") << 2;
        QTest::newRow("multiple") << QStringLiteral("[h264, h265, vp9, av1]") << 4;
    }

    void testVideoConfigCountMatchesDescriptor() {
        QFETCH(QString, codecYaml);
        QFETCH(int, expectedCount);

        QTemporaryFile file;
        QVERIFY(file.open());
        const QByteArray yaml = "video:\n  codecs: " + codecYaml.toUtf8() + "\n";
        QCOMPARE(file.write(yaml), yaml.size());
        file.flush();

        oap::YamlConfig yamlConfig;
        yamlConfig.load(file.fileName());
        oap::aa::ServiceDiscoveryBuilder builder(&yamlConfig);
        QCOMPARE(static_cast<int>(builder.videoConfigCount()), expectedCount);

        const auto config = builder.build();
        for (const auto& channel : config.channels) {
            if (channel.channelId != 3)
                continue;
            oaa::proto::data::ChannelDescriptor descriptor;
            QVERIFY(descriptor.ParseFromArray(channel.descriptor.constData(),
                                              channel.descriptor.size()));
            QCOMPARE(descriptor.av_channel().video_configs_size(), expectedCount);
            return;
        }
        QFAIL("Video descriptor missing");
    }

    void testDefaultBuildProducesAllChannels() {
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();

        // Should have 12 channels: video, media, speech, system, input,
        // sensor, bluetooth, wifi, avinput, navigation, media status, phone status
        QCOMPARE(config.channels.size(), 12);
    }

    void testVersionIdentityIsCompiledIn() {
        oap::aa::ServiceDiscoveryBuilder builder;
        auto config = builder.build();
        // Both fields are serialized into the AA ServiceDiscoveryResponse
        // (AASession.cpp set_sw_build/set_sw_version) — the phone logs them.
        QCOMPARE(config.swBuild, QString::fromLatin1(OAP_VERSION));
        QCOMPARE(config.swVersion, QString::fromLatin1(OAP_VERSION));
        QCOMPARE(config.protocolMajor, uint16_t{1});
        QCOMPARE(config.protocolMinor, uint16_t{7});
        QVERIFY(!config.protocolPolicy().requiresMinimumCompatibleResponse());
    }

    void galSelectionIsSessionWideWhenClusterIsDisabled() {
        oap::aa::ServiceDiscoveryBuilder legacyBuilder;
        legacyBuilder.setProtocolVersion(oaa::kGalVersion1_7);
        const auto legacy = legacyBuilder.build();

        oap::aa::ServiceDiscoveryBuilder modernBuilder;
        modernBuilder.setProtocolVersion(oaa::kGalVersion4_3);
        const auto modern = modernBuilder.build();

        QCOMPARE(modern.protocolMajor, uint16_t{4});
        QCOMPARE(modern.protocolMinor, uint16_t{3});
        QVERIFY(modern.protocolPolicy().requiresMinimumCompatibleResponse());
        QCOMPARE(modern.sessionConfiguration, legacy.sessionConfiguration);
        QCOMPARE(modern.channels.size(), legacy.channels.size());
        for (int i = 0; i < legacy.channels.size(); ++i) {
            QCOMPARE(modern.channels[i].channelId, legacy.channels[i].channelId);
            QCOMPARE(modern.channels[i].descriptor, legacy.channels[i].descriptor);
        }
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
                         oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP);
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
                QCOMPARE(desc.sensor_channel().sensors(0).sensor_type(),
                         oaa::proto::enums::SensorType::NIGHT_DATA);
                QCOMPARE(desc.sensor_channel().sensors(1).sensor_type(),
                         oaa::proto::enums::SensorType::DRIVING_STATUS);
                QCOMPARE(desc.sensor_channel().sensors(2).sensor_type(),
                         oaa::proto::enums::SensorType::PARKING_BRAKE);
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
                         oaa::proto::enums::MediaCodecType::MEDIA_CODEC_AUDIO_PCM);
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

    void testSessionConfigWhenNavbarEnabled() {
        // session_configuration bitmask should hide clock/signal/battery
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", true);
        config.setValueByPath("navbar.edge", QString("bottom"));

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        // 1|2|4 = hide clock, signal, battery
        // HIDE_CLOCK only — AA 16.2 forcibly keeps signal/battery when clock is hidden
        QCOMPARE(sessionConfig.sessionConfiguration, static_cast<int32_t>(1));
    }

    void testSessionConfigDefaultUnset() {
        // When navbar.show_during_aa is not set, default is true — should still hide clock
        oap::YamlConfig config;
        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        QCOMPARE(sessionConfig.sessionConfiguration, static_cast<int32_t>(1));
    }

    void testNoSessionConfigWhenNavbarDisabled() {
        oap::YamlConfig config;
        config.setValueByPath("navbar.show_during_aa", false);

        oap::aa::ServiceDiscoveryBuilder builder(&config);
        builder.setDisplayDimensions(1024, 600);
        auto sessionConfig = builder.build();

        // Should NOT set session_configuration when navbar is disabled
        QCOMPARE(sessionConfig.sessionConfiguration, static_cast<int32_t>(0));
    }

    void videoUiFeatureSerializationMatrix_data() {
        QTest::addColumn<int>("galMajor");
        QTest::addColumn<int>("galMinor");
        QTest::addColumn<bool>("navbarDuringAa");
        QTest::addColumn<bool>("nativeTurnCardAvailable");
        QTest::addColumn<int>("expectedSessionMask");
        QTest::addColumn<int>("expectedMainElement");
        QTest::addColumn<int>("expectedClusterElement");

        QTest::newRow("1.7-navbar-on")
            << 1 << 7 << true << false << 1 << -1 << -1;
        QTest::newRow("1.7-navbar-off")
            << 1 << 7 << false << false << 0 << -1 << -1;
        QTest::newRow("4.3-navbar-on-no-native-turn")
            << 4 << 3 << true << false << 0
            << static_cast<int>(oaa::proto::data::UI_ELEMENT_CLOCK) << -1;
        QTest::newRow("4.3-navbar-off-no-native-turn")
            << 4 << 3 << false << false << 0 << -1 << -1;
        QTest::newRow("4.3-navbar-off-native-turn")
            << 4 << 3 << false << true << 0 << -1
            << static_cast<int>(
                   oaa::proto::data::UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE);
        QTest::newRow("4.3-navbar-on-native-turn")
            << 4 << 3 << true << true << 0
            << static_cast<int>(oaa::proto::data::UI_ELEMENT_CLOCK)
            << static_cast<int>(
                   oaa::proto::data::UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE);
    }

    void videoUiFeatureSerializationMatrix() {
        QFETCH(int, galMajor);
        QFETCH(int, galMinor);
        QFETCH(bool, navbarDuringAa);
        QFETCH(bool, nativeTurnCardAvailable);
        QFETCH(int, expectedSessionMask);
        QFETCH(int, expectedMainElement);
        QFETCH(int, expectedClusterElement);

        oap::YamlConfig yaml;
        yaml.setValueByPath("navbar.show_during_aa", navbarDuringAa);

        oap::aa::ProjectedClusterConfig cluster;
        cluster.enabled = true;
        cluster.profile.nativeTurnCardAvailable = nativeTurnCardAvailable;

        oap::aa::ServiceDiscoveryBuilder builder(&yaml);
        builder.setProtocolVersion({static_cast<uint16_t>(galMajor),
                                    static_cast<uint16_t>(galMinor)});
        builder.setProjectedClusterConfig(cluster);
        const auto session = builder.build();

        QCOMPARE(session.sessionConfiguration, expectedSessionMask);
        QCOMPARE(session.sessionConfiguration & 16, 0);

        const auto main = descriptorById(session, 3).av_channel();
        QCOMPARE(main.video_configs_size(), 2);
        for (const auto& video : main.video_configs()) {
            QCOMPARE(video.has_additional_config(), expectedMainElement >= 0);
            if (!video.has_additional_config())
                continue;
            const auto& additional = video.additional_config();
            verifyCompanionInsets(
                additional, video.margin_width(), video.margin_height());
            QCOMPARE(additional.hidden_ui_elements_size(), 1);
            QCOMPARE(static_cast<int>(additional.hidden_ui_elements(0)),
                     expectedMainElement);
        }

        const auto clusterVideo = descriptorById(session, 12).av_channel();
        QCOMPARE(clusterVideo.video_configs_size(), 1);
        QVERIFY(!clusterVideo.has_keycode());
        const auto& video = clusterVideo.video_configs(0);
        QCOMPARE(video.has_additional_config(), expectedClusterElement >= 0);
        if (video.has_additional_config()) {
            const auto& additional = video.additional_config();
            verifyCompanionInsets(
                additional, video.margin_width(), video.margin_height());
            QCOMPARE(additional.hidden_ui_elements_size(), 1);
            QCOMPARE(static_cast<int>(additional.hidden_ui_elements(0)),
                     expectedClusterElement);
        }
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

    void disabledClusterPreservesLegacyMainDescriptorBytes() {
        static const QByteArray legacyVideo = QByteArray::fromHex(
            "08031a220803220d0802100218002028288c015003"
            "220d0802100218002028288c0150074803");
        static const QByteArray legacyInput = QByteArray::fromHex(
            "080122170a0d030454555657587e7fdb01e701120608800a10a805");

        oap::aa::ServiceDiscoveryBuilder implicitDefault;
        const auto implicitConfig = implicitDefault.build();
        QCOMPARE(channelById(implicitConfig, 3)->descriptor, legacyVideo);
        QCOMPARE(channelById(implicitConfig, 1)->descriptor, legacyInput);

        oap::aa::ServiceDiscoveryBuilder explicitDisabled;
        explicitDisabled.setProjectedClusterConfig({false, {}});
        const auto config = explicitDisabled.build();
        QCOMPARE(channelIds(config),
                 QList<uint8_t>({3, 4, 5, 6, 1, 2, 8, 14, 7, 9, 10, 11}));
        QCOMPARE(channelById(config, 3)->descriptor, legacyVideo);
        QCOMPARE(channelById(config, 1)->descriptor, legacyInput);

        const auto video = descriptorById(config, 3).av_channel();
        QVERIFY(!video.has_display_id());
        QVERIFY(!video.has_display_type());
        const auto input = descriptorById(config, 1).input_channel();
        QVERIFY(!input.has_display_id());
        QVERIFY(!input.touch_screen_configs(0).has_display_type());
    }

    void enabledClusterAdvertisesPairedFixedTopology() {
        oap::aa::ServiceDiscoveryBuilder builder;
        builder.setProjectedClusterConfig({true, {}});
        const auto config = builder.build();

        const auto mainVideo = descriptorById(config, 3).av_channel();
        QCOMPARE(mainVideo.display_id(), 0u);
        QCOMPARE(mainVideo.display_type(),
                 oaa::proto::enums::DisplayType::MAIN);

        const auto mainInput = descriptorById(config, 1).input_channel();
        QCOMPARE(mainInput.display_id(), 0u);
        QCOMPARE(mainInput.touch_screen_configs(0).display_type(),
                 oaa::proto::enums::DisplayType::MAIN);

        const auto clusterVideo = descriptorById(config, 12).av_channel();
        QCOMPARE(clusterVideo.display_id(), 1u);
        QCOMPARE(clusterVideo.display_type(),
                 oaa::proto::enums::DisplayType::CLUSTER);
        QVERIFY(!clusterVideo.has_keycode());
        QCOMPARE(clusterVideo.video_configs_size(), 1);
        const auto& videoConfig = clusterVideo.video_configs(0);
        QCOMPARE(videoConfig.video_resolution(),
                 oaa::proto::enums::VideoResolution::VIDEO_800x480);
        QCOMPARE(videoConfig.video_fps(), oaa::proto::enums::VideoFPS::_30);
        QCOMPARE(videoConfig.codec(),
                 oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP);
        QCOMPARE(videoConfig.dpi(), 140u);
        QCOMPARE(videoConfig.margin_width(),
                 static_cast<uint32_t>(
                     oap::aa::kClusterViewportGeometry.marginWidth()));
        QCOMPARE(videoConfig.margin_height(),
                 static_cast<uint32_t>(
                     oap::aa::kClusterViewportGeometry.marginHeight()));

        const auto clusterInput = descriptorById(config, 13).input_channel();
        QCOMPARE(clusterInput.display_id(), 1u);
        QCOMPARE(clusterInput.touch_screen_configs_size(), 0);
        QCOMPARE(clusterInput.supported_keycodes_size(), 0);
        QCOMPARE(clusterInput.touchpad_configs_size(), 0);
        QCOMPARE(clusterInput.supported_haptic_types_size(), 0);
        QCOMPARE(builder.videoConfigCount(
                     oap::aa::ProjectedDisplayRole::Cluster), 1u);

        static const QByteArray pairedMainVideo = QByteArray::fromHex(
            "08031a260803220d0802100218002028288c015003"
            "220d0802100218002028288c015007300038004803");
        static const QByteArray pairedMainInput = QByteArray::fromHex(
            "0801221b0a0d030454555657587e7fdb01e701"
            "120808800a10a80518002800");
        static const QByteArray clusterVideoDescriptor = QByteArray::fromHex(
            "080c1a170803220f0801100218f40320b401288c01500330013801");
        static const QByteArray clusterInputDescriptor = QByteArray::fromHex(
            "080d22022801");
        QCOMPARE(channelById(config, 3)->descriptor, pairedMainVideo);
        QCOMPARE(channelById(config, 1)->descriptor, pairedMainInput);
        QCOMPARE(channelById(config, 12)->descriptor, clusterVideoDescriptor);
        QCOMPARE(channelById(config, 13)->descriptor, clusterInputDescriptor);
    }

    void runtimeClusterProfileDrivesDescriptorAndNativeTurnCardDeclaration() {
        oap::aa::ServiceDiscoveryBuilder builder;
        oap::aa::ProjectedClusterConfig clusterConfig;
        clusterConfig.enabled = true;
        clusterConfig.profile = {
            QStringLiteral("720p"), 160, 600, 400, true};
        builder.setProtocolVersion(oaa::kGalVersion4_3);
        builder.setProjectedClusterConfig(clusterConfig);

        const auto config = builder.build();
        const auto clusterVideo = descriptorById(config, 12).av_channel();
        const auto& videoConfig = clusterVideo.video_configs(0);
        QCOMPARE(videoConfig.video_resolution(),
                 oaa::proto::enums::VideoResolution::VIDEO_1280x720);
        QCOMPARE(videoConfig.dpi(), 160u);
        QCOMPARE(videoConfig.margin_width(), 680u);
        QCOMPARE(videoConfig.margin_height(), 320u);
        QCOMPARE(config.sessionConfiguration, static_cast<int32_t>(0));
        QCOMPARE(config.sessionConfiguration & 16, 0);
        QVERIFY(videoConfig.has_additional_config());
        const auto& additional = videoConfig.additional_config();
        verifyCompanionInsets(
            additional, videoConfig.margin_width(), videoConfig.margin_height());
        QCOMPARE(additional.display_insets().left(), 340u);
        QCOMPARE(additional.display_insets().right(), 340u);
        QCOMPARE(additional.display_insets().top(), 160u);
        QCOMPARE(additional.display_insets().bottom(), 160u);
        QCOMPARE(additional.hidden_ui_elements_size(), 1);
        QCOMPARE(additional.hidden_ui_elements(0),
                 oaa::proto::data::UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE);
    }

    void gal43AdditionalConfigPreservesBaseVideoConfigBytes() {
        oap::YamlConfig yaml;
        yaml.setVideoResolution(QStringLiteral("480p"));
        yaml.setValueByPath("navbar.show_during_aa", true);

        oap::aa::ProjectedClusterConfig legacyCluster;
        legacyCluster.enabled = true;
        oap::aa::ServiceDiscoveryBuilder legacyBuilder(&yaml);
        legacyBuilder.setProtocolVersion(oaa::kGalVersion1_7);
        legacyBuilder.setDisplayDimensions(1024, 600);
        legacyBuilder.setNavbarThickness(60);
        legacyBuilder.setProjectedClusterConfig(legacyCluster);
        const auto legacy = legacyBuilder.build();

        auto modernCluster = legacyCluster;
        modernCluster.profile.nativeTurnCardAvailable = true;
        oap::aa::ServiceDiscoveryBuilder modernBuilder(&yaml);
        modernBuilder.setProtocolVersion(oaa::kGalVersion4_3);
        modernBuilder.setDisplayDimensions(1024, 600);
        modernBuilder.setNavbarThickness(60);
        modernBuilder.setProjectedClusterConfig(modernCluster);
        const auto modern = modernBuilder.build();

        const auto legacyMain = descriptorById(legacy, 3).av_channel();
        const auto modernMain = descriptorById(modern, 3).av_channel();
        QCOMPARE(modernMain.video_configs_size(), legacyMain.video_configs_size());
        for (int i = 0; i < legacyMain.video_configs_size(); ++i) {
            const auto& modernVideo = modernMain.video_configs(i);
            QCOMPARE(modernVideo.margin_width(), 0u);
            QCOMPARE(modernVideo.margin_height(), 58u);
            QVERIFY(modernVideo.has_additional_config());
            verifyCompanionInsets(
                modernVideo.additional_config(), 0u, 58u);
            QCOMPARE(modernVideo.additional_config().display_insets().top(), 29u);
            QCOMPARE(modernVideo.additional_config().display_insets().bottom(), 29u);
            QCOMPARE(baseVideoConfigBytes(modernMain.video_configs(i)),
                     baseVideoConfigBytes(legacyMain.video_configs(i)));
        }

        const auto legacyClusterVideo = descriptorById(legacy, 12).av_channel();
        const auto modernClusterVideo = descriptorById(modern, 12).av_channel();
        const auto& modernClusterConfig = modernClusterVideo.video_configs(0);
        QVERIFY(modernClusterConfig.has_additional_config());
        verifyCompanionInsets(
            modernClusterConfig.additional_config(), 500u, 180u);
        QCOMPARE(
            modernClusterConfig.additional_config().display_insets().left(),
            250u);
        QCOMPARE(
            modernClusterConfig.additional_config().display_insets().right(),
            250u);
        QCOMPARE(
            modernClusterConfig.additional_config().display_insets().top(),
            90u);
        QCOMPARE(
            modernClusterConfig.additional_config().display_insets().bottom(),
            90u);
        QCOMPARE(baseVideoConfigBytes(modernClusterVideo.video_configs(0)),
                 baseVideoConfigBytes(legacyClusterVideo.video_configs(0)));

        static const QByteArray modernMainGolden = QByteArray::fromHex(
            "08031a420803221b080110021800203a288c015003"
            "5a0c0a08081d101d180020002801"
            "221b080110021800203a288c015007"
            "5a0c0a08081d101d180020002801300038004803");
        static const QByteArray modernClusterGolden = QByteArray::fromHex(
            "080c1a270803221f0801100218f40320b401288c015003"
            "5a0e0a0a085a105a18fa0120fa01280530013801");
        QCOMPARE(channelById(modern, 3)->descriptor, modernMainGolden);
        QCOMPARE(channelById(modern, 12)->descriptor, modernClusterGolden);
    }

    void gal43CompanionInsetsPreserveOddMarginTotals() {
        oap::YamlConfig yaml;
        yaml.setVideoResolution(QStringLiteral("480p"));
        yaml.setValueByPath("navbar.show_during_aa", true);

        oap::aa::ProjectedClusterConfig cluster;
        cluster.enabled = true;

        oap::aa::ServiceDiscoveryBuilder builder(&yaml);
        builder.setProtocolVersion(oaa::kGalVersion4_3);
        builder.setDisplayDimensions(1024, 600);
        builder.setNavbarThickness(2);
        builder.setProjectedClusterConfig(cluster);
        const auto session = builder.build();

        const auto main = descriptorById(session, 3).av_channel();
        for (const auto& video : main.video_configs()) {
            QCOMPARE(video.margin_width(), 0u);
            QCOMPARE(video.margin_height(), 13u);
            QVERIFY(video.has_additional_config());
            const auto& insets = video.additional_config().display_insets();
            QCOMPARE(insets.top(), 6u);
            QCOMPARE(insets.bottom(), 7u);
            QCOMPARE(insets.top() + insets.bottom(), 13u);
        }
    }
};

QTEST_MAIN(TestServiceDiscoveryBuilder)
#include "test_service_discovery_builder.moc"
