#include "BluetoothManager.hpp"
#include "IConfigService.hpp"
#include "ui/PairedDevicesModel.hpp"
#include "../Logging.hpp"
#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>

namespace {

constexpr int kDbusTimeoutMs = 2000;
const QString kBluezService = QStringLiteral("org.bluez");
const QString kObjectManagerInterface = QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString kPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

oap::BluezManagedObjectMap parseManagedObjects(const QDBusMessage& reply)
{
    oap::BluezManagedObjectMap objects;
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return objects;

    const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath objectPath;
        arg >> objectPath;

        oap::BluezInterfaceMap interfaces;
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QString interfaceName;
            arg >> interfaceName;

            QVariantMap properties;
            arg.beginMap();
            while (!arg.atEnd()) {
                arg.beginMapEntry();
                QString key;
                QDBusVariant value;
                arg >> key >> value;
                properties.insert(key, value.variant());
                arg.endMapEntry();
            }
            arg.endMap();
            interfaces.insert(interfaceName, properties);
            arg.endMapEntry();
        }
        arg.endMap();
        objects.insert(objectPath.path(), interfaces);
        arg.endMapEntry();
    }
    arg.endMap();
    return objects;
}

QDBusMessage propertiesMessage(const QString& path, const QString& method)
{
    return QDBusMessage::createMethodCall(kBluezService, path,
                                           kPropertiesInterface, method);
}

} // namespace

// BluezAgentAdaptor — handles org.bluez.Agent1 D-Bus method calls from BlueZ.
// Defined here (not in header) because it's an implementation detail.
// Requires #include "BluetoothManager.moc" at end of file for AUTOMOC.
class BluezAgentAdaptor : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")
public:
    explicit BluezAgentAdaptor(oap::BluetoothManager* manager, QObject* parent = nullptr)
        : QObject(parent), manager_(manager) {}

public slots:
    void Release() {
        qCInfo(lcBT) << "[Agent] Released";
    }

    void RequestConfirmation(const QDBusObjectPath& device, uint passkey) {
        qCInfo(lcBT) << "[Agent] RequestConfirmation:" << device.path() << passkey;
        setDelayedReply(true);
        manager_->handleAgentRequestConfirmation(message(), device.path(), passkey);
    }

    QString RequestPinCode(const QDBusObjectPath& device) {
        qCWarning(lcBT) << "[Agent] RequestPinCode rejected (no keyboard input):"
                        << device.path();
        sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                       QStringLiteral("Head unit does not provide PIN entry"));
        return {};
    }

    uint RequestPasskey(const QDBusObjectPath& device) {
        qCWarning(lcBT) << "[Agent] RequestPasskey rejected (no keyboard input):"
                        << device.path();
        sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                       QStringLiteral("Head unit does not provide passkey entry"));
        return 0;
    }

    void DisplayPinCode(const QDBusObjectPath& device, const QString& pinCode) {
        qCInfo(lcBT) << "[Agent] DisplayPinCode:" << device.path() << pinCode;
        manager_->handleAgentDisplayPasskey(device.path(), pinCode);
    }

    void DisplayPasskey(const QDBusObjectPath& device, uint passkey, ushort entered) {
        qCInfo(lcBT) << "[Agent] DisplayPasskey:" << device.path() << passkey;
        manager_->handleAgentDisplayPasskey(
            device.path(), QStringLiteral("%1").arg(passkey, 6, 10, QChar('0')), entered);
    }

    void RequestAuthorization(const QDBusObjectPath& device) {
        qCInfo(lcBT) << "[Agent] RequestAuthorization:" << device.path();
        setDelayedReply(true);
        manager_->handleAgentRequestAuthorization(message(), device.path());
    }

    void AuthorizeService(const QDBusObjectPath& device, const QString& uuid) {
        qCDebug(lcBT) << "[Agent] AuthorizeService:" << device.path() << uuid;
        // Auto-accept all services from paired devices (no delayed reply needed)
    }

    void Cancel() {
        qCInfo(lcBT) << "[Agent] Cancel";
        manager_->handleAgentCancel();
    }

private:
    oap::BluetoothManager* manager_;
};

