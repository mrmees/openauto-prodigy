#include "ServiceDiscoveryBuilder.hpp"
#include "../../core/YamlConfig.hpp"

#include <cmath>
#include "../Logging.hpp"
#include <QMap>
#include <QSet>
#include "oaa/Channel/ChannelId.hpp"

// oaa proto headers
#include "oaa/control/ChannelDescriptorData.pb.h"
#include "oaa/av/AVChannelData.pb.h"
#include "oaa/av/AndroidKeycodeEnum.pb.h"
#include "oaa/video/VideoConfigData.pb.h"
#include "oaa/video/AdditionalVideoConfigData.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"
#include "oaa/audio/AudioConfigData.pb.h"
#include "oaa/input/InputChannelData.pb.h"
#include "oaa/input/TouchConfigData.pb.h"
#include "oaa/sensor/SensorChannelData.pb.h"
#include "oaa/sensor/SensorData.pb.h"
#include "oaa/bluetooth/BluetoothChannelData.pb.h"
#include "oaa/bluetooth/BluetoothPairingMethodEnum.pb.h"
#include "oaa/wifi/WifiChannelData.pb.h"
#include "oaa/av/AVInputChannelData.pb.h"
#include "oaa/navigation/NavigationChannelData.pb.h"
#include "oaa/navigation/NavigationImageOptionsData.pb.h"
#include "oaa/media/MediaChannelData.pb.h"
#include "oaa/audio/AudioTypeEnum.pb.h"
#include "oaa/video/VideoResolutionEnum.pb.h"
#include "oaa/video/VideoFPSEnum.pb.h"
#include "oaa/sensor/SensorTypeEnum.pb.h"
#include "oaa/video/DisplayTypeEnum.pb.h"
#include "oaa/navigation/NavigationTypeEnum.pb.h"

