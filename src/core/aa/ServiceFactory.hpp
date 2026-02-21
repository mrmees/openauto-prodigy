#pragma once

#include <boost/asio.hpp>
#include <aasdk/Messenger/IMessenger.hpp>
#include "IService.hpp"
#include "../../core/Configuration.hpp"

namespace oap {

class IAudioService;
class YamlConfig;

namespace aa {

class VideoDecoder;
class VideoService;
class TouchHandler;
class NightModeProvider;

struct ServiceFactoryResult {
    IService::ServiceList services;
    std::shared_ptr<VideoService> videoService;
};

class ServiceFactory
{
public:
    static ServiceFactoryResult create(
        boost::asio::io_service& ioService,
        aasdk::messenger::IMessenger::Pointer messenger,
        std::shared_ptr<oap::Configuration> config,
        VideoDecoder* videoDecoder,
        TouchHandler* touchHandler = nullptr,
        IAudioService* audioService = nullptr,
        oap::YamlConfig* yamlConfig = nullptr,
        NightModeProvider* nightProvider = nullptr);
};

} // namespace aa
} // namespace oap