namespace oap {

BluetoothManager::BluetoothManager(IConfigService* configService, QObject* parent)
    : BluetoothManager(configService, QDBusConnection::systemBus(), parent)
{
}

BluetoothManager::BluetoothManager(IConfigService* configService,
                                   const QDBusConnection& bus, QObject* parent)
    : QObject(parent)
    , configService_(configService)
    , bus_(bus)
{
    pairedDevicesModel_ = new PairedDevicesModel(this);
}

BluetoothManager::~BluetoothManager()
{
    shutdown();
}

QString BluetoothManager::adapterAddress() const { return adapterAddress_; }
QString BluetoothManager::adapterAlias() const { return adapterAlias_; }
bool BluetoothManager::isDiscoverable() const { return discoverable_; }
bool BluetoothManager::isPairable() const { return pairable_; }

void BluetoothManager::setPairable(bool enabled)
{
    if (pairable_ == enabled) return;
    pairable_ = enabled;
    emit pairableChanged();
    qCDebug(lcBT) << "Pairable:" << enabled;
    if (!adapterPath_.isEmpty())
        setAdapterProperty("Pairable", enabled);
}

bool BluetoothManager::needsFirstPairing() const { return needsFirstPairing_; }
bool BluetoothManager::isPairingActive() const { return pairingActive_; }
QString BluetoothManager::pairingDeviceName() const { return pairingDeviceName_; }
QString BluetoothManager::pairingPasskey() const { return pairingPasskey_; }
int BluetoothManager::pairingEntered() const { return pairingEntered_; }
bool BluetoothManager::pairingRequiresConfirmation() const
{
    return pairingPromptMode_ == PairingPromptMode::Confirmation
        || pairingPromptMode_ == PairingPromptMode::Authorization;
}

void BluetoothManager::confirmPairing()
{
    if (!pairingActive_) return;
    if (!pairingRequiresConfirmation()) {
        qCInfo(lcBT) << "Pairing display dismissed";
        clearPairingPrompt();
        return;
    }

    qCInfo(lcBT) << "Pairing confirmed by user";
    bus_.send(pendingPairingMessage_.createReply());

    // Trust the device so future connections auto-accept. BlueZ does not mark
    // Device1.Paired until the over-air exchange completes, so first-run state
    // is cleared only by a later managed-object snapshot.
    setDeviceProperty(pendingPairingDevicePath_, QStringLiteral("Trusted"), true);
    clearPairingPrompt();
    requestManagedObjectsRefresh();
}

void BluetoothManager::rejectPairing()
{
    if (!pairingActive_) return;
    if (!pairingRequiresConfirmation()) {
        clearPairingPrompt();
        return;
    }
    qCInfo(lcBT) << "Pairing rejected by user";

    auto reply = pendingPairingMessage_.createErrorReply(
        QStringLiteral("org.bluez.Error.Rejected"),
        QStringLiteral("User rejected pairing"));
    bus_.send(reply);
    clearPairingPrompt();
}

QAbstractListModel* BluetoothManager::pairedDevicesModel()
{
    return pairedDevicesModel_;
}

void BluetoothManager::forgetDevice(const QString& address)
{
    if (adapterPath_.isEmpty()) return;
    qCInfo(lcBT) << "Forget device:" << address;

    // Device paths are like /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF
    QString devAddr = address;
    devAddr.replace(':', '_');
    QString devicePath = adapterPath_ + "/dev_" + devAddr;

    QDBusMessage message = QDBusMessage::createMethodCall(
        kBluezService, adapterPath_, QStringLiteral("org.bluez.Adapter1"),
        QStringLiteral("RemoveDevice"));
    message << QVariant::fromValue(QDBusObjectPath(devicePath));
    const QDBusMessage reply = bus_.call(message, QDBus::Block, kDbusTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
        qCWarning(lcBT) << "RemoveDevice failed:" << reply.errorMessage();

    requestManagedObjectsRefresh();
}

void BluetoothManager::startAutoConnect()
{
    if (adapterPath_.isEmpty() || !connectedDeviceAddress_.isEmpty()) return;

    // Check config
    if (configService_) {
        QVariant enabled = configService_->value("connection.auto_connect_aa");
        if (enabled.isValid() && !enabled.toBool()) {
            qCDebug(lcBT) << "Auto-connect disabled in config";
            return;
        }
    }

    // Build list of paired device paths
    pairedDevicePaths_.clear();
    for (int i = 0; i < pairedDevicesModel_->rowCount(); ++i) {
        QString addr = pairedDevicesModel_->data(
            pairedDevicesModel_->index(i, 0), PairedDevicesModel::AddressRole).toString();
        addr.replace(':', '_');
        pairedDevicePaths_.append(adapterPath_ + "/dev_" + addr);
    }

    if (pairedDevicePaths_.isEmpty()) {
        qCDebug(lcBT) << "No paired devices — skipping auto-connect";
        return;
    }

    autoConnectAttempt_ = 0;
    autoConnectDeviceIndex_ = 0;
    autoConnectInFlight_ = false;

    if (!autoConnectTimer_) {
        autoConnectTimer_ = new QTimer(this);
        autoConnectTimer_->setSingleShot(true);
        connect(autoConnectTimer_, &QTimer::timeout, this, &BluetoothManager::attemptConnect);
    }

    qCInfo(lcBT) << "Starting auto-connect for" << pairedDevicePaths_.size() << "device(s)";
    attemptConnect();  // First attempt immediately
}

void BluetoothManager::cancelAutoConnect()
{
    if (autoConnectTimer_) {
        autoConnectTimer_->stop();
    }
    autoConnectAttempt_ = MAX_ATTEMPTS;  // prevent further attempts
    autoConnectInFlight_ = false;
    pairedDevicePaths_.clear();
    qCInfo(lcBT) << "Auto-connect cancelled";
}

void BluetoothManager::attemptConnect()
{
    if (autoConnectInFlight_) return;
    if (autoConnectAttempt_ >= MAX_ATTEMPTS || pairedDevicePaths_.isEmpty()) {
        qCDebug(lcBT) << "Auto-connect exhausted after" << autoConnectAttempt_ << "attempts";
        return;
    }

    QString devicePath = pairedDevicePaths_[autoConnectDeviceIndex_ % pairedDevicePaths_.size()];
    autoConnectDeviceIndex_++;
    autoConnectInFlight_ = true;

    qCDebug(lcBT) << "Auto-connect attempt" << (autoConnectAttempt_ + 1)
            << "/" << MAX_ATTEMPTS << "→" << devicePath;

    // Async D-Bus call: Device1.Connect(). A raw message avoids the blocking
    // introspection performed by a dynamic QDBusInterface constructor.
    QDBusMessage message = QDBusMessage::createMethodCall(
        kBluezService, devicePath, QStringLiteral("org.bluez.Device1"),
        QStringLiteral("Connect"));
    QDBusPendingCall pending = bus_.asyncCall(message, kDbusTimeoutMs);
    auto* watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        watcher->deleteLater();
        autoConnectInFlight_ = false;

        if (watcher->isError()) {
            qCDebug(lcBT) << "Connect failed:" << watcher->error().message();
        } else {
            qCDebug(lcBT) << "Connect call returned success";
            // Cancellation happens in updateConnectedDevice() when Device1.Connected flips
        }

        autoConnectAttempt_++;
        int interval = nextRetryInterval();
        if (interval > 0 && autoConnectTimer_) {
            autoConnectTimer_->start(interval);
        } else {
            qCDebug(lcBT) << "Auto-connect schedule exhausted";
        }
    });
}

