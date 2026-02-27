#pragma once

#include "IBluetoothService.hpp"
#include <QObject>
#include <QDBusConnection>
#include <QDBusVariant>
#include <QAbstractListModel>
#include <memory>

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

private:
    // D-Bus helpers
    void setupAdapter();
    void setAdapterProperty(const QString& property, const QVariant& value);
    QVariant getAdapterProperty(const QString& property);
    QString findAdapterPath();

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
};

} // namespace oap
