#pragma once

#include <QObject>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QBluetoothServiceInfo>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <memory>
#include <vector>
#include <google/protobuf/message.h>
#include "../../core/Configuration.hpp"

namespace oap {
namespace aa {

/// Minimal D-Bus adaptor implementing org.bluez.Profile1 interface.
/// Handles NewConnection by dup'ing the fd to keep the connection alive.
/// Handles Release and RequestDisconnection as no-ops.
class BluezProfile1Adaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Profile1")

public:
    explicit BluezProfile1Adaptor(QObject* parent, std::vector<int>& fdStore)
        : QDBusAbstractAdaptor(parent), fdStore_(fdStore) {}

public slots:
    void NewConnection(const QDBusObjectPath& device,
                       const QDBusUnixFileDescriptor& fd,
                       const QVariantMap& properties);
    void RequestDisconnection(const QDBusObjectPath& device);
    void Release();

private:
    std::vector<int>& fdStore_;
};

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
    void registerBluetoothProfiles();
    void unregisterBluetoothProfiles();

    std::string getLocalIP(const QString& interfaceName) const;

    std::shared_ptr<oap::Configuration> config_;
    std::unique_ptr<QBluetoothServer> rfcommServer_;
    QBluetoothSocket* socket_ = nullptr;
    QByteArray buffer_;
    QString wifiInterface_;
    uint32_t sdpRecordHandle_ = 0;

    // D-Bus ProfileManager1 profiles (HFP AG, HSP HS) — keep fds alive
    std::vector<int> profileFds_;
    QStringList registeredProfilePaths_;
    std::vector<std::unique_ptr<QObject>> profileObjects_;  // D-Bus path holders
};

} // namespace aa
} // namespace oap
