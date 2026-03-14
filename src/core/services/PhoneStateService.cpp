#include "PhoneStateService.hpp"
#include "INotificationService.hpp"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusServiceWatcher>

namespace oap {

PhoneStateService::PhoneStateService(QObject* parent)
    : IPhoneStateService(parent)
{
    callTimer_.setInterval(1000);
    connect(&callTimer_, &QTimer::timeout, this, &PhoneStateService::updateCallDuration);
}

PhoneStateService::~PhoneStateService()
{
    stopDBusMonitoring();
}

int PhoneStateService::callState() const {
    return static_cast<int>(callState_);
}

QString PhoneStateService::callerName() const {
    return callerName_;
}

QString PhoneStateService::callerNumber() const {
    return callerNumber_;
}

void PhoneStateService::answer()
{
    if (callState_ != ICallStateProvider::Ringing) return;
    setCallStateInternal(ICallStateProvider::Active);
}

void PhoneStateService::hangup()
{
    if (callState_ == ICallStateProvider::Idle) return;
    callerNumber_.clear();
    callerName_.clear();
    setCallStateInternal(ICallStateProvider::Idle);
}

void PhoneStateService::setIncomingCall(const QString& number, const QString& name)
{
    callerNumber_ = number;
    callerName_ = name;
    setCallStateInternal(ICallStateProvider::Ringing);
}

void PhoneStateService::setCallStateInternal(ICallStateProvider::CallState state)
{
    if (state == callState_) return;
    callState_ = state;

    if (state == ICallStateProvider::Active) {
        callDuration_ = 0;
        callTimer_.start();
    } else {
        callTimer_.stop();
    }

    // Dismiss incoming call notification when no longer ringing
    if (state != ICallStateProvider::Ringing && !activeCallNotificationId_.isEmpty()) {
        if (notificationService_)
            notificationService_->dismiss(activeCallNotificationId_);
        activeCallNotificationId_.clear();
    }

    if (state == ICallStateProvider::Ringing && notificationService_) {
        QString displayName = callerName_.isEmpty() ? callerNumber_ : callerName_;
        activeCallNotificationId_ = notificationService_->post({
            {"kind", "incoming_call"},
            {"message", displayName},
            {"sourcePluginId", "org.openauto.phone"},
            {"priority", 90}
        });
    }

    emit callStateChanged();
}

void PhoneStateService::updateCallDuration()
{
    callDuration_++;
    emit callDurationChanged();
}

void PhoneStateService::startDBusMonitoring()
{
    if (monitoring_) return;

    auto bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) return;

    bluezWatcher_ = new QDBusServiceWatcher(
        QStringLiteral("org.bluez"), bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
        this);

    connect(bluezWatcher_, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        scanExistingDevices();
    });

    connect(bluezWatcher_, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        if (phoneConnected_) {
            phoneConnected_ = false;
            deviceName_.clear();
            devicePath_.clear();
            emit connectionChanged();
        }
        setCallStateInternal(ICallStateProvider::Idle);
    });

    bus.connect(QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap)));

    bus.connect(QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));

    bus.connect(QStringLiteral("org.bluez"), QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    monitoring_ = true;
    scanExistingDevices();
}

void PhoneStateService::stopDBusMonitoring()
{
    if (!monitoring_) return;

    auto bus = QDBusConnection::systemBus();
    bus.disconnect(QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap)));
    bus.disconnect(QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    bus.disconnect(QStringLiteral("org.bluez"), QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    delete bluezWatcher_;
    bluezWatcher_ = nullptr;
    monitoring_ = false;
}

void PhoneStateService::scanExistingDevices()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("GetManagedObjects"));

    QDBusMessage reply = QDBusConnection::systemBus().call(msg, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage) return;

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath objPath;
        arg >> objPath;
        QString path = objPath.path();

        const QDBusArgument ifacesArg = qvariant_cast<QDBusArgument>(arg.asVariant());
        ifacesArg.beginMap();
        while (!ifacesArg.atEnd()) {
            ifacesArg.beginMapEntry();
            QString iface;
            ifacesArg >> iface;

            const QDBusArgument propsArg = qvariant_cast<QDBusArgument>(ifacesArg.asVariant());
            QVariantMap props;
            propsArg.beginMap();
            while (!propsArg.atEnd()) {
                propsArg.beginMapEntry();
                QString key;
                QDBusVariant dbusVal;
                propsArg >> key >> dbusVal;
                props[key] = dbusVal.variant();
                propsArg.endMapEntry();
            }
            propsArg.endMap();
            ifacesArg.endMapEntry();

            if (iface == QLatin1String("org.bluez.Device1")) {
                bool connected = props.value(QStringLiteral("Connected")).toBool();
                QStringList uuids = props.value(QStringLiteral("UUIDs")).toStringList();

                bool hasHfp = false;
                for (const auto& uuid : uuids) {
                    if (uuid.startsWith(QLatin1String("0000111e"))
                        || uuid.startsWith(QLatin1String("0000111f"))) {
                        hasHfp = true;
                        break;
                    }
                }

                if (connected && hasHfp) {
                    devicePath_ = path;
                    deviceName_ = props.value(QStringLiteral("Alias")).toString();
                    phoneConnected_ = true;
                    emit connectionChanged();
                }
            }
        }
        ifacesArg.endMap();
        arg.endMapEntry();
    }
    arg.endMap();
}

void PhoneStateService::onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces)
{
    Q_UNUSED(interfaces)
    const QString pathStr = path.path();
    if (!pathStr.contains(QLatin1String("/dev_"))) return;

    QDBusInterface iface(QStringLiteral("org.bluez"), pathStr,
        QStringLiteral("org.bluez.Device1"), QDBusConnection::systemBus());
    if (!iface.isValid()) return;

    bool connected = iface.property("Connected").toBool();
    QStringList uuids = iface.property("UUIDs").toStringList();

    bool hasHfp = false;
    for (const auto& uuid : uuids) {
        if (uuid.startsWith(QLatin1String("0000111e"))
            || uuid.startsWith(QLatin1String("0000111f"))) {
            hasHfp = true;
            break;
        }
    }

    if (connected && hasHfp && !phoneConnected_) {
        devicePath_ = pathStr;
        deviceName_ = iface.property("Alias").toString();
        phoneConnected_ = true;
        emit connectionChanged();
    }
}

void PhoneStateService::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces)
{
    if (path.path() == devicePath_ && interfaces.contains(QStringLiteral("org.bluez.Device1"))) {
        phoneConnected_ = false;
        deviceName_.clear();
        devicePath_.clear();
        emit connectionChanged();
        setCallStateInternal(ICallStateProvider::Idle);
    }
}

void PhoneStateService::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                             const QStringList& /*invalidated*/)
{
    if (interface == QLatin1String("org.bluez.Device1")) {
        if (changed.contains(QStringLiteral("Connected"))) {
            bool connected = changed.value(QStringLiteral("Connected")).toBool();
            if (!connected && phoneConnected_) {
                phoneConnected_ = false;
                deviceName_.clear();
                emit connectionChanged();
                setCallStateInternal(ICallStateProvider::Idle);
            }
        }
    }
}

} // namespace oap
