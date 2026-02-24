#include "BluetoothDiscoveryService.hpp"

#include <QBluetoothAddress>
#include <QBluetoothLocalDevice>
#include <QBluetoothUuid>
#include <QDataStream>
#include <QNetworkInterface>
#include <QDebug>

#include "WifiInfoRequestMessage.pb.h"
#include "WifiInfoResponseMessage.pb.h"
#include "WifiSecurityRequestMessage.pb.h"
#include "WifiSecurityResponseMessage.pb.h"

namespace oap {
namespace aa {

// Android Auto Wireless projection UUID
static const QBluetoothUuid kAAWirelessUuid(
    QStringLiteral("4de17a00-52cb-11e6-bdf4-0800200c9a66"));

// RFCOMM handshake message IDs (AA wireless projection protocol)
static constexpr uint16_t kMsgWifiStartRequest = 1;      // HU -> Phone: IP + port
static constexpr uint16_t kMsgWifiInfoRequest = 2;        // Phone -> HU: "give me credentials"
static constexpr uint16_t kMsgWifiInfoResponse = 3;       // HU -> Phone: SSID + key + security
static constexpr uint16_t kMsgWifiStartResponse = 6;      // Phone -> HU: acknowledgement
static constexpr uint16_t kMsgWifiConnectionStatus = 7;   // Phone -> HU: WiFi connected

BluetoothDiscoveryService::BluetoothDiscoveryService(
    std::shared_ptr<oap::Configuration> config,
    const QString& wifiInterface,
    QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
    , wifiInterface_(wifiInterface)
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
        qCritical() << "[BTDiscovery] Failed to start RFCOMM server";
        emit error("Failed to start Bluetooth RFCOMM server");
        return;
    }

    qInfo() << "[BTDiscovery] RFCOMM listening on port"
            << rfcommServer_->serverPort();

    // Register SDP service so the phone can discover us
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceName,
                              QStringLiteral("OpenAutoProdigy"));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceDescription,
                              QStringLiteral("Android Auto Wireless"));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceProvider,
                              QStringLiteral("OpenAutoProdigy"));

    // Bluetooth profile: Serial Port
    QBluetoothServiceInfo::Sequence classId;
    classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::SerialPort));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList, classId);

    // Service class: AA Wireless UUID + SerialPort
    classId.prepend(QVariant::fromValue(kAAWirelessUuid));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);

    // Make discoverable via public browse group
    QBluetoothServiceInfo::Sequence publicBrowse;
    publicBrowse << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::PublicBrowseGroup));
    serviceInfo_.setAttribute(QBluetoothServiceInfo::BrowseGroupList, publicBrowse);

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
        qCritical() << "[BTDiscovery] Failed to register SDP service";
        emit error("Failed to register Bluetooth SDP service");
        return;
    }

    qInfo() << "[BTDiscovery] SDP service registered (AA Wireless)";
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

    qInfo() << "[BTDiscovery] Stopped";
}

QString BluetoothDiscoveryService::localAddress() const
{
    QBluetoothLocalDevice localDevice;
    return localDevice.address().toString();
}

