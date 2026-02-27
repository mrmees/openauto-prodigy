#pragma once

#include "IBluetoothService.hpp"
#include "ui/PairedDevicesModel.hpp"
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QAbstractListModel>
#include <QTimer>
#include <QStringList>
#include <memory>

class BluezAgentAdaptor;

namespace oap {

class IConfigService;

class BluetoothManager : public QObject, public IBluetoothService {
    Q_OBJECT
    Q_PROPERTY(QString adapterAddress READ adapterAddress CONSTANT)
    Q_PROPERTY(QString adapterAlias READ adapterAlias NOTIFY adapterAliasChanged)
    Q_PROPERTY(bool discoverable READ isDiscoverable NOTIFY discoverableChanged)
    Q_PROPERTY(bool pairable READ isPairable WRITE setPairable NOTIFY pairableChanged)
    Q_PROPERTY(bool pairingActive READ isPairingActive NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString pairingDeviceName READ pairingDeviceName NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString pairingPasskey READ pairingPasskey NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY connectedDeviceChanged)

public:
    explicit BluetoothManager(IConfigService* configService, QObject* parent = nullptr);
    ~BluetoothManager() override;

    // IBluetoothService
    QString adapterAddress() const override;
    QString adapterAlias() const override;
    bool isDiscoverable() const override;
    bool isPairable() const override;
    void setPairable(bool enabled) override;
    bool isPairingActive() const override;
    QString pairingDeviceName() const override;
    QString pairingPasskey() const override;
    Q_INVOKABLE void confirmPairing() override;
    Q_INVOKABLE void rejectPairing() override;
    QAbstractListModel* pairedDevicesModel() override;
    Q_INVOKABLE void forgetDevice(const QString& address) override;
    void startAutoConnect() override;
    void cancelAutoConnect() override;
    QString connectedDeviceName() const override;
    QString connectedDeviceAddress() const override;
    void initialize() override;
    void shutdown() override;

signals:
    void adapterAliasChanged();
    void discoverableChanged();
    void pairableChanged();
    void pairingActiveChanged();
    void connectedDeviceChanged();
    void profileNewConnection();  // RFCOMM NewConnection — auto-connect stop signal

private slots:
    void onDevicePropertiesChanged(const QString& interface,
                                   const QVariantMap& changed,
                                   const QStringList& invalidated);

private:
    friend class ::BluezAgentAdaptor;

    // D-Bus helpers
    void setupAdapter();
    void registerAgent();
    void unregisterAgent();
    void setAdapterProperty(const QString& property, const QVariant& value);
    QVariant getAdapterProperty(const QString& property);
    QString findAdapterPath();
    QString deviceNameFromPath(const QString& devicePath);
    void setDeviceProperty(const QString& devicePath, const QString& property, const QVariant& value);
    void refreshPairedDevices();

    // Called by BluezAgentAdaptor
    void handleAgentRequestConfirmation(const QDBusMessage& msg, const QString& devicePath, uint passkey);
    void handleAgentCancel();

    // Auto-connect
    void attemptConnect();
    int nextRetryInterval() const;
    static constexpr int MAX_ATTEMPTS = 13;  // 6 + 4 + 3

    IConfigService* configService_ = nullptr;
    QString adapterPath_;  // e.g. "/org/bluez/hci0"
    QString adapterAddress_;
    QString adapterAlias_;
    bool discoverable_ = false;
    bool pairable_ = false;
    bool pairingActive_ = false;
    QString pairingDeviceName_;
    QString pairingPasskey_;
    QString connectedDeviceName_;
    QString connectedDeviceAddress_;
    BluezAgentAdaptor* agentAdaptor_ = nullptr;
    QDBusMessage pendingPairingMessage_;
    QString pendingPairingDevicePath_;
    PairedDevicesModel* pairedDevicesModel_ = nullptr;

    // Auto-connect state
    QTimer* autoConnectTimer_ = nullptr;
    int autoConnectAttempt_ = 0;
    int autoConnectDeviceIndex_ = 0;
    bool autoConnectInFlight_ = false;
    QStringList pairedDevicePaths_;
};

} // namespace oap
