#include "BluetoothManager.hpp"
#include "IConfigService.hpp"
#include <QDebug>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusServiceWatcher>

namespace oap {

BluetoothManager::BluetoothManager(IConfigService* configService, QObject* parent)
    : QObject(parent)
    , configService_(configService)
{
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

bool BluetoothManager::isPairingActive() const { return pairingActive_; }
QString BluetoothManager::pairingDeviceName() const { return pairingDeviceName_; }
QString BluetoothManager::pairingPasskey() const { return pairingPasskey_; }

void BluetoothManager::confirmPairing()
{
    qInfo() << "[BtManager] Pairing confirmed by user";
    // TODO: reply to pending Agent1 D-Bus method call
}

void BluetoothManager::rejectPairing()
{
    qInfo() << "[BtManager] Pairing rejected by user";
    // TODO: reply with rejection to pending Agent1 D-Bus method call
}

QAbstractListModel* BluetoothManager::pairedDevicesModel()
{
    return nullptr;  // TODO: return PairedDevicesModel
}

void BluetoothManager::forgetDevice(const QString& address)
{
    qInfo() << "[BtManager] Forget device:" << address;
    // TODO: call Adapter1.RemoveDevice via D-Bus
}

void BluetoothManager::startAutoConnect()
{
    qInfo() << "[BtManager] Starting auto-connect";
    // TODO: enumerate paired devices, start retry loop
}

void BluetoothManager::cancelAutoConnect()
{
    qInfo() << "[BtManager] Cancelling auto-connect";
    // TODO: stop retry timer
}

QString BluetoothManager::connectedDeviceName() const { return connectedDeviceName_; }
QString BluetoothManager::connectedDeviceAddress() const { return connectedDeviceAddress_; }

void BluetoothManager::initialize()
{
    qInfo() << "[BtManager] Initializing...";
    setupAdapter();

    auto* watcher = new QDBusServiceWatcher("org.bluez",
        QDBusConnection::systemBus(),
        QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this]() {
        qInfo() << "[BtManager] BlueZ restarted — re-initializing adapter";
        setupAdapter();
    });
    // TODO: agent registration, profile registration, auto-connect
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
}

void BluetoothManager::shutdown()
{
    qInfo() << "[BtManager] Shutting down";
    cancelAutoConnect();
    // TODO: unregister agent, profiles
}

} // namespace oap
