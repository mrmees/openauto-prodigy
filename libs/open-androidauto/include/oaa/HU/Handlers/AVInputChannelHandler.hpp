#pragma once

#include <atomic>

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oaa {
namespace hu {

class AVInputChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit AVInputChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::AVInput; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

    // Send mic data upstream to phone — called from PipeWire capture callback
    void sendMicData(const QByteArray& data, uint64_t timestamp);

signals:
    void micCaptureRequested(bool open);

private:
    void handleInputOpenRequest(const QByteArray& payload);
    void handleAckIndication(const QByteArray& payload);

    std::atomic<bool> channelOpen_{false};
    std::atomic<bool> capturing_{false};
    int32_t session_ = 0;
    int32_t maxUnacked_ = 1;
};

} // namespace hu
} // namespace oaa
