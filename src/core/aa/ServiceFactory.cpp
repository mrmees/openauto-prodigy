#include "ServiceFactory.hpp"
#include "VideoService.hpp"

#include <boost/log/trivial.hpp>

// Channel headers
#include <aasdk/Channel/AV/VideoServiceChannel.hpp>
#include <aasdk/Channel/AV/MediaAudioServiceChannel.hpp>
#include <aasdk/Channel/AV/SpeechAudioServiceChannel.hpp>
#include <aasdk/Channel/AV/SystemAudioServiceChannel.hpp>
#include <aasdk/Channel/Input/InputServiceChannel.hpp>
#include <aasdk/Channel/Sensor/SensorServiceChannel.hpp>
#include <aasdk/Channel/Bluetooth/BluetoothServiceChannel.hpp>
#include <aasdk/Channel/WIFI/WIFIServiceChannel.hpp>

// Event handler interfaces
#include <aasdk/Channel/AV/IVideoServiceChannelEventHandler.hpp>
#include <aasdk/Channel/AV/IAudioServiceChannelEventHandler.hpp>
#include <aasdk/Channel/Input/IInputServiceChannelEventHandler.hpp>
#include <aasdk/Channel/Sensor/ISensorServiceChannelEventHandler.hpp>
#include <aasdk/Channel/Bluetooth/IBluetoothServiceChannelEventHandler.hpp>
#include <aasdk/Channel/WIFI/IWIFIServiceChannelEventHandler.hpp>

// Proto messages for service discovery
#include <aasdk_proto/ChannelDescriptorData.pb.h>
#include <aasdk_proto/AVChannelData.pb.h>
#include <aasdk_proto/VideoConfigData.pb.h>
#include <aasdk_proto/AudioConfigData.pb.h>
#include <aasdk_proto/InputChannelData.pb.h>
#include <aasdk_proto/TouchConfigData.pb.h>
#include <aasdk_proto/SensorChannelData.pb.h>
#include <aasdk_proto/SensorData.pb.h>
#include <aasdk_proto/BluetoothChannelData.pb.h>
#include <aasdk_proto/BluetoothPairingMethodEnum.pb.h>
#include <aasdk_proto/WifiChannelData.pb.h>
#include <aasdk_proto/WifiSecurityResponseMessage.pb.h>
#include <aasdk_proto/AVStreamTypeEnum.pb.h>
#include <aasdk_proto/AudioTypeEnum.pb.h>
#include <aasdk_proto/VideoResolutionEnum.pb.h>
#include <aasdk_proto/VideoFPSEnum.pb.h>
#include <aasdk_proto/SensorTypeEnum.pb.h>
#include <aasdk_proto/ChannelOpenResponseMessage.pb.h>
#include <aasdk_proto/AVChannelSetupResponseMessage.pb.h>
#include <aasdk_proto/AVChannelSetupStatusEnum.pb.h>
#include <aasdk_proto/StatusEnum.pb.h>
#include <aasdk_proto/VideoFocusIndicationMessage.pb.h>
#include <aasdk_proto/VideoFocusModeEnum.pb.h>
#include <aasdk_proto/BindingResponseMessage.pb.h>
#include <aasdk_proto/SensorStartResponseMessage.pb.h>
#include <aasdk_proto/SensorEventIndicationMessage.pb.h>
#include <aasdk_proto/NightModeData.pb.h>
#include <aasdk_proto/DrivingStatusData.pb.h>
#include <aasdk_proto/DrivingStatusEnum.pb.h>

#include <aasdk/Messenger/ChannelId.hpp>
#include <aasdk/Channel/Promise.hpp>

#ifdef HAS_BLUETOOTH
#include <QBluetoothLocalDevice>
#endif

