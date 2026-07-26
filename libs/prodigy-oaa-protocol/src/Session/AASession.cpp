#include <oaa/Session/AASession.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <oaa/Version.hpp>
#include <QDateTime>
#include <QDebug>

#include "oaa/control/ServiceDiscoveryRequestMessage.pb.h"
#include "oaa/control/ControlChannelConfigMessage.pb.h"
#include "oaa/control/ServiceDiscoveryResponseMessage.pb.h"
#include "oaa/control/ChannelDescriptorData.pb.h"
#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/audio/AudioFocusRequestMessage.pb.h"
#include "oaa/audio/AudioFocusResponseMessage.pb.h"
#include "oaa/audio/AudioFocusTypeEnum.pb.h"
#include "oaa/audio/AudioFocusStateEnum.pb.h"
#include "oaa/common/DriverPositionEnum.pb.h"

namespace oaa {

namespace {

bool supportsExplicitReopenLifecycle(uint8_t channelId)
{
    // CLUSTER channels use replacement opens as an observed renegotiation
    // boundary and must not receive traffic before that boundary. Preserve the
    // established open/dispatch behavior of every legacy channel; this
    // experimental path must not redefine their lifecycle contract.
    return channelId == ChannelId::ClusterVideo
        || channelId == ChannelId::ClusterInput;
}

} // namespace

AASession::AASession(ITransport* transport, const SessionConfig& config,
                     QObject* parent)
    : QObject(parent)
    , config_(config)
    , transport_(transport)
    , messenger_(new Messenger(transport, this))
    , controlChannel_(new ControlChannel(this))
{
    const SessionConfig defaults;
    if (config_.pingInterval <= 0) {
        qWarning() << "[AASession] Invalid ping interval"
                   << config_.pingInterval << "— using"
                   << defaults.pingInterval << "ms";
        config_.pingInterval = defaults.pingInterval;
    }
    if (config_.pingTimeout <= 0) {
        qWarning() << "[AASession] Invalid ping timeout"
                   << config_.pingTimeout << "— using"
                   << defaults.pingTimeout << "ms";
        config_.pingTimeout = defaults.pingTimeout;
    }

    stateTimer_.setSingleShot(true);
    pongDeadlineTimer_.setSingleShot(true);
    connect(&stateTimer_, &QTimer::timeout, this, &AASession::onStateTimeout);
    connect(&pingTimer_, &QTimer::timeout, this, &AASession::onPingTick);
    connect(&pongDeadlineTimer_, &QTimer::timeout,
            this, &AASession::onPongDeadline);

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
    connect(messenger_, &Messenger::handshakeFailed,
            this, &AASession::onHandshakeFailed);
    connect(messenger_, &Messenger::tlsFailed,
            this, &AASession::onTlsFailed);
    connect(messenger_, &Messenger::protocolFailed,
            this, &AASession::onProtocolFailed);

    // ControlChannel signals
    connect(controlChannel_, &ControlChannel::versionReceived,
            this, &AASession::onVersionReceived);
    connect(controlChannel_, &ControlChannel::versionResponseMalformed,
            this, &AASession::onVersionResponseMalformed);
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
                case proto::enums::AudioFocusType::GAIN_TRANSIENT_MAY_DUCK:
                    state = proto::enums::AudioFocusState::GAIN_TRANSIENT_GUIDANCE_ONLY; break;
                case proto::enums::AudioFocusType::RELEASE:
                    state = proto::enums::AudioFocusState::LOSS; break;
                default:
                    state = proto::enums::AudioFocusState::INVALID; break;
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
    // External channel handlers and the transport can already be gone during
    // QObject parent-chain destruction. Never initiate protocol work or call
    // through their raw pointers here; the owner uses finalize() while they
    // are alive.
    channels_.clear();
}

void AASession::start() {
    if (finalized_)
        return;
    if (state_ != SessionState::Idle && state_ != SessionState::Disconnected)
        return;

    messenger_->start();
    channelsClosed_ = false;
    warnedNonControlOpenChannels_.clear();
    warnedLegacyCloseChannels_.clear();
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
        stopLivenessTimers();
        setState(SessionState::ShuttingDown);
        if (finalized_ || state_ != SessionState::ShuttingDown)
            return;
        startStateTimer(5000); // 5s shutdown timeout
        controlChannel_->sendShutdownRequest(reason);
    } else {
        // Not yet active, just disconnect
        stopStateTimer();
        stopLivenessTimers();
        setState(SessionState::Disconnected);
        emit disconnected(DisconnectReason::UserRequested);
    }
}

