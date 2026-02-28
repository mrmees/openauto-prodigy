#include "ServiceDiscoveryBuilder.hpp"
#include "../../core/YamlConfig.hpp"

#include <cmath>
#include <QDebug>
#include <QMap>

// oaa proto headers
#include "oaa/control/ChannelDescriptorData.pb.h"
#include "oaa/av/AVChannelData.pb.h"
#include "oaa/video/VideoConfigData.pb.h"
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
#include "oaa/phone/PhoneStatusChannelData.pb.h"
#include "oaa/av/AVStreamTypeEnum.pb.h"
#include "oaa/audio/AudioTypeEnum.pb.h"
#include "oaa/video/VideoResolutionEnum.pb.h"
#include "oaa/video/VideoFPSEnum.pb.h"
#include "oaa/sensor/SensorTypeEnum.pb.h"
#include "oaa/video/DisplayTypeEnum.pb.h"
#include "oaa/navigation/NavigationTypeEnum.pb.h"

namespace oap {
namespace aa {

ServiceDiscoveryBuilder::ServiceDiscoveryBuilder(
    oap::YamlConfig* yamlConfig,
    const QString& btMacAddress,
    const QString& wifiSsid,
    const QString& wifiPassword)
    : yamlConfig_(yamlConfig)
    , btMacAddress_(btMacAddress)
    , wifiSsid_(wifiSsid)
    , wifiPassword_(wifiPassword)
{
}

oaa::SessionConfig ServiceDiscoveryBuilder::build() const
{
    oaa::SessionConfig config;

    // Head unit identity
    // Phone matches on: manufacturer + model + modelyear + vehicleid
    config.headUnitName = "Crankshaft-NG";
    config.carModel = "Universal";
    config.carYear = "2018";
    config.carSerial = "20180301";
    config.leftHandDrive = true;
    config.manufacturer = "f1x";
    config.model = "Crankshaft-NG Autoapp";
    config.swBuild = "1";
    config.swVersion = "1.0";
    config.canPlayNativeMediaDuringVr = true;

    // Build channel descriptors
    auto addChannel = [&](uint8_t id, QByteArray descriptor) {
        config.channels.append({id, std::move(descriptor)});
    };

    addChannel(3,  buildVideoDescriptor());
    addChannel(4,  buildMediaAudioDescriptor());
    addChannel(5,  buildSpeechAudioDescriptor());
    addChannel(6,  buildSystemAudioDescriptor());
    addChannel(1,  buildInputDescriptor());
    addChannel(2,  buildSensorDescriptor());
    addChannel(8,  buildBluetoothDescriptor());
    addChannel(14, buildWifiDescriptor());
    addChannel(7,  buildAVInputDescriptor());
    addChannel(9,  buildNavigationDescriptor());
    addChannel(10, buildMediaStatusDescriptor());
    addChannel(11, buildPhoneStatusDescriptor());

    return config;
}

// ---- Shared margin calculation ----

void ServiceDiscoveryBuilder::calcMargins(int remoteW, int remoteH,
                                           int& marginW, int& marginH) const
{
    marginW = 0;
    marginH = 0;
    if (!yamlConfig_ || !yamlConfig_->sidebarEnabled() || yamlConfig_->sidebarWidth() <= 0)
        return;

    int displayW = yamlConfig_->displayWidth();
    int displayH = yamlConfig_->displayHeight();
    int sidebarW = yamlConfig_->sidebarWidth();
    QString pos = yamlConfig_->sidebarPosition();
    bool horizontal = (pos == "top" || pos == "bottom");

    int aaViewportW = horizontal ? displayW : (displayW - sidebarW);
    int aaViewportH = horizontal ? (displayH - sidebarW) : displayH;
    float screenRatio = static_cast<float>(aaViewportW) / aaViewportH;
    float remoteRatio = static_cast<float>(remoteW) / remoteH;
    if (screenRatio < remoteRatio)
        marginW = std::round(remoteW - (remoteH * screenRatio));
    else
        marginH = std::round(remoteH - (remoteW / screenRatio));
}

// ---- Channel descriptor builders ----

QByteArray ServiceDiscoveryBuilder::buildVideoDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(3);

    auto* avChannel = desc.mutable_av_channel();
    avChannel->set_stream_type(oaa::proto::enums::AVStreamType::VIDEO);
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
    using Codec = oaa::proto::enums::MediaCodecType;

    // Map codec name strings to protobuf enum values
    static const QMap<QString, Codec::Enum> codecMap = {
        { "h264", Codec::MEDIA_CODEC_VIDEO_H264_BP },
        { "h265", Codec::MEDIA_CODEC_VIDEO_H265 },
        { "vp9",  Codec::MEDIA_CODEC_VIDEO_VP9 },
        { "av1",  Codec::MEDIA_CODEC_VIDEO_AV1 },
    };