namespace oap {
namespace aa {

// ============================================================================
// Audio Service (stub — works for Media, Speech, System audio channels)
// ============================================================================
template<typename ChannelType, aasdk::messenger::ChannelId ChannelIdValue,
         aasdk::proto::enums::AudioType::Enum AudioTypeValue,
         uint32_t SampleRate, uint32_t ChannelCount>
class AudioServiceStub
    : public IService
    , public aasdk::channel::av::IAudioServiceChannelEventHandler
    , public std::enable_shared_from_this<AudioServiceStub<ChannelType, ChannelIdValue, AudioTypeValue, SampleRate, ChannelCount>>
{
    using Self = AudioServiceStub<ChannelType, ChannelIdValue, AudioTypeValue, SampleRate, ChannelCount>;
public:
    AudioServiceStub(boost::asio::io_service& ioService,
                     aasdk::messenger::IMessenger::Pointer messenger)
        : strand_(ioService)
        , channel_(std::make_shared<ChannelType>(strand_, std::move(messenger)))
    {}

    void start() override {
        strand_.dispatch([this, self = this->shared_from_this()]() {
            BOOST_LOG_TRIVIAL(info) << "[AudioService:" << static_cast<int>(ChannelIdValue) << "] Started (stub)";
            channel_->receive(this->shared_from_this());
        });
    }

    void stop() override {}

    void fillFeatures(aasdk::proto::messages::ServiceDiscoveryResponse& response) override {
        auto* desc = response.add_channels();
        desc->set_channel_id(static_cast<uint32_t>(ChannelIdValue));

        auto* avChannel = desc->mutable_av_channel();
        avChannel->set_stream_type(aasdk::proto::enums::AVStreamType::AUDIO);
        avChannel->set_audio_type(AudioTypeValue);
        avChannel->set_available_while_in_call(true);

        auto* audioConfig = avChannel->add_audio_configs();
        audioConfig->set_sample_rate(SampleRate);
        audioConfig->set_bit_depth(16);
        audioConfig->set_channel_count(ChannelCount);
    }

    void onChannelOpenRequest(const aasdk::proto::messages::ChannelOpenRequest& request) override {
        BOOST_LOG_TRIVIAL(info) << "[AudioService:" << static_cast<int>(ChannelIdValue) << "] Channel open";
        aasdk::proto::messages::ChannelOpenResponse response;
        response.set_status(aasdk::proto::enums::Status::OK);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {});
        channel_->sendChannelOpenResponse(response, std::move(promise));
        channel_->receive(this->shared_from_this());
    }

    void onAVChannelSetupRequest(const aasdk::proto::messages::AVChannelSetupRequest& request) override {
        BOOST_LOG_TRIVIAL(info) << "[AudioService:" << static_cast<int>(ChannelIdValue) << "] AV setup";
        aasdk::proto::messages::AVChannelSetupResponse response;
        response.set_media_status(aasdk::proto::enums::AVChannelSetupStatus::OK);
        response.set_max_unacked(1);
        response.add_configs(0);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {});
        channel_->sendAVChannelSetupResponse(response, std::move(promise));
        channel_->receive(this->shared_from_this());
    }

    void onAVChannelStartIndication(const aasdk::proto::messages::AVChannelStartIndication& indication) override {
        BOOST_LOG_TRIVIAL(info) << "[AudioService:" << static_cast<int>(ChannelIdValue) << "] Start";
        session_ = indication.session();
        channel_->receive(this->shared_from_this());
    }

    void onAVChannelStopIndication(const aasdk::proto::messages::AVChannelStopIndication& indication) override {
        channel_->receive(this->shared_from_this());
    }

    void onAVMediaWithTimestampIndication(aasdk::messenger::Timestamp::ValueType,
                                           const aasdk::common::DataConstBuffer& buffer) override {
        // ACK but don't play
        aasdk::proto::messages::AVMediaAckIndication ack;
        ack.set_session(session_);
        ack.set_value(1);
        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {});
        channel_->sendAVMediaAckIndication(ack, std::move(promise));
        channel_->receive(this->shared_from_this());
    }

    void onAVMediaIndication(const aasdk::common::DataConstBuffer& buffer) override {
        channel_->receive(this->shared_from_this());
    }

    void onChannelError(const aasdk::error::Error& e) override {
        BOOST_LOG_TRIVIAL(error) << "[AudioService:" << static_cast<int>(ChannelIdValue) << "] Error: " << e.what();
    }

