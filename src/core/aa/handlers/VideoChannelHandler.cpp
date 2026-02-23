#include "VideoChannelHandler.hpp"

#include <QDebug>

#include "AVChannelSetupRequestMessage.pb.h"
#include "AVChannelSetupResponseMessage.pb.h"
#include "AVChannelSetupStatusEnum.pb.h"
#include "AVChannelStartIndicationMessage.pb.h"
#include "AVChannelStopIndicationMessage.pb.h"
#include "AVMediaAckIndicationMessage.pb.h"
#include "VideoFocusRequestMessage.pb.h"
#include "VideoFocusIndicationMessage.pb.h"
#include "VideoFocusModeEnum.pb.h"
#include "VideoFocusReasonEnum.pb.h"

namespace oap {
namespace aa {

VideoChannelHandler::VideoChannelHandler(QObject* parent)
    : oaa::IAVChannelHandler(parent)
{
}

void VideoChannelHandler::onChannelOpened()
{
    channelOpen_ = true;
    streaming_ = false;
    session_ = -1;
    ackCounter_ = 0;
    qDebug() << "[VideoChannel] opened";
}

void VideoChannelHandler::onChannelClosed()
{
    channelOpen_ = false;
    streaming_ = false;
    qDebug() << "[VideoChannel] closed";
}

void VideoChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload)
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
    case oaa::AVMessageId::VIDEO_FOCUS_INDICATION:
        handleVideoFocusIndication(payload);
        break;
    default:
        qWarning() << "[VideoChannel] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, payload);
        break;
    }
}

void VideoChannelHandler::handleSetupRequest(const QByteArray& payload)
{
    oaa::proto::messages::AVChannelSetupRequest req;
    if (!req.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[VideoChannel] failed to parse SetupRequest";
        return;
    }

    qDebug() << "[VideoChannel] setup request, config_index:" << req.config_index();

    oaa::proto::messages::AVChannelSetupResponse resp;
    resp.set_media_status(oaa::proto::enums::AVChannelSetupStatus::OK);
    resp.set_max_unacked(1);
    resp.add_configs(0); // Always use primary config

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::AVMessageId::SETUP_RESPONSE, data);
}

void VideoChannelHandler::handleStartIndication(const QByteArray& payload)
{
    oaa::proto::messages::AVChannelStartIndication start;
    if (!start.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[VideoChannel] failed to parse StartIndication";
        return;
    }

    session_ = start.session();
    streaming_ = true;
    ackCounter_ = 0;
    qDebug() << "[VideoChannel] stream started, session:" << session_
             << "config:" << start.config();
    emit streamStarted(session_, start.config());
}

void VideoChannelHandler::handleStopIndication()
{
    streaming_ = false;
    qDebug() << "[VideoChannel] stream stopped";
    emit streamStopped();
}

void VideoChannelHandler::handleVideoFocusIndication(const QByteArray& payload)
{
    oaa::proto::messages::VideoFocusIndication indication;
    if (!indication.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[VideoChannel] failed to parse VideoFocusIndication";
        return;
    }

    int mode = static_cast<int>(indication.focus_mode());
    bool unrequested = indication.unrequested();
    qDebug() << "[VideoChannel] focus indication, mode:" << mode
             << "unrequested:" << unrequested;
    emit videoFocusChanged(mode, unrequested);
}

void VideoChannelHandler::onMediaData(const QByteArray& data, uint64_t timestamp)
{
    if (!channelOpen_ || !streaming_)
        return;

    emit videoFrameData(data, timestamp);
    sendAck();
}

void VideoChannelHandler::requestVideoFocus(bool focused)
{
    if (!channelOpen_)
        return;

    oaa::proto::messages::VideoFocusRequest req;
    req.set_focus_mode(focused ? oaa::proto::enums::VideoFocusMode::FOCUSED
                               : oaa::proto::enums::VideoFocusMode::UNFOCUSED);
    req.set_focus_reason(oaa::proto::enums::VideoFocusReason::NONE);

    QByteArray data(req.ByteSizeLong(), '\0');
    req.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::AVMessageId::VIDEO_FOCUS_REQUEST, data);
}

void VideoChannelHandler::sendAck()
{
    ++ackCounter_;
    oaa::proto::messages::AVMediaAckIndication ack;
    ack.set_session(session_);
    ack.set_value(ackCounter_);

    QByteArray data(ack.ByteSizeLong(), '\0');
    ack.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::AVMessageId::ACK_INDICATION, data);
}

} // namespace aa
} // namespace oap