    struct ResInfo { Res::Enum res; int w; int h; const char* label; };
    ResInfo chosen = { Res::_720p, 1280, 720, "720p" };
    if (res == "1080p") chosen = { Res::_1080p, 1920, 1080, "1080p" };
    else if (res == "480p") chosen = { Res::_480p, 800, 480, "480p" };

    int mW = 0, mH = 0;
    calcMargins(chosen.w, chosen.h, mW, mH);

    // Read enabled codecs from YAML config
    QStringList enabledCodecs = yamlConfig_ ? yamlConfig_->videoCodecs()
                                            : QStringList{"h264", "h265"};

    int configIdx = 0;
    for (const auto& codecName : enabledCodecs) {
        auto it = codecMap.find(codecName.toLower());
        if (it == codecMap.end()) {
            qWarning() << "[ServiceDiscoveryBuilder] Unknown codec in config:" << codecName
                        << "— skipping";
            continue;
        }
        auto* cfg = avChannel->add_video_configs();
        cfg->set_video_resolution(chosen.res);
        cfg->set_video_fps(fpsEnum);
        cfg->set_margin_width(mW);
        cfg->set_margin_height(mH);
        cfg->set_dpi(dpi);
        cfg->set_codec(it.value());
        qInfo() << "[ServiceDiscoveryBuilder] config[" << configIdx++ << "]:"
                << chosen.label << codecName << "margins:" << mW << "x" << mH;
    }

    if (configIdx == 0) {
        // Fallback: if no valid codecs in config, always advertise H.264
        auto* cfg = avChannel->add_video_configs();
        cfg->set_video_resolution(chosen.res);
        cfg->set_video_fps(fpsEnum);
        cfg->set_margin_width(mW);
        cfg->set_margin_height(mH);
        cfg->set_dpi(dpi);
        cfg->set_codec(Codec::MEDIA_CODEC_VIDEO_H264_BP);
        qWarning() << "[ServiceDiscoveryBuilder] No valid codecs in config, falling back to H.264";
        configIdx = 1;
    }

    qInfo() << "[ServiceDiscoveryBuilder] Advertised" << configIdx << "video configs";

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildMediaAudioDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(4);

    auto* avChannel = desc.mutable_av_channel();
    avChannel->set_stream_type(oaa::proto::enums::AVStreamType::AUDIO);
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
    avChannel->set_stream_type(oaa::proto::enums::AVStreamType::AUDIO);
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
    avChannel->set_stream_type(oaa::proto::enums::AVStreamType::AUDIO);
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

    // Touch screen config — must match content dimensions (after margins)
    int touchW = 1280, touchH = 720;
    if (yamlConfig_) {
        QString res = yamlConfig_->videoResolution();
        if (res == "1080p") { touchW = 1920; touchH = 1080; }
        else if (res == "480p") { touchW = 800; touchH = 480; }
    }
    if (yamlConfig_ && yamlConfig_->sidebarEnabled() && yamlConfig_->sidebarWidth() > 0) {
        int displayW = yamlConfig_->displayWidth();
        int displayH = yamlConfig_->displayHeight();
        int sidebarW = yamlConfig_->sidebarWidth();
        QString pos = yamlConfig_->sidebarPosition();
        bool horizontal = (pos == "top" || pos == "bottom");

        int aaViewportW = horizontal ? displayW : (displayW - sidebarW);
        int aaViewportH = horizontal ? (displayH - sidebarW) : displayH;
        float screenRatio = static_cast<float>(aaViewportW) / aaViewportH;
        float remoteRatio = static_cast<float>(touchW) / touchH;
        if (screenRatio < remoteRatio) {
            int marginW = std::round(touchW - (touchH * screenRatio));
            touchW -= marginW;
        } else {
            int marginH = std::round(touchH - (touchW / screenRatio));
            touchH -= marginH;
        }
    }

    auto* touchConfig = inputChannel->add_touch_screen_config();
    touchConfig->set_width(touchW);
    touchConfig->set_height(touchH);

    qDebug() << "[ServiceDiscoveryBuilder] touch_screen_config:" << touchW << "x" << touchH;

    // Android keycodes: HOME, BACK, MICROPHONE
    inputChannel->add_supported_keycodes(3);   // KEYCODE_HOME
    inputChannel->add_supported_keycodes(4);   // KEYCODE_BACK
    inputChannel->add_supported_keycodes(84);  // KEYCODE_MICROPHONE_1

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
        sensorChannel->add_sensors()->set_type(type);
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
        oaa::proto::enums::BluetoothPairingMethod::HFP);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildWifiDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(14);

    auto* wifiChannel = desc.mutable_wifi_channel();
    wifiChannel->set_ssid(wifiSsid_.toStdString());

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

QByteArray ServiceDiscoveryBuilder::buildAVInputDescriptor() const
{
    oaa::proto::data::ChannelDescriptor desc;
    desc.set_channel_id(7);

    auto* avInputChannel = desc.mutable_av_input_channel();
    avInputChannel->set_stream_type(oaa::proto::enums::AVStreamType::AUDIO);
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