private:
    boost::asio::io_service::strand strand_;
    std::shared_ptr<ChannelType> channel_;
    int32_t session_ = -1;
};

using MediaAudioServiceStub = AudioServiceStub<
    aasdk::channel::av::MediaAudioServiceChannel,
    aasdk::messenger::ChannelId::MEDIA_AUDIO,
    aasdk::proto::enums::AudioType::MEDIA, 48000, 2>;

using SpeechAudioServiceStub = AudioServiceStub<
    aasdk::channel::av::SpeechAudioServiceChannel,
    aasdk::messenger::ChannelId::SPEECH_AUDIO,
    aasdk::proto::enums::AudioType::SPEECH, 16000, 1>;

using SystemAudioServiceStub = AudioServiceStub<
    aasdk::channel::av::SystemAudioServiceChannel,
    aasdk::messenger::ChannelId::SYSTEM_AUDIO,
    aasdk::proto::enums::AudioType::SYSTEM, 16000, 1>;

// ============================================================================
// Input Service (stub)
// ============================================================================
class InputServiceStub
    : public IService
    , public aasdk::channel::input::IInputServiceChannelEventHandler
    , public std::enable_shared_from_this<InputServiceStub>
{
public:
    InputServiceStub(boost::asio::io_service& ioService,
                     aasdk::messenger::IMessenger::Pointer messenger,
                     std::shared_ptr<oap::Configuration> config)
        : strand_(ioService)
        , channel_(std::make_shared<aasdk::channel::input::InputServiceChannel>(strand_, std::move(messenger)))
        , config_(std::move(config))
    {}

    void start() override {
        strand_.dispatch([this, self = shared_from_this()]() {
            BOOST_LOG_TRIVIAL(info) << "[InputService] Started (stub)";
            channel_->receive(shared_from_this());
        });
    }

    void stop() override {}

    void fillFeatures(aasdk::proto::messages::ServiceDiscoveryResponse& response) override {
        auto* desc = response.add_channels();
        desc->set_channel_id(static_cast<uint32_t>(aasdk::messenger::ChannelId::INPUT));

        auto* inputChannel = desc->mutable_input_channel();
        // Touch screen config — 1024x600 for DFRobot 7" display
        auto* touchConfig = inputChannel->mutable_touch_screen_config();
        touchConfig->set_width(1024);
        touchConfig->set_height(600);

        // Android keycodes: HOME, BACK, MICROPHONE
        inputChannel->add_supported_keycodes(3);   // KEYCODE_HOME
        inputChannel->add_supported_keycodes(4);   // KEYCODE_BACK
        inputChannel->add_supported_keycodes(84);  // KEYCODE_MICROPHONE_1 (voice commands)
    }

    void onChannelOpenRequest(const aasdk::proto::messages::ChannelOpenRequest& request) override {
        BOOST_LOG_TRIVIAL(info) << "[InputService] Channel open";
        aasdk::proto::messages::ChannelOpenResponse response;
        response.set_status(aasdk::proto::enums::Status::OK);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {});
        channel_->sendChannelOpenResponse(response, std::move(promise));
        channel_->receive(shared_from_this());
    }

    void onBindingRequest(const aasdk::proto::messages::BindingRequest& request) override {
        BOOST_LOG_TRIVIAL(info) << "[InputService] Binding request";
        aasdk::proto::messages::BindingResponse response;
        response.set_status(aasdk::proto::enums::Status::OK);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {});
        channel_->sendBindingResponse(response, std::move(promise));
        channel_->receive(shared_from_this());
    }

    void onChannelError(const aasdk::error::Error& e) override {
        BOOST_LOG_TRIVIAL(error) << "[InputService] Error: " << e.what();
    }

