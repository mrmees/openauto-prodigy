#include "BluetoothManager.hpp"
#include "IConfigService.hpp"
#include "ui/PairedDevicesModel.hpp"
#include <QDebug>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusContext>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusAbstractAdaptor>
#include <QDBusUnixFileDescriptor>
#include <unistd.h>

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
        qInfo() << "[BtAgent] Released";
    }

    void RequestConfirmation(const QDBusObjectPath& device, uint passkey) {
        qInfo() << "[BtAgent] RequestConfirmation:" << device.path() << passkey;
        setDelayedReply(true);
        manager_->handleAgentRequestConfirmation(message(), device.path(), passkey);
    }

    void AuthorizeService(const QDBusObjectPath& device, const QString& uuid) {
        qInfo() << "[BtAgent] AuthorizeService:" << device.path() << uuid;
        // Auto-accept all services from paired devices (no delayed reply needed)
    }

    void Cancel() {
        qInfo() << "[BtAgent] Cancel";
        manager_->handleAgentCancel();
    }

private:
    oap::BluetoothManager* manager_;
};

// D-Bus adaptor implementing org.bluez.Profile1 — holds NewConnection fds
// and notifies BluetoothManager when a profile connection arrives.
class BluezProfile1Handler : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Profile1")

public:
    explicit BluezProfile1Handler(QObject* parent, std::vector<int>& fdStore,
                                   oap::BluetoothManager* manager)
        : QDBusAbstractAdaptor(parent), fdStore_(fdStore), manager_(manager) {}

public slots:
    void NewConnection(const QDBusObjectPath& device,
                       const QDBusUnixFileDescriptor& fd,
                       const QVariantMap& /*properties*/)
    {
        if (fd.isValid()) {
            int dupFd = ::dup(fd.fileDescriptor());
            if (dupFd >= 0) {
                fdStore_.push_back(dupFd);
                qInfo() << "[BtManager] Profile NewConnection from"
                         << device.path() << "— holding fd" << dupFd;
            }
        }
        if (manager_)
            emit manager_->profileNewConnection();
    }

    void RequestDisconnection(const QDBusObjectPath& device)
    {
        qInfo() << "[BtManager] Profile RequestDisconnection:" << device.path();
    }

    void Release()
    {
        qInfo() << "[BtManager] Profile released";
    }

private:
    std::vector<int>& fdStore_;
    oap::BluetoothManager* manager_;
};

namespace oap {

BluetoothManager::BluetoothManager(IConfigService* configService, QObject* parent)
    : QObject(parent)
    , configService_(configService)
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
    qInfo() << "[BtManager] Pairable:" << enabled;
    if (!adapterPath_.isEmpty())
        setAdapterProperty("Pairable", enabled);
}

bool BluetoothManager::needsFirstPairing() const { return needsFirstPairing_; }
bool BluetoothManager::isPairingActive() const { return pairingActive_; }
QString BluetoothManager::pairingDeviceName() const { return pairingDeviceName_; }
QString BluetoothManager::pairingPasskey() const { return pairingPasskey_; }

void BluetoothManager::confirmPairing()
{
    if (!pairingActive_) return;
    qInfo() << "[BtManager] Pairing confirmed by user";

    auto reply = pendingPairingMessage_.createReply();
    QDBusConnection::systemBus().send(reply);

    // Trust the device so future connections auto-accept
    setDeviceProperty(pendingPairingDevicePath_, "Trusted", true);

    pairingActive_ = false;
    pairingDeviceName_.clear();
    pairingPasskey_.clear();
    pendingPairingDevicePath_.clear();
    emit pairingActiveChanged();

    refreshPairedDevices();

    // Clear first-run banner if we now have paired devices
    if (needsFirstPairing_ && pairedDevicesModel_->rowCount() > 0) {
        qInfo() << "[BtManager] First device paired — clearing first-run state";
        needsFirstPairing_ = false;
        emit needsFirstPairingChanged();
        if (pairableRenewTimer_)
            pairableRenewTimer_->stop();
    }
}

void BluetoothManager::rejectPairing()
{
    if (!pairingActive_) return;
    qInfo() << "[BtManager] Pairing rejected by user";

    auto reply = pendingPairingMessage_.createErrorReply(
        QStringLiteral("org.bluez.Error.Rejected"),
        QStringLiteral("User rejected pairing"));
    QDBusConnection::systemBus().send(reply);

    pairingActive_ = false;
    pairingDeviceName_.clear();
    pairingPasskey_.clear();
    pendingPairingDevicePath_.clear();
    emit pairingActiveChanged();
}

