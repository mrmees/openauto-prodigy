#include "AndroidAutoEntity.hpp"
#include "../../core/YamlConfig.hpp"

#include <boost/log/trivial.hpp>
#include <aasdk_proto/ControlMessageIdsEnum.pb.h>
#include <aasdk_proto/StatusEnum.pb.h>
#include <aasdk_proto/AuthCompleteIndicationMessage.pb.h>
#include <aasdk_proto/ServiceDiscoveryResponseMessage.pb.h>
#include <aasdk_proto/AudioFocusResponseMessage.pb.h>
#include <aasdk_proto/AudioFocusStateEnum.pb.h>
#include <aasdk_proto/AudioFocusTypeEnum.pb.h>
#include <aasdk_proto/NavigationFocusResponseMessage.pb.h>
#include <aasdk_proto/PingResponseMessage.pb.h>
#include <aasdk_proto/PingRequestMessage.pb.h>
#include <aasdk_proto/ShutdownRequestMessage.pb.h>
#include <aasdk_proto/ShutdownResponseMessage.pb.h>
#include <aasdk_proto/ShutdownReasonEnum.pb.h>
#include <aasdk_proto/ConnectionConfigurationData.pb.h>
#include <aasdk_proto/PingConfigurationData.pb.h>
#include <aasdk_proto/HeadUnitInfoData.pb.h>

namespace oap {
namespace aa {

AndroidAutoEntity::AndroidAutoEntity(
    boost::asio::io_service& ioService,
    aasdk::messenger::ICryptor::Pointer cryptor,
    aasdk::messenger::IMessenger::Pointer messenger,
    IService::ServiceList serviceList,
    oap::YamlConfig* yamlConfig)
    : strand_(ioService)
    , cryptor_(std::move(cryptor))
    , controlChannel_(std::make_shared<aasdk::channel::control::ControlServiceChannel>(strand_, messenger))
    , serviceList_(std::move(serviceList))
    , yamlConfig_(yamlConfig)
    , pingTimer_(ioService)
{
}

void AndroidAutoEntity::start(IAndroidAutoEntityEventHandler& eventHandler)
{
    strand_.dispatch([this, self = this->shared_from_this(), &eventHandler]() {
        BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Starting...";
        eventHandler_ = &eventHandler;

        for (auto& service : serviceList_) {
            service->start();
        }

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {
            BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Version request sent";
        }, std::bind(&AndroidAutoEntity::onChannelSendError, this->shared_from_this(), std::placeholders::_1));

        controlChannel_->sendVersionRequest(std::move(promise));
        controlChannel_->receive(this->shared_from_this());
    });
}

void AndroidAutoEntity::stop()
{
    strand_.dispatch([this, self = this->shared_from_this()]() {
        BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Stopping...";
        pingTimer_.cancel();

        for (auto& service : serviceList_) {
            service->stop();
        }

        eventHandler_ = nullptr;
    });
}

void AndroidAutoEntity::requestShutdown()
{
    strand_.dispatch([this, self = this->shared_from_this()]() {
        BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Sending shutdown request to phone";
        pingTimer_.cancel();

        aasdk::proto::messages::ShutdownRequest request;
        request.set_reason(aasdk::proto::enums::ShutdownReason::QUIT);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {
            BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Shutdown request sent";
        }, std::bind(&AndroidAutoEntity::onChannelSendError,
                     this->shared_from_this(), std::placeholders::_1));

        controlChannel_->sendShutdownRequest(request, std::move(promise));
        controlChannel_->receive(this->shared_from_this());
    });
}