private:
    boost::asio::io_service::strand strand_;
    std::shared_ptr<aasdk::channel::input::InputServiceChannel> channel_;
    std::shared_ptr<oap::Configuration> config_;
};

// ============================================================================
// Sensor Service (stub — advertises NIGHT_DATA for day/night switching)
// ============================================================================
class SensorServiceStub
    : public IService
    , public aasdk::channel::sensor::ISensorServiceChannelEventHandler
    , public std::enable_shared_from_this<SensorServiceStub>
{
public:
    SensorServiceStub(boost::asio::io_service& ioService,
                      aasdk::messenger::IMessenger::Pointer messenger)
        : strand_(ioService)
        , channel_(std::make_shared<aasdk::channel::sensor::SensorServiceChannel>(strand_, std::move(messenger)))
    {}

    void start() override {
        strand_.dispatch([this, self = shared_from_this()]() {
            BOOST_LOG_TRIVIAL(info) << "[SensorService] Started (stub)";
            channel_->receive(shared_from_this());
        });
    }

    void stop() override {}

    void fillFeatures(aasdk::proto::messages::ServiceDiscoveryResponse& response) override {
        auto* desc = response.add_channels();
        desc->set_channel_id(static_cast<uint32_t>(aasdk::messenger::ChannelId::SENSOR));

        auto* sensorChannel = desc->mutable_sensor_channel();
        auto* nightSensor = sensorChannel->add_sensors();
        nightSensor->set_type(aasdk::proto::enums::SensorType::NIGHT_DATA);
        auto* drivingSensor = sensorChannel->add_sensors();
        drivingSensor->set_type(aasdk::proto::enums::SensorType::DRIVING_STATUS);
    }

    void onChannelOpenRequest(const aasdk::proto::messages::ChannelOpenRequest& request) override {
        BOOST_LOG_TRIVIAL(info) << "[SensorService] Channel open";
        aasdk::proto::messages::ChannelOpenResponse response;
        response.set_status(aasdk::proto::enums::Status::OK);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {});
        channel_->sendChannelOpenResponse(response, std::move(promise));
        channel_->receive(shared_from_this());
    }

    void onSensorStartRequest(const aasdk::proto::messages::SensorStartRequestMessage& request) override {
        BOOST_LOG_TRIVIAL(info) << "[SensorService] Sensor start request (type=" << request.sensor_type() << ")";

        aasdk::proto::messages::SensorStartResponseMessage response;
        response.set_status(aasdk::proto::enums::Status::OK);

        auto self = shared_from_this();
        auto sensorType = request.sensor_type();

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([this, self, sensorType]() {
            sendInitialSensorData(sensorType);
        }, [](const aasdk::error::Error& e) {
            BOOST_LOG_TRIVIAL(error) << "[SensorService] Sensor start response error: " << e.what();
        });
        channel_->sendSensorStartResponse(response, std::move(promise));
        channel_->receive(shared_from_this());
    }

    void sendInitialSensorData(int32_t sensorType) {
        aasdk::proto::messages::SensorEventIndication indication;

        if (sensorType == static_cast<int32_t>(aasdk::proto::enums::SensorType::NIGHT_DATA)) {
            auto* nightMode = indication.add_night_mode();
            nightMode->set_is_night(false);
            BOOST_LOG_TRIVIAL(info) << "[SensorService] Sending night mode: day";
        } else if (sensorType == static_cast<int32_t>(aasdk::proto::enums::SensorType::DRIVING_STATUS)) {
            auto* drivingStatus = indication.add_driving_status();
            drivingStatus->set_status(
                static_cast<int32_t>(aasdk::proto::enums::DrivingStatus::UNRESTRICTED));
            BOOST_LOG_TRIVIAL(info) << "[SensorService] Sending driving status: UNRESTRICTED";
        } else {
            BOOST_LOG_TRIVIAL(warning) << "[SensorService] Unknown sensor type: " << sensorType;
            return;
        }

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {
            BOOST_LOG_TRIVIAL(error) << "[SensorService] Sensor event send error: " << e.what();
        });
        channel_->sendSensorEventIndication(indication, std::move(promise));
    }

    void onChannelError(const aasdk::error::Error& e) override {
        BOOST_LOG_TRIVIAL(error) << "[SensorService] Error: " << e.what();
    }