namespace oap {
namespace aa {

namespace {

QStringList resolveVideoCodecNames(oap::YamlConfig* yamlConfig)
{
    static const QSet<QString> recognized = {
        QStringLiteral("h264"), QStringLiteral("h265"),
        QStringLiteral("vp9"), QStringLiteral("av1"),
    };

    const QStringList configured = yamlConfig
        ? yamlConfig->videoCodecs()
        : QStringList{QStringLiteral("h264"), QStringLiteral("h265")};
    QStringList resolved;
    for (const QString& configuredName : configured) {
        const QString name = configuredName.toLower();
        if (!recognized.contains(name)) {
            qCWarning(lcAA) << "Unknown codec in config:" << configuredName
                            << "— skipping";
            continue;
        }
        resolved.push_back(name);
    }

    if (resolved.isEmpty()) {
        qCWarning(lcAA) << "No valid codecs in config, falling back to H.264";
        resolved.push_back(QStringLiteral("h264"));
    }
    return resolved;
}

bool navbarShownDuringAa(oap::YamlConfig* yamlConfig)
{
    if (!yamlConfig)
        return false;
    const QVariant configured =
        yamlConfig->valueByPath(QStringLiteral("navbar.show_during_aa"));
    return configured.isNull() || configured.toBool();
}

void appendHiddenUiElementWithInsets(
    oaa::proto::data::VideoConfig* videoConfig,
    oaa::proto::data::UIElement element,
    uint32_t marginWidth,
    uint32_t marginHeight)
{
    if (!videoConfig)
        return;

    auto* additional = videoConfig->mutable_additional_config();
    auto* insets = additional->mutable_display_insets();
    const uint32_t left = marginWidth / 2;
    const uint32_t top = marginHeight / 2;
    insets->set_left(left);
    insets->set_right(marginWidth - left);
    insets->set_top(top);
    insets->set_bottom(marginHeight - top);

    for (const auto existing : additional->hidden_ui_elements()) {
        if (existing == element)
            return;
    }
    additional->add_hidden_ui_elements(element);
}

oaa::proto::enums::MediaCodecType::Enum videoCodecType(
    const QString& codecName)
{
    using Codec = oaa::proto::enums::MediaCodecType;
    static const QMap<QString, Codec::Enum> codecMap = {
        {QStringLiteral("h264"), Codec::MEDIA_CODEC_VIDEO_H264_BP},
        {QStringLiteral("h265"), Codec::MEDIA_CODEC_VIDEO_H265},
        {QStringLiteral("vp9"), Codec::MEDIA_CODEC_VIDEO_VP9},
        {QStringLiteral("av1"), Codec::MEDIA_CODEC_VIDEO_AV1},
    };

    const auto it = codecMap.find(codecName.toLower());
    Q_ASSERT(it != codecMap.end());
    return it.value();
}

} // namespace

ServiceDiscoveryBuilder::ServiceDiscoveryBuilder(
    oap::YamlConfig* yamlConfig,
    const QString& btMacAddress,
    const QString& wifiSsid,
    const QString& wifiPassword,
    const QString& wifiBssid)
    : yamlConfig_(yamlConfig)
    , btMacAddress_(btMacAddress)
    , wifiSsid_(wifiSsid)
    , wifiPassword_(wifiPassword)
    , wifiBssid_(wifiBssid)
    , videoCodecNames_(resolveVideoCodecNames(yamlConfig))
{
}

uint32_t ServiceDiscoveryBuilder::videoConfigCount() const
{
    if (oaa::SessionProtocolPolicy(protocolVersion_)
            .requiresSingleVideoCodecPerDisplay()) {
        return 1;
    }
    return static_cast<uint32_t>(videoCodecNames_.size());
}

uint32_t ServiceDiscoveryBuilder::videoConfigCount(ProjectedDisplayRole role) const
{
    if (role == ProjectedDisplayRole::Cluster)
        return projectedClusterConfig_.enabled ? 1u : 0u;
    return videoConfigCount();
}

void ServiceDiscoveryBuilder::setProtocolVersion(oaa::ProtocolVersion version)
{
    protocolVersion_ = version;
}

void ServiceDiscoveryBuilder::setProjectedClusterConfig(
    const ProjectedClusterConfig& config)
{
    projectedClusterConfig_ = config;
}

void ServiceDiscoveryBuilder::setDisplayDimensions(int w, int h)
{
    overrideDisplayW_ = w;
    overrideDisplayH_ = h;
}

void ServiceDiscoveryBuilder::setNavbarThickness(int thickness)
{
    navbarThickness_ = thickness;
}

oaa::SessionConfig ServiceDiscoveryBuilder::build() const
{
    oaa::SessionConfig config;

    const oaa::SessionProtocolPolicy policy(protocolVersion_);
    config.protocolMajor = protocolVersion_.major;
    config.protocolMinor = protocolVersion_.minor;

    // Head unit identity
    // Phone matches on: manufacturer + model + modelyear + vehicleid
    config.headUnitName = "Crankshaft-NG";
    config.carModel = "Universal";
    config.carYear = "2018";
    config.carSerial = "20180301";
    config.leftHandDrive = true;
    config.manufacturer = "f1x";
    config.model = "Crankshaft-NG Autoapp";
    config.swBuild = OAP_VERSION;
    config.swVersion = OAP_VERSION;
    config.canPlayNativeMediaDuringVr = true;

    // Build channel descriptors
    auto addChannel = [&](uint8_t id, QByteArray descriptor) {
        config.channels.append({id, std::move(descriptor)});
    };

    addChannel(3,  buildVideoDescriptor());
    if (projectedClusterConfig_.enabled)
        addChannel(oaa::ChannelId::ClusterVideo, buildClusterVideoDescriptor());
    addChannel(4,  buildMediaAudioDescriptor());
    addChannel(5,  buildSpeechAudioDescriptor());
    addChannel(6,  buildSystemAudioDescriptor());
    addChannel(1,  buildInputDescriptor());
    if (projectedClusterConfig_.enabled)
        addChannel(oaa::ChannelId::ClusterInput, buildClusterInputDescriptor());
    addChannel(2,  buildSensorDescriptor());
    addChannel(8,  buildBluetoothDescriptor());
    addChannel(14, buildWifiDescriptor());
    addChannel(7,  buildAVInputDescriptor());
    addChannel(9,  buildNavigationDescriptor());
    addChannel(10, buildMediaStatusDescriptor());
    addChannel(11, buildPhoneStatusDescriptor());

    // Hide phone's AA status bar elements when our navbar shows them
    if (!policy.usesModernDisplayPolicy()) {
        if (navbarShownDuringAa(yamlConfig_)) {
            // session_configuration bitmask (SDR field 13) — does NOT touch AdditionalVideoConfig
            // NOTE: AA 16.2 UI logic (mcr.java) forcibly keeps signal/battery visible when
            // hideClock is set. Can't hide all three. We hide clock only since we render our own.
            // Battery/signal data not available over AA wire — needs companion app as source.
            config.sessionConfiguration = 1;  // HIDE_CLOCK only
        }
    }
    return config;
}

// ---- Shared viewport calculation (navbar-aware) ----

void ServiceDiscoveryBuilder::calcNavbarViewport(int& viewportW, int& viewportH) const
{
    int displayW = (overrideDisplayW_ > 0) ? overrideDisplayW_ : 1024;
    int displayH = (overrideDisplayH_ > 0) ? overrideDisplayH_ : 600;
    viewportW = displayW;
    viewportH = displayH;

    if (!yamlConfig_) return;

    QVariant showDuringAA = yamlConfig_->valueByPath("navbar.show_during_aa");
    bool navbarDuringAA = (showDuringAA.isNull() || showDuringAA.toBool());
    if (!navbarDuringAA) return;

    QString edge = yamlConfig_->valueByPath("navbar.edge").toString();
    if (edge.isEmpty()) edge = "bottom";

    bool horizontal = (edge == "top" || edge == "bottom");
    if (horizontal)
        viewportH -= navbarThickness_;
    else
        viewportW -= navbarThickness_;
}

// ---- Shared content dimension calculation ----

std::pair<int,int> ServiceDiscoveryBuilder::computeContentDimensions(
    int videoW, int videoH, int displayW, int displayH,
    bool navbarDuringAA, const QString& navbarEdge, int navbarThickness)
{
    int viewportW = displayW, viewportH = displayH;

    if (navbarDuringAA) {
        bool horizontal = (navbarEdge == "top" || navbarEdge == "bottom");
        if (horizontal)
            viewportH -= navbarThickness;
        else
            viewportW -= navbarThickness;
    }

    int contentW = videoW, contentH = videoH;
    float screenRatio = static_cast<float>(viewportW) / viewportH;
    float remoteRatio = static_cast<float>(videoW) / videoH;
    if (screenRatio < remoteRatio)
        contentW -= static_cast<int>(std::round(videoW - (videoH * screenRatio)));
    else if (screenRatio > remoteRatio)
        contentH -= static_cast<int>(std::round(videoH - (videoW / screenRatio)));
    return {contentW, contentH};
}

// ---- Shared margin calculation (delegates to computeContentDimensions) ----

void ServiceDiscoveryBuilder::calcMargins(int remoteW, int remoteH,
                                           int& marginW, int& marginH) const
{
    int displayW = (overrideDisplayW_ > 0) ? overrideDisplayW_ : 1024;
    int displayH = (overrideDisplayH_ > 0) ? overrideDisplayH_ : 600;

    bool navbarDuringAA = true;
    QString edge = "bottom";
    if (yamlConfig_) {
        QVariant showDuringAA = yamlConfig_->valueByPath("navbar.show_during_aa");
        navbarDuringAA = (showDuringAA.isNull() || showDuringAA.toBool());
        edge = yamlConfig_->valueByPath("navbar.edge").toString();
        if (edge.isEmpty()) edge = "bottom";
    }

    auto [contentW, contentH] = computeContentDimensions(
        remoteW, remoteH, displayW, displayH, navbarDuringAA, edge, navbarThickness_);
    marginW = remoteW - contentW;
    marginH = remoteH - contentH;
}

// ---- Channel descriptor builders ----

QByteArray ServiceDiscoveryBuilder::buildVideoDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(3);