void AASession::finalize() {
    if (finalized_)
        return;
    finalized_ = true;

    const bool publishStateChange = state_ != SessionState::Idle
                                    && state_ != SessionState::Disconnected;
    if (publishStateChange)
        state_ = SessionState::Disconnected;

    stopStateTimer();
    stopLivenessTimers();
    messenger_->stop();
    closeChannels();
    for (auto* handler : channels_)
        disconnectHandler(handler);
    channels_.clear();

    if (publishStateChange) {
        qDebug() << "[AASession] State:" << static_cast<int>(state_);
        emit stateChanged(state_);
    }
}

void AASession::registerChannel(uint8_t channelId, IChannelHandler* handler) {
    if (finalized_ || !handler)
        return;

    auto existing = channels_.find(channelId);
    if (existing != channels_.end() && existing.value() != handler) {
        const bool wasOpen = openChannels_.contains(channelId);
        disconnectHandler(existing.value());
        if (wasOpen)
            existing.value()->onChannelClosed();
        openChannels_.remove(channelId);
        channels_[channelId] = handler;
        if (wasOpen && !finalized_ && state_ == SessionState::Active) {
            connectHandler(handler);
            openChannels_.insert(channelId);
            handler->onChannelOpened();
        }
        return;
    }
    channels_[channelId] = handler;
}

SessionState AASession::state() const { return state_; }
Messenger* AASession::messenger() const { return messenger_; }
ControlChannel* AASession::controlChannel() const { return controlChannel_; }

void AASession::setState(SessionState newState) {
    if (state_ == newState) return;
    state_ = newState;

    if (newState == SessionState::Disconnected)
        closeChannels();

    qDebug() << "[AASession] State:" << static_cast<int>(newState);
    emit stateChanged(newState);
}

void AASession::connectHandler(IChannelHandler* handler) {
    connect(handler, &IChannelHandler::sendRequested,
            messenger_, &Messenger::sendMessage, Qt::UniqueConnection);
}

void AASession::disconnectHandler(IChannelHandler* handler) {
    disconnect(handler, &IChannelHandler::sendRequested,
               messenger_, &Messenger::sendMessage);
}

bool AASession::closeServiceChannel(uint8_t channelId) {
    if (!openChannels_.contains(channelId))
        return false;
    auto it = channels_.find(channelId);
    if (it == channels_.end()) {
        openChannels_.remove(channelId);
        return false;
    }

    IChannelHandler* handler = it.value();
    openChannels_.remove(channelId);
    disconnectHandler(handler);
    handler->onChannelClosed();
    if (!finalized_)
        emit channelClosed(channelId);
    return true;
}

void AASession::closeChannels() {
    stopStateTimer();
    stopLivenessTimers();
    messenger_->stop();

    if (channelsClosed_)
        return;
    channelsClosed_ = true;

    controlChannel_->onChannelClosed();
    for (auto* handler : channels_) {
        disconnectHandler(handler);
        handler->onChannelClosed();
    }
    openChannels_.clear();
}

void AASession::startStateTimer(int timeoutMs) {
    stateTimer_.start(timeoutMs);
}

void AASession::stopStateTimer() {
    stateTimer_.stop();
}

void AASession::stopLivenessTimers() {
    pingTimer_.stop();
    pongDeadlineTimer_.stop();
    outstandingPingTimestamps_.clear();
}

