#include "BluetoothDiscoveryService.hpp"

#include <QBluetoothAddress>
#include <QBluetoothLocalDevice>
#include <QBluetoothUuid>
#include <QDataStream>
#include <QNetworkInterface>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDBusUnixFileDescriptor>
#include <QDebug>

#include <unistd.h>

// BlueZ SDP C library for direct SDP record registration.
// The AA RFCOMM record is registered via the legacy SDP socket (--compat mode)
// because ProfileManager1 would conflict with QBluetoothServer on the same channel.
// HFP AG and HSP HS profiles use ProfileManager1 (different UUIDs, no conflict).
extern "C" {
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
}

#include "WifiStartRequestMessage.pb.h"
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

    quint16 port = rfcommServer_->serverPort();
    qInfo() << "[BTDiscovery] RFCOMM listening on port" << port;

    // Register SDP record via BlueZ's legacy SDP socket (requires --compat).
    // We can't use ProfileManager1 D-Bus because it tries to bind its own
    // RFCOMM socket on the same channel, conflicting with QBluetoothServer.
    if (!registerSdpRecord(static_cast<uint8_t>(port))) {
        qCritical() << "[BTDiscovery] Failed to register SDP record";
        emit error("Failed to register Bluetooth SDP service");
        return;
    }

    qInfo() << "[BTDiscovery] SDP service registered (AA Wireless)";

    // Register HFP AG and HSP HS profiles via D-Bus so the phone sees
    // standard profiles and doesn't disconnect with "No profiles".
    registerBluetoothProfiles();
}