private:
    boost::asio::io_service::strand strand_;
    std::shared_ptr<aasdk::channel::sensor::SensorServiceChannel> channel_;
};

// ============================================================================
// Bluetooth Service (stub)
// ============================================================================
class BluetoothServiceStub
    : public IService
    , public aasdk::channel::bluetooth::IBluetoothServiceChannelEventHandler
    , public std::enable_shared_from_this<BluetoothServiceStub>
{
public:
    BluetoothServiceStub(boost::asio::io_service& ioService,
                         aasdk::messenger::IMessenger::Pointer messenger,
                         std::string btMacAddress)
        : strand_(ioService)
        , channel_(std::make_shared<aasdk::channel::bluetooth::BluetoothServiceChannel>(strand_, std::move(messenger)))
        , btMacAddress_(std::move(btMacAddress))
    {}

    void start() override {
        strand_.dispatch([this, self = shared_from_this()]() {
            BOOST_LOG_TRIVIAL(info) << "[BluetoothService] Started (stub)";
            channel_->receive(shared_from_this());
        });
    }

    void stop() override {}

    void fillFeatures(aasdk::proto::messages::ServiceDiscoveryResponse& response) override {
        auto* desc = response.add_channels();
        desc->set_channel_id(static_cast<uint32_t>(aasdk::messenger::ChannelId::BLUETOOTH));

        auto* btChannel = desc->mutable_bluetooth_channel();
        btChannel->set_adapter_address(btMacAddress_);
        btChannel->add_supported_pairing_methods(
            aasdk::proto::enums::BluetoothPairingMethod::HFP);
    }

    void onChannelOpenRequest(const aasdk::proto::messages::ChannelOpenRequest& request) override {
        BOOST_LOG_TRIVIAL(info) << "[BluetoothService] Channel open";
        aasdk::proto::messages::ChannelOpenResponse response;
        response.set_status(aasdk::proto::enums::Status::OK);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {});
        channel_->sendChannelOpenResponse(response, std::move(promise));
        channel_->receive(shared_from_this());
    }

    void onBluetoothPairingRequest(const aasdk::proto::messages::BluetoothPairingRequest& request) override {
        BOOST_LOG_TRIVIAL(info) << "[BluetoothService] Pairing request";
        channel_->receive(shared_from_this());
    }

    void onChannelError(const aasdk::error::Error& e) override {
        BOOST_LOG_TRIVIAL(error) << "[BluetoothService] Error: " << e.what();
    }

private:
    boost::asio::io_service::strand strand_;
    std::shared_ptr<aasdk::channel::bluetooth::BluetoothServiceChannel> channel_;
    std::string btMacAddress_;
};

