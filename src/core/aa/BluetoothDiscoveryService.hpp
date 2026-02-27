#pragma once

#include <QObject>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QBluetoothServiceInfo>
#include <google/protobuf/message.h>
#include "../../core/Configuration.hpp"

namespace oap {
namespace aa {

class BluetoothDiscoveryService : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothDiscoveryService(
        std::shared_ptr<oap::Configuration> config,
        const QString& wifiInterface = QStringLiteral("wlan0"),
        QObject* parent = nullptr);
    ~BluetoothDiscoveryService() override;

    void start();
    void stop();

    /// Re-send WifiStartRequest through the existing RFCOMM socket to make the
    /// phone re-initiate the WiFi → TCP → AA connection. Used after a deliberate
    /// session disconnect (e.g. video settings change) when BT is still connected.
    void retrigger();

    QString localAddress() const;

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

    std::shared_ptr<oap::Configuration> config_;
    std::unique_ptr<QBluetoothServer> rfcommServer_;
    QBluetoothSocket* socket_ = nullptr;
    QByteArray buffer_;
    QString wifiInterface_;
    uint32_t sdpRecordHandle_ = 0;
};

} // namespace aa
} // namespace oap