void BluetoothDiscoveryService::onClientConnected()
{
    if (socket_) {
        socket_->deleteLater();
        socket_ = nullptr;
    }

    socket_ = rfcommServer_->nextPendingConnection();
    if (!socket_) {
        qCritical() << "[BTDiscovery] Null socket from pending connection";
        return;
    }

    buffer_.clear();

    qInfo() << "[BTDiscovery] Phone connected via BT:"
            << socket_->peerName();

    connect(socket_, &QBluetoothSocket::readyRead,
            this, &BluetoothDiscoveryService::readSocket);

    // Step 1: Send WifiStartRequest with our IP and TCP port
    std::string localIp = getLocalIP(wifiInterface_);
    if (localIp.empty() && wifiInterface_ == "wlan0") {
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
                    qInfo() << "[BTDiscovery] Using IP from"
                            << iface.name() << ":" << localIp.c_str();
                    break;
                }
            }
            if (!localIp.empty()) break;
        }
    }

    if (localIp.empty()) {
        qCritical() << "[BTDiscovery] No usable IPv4 address found!";
        emit error("No usable IPv4 address for WiFi handshake");
        return;
    }

    // Send WifiStartRequest (msgId=1) with our IP and TCP port
    oaa::proto::messages::WifiInfoRequest request;
    request.set_ip_address(localIp);
    request.set_port(config_->tcpPort());

    qInfo() << "[BTDiscovery] Sending WifiStartRequest: ip=" << localIp.c_str()
            << "port=" << config_->tcpPort();
    sendMessage(request, kMsgWifiStartRequest);
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

        qInfo() << "[BTDiscovery] Received msgId=" << messageId
                << "length=" << length;

        switch (messageId) {
        case kMsgWifiInfoRequest:        // Phone asks for WiFi credentials
            handleWifiCredentialRequest();
            break;
        case kMsgWifiStartResponse:      // Phone acknowledges, connecting to WiFi
            qInfo() << "[BTDiscovery] Phone acknowledged WifiStartRequest";
            break;
        case kMsgWifiConnectionStatus:   // Phone reports WiFi connection result
            handleWifiConnectionStatus(buffer_, length);
            break;
        default:
            qWarning() << "[BTDiscovery] Unknown message ID:" << messageId;
            break;
        }

        // Consume this message
        buffer_ = buffer_.mid(length + 4);
    }
}

void BluetoothDiscoveryService::handleWifiCredentialRequest()
{
    // Phone is asking for WiFi credentials (msgId=2, empty payload)
    qInfo() << "[BTDiscovery] Phone requested WiFi credentials";

    // Send WifiInfoResponse (msgId=3) with AP credentials
    oaa::proto::messages::WifiSecurityResponse response;
    response.set_ssid(config_->wifiSsid().toStdString());
    response.set_key(config_->wifiPassword().toStdString());
    response.set_security_mode(
        oaa::proto::messages::WifiSecurityResponse_SecurityMode_WPA2_PERSONAL);
    response.set_access_point_type(
        oaa::proto::messages::WifiSecurityResponse_AccessPointType_DYNAMIC);

    // BSSID is required — phone uses it to identify which AP to auto-connect to.
    // Read MAC address of the WiFi interface (wlan0).
    QString bssid;
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (iface.name() == wifiInterface_) {
            bssid = iface.hardwareAddress();
            break;
        }
    }
    if (bssid.isEmpty()) {
        qWarning() << "[BTDiscovery] Could not read wlan0 MAC, using default";
        bssid = "00:00:00:00:00:00";
    }
    response.set_bssid(bssid.toStdString());

    qInfo() << "[BTDiscovery] Sending WifiInfoResponse (creds): ssid="
            << config_->wifiSsid() << "bssid=" << bssid;
    sendMessage(response, kMsgWifiInfoResponse);
}

void BluetoothDiscoveryService::handleWifiConnectionStatus(
    const QByteArray& data, uint16_t length)
{
    // Phone reports WiFi connection result (msgId=7)
    oaa::proto::messages::WifiInfoResponse msg;
    if (!msg.ParseFromArray(data.data() + 4, length)) {
        qCritical() << "[BTDiscovery] Failed to parse WifiConnectionStatus";
        return;
    }

    qInfo() << "[BTDiscovery] WifiConnectionStatus:"
            << msg.ShortDebugString().c_str();

    if (msg.status() == oaa::proto::messages::WifiInfoResponse_Status_STATUS_SUCCESS) {
        qInfo() << "[BTDiscovery] Phone connected to WiFi!";
        emit phoneWillConnect();
    } else {
        qCritical() << "[BTDiscovery] Phone WiFi connection failed:"
                     << msg.status();
        emit error(QString("Phone WiFi connection failed (status %1)").arg(msg.status()));
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

    qDebug() << "[BTDiscovery] Sending" << message.GetTypeName().c_str()
             << "(msgId=" << type << ", size=" << byteSize << ")";

    qint64 written = socket_->write(out);
    if (written < 0) {
        qCritical() << "[BTDiscovery] Failed to write to BT socket";
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

} // namespace aa
} // namespace oap
