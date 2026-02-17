#pragma once

#include <QObject>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QBluetoothServiceInfo>
#include <memory>
#include <google/protobuf/message.h>
#include "../../core/Configuration.hpp"

class BluetoothDiscoveryService : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothDiscoveryService(
        std::shared_ptr<oap::Configuration> config,
        QObject* parent = nullptr);
    ~BluetoothDiscoveryService() override;

    void start();
    void stop();

signals:
    void phoneWillConnect();
    void error(const QString& message);

private slots:
    void onClientConnected();
    void readSocket();

private:
    void sendMessage(const google::protobuf::Message& message, uint16_t type);
    void handleWifiSecurityRequest(const QByteArray& data, uint16_t length);
    void handleWifiInfoResponse(const QByteArray& data, uint16_t length);

    std::string getLocalIP(const QString& interfaceName) const;

    std::shared_ptr<oap::Configuration> config_;
    std::unique_ptr<QBluetoothServer> rfcommServer_;
    QBluetoothSocket* socket_ = nullptr;
    QBluetoothServiceInfo serviceInfo_;
    QByteArray buffer_;
};
