#pragma once

#include <oaa/Channel/IAVChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oaa {
namespace hu {

class VideoChannelHandler : public oaa::IAVChannelHandler {
    Q_OBJECT
public:
    explicit VideoChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::Video; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

    // IAVChannelHandler
    void onMediaData(const QByteArray& data, uint64_t timestamp) override;
    bool canAcceptMedia() const override { return channelOpen_ && streaming_; }

    // Video focus control — called by orchestrator
    void requestVideoFocus(bool focused);

    /// Set how many video configs were advertised (for setup response)
    void setNumVideoConfigs(uint32_t n) { numVideoConfigs_ = n; }

signals:
    void videoFrameData(const QByteArray& data, uint64_t timestamp);
    void streamStarted(int32_t session, uint32_t configIndex);
    void streamStopped();
    void videoFocusChanged(int focusMode, bool unrequested);

private:
    void handleSetupRequest(const QByteArray& payload);
    void handleStartIndication(const QByteArray& payload);
    void handleStopIndication();
    void handleVideoFocusRequest(const QByteArray& payload);
    void handleVideoFocusIndication(const QByteArray& payload);
    void sendAck();

    int32_t session_ = -1;
    uint32_t ackCounter_ = 0;
    uint32_t numVideoConfigs_ = 1;
    bool channelOpen_ = false;
    bool streaming_ = false;
};

} // namespace hu
} // namespace oaa