void AASession::onTransportConnected() {
    if (state_ != SessionState::Connecting)
        return;

    qDebug() << "[AASession] Transport connected, sending VERSION_REQUEST";
    setState(SessionState::VersionExchange);
    if (finalized_ || state_ != SessionState::VersionExchange)
        return;
    startStateTimer(config_.versionTimeout);
    controlChannel_->sendVersionRequest(config_.protocolMajor, config_.protocolMinor);
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

void AASession::onVersionReceived(uint16_t major, uint16_t minor,
                                  uint16_t rawStatus,
                                  const QByteArray& trailingBytes) {
    if (state_ != SessionState::VersionExchange) return;
    stopStateTimer();

    const QString requestedVersion = QStringLiteral("%1.%2")
                                         .arg(config_.protocolMajor)
                                         .arg(config_.protocolMinor);
    const QString reportedVersion = QStringLiteral("%1.%2")
                                        .arg(major)
                                        .arg(minor);
    const QString statusText = QStringLiteral("0x%1")
                                   .arg(rawStatus, 4, 16, QLatin1Char('0'));
    qDebug().noquote() << "[AASession] Version response requested="
                       << requestedVersion << "reported=" << reportedVersion
                       << "status=" << statusText
                       << "trailing_bytes=" << trailingBytes.size();

    if (!trailingBytes.isEmpty()) {
        proto::messages::ControlChannelConfigWrapper wrapper;
        if (wrapper.ParseFromArray(trailingBytes.constData(),
                                   trailingBytes.size())) {
            const QString summary = QString::fromStdString(
                                        wrapper.ShortDebugString())
                                        .left(512);
            qDebug().noquote()
                << "[AASession] Version response control config:" << summary;
        } else {
            qWarning().noquote()
                << "[AASession] Version response optional control config"
                << "is opaque or malformed; bytes=" << trailingBytes.size()
                << "prefix=" << trailingBytes.left(16).toHex();
        }
    }

    const bool statusMatches = rawStatus == 0x0000;
    const bool reportedVersionIsCompatible = major > config_.protocolMajor
        || (major == config_.protocolMajor && minor >= config_.protocolMinor);
    if (!statusMatches
        || (config_.requireMinimumCompatibleProtocolVersion
            && !reportedVersionIsCompatible)) {
        setState(SessionState::Disconnected);
        emit disconnected(DisconnectReason::VersionMismatch);
        return;
    }

    // Enter the state before the initial synchronous handshake drive so a
    // fatal error emitted by that drive cannot be mistaken for stale input.
    setState(SessionState::TLSHandshake);
    if (finalized_ || state_ != SessionState::TLSHandshake)
        return;
    startStateTimer(config_.handshakeTimeout);
    startTlsHandshake();
}

void AASession::onVersionResponseMalformed(int payloadSize) {
    if (state_ != SessionState::VersionExchange)
        return;
    stopStateTimer();
    qWarning() << "[AASession] Malformed VERSION_RESPONSE fixed prefix:"
               << payloadSize << "bytes; expected at least 6";
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::VersionMismatch);
}

void AASession::startTlsHandshake() {
    messenger_->startHandshake();
}

void AASession::onHandshakeComplete() {
    if (state_ != SessionState::TLSHandshake) return;
    stopStateTimer();

    qDebug() << "[AASession] TLS handshake complete, sending AUTH_COMPLETE";
    setState(SessionState::ServiceDiscovery);
    if (finalized_ || state_ != SessionState::ServiceDiscovery)
        return;
    startStateTimer(config_.discoveryTimeout);
    controlChannel_->sendAuthComplete(true);
}

void AASession::onHandshakeFailed(const QString& message) {
    if (state_ != SessionState::TLSHandshake)
        return;

    qWarning() << "[AASession] TLS handshake failed:" << message;
    stopStateTimer();
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::HandshakeError);
}

void AASession::onTlsFailed(const QString& message) {
    if (state_ == SessionState::Idle || state_ == SessionState::Disconnected)
        return;

    if (state_ == SessionState::TLSHandshake) {
        onHandshakeFailed(message);
        return;
    }

    qWarning() << "[AASession] TLS runtime failure:" << message;
    stopStateTimer();
    stopLivenessTimers();
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::TlsError);
}

