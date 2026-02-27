#include "oaa/HU/Handlers/AudioChannelHandler.hpp"

#include <QDebug>

#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/AVChannelSetupResponseMessage.pb.h"
#include "oaa/av/AVChannelSetupStatusEnum.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"
#include "oaa/av/AVChannelStopIndicationMessage.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"

namespace oaa {
namespace hu {

AudioChannelHandler::AudioChannelHandler(uint8_t channelId, QObject* parent)
    : oaa::IAVChannelHandler(parent)
    , channelId_(channelId)
{
}

void AudioChannelHandler::onChannelOpened()
{
    channelOpen_ = true;
    streaming_ = false;
    session_ = -1;

    qDebug() << "[AudioChannel" << channelId_ << "] opened";
}

void AudioChannelHandler::onChannelClosed()
{
    channelOpen_ = false;
    streaming_ = false;
    qDebug() << "[AudioChannel" << channelId_ << "] closed";
}

void AudioChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset)
{
    // Zero-copy view of data past the message ID header
    const QByteArray data = QByteArray::fromRawData(
        payload.constData() + dataOffset, payload.size() - dataOffset);

    switch (messageId) {
    case oaa::AVMessageId::SETUP_REQUEST:
        handleSetupRequest(data);
        break;
    case oaa::AVMessageId::START_INDICATION:
        handleStartIndication(data);
        break;
    case oaa::AVMessageId::STOP_INDICATION:
        handleStopIndication();
        break;
    case oaa::AVMessageId::VIDEO_FOCUS_NOTIFICATION:
    case oaa::AVMessageId::UPDATE_UI_CONFIG_REQUEST:
    case oaa::AVMessageId::UPDATE_UI_CONFIG_REPLY:
    case oaa::AVMessageId::AUDIO_UNDERFLOW:
    case oaa::AVMessageId::ACTION_TAKEN:
    case oaa::AVMessageId::OVERLAY_PARAMETERS:
    case oaa::AVMessageId::OVERLAY_START:
    case oaa::AVMessageId::OVERLAY_STOP:
    case oaa::AVMessageId::OVERLAY_SESSION_UPDATE:
    case oaa::AVMessageId::UPDATE_HU_UI_CONFIG_REQUEST:
    case oaa::AVMessageId::UPDATE_HU_UI_CONFIG_RESPONSE:
    case oaa::AVMessageId::MEDIA_STATS:
    case oaa::AVMessageId::MEDIA_OPTIONS:
        qDebug() << "[AudioChannel" << channelId_
                 << "] newer AV message:" << Qt::hex << messageId
                 << "size:" << data.size();
        break;
    default:
        qWarning() << "[AudioChannel" << channelId_
                   << "] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, data);
        break;
    }
}

void AudioChannelHandler::handleSetupRequest(const QByteArray& payload)
{
    oaa::proto::messages::AVChannelSetupRequest req;
    if (!req.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[AudioChannel" << channelId_ << "] failed to parse SetupRequest";
        return;
    }

    qInfo() << "[AudioChannel" << channelId_
            << "] setup request, config_index:" << req.config_index()
            << "raw:" << payload.toHex(' ')
            << "debug:" << QString::fromStdString(req.ShortDebugString());

    oaa::proto::messages::AVChannelSetupResponse resp;
    resp.set_media_status(oaa::proto::enums::AVChannelSetupStatus::OK);
    resp.set_max_unacked(10);
    resp.add_configs(0);

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId_, oaa::AVMessageId::SETUP_RESPONSE, data);
}

void AudioChannelHandler::handleStartIndication(const QByteArray& payload)
{
    oaa::proto::messages::AVChannelStartIndication start;
    if (!start.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[AudioChannel" << channelId_ << "] failed to parse StartIndication";
        return;
    }

    session_ = start.session();
    streaming_ = true;

    unackedCount_ = 0;
    qDebug() << "[AudioChannel" << channelId_
             << "] stream started, session:" << session_;
    emit streamStarted(session_);
}

void AudioChannelHandler::handleStopIndication()
{
    streaming_ = false;
    qDebug() << "[AudioChannel" << channelId_ << "] stream stopped";
    emit streamStopped();
}

void AudioChannelHandler::onMediaData(const QByteArray& data, uint64_t timestamp)
{
    if (!channelOpen_ || !streaming_)
        return;

    emit audioDataReceived(data, timestamp);

    // n-ACK flow control per HUIG: the phone sends up to max_unacked frames
    // before pausing for an ACK. We use this as backpressure by only ACKing
    // when our buffer has room for more data.
    ++unackedCount_;

    // Always ACK at max_unacked to avoid stalling the phone entirely.
    // But also ACK at half max_unacked if buffer pressure allows.
    // The phone will self-pace based on how fast ACKs arrive.
    if (unackedCount_ >= 10) {
        sendAck(unackedCount_);
        unackedCount_ = 0;
    }
}

void AudioChannelHandler::sendAck(uint32_t frameCount)
{
    oaa::proto::messages::AVMediaAckIndication ack;
    ack.set_session(session_);
    // Value = number of frames being acknowledged (permit replenishment),
    // not cumulative total. Phone uses this to restore its send permits.
    ack.set_value(frameCount);

    QByteArray data(ack.ByteSizeLong(), '\0');
    ack.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId_, oaa::AVMessageId::ACK_INDICATION, data);
}

} // namespace hu
} // namespace oaa
