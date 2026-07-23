#pragma once

#include "IBluetoothService.hpp"
#include "TelephonyClient.hpp"
#include "ui/PairedDevicesModel.hpp"
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QAbstractListModel>
#include <QTimer>
#include <QStringList>
#include <QMap>

class BluezAgentAdaptor;
class QDBusServiceWatcher;

namespace oap {

class IConfigService;
class BluetoothManagerTestAccess;

using BluezInterfaceMap = InterfaceMap;
using BluezManagedObjectMap = QMap<QString, BluezInterfaceMap>;

class BluetoothManager : public QObject, public IBluetoothService {
    Q_OBJECT
    Q_PROPERTY(QString adapterAddress READ adapterAddress NOTIFY adapterAddressChanged)
    Q_PROPERTY(QString adapterAlias READ adapterAlias NOTIFY adapterAliasChanged)
    Q_PROPERTY(bool discoverable READ isDiscoverable NOTIFY discoverableChanged)
    Q_PROPERTY(bool pairable READ isPairable WRITE setPairable NOTIFY pairableChanged)
    Q_PROPERTY(bool pairingActive READ isPairingActive NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString pairingDeviceName READ pairingDeviceName NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString pairingPasskey READ pairingPasskey NOTIFY pairingActiveChanged)
    Q_PROPERTY(int pairingEntered READ pairingEntered NOTIFY pairingActiveChanged)
    Q_PROPERTY(bool pairingRequiresConfirmation READ pairingRequiresConfirmation NOTIFY pairingActiveChanged)
    Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY connectedDeviceChanged)
    Q_PROPERTY(bool needsFirstPairing READ needsFirstPairing NOTIFY needsFirstPairingChanged)

public:
    explicit BluetoothManager(IConfigService* configService, QObject* parent = nullptr);
    BluetoothManager(IConfigService* configService, const QDBusConnection& bus,
                     QObject* parent = nullptr);
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
    int pairingEntered() const;
    bool pairingRequiresConfirmation() const;
    Q_INVOKABLE void confirmPairing() override;
    Q_INVOKABLE void rejectPairing() override;
    QAbstractListModel* pairedDevicesModel() override;
    Q_INVOKABLE void forgetDevice(const QString& address) override;
    Q_INVOKABLE void dismissFirstRunBanner();
    bool needsFirstPairing() const;
    void startAutoConnect() override;
    void cancelAutoConnect() override;
    QString connectedDeviceName() const override;
    QString connectedDeviceAddress() const override;
    void initialize() override;
    void shutdown() override;

    /// Apply one fully parsed ObjectManager snapshot. Production reaches this
    /// through the asynchronous D-Bus reply; tests use it to pin state
    /// derivation without a live bluetoothd.
    void applyManagedObjectsSnapshot(const BluezManagedObjectMap& objects);

signals:
    void adapterAddressChanged();
    void adapterAliasChanged();
    void discoverableChanged();
    void pairableChanged();
    void pairingActiveChanged();
    void connectedDeviceChanged();
    void needsFirstPairingChanged();

private slots:
    void onDevicePropertiesChanged(const QString& interface,
                                   const QVariantMap& changed,
                                   const QStringList& invalidated);
    void onInterfacesAdded(const QDBusObjectPath& path,
                           const BluezInterfaceMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path,
                             const QStringList& interfaces);
    // Direct test seam for refresh coalescing; production ObjectManager
    // subscriptions use the typed slots above.
    void onInterfacesChanged();

private:
    friend class ::BluezAgentAdaptor;
    friend class BluetoothManagerTestAccess;

    // D-Bus helpers
    void setupAdapter();
    void registerAgent();
    void unregisterAgent();
    void setAdapterProperty(const QString& property, const QVariant& value);
    QVariant getAdapterProperty(const QString& property);
    QString deviceNameFromPath(const QString& devicePath);
    void setDeviceProperty(const QString& devicePath, const QString& property, const QVariant& value);
    void requestManagedObjectsRefresh();
    void finishManagedObjectsRefresh(const QDBusMessage& reply);
    void resetAdapterEpoch();

    // Called by BluezAgentAdaptor
    void handleAgentRequestConfirmation(const QDBusMessage& msg, const QString& devicePath, uint passkey);
    void handleAgentRequestAuthorization(const QDBusMessage& msg, const QString& devicePath);
    void handleAgentDisplayPasskey(const QString& devicePath, const QString& passkey,
                                   int entered = -1);
    void handleAgentRelease();
    void handleAgentCancel();
    void abortPairingPrompt(const QString& reason);
    void clearPairingPrompt();

    // First-run pairing
    void checkFirstRunPairing();

    // Connected device tracking
    void updateConnectedDevice();

    // Auto-connect
    void attemptConnect();
    int nextRetryInterval() const;
    static constexpr int MAX_ATTEMPTS = 13;  // 6 + 4 + 3

    IConfigService* configService_ = nullptr;
    QDBusConnection bus_;
    QString adapterPath_;  // e.g. "/org/bluez/hci0"
    QString adapterAddress_;
    QString adapterAlias_;
    bool discoverable_ = false;
    bool pairable_ = false;
    bool pairingActive_ = false;
    QString pairingDeviceName_;
    QString pairingPasskey_;
    int pairingEntered_ = -1;
    enum class PairingPromptMode { None, Confirmation, Authorization, DisplayOnly };
    PairingPromptMode pairingPromptMode_ = PairingPromptMode::None;
    QString connectedDeviceName_;
    QString connectedDeviceAddress_;
    QMap<QString, QString> deviceNamesByPath_;
    BluezAgentAdaptor* agentAdaptor_ = nullptr;
    QDBusMessage pendingPairingMessage_;
    QString pendingPairingDevicePath_;
    PairedDevicesModel* pairedDevicesModel_ = nullptr;
    int lastPairedCount_ = -1;  // last logged paired-device count (log on change only)
    bool shutdown_ = false;
    bool needsFirstPairing_ = false;
    QTimer* pairableRenewTimer_ = nullptr;
    bool managedObjectsRefreshInFlight_ = false;
    bool managedObjectsRefreshPending_ = false;
    quint64 bluezServiceGeneration_ = 0;
    quint64 managedObjectsRequestGeneration_ = 0;
    bool initialSnapshotApplied_ = false;
    QString configuredAdapterPath_;
    bool subscriptionsConnected_ = false;
    QDBusServiceWatcher* bluezServiceWatcher_ = nullptr;

    // Auto-connect state
    QTimer* autoConnectTimer_ = nullptr;
    int autoConnectAttempt_ = 0;
    int autoConnectDeviceIndex_ = 0;
    bool autoConnectInFlight_ = false;
    quint64 autoConnectGeneration_ = 0;
    QStringList pairedDevicePaths_;
};

} // namespace oap