QAbstractListModel* BluetoothManager::pairedDevicesModel()
{
    return pairedDevicesModel_;
}

void BluetoothManager::forgetDevice(const QString& address)
{
    if (adapterPath_.isEmpty()) return;
    qInfo() << "[BtManager] Forget device:" << address;

    // Device paths are like /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF
    QString devAddr = address;
    devAddr.replace(':', '_');
    QString devicePath = adapterPath_ + "/dev_" + devAddr;

    QDBusInterface adapter("org.bluez", adapterPath_,
        "org.bluez.Adapter1", QDBusConnection::systemBus());
    QDBusReply<void> reply = adapter.call("RemoveDevice",
        QVariant::fromValue(QDBusObjectPath(devicePath)));
    if (!reply.isValid())
        qWarning() << "[BtManager] RemoveDevice failed:" << reply.error().message();

    refreshPairedDevices();
}

void BluetoothManager::startAutoConnect()
{
    if (adapterPath_.isEmpty()) return;

    // Check config
    if (configService_) {
        QVariant enabled = configService_->value("connection.auto_connect_aa");
        if (enabled.isValid() && !enabled.toBool()) {
            qInfo() << "[BtManager] Auto-connect disabled in config";
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
        qInfo() << "[BtManager] No paired devices — skipping auto-connect";
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

    qInfo() << "[BtManager] Starting auto-connect for" << pairedDevicePaths_.size() << "device(s)";
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
    qInfo() << "[BtManager] Auto-connect cancelled";
}

void BluetoothManager::attemptConnect()
{
    if (autoConnectInFlight_) return;
    if (autoConnectAttempt_ >= MAX_ATTEMPTS || pairedDevicePaths_.isEmpty()) {
        qInfo() << "[BtManager] Auto-connect exhausted after" << autoConnectAttempt_ << "attempts";
        return;
    }

    QString devicePath = pairedDevicePaths_[autoConnectDeviceIndex_ % pairedDevicePaths_.size()];
    autoConnectDeviceIndex_++;
    autoConnectInFlight_ = true;

    qInfo() << "[BtManager] Auto-connect attempt" << (autoConnectAttempt_ + 1)
            << "/" << MAX_ATTEMPTS << "→" << devicePath;

    // Async D-Bus call: Device1.Connect()
    QDBusInterface device("org.bluez", devicePath,
        "org.bluez.Device1", QDBusConnection::systemBus());

    QDBusPendingCall pending = device.asyncCall("Connect");
    auto* watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        watcher->deleteLater();
        autoConnectInFlight_ = false;

        if (watcher->isError()) {
            qInfo() << "[BtManager] Connect failed:" << watcher->error().message();
        } else {
            qInfo() << "[BtManager] Connect call returned success";
            // Don't cancel yet — wait for profileNewConnection (RFCOMM) as the true success signal
        }

        autoConnectAttempt_++;
        int interval = nextRetryInterval();
        if (interval > 0 && autoConnectTimer_) {
            autoConnectTimer_->start(interval);
        } else {
            qInfo() << "[BtManager] Auto-connect schedule exhausted";
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

    qInfo() << "[BtManager] No paired devices — enabling first-run pairable mode";
    needsFirstPairing_ = true;
    emit needsFirstPairingChanged();

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
            qInfo() << "[BtManager] Renewing pairable mode for first-run";
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
    qInfo() << "[BtManager] First-run banner dismissed by user";
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
    qInfo() << "[BtManager] Initializing...";
    setupAdapter();
    registerAgent();
    registerProfiles();

    // Watch for BlueZ device/adapter property changes
    QDBusConnection::systemBus().connect(
        "org.bluez", QString(), "org.freedesktop.DBus.Properties", "PropertiesChanged",
        this, SLOT(onDevicePropertiesChanged(QString,QVariantMap,QStringList)));

    // Watch for new/removed devices (paired externally, removed via bluetoothctl, etc.)
    QDBusConnection::systemBus().connect(
        "org.bluez", QString(), "org.freedesktop.DBus.ObjectManager", "InterfacesAdded",
        this, SLOT(onInterfacesChanged()));
    QDBusConnection::systemBus().connect(
        "org.bluez", QString(), "org.freedesktop.DBus.ObjectManager", "InterfacesRemoved",
        this, SLOT(onInterfacesChanged()));

    auto* watcher = new QDBusServiceWatcher("org.bluez",
        QDBusConnection::systemBus(),
        QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this]() {
        qInfo() << "[BtManager] BlueZ restarted — re-initializing";
        setupAdapter();
        registerAgent();
        registerProfiles();
    });
    // Cancel auto-connect when RFCOMM connection arrives
    connect(this, &BluetoothManager::profileNewConnection,
            this, &BluetoothManager::cancelAutoConnect);

    startAutoConnect();
    checkFirstRunPairing();
}

QString BluetoothManager::findAdapterPath()
{
    QDBusInterface objectManager("org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", QDBusConnection::systemBus());

    QDBusMessage reply = objectManager.call("GetManagedObjects");
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};

    // Result is a{oa{sa{sv}}} — object path → interface → properties
    // All nested maps are in the same QDBusArgument stream — just keep calling beginMap/endMap
    const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath objPath;
        arg >> objPath;

        // Iterate interfaces map: a{sa{sv}}
        bool hasAdapter = false;
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QString ifaceName;
            arg >> ifaceName;
            // Skip the properties sub-map: a{sv}
            arg.beginMap();
            while (!arg.atEnd()) {
                arg.beginMapEntry();
                QString key;
                arg >> key;
                QDBusVariant val;
                arg >> val;
                arg.endMapEntry();
            }
            arg.endMap();
            if (ifaceName == "org.bluez.Adapter1")
                hasAdapter = true;
            arg.endMapEntry();
        }
        arg.endMap();

        arg.endMapEntry();

        if (hasAdapter)
            return objPath.path();
    }
    arg.endMap();
    return {};
}

