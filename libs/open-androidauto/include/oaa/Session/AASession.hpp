#pragma once

#include <QObject>
#include <QTimer>
#include <QHash>

#include <oaa/Transport/ITransport.hpp>
#include <oaa/Messenger/Messenger.hpp>
#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/IAVChannelHandler.hpp>
#include <oaa/Channel/ControlChannel.hpp>
#include <oaa/Session/SessionState.hpp>
#include <oaa/Session/SessionConfig.hpp>

namespace oaa {

class AASession : public QObject {
    Q_OBJECT
public:
    AASession(ITransport* transport, const SessionConfig& config,
              QObject* parent = nullptr);
    ~AASession() override;

    void start();
    void stop();

    void registerChannel(uint8_t channelId, IChannelHandler* handler);
    SessionState state() const;
    Messenger* messenger() const;
    ControlChannel* controlChannel() const;

signals:
    void stateChanged(oaa::SessionState newState);
    void channelOpened(uint8_t channelId);
    void channelOpenRejected(uint8_t channelId);
    void disconnected(oaa::DisconnectReason reason);

private:
    void setState(SessionState newState);
    void startStateTimer(int timeoutMs);
    void stopStateTimer();

    // State handlers
    void onTransportConnected();
    void onTransportDisconnected();
    void onTransportError(const QString& message);
    void onVersionReceived(uint16_t major, uint16_t minor, bool match);
    void onHandshakeComplete();
    void onServiceDiscoveryRequested(const QByteArray& payload);
    void onChannelOpenRequested(uint8_t channelId, const QByteArray& payload);
    void onMessage(uint8_t channelId, uint16_t messageId,
                   const QByteArray& payload);
    void onPingTick();
    void onPongReceived(int64_t timestamp);
    void onShutdownRequested(int reason);
    void onShutdownAcknowledged();
    void onStateTimeout();

    QByteArray buildServiceDiscoveryResponse() const;

    SessionConfig config_;
    ITransport* transport_;
    Messenger* messenger_;
    ControlChannel* controlChannel_;
    QHash<uint8_t, IChannelHandler*> channels_;
    SessionState state_ = SessionState::Idle;

    QTimer stateTimer_;
    QTimer pingTimer_;
    int missedPings_ = 0;
    int64_t lastPingTimestamp_ = 0;
};

} // namespace oaa
