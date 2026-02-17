#include "BluetoothDiscoveryService.hpp"

#include <QBluetoothAddress>
#include <QBluetoothUuid>
#include <QDataStream>
#include <QNetworkInterface>
#include <boost/log/trivial.hpp>

#include <aasdk_proto/WifiInfoRequestMessage.pb.h>
#include <aasdk_proto/WifiInfoResponseMessage.pb.h>
#include <aasdk_proto/WifiSecurityRequestMessage.pb.h>
#include <aasdk_proto/WifiSecurityResponseMessage.pb.h>

// Android Auto Wireless projection UUID
static const QBluetoothUuid kAAWirelessUuid(
    QStringLiteral("4de17a00-52cb-11e6-bdf4-0800200c9a66"));

// BT handshake message IDs
static constexpr uint16_t kMsgWifiInfoRequest = 1;
static constexpr uint16_t kMsgWifiSecurityRequest = 2;
static constexpr uint16_t kMsgWifiSecurityResponse = 3;
static constexpr uint16_t kMsgWifiInfoResponse = 7;

BluetoothDiscoveryService::BluetoothDiscoveryService(
    std::shared_ptr<oap::Configuration> config,
    QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , rfcommServer_(std::make_unique<QBluetoothServer>(
          QBluetoothServiceInfo::RfcommProtocol, this))
{
    connect(rfcommServer_.get(), &QBluetoothServer::newConnection,
            this, &BluetoothDiscoveryService::onClientConnected);
}

BluetoothDiscoveryService::~BluetoothDiscoveryService()
{
    stop();
}

void BluetoothDiscoveryService::start()
{
    if (!rfcommServer_->listen(QBluetoothAddress())) {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] Failed to start RFCOMM server";
        emit error("Failed to start Bluetooth RFCOMM server");
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] RFCOMM listening on port "
                            << rfcommServer_->serverPort();

    // Register SDP service so the phone can discover us
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceName,
                              QStringLiteral("OpenAutoProdigy"));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceDescription,
                              QStringLiteral("Android Auto Wireless"));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceProvider,
                              QStringLiteral("OpenAutoProdigy"));

    // Set the AA Wireless UUID
    QBluetoothServiceInfo::Sequence classId;
    classId << QVariant::fromValue(kAAWirelessUuid);
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);

    // Protocol descriptor (L2CAP + RFCOMM)
    QBluetoothServiceInfo::Sequence protocolDescriptorList;
    QBluetoothServiceInfo::Sequence protocol;
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::ProtocolUuid::L2cap));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    protocol.clear();
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::ProtocolUuid::Rfcomm))
             << QVariant::fromValue(quint8(rfcommServer_->serverPort()));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,
                              protocolDescriptorList);

    // Service UUID
    serviceInfo_.setServiceUuid(kAAWirelessUuid);

    if (!serviceInfo_.registerService()) {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] Failed to register SDP service";
        emit error("Failed to register Bluetooth SDP service");
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] SDP service registered (AA Wireless)";
}

void BluetoothDiscoveryService::stop()
{
    if (serviceInfo_.isRegistered()) {
        serviceInfo_.unregisterService();
    }

    if (socket_) {
        socket_->disconnectFromService();
        socket_->deleteLater();
        socket_ = nullptr;
    }

    rfcommServer_->close();
    buffer_.clear();

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Stopped";
}

void BluetoothDiscoveryService::onClientConnected()
{
    if (socket_) {
        socket_->deleteLater();
        socket_ = nullptr;
    }

    socket_ = rfcommServer_->nextPendingConnection();
    if (!socket_) {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] Null socket from pending connection";
        return;
    }

    buffer_.clear();

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Phone connected via BT: "
                            << socket_->peerName().toStdString();

    connect(socket_, &QBluetoothSocket::readyRead,
            this, &BluetoothDiscoveryService::readSocket);

    // Step 1: Send WifiInfoRequest with our IP and TCP port
    std::string localIp = getLocalIP(QStringLiteral("wlan0"));
    if (localIp.empty()) {
        // Fallback: try common AP interface names
        localIp = getLocalIP(QStringLiteral("ap0"));
    }
    if (localIp.empty()) {
        // Last resort: try any non-loopback IPv4
        for (const auto& iface : QNetworkInterface::allInterfaces()) {
            if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
            if (!iface.flags().testFlag(QNetworkInterface::IsUp)) continue;
            for (const auto& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    localIp = entry.ip().toString().toStdString();
                    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Using IP from "
                                            << iface.name().toStdString()
                                            << ": " << localIp;
                    break;
                }
            }
            if (!localIp.empty()) break;
        }
    }

    if (localIp.empty()) {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] No usable IPv4 address found!";
        emit error("No usable IPv4 address for WiFi handshake");
        return;
    }

    aasdk::proto::messages::WifiInfoRequest request;
    request.set_ip_address(localIp);
    request.set_port(config_->tcpPort());

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Sending WifiInfoRequest: ip=" << localIp
                            << " port=" << config_->tcpPort();
    sendMessage(request, kMsgWifiInfoRequest);
}

