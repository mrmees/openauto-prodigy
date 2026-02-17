#include "AndroidAutoEntity.hpp"

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
#include <aasdk_proto/ShutdownResponseMessage.pb.h>

namespace oap {
namespace aa {

AndroidAutoEntity::AndroidAutoEntity(
    boost::asio::io_service& ioService,
    aasdk::messenger::ICryptor::Pointer cryptor,
    aasdk::messenger::IMessenger::Pointer messenger,
    IService::ServiceList serviceList)
    : strand_(ioService)
    , cryptor_(std::move(cryptor))
    , controlChannel_(std::make_shared<aasdk::channel::control::ControlServiceChannel>(strand_, messenger))
    , serviceList_(std::move(serviceList))
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
    response.set_head_unit_name("OpenAuto Prodigy");
    response.set_car_model("Universal");
    response.set_car_year("2026");
    response.set_car_serial("OAP-0001");
    response.set_left_hand_drive_vehicle(true);
    response.set_headunit_manufacturer("OpenAuto");
    response.set_headunit_model("Prodigy");
    response.set_sw_build("0.1.0");
    response.set_sw_version("0.1.0");
    response.set_can_play_native_media_during_vr(false);
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
    BOOST_LOG_TRIVIAL(info) << "[AndroidAutoEntity] Navigation focus request";

    aasdk::proto::messages::NavigationFocusResponse response;
    response.set_type(1); // 1 = projected/focused

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {},
        std::bind(&AndroidAutoEntity::onChannelSendError,
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