void AASession::onProtocolFailed(const QString& message) {
    if (state_ == SessionState::Idle || state_ == SessionState::Disconnected)
        return;

    qWarning() << "[AASession] Protocol framing failure:" << message;
    stopStateTimer();
    stopLivenessTimers();
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::ProtocolError);
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
    if (finalized_ || state_ != SessionState::ServiceDiscovery)
        return;

    // Enter active state
    setState(SessionState::Active);
    if (finalized_ || state_ != SessionState::Active)
        return;
    controlChannel_->onChannelOpened();
    pingTimer_.start(config_.pingInterval);
    onPingTick();
}

void AASession::onChannelOpenRequested(int32_t channelId, const QByteArray& /*payload*/) {
    if (state_ != SessionState::Active) return;

    if (channelId <= 0 || channelId > UINT8_MAX) {
        qWarning() << "[AASession] Rejecting invalid channel id" << channelId;
        emit channelOpenRejected(channelId);
        return;
    }

    const auto targetChannel = static_cast<uint8_t>(channelId);

    if (channels_.contains(targetChannel)) {
        qDebug() << "[AASession] Opening channel" << targetChannel;
        IChannelHandler* handler = channels_.value(targetChannel);
        if (openChannels_.contains(targetChannel)
            && supportsExplicitReopenLifecycle(targetChannel)) {
            closeServiceChannel(targetChannel);
            if (finalized_ || state_ != SessionState::Active
                || channels_.value(targetChannel, nullptr) != handler)
                return;
        }
        controlChannel_->sendChannelOpenResponse(targetChannel, true);
        if (finalized_ || state_ != SessionState::Active
            || channels_.value(targetChannel, nullptr) != handler)
            return;
        connectHandler(handler);
        openChannels_.insert(targetChannel);
        handler->onChannelOpened();
        if (finalized_ || state_ != SessionState::Active
            || channels_.value(targetChannel, nullptr) != handler)
            return;
        emit channelOpened(targetChannel);
    } else {
        qDebug() << "[AASession] Rejecting channel" << targetChannel << "(not registered)";
        controlChannel_->sendChannelOpenResponse(targetChannel, false);
        emit channelOpenRejected(channelId);
    }
}

