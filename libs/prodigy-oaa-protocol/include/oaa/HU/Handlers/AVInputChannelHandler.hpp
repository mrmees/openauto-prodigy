#pragma once

#include <functional>

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oaa {
namespace hu {

class AVInputChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    using CaptureController = std::function<bool(bool open)>;

    explicit AVInputChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::AVInput; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

    // Installs the application-owned capture lifecycle edge. Invoked
    // synchronously on this handler's Qt owner thread before the open response
    // is sent. With no controller installed, open requests fail closed.
    void setCaptureController(CaptureController controller);

    // Send mic data upstream to the phone. Must be called on this handler's Qt
    // owner thread, never from a PipeWire real-time callback. Returns false
    // when the channel/capture is closed or the phone's transmit window is full.
    bool sendMicData(const QByteArray& data, uint64_t timestamp);

    // Application-side runtime failure/teardown edge. Disables sending and
    // resets permits without invoking the capture controller or sending an
    // unsolicited protocol response.
    void abortCapture();

signals:
    void micCaptureRequested(bool open);
    void sendWindowAvailable();

private:
    void handleSetupRequest(const QByteArray& payload);
    void handleInputOpenRequest(const QByteArray& payload);
    void handleAckIndication(const QByteArray& payload);

    void stopCapture();
    void sendInputOpenResponse(bool success);

    CaptureController captureController_;
    bool channelOpen_ = false;
    bool capturing_ = false;
    int32_t session_ = 0;
    int32_t maxUnacked_ = 1;
    uint32_t unacked_ = 0;
};

} // namespace hu
} // namespace oaa
