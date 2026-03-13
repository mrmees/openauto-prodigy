#pragma once

#include <QObject>
#include <QTimer>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QBluetoothServiceInfo>
#include <google/protobuf/message.h>

#include "oaa/wifi/WifiStartRequestMessage.pb.h"
#include "oaa/wifi/WifiSecurityResponseMessage.pb.h"

namespace oap {

class IConfigService;

namespace aa {

class BluetoothDiscoveryService : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothDiscoveryService(
        oap::IConfigService* configService,
        const QString& wifiInterface = QStringLiteral("wlan0"),
        QObject* parent = nullptr);
    ~BluetoothDiscoveryService() override;

    void start();
    void stop();

    /// Re-send WifiStartRequest through the existing RFCOMM socket to make the
    /// phone re-initiate the WiFi -> TCP -> AA connection. Used after a deliberate
    /// session disconnect (e.g. video settings change) when BT is still connected.
    void retrigger();

    QString localAddress() const;

    /// Pure message builders — testable without hardware/sockets
    static oaa::proto::messages::WifiStartRequest buildWifiStartRequest(
        const std::string& ip, uint32_t port);
    static oaa::proto::messages::WifiSecurityResponse buildWifiCredentialResponse(
        const QString& ssid, const QString& password, const QString& bssid);

signals:
    void phoneWillConnect();
    void error(const QString& message);

private slots:
    void onClientConnected();
    void readSocket();

private:
    void sendMessage(const google::protobuf::Message& message, uint16_t type);
    void sendWifiStartRequest();
    void handleWifiCredentialRequest();
    void handleWifiConnectionStatus(const QByteArray& data, uint16_t length);
    bool registerSdpRecord(uint8_t rfcommChannel);
    void unregisterSdpRecord();
    std::string getLocalIP(const QString& interfaceName) const;

    void attemptSdpRegistration();

    oap::IConfigService* configService_;
    std::unique_ptr<QBluetoothServer> rfcommServer_;
    QBluetoothSocket* socket_ = nullptr;
    QByteArray buffer_;
    QString wifiInterface_;
    uint32_t sdpRecordHandle_ = 0;
    QTimer sdpRetryTimer_;
    uint8_t rfcommPort_ = 0;
    int sdpRetryCount_ = 0;
};

} // namespace aa
} // namespace oap
