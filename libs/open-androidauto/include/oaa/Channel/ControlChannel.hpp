#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <cstdint>

namespace oaa {

class ControlChannel : public IChannelHandler {
    Q_OBJECT
public:
    explicit ControlChannel(QObject* parent = nullptr);
    ~ControlChannel() override;

    uint8_t channelId() const override;
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

    // Outgoing protocol messages
    void sendVersionRequest(uint16_t major, uint16_t minor);
    void sendAuthComplete(bool success);
    void sendChannelOpenResponse(uint8_t targetChannelId, bool accepted);
    void sendPingRequest(int64_t timestamp);
    void sendPingResponse(int64_t timestamp);
    void sendShutdownRequest(int reason);
    void sendShutdownResponse();
    void sendAudioFocusResponse(const QByteArray& payload);
    void sendNavigationFocusResponse(const QByteArray& payload);
    void sendCallAvailability(bool available);

signals:
    void versionReceived(uint16_t major, uint16_t minor, bool match);
    void sslHandshakeData(const QByteArray& data);
    void serviceDiscoveryRequested(const QByteArray& payload);
    void channelOpenRequested(uint8_t channelId, const QByteArray& payload);
    void pingReceived(int64_t timestamp);
    void pongReceived(int64_t timestamp);
    void navigationFocusRequested(const QByteArray& payload);
    void shutdownRequested(int reason);
    void shutdownAcknowledged();
    void voiceSessionRequested(const QByteArray& payload);
    void audioFocusRequested(const QByteArray& payload);
};

} // namespace oaa
