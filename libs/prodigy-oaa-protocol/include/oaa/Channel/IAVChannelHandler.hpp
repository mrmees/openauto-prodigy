#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <cstdint>

namespace oaa {

class IAVChannelHandler : public IChannelHandler {
    Q_OBJECT
public:
    using IChannelHandler::IChannelHandler;
    ~IAVChannelHandler() override;

    virtual void onMediaData(const QByteArray& data, uint64_t timestamp) = 0;
    virtual bool canAcceptMedia() const = 0;
};

} // namespace oaa
