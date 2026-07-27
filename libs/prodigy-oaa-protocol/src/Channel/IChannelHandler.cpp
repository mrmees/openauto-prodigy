#include <oaa/Channel/IChannelHandler.hpp>

namespace oaa {

IChannelHandler::~IChannelHandler() = default;

void IChannelHandler::configureSession(const SessionProtocolPolicy& policy)
{
    Q_UNUSED(policy)
}

} // namespace oaa
