#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <aasdk/Channel/Control/ControlServiceChannel.hpp>
#include <aasdk/Channel/Control/IControlServiceChannelEventHandler.hpp>
#include <aasdk/Messenger/ICryptor.hpp>
#include <aasdk/Messenger/IMessenger.hpp>
#include "IService.hpp"

namespace oap {
class YamlConfig;
namespace aa {

class IAndroidAutoEntityEventHandler
{
public:
    virtual ~IAndroidAutoEntityEventHandler() = default;
    virtual void onConnected() = 0;
    virtual void onDisconnected() = 0;
    virtual void onProjectionFocusLost() = 0;
    virtual void onError(const std::string& message) = 0;
};

class AndroidAutoEntity
    : public aasdk::channel::control::IControlServiceChannelEventHandler
    , public std::enable_shared_from_this<AndroidAutoEntity>
{
public:
    typedef std::shared_ptr<AndroidAutoEntity> Pointer;

    AndroidAutoEntity(boost::asio::io_service& ioService,
                      aasdk::messenger::ICryptor::Pointer cryptor,
                      aasdk::messenger::IMessenger::Pointer messenger,
                      IService::ServiceList serviceList,
                      oap::YamlConfig* yamlConfig = nullptr);

    void start(IAndroidAutoEntityEventHandler& eventHandler);
    void stop();

    // IControlServiceChannelEventHandler
    void onVersionResponse(uint16_t majorCode, uint16_t minorCode,
                           aasdk::proto::enums::VersionResponseStatus::Enum status) override;
    void onHandshake(const aasdk::common::DataConstBuffer& payload) override;
    void onServiceDiscoveryRequest(const aasdk::proto::messages::ServiceDiscoveryRequest& request) override;
    void onAudioFocusRequest(const aasdk::proto::messages::AudioFocusRequest& request) override;
    void onShutdownRequest(const aasdk::proto::messages::ShutdownRequest& request) override;
    void onShutdownResponse(const aasdk::proto::messages::ShutdownResponse& response) override;
    void onNavigationFocusRequest(const aasdk::proto::messages::NavigationFocusRequest& request) override;
    void onPingRequest(const aasdk::proto::messages::PingRequest& request) override;
    void onPingResponse(const aasdk::proto::messages::PingResponse& response) override;
    void onVoiceSessionRequest(const aasdk::proto::messages::VoiceSessionRequest& request) override;
    void onChannelError(const aasdk::error::Error& e) override;

private:
    void onChannelSendError(const aasdk::error::Error& e);
    void schedulePing();

    boost::asio::io_service::strand strand_;
    aasdk::messenger::ICryptor::Pointer cryptor_;
    aasdk::channel::control::IControlServiceChannel::Pointer controlChannel_;
    IService::ServiceList serviceList_;
    IAndroidAutoEntityEventHandler* eventHandler_ = nullptr;
    oap::YamlConfig* yamlConfig_ = nullptr;
    boost::asio::deadline_timer pingTimer_;
};

} // namespace aa
} // namespace oap