    auto* avChannel = desc.mutable_av_channel();
    avChannel->set_stream_type(
        oaa::SessionProtocolPolicy(protocolVersion_)
                .requiresSingleVideoCodecPerDisplay()
            ? videoCodecType(videoCodecNames_.front())
            : oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP);
    avChannel->set_color_scheme_support(oaa::proto::enums::ColorSchemeSupport::COLOR_SCHEME_MATERIAL_YOU_V3);
    if (projectedClusterConfig_.enabled) {
        avChannel->set_display_id(kMainDisplayId);
        avChannel->set_display_type(oaa::proto::enums::DisplayType::MAIN);
    }
    // Field 5 in APK is uint32, not bool. Omitting has no effect on session.

    // Resolve preferred resolution from config
    QString res = yamlConfig_ ? yamlConfig_->videoResolution() : QStringLiteral("720p");
    int dpi = yamlConfig_ ? yamlConfig_->videoDpi() : 140;
    int fps = yamlConfig_ ? yamlConfig_->videoFps() : 30;
    auto fpsEnum = (fps == 60) ? oaa::proto::enums::VideoFPS::_60
                               : oaa::proto::enums::VideoFPS::_30;

    int remoteW = 1280, remoteH = 720;
    if (res == "1080p") { remoteW = 1920; remoteH = 1080; }
    else if (res == "480p") { remoteW = 800; remoteH = 480; }

