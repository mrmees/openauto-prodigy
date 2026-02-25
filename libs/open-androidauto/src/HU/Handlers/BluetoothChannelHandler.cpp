#include "oaa/HU/Handlers/BluetoothChannelHandler.hpp"

#include <QDebug>

#include "BluetoothPairingRequestMessage.pb.h"
#include "BluetoothPairingResponseMessage.pb.h"
#include "BluetoothPairingStatusEnum.pb.h"

namespace oaa {
namespace hu {

BluetoothChannelHandler::BluetoothChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
}

void BluetoothChannelHandler::onChannelOpened()
{
    channelOpen_ = true;
    qDebug() << "[BluetoothChannel] opened";
}

void BluetoothChannelHandler::onChannelClosed()
{
    channelOpen_ = false;
    qDebug() << "[BluetoothChannel] closed";
}

void BluetoothChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload)
{
    switch (messageId) {
    case oaa::BluetoothMessageId::PAIRING_REQUEST:
        handlePairingRequest(payload);
        break;
    default:
        qWarning() << "[BluetoothChannel] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, payload);
        break;
    }
}

void BluetoothChannelHandler::handlePairingRequest(const QByteArray& payload)
{
    if (!channelOpen_)
        return;

    oaa::proto::messages::BluetoothPairingRequest req;
    if (!req.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[BluetoothChannel] failed to parse PairingRequest";
        return;
    }

    QString phoneAddr = QString::fromStdString(req.phone_address());
    qDebug() << "[BluetoothChannel] pairing request from" << phoneAddr
             << "method:" << req.pairing_method();
    if (req.has_phone_name())
        qInfo() << "[BluetoothChannel]   phone_name:" << QString::fromStdString(req.phone_name());

    emit pairingRequested(phoneAddr);

    // Respond: already paired (we handle BT pairing externally via bluetoothctl)
    oaa::proto::messages::BluetoothPairingResponse resp;
    resp.set_already_paired(true);
    resp.set_status(oaa::proto::enums::BluetoothPairingStatus::OK);

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::BluetoothMessageId::PAIRING_RESPONSE, data);
}

} // namespace hu
} // namespace oaa
