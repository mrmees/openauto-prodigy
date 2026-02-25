#include <oaa/Channel/ControlChannel.hpp>
#include <QtEndian>
#include <QDebug>

#include "PingRequestMessage.pb.h"
#include "PingResponseMessage.pb.h"
#include "ChannelOpenRequestMessage.pb.h"
#include "ChannelOpenResponseMessage.pb.h"
#include "AuthCompleteIndicationMessage.pb.h"
#include "ShutdownRequestMessage.pb.h"
#include "ShutdownResponseMessage.pb.h"
#include "StatusEnum.pb.h"
#include "ShutdownReasonEnum.pb.h"
#include "CallAvailabilityMessage.pb.h"
#include "ServiceDiscoveryRequestMessage.pb.h"

namespace oaa {

namespace {
// Control channel message IDs
constexpr uint16_t MSG_VERSION_REQUEST           = 0x0001;
constexpr uint16_t MSG_VERSION_RESPONSE          = 0x0002;
constexpr uint16_t MSG_SSL_HANDSHAKE             = 0x0003;
constexpr uint16_t MSG_AUTH_COMPLETE             = 0x0004;
constexpr uint16_t MSG_SERVICE_DISCOVERY_REQUEST = 0x0005;
constexpr uint16_t MSG_SERVICE_DISCOVERY_RESPONSE = 0x0006;
constexpr uint16_t MSG_CHANNEL_OPEN_REQUEST      = 0x0007;
constexpr uint16_t MSG_CHANNEL_OPEN_RESPONSE     = 0x0008;
constexpr uint16_t MSG_PING_REQUEST              = 0x000b;
constexpr uint16_t MSG_PING_RESPONSE             = 0x000c;
constexpr uint16_t MSG_NAV_FOCUS_REQUEST         = 0x000d;
constexpr uint16_t MSG_SHUTDOWN_REQUEST          = 0x000f;
constexpr uint16_t MSG_SHUTDOWN_RESPONSE         = 0x0010;
constexpr uint16_t MSG_VOICE_SESSION_REQUEST     = 0x0011;
constexpr uint16_t MSG_AUDIO_FOCUS_REQUEST       = 0x0012;
constexpr uint16_t MSG_AUDIO_FOCUS_RESPONSE      = 0x0013;
constexpr uint16_t MSG_CHANNEL_CLOSE             = 0x0009;
constexpr uint16_t MSG_CALL_AVAILABILITY         = 0x0018;
constexpr uint16_t MSG_SERVICE_DISCOVERY_UPDATE  = 0x001a;
} // namespace

ControlChannel::ControlChannel(QObject* parent)
    : IChannelHandler(parent) {}

ControlChannel::~ControlChannel() = default;

uint8_t ControlChannel::channelId() const { return 0; }

void ControlChannel::onChannelOpened() {
    qDebug() << "[ControlChannel] opened";
}

void ControlChannel::onChannelClosed() {
    qDebug() << "[ControlChannel] closed";
}

void ControlChannel::onMessage(uint16_t messageId, const QByteArray& payload) {
    switch (messageId) {

    case MSG_VERSION_RESPONSE: {
        // Raw binary: major(2B BE) + minor(2B BE) + status(2B BE)
        if (payload.size() < 6) {
            emit versionReceived(0, 0, false);
            return;
        }
        uint16_t major = qFromBigEndian<uint16_t>(
            reinterpret_cast<const uchar*>(payload.constData()));
        uint16_t minor = qFromBigEndian<uint16_t>(
            reinterpret_cast<const uchar*>(payload.constData() + 2));
        uint16_t status = qFromBigEndian<uint16_t>(
            reinterpret_cast<const uchar*>(payload.constData() + 4));
        emit versionReceived(major, minor, status == 0x0000);
        break;
    }

    case MSG_SSL_HANDSHAKE:
        emit sslHandshakeData(payload);
        break;

    case MSG_SERVICE_DISCOVERY_REQUEST: {
        proto::messages::ServiceDiscoveryRequest sdReq;
        if (sdReq.ParseFromArray(payload.constData(), payload.size())) {
            qInfo() << "[ControlChannel] ServiceDiscoveryRequest:"
                     << QString::fromStdString(sdReq.ShortDebugString());
        }
        emit serviceDiscoveryRequested(payload);
        break;
    }

    case MSG_CHANNEL_OPEN_REQUEST: {
        proto::messages::ChannelOpenRequest req;
        if (req.ParseFromArray(payload.constData(), payload.size())) {
            qInfo() << "[ControlChannel] ChannelOpenRequest:"
                     << QString::fromStdString(req.ShortDebugString());
            emit channelOpenRequested(
                static_cast<uint8_t>(req.channel_id()), payload);
        } else {
            qWarning() << "[ControlChannel] Failed to parse ChannelOpenRequest";
        }
        break;
    }

    case MSG_PING_REQUEST: {
        proto::messages::PingRequest req;
        if (req.ParseFromArray(payload.constData(), payload.size())) {
            int64_t ts = req.timestamp();
            // Auto-respond with PingResponse
            sendPingResponse(ts);
            emit pingReceived(ts);
        } else {
            qWarning() << "[ControlChannel] Failed to parse PingRequest";
        }
        break;
    }

    case MSG_PING_RESPONSE: {
        proto::messages::PingResponse resp;
        if (resp.ParseFromArray(payload.constData(), payload.size())) {
            emit pongReceived(resp.timestamp());
        }
        break;
    }

    case MSG_NAV_FOCUS_REQUEST:
        emit navigationFocusRequested(payload);
        break;

    case MSG_SHUTDOWN_REQUEST: {
        proto::messages::ShutdownRequest req;
        if (req.ParseFromArray(payload.constData(), payload.size())) {
            emit shutdownRequested(static_cast<int>(req.reason()));
        } else {
            emit shutdownRequested(0);
        }
        break;
    }

    case MSG_SHUTDOWN_RESPONSE:
        emit shutdownAcknowledged();
        break;

    case MSG_VOICE_SESSION_REQUEST:
        emit voiceSessionRequested(payload);
        break;

    case MSG_AUDIO_FOCUS_REQUEST:
        emit audioFocusRequested(payload);
        break;

    case MSG_CHANNEL_CLOSE:
        qDebug() << "[ControlChannel] channel close notification";
        break;

    case MSG_CALL_AVAILABILITY:
        qDebug() << "[ControlChannel] call availability (unexpected direction)";
        break;

    case MSG_SERVICE_DISCOVERY_UPDATE:
        qDebug() << "[ControlChannel] service discovery update";
        break;

    default:
        emit unknownMessage(messageId, payload);
        break;
    }
}

void ControlChannel::sendVersionRequest(uint16_t major, uint16_t minor) {
    QByteArray payload(4, '\0');
    qToBigEndian(major, reinterpret_cast<uchar*>(payload.data()));
    qToBigEndian(minor, reinterpret_cast<uchar*>(payload.data() + 2));
    emit sendRequested(0, MSG_VERSION_REQUEST, payload);
}

void ControlChannel::sendAuthComplete(bool success) {
    proto::messages::AuthCompleteIndication msg;
    msg.set_status(success ? proto::enums::Status::OK
                           : proto::enums::Status::AUTHENTICATION_FAILURE);
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, MSG_AUTH_COMPLETE, payload);
}