void AndroidAutoEntity::onVersionResponse(
    uint16_t majorCode, uint16_t minorCode,
    aasdk::proto::enums::VersionResponseStatus::Enum status)
{
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Version response: "
                            << majorCode << "." << minorCode
                            << " status=" << static_cast<int>(status);

    if (status == aasdk::proto::enums::VersionResponseStatus::MATCH) {
        BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Version matched, starting SSL handshake";

        try {
            cryptor_->doHandshake();
            auto handshakeData = cryptor_->readHandshakeBuffer();

            if (!handshakeData.empty()) {
                auto promise = aasdk::channel::SendPromise::defer(strand_);
                promise->then([]() {},
                    std::bind(&AndroidAutoEntity::onChannelSendError,
                              this->shared_from_this(), std::placeholders::_1));
                controlChannel_->sendHandshake(std::move(handshakeData), std::move(promise));
            }
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "[AndroidAutoEntity] SSL init error: " << e.what();
            if (eventHandler_) eventHandler_->onError(std::string("SSL error: ") + e.what());
            return;
        }
    } else {
        BOOST_LOG_TRIVIAL(error) << "[AndroidAutoEntity] Version mismatch!";
        if (eventHandler_) eventHandler_->onError("Protocol version mismatch");
        return;
    }

    controlChannel_->receive(this->shared_from_this());
}

void AndroidAutoEntity::onHandshake(const aasdk::common::DataConstBuffer& payload)
{
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] SSL handshake data received ("
                            << payload.size << " bytes)";

    try {
        cryptor_->writeHandshakeBuffer(payload);

        if (!cryptor_->doHandshake()) {
            auto handshakeData = cryptor_->readHandshakeBuffer();
            if (!handshakeData.empty()) {
                auto promise = aasdk::channel::SendPromise::defer(strand_);
                promise->then([]() {},
                    std::bind(&AndroidAutoEntity::onChannelSendError,
                              this->shared_from_this(), std::placeholders::_1));
                controlChannel_->sendHandshake(std::move(handshakeData), std::move(promise));
            }
        } else {
            BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] SSL handshake complete, sending auth";

            aasdk::proto::messages::AuthCompleteIndication authComplete;
            authComplete.set_status(aasdk::proto::enums::Status::OK);

            auto promise = aasdk::channel::SendPromise::defer(strand_);
            promise->then([this, self = this->shared_from_this()]() {
                BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Auth complete sent";
            }, std::bind(&AndroidAutoEntity::onChannelSendError,
                         this->shared_from_this(), std::placeholders::_1));

            controlChannel_->sendAuthComplete(authComplete, std::move(promise));
        }
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[AndroidAutoEntity] SSL handshake error: " << e.what();
        if (eventHandler_) eventHandler_->onError(std::string("SSL handshake error: ") + e.what());
        return;
    }

    controlChannel_->receive(this->shared_from_this());
}