// ============================================================================
// WiFi Service (stub — critical for wireless AA)
// ============================================================================
class WIFIServiceStub
    : public IService
    , public aasdk::channel::wifi::IWIFIServiceChannelEventHandler
    , public std::enable_shared_from_this<WIFIServiceStub>
{
public:
    WIFIServiceStub(boost::asio::io_service& ioService,
                    aasdk::messenger::IMessenger::Pointer messenger,
                    std::shared_ptr<oap::Configuration> config)
        : strand_(ioService)
        , channel_(std::make_shared<aasdk::channel::wifi::WIFIServiceChannel>(strand_, std::move(messenger)))
        , config_(std::move(config))
    {}

    void start() override {
        strand_.dispatch([this, self = shared_from_this()]() {
            BOOST_LOG_TRIVIAL(info) << "[WIFIService] Started (stub)";
            channel_->receive(shared_from_this());
        });
    }

    void stop() override {}

    void fillFeatures(aasdk::proto::messages::ServiceDiscoveryResponse& response) override {
        auto* desc = response.add_channels();
        desc->set_channel_id(static_cast<uint32_t>(aasdk::messenger::ChannelId::WIFI));

        auto* wifiChannel = desc->mutable_wifi_channel();
        wifiChannel->set_ssid(config_->wifiSsid().toStdString());
    }

    void onChannelOpenRequest(const aasdk::proto::messages::ChannelOpenRequest& request) override {
        BOOST_LOG_TRIVIAL(info) << "[WIFIService] Channel open";
        aasdk::proto::messages::ChannelOpenResponse response;
        response.set_status(aasdk::proto::enums::Status::OK);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {});
        channel_->sendChannelOpenResponse(response, std::move(promise));
        channel_->receive(shared_from_this());
    }

    void onWifiSecurityRequest() override {
        BOOST_LOG_TRIVIAL(info) << "[WIFIService] WiFi security request — sending credentials";

        aasdk::proto::messages::WifiSecurityResponse response;
        response.set_ssid(config_->wifiSsid().toStdString());
        response.set_key(config_->wifiPassword().toStdString());
        response.set_security_mode(
            aasdk::proto::messages::WifiSecurityResponse_SecurityMode_WPA2_PERSONAL);
        response.set_access_point_type(
            aasdk::proto::messages::WifiSecurityResponse_AccessPointType_STATIC);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {
            BOOST_LOG_TRIVIAL(info) << "[WIFIService] WiFi security response sent";
        }, [](const aasdk::error::Error& e) {
            BOOST_LOG_TRIVIAL(error) << "[WIFIService] WiFi security response error: " << e.what();
        });
        channel_->sendWifiSecurityResponse(response, std::move(promise));
        channel_->receive(shared_from_this());
    }

    void onChannelError(const aasdk::error::Error& e) override {
        BOOST_LOG_TRIVIAL(error) << "[WIFIService] Error: " << e.what();
    }

private:
    boost::asio::io_service::strand strand_;
    std::shared_ptr<aasdk::channel::wifi::WIFIServiceChannel> channel_;
    std::shared_ptr<oap::Configuration> config_;
};

// ============================================================================
// Factory
// ============================================================================

IService::ServiceList ServiceFactory::create(
    boost::asio::io_service& ioService,
    aasdk::messenger::IMessenger::Pointer messenger,
    std::shared_ptr<oap::Configuration> config,
    VideoDecoder* videoDecoder)
{
    IService::ServiceList services;

    services.push_back(std::make_shared<VideoService>(ioService, messenger, config, videoDecoder));
    services.push_back(std::make_shared<MediaAudioServiceStub>(ioService, messenger));
    services.push_back(std::make_shared<SpeechAudioServiceStub>(ioService, messenger));
    services.push_back(std::make_shared<SystemAudioServiceStub>(ioService, messenger));
    services.push_back(std::make_shared<InputServiceStub>(ioService, messenger, config));
    services.push_back(std::make_shared<SensorServiceStub>(ioService, messenger));
    // Read real BT adapter MAC if available, else use placeholder
    std::string btMac = "00:00:00:00:00:00";
#ifdef HAS_BLUETOOTH
    {
        QBluetoothLocalDevice localDevice;
        if (localDevice.isValid()) {
            btMac = localDevice.address().toString().toStdString();
            BOOST_LOG_TRIVIAL(info) << "[ServiceFactory] BT adapter MAC: " << btMac;
        }
    }
#endif
    services.push_back(std::make_shared<BluetoothServiceStub>(ioService, messenger, btMac));
    services.push_back(std::make_shared<WIFIServiceStub>(ioService, messenger, config));

    BOOST_LOG_TRIVIAL(info) << "[ServiceFactory] Created " << services.size() << " services";
    return services;
}

} // namespace aa
} // namespace oap