void ControlChannel::sendChannelOpenResponse(uint8_t targetChannelId, bool accepted) {
    proto::messages::ChannelOpenResponse msg;
    msg.set_status(accepted ? proto::enums::Status::OK
                            : proto::enums::Status::INVALID_CHANNEL);
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, MSG_CHANNEL_OPEN_RESPONSE, payload);
}

void ControlChannel::sendPingRequest(int64_t timestamp) {
    proto::messages::PingRequest msg;
    msg.set_timestamp(timestamp);
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, MSG_PING_REQUEST, payload);
}

void ControlChannel::sendPingResponse(int64_t timestamp) {
    proto::messages::PingResponse msg;
    msg.set_timestamp(timestamp);
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, MSG_PING_RESPONSE, payload);
}

void ControlChannel::sendShutdownRequest(int reason) {
    proto::messages::ShutdownRequest msg;
    msg.set_reason(static_cast<proto::enums::ShutdownReason::Enum>(reason));
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, MSG_SHUTDOWN_REQUEST, payload);
}

void ControlChannel::sendShutdownResponse() {
    proto::messages::ShutdownResponse msg;
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, MSG_SHUTDOWN_RESPONSE, payload);
}

void ControlChannel::sendAudioFocusResponse(const QByteArray& payload) {
    emit sendRequested(0, 0x0013, payload);
}

void ControlChannel::sendNavigationFocusResponse(const QByteArray& payload) {
    emit sendRequested(0, 0x000e, payload);
}

void ControlChannel::sendCallAvailability(bool available) {
    proto::messages::CallAvailabilityStatus msg;
    msg.set_call_available(available);
    QByteArray payload(msg.ByteSizeLong(), '\0');
    msg.SerializeToArray(payload.data(), payload.size());
    emit sendRequested(0, MSG_CALL_AVAILABILITY, payload);
}

} // namespace oaa