void AASession::onMessage(uint8_t channelId, uint16_t messageId,
                          const QByteArray& payload, int dataOffset,
                          MessageType messageType) {
    const int dataSize = payload.size() - dataOffset;

    // Log ALL incoming messages for debugging
    qDebug() << "[AASession] RX ch" << channelId << "msgId" << Qt::hex << messageId
             << "len" << dataSize;

    // Channel 0 → ControlChannel
    if (channelId == 0) {
        controlChannel_->onMessage(messageId, payload, dataOffset);
        return;
    }

    // Service-channel close notifications carry their target in the frame's
    // channel id. Only the experimental CLUSTER channels opt into this
    // explicit lifecycle; preserve established legacy dispatch semantics.
    if (messageId == SessionMessageId::CHANNEL_CLOSE_NOTIFICATION
        && messageType == MessageType::Control) {
        if (supportsExplicitReopenLifecycle(channelId)) {
            if (state_ == SessionState::Active)
                closeServiceChannel(channelId);
            return;
        }
        if (channels_.contains(channelId)
            && !warnedLegacyCloseChannels_.contains(channelId)) {
            warnedLegacyCloseChannels_.insert(channelId);
            qWarning() << "[AASession] legacy channel" << channelId
                       << "control close uses compatibility passthrough";
        }
    }

    // CHANNEL_OPEN_REQUEST (0x0007) arrives on the TARGET channel, not ch0.
    // Handle it here and respond on the same channel.
    if (messageId == SessionMessageId::CHANNEL_OPEN_REQUEST
        && messageType == MessageType::Control) {
        if (state_ != SessionState::Active) return;

        proto::messages::ChannelOpenRequest req;
        if (req.ParseFromArray(payload.constData() + dataOffset, dataSize)) {
            const int32_t requestedChannel = req.channel_id();
            if (requestedChannel <= 0 || requestedChannel > UINT8_MAX
                || requestedChannel != channelId) {
                qWarning() << "[AASession] Rejecting channel-open request on"
                           << channelId << "for invalid/mismatched channel"
                           << requestedChannel;
                emit channelOpenRejected(requestedChannel);
                return;
            }

            const auto targetCh = static_cast<uint8_t>(requestedChannel);
            if (channels_.contains(targetCh)) {
                qDebug() << "[AASession] Opening channel" << targetCh;
                IChannelHandler* handler = channels_.value(targetCh);
                if (openChannels_.contains(targetCh)
                    && supportsExplicitReopenLifecycle(targetCh)) {
                    closeServiceChannel(targetCh);
                    if (finalized_ || state_ != SessionState::Active
                        || channels_.value(targetCh, nullptr) != handler)
                        return;
                }
                // Response goes on the target channel, not ch0.
                controlChannel_->sendChannelOpenResponse(targetCh, true);
                if (finalized_ || state_ != SessionState::Active
                    || channels_.value(targetCh, nullptr) != handler)
                    return;
                connectHandler(handler);
                openChannels_.insert(targetCh);
                handler->onChannelOpened();
                if (finalized_ || state_ != SessionState::Active
                    || channels_.value(targetCh, nullptr) != handler)
                    return;
                emit channelOpened(targetCh);
            } else {
                qDebug() << "[AASession] Rejecting channel" << targetCh
                         << "(not registered)";
                controlChannel_->sendChannelOpenResponse(targetCh, false);
                emit channelOpenRejected(targetCh);
            }
        }
        return;
    }

    if (!openChannels_.contains(channelId)
        && supportsExplicitReopenLifecycle(channelId)) {
        if (channels_.contains(channelId)
            && messageId == SessionMessageId::CHANNEL_OPEN_REQUEST) {
            proto::messages::ChannelOpenRequest request;
            const bool isMatchingOpenRequest =
                request.ParseFromArray(
                    payload.constData() + dataOffset, dataSize)
                && request.channel_id() == channelId;
            if (isMatchingOpenRequest
                && !warnedNonControlOpenChannels_.contains(channelId)) {
                warnedNonControlOpenChannels_.insert(channelId);
                qWarning()
                    << "[AASession] Ignoring non-control channel-open request on"
                    << channelId << "message type"
                    << static_cast<int>(messageType);
            }
        }

        qDebug() << "[AASession] Dropping traffic for closed channel"
                 << channelId << "msgId" << Qt::hex << messageId;
        return;
    }

    auto it = channels_.find(channelId);
    if (it == channels_.end()) {
        qDebug() << "[AA:unhandled] ch" << channelId
                 << "msgId" << QString("0x%1").arg(messageId, 4, 16, QChar('0'))
                 << "len" << dataSize
                 << "hex:" << payload.mid(dataOffset, qMin(dataSize, 128)).toHex(' ');
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

    lastPingTimestamp_ = QDateTime::currentMSecsSinceEpoch();
    outstandingPingTimestamps_.insert(lastPingTimestamp_);
    controlChannel_->sendPingRequest(lastPingTimestamp_);
    if (state_ == SessionState::Active
        && outstandingPingTimestamps_.contains(lastPingTimestamp_)
        && !pongDeadlineTimer_.isActive()) {
        pongDeadlineTimer_.start(config_.pingTimeout);
    }
}

void AASession::onPongReceived(int64_t timestamp) {
    if (state_ != SessionState::Active
        || !outstandingPingTimestamps_.contains(timestamp)) {
        return;
    }

    outstandingPingTimestamps_.clear();
    pongDeadlineTimer_.stop();
}

void AASession::onPongDeadline() {
    if (state_ != SessionState::Active)
        return;

    qWarning() << "[AASession] Pong deadline expired after"
               << config_.pingTimeout << "ms";
    stopLivenessTimers();
    setState(SessionState::Disconnected);
    emit disconnected(DisconnectReason::PingTimeout);
}

void AASession::onShutdownRequested(int reason) {
    qDebug() << "[AASession] Phone requested shutdown, reason:" << reason;
    controlChannel_->sendShutdownResponse();
    if (finalized_ || state_ == SessionState::Disconnected)
        return;
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

    if (config_.sessionConfiguration != 0) {
        resp.set_session_configuration(config_.sessionConfiguration);
    }

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
