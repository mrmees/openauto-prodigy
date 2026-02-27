#include "oaa/HU/Handlers/WiFiChannelHandler.hpp"

#include <QDebug>

#include "oaa/wifi/WifiSecurityRequestMessage.pb.h"
#include "oaa/wifi/WifiSecurityResponseMessage.pb.h"

namespace oaa {
namespace hu {

WiFiChannelHandler::WiFiChannelHandler(const QString& ssid,
                                         const QString& password,
                                         QObject* parent)
    : oaa::IChannelHandler(parent)
    , ssid_(ssid)
    , password_(password)
{
}

void WiFiChannelHandler::onChannelOpened()
{
    channelOpen_ = true;
    qDebug() << "[WiFiChannel] opened";
}

void WiFiChannelHandler::onChannelClosed()
{
    channelOpen_ = false;
    qDebug() << "[WiFiChannel] closed";
}

void WiFiChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset)
{
    // Zero-copy view of data past the message ID header
    const QByteArray data = QByteArray::fromRawData(
        payload.constData() + dataOffset, payload.size() - dataOffset);

    switch (messageId) {
    case oaa::WiFiMessageId::CREDENTIALS_REQUEST:
        handleSecurityRequest(data);
        break;
    default:
        qWarning() << "[WiFiChannel] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, data);
        break;
    }
}

void WiFiChannelHandler::handleSecurityRequest(const QByteArray& payload)
{
    if (!channelOpen_)
        return;

    Q_UNUSED(payload);
    qDebug() << "[WiFiChannel] security request — sending credentials for SSID:" << ssid_;

    oaa::proto::messages::WifiSecurityResponse resp;
    resp.set_ssid(ssid_.toStdString());
    resp.set_key(password_.toStdString());
    resp.set_security_mode(oaa::proto::messages::WifiSecurityResponse::WPA2_PERSONAL);
    resp.set_access_point_type(oaa::proto::messages::WifiSecurityResponse::STATIC);

    QByteArray data(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::WiFiMessageId::CREDENTIALS_RESPONSE, data);
}

} // namespace hu
} // namespace oaa