    // Advertise only the configured resolution with codecs from config.
    // Config populated by capability detection (Task 6) or defaults to H.264+H.265.
    using Res = oaa::proto::enums::VideoResolution;

    struct ResInfo { Res::Enum res; int w; int h; const char* label; };
    ResInfo chosen = { Res::VIDEO_1280x720, 1280, 720, "720p" };
    if (res == "1080p") chosen = { Res::VIDEO_1920x1080, 1920, 1080, "1080p" };
    else if (res == "480p") chosen = { Res::VIDEO_800x480, 800, 480, "480p" };

    int mW = 0, mH = 0;
    calcMargins(chosen.w, chosen.h, mW, mH);

    int configIdx = 0;
    const qsizetype codecCount = oaa::SessionProtocolPolicy(protocolVersion_)
                                        .requiresSingleVideoCodecPerDisplay()
        ? 1
        : videoCodecNames_.size();
    for (qsizetype i = 0; i < codecCount; ++i) {
        const QString& codecName = videoCodecNames_.at(i);
        auto* cfg = avChannel->add_video_configs();
        cfg->set_video_resolution(chosen.res);
        cfg->set_video_fps(fpsEnum);
        cfg->set_margin_width(mW);
        cfg->set_margin_height(mH);
        cfg->set_dpi(dpi);
        cfg->set_codec(videoCodecType(codecName));
        if (oaa::SessionProtocolPolicy(protocolVersion_)
                .usesModernDisplayPolicy()
            && navbarShownDuringAa(yamlConfig_)) {
            appendHiddenUiElementWithInsets(
                cfg,
                oaa::proto::data::UI_ELEMENT_CLOCK,
                static_cast<uint32_t>(mW),
                static_cast<uint32_t>(mH));
        }
        qCInfo(lcAA) << "config[" << configIdx++ << "]:"
                << chosen.label << codecName << "margins:" << mW << "x" << mH;
    }

    qCInfo(lcAA) << "Advertised" << configIdx << "video configs";

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildClusterVideoDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(oaa::ChannelId::ClusterVideo);

    auto* avChannel = desc.mutable_av_channel();
    avChannel->set_stream_type(
        oaa::SessionProtocolPolicy(protocolVersion_)
                .requiresSingleVideoCodecPerDisplay()
            ? videoCodecType(videoCodecNames_.front())
            : oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP);
    avChannel->set_display_id(kClusterDisplayId);
    avChannel->set_display_type(oaa::proto::enums::DisplayType::AUXILIARY);
    avChannel->set_keycode(
        oaa::proto::enums::AndroidKeycode::KEYCODE_NAVIGATION);