void BluetoothDiscoveryService::stop()
{
    unregisterBluetoothProfiles();
    unregisterSdpRecord();

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

bool BluetoothDiscoveryService::registerSdpRecord(uint8_t rfcommChannel)
{
    // Connect to local SDP server (legacy socket at /var/run/sdp)
    bdaddr_t anyAddr = {{0, 0, 0, 0, 0, 0}};
    bdaddr_t localAddr = {{0, 0, 0, 0xff, 0xff, 0xff}};
    sdp_session_t* session = sdp_connect(&anyAddr, &localAddr, SDP_RETRY_IF_BUSY);
    if (!session) {
        qCritical() << "[BTDiscovery] sdp_connect failed:" << strerror(errno)
                     << "- is bluetoothd running with --compat?";
        return false;
    }

    // Remove BlueZ core SDP records (handles 0x10000-0x10003: PnP, GAP, GATT, DevInfo).
    // These records contain mixed 16-bit/128-bit UUIDs that trigger Android's
    // sdpu_compare_uuid_with_attr() size mismatch bug during SDP browse.
    for (uint32_t handle = 0x10000; handle <= 0x10003; ++handle) {
        if (sdp_device_record_unregister_binary(session, &localAddr, handle) < 0) {
            sdp_record_t coreRec;
            memset(&coreRec, 0, sizeof(coreRec));
            coreRec.handle = handle;
            if (sdp_record_unregister(session, &coreRec) < 0) {
                qDebug() << "[BTDiscovery] Could not remove core SDP record"
                         << Qt::hex << handle << "(may not exist)";
            } else {
                qInfo() << "[BTDiscovery] Removed core SDP record" << Qt::hex << handle;
            }
        } else {
            qInfo() << "[BTDiscovery] Removed core SDP record" << Qt::hex << handle;
        }
    }

    sdp_record_t* record = sdp_record_alloc();

    // AA Wireless UUID: 4de17a00-52cb-11e6-bdf4-0800200c9a66
    // Network byte order (big-endian)
    uint128_t uuid128 = {{
        0x4d, 0xe1, 0x7a, 0x00, 0x52, 0xcb, 0x11, 0xe6,
        0xbd, 0xf4, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66
    }};
    uuid_t aaUuid;
    sdp_uuid128_create(&aaUuid, &uuid128);

    // Attribute 0x0001: ServiceClassIDList = [AA UUID] only.
    // DO NOT include SerialPort (16-bit UUID) — Android's sdpu_compare_uuid_with_attr()
    // does strict size comparison and rejects records mixing 16-bit and 128-bit UUIDs.
    sdp_list_t* classList = sdp_list_append(nullptr, &aaUuid);
    sdp_set_service_classes(record, classList);

    // Attribute 0x0004: ProtocolDescriptorList = [[L2CAP], [RFCOMM, channel]]
    uuid_t l2capUuid, rfcommUuid;
    sdp_uuid16_create(&l2capUuid, L2CAP_UUID);
    sdp_uuid16_create(&rfcommUuid, RFCOMM_UUID);

    sdp_list_t* l2capList = sdp_list_append(nullptr, &l2capUuid);

    sdp_data_t* channelData = sdp_data_alloc(SDP_UINT8, &rfcommChannel);
    sdp_list_t* rfcommList = sdp_list_append(nullptr, &rfcommUuid);
    rfcommList = sdp_list_append(rfcommList, channelData);

    sdp_list_t* protoList = sdp_list_append(nullptr, l2capList);
    protoList = sdp_list_append(protoList, rfcommList);

    sdp_list_t* accessProtoList = sdp_list_append(nullptr, protoList);
    sdp_set_access_protos(record, accessProtoList);

    // Attribute 0x0005: BrowseGroupList = [PublicBrowseGroup]
    uuid_t browseUuid;
    sdp_uuid16_create(&browseUuid, PUBLIC_BROWSE_GROUP);
    sdp_list_t* browseList = sdp_list_append(nullptr, &browseUuid);
    sdp_set_browse_groups(record, browseList);

    // No ProfileDescriptorList — SerialPort profile descriptor uses a 16-bit UUID
    // which triggers the same Android UUID size mismatch bug.

    // Attribute 0x0100: ServiceName
    sdp_set_info_attr(record, "Android Auto Wireless", nullptr, nullptr);

    // Register with SDP server
    if (sdp_record_register(session, record, 0) < 0) {
        qCritical() << "[BTDiscovery] sdp_record_register failed:" << strerror(errno);
        sdp_record_free(record);
        sdp_close(session);
        return false;
    }

    sdpRecordHandle_ = record->handle;
    qInfo() << "[BTDiscovery] SDP record handle:" << Qt::hex << sdpRecordHandle_;

    // Free lists (record data is now owned by SDP server)
    sdp_data_free(channelData);
    sdp_list_free(classList, nullptr);
    sdp_list_free(l2capList, nullptr);
    sdp_list_free(rfcommList, nullptr);
    sdp_list_free(protoList, nullptr);
    sdp_list_free(accessProtoList, nullptr);
    sdp_list_free(browseList, nullptr);

    // Keep session open — closing it would unregister the record
    // Store the session pointer so we can close it in stop()
    // Actually, sdp_record_register with SDP_SERVER_RECORD_PERSIST would
    // persist across session close, but that flag requires root.
    // Instead we just leak the session — it closes when the process exits.
    // This is fine for a long-running app.

    sdp_record_free(record);
    // Don't close session — record is unregistered when session closes
    // sdp_close(session) would remove the record!

    return true;
}

void BluetoothDiscoveryService::unregisterSdpRecord()
{
    // Record is automatically unregistered when the SDP session closes,
    // which happens when the process exits. For explicit cleanup,
    // we'd need to keep the session handle, but this is fine for our use case.
    sdpRecordHandle_ = 0;
}

// --- BluezProfile1Adaptor implementation ---

void BluezProfile1Adaptor::NewConnection(
    const QDBusObjectPath& device,
    const QDBusUnixFileDescriptor& fd,
    const QVariantMap& /*properties*/)
{
    // Dup the fd so it stays alive after BlueZ closes its end.
    // This keeps the profile connection open — without it, the phone
    // sees a disconnect and may drop the BT link.
    if (fd.isValid()) {
        int dupFd = ::dup(fd.fileDescriptor());
        if (dupFd >= 0) {
            fdStore_.push_back(dupFd);
            qInfo() << "[BTDiscovery] Profile NewConnection from"
                     << device.path() << "— holding fd" << dupFd;
        }
    }
}

void BluezProfile1Adaptor::RequestDisconnection(const QDBusObjectPath& device)
{
    qInfo() << "[BTDiscovery] Profile RequestDisconnection:" << device.path();
}

void BluezProfile1Adaptor::Release()
{
    qInfo() << "[BTDiscovery] Profile released";
}

// --- BluetoothDiscoveryService profile registration ---

void BluetoothDiscoveryService::registerBluetoothProfiles()
{
    // Register dummy HFP AG and HSP HS profiles via BlueZ ProfileManager1.
    // Required because:
    // 1. Android requires HFP AG or logs WIRELESS_SETUP_FAILED_TO_START_NO_HFP_FROM_HU_PRESENCE
    // 2. Without a standard profile, phone shows "No profiles" and disconnects
    //
    // These don't conflict with QBluetoothServer — different UUIDs, different channels.

    struct ProfileInfo {
        const char* uuid;
        const char* path;
        const char* name;
    };

    static const ProfileInfo profiles[] = {
        {"0000111f-0000-1000-8000-00805f9b34fb", "/org/openauto/hfp_ag", "HFP AG"},
        {"00001108-0000-1000-8000-00805f9b34fb", "/org/openauto/hsp_hs", "HSP HS"},
    };

    auto bus = QDBusConnection::systemBus();

    for (const auto& prof : profiles) {
        // Create a QObject to live at the D-Bus path, with a Profile1 adaptor
        // that handles NewConnection/Release/RequestDisconnection.
        auto obj = std::make_unique<QObject>();
        new BluezProfile1Adaptor(obj.get(), profileFds_);

        if (!bus.registerObject(prof.path, obj.get(),
                QDBusConnection::ExportAdaptors)) {
            qWarning() << "[BTDiscovery] Failed to register D-Bus object at"
                        << prof.path;
            continue;
        }

        profileObjects_.push_back(std::move(obj));

        // Now tell BlueZ to register this profile
        QVariantMap options;
        options["Role"] = QVariant::fromValue(QDBusVariant(QString("server")));
        options["RequireAuthentication"] = QVariant::fromValue(QDBusVariant(false));
        options["RequireAuthorization"] = QVariant::fromValue(QDBusVariant(false));
        options["AutoConnect"] = QVariant::fromValue(QDBusVariant(true));

        QDBusMessage call = QDBusMessage::createMethodCall(
            "org.bluez",
            "/org/bluez",
            "org.bluez.ProfileManager1",
            "RegisterProfile");
        call << QVariant::fromValue(QDBusObjectPath(prof.path))
             << QString(prof.uuid)
             << options;

        QDBusMessage reply = bus.call(call, QDBus::Block, 5000);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "[BTDiscovery] Failed to register" << prof.name
                        << "profile:" << reply.errorMessage();
        } else {
            qInfo() << "[BTDiscovery] Registered" << prof.name
                     << "profile via ProfileManager1";
            registeredProfilePaths_.append(prof.path);
        }
    }
}