int BluetoothManager::nextRetryInterval() const
{
    if (autoConnectAttempt_ < 6) return 5000;    // 5s × 6
    if (autoConnectAttempt_ < 10) return 30000;   // 30s × 4
    if (autoConnectAttempt_ < 13) return 60000;   // 60s × 3
    return -1;  // stop
}

void BluetoothManager::checkFirstRunPairing()
{
    if (pairedDevicesModel_->rowCount() > 0) return;

    qCInfo(lcBT) << "No paired devices — enabling first-run pairable mode";
    if (!needsFirstPairing_) {
        needsFirstPairing_ = true;
        emit needsFirstPairingChanged();
    }

    setPairable(true);

    // BlueZ PairableTimeout is 120s. Renew at 110s to avoid gaps.
    if (!pairableRenewTimer_) {
        pairableRenewTimer_ = new QTimer(this);
        pairableRenewTimer_->setInterval(110000);
        connect(pairableRenewTimer_, &QTimer::timeout, this, [this]() {
            if (!needsFirstPairing_) {
                pairableRenewTimer_->stop();
                return;
            }
            qCDebug(lcBT) << "Renewing pairable mode for first-run";
            setAdapterProperty("Pairable", true);
            if (!pairable_) {
                pairable_ = true;
                emit pairableChanged();
            }
        });
    }
    pairableRenewTimer_->start();
}