void BluetoothDiscoveryService::readSocket()
{
    buffer_ += socket_->readAll();

    // Message framing: [2B length][2B msgId][protobuf payload]
    while (buffer_.size() >= 4) {
        QDataStream stream(buffer_);
        stream.setByteOrder(QDataStream::BigEndian);
        uint16_t length = 0;
        stream >> length;

        if (buffer_.size() < length + 4) {
            // Not enough data yet
            return;
        }

        uint16_t messageId = 0;
        stream >> messageId;

        BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Received msgId=" << messageId
                                << " length=" << length;

        switch (messageId) {
        case kMsgWifiSecurityRequest:
            handleWifiSecurityRequest(buffer_, length);
            break;
        case kMsgWifiInfoResponse:
            handleWifiInfoResponse(buffer_, length);
            break;
        default:
            BOOST_LOG_TRIVIAL(warning) << "[BTDiscovery] Unknown message ID: " << messageId;
            break;
        }

        // Consume this message
        buffer_ = buffer_.mid(length + 4);
    }
}

void BluetoothDiscoveryService::handleWifiSecurityRequest(
    const QByteArray& data, uint16_t length)
{
    // Phone is asking for WiFi credentials
    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Phone requested WiFi credentials";

    aasdk::proto::messages::WifiSecurityResponse response;
    response.set_ssid(config_->wifiSsid().toStdString());
    response.set_key(config_->wifiPassword().toStdString());
    response.set_security_mode(
        aasdk::proto::messages::WifiSecurityResponse_SecurityMode_WPA2_PERSONAL);
    response.set_access_point_type(
        aasdk::proto::messages::WifiSecurityResponse_AccessPointType_STATIC);

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Sending WifiSecurityResponse: ssid="
                            << config_->wifiSsid().toStdString();
    sendMessage(response, kMsgWifiSecurityResponse);
}

void BluetoothDiscoveryService::handleWifiInfoResponse(
    const QByteArray& data, uint16_t length)
{
    aasdk::proto::messages::WifiInfoResponse msg;
    if (!msg.ParseFromArray(data.data() + 4, length)) {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] Failed to parse WifiInfoResponse";
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] WifiInfoResponse: "
                            << msg.ShortDebugString();

    if (msg.status() == aasdk::proto::messages::WifiInfoResponse_Status_STATUS_SUCCESS) {
        BOOST_LOG_TRIVIAL(info) << "[BTDiscovery] Phone will connect via WiFi!";
        emit phoneWillConnect();
    } else {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] Phone reported error status: "
                                 << msg.status();
        emit error(QString("Phone WiFi handshake failed (status %1)").arg(msg.status()));
    }
}

void BluetoothDiscoveryService::sendMessage(
    const google::protobuf::Message& message, uint16_t type)
{
    int byteSize = message.ByteSizeLong();
    QByteArray out(byteSize + 4, 0);
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << static_cast<uint16_t>(byteSize);
    ds << type;
    message.SerializeToArray(out.data() + 4, byteSize);

    BOOST_LOG_TRIVIAL(debug) << "[BTDiscovery] Sending " << message.GetTypeName()
                             << " (msgId=" << type << ", size=" << byteSize << ")";

    qint64 written = socket_->write(out);
    if (written < 0) {
        BOOST_LOG_TRIVIAL(error) << "[BTDiscovery] Failed to write to BT socket";
    }
}

std::string BluetoothDiscoveryService::getLocalIP(const QString& interfaceName) const
{
    QNetworkInterface iface = QNetworkInterface::interfaceFromName(interfaceName);
    if (!iface.isValid()) return "";

    for (const auto& entry : iface.addressEntries()) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
            return entry.ip().toString().toStdString();
        }
    }
    return "";
}