    const auto& profile = projectedClusterConfig_.profile;
    const ProjectedViewportGeometry geometry = profile.geometry();
    auto* config = avChannel->add_video_configs();
    config->set_video_resolution(profile.resolution == QStringLiteral("720p")
        ? oaa::proto::enums::VideoResolution::VIDEO_1280x720
        : oaa::proto::enums::VideoResolution::VIDEO_800x480);
    config->set_video_fps(oaa::proto::enums::VideoFPS::_30);
    config->set_margin_width(geometry.marginWidth());
    config->set_margin_height(geometry.marginHeight());
    config->set_dpi(profile.dpi);
    config->set_codec(
        oaa::SessionProtocolPolicy(protocolVersion_)
                .requiresSingleVideoCodecPerDisplay()
            ? videoCodecType(videoCodecNames_.front())
            : oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP);
    if (oaa::SessionProtocolPolicy(protocolVersion_).usesModernDisplayPolicy()
        && profile.nativeTurnCardAvailable) {
        appendHiddenUiElementWithInsets(
            config,
            oaa::proto::data::UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE,
            static_cast<uint32_t>(geometry.marginWidth()),
            static_cast<uint32_t>(geometry.marginHeight()));
    }

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildMediaAudioDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(4);

    auto* avChannel = desc.mutable_av_channel();
    avChannel->set_stream_type(
        oaa::proto::enums::MediaCodecType::MEDIA_CODEC_AUDIO_PCM);
    avChannel->set_audio_type(oaa::proto::enums::AudioType::MEDIA);
    // Field 5 in APK is uint32, not bool. Omitting has no effect on session.

    auto* audioConfig = avChannel->add_audio_configs();
    audioConfig->set_sample_rate(48000);
    audioConfig->set_bit_depth(16);
    audioConfig->set_channel_count(2);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildSpeechAudioDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(5);

    auto* avChannel = desc.mutable_av_channel();
    avChannel->set_stream_type(
        oaa::proto::enums::MediaCodecType::MEDIA_CODEC_AUDIO_PCM);
    avChannel->set_audio_type(oaa::proto::enums::AudioType::SPEECH);
    // Field 5 in APK is uint32, not bool. Omitting has no effect on session.

    auto* audioConfig = avChannel->add_audio_configs();
    audioConfig->set_sample_rate(48000);  // Upgraded from 16kHz per probe findings
    audioConfig->set_bit_depth(16);
    audioConfig->set_channel_count(1);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildSystemAudioDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(6);

    auto* avChannel = desc.mutable_av_channel();
    avChannel->set_stream_type(
        oaa::proto::enums::MediaCodecType::MEDIA_CODEC_AUDIO_PCM);
    avChannel->set_audio_type(oaa::proto::enums::AudioType::SYSTEM);
    // Field 5 in APK is uint32, not bool. Omitting has no effect on session.

    auto* audioConfig = avChannel->add_audio_configs();
    audioConfig->set_sample_rate(16000);
    audioConfig->set_bit_depth(16);
    audioConfig->set_channel_count(1);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildInputDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(1);

    auto* inputChannel = desc.mutable_input_channel();
    if (projectedClusterConfig_.enabled)
        inputChannel->set_display_id(kMainDisplayId);

    // Touch screen config — must match content dimensions (after margins)
    int touchW = 1280, touchH = 720;
    if (yamlConfig_) {
        QString res = yamlConfig_->videoResolution();
        if (res == "1080p") { touchW = 1920; touchH = 1080; }
        else if (res == "480p") { touchW = 800; touchH = 480; }
    }
    // Use shared viewport calculation (navbar-aware) to adjust touch dimensions
    {
        int mW = 0, mH = 0;
        calcMargins(touchW, touchH, mW, mH);
        touchW -= mW;
        touchH -= mH;
    }

