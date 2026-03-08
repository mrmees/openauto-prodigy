#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <atomic>

namespace oaa {
namespace hu {

class InputChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    struct Pointer { int x; int y; int id; };

    explicit InputChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::Input; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

    // Called from EvdevTouchReader — builds and sends InputEventIndication
    void sendTouchIndication(int pointerCount, const Pointer* pointers,
                             int actionIndex, int action, uint64_t timestamp);

    // Send a button press/release event (e.g. media keys, home, back)
    void sendButtonEvent(uint32_t keycode, bool pressed, uint64_t timestamp);

signals:
    void hapticFeedbackRequested(int feedbackType);

private:
    void handleBindingRequest(const QByteArray& payload);
    void handleBindingNotification(const QByteArray& payload);
    std::atomic<bool> channelOpen_{false};
};

} // namespace hu
} // namespace oaa