QVariant BluetoothManager::getAdapterProperty(const QString& property)
{
    QDBusInterface props("org.bluez", adapterPath_,
        "org.freedesktop.DBus.Properties", QDBusConnection::systemBus());
    QDBusReply<QDBusVariant> reply = props.call("Get", "org.bluez.Adapter1", property);
    if (reply.isValid())
        return reply.value().variant();
    qWarning() << "[BtManager] Failed to get" << property << ":" << reply.error().message();
    return {};
}

void BluetoothManager::setAdapterProperty(const QString& property, const QVariant& value)
{
    QDBusInterface props("org.bluez", adapterPath_,
        "org.freedesktop.DBus.Properties", QDBusConnection::systemBus());
    QDBusMessage reply = props.call("Set", "org.bluez.Adapter1", property,
        QVariant::fromValue(QDBusVariant(value)));
    if (reply.type() == QDBusMessage::ErrorMessage)
        qWarning() << "[BtManager] Failed to set" << property << ":" << reply.errorMessage();
}

void BluetoothManager::setupAdapter()
{
    adapterPath_ = findAdapterPath();
    if (adapterPath_.isEmpty()) {
        qWarning() << "[BtManager] No BlueZ adapter found";
        return;
    }

    // Read adapter address
    adapterAddress_ = getAdapterProperty("Address").toString();

    // Read alias from config, fallback to "OpenAutoProdigy"
    QString alias = configService_->value("connection.bt_name").toString();
    if (alias.isEmpty())
        alias = QStringLiteral("OpenAutoProdigy");

    // Power on
    setAdapterProperty("Powered", true);

    // Set alias
    setAdapterProperty("Alias", alias);
    adapterAlias_ = alias;
    emit adapterAliasChanged();

    // Make discoverable (no timeout)
    setAdapterProperty("DiscoverableTimeout", QVariant::fromValue(quint32(0)));
    setAdapterProperty("Discoverable", true);
    discoverable_ = true;
    emit discoverableChanged();

    // Pairable timeout but not pairable by default
    setAdapterProperty("PairableTimeout", QVariant::fromValue(quint32(120)));
    setAdapterProperty("Pairable", false);
    pairable_ = false;

    qInfo() << "[BtManager] Adapter:" << adapterAddress_
            << "alias:" << adapterAlias_
            << "discoverable:" << discoverable_;

    refreshPairedDevices();
}

