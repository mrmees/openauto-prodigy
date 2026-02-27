#include <oaa/Session/AASession.hpp>
#include <oaa/Version.hpp>
#include <QDateTime>
#include <QDebug>

#include "oaa/control/ServiceDiscoveryRequestMessage.pb.h"
#include "oaa/control/ServiceDiscoveryResponseMessage.pb.h"
#include "oaa/control/ChannelDescriptorData.pb.h"
#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/control/ChannelOpenResponseMessage.pb.h"
#include "oaa/common/StatusEnum.pb.h"
#include "oaa/audio/AudioFocusRequestMessage.pb.h"
#include "oaa/audio/AudioFocusResponseMessage.pb.h"
#include "oaa/audio/AudioFocusTypeEnum.pb.h"
#include "oaa/audio/AudioFocusStateEnum.pb.h"
#include "oaa/common/DriverPositionEnum.pb.h"

namespace oaa {

AASession::AASession(ITransport* transport, const SessionConfig& config,
                     QObject* parent)
    : QObject(parent)
    , config_(config)
    , transport_(transport)
    , messenger_(new Messenger(transport, this))
    , controlChannel_(new ControlChannel(this))
{
    stateTimer_.setSingleShot(true);
    connect(&stateTimer_, &QTimer::timeout, this, &AASession::onStateTimeout);
    connect(&pingTimer_, &QTimer::timeout, this, &AASession::onPingTick);

    // Transport signals
    connect(transport_, &ITransport::connected,
            this, &AASession::onTransportConnected);
    connect(transport_, &ITransport::disconnected,
            this, &AASession::onTransportDisconnected);
    connect(transport_, &ITransport::error,
            this, &AASession::onTransportError);

    // Messenger signals
    connect(messenger_, &Messenger::messageReceived,
            this, &AASession::onMessage);
    connect(messenger_, &Messenger::handshakeComplete,
            this, &AASession::onHandshakeComplete);

    // ControlChannel signals
    connect(controlChannel_, &ControlChannel::versionReceived,
            this, &AASession::onVersionReceived);
    connect(controlChannel_, &ControlChannel::serviceDiscoveryRequested,
            this, &AASession::onServiceDiscoveryRequested);
    connect(controlChannel_, &ControlChannel::channelOpenRequested,
            this, &AASession::onChannelOpenRequested);
    connect(controlChannel_, &ControlChannel::pongReceived,
            this, &AASession::onPongReceived);
    connect(controlChannel_, &ControlChannel::shutdownRequested,
            this, &AASession::onShutdownRequested);
    connect(controlChannel_, &ControlChannel::shutdownAcknowledged,
            this, &AASession::onShutdownAcknowledged);

    // Auto-respond to audio focus and nav focus requests.
    // Maps AudioFocusType → AudioFocusState:
    //   GAIN(1)→GAIN(1), GAIN_TRANSIENT(2)→GAIN_TRANSIENT(2),
    //   GAIN_NAVI(3)→GAIN_TRANSIENT_GUIDANCE_ONLY(7), RELEASE(4)→LOSS(3)
    connect(controlChannel_, &ControlChannel::audioFocusRequested,
            this, [this](const QByteArray& payload) {
                proto::messages::AudioFocusRequest req;
                if (!req.ParseFromArray(payload.constData(), payload.size()))
                    return;
                auto type = req.audio_focus_type();
                proto::enums::AudioFocusState::Enum state;
                switch (type) {
                case proto::enums::AudioFocusType::GAIN:
                    state = proto::enums::AudioFocusState::GAIN; break;
                case proto::enums::AudioFocusType::GAIN_TRANSIENT:
                    state = proto::enums::AudioFocusState::GAIN_TRANSIENT; break;
                case proto::enums::AudioFocusType::GAIN_NAVI:
                    state = proto::enums::AudioFocusState::GAIN_TRANSIENT_GUIDANCE_ONLY; break;
                case proto::enums::AudioFocusType::RELEASE:
                    state = proto::enums::AudioFocusState::LOSS; break;
                default:
                    state = proto::enums::AudioFocusState::NONE; break;
                }
                qDebug() << "[AASession] Audio focus request type:" << (int)type
                         << "→ state:" << (int)state;
                proto::messages::AudioFocusResponse resp;
                resp.set_audio_focus_state(state);
                QByteArray data(resp.ByteSizeLong(), '\0');
                resp.SerializeToArray(data.data(), data.size());
                controlChannel_->sendAudioFocusResponse(data);

                // Notify app layer for PipeWire stream ducking
                emit audioFocusChanged(static_cast<int>(type));
            });
    // Log voice session requests (mic channel already streams audio —
    // phone may just need audio focus, not a separate response)
    connect(controlChannel_, &ControlChannel::voiceSessionRequested,
            this, [](const QByteArray& payload) {
                Q_UNUSED(payload)
                qDebug() << "[AASession] Voice session requested — mic channel active";
            });

    connect(controlChannel_, &ControlChannel::navigationFocusRequested,
            this, [this](const QByteArray& payload) {
                qDebug() << "[AASession] Nav focus request, auto-granting";
                controlChannel_->sendNavigationFocusResponse(payload);
            });

    // Route ControlChannel sends through Messenger
    connect(controlChannel_, &IChannelHandler::sendRequested,
            messenger_, &Messenger::sendMessage);
}

AASession::~AASession() {
    stop();
}

void AASession::start() {
    if (state_ != SessionState::Idle && state_ != SessionState::Disconnected)
        return;

    messenger_->start();
    setState(SessionState::Connecting);

    if (transport_->isConnected()) {
        // Already connected — advance immediately
        onTransportConnected();
    }
}

void AASession::stop(int reason) {
    if (state_ == SessionState::Disconnected || state_ == SessionState::Idle)
        return;

    if (state_ == SessionState::Active) {
        // Graceful shutdown
        qInfo() << "[AASession] Sending ShutdownRequest reason:" << reason;
        controlChannel_->sendShutdownRequest(reason);
        setState(SessionState::ShuttingDown);
        startStateTimer(5000); // 5s shutdown timeout
    } else {
        // Not yet active, just disconnect
        stopStateTimer();
        pingTimer_.stop();
        setState(SessionState::Disconnected);
        emit disconnected(DisconnectReason::UserRequested);
    }
}

void AASession::registerChannel(uint8_t channelId, IChannelHandler* handler) {
    channels_[channelId] = handler;
    // Route handler sends through Messenger
    connect(handler, &IChannelHandler::sendRequested,
            messenger_, &Messenger::sendMessage);
}

SessionState AASession::state() const { return state_; }
Messenger* AASession::messenger() const { return messenger_; }
ControlChannel* AASession::controlChannel() const { return controlChannel_; }

void AASession::setState(SessionState newState) {
    if (state_ == newState) return;
    state_ = newState;
    qDebug() << "[AASession] State:" << static_cast<int>(newState);
    emit stateChanged(newState);

    if (newState == SessionState::Disconnected) {
        stopStateTimer();
        pingTimer_.stop();
        controlChannel_->onChannelClosed();
        for (auto* handler : channels_) {
            handler->onChannelClosed();
        }
    }
}

void AASession::startStateTimer(int timeoutMs) {
    stateTimer_.start(timeoutMs);
}

void AASession::stopStateTimer() {
    stateTimer_.stop();
}

void AASession::onTransportConnected() {
    if (state_ != SessionState::Connecting)
        return;

    qDebug() << "[AASession] Transport connected, sending VERSION_REQUEST";
    controlChannel_->sendVersionRequest(config_.protocolMajor, config_.protocolMinor);
    setState(SessionState::VersionExchange);
    startStateTimer(config_.versionTimeout);
}

void AASession::onTransportDisconnected() {
    if (state_ == SessionState::Disconnected) return;
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::TransportError);
}

