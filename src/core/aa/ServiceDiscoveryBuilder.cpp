#include "ServiceDiscoveryBuilder.hpp"
#include "../../core/YamlConfig.hpp"

#include <cmath>
#include <QDebug>

// oaa proto headers
#include "ChannelDescriptorData.pb.h"
#include "AVChannelData.pb.h"
#include "VideoConfigData.pb.h"
#include "AudioConfigData.pb.h"
#include "InputChannelData.pb.h"
#include "TouchConfigData.pb.h"
#include "SensorChannelData.pb.h"
#include "SensorData.pb.h"
#include "BluetoothChannelData.pb.h"
#include "BluetoothPairingMethodEnum.pb.h"
#include "WifiChannelData.pb.h"
#include "AVInputChannelData.pb.h"
#include "AVStreamTypeEnum.pb.h"
#include "AudioTypeEnum.pb.h"
#include "VideoResolutionEnum.pb.h"
#include "VideoFPSEnum.pb.h"
#include "SensorTypeEnum.pb.h"

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
    config.headUnitName = "OpenAuto Prodigy";
    config.carModel = "Universal";
    config.carYear = "2025";
    config.carSerial = "0000000000000001";
    config.leftHandDrive = true;
    config.manufacturer = "OpenAuto";
    config.model = "Prodigy";
    config.swBuild = "dev";
    config.swVersion = "0.4.0";
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
    avChannel->set_available_while_in_call(true);

    // Resolve preferred resolution from config
    QString res = yamlConfig_ ? yamlConfig_->videoResolution() : QStringLiteral("720p");
    int dpi = yamlConfig_ ? yamlConfig_->videoDpi() : 140;
    int fps = yamlConfig_ ? yamlConfig_->videoFps() : 30;
    auto fpsEnum = (fps == 60) ? oaa::proto::enums::VideoFPS::_60
                               : oaa::proto::enums::VideoFPS::_30;

    int remoteW = 1280, remoteH = 720;
    if (res == "1080p") { remoteW = 1920; remoteH = 1080; }
    else if (res == "480p") { remoteW = 800; remoteH = 480; }

    int marginW = 0, marginH = 0;
    calcMargins(remoteW, remoteH, marginW, marginH);

    // Primary video config
    auto* primaryConfig = avChannel->add_video_configs();
    if (res == "1080p")
        primaryConfig->set_video_resolution(oaa::proto::enums::VideoResolution::_1080p);
    else if (res == "480p")
        primaryConfig->set_video_resolution(oaa::proto::enums::VideoResolution::_480p);
    else
        primaryConfig->set_video_resolution(oaa::proto::enums::VideoResolution::_720p);
    primaryConfig->set_video_fps(fpsEnum);
    primaryConfig->set_margin_width(marginW);
    primaryConfig->set_margin_height(marginH);
    primaryConfig->set_dpi(dpi);

    // Mandatory 480p fallback
    if (res != "480p") {
        int fbMarginW = 0, fbMarginH = 0;
        calcMargins(800, 480, fbMarginW, fbMarginH);

        auto* fallbackConfig = avChannel->add_video_configs();
        fallbackConfig->set_video_resolution(oaa::proto::enums::VideoResolution::_480p);
        fallbackConfig->set_video_fps(oaa::proto::enums::VideoFPS::_30);
        fallbackConfig->set_margin_width(fbMarginW);
        fallbackConfig->set_margin_height(fbMarginH);
        fallbackConfig->set_dpi(dpi);
    }

    qDebug() << "[ServiceDiscoveryBuilder] Video:" << res << "@" << fps << "fps,"
             << dpi << "dpi, margins:" << marginW << "x" << marginH;

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
    avChannel->set_available_while_in_call(true);

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
    avChannel->set_available_while_in_call(true);

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
    avChannel->set_available_while_in_call(true);

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

    auto* touchConfig = inputChannel->mutable_touch_screen_config();
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

    addSensor(oaa::proto::enums::SensorType::NIGHT_DATA);
    addSensor(oaa::proto::enums::SensorType::DRIVING_STATUS);
    addSensor(oaa::proto::enums::SensorType::LOCATION);
    addSensor(oaa::proto::enums::SensorType::COMPASS);
    addSensor(oaa::proto::enums::SensorType::ACCEL);
    addSensor(oaa::proto::enums::SensorType::GYRO);

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
    avInputChannel->set_available_while_in_call(true);

    auto* audioConfig = avInputChannel->mutable_audio_config();
    audioConfig->set_sample_rate(16000);
    audioConfig->set_bit_depth(16);
    audioConfig->set_channel_count(1);

    QByteArray data(desc.ByteSizeLong(), '\0');
    desc.SerializeToArray(data.data(), data.size());
    return data;
}

} // namespace aa
} // namespace oap
