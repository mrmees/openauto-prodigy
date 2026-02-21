#include "PhonePlugin.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/INotificationService.hpp"
#include <QQmlContext>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusServiceWatcher>
#include <QTimer>

namespace oap {
namespace plugins {

PhonePlugin::PhonePlugin(QObject* parent)
    : QObject(parent)
{
}

PhonePlugin::~PhonePlugin()
{
    shutdown();
}

bool PhonePlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    // Call duration timer
    callTimer_ = new QTimer(this);
    callTimer_->setInterval(1000);
    connect(callTimer_, &QTimer::timeout, this, &PhonePlugin::updateCallDuration);

    startDBusMonitoring();

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Phone plugin initialized"));

    return true;
}

void PhonePlugin::shutdown()
{
    stopDBusMonitoring();
    if (callTimer_) {
        callTimer_->stop();
        callTimer_ = nullptr;
    }
}

void PhonePlugin::startDBusMonitoring()
{
    if (monitoring_) return;

    auto bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        if (hostContext_)
            hostContext_->log(LogLevel::Warning,
                QStringLiteral("Phone: Cannot connect to system D-Bus — HFP monitoring disabled"));
        return;
    }

    bluezWatcher_ = new QDBusServiceWatcher(
        QStringLiteral("org.bluez"),
        bus,
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
        setCallState(Idle);
    });

    // InterfacesAdded/Removed for device connect/disconnect
    bus.connect(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap)));

    bus.connect(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this,
        SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));

    // PropertiesChanged for device state updates
    bus.connect(
        QStringLiteral("org.bluez"),
        QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    monitoring_ = true;
    scanExistingDevices();
}

void PhonePlugin::stopDBusMonitoring()
{
    if (!monitoring_) return;

    auto bus = QDBusConnection::systemBus();
    bus.disconnect(
        QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap)));
    bus.disconnect(
        QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    bus.disconnect(
        QStringLiteral("org.bluez"), QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    delete bluezWatcher_;
    bluezWatcher_ = nullptr;
    monitoring_ = false;
}

void PhonePlugin::scanExistingDevices()
{
    // Look for connected BT devices with HandsfreeGateway UUID
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
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

            // Skip the properties — just check for Device1
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

                // Check for HFP Handsfree UUID (0000111e) or HFP AG UUID (0000111f)
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

void PhonePlugin::onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces)
{
    Q_UNUSED(interfaces)

    // Check if a Device1 appeared with HFP
    const QString pathStr = path.path();
    if (!pathStr.contains(QLatin1String("/dev_"))) return;

    QDBusInterface iface(
        QStringLiteral("org.bluez"), pathStr,
        QStringLiteral("org.bluez.Device1"),
        QDBusConnection::systemBus());
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

        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                QStringLiteral("Phone: HFP device connected: %1").arg(deviceName_));
    }
}

void PhonePlugin::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces)
{
    if (path.path() == devicePath_ && interfaces.contains(QStringLiteral("org.bluez.Device1"))) {
        phoneConnected_ = false;
        deviceName_.clear();
        devicePath_.clear();
        emit connectionChanged();
        setCallState(Idle);
    }
}

void PhonePlugin::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                       const QStringList& /*invalidated*/)
{
    if (interface == QLatin1String("org.bluez.Device1")) {
        if (changed.contains(QStringLiteral("Connected"))) {
            bool connected = changed.value(QStringLiteral("Connected")).toBool();
            if (!connected && phoneConnected_) {
                phoneConnected_ = false;
                deviceName_.clear();
                emit connectionChanged();
                setCallState(Idle);
            }
        }
    }
}

void PhonePlugin::setCallState(CallState state)
{
    if (state == callState_) return;
    callState_ = state;

    if (state == Active) {
        callDuration_ = 0;
        callTimer_->start();
    } else {
        callTimer_->stop();
    }

    // Dismiss incoming call notification when no longer ringing
    if (state != Ringing && !activeCallNotificationId_.isEmpty()) {
        if (hostContext_ && hostContext_->notificationService())
            hostContext_->notificationService()->dismiss(activeCallNotificationId_);
        activeCallNotificationId_.clear();
    }

    if (state == Ringing) {
        emit incomingCall(callerNumber_, callerName_);
        // Post notification via NotificationService
        if (hostContext_ && hostContext_->notificationService()) {
            QString displayName = callerName_.isEmpty() ? callerNumber_ : callerName_;
            activeCallNotificationId_ = hostContext_->notificationService()->post({
                {"kind", "incoming_call"},
                {"message", displayName},
                {"sourcePluginId", id()},
                {"priority", 90}
            });
        }
    }

    emit callStateChanged();
}

void PhonePlugin::updateCallDuration()
{
    callDuration_++;
    emit callDurationChanged();
}

void PhonePlugin::onActivated(QQmlContext* context)
{
    if (!context) return;
    context->setContextProperty("PhonePlugin", this);
}

void PhonePlugin::onDeactivated()
{
    // Child context destroyed by PluginRuntimeContext
}

QUrl PhonePlugin::qmlComponent() const
{
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/PhoneView.qml"));
}

QUrl PhonePlugin::iconSource() const
{
    return {};  // Font-based icons — see MaterialIcon.qml (\uf0d4 phone)
}

void PhonePlugin::dial(const QString& number)
{
    if (!phoneConnected_ || number.isEmpty()) return;

    // TODO: Initiate call via BlueZ telephony D-Bus interface
    // For now, just update state for UI testing
    callerNumber_ = number;
    callerName_.clear();
    emit callInfoChanged();
    setCallState(Dialing);

    if (hostContext_)
        hostContext_->log(LogLevel::Info,
            QStringLiteral("Phone: Dialing %1").arg(number));
}

void PhonePlugin::answer()
{
    if (callState_ != Ringing) return;
    setCallState(Active);
}

void PhonePlugin::hangup()
{
    if (callState_ == Idle) return;
    setCallState(Ended);

    // Brief delay then reset to idle
    QTimer::singleShot(1500, this, [this]() {
        callerNumber_.clear();
        callerName_.clear();
        emit callInfoChanged();
        setCallState(Idle);
    });
}

void PhonePlugin::appendDigit(const QString& digit)
{
    if (callState_ == Active) {
        sendDTMF(digit);
    } else {
        dialedNumber_ += digit;
        emit dialedNumberChanged();
    }
}

void PhonePlugin::clearDialed()
{
    if (dialedNumber_.isEmpty()) return;
    dialedNumber_.chop(1);
    emit dialedNumberChanged();
}

void PhonePlugin::sendDTMF(const QString& tone)
{
    Q_UNUSED(tone)
    // TODO: Send DTMF tone via BlueZ HFP
}

} // namespace plugins
} // namespace oap