void AASession::onTransportError(const QString& message) {
    qWarning() << "[AASession] Transport error:" << message;
    if (state_ == SessionState::Disconnected) return;
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::TransportError);
}

void AASession::onVersionReceived(uint16_t major, uint16_t minor, bool match) {
    if (state_ != SessionState::VersionExchange) return;
    stopStateTimer();

    qDebug() << "[AASession] Version response:" << major << "." << minor
             << (match ? "MATCH" : "MISMATCH");

    if (!match) {
        setState(SessionState::Disconnected);
        emit disconnected(DisconnectReason::VersionMismatch);
        return;
    }

    // Start TLS handshake
    messenger_->startHandshake();
    setState(SessionState::TLSHandshake);
    startStateTimer(config_.handshakeTimeout);
}

void AASession::onHandshakeComplete() {
    if (state_ != SessionState::TLSHandshake) return;
    stopStateTimer();

    qDebug() << "[AASession] TLS handshake complete, sending AUTH_COMPLETE";
    controlChannel_->sendAuthComplete(true);
    setState(SessionState::ServiceDiscovery);
    startStateTimer(config_.discoveryTimeout);
}

void AASession::onServiceDiscoveryRequested(const QByteArray& payload) {
    if (state_ != SessionState::ServiceDiscovery) return;
    stopStateTimer();

    // Parse request for logging
    proto::messages::ServiceDiscoveryRequest req;
    if (req.ParseFromArray(payload.constData(), payload.size())) {
        qDebug() << "[AASession] Service discovery from:"
                 << QString::fromStdString(req.device_name())
                 << QString::fromStdString(req.device_brand());
    }

    // Build and send response
    QByteArray response = buildServiceDiscoveryResponse();
    messenger_->sendMessage(0, 0x0006, response);

    // Enter active state
    setState(SessionState::Active);
    controlChannel_->onChannelOpened();
    missedPings_ = 0;
    pingTimer_.start(config_.pingInterval);
}

