#include "oaa/HU/Handlers/PhoneStatusChannelHandler.hpp"

#include <QDebug>

#include "PhoneStatusMessage.pb.h"

namespace oaa {
namespace hu {

PhoneStatusChannelHandler::PhoneStatusChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
}

void PhoneStatusChannelHandler::onChannelOpened()
{
    qInfo() << "[PhoneStatusChannel] opened";
}

void PhoneStatusChannelHandler::onChannelClosed()
{
    qInfo() << "[PhoneStatusChannel] closed";
}

void PhoneStatusChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset)
{
    // Zero-copy view of data past the message ID header
    const QByteArray data = QByteArray::fromRawData(
        payload.constData() + dataOffset, payload.size() - dataOffset);

    switch (messageId) {
    case oaa::PhoneStatusMessageId::PHONE_STATUS:
        handlePhoneStatus(data);
        break;
    default:
        qInfo() << "[PhoneStatusChannel] unknown msgId:" << QString("0x%1").arg(messageId, 4, 16, QChar('0'))
                << "len:" << data.size()
                << "hex:" << data.left(64).toHex(' ');
        break;
    }
}

void PhoneStatusChannelHandler::handlePhoneStatus(const QByteArray& payload)
{
    oaa::proto::messages::PhoneStatusUpdate msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[PhoneStatusChannel] failed to parse PhoneStatusUpdate";
        return;
    }

    if (msg.calls_size() == 0) {
        qDebug() << "[PhoneStatusChannel] idle (no active calls)";
        emit callsIdle();
        return;
    }

    for (int i = 0; i < msg.calls_size(); ++i) {
        const auto& call = msg.calls(i);
        int state = call.call_state();
        QString number = QString::fromStdString(call.phone_number());
        QString name = QString::fromStdString(call.display_name());
        QByteArray photo;
        if (call.has_contact_photo())
            photo = QByteArray(call.contact_photo().data(), call.contact_photo().size());

        const char* stateStr = (state == Ringing) ? "RINGING"
                             : (state == Active)  ? "ACTIVE"
                             : "UNKNOWN";

        qInfo() << "[PhoneStatusChannel]" << stateStr
                << number << name
                << (photo.isEmpty() ? "" : QString("photo: %1 bytes").arg(photo.size()));

        emit callStateChanged(state, number, name, photo);
    }
}

} // namespace hu
} // namespace oaa
