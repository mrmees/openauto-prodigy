#pragma once

#include <memory>
#include <vector>
#include <aasdk_proto/ServiceDiscoveryResponseMessage.pb.h>

namespace oap {
namespace aa {

class IService
{
public:
    typedef std::shared_ptr<IService> Pointer;
    typedef std::vector<Pointer> ServiceList;

    virtual ~IService() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fillFeatures(aasdk::proto::messages::ServiceDiscoveryResponse& response) = 0;
};

} // namespace aa
} // namespace oap