void AASession::onChannelOpenRequested(uint8_t channelId, const QByteArray& /*payload*/) {
    if (state_ != SessionState::Active) return;

    if (channels_.contains(channelId)) {
        qDebug() << "[AASession] Opening channel" << channelId;
        controlChannel_->sendChannelOpenResponse(channelId, true);
        channels_[channelId]->onChannelOpened();
        emit channelOpened(channelId);
    } else {
        qDebug() << "[AASession] Rejecting channel" << channelId << "(not registered)";
        controlChannel_->sendChannelOpenResponse(channelId, false);
        emit channelOpenRejected(channelId);
    }
}

void AASession::onMessage(uint8_t channelId, uint16_t messageId,
                          const QByteArray& payload, int dataOffset) {
    const int dataSize = payload.size() - dataOffset;

    // Log ALL incoming messages for debugging
    qDebug() << "[AASession] RX ch" << channelId << "msgId" << Qt::hex << messageId
             << "len" << dataSize;

    // Channel 0 → ControlChannel
    if (channelId == 0) {
        controlChannel_->onMessage(messageId, payload, dataOffset);
        return;
    }

    // CHANNEL_OPEN_REQUEST (0x0007) arrives on the TARGET channel, not ch0.
    // Handle it here and respond on the same channel.
    if (messageId == 0x0007) {
        if (state_ != SessionState::Active) return;

        proto::messages::ChannelOpenRequest req;
        if (req.ParseFromArray(payload.constData() + dataOffset, dataSize)) {
            uint8_t targetCh = static_cast<uint8_t>(req.channel_id());
            if (channels_.contains(targetCh)) {
                qDebug() << "[AASession] Opening channel" << targetCh;
                // Response goes on the target channel, not ch0
                proto::messages::ChannelOpenResponse resp;
                resp.set_status(proto::enums::Status::OK);
                QByteArray respData(resp.ByteSizeLong(), '\0');
                resp.SerializeToArray(respData.data(), respData.size());
                messenger_->sendMessage(targetCh, 0x0008, respData);
                channels_[targetCh]->onChannelOpened();
                emit channelOpened(targetCh);
            } else {
                qDebug() << "[AASession] Rejecting channel" << targetCh
                         << "(not registered)";
                proto::messages::ChannelOpenResponse resp;
                resp.set_status(proto::enums::Status::INVALID_CHANNEL);
                QByteArray respData(resp.ByteSizeLong(), '\0');
                resp.SerializeToArray(respData.data(), respData.size());
                messenger_->sendMessage(targetCh, 0x0008, respData);
                emit channelOpenRejected(targetCh);
            }
        }
        return;
    }

    auto it = channels_.find(channelId);
    if (it == channels_.end()) {
        qDebug() << "[AASession] Message for unregistered channel" << channelId
                 << "msgId" << Qt::hex << messageId;
        return;
    }

    IChannelHandler* handler = it.value();

    // AV media data — route to IAVChannelHandler if applicable
    if (messageId == 0x0000 || messageId == 0x0001) {
        auto* avHandler = qobject_cast<IAVChannelHandler*>(handler);
        if (avHandler) {
            const char* data = payload.constData() + dataOffset;
            if (messageId == 0x0000 && dataSize >= 8) {
                // AV_MEDIA_WITH_TIMESTAMP: first 8 bytes = uint64 BE timestamp
                uint64_t timestamp = 0;
                for (int i = 0; i < 8; ++i)
                    timestamp = (timestamp << 8) | static_cast<uint8_t>(data[i]);
                avHandler->onMediaData(payload.mid(dataOffset + 8), timestamp);
            } else {
                // AV_MEDIA_INDICATION: no timestamp
                avHandler->onMediaData(payload.mid(dataOffset), 0);
            }
            return;
        }
    }

    // Regular message dispatch — pass full payload with offset
    handler->onMessage(messageId, payload, dataOffset);
}