void BluetoothManager::dismissFirstRunBanner()
{
    if (!needsFirstPairing_) return;
    qCInfo(lcBT) << "First-run banner dismissed by user";
    needsFirstPairing_ = false;
    emit needsFirstPairingChanged();
    if (pairableRenewTimer_)
        pairableRenewTimer_->stop();
    // Don't disable pairable — let it expire naturally via BlueZ timeout
}

QString BluetoothManager::connectedDeviceName() const { return connectedDeviceName_; }
QString BluetoothManager::connectedDeviceAddress() const { return connectedDeviceAddress_; }

void BluetoothManager::initialize()
{
    qCDebug(lcBT) << "Initializing...";
    shutdown_ = false;

    // Watch for BlueZ device/adapter property changes
    const bool okProperties = bus_.connect(
        kBluezService, QString(), kPropertiesInterface, QStringLiteral("PropertiesChanged"),
        this, SLOT(onDevicePropertiesChanged(QString,QVariantMap,QStringList)));

    // Watch for new/removed devices (paired externally, removed via bluetoothctl, etc.)
    const bool okAdded = bus_.connect(
        kBluezService, QString(), kObjectManagerInterface, QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesChanged()));
    const bool okRemoved = bus_.connect(
        kBluezService, QString(), kObjectManagerInterface, QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesChanged()));
    subscriptionsConnected_ = okProperties && okAdded && okRemoved;
    if (!subscriptionsConnected_) {
        qCWarning(lcBT) << "D-Bus signal subscription failed:"
                        << "PropertiesChanged" << okProperties
                        << "InterfacesAdded" << okAdded
                        << "InterfacesRemoved" << okRemoved;
    }

    auto* watcher = new QDBusServiceWatcher(
        kBluezService, bus_,
        QDBusServiceWatcher::WatchForRegistration
            | QDBusServiceWatcher::WatchForUnregistration,
        this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this]() {
        qCInfo(lcBT) << "BlueZ restarted — re-initializing";
        configuredAdapterPath_.clear();
        initialSnapshotApplied_ = false;
        requestManagedObjectsRefresh();
    });
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this]() {
        qCInfo(lcBT) << "BlueZ disappeared";
        configuredAdapterPath_.clear();
        initialSnapshotApplied_ = false;
        applyManagedObjectsSnapshot({});
    });

    requestManagedObjectsRefresh();
}

void BluetoothManager::requestManagedObjectsRefresh()
{
    if (shutdown_) return;
    if (managedObjectsRefreshInFlight_) {
        managedObjectsRefreshPending_ = true;
        return;
    }

    managedObjectsRefreshInFlight_ = true;
    QDBusMessage message = QDBusMessage::createMethodCall(
        kBluezService, QStringLiteral("/"), kObjectManagerInterface,
        QStringLiteral("GetManagedObjects"));
    auto* watcher = new QDBusPendingCallWatcher(
        bus_.asyncCall(message, kDbusTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher]() {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        managedObjectsRefreshInFlight_ = false;
        if (!shutdown_)
            finishManagedObjectsRefresh(reply);

        if (!shutdown_ && managedObjectsRefreshPending_) {
            managedObjectsRefreshPending_ = false;
            requestManagedObjectsRefresh();
        }
    });
}

void BluetoothManager::finishManagedObjectsRefresh(const QDBusMessage& reply)
{
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        qCWarning(lcBT) << "GetManagedObjects failed:" << reply.errorMessage();
        return;
    }
    applyManagedObjectsSnapshot(parseManagedObjects(reply));
}