void AndroidAutoEntity::onServiceDiscoveryRequest(
    const aasdk::proto::messages::ServiceDiscoveryRequest& request)
{
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Service discovery from "
                            << request.device_name()
                            << " (" << request.device_brand() << ")";
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Phone request: " << request.ShortDebugString();

    aasdk::proto::messages::ServiceDiscoveryResponse response;

    // Modern fields (required by newer Android Auto versions)
    response.set_display_name("OpenAuto Prodigy");
    response.set_probe_for_support(false);

    auto* connConfig = response.mutable_connection_configuration();
    auto* pingConfig = connConfig->mutable_ping_configuration();
    pingConfig->set_timeout_ms(3000);
    pingConfig->set_interval_ms(1000);
    pingConfig->set_high_latency_threshold_ms(200);
    pingConfig->set_tracked_ping_count(5);

    // Identity fields — driven by YamlConfig with sensible defaults
    const std::string huName = yamlConfig_ ? yamlConfig_->headUnitName().toStdString() : "OpenAuto Prodigy";
    const std::string mfr = yamlConfig_ ? yamlConfig_->manufacturer().toStdString() : "OpenAuto";
    const std::string mdl = yamlConfig_ ? yamlConfig_->model().toStdString() : "Prodigy";
    const std::string swVer = yamlConfig_ ? yamlConfig_->swVersion().toStdString() : "0.1.0";
    const std::string carMdl = yamlConfig_ && !yamlConfig_->carModel().isEmpty()
                                   ? yamlConfig_->carModel().toStdString() : "Universal";
    const std::string carYr = yamlConfig_ && !yamlConfig_->carYear().isEmpty()
                                  ? yamlConfig_->carYear().toStdString() : "2026";
    const bool lhd = yamlConfig_ ? yamlConfig_->leftHandDrive() : true;

    // Git hash compiled in via CMake — falls back to "unknown"
#ifndef OAP_GIT_HASH
#define OAP_GIT_HASH "unknown"
#endif
    const std::string swBuild = OAP_GIT_HASH;

    auto* huInfo = response.mutable_headunit_info();
    huInfo->set_make(mfr);
    huInfo->set_model(carMdl);
    huInfo->set_year(carYr);
    huInfo->set_vehicle_id("OAP-0001");
    huInfo->set_head_unit_make(mfr);
    huInfo->set_head_unit_model(mdl);
    huInfo->set_head_unit_software_build(swBuild);
    huInfo->set_head_unit_software_version(swVer);

    // Legacy fields (deprecated but included for older AA versions)
    response.set_head_unit_name(huName);
    response.set_car_model(carMdl);
    response.set_car_year(carYr);
    response.set_car_serial("OAP-0001");
    response.set_left_hand_drive_vehicle(lhd);
    response.set_headunit_manufacturer(mfr);
    response.set_headunit_model(mdl);
    response.set_sw_build(swBuild);
    response.set_sw_version(swVer);
    response.set_can_play_native_media_during_vr(true);
    response.set_hide_clock(false);

    for (auto& service : serviceList_) {
        service->fillFeatures(response);
    }

    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Responding with "
                            << response.channels_size() << " channels"
                            << " (serialized size: " << response.ByteSizeLong() << " bytes)";
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Response: " << response.ShortDebugString();

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([this, self = this->shared_from_this()]() {
        BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Service discovery sent — connected!";
        if (eventHandler_) eventHandler_->onConnected();
        schedulePing();
    }, std::bind(&AndroidAutoEntity::onChannelSendError,
                 this->shared_from_this(), std::placeholders::_1));

    controlChannel_->sendServiceDiscoveryResponse(response, std::move(promise));
    controlChannel_->receive(this->shared_from_this());
}

void AndroidAutoEntity::onAudioFocusRequest(
    const aasdk::proto::messages::AudioFocusRequest& request)
{
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Audio focus request (type="
                            << request.audio_focus_type() << ")";

    aasdk::proto::messages::AudioFocusResponse response;

    // Grant the focus type the phone requests
    switch (request.audio_focus_type()) {
    case aasdk::proto::enums::AudioFocusType::GAIN:
        response.set_audio_focus_state(aasdk::proto::enums::AudioFocusState::GAIN);
        break;
    case aasdk::proto::enums::AudioFocusType::GAIN_TRANSIENT:
        response.set_audio_focus_state(aasdk::proto::enums::AudioFocusState::GAIN_TRANSIENT);
        break;
    case aasdk::proto::enums::AudioFocusType::GAIN_NAVI:
        response.set_audio_focus_state(
            aasdk::proto::enums::AudioFocusState::GAIN_TRANSIENT_GUIDANCE_ONLY);
        break;
    case aasdk::proto::enums::AudioFocusType::RELEASE:
        response.set_audio_focus_state(aasdk::proto::enums::AudioFocusState::LOSS);
        break;
    default:
        response.set_audio_focus_state(aasdk::proto::enums::AudioFocusState::GAIN);
        break;
    }

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {},
        std::bind(&AndroidAutoEntity::onChannelSendError,
                  this->shared_from_this(), std::placeholders::_1));

    controlChannel_->sendAudioFocusResponse(response, std::move(promise));
    controlChannel_->receive(this->shared_from_this());
}