void BluetoothManager::registerAgent()
{
    if (adapterPath_.isEmpty()) return;

    if (!agentAdaptor_) {
        agentAdaptor_ = new BluezAgentAdaptor(this, this);
        QDBusConnection::systemBus().registerObject(
            QStringLiteral("/org/openauto/agent"), agentAdaptor_,
            QDBusConnection::ExportAllSlots);
    }

    QDBusInterface agentMgr("org.bluez", "/org/bluez",
        "org.bluez.AgentManager1", QDBusConnection::systemBus());

    QDBusReply<void> reply = agentMgr.call("RegisterAgent",
        QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/openauto/agent"))),
        QStringLiteral("DisplayYesNo"));
    if (!reply.isValid())
        qWarning() << "[BtManager] RegisterAgent failed:" << reply.error().message();

    reply = agentMgr.call("RequestDefaultAgent",
        QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/openauto/agent"))));
    if (!reply.isValid())
        qWarning() << "[BtManager] RequestDefaultAgent failed:" << reply.error().message();
    else
        qInfo() << "[BtManager] Registered as default agent";
}

void BluetoothManager::unregisterAgent()
{
    if (!agentAdaptor_) return;

    QDBusInterface agentMgr("org.bluez", "/org/bluez",
        "org.bluez.AgentManager1", QDBusConnection::systemBus());
    agentMgr.call("UnregisterAgent",
        QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/openauto/agent"))));

    QDBusConnection::systemBus().unregisterObject(QStringLiteral("/org/openauto/agent"));
    qInfo() << "[BtManager] Agent unregistered";
}

void BluetoothManager::registerProfiles()
{
    if (adapterPath_.isEmpty()) return;

    struct ProfileInfo {
        const char* uuid;
        const char* path;
        const char* name;
    };

    static const ProfileInfo profiles[] = {
        {"0000111f-0000-1000-8000-00805f9b34fb", "/org/openauto/bt/hfp_ag", "HFP AG"},
        {"00001108-0000-1000-8000-00805f9b34fb", "/org/openauto/bt/hsp_hs", "HSP HS"},
    };

    auto bus = QDBusConnection::systemBus();

    for (const auto& prof : profiles) {
        auto obj = std::make_unique<QObject>();
        new BluezProfile1Handler(obj.get(), profileFds_, this);

        if (!bus.registerObject(prof.path, obj.get(), QDBusConnection::ExportAdaptors)) {
            qWarning() << "[BtManager] Failed to register D-Bus object at" << prof.path;
            continue;
        }
        profileObjects_.push_back(std::move(obj));

        QVariantMap options;
        options["Role"] = QVariant::fromValue(QDBusVariant(QString("server")));
        options["RequireAuthentication"] = QVariant::fromValue(QDBusVariant(false));
        options["RequireAuthorization"] = QVariant::fromValue(QDBusVariant(false));

        QDBusMessage call = QDBusMessage::createMethodCall(
            "org.bluez", "/org/bluez", "org.bluez.ProfileManager1", "RegisterProfile");
        call << QVariant::fromValue(QDBusObjectPath(prof.path))
             << QString(prof.uuid)
             << options;

        QDBusMessage reply = bus.call(call, QDBus::Block, 5000);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "[BtManager] Failed to register" << prof.name << ":" << reply.errorMessage();
        } else {
            qInfo() << "[BtManager] Registered" << prof.name << "profile";
            registeredProfilePaths_.append(prof.path);
        }
    }
}

void BluetoothManager::unregisterProfiles()
{
    auto bus = QDBusConnection::systemBus();
    for (const auto& path : registeredProfilePaths_) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            "org.bluez", "/org/bluez", "org.bluez.ProfileManager1", "UnregisterProfile");
        call << QVariant::fromValue(QDBusObjectPath(path));
        bus.call(call, QDBus::Block, 2000);
        bus.unregisterObject(path);
    }
    registeredProfilePaths_.clear();
    profileObjects_.clear();
    for (int fd : profileFds_)
        ::close(fd);
    profileFds_.clear();
}

void BluetoothManager::handleAgentRequestConfirmation(const QDBusMessage& msg, const QString& devicePath, uint passkey)
{
    pendingPairingMessage_ = msg;
    pendingPairingDevicePath_ = devicePath;
    pairingDeviceName_ = deviceNameFromPath(devicePath);
    pairingPasskey_ = QStringLiteral("%1").arg(passkey, 6, 10, QChar('0'));
    pairingActive_ = true;
    emit pairingActiveChanged();
}

void BluetoothManager::handleAgentCancel()
{
    if (pairingActive_) {
        pairingActive_ = false;
        pairingDeviceName_.clear();
        pairingPasskey_.clear();
        pendingPairingDevicePath_.clear();
        emit pairingActiveChanged();
        qInfo() << "[BtManager] BlueZ cancelled pairing request";
    }
}

QString BluetoothManager::deviceNameFromPath(const QString& devicePath)
{
    QDBusInterface props("org.bluez", devicePath,
        "org.freedesktop.DBus.Properties", QDBusConnection::systemBus());
    QDBusReply<QDBusVariant> reply = props.call("Get", "org.bluez.Device1", "Name");
    if (reply.isValid())
        return reply.value().variant().toString();

    // Fallback: extract MAC from path (e.g. /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF)
    QString mac = devicePath.section('/', -1);
    if (mac.startsWith(QLatin1String("dev_")))
        mac = mac.mid(4);
    mac.replace('_', ':');
    return mac;
}

