#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oaa {
namespace hu {

class PhoneStatusChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit PhoneStatusChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::PhoneStatus; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

    // PhoneCallState values from APK wae.java
    enum CallState {
        Unknown     = 0,
        Active      = 1,   // IN_CALL
        OnHold      = 2,
        Inactive    = 3,
        Ringing     = 4,   // INCOMING
        Conferenced = 5,
        Muted       = 6
    };

signals:
    /// Emitted when no calls are active (idle)
    void callsIdle();

    /// Emitted for each active call with its details
    void callStateChanged(int callState, const QString& number,
                           const QString& displayName, const QByteArray& contactPhoto);

private:
    void handlePhoneStatus(const QByteArray& payload);
};

} // namespace hu
} // namespace oaa