void AndroidAutoEntity::onShutdownRequest(
    const aasdk::proto::messages::ShutdownRequest& request)
{
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Shutdown request (reason: "
                            << request.reason() << ")";

    aasdk::proto::messages::ShutdownResponse response;

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([this, self = this->shared_from_this()]() {
        BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Shutdown response sent";
        if (eventHandler_) eventHandler_->onDisconnected();
    }, std::bind(&AndroidAutoEntity::onChannelSendError,
                 this->shared_from_this(), std::placeholders::_1));

    controlChannel_->sendShutdownResponse(response, std::move(promise));
}

void AndroidAutoEntity::onShutdownResponse(
    const aasdk::proto::messages::ShutdownResponse& response)
{
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Shutdown response received";
    if (eventHandler_) eventHandler_->onDisconnected();
}

void AndroidAutoEntity::onNavigationFocusRequest(
    const aasdk::proto::messages::NavigationFocusRequest& request)
{
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Navigation focus request (type="
                            << request.type() << ")";

    aasdk::proto::messages::NavigationFocusResponse response;
    response.set_type(request.type());

    // type 1 = gain projection focus, type 2 = release (exit to car)
    bool exitToCar = (request.type() != 1);

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([this, self = this->shared_from_this(), exitToCar]() {
        if (exitToCar && eventHandler_) {
            BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Projection focus released — exit to car";
            eventHandler_->onProjectionFocusLost();
        }
    }, std::bind(&AndroidAutoEntity::onChannelSendError,
                 this->shared_from_this(), std::placeholders::_1));

    controlChannel_->sendNavigationFocusResponse(response, std::move(promise));
    controlChannel_->receive(this->shared_from_this());
}

void AndroidAutoEntity::onPingRequest(const aasdk::proto::messages::PingRequest& request)
{
    BOOST_LOG_TRIVIAL(debug) << "[AndroidAutoEntity] Ping request";

    aasdk::proto::messages::PingResponse response;
    response.set_timestamp(request.timestamp());

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {},
        std::bind(&AndroidAutoEntity::onChannelSendError,
                  this->shared_from_this(), std::placeholders::_1));

    controlChannel_->sendPingResponse(response, std::move(promise));
    controlChannel_->receive(this->shared_from_this());
}

void AndroidAutoEntity::onPingResponse(const aasdk::proto::messages::PingResponse& response)
{
    BOOST_LOG_TRIVIAL(debug) << "[AndroidAutoEntity] Ping response";
    controlChannel_->receive(this->shared_from_this());
}

void AndroidAutoEntity::onVoiceSessionRequest(
    const aasdk::proto::messages::VoiceSessionRequest& request)
{
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Voice session request";
    controlChannel_->receive(this->shared_from_this());
}

void AndroidAutoEntity::onChannelError(const aasdk::error::Error& e)
{
    BOOST_LOG_TRIVIAL(error) << "[AndroidAutoEntity] Channel error: " << e.what();
    if (eventHandler_) eventHandler_->onError(std::string("Channel error: ") + e.what());
    if (eventHandler_) eventHandler_->onDisconnected();
}

void AndroidAutoEntity::onChannelSendError(const aasdk::error::Error& e)
{
    BOOST_LOG_TRIVIAL(error) << "[AndroidAutoEntity] Send error: " << e.what();
}

void AndroidAutoEntity::schedulePing()
{
    pingTimer_.expires_from_now(boost::posix_time::seconds(5));
    pingTimer_.async_wait(strand_.wrap([this, self = this->shared_from_this()](const boost::system::error_code& ec) {
        if (ec) return;

        aasdk::proto::messages::PingRequest request;
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch());
        request.set_timestamp(timestamp.count());

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then(
            [this, self = this->shared_from_this()]() { schedulePing(); },
            std::bind(&AndroidAutoEntity::onChannelSendError,
                      this->shared_from_this(), std::placeholders::_1));

        controlChannel_->sendPingRequest(request, std::move(promise));
    }));
}

} // namespace aa
} // namespace oap
