#include "AudioChannelHandler.hpp"

#include <QDebug>

#include "AVChannelSetupRequestMessage.pb.h"
#include "AVChannelSetupResponseMessage.pb.h"
#include "AVChannelSetupStatusEnum.pb.h"
#include "AVChannelStartIndicationMessage.pb.h"
#include "AVChannelStopIndicationMessage.pb.h"
#include "AVMediaAckIndicationMessage.pb.h"

namespace oap {
namespace aa {

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
    ackCounter_ = 0;
    qDebug() << "[AudioChannel" << channelId_ << "] opened";
}

void AudioChannelHandler::onChannelClosed()
{
    channelOpen_ = false;
    streaming_ = false;
    qDebug() << "[AudioChannel" << channelId_ << "] closed";
}

void AudioChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload)
{
    switch (messageId) {
    case oaa::AVMessageId::SETUP_REQUEST:
        handleSetupRequest(payload);
        break;
    case oaa::AVMessageId::START_INDICATION:
        handleStartIndication(payload);
        break;
    case oaa::AVMessageId::STOP_INDICATION:
        handleStopIndication();
        break;
    default:
        qWarning() << "[AudioChannel" << channelId_
                   << "] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, payload);
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

    qDebug() << "[AudioChannel" << channelId_
             << "] setup request, config_index:" << req.config_index();

    oaa::proto::messages::AVChannelSetupResponse resp;
    resp.set_media_status(oaa::proto::enums::AVChannelSetupStatus::OK);
    resp.set_max_unacked(1);
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
    ackCounter_ = 0;
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
    sendAck();
}

void AudioChannelHandler::sendAck()
{
    ++ackCounter_;
    oaa::proto::messages::AVMediaAckIndication ack;
    ack.set_session(session_);
    ack.set_value(ackCounter_);

    QByteArray data(ack.ByteSizeLong(), '\0');
    ack.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId_, oaa::AVMessageId::ACK_INDICATION, data);
}

} // namespace aa
} // namespace oap
