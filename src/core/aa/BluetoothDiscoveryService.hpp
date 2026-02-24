#pragma once

#include <QObject>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QBluetoothServiceInfo>
#include <memory>
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

    QString localAddress() const;

signals:
    void phoneWillConnect();
    void error(const QString& message);

private slots:
    void onClientConnected();
    void readSocket();

private:
    void sendMessage(const google::protobuf::Message& message, uint16_t type);
    void handleWifiCredentialRequest();
    void handleWifiConnectionStatus(const QByteArray& data, uint16_t length);

    std::string getLocalIP(const QString& interfaceName) const;

    std::shared_ptr<oap::Configuration> config_;
    std::unique_ptr<QBluetoothServer> rfcommServer_;
    QBluetoothSocket* socket_ = nullptr;
    QBluetoothServiceInfo serviceInfo_;
    QByteArray buffer_;
    QString wifiInterface_;
};

} // namespace aa
} // namespace oap