void BluetoothManager::setDeviceProperty(const QString& devicePath, const QString& property, const QVariant& value)
{
    QDBusInterface props("org.bluez", devicePath,
        "org.freedesktop.DBus.Properties", QDBusConnection::systemBus());
    QDBusMessage reply = props.call("Set", "org.bluez.Device1", property,
        QVariant::fromValue(QDBusVariant(value)));
    if (reply.type() == QDBusMessage::ErrorMessage)
        qWarning() << "[BtManager] Failed to set" << property << "on" << devicePath << ":" << reply.errorMessage();
}

void BluetoothManager::refreshPairedDevices()
{
    QDBusInterface objectManager("org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", QDBusConnection::systemBus());

    QDBusMessage reply = objectManager.call("GetManagedObjects");
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return;

    QList<PairedDeviceInfo> devices;
    const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();

    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath objPath;
        arg >> objPath;

        // Parse interfaces map
        bool hasDevice = false;
        bool paired = false;
        bool connected = false;
        QString address, name;

        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QString ifaceName;
            arg >> ifaceName;

            // Parse properties sub-map
            arg.beginMap();
            while (!arg.atEnd()) {
                arg.beginMapEntry();
                QString key;
                arg >> key;
                QDBusVariant val;
                arg >> val;

                if (ifaceName == "org.bluez.Device1") {
                    hasDevice = true;
                    if (key == "Address") address = val.variant().toString();
                    else if (key == "Name") name = val.variant().toString();
                    else if (key == "Alias" && name.isEmpty()) name = val.variant().toString();
                    else if (key == "Paired") paired = val.variant().toBool();
                    else if (key == "Connected") connected = val.variant().toBool();
                }
                arg.endMapEntry();
            }
            arg.endMap();
            arg.endMapEntry();
        }
        arg.endMap();
        arg.endMapEntry();

        if (hasDevice && paired) {
            devices.append({address, name.isEmpty() ? address : name, connected});
        }
    }
    arg.endMap();

    pairedDevicesModel_->setDevices(devices);
    qInfo() << "[BtManager] Found" << devices.size() << "paired device(s)";
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
                qInfo() << "[BtManager] Device connected:" << name << addr;
            }
            // Stop auto-connect on any successful device connection
            if (autoConnectTimer_ && autoConnectAttempt_ < MAX_ATTEMPTS) {
                qInfo() << "[BtManager] Device connected — stopping auto-connect";
                cancelAutoConnect();
            }
            return;
        }
    }
    // No connected device found
    if (!connectedDeviceName_.isEmpty()) {
        qInfo() << "[BtManager] Device disconnected:" << connectedDeviceAddress_;
        connectedDeviceName_.clear();
        connectedDeviceAddress_.clear();
        emit connectedDeviceChanged();
    }
}

void BluetoothManager::onDevicePropertiesChanged(const QString& interface,
    const QVariantMap& changed, const QStringList& /*invalidated*/)
{
    if (interface == QLatin1String("org.bluez.Device1")) {
        refreshPairedDevices();
        updateConnectedDevice();
    }

    // Track adapter pairable state (BlueZ auto-toggles off after PairableTimeout)
    if (interface == QLatin1String("org.bluez.Adapter1") && changed.contains("Pairable")) {
        bool newPairable = changed.value("Pairable").toBool();
        if (pairable_ != newPairable) {
            pairable_ = newPairable;
            emit pairableChanged();
            qInfo() << "[BtManager] Adapter pairable changed to:" << pairable_;
        }
        // Re-enable pairable if BlueZ timeout killed it during first-run
        if (needsFirstPairing_ && !newPairable) {
            QTimer::singleShot(1000, this, [this]() {
                if (needsFirstPairing_) {
                    qInfo() << "[BtManager] Re-enabling pairable after BlueZ timeout (first-run)";
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
    refreshPairedDevices();
}

void BluetoothManager::shutdown()
{
    if (shutdown_) return;
    shutdown_ = true;
    qInfo() << "[BtManager] Shutting down";
    if (pairableRenewTimer_)
        pairableRenewTimer_->stop();
    cancelAutoConnect();
    unregisterProfiles();
    unregisterAgent();
}

} // namespace oap

#include "BluetoothManager.moc"