void BluetoothDiscoveryService::unregisterBluetoothProfiles()
{
    auto bus = QDBusConnection::systemBus();

    for (const auto& path : registeredProfilePaths_) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            "org.bluez",
            "/org/bluez",
            "org.bluez.ProfileManager1",
            "UnregisterProfile");
        call << QVariant::fromValue(QDBusObjectPath(path));
        bus.call(call, QDBus::Block, 2000);  // Best-effort
        bus.unregisterObject(path);
    }
    registeredProfilePaths_.clear();
    profileObjects_.clear();  // Destroys QObjects (and their adaptors)

    // Close held profile fds
    for (int fd : profileFds_) {
        ::close(fd);
    }
    profileFds_.clear();
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

    sendWifiStartRequest();
}

void BluetoothDiscoveryService::sendWifiStartRequest()
{
    if (!socket_) return;

    std::string localIp = getLocalIP(wifiInterface_);
    if (localIp.empty() && wifiInterface_ == "wlan0") {
        localIp = getLocalIP(QStringLiteral("ap0"));
    }
    if (localIp.empty()) {
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

    oaa::proto::messages::WifiStartRequest request;
    request.set_ip_address(localIp);
    request.set_port(config_->tcpPort());

    qInfo() << "[BTDiscovery] Sending WifiStartRequest: ip=" << localIp.c_str()
            << "port=" << config_->tcpPort();
    sendMessage(request, kMsgWifiStartRequest);
}

void BluetoothDiscoveryService::retrigger()
{
    if (!socket_ || socket_->state() != QBluetoothSocket::SocketState::ConnectedState) {
        qInfo() << "[BTDiscovery] retrigger: RFCOMM socket not connected, phone must reconnect via BT";
        return;
    }
    qInfo() << "[BTDiscovery] Retrigger: re-sending WifiStartRequest to reconnect";
    sendWifiStartRequest();
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
    // WifiInfoResponse (APK class wdm) has: ssid, bssid, passphrase, security_mode, ap_type
    // A successful connection status parses OK; failure is indicated by parse failure
    // or empty/missing fields.
    oaa::proto::messages::WifiInfoResponse msg;
    if (!msg.ParseFromArray(data.data() + 4, length)) {
        qCritical() << "[BTDiscovery] Failed to parse WifiConnectionStatus";
        emit error("Phone WiFi connection status parse failed");
        return;
    }

    qInfo() << "[BTDiscovery] WifiConnectionStatus:"
            << msg.ShortDebugString().c_str();

    // If we received a parseable response, the phone has connected
    qInfo() << "[BTDiscovery] Phone connected to WiFi!";
    emit phoneWillConnect();
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
    qDebug() << "[BTDiscovery] Payload hex:" << out.mid(4).toHex(' ');
    qDebug() << "[BTDiscovery] Proto debug:" << message.ShortDebugString().c_str();

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