void AASession::onPingTick() {
    if (state_ != SessionState::Active) return;

    missedPings_++;
    if (missedPings_ > 3) {
        qWarning() << "[AASession] Ping timeout — missed" << missedPings_ << "pings";
        pingTimer_.stop();
        setState(SessionState::Disconnected);
        emit disconnected(DisconnectReason::PingTimeout);
        return;
    }

    lastPingTimestamp_ = QDateTime::currentMSecsSinceEpoch();
    controlChannel_->sendPingRequest(lastPingTimestamp_);
}

void AASession::onPongReceived(int64_t /*timestamp*/) {
    missedPings_ = 0;
}

void AASession::onShutdownRequested(int reason) {
    qDebug() << "[AASession] Phone requested shutdown, reason:" << reason;
    controlChannel_->sendShutdownResponse();
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::Normal);
}

void AASession::onShutdownAcknowledged() {
    if (state_ != SessionState::ShuttingDown) return;
    qDebug() << "[AASession] Shutdown acknowledged";
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::Normal);
}

void AASession::onStateTimeout() {
    qWarning() << "[AASession] State timeout in state" << static_cast<int>(state_);
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::Timeout);
}

QByteArray AASession::buildServiceDiscoveryResponse() const {
    proto::messages::ServiceDiscoveryResponse resp;

    resp.set_head_unit_name(config_.headUnitName.toStdString());
    resp.set_car_model(config_.carModel.toStdString());
    resp.set_car_year(config_.carYear.toStdString());
    resp.set_car_serial(config_.carSerial.toStdString());
    resp.set_driver_position(config_.leftHandDrive
        ? proto::enums::DriverPosition::LEFT
        : proto::enums::DriverPosition::RIGHT);
    resp.set_headunit_manufacturer(config_.manufacturer.toStdString());
    resp.set_headunit_model(config_.model.toStdString());
    resp.set_sw_build(config_.swBuild.toStdString());
    resp.set_sw_version(config_.swVersion.toStdString());
    resp.set_can_play_native_media_during_vr(config_.canPlayNativeMediaDuringVr);

    // Insert pre-built channel descriptors from config
    // These are fully populated by Prodigy's ServiceDiscoveryBuilder
    for (const auto& ch : config_.channels) {
        auto* desc = resp.add_channels();
        desc->ParseFromArray(ch.descriptor.constData(), ch.descriptor.size());
    }

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    return data;
}

} // namespace oaa
