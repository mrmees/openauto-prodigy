#include "oaa/HU/Handlers/VideoChannelHandler.hpp"

#include <chrono>
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

namespace oaa {
namespace hu {

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

void VideoChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset)
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
    case oaa::AVMessageId::VIDEO_FOCUS_REQUEST:
        handleVideoFocusRequest(data);
        break;
    case oaa::AVMessageId::VIDEO_FOCUS_INDICATION:
        handleVideoFocusIndication(data);
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
        qDebug() << "[VideoChannel] newer AV message:" << Qt::hex << messageId
                 << "size:" << data.size();
        break;
    default:
        qWarning() << "[VideoChannel] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, data);
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

    qInfo() << "[VideoChannel] setup request, config_index:" << req.config_index()
            << "raw:" << payload.toHex(' ')
            << "debug:" << QString::fromStdString(req.ShortDebugString());

    oaa::proto::messages::AVChannelSetupResponse resp;
    resp.set_media_status(oaa::proto::enums::AVChannelSetupStatus::OK);
    resp.set_max_unacked(10);
    // Accept all our advertised configs (phone picks best match)
    for (uint32_t i = 0; i < numVideoConfigs_; ++i)
        resp.add_configs(i);

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    qDebug() << "[VideoChannel] sending SETUP_RESPONSE, ch:" << channelId()
             << "msgId:" << Qt::hex << oaa::AVMessageId::SETUP_RESPONSE
             << "size:" << data.size();
    emit sendRequested(channelId(), oaa::AVMessageId::SETUP_RESPONSE, data);

    // Send unsolicited VIDEO_FOCUS_INDICATION — some phones (e.g. Moto G Play)
    // won't send VIDEO_FOCUS_REQUEST and expect the HU to indicate focus first.
    oaa::proto::messages::VideoFocusIndication focus;
    focus.set_focus_mode(oaa::proto::enums::VideoFocusMode::PROJECTED);
    focus.set_unrequested(false);
    QByteArray focusData(focus.ByteSizeLong(), '\0');
    focus.SerializeToArray(focusData.data(), focusData.size());
    qDebug() << "[VideoChannel] sending VIDEO_FOCUS_INDICATION, ch:" << channelId()
             << "msgId:" << Qt::hex << oaa::AVMessageId::VIDEO_FOCUS_INDICATION
             << "size:" << focusData.size();
    emit sendRequested(channelId(), oaa::AVMessageId::VIDEO_FOCUS_INDICATION, focusData);
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
    qInfo() << "[VideoChannel] stream started, session:" << session_
            << "config:" << start.config()
            << "(of" << numVideoConfigs_ << "offered)";
    emit streamStarted(session_, start.config());
}

void VideoChannelHandler::handleStopIndication()
{
    streaming_ = false;
    qDebug() << "[VideoChannel] stream stopped";
    emit streamStopped();
}

void VideoChannelHandler::handleVideoFocusRequest(const QByteArray& payload)
{
    oaa::proto::messages::VideoFocusRequest req;
    if (!req.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[VideoChannel] failed to parse VideoFocusRequest";
        return;
    }

    auto mode = req.focus_mode();
    qDebug() << "[VideoChannel] focus request, mode:" << (int)mode;

    // Echo back the phone's requested focus mode
    oaa::proto::messages::VideoFocusIndication resp;
    resp.set_focus_mode(mode);
    resp.set_unrequested(false);
    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::AVMessageId::VIDEO_FOCUS_INDICATION, data);

    // Notify orchestrator of focus change
    emit videoFocusChanged(static_cast<int>(mode), false);
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

    auto now = std::chrono::steady_clock::now();
    qint64 enqueueNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    emit videoFrameData(data, enqueueNs);
    sendAck();
}

void VideoChannelHandler::requestVideoFocus(bool focused)
{
    if (!channelOpen_)
        return;

    // HU tells the phone about focus via VIDEO_FOCUS_INDICATION (unsolicited),
    // NOT VIDEO_FOCUS_REQUEST (that's phone→HU only).
    oaa::proto::messages::VideoFocusIndication indication;
    indication.set_focus_mode(focused ? oaa::proto::enums::VideoFocusMode::PROJECTED
                                      : oaa::proto::enums::VideoFocusMode::NONE);
    indication.set_unrequested(true);  // HU-initiated

    QByteArray data(indication.ByteSizeLong(), '\0');
    indication.SerializeToArray(data.data(), data.size());
    qDebug() << "[VideoChannel] sending unsolicited focus indication, focused:" << focused;
    emit sendRequested(channelId(), oaa::AVMessageId::VIDEO_FOCUS_INDICATION, data);
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

} // namespace hu
} // namespace oaa