    auto* touchConfig = inputChannel->add_touch_screen_configs();
    touchConfig->set_width(touchW);
    touchConfig->set_height(touchH);
    if (projectedClusterConfig_.enabled)
        touchConfig->set_display_type(oaa::proto::enums::DisplayType::MAIN);

    qCDebug(lcAA) << "touch_screen_config:" << touchW << "x" << touchH;

    // Android keycodes: navigation, voice, media controls
    inputChannel->add_supported_keycodes(3);   // KEYCODE_HOME
    inputChannel->add_supported_keycodes(4);   // KEYCODE_BACK
    inputChannel->add_supported_keycodes(84);  // KEYCODE_SEARCH
    inputChannel->add_supported_keycodes(85);  // KEYCODE_MEDIA_PLAY_PAUSE
    inputChannel->add_supported_keycodes(86);  // KEYCODE_MEDIA_STOP
    inputChannel->add_supported_keycodes(87);  // KEYCODE_MEDIA_NEXT
    inputChannel->add_supported_keycodes(88);  // KEYCODE_MEDIA_PREVIOUS
    inputChannel->add_supported_keycodes(126); // KEYCODE_MEDIA_PLAY
    inputChannel->add_supported_keycodes(127); // KEYCODE_MEDIA_PAUSE
    inputChannel->add_supported_keycodes(219); // KEYCODE_ASSIST (Google Assistant)
    inputChannel->add_supported_keycodes(231); // KEYCODE_VOICE_ASSIST

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildClusterInputDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(oaa::ChannelId::ClusterInput);
    desc.mutable_input_channel()->set_display_id(kClusterDisplayId);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildSensorDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(2);

    auto* sensorChannel = desc.mutable_sensor_channel();

    auto addSensor = [&](oaa::proto::enums::SensorType::Enum type) {
        sensorChannel->add_sensors()->set_sensor_type(type);
    };

    // Only advertise sensors we can actually populate.
    // Future sensor providers (OBD-II, GPS) will register dynamically.
    addSensor(oaa::proto::enums::SensorType::NIGHT_DATA);
    addSensor(oaa::proto::enums::SensorType::DRIVING_STATUS);
    addSensor(oaa::proto::enums::SensorType::PARKING_BRAKE);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildBluetoothDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(8);

    auto* btChannel = desc.mutable_bluetooth_channel();
    btChannel->set_adapter_address(btMacAddress_.toStdString());
    btChannel->add_supported_pairing_methods(
        oaa::proto::enums::BluetoothPairingMethod::PIN);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildWifiDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(14);

    auto* wifiChannel = desc.mutable_wifi_channel();
    wifiChannel->set_bssid(wifiBssid_.toStdString());

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildAVInputDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(7);

    auto* avInputChannel = desc.mutable_av_input_channel();
    avInputChannel->set_stream_type(
        oaa::proto::enums::MediaCodecType::MEDIA_CODEC_AUDIO_PCM);
    // Field 3 in APK is uint32, not bool. Omitting has no effect on session.

    auto* audioConfig = avInputChannel->mutable_audio_config();
    audioConfig->set_sample_rate(16000);
    audioConfig->set_bit_depth(16);
    audioConfig->set_channel_count(1);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildNavigationDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(9);

    auto* navChannel = desc.mutable_navigation_channel();
    navChannel->set_minimum_interval_ms(500);
    navChannel->set_type(oaa::proto::enums::NavigationType::TURN_BY_TURN);
    auto* imageOpts = navChannel->mutable_image_options();
    imageOpts->set_width(64);
    imageOpts->set_height(64);
    imageOpts->set_colour_depth_bits(32);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildMediaStatusDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(10);

    desc.mutable_media_info_channel(); // empty — just advertise support

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildPhoneStatusDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(11);

    desc.mutable_phone_status_channel(); // empty — just advertise support

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

} // namespace aa
} // namespace oap
