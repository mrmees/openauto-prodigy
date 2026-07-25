#include "oaa/HU/Handlers/AVInputChannelHandler.hpp"

#include <QDebug>
#include <QThread>

#include <QtEndian>

#include <utility>

#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/AVChannelSetupResponseMessage.pb.h"
#include "oaa/av/AVChannelSetupStatusEnum.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"
#include "oaa/av/AVInputOpenRequestMessage.pb.h"
#include "oaa/av/AVInputOpenResponseMessage.pb.h"

namespace oaa {
namespace hu {

AVInputChannelHandler::AVInputChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
}

void AVInputChannelHandler::setCaptureController(CaptureController controller)
{
    Q_ASSERT(QThread::currentThread() == thread());
    captureController_ = std::move(controller);
}

void AVInputChannelHandler::onChannelOpened()
{
    channelOpen_ = true;
    // A duplicate open is a lifecycle reset, not permission to orphan an
    // already-running capture while silently disabling transmission.
    stopCapture();
    session_ = 0;
    maxUnacked_ = 1;
    unacked_ = 0;
    qDebug() << "[AVInputChannel] opened";
}

void AVInputChannelHandler::onChannelClosed()
{
    Q_ASSERT(QThread::currentThread() == thread());
    channelOpen_ = false;
    stopCapture();
    maxUnacked_ = 1;
    qDebug() << "[AVInputChannel] closed";
}

void AVInputChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset)
{
    // Zero-copy view of data past the message ID header
    const QByteArray data = QByteArray::fromRawData(
        payload.constData() + dataOffset, payload.size() - dataOffset);

    switch (messageId) {
    case oaa::AVMessageId::SETUP_REQUEST:
        handleSetupRequest(data);
        break;
    case oaa::AVMessageId::INPUT_OPEN_REQUEST:
        handleInputOpenRequest(data);
        break;
    case oaa::AVMessageId::ACK_INDICATION:
        handleAckIndication(data);
        break;
    default:
        qWarning() << "[AVInputChannel] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, data);
        break;
    }
}

void AVInputChannelHandler::handleSetupRequest(const QByteArray& payload)
{
    Q_ASSERT(QThread::currentThread() == thread());

    oaa::proto::messages::AVChannelSetupRequest request;
    if (!request.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[AVInputChannel] failed to parse SetupRequest";
        return;
    }

    const bool supported = request.media_codec_type()
        == oaa::proto::enums::MediaCodecType::MEDIA_CODEC_AUDIO_PCM;
    qInfo() << "[AVInputChannel] setup request, codec:"
            << request.media_codec_type() << "supported:" << supported;

    oaa::proto::messages::AVChannelSetupResponse response;
    response.set_media_status(supported
        ? oaa::proto::enums::AVChannelSetupStatus::OK
        : oaa::proto::enums::AVChannelSetupStatus::FAIL);
    response.set_max_unacked(1);
    if (supported)
        response.add_configs(0);

    QByteArray data(response.ByteSizeLong(), '\0');
    response.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::AVMessageId::SETUP_RESPONSE, data);
}

void AVInputChannelHandler::handleInputOpenRequest(const QByteArray& payload)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if (!channelOpen_) {
        qWarning() << "[AVInputChannel] ignoring input request before channel open";
        return;
    }

    oaa::proto::messages::AVInputOpenRequest req;
    if (!req.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[AVInputChannel] failed to parse InputOpenRequest";
        return;
    }

    const bool open = req.open();
    const int32_t requestedWindow = req.has_max_unacked() ? req.max_unacked() : 1;

    qDebug() << "[AVInputChannel] input open request:" << (open ? "OPEN" : "CLOSE")
             << "anc:" << req.anc() << "ec:" << req.ec()
             << "max_unacked:" << requestedWindow;

    if (!open) {
        stopCapture();
        maxUnacked_ = 1;
        sendInputOpenResponse(true);
        return;
    }

    // A repeated OPEN starts a fresh generation. Stop the prior capture before
    // asking the application to create its replacement, and reset all permits.
    stopCapture();
    maxUnacked_ = requestedWindow > 0 ? requestedWindow : 1;
    unacked_ = 0;

    bool success = false;
    if (captureController_) {
        success = captureController_(true);
    } else {
        qWarning() << "[AVInputChannel] no capture controller; failing open request";
    }

    capturing_ = success;
    if (!success) {
        maxUnacked_ = 1;
        unacked_ = 0;
    }
    sendInputOpenResponse(success);
    if (capturing_)
        emit micCaptureRequested(true);
}

void AVInputChannelHandler::handleAckIndication(const QByteArray& payload)
{
    Q_ASSERT(QThread::currentThread() == thread());

    oaa::proto::messages::AVMediaAckIndication ack;
    if (!ack.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[AVInputChannel] failed to parse AVMediaAckIndication";
        return;
    }
    if (!capturing_ || ack.session_id() != session_)
        return;

    uint32_t count = 0;
    if (ack.receive_timestamps_size() > 0) {
        count = static_cast<uint32_t>(ack.receive_timestamps_size());
    } else if (ack.has_ack_count()) {
        count = ack.ack_count();
    }
    if (count == 0 || unacked_ == 0 || count > unacked_)
        return;

    const bool wasFull = unacked_ >= static_cast<uint32_t>(maxUnacked_);
    unacked_ -= count;
    if (wasFull && unacked_ < static_cast<uint32_t>(maxUnacked_))
        emit sendWindowAvailable();
}

bool AVInputChannelHandler::sendMicData(const QByteArray& data, uint64_t timestamp)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!channelOpen_ || !capturing_)
        return false;
    if (unacked_ >= static_cast<uint32_t>(maxUnacked_))
        return false;

    // AV_MEDIA_WITH_TIMESTAMP wire format: [8-byte BE timestamp][raw audio]
    // Messenger only prepends the messageId, so we must pack the timestamp here.
    QByteArray payload;
    payload.reserve(8 + data.size());
    uint64_t tsBE = qToBigEndian(timestamp);
    payload.append(reinterpret_cast<const char*>(&tsBE), 8);
    payload.append(data);

    ++unacked_;
    emit sendRequested(channelId(), oaa::AVMessageId::AV_MEDIA_WITH_TIMESTAMP, payload);
    return true;
}

void AVInputChannelHandler::abortCapture()
{
    Q_ASSERT(QThread::currentThread() == thread());
    unacked_ = 0;
    maxUnacked_ = 1;
    if (!capturing_)
        return;

    capturing_ = false;
    emit micCaptureRequested(false);
}

void AVInputChannelHandler::stopCapture()
{
    unacked_ = 0;
    if (!capturing_)
        return;

    // Clear first so a re-entrant observer cannot enqueue another mic frame.
    capturing_ = false;
    emit micCaptureRequested(false);
    if (captureController_)
        captureController_(false);
}

void AVInputChannelHandler::sendInputOpenResponse(bool success)
{
    oaa::proto::messages::AVInputOpenResponse resp;
    resp.set_session(session_);
    resp.set_value(success ? 0 : 1);

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::AVMessageId::INPUT_OPEN_RESPONSE, data);
}

} // namespace hu
} // namespace oaa
