#include "oaa/HU/Handlers/InputChannelHandler.hpp"

#include <QDebug>

#include "InputEventIndicationMessage.pb.h"
#include "TouchEventData.pb.h"
#include "TouchLocationData.pb.h"
#include "TouchActionEnum.pb.h"
#include "BindingRequestMessage.pb.h"
#include "BindingResponseMessage.pb.h"
#include "StatusEnum.pb.h"

namespace oaa {
namespace hu {

InputChannelHandler::InputChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
}

void InputChannelHandler::onChannelOpened()
{
    channelOpen_ = true;
    qDebug() << "[InputChannel] opened";
}

void InputChannelHandler::onChannelClosed()
{
    channelOpen_ = false;
    qDebug() << "[InputChannel] closed";
}

void InputChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload)
{
    switch (messageId) {
    case oaa::InputMessageId::BINDING_REQUEST:
        handleBindingRequest(payload);
        break;
    default:
        qWarning() << "[InputChannel] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, payload);
        break;
    }
}

void InputChannelHandler::handleBindingRequest(const QByteArray& payload)
{
    oaa::proto::messages::BindingRequest req;
    if (!req.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[InputChannel] failed to parse BindingRequest";
        return;
    }

    qDebug() << "[InputChannel] binding request with" << req.scan_codes_size() << "scan codes";

    oaa::proto::messages::BindingResponse resp;
    resp.set_status(oaa::proto::enums::Status::OK);

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::InputMessageId::BINDING_RESPONSE, data);
}

void InputChannelHandler::sendTouchIndication(int pointerCount, const Pointer* pointers,
                                                int actionIndex, int action, uint64_t timestamp)
{
    if (!channelOpen_)
        return;

    oaa::proto::messages::InputEventIndication indication;
    indication.set_timestamp(timestamp);
    indication.set_disp_channel(0);

    auto* touchEvent = indication.mutable_touch_event();
    touchEvent->set_touch_action(static_cast<oaa::proto::enums::TouchAction::Enum>(action));
    touchEvent->set_action_index(actionIndex);

    for (int i = 0; i < pointerCount; ++i) {
        auto* loc = touchEvent->add_touch_location();
        loc->set_x(pointers[i].x);
        loc->set_y(pointers[i].y);
        loc->set_pointer_id(pointers[i].id);
    }

    QByteArray data(indication.ByteSizeLong(), '\0');
    indication.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::InputMessageId::INPUT_EVENT_INDICATION, data);
}

} // namespace hu
} // namespace oaa
