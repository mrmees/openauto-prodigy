#include "AVInputChannelHandler.hpp"

#include <QDebug>

#include <QtEndian>

#include "AVInputOpenRequestMessage.pb.h"
#include "AVInputOpenResponseMessage.pb.h"

namespace oap {
namespace aa {

AVInputChannelHandler::AVInputChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
}

void AVInputChannelHandler::onChannelOpened()
{
    channelOpen_ = true;
    capturing_ = false;
    session_ = 0;
    qDebug() << "[AVInputChannel] opened";
}

void AVInputChannelHandler::onChannelClosed()
{
    channelOpen_ = false;
    if (capturing_) {
        capturing_ = false;
        emit micCaptureRequested(false);
    }
    qDebug() << "[AVInputChannel] closed";
}

void AVInputChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload)
{
    switch (messageId) {
    case oaa::AVMessageId::INPUT_OPEN_REQUEST:
        handleInputOpenRequest(payload);
        break;
    case oaa::AVMessageId::ACK_INDICATION:
        handleAckIndication(payload);
        break;
    default:
        qWarning() << "[AVInputChannel] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, payload);
        break;
    }
}

void AVInputChannelHandler::handleInputOpenRequest(const QByteArray& payload)
{
    oaa::proto::messages::AVInputOpenRequest req;
    if (!req.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[AVInputChannel] failed to parse InputOpenRequest";
        return;
    }

    bool open = req.open();
    if (req.has_max_unacked())
        maxUnacked_ = req.max_unacked();

    qDebug() << "[AVInputChannel] input open request:" << (open ? "OPEN" : "CLOSE")
             << "anc:" << req.anc() << "ec:" << req.ec()
             << "max_unacked:" << maxUnacked_;

    // Send response
    oaa::proto::messages::AVInputOpenResponse resp;
    resp.set_session(session_);
    resp.set_value(0);

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::AVMessageId::INPUT_OPEN_RESPONSE, data);

    capturing_ = open;
    emit micCaptureRequested(open);
}

void AVInputChannelHandler::handleAckIndication(const QByteArray& payload)
{
    Q_UNUSED(payload);
    // Phone acknowledged our mic data — nothing to do currently
}

void AVInputChannelHandler::sendMicData(const QByteArray& data, uint64_t timestamp)
{
    if (!channelOpen_ || !capturing_)
        return;

    // AV_MEDIA_WITH_TIMESTAMP wire format: [8-byte BE timestamp][raw audio]
    // Messenger only prepends the messageId, so we must pack the timestamp here.
    QByteArray payload;
    payload.reserve(8 + data.size());
    uint64_t tsBE = qToBigEndian(timestamp);
    payload.append(reinterpret_cast<const char*>(&tsBE), 8);
    payload.append(data);

    emit sendRequested(channelId(), oaa::AVMessageId::AV_MEDIA_WITH_TIMESTAMP, payload);
}

} // namespace aa
} // namespace oap