QVariant BluetoothManager::getAdapterProperty(const QString& property)
{
    QDBusMessage message = propertiesMessage(adapterPath_, QStringLiteral("Get"));
    message << QStringLiteral("org.bluez.Adapter1") << property;
    const QDBusMessage reply = bus_.call(message, QDBus::Block, kDbusTimeoutMs);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
        return reply.arguments().first().value<QDBusVariant>().variant();
    qCWarning(lcBT) << "Failed to get" << property << ":" << reply.errorMessage();
    return {};
}

void BluetoothManager::setAdapterProperty(const QString& property, const QVariant& value)
{
    QDBusMessage message = propertiesMessage(adapterPath_, QStringLiteral("Set"));
    message << QStringLiteral("org.bluez.Adapter1") << property
            << QVariant::fromValue(QDBusVariant(value));
    const QDBusMessage reply = bus_.call(message, QDBus::Block, kDbusTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
        qCWarning(lcBT) << "Failed to set" << property << ":" << reply.errorMessage();
}

void BluetoothManager::setupAdapter()
{
    if (adapterPath_.isEmpty()) {
        qCWarning(lcBT) << "No BlueZ adapter found";
        return;
    }

    // The ObjectManager snapshot normally carries Address. Keep the bounded
    // fallback for older BlueZ implementations that omit it.
    if (adapterAddress_.isEmpty()) {
        const QString address = getAdapterProperty("Address").toString();
        if (!address.isEmpty()) {
            adapterAddress_ = address;
            emit adapterAddressChanged();
        }
    }

    // Read alias from config, fallback to "OpenAutoProdigy"
    QString alias = configService_
        ? configService_->value("connection.bt_name").toString()
        : QString();
    if (alias.isEmpty())
        alias = QStringLiteral("OpenAutoProdigy");

    // Power on
    setAdapterProperty("Powered", true);

    // Set alias
    setAdapterProperty("Alias", alias);
    if (adapterAlias_ != alias) {
        adapterAlias_ = alias;
        emit adapterAliasChanged();
    }

    // Make discoverable (no timeout)
    setAdapterProperty("DiscoverableTimeout", QVariant::fromValue(quint32(0)));
    setAdapterProperty("Discoverable", true);
    if (!discoverable_) {
        discoverable_ = true;
        emit discoverableChanged();
    }

    // Pairable timeout but not pairable by default
    setAdapterProperty("PairableTimeout", QVariant::fromValue(quint32(120)));
    setAdapterProperty("Pairable", false);
    if (pairable_) {
        pairable_ = false;
        emit pairableChanged();
    }

    qCInfo(lcBT) << "Adapter:" << adapterAddress_
            << "alias:" << adapterAlias_
            << "discoverable:" << discoverable_;

    configuredAdapterPath_ = adapterPath_;
}

void BluetoothManager::registerAgent()
{
    if (adapterPath_.isEmpty()) return;

    if (!agentAdaptor_) {
        agentAdaptor_ = new BluezAgentAdaptor(this, this);
        if (!bus_.registerObject(QStringLiteral("/org/openauto/agent"), agentAdaptor_,
                                 QDBusConnection::ExportAllSlots)) {
            qCWarning(lcBT) << "Failed to export Agent1 object:" << bus_.lastError();
            agentAdaptor_->deleteLater();
            agentAdaptor_ = nullptr;
            return;
        }
    }

    const QDBusObjectPath agentPath(QStringLiteral("/org/openauto/agent"));
    QDBusMessage message = QDBusMessage::createMethodCall(
        kBluezService, QStringLiteral("/org/bluez"),
        QStringLiteral("org.bluez.AgentManager1"), QStringLiteral("RegisterAgent"));
    message << QVariant::fromValue(agentPath) << QStringLiteral("DisplayYesNo");
    QDBusMessage reply = bus_.call(message, QDBus::Block, kDbusTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qCWarning(lcBT) << "RegisterAgent failed:" << reply.errorMessage();
        return;
    }

    message = QDBusMessage::createMethodCall(
        kBluezService, QStringLiteral("/org/bluez"),
        QStringLiteral("org.bluez.AgentManager1"), QStringLiteral("RequestDefaultAgent"));
    message << QVariant::fromValue(agentPath);
    reply = bus_.call(message, QDBus::Block, kDbusTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
        qCWarning(lcBT) << "RequestDefaultAgent failed:" << reply.errorMessage();
    else
        qCInfo(lcBT) << "Registered as default agent";
}

void BluetoothManager::unregisterAgent()
{
    if (!agentAdaptor_) return;

    QDBusMessage message = QDBusMessage::createMethodCall(
        kBluezService, QStringLiteral("/org/bluez"),
        QStringLiteral("org.bluez.AgentManager1"), QStringLiteral("UnregisterAgent"));
    message << QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/openauto/agent")));
    const QDBusMessage reply = bus_.call(message, QDBus::Block, kDbusTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
        qCDebug(lcBT) << "UnregisterAgent failed:" << reply.errorMessage();

    bus_.unregisterObject(QStringLiteral("/org/openauto/agent"));
    agentAdaptor_->deleteLater();
    agentAdaptor_ = nullptr;
    qCInfo(lcBT) << "Agent unregistered";
}

void BluetoothManager::handleAgentRequestConfirmation(const QDBusMessage& msg, const QString& devicePath, uint passkey)
{
    const QString deviceName = deviceNameFromPath(devicePath);
    const QString formattedPasskey =
        QStringLiteral("%1").arg(passkey, 6, 10, QChar('0'));
    const bool observableChanged = !pairingActive_
        || pairingDeviceName_ != deviceName
        || pairingPasskey_ != formattedPasskey
        || pairingPromptMode_ != PairingPromptMode::Confirmation;
    pendingPairingMessage_ = msg;
    pendingPairingDevicePath_ = devicePath;
    pairingDeviceName_ = deviceName;
    pairingPasskey_ = formattedPasskey;
    pairingEntered_ = -1;
    pairingPromptMode_ = PairingPromptMode::Confirmation;
    pairingActive_ = true;
    if (observableChanged)
        emit pairingActiveChanged();
}

void BluetoothManager::handleAgentRequestAuthorization(
    const QDBusMessage& msg, const QString& devicePath)
{
    const QString deviceName = deviceNameFromPath(devicePath);
    const bool observableChanged = !pairingActive_
        || pairingDeviceName_ != deviceName
        || !pairingPasskey_.isEmpty()
        || pairingPromptMode_ != PairingPromptMode::Authorization;
    pendingPairingMessage_ = msg;
    pendingPairingDevicePath_ = devicePath;
    pairingDeviceName_ = deviceName;
    pairingPasskey_.clear();
    pairingEntered_ = -1;
    pairingPromptMode_ = PairingPromptMode::Authorization;
    pairingActive_ = true;
    if (observableChanged)
        emit pairingActiveChanged();
}

void BluetoothManager::handleAgentDisplayPasskey(
    const QString& devicePath, const QString& passkey, int entered)
{
    const QString deviceName = deviceNameFromPath(devicePath);
    const int boundedEntered = entered < 0 ? -1 : qBound(0, entered, passkey.size());
    const bool observableChanged = !pairingActive_
        || pairingDeviceName_ != deviceName
        || pairingPasskey_ != passkey
        || pairingEntered_ != boundedEntered
        || pairingPromptMode_ != PairingPromptMode::DisplayOnly;
    pendingPairingMessage_ = {};
    pendingPairingDevicePath_.clear();
    pairingDeviceName_ = deviceName;
    pairingPasskey_ = passkey;
    pairingEntered_ = boundedEntered;
    pairingPromptMode_ = PairingPromptMode::DisplayOnly;
    pairingActive_ = true;
    if (observableChanged)
        emit pairingActiveChanged();
}

void BluetoothManager::handleAgentCancel()
{
    if (pairingActive_) {
        clearPairingPrompt();
        qCInfo(lcBT) << "BlueZ cancelled pairing request";
    }
}

void BluetoothManager::clearPairingPrompt()
{
    if (!pairingActive_ && pairingPromptMode_ == PairingPromptMode::None)
        return;
    pairingActive_ = false;
    pairingDeviceName_.clear();
    pairingPasskey_.clear();
    pairingEntered_ = -1;
    pairingPromptMode_ = PairingPromptMode::None;
    pendingPairingMessage_ = {};
    pendingPairingDevicePath_.clear();
    emit pairingActiveChanged();
}

QString BluetoothManager::deviceNameFromPath(const QString& devicePath)
{
    const QString cachedName = deviceNamesByPath_.value(devicePath);
    if (!cachedName.isEmpty())
        return cachedName;

    // Fallback: extract MAC from path (e.g. /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF)
    QString mac = devicePath.section('/', -1);
    if (mac.startsWith(QLatin1String("dev_")))
        mac = mac.mid(4);
    mac.replace('_', ':');
    return mac;
}

void BluetoothManager::setDeviceProperty(const QString& devicePath, const QString& property, const QVariant& value)
{
    QDBusMessage message = propertiesMessage(devicePath, QStringLiteral("Set"));
    message << QStringLiteral("org.bluez.Device1") << property
            << QVariant::fromValue(QDBusVariant(value));
    const QDBusMessage reply = bus_.call(message, QDBus::Block, kDbusTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
        qCWarning(lcBT) << "Failed to set" << property << "on" << devicePath << ":" << reply.errorMessage();
}

void BluetoothManager::applyManagedObjectsSnapshot(const BluezManagedObjectMap& objects)
{
    QString nextAdapterPath;
    QVariantMap adapterProperties;
    QList<PairedDeviceInfo> devices;
    QMap<QString, QString> nextDeviceNames;

    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const auto adapter = it.value().constFind(QStringLiteral("org.bluez.Adapter1"));
        if (adapter != it.value().cend() && nextAdapterPath.isEmpty()) {
            nextAdapterPath = it.key();
            adapterProperties = adapter.value();
        }

        const auto device = it.value().constFind(QStringLiteral("org.bluez.Device1"));
        if (device == it.value().cend())
            continue;
        const QVariantMap& properties = device.value();
        const QString address = properties.value(QStringLiteral("Address")).toString();
        QString name = properties.value(QStringLiteral("Name")).toString();
        if (name.isEmpty())
            name = properties.value(QStringLiteral("Alias")).toString();
        if (name.isEmpty())
            name = address;
        nextDeviceNames.insert(it.key(), name);
        if (properties.value(QStringLiteral("Paired")).toBool()) {
            devices.append({address, name,
                            properties.value(QStringLiteral("Connected")).toBool()});
        }
    }

    const bool hadAdapter = !adapterPath_.isEmpty();
    adapterPath_ = nextAdapterPath;
    deviceNamesByPath_ = nextDeviceNames;
    if (!adapterPath_.isEmpty()) {
        const QString address = adapterProperties.value(QStringLiteral("Address")).toString();
        // ObjectManager may omit a property from an otherwise complete
        // interface map. Preserve the last known address until the adapter
        // itself disappears; setupAdapter() owns the bounded initial fallback.
        if (!address.isEmpty() && adapterAddress_ != address) {
            adapterAddress_ = address;
            emit adapterAddressChanged();
        }
        const bool snapshotPairable =
            adapterProperties.value(QStringLiteral("Pairable"), pairable_).toBool();
        if (pairable_ != snapshotPairable) {
            pairable_ = snapshotPairable;
            emit pairableChanged();
        }
        if (configuredAdapterPath_ != adapterPath_) {
            setupAdapter();
            registerAgent();
        }
    } else if (hadAdapter) {
        configuredAdapterPath_.clear();
        if (!adapterAddress_.isEmpty()) {
            adapterAddress_.clear();
            emit adapterAddressChanged();
        }
        if (discoverable_) {
            discoverable_ = false;
            emit discoverableChanged();
        }
        if (pairable_) {
            pairable_ = false;
            emit pairableChanged();
        }
    }
    if (adapterPath_.isEmpty() && needsFirstPairing_) {
        needsFirstPairing_ = false;
        if (pairableRenewTimer_)
            pairableRenewTimer_->stop();
        emit needsFirstPairingChanged();
    }

    pairedDevicesModel_->setDevices(devices);
    if (devices.size() != lastPairedCount_) {
        qCInfo(lcBT) << "Found" << devices.size() << "paired device(s)";
        lastPairedCount_ = devices.size();
    }

    updateConnectedDevice();

    if (needsFirstPairing_ && !devices.isEmpty()) {
        needsFirstPairing_ = false;
        if (pairableRenewTimer_)
            pairableRenewTimer_->stop();
        emit needsFirstPairingChanged();
    }

    if (!initialSnapshotApplied_ && !adapterPath_.isEmpty()) {
        initialSnapshotApplied_ = true;
        if (devices.isEmpty())
            checkFirstRunPairing();
        else if (connectedDeviceAddress_.isEmpty())
            startAutoConnect();
    }
}

void BluetoothManager::updateConnectedDevice()
{
    // Scan paired devices for any that are connected
    for (int i = 0; i < pairedDevicesModel_->rowCount(); ++i) {
        QModelIndex idx = pairedDevicesModel_->index(i, 0);
        bool connected = pairedDevicesModel_->data(idx, PairedDevicesModel::ConnectedRole).toBool();
        if (connected) {
            QString name = pairedDevicesModel_->data(idx, PairedDevicesModel::NameRole).toString();
            QString addr = pairedDevicesModel_->data(idx, PairedDevicesModel::AddressRole).toString();
            if (connectedDeviceName_ != name || connectedDeviceAddress_ != addr) {
                connectedDeviceName_ = name;
                connectedDeviceAddress_ = addr;
                emit connectedDeviceChanged();
                qCInfo(lcBT) << "Device connected:" << name << addr;
            }
            // Stop auto-connect on any successful device connection
            if (autoConnectTimer_ && autoConnectAttempt_ < MAX_ATTEMPTS) {
                qCDebug(lcBT) << "Device connected — stopping auto-connect";
                cancelAutoConnect();
            }
            return;
        }
    }
    // No connected device found
    if (!connectedDeviceName_.isEmpty() || !connectedDeviceAddress_.isEmpty()) {
        qCInfo(lcBT) << "Device disconnected:" << connectedDeviceAddress_;
        connectedDeviceName_.clear();
        connectedDeviceAddress_.clear();
        emit connectedDeviceChanged();
    }
}

void BluetoothManager::onDevicePropertiesChanged(const QString& interface,
    const QVariantMap& changed, const QStringList& /*invalidated*/)
{
    if (interface == QLatin1String("org.bluez.Device1")) {
        requestManagedObjectsRefresh();
    }

    // Track adapter pairable state (BlueZ auto-toggles off after PairableTimeout)
    if (interface == QLatin1String("org.bluez.Adapter1") && changed.contains("Pairable")) {
        bool newPairable = changed.value("Pairable").toBool();
        if (pairable_ != newPairable) {
            pairable_ = newPairable;
            emit pairableChanged();
            qCDebug(lcBT) << "Adapter pairable changed to:" << pairable_;
        }
        // Re-enable pairable if BlueZ timeout killed it during first-run
        if (needsFirstPairing_ && !newPairable) {
            QTimer::singleShot(1000, this, [this]() {
                if (needsFirstPairing_) {
                    qCDebug(lcBT) << "Re-enabling pairable after BlueZ timeout (first-run)";
                    setAdapterProperty("Pairable", true);
                    if (!pairable_) {
                        pairable_ = true;
                        emit pairableChanged();
                    }
                }
            });
        }
    }
}

void BluetoothManager::onInterfacesChanged()
{
    requestManagedObjectsRefresh();
}

void BluetoothManager::shutdown()
{
    if (shutdown_) return;
    shutdown_ = true;
    qCInfo(lcBT) << "Shutting down";
    if (pairableRenewTimer_)
        pairableRenewTimer_->stop();
    clearPairingPrompt();
    cancelAutoConnect();
    // Disconnect each subscription even when initialize() reported a partial
    // failure; any individual successful connection must still be torn down.
    bus_.disconnect(kBluezService, QString(), kPropertiesInterface,
                    QStringLiteral("PropertiesChanged"), this,
                    SLOT(onDevicePropertiesChanged(QString,QVariantMap,QStringList)));
    bus_.disconnect(kBluezService, QString(), kObjectManagerInterface,
                    QStringLiteral("InterfacesAdded"), this, SLOT(onInterfacesChanged()));
    bus_.disconnect(kBluezService, QString(), kObjectManagerInterface,
                    QStringLiteral("InterfacesRemoved"), this, SLOT(onInterfacesChanged()));
    subscriptionsConnected_ = false;
    unregisterAgent();
}

} // namespace oap

#include "BluetoothManager.moc"
