#pragma once

#include <oaa/Channel/IAVChannelHandler.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oaa {
namespace hu {

class AudioChannelHandler : public oaa::IAVChannelHandler {
    Q_OBJECT
public:
    explicit AudioChannelHandler(uint8_t channelId, QObject* parent = nullptr);

    uint8_t channelId() const override { return channelId_; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

    // IAVChannelHandler
    void onMediaData(const QByteArray& data, uint64_t timestamp) override;
    bool canAcceptMedia() const override { return channelOpen_ && streaming_; }

signals:
    void audioDataReceived(const QByteArray& data, uint64_t timestamp);
    void streamStarted(int32_t session);
    void streamStopped();

private:
    void handleSetupRequest(const QByteArray& payload);
    void handleStartIndication(const QByteArray& payload);
    void handleStopIndication();
    void sendAck(uint32_t frameCount);

    uint8_t channelId_;
    int32_t session_ = -1;
    uint32_t unackedCount_ = 0;
    bool channelOpen_ = false;
    bool streaming_ = false;
};

} // namespace hu
} // namespace oaa
