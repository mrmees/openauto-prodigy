#include "PhoneStateService.hpp"
#include "INotificationService.hpp"
#include "TelephonyClient.hpp"
#include "core/audio/ScoNodeMonitor.hpp"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDBusServiceWatcher>

namespace oap {

PhoneStateService::PhoneStateService(QObject* parent)
    : IPhoneStateService(parent)
{
    callTimer_.setInterval(1000);
    connect(&callTimer_, &QTimer::timeout, this, &PhoneStateService::updateCallDuration);

    settleTimer_.setSingleShot(true);
    connect(&settleTimer_, &QTimer::timeout, this, &PhoneStateService::onSettleTimeout);
    scoDebounceTimer_.setSingleShot(true);
    connect(&scoDebounceTimer_, &QTimer::timeout, this, &PhoneStateService::onScoDebounceTimeout);
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

bool PhoneStateService::answer()
{
    if (callState_ != ICallStateProvider::Ringing) return false;
    if (telephony_ && telephonyAvailable_) {
        // Real mode: request only. The transition arrives via telephony/SCO
        // events — never optimistically (design §12.5).
        telephony_->answer();
        return true;
    }
    setCallStateInternal(ICallStateProvider::Active);  // mock mode
    return true;
}

bool PhoneStateService::hangup()
{
    if (callState_ == ICallStateProvider::Idle) return false;
    if (telephony_ && telephonyAvailable_) {
        telephony_->hangupAll();
        return true;
    }
    callerNumber_.clear();  // mock mode
    callerName_.clear();
    setCallStateInternal(ICallStateProvider::Idle);
    return true;
}

bool PhoneStateService::dial(const QString& number)
{
    if (callState_ != ICallStateProvider::Idle || number.isEmpty()) return false;
    if (telephony_ && telephonyAvailable_) {
        telephony_->dial(number);
        return true;
    }
    if (!telephony_) {                          // mock mode: keep dev UI alive
        callerNumber_ = number;
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Dialing);
        return true;
    }
    return false;                               // attached but no phone
}

bool PhoneStateService::sendDtmf(const QString& tones)
{
    if (callState_ != ICallStateProvider::Active || tones.isEmpty()) return false;
    if (telephony_ && telephonyAvailable_) {
        telephony_->sendTones(tones);
        return true;
    }
    return telephony_ == nullptr;               // mock mode pretends success
}

void PhoneStateService::attachTelephony(TelephonyClient* client)
{
    telephony_ = client;
    if (!client) return;
    connect(client, &TelephonyClient::callSetupStarted,
            this, &PhoneStateService::onCallSetupStarted);
    connect(client, &TelephonyClient::callSetupChanged,
            this, &PhoneStateService::onCallSetupChanged);
    connect(client, &TelephonyClient::callSetupEnded,
            this, &PhoneStateService::onCallSetupEnded);
    connect(client, &TelephonyClient::transportStateChanged,
            this, &PhoneStateService::onTransportStateChanged);
    connect(client, &TelephonyClient::availableChanged,
            this, &PhoneStateService::onTelephonyAvailable);
    onTelephonyAvailable(client->available());
}

void PhoneStateService::attachScoMonitor(ScoNodeMonitor* monitor)
{
    if (!monitor) return;
    connect(monitor, &ScoNodeMonitor::scoRunningChanged,
            this, &PhoneStateService::onScoRunningChanged);
    onScoRunningChanged(monitor->scoRunning());
}

void PhoneStateService::onCallSetupStarted(const QString& state, const QString& line,
                                           const QString& name)
{
    if (callState_ == ICallStateProvider::Active) {
        // Call-waiting: single-call model in v1 — log and ignore (design §5).
        qWarning() << "PhoneStateService: second call ignored (call-waiting unsupported):" << line;
        return;
    }
    callerNumber_ = line;
    callerName_ = name;
    if (state == QLatin1String("incoming"))
        setCallStateInternal(ICallStateProvider::Ringing);
    else if (state == QLatin1String("dialing"))
        setCallStateInternal(ICallStateProvider::Dialing);
    else if (state == QLatin1String("alerting"))
        setCallStateInternal(ICallStateProvider::Alerting);
    else
        qWarning() << "PhoneStateService: unknown setup state" << state;
}

void PhoneStateService::onCallSetupChanged(const QString& state)
{
    if (!inSetupState()) return;
    if (state == QLatin1String("alerting")) {
        setCallStateInternal(ICallStateProvider::Alerting);
    } else if (state == QLatin1String("active")) {
        inSettle_ = false;
        settleTimer_.stop();
        setCallStateInternal(ICallStateProvider::Active);
    } else if (state == QLatin1String("disconnected")) {
        // Live-verified (L2): reject/failure emits State→"disconnected"
        // before InterfacesRemoved — resolve now, skip the Settling wait.
        inSettle_ = false;
        settleTimer_.stop();
        callerNumber_.clear();
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Idle);
    }
}

void PhoneStateService::onCallSetupEnded()
{
    if (!inSetupState()) return;   // Active: waiting-call resolution etc. — no transition
    if (scoRunning_) {
        setCallStateInternal(ICallStateProvider::Active);
        return;
    }
    // Ambiguous: answered (SCO imminent) or rejected/cancelled. Keep
    // reporting the setup state during the grace window (no UI flap).
    inSettle_ = true;
    settleTimer_.start(settleGraceMs_);
}

void PhoneStateService::onSettleTimeout()
{
    if (!inSettle_) return;
    inSettle_ = false;
    callerNumber_.clear();
    callerName_.clear();
    setCallStateInternal(ICallStateProvider::Idle);
}

void PhoneStateService::onScoRunningChanged(bool running)
{
    scoRunning_ = running;
    if (running) {
        scoDebounceTimer_.stop();
        if (inSettle_) {
            inSettle_ = false;
            settleTimer_.stop();
            setCallStateInternal(ICallStateProvider::Active);
        } else if (callState_ == ICallStateProvider::Idle && telephonyAvailable_) {
            // Recovery: restarted mid-call, or user routed audio back to car.
            setCallStateInternal(ICallStateProvider::Active);
        }
    } else {
        if (callState_ == ICallStateProvider::Active)
            scoDebounceTimer_.start(scoDebounceMs_);
    }
}

void PhoneStateService::onScoDebounceTimeout()
{
    if (callState_ == ICallStateProvider::Active && !scoRunning_) {
        callerNumber_.clear();
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Idle);
    }
}

void PhoneStateService::onTransportStateChanged(const QString& state)
{
    transportState_ = state;
    // Transport state is ADVISORY (pre-call "active" quirk, decision §6.4):
    // trusted only (a) as the settle-window accept signal when SCO is
    // unobservable, (b) as an end signal while Active with SCO down.
    if (state == QLatin1String("active") && inSettle_) {
        inSettle_ = false;
        settleTimer_.stop();
        setCallStateInternal(ICallStateProvider::Active);
    } else if (state == QLatin1String("idle")
               && callState_ == ICallStateProvider::Active && !scoRunning_) {
        scoDebounceTimer_.stop();
        callerNumber_.clear();
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Idle);
    }
}

void PhoneStateService::onTelephonyAvailable(bool available)
{
    if (available == telephonyAvailable_) return;
    telephonyAvailable_ = available;
    if (!available) {
        inSettle_ = false;
        settleTimer_.stop();
        scoDebounceTimer_.stop();
        callerNumber_.clear();
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Idle);
    }
    emit telephonyAvailableChanged();
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
    qInfo() << "PhoneStateService: call state" << callState_ << "→" << state;
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

    // ObjectManager InterfacesAdded is a{sa{sv}} — QtDBus refuses to deliver it
    // into a QVariantMap slot, so the connect below fails silently unless the
    // QMap<QString,QVariantMap> metatype is registered first (bench 2026-07-10
    // root cause). The named registration lets QMetaObject::invokeMethod resolve
    // the type too.
    qDBusRegisterMetaType<BluezInterfaceMap>();
    qRegisterMetaType<oap::BluezInterfaceMap>("oap::BluezInterfaceMap");

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

    const bool okAdded = bus.connect(QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,BluezInterfaceMap)));

    const bool okRemoved = bus.connect(QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));

    const bool okProps = bus.connect(QStringLiteral("org.bluez"), QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    qInfo() << "PhoneStateService D-Bus subscriptions: InterfacesAdded=" << okAdded
            << "InterfacesRemoved=" << okRemoved << "PropertiesChanged=" << okProps;

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
        this, SLOT(onInterfacesAdded(QDBusObjectPath,BluezInterfaceMap)));
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

            if (iface == QLatin1String("org.bluez.Device1"))
                adoptBluezDevice(path, props);
        }
        ifacesArg.endMap();
        arg.endMapEntry();
    }
    arg.endMap();
}

void PhoneStateService::onInterfacesAdded(const QDBusObjectPath& path,
                                          const BluezInterfaceMap& interfaces)
{
    // The Device1 properties ride along in the signal payload — adopt straight
    // from it. NO QDBusInterface read-back: the old synchronous property query
    // was both an extra round-trip and, coupled with the QVariantMap slot
    // signature that never connected, dead code (bench 2026-07-10).
    auto it = interfaces.constFind(QStringLiteral("org.bluez.Device1"));
    if (it == interfaces.constEnd()) return;
    adoptBluezDevice(path.path(), it.value());
}

void PhoneStateService::adoptBluezDevice(const QString& path, const QVariantMap& deviceProps)
{
    if (phoneConnected_) return;   // single-phone model — first connected HFP wins

    const bool connected = deviceProps.value(QStringLiteral("Connected")).toBool();
    const QStringList uuids = deviceProps.value(QStringLiteral("UUIDs")).toStringList();

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
        deviceName_ = deviceProps.value(QStringLiteral("Alias")).toString();
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

bool PhoneStateService::shouldRescanOnDeviceChange(bool phoneConnected,
                                                   const QVariantMap& changed)
{
    if (phoneConnected) return false;   // single-phone model — first connected wins
    if (changed.value(QStringLiteral("Connected")).toBool()) return true;
    if (changed.contains(QStringLiteral("UUIDs"))) return true;
    return false;
}

void PhoneStateService::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                             const QStringList& /*invalidated*/)
{
    if (interface != QLatin1String("org.bluez.Device1")) return;

    // Disconnection branch (unchanged): a live phone dropping Connected tears
    // the session down.
    if (changed.contains(QStringLiteral("Connected"))) {
        bool connected = changed.value(QStringLiteral("Connected")).toBool();
        if (!connected && phoneConnected_) {
            phoneConnected_ = false;
            deviceName_.clear();
            emit connectionChanged();
            setCallStateInternal(ICallStateProvider::Idle);
        }
    }

    // Late-arriving fresh pair: BlueZ can create Device1 disconnected and
    // deliver Connected=true / UUIDs later. Re-run the managed-objects scan —
    // it demarshals full device state and routes through adoptBluezDevice(),
    // so a phone paired after boot is adopted without a restart.
    if (shouldRescanOnDeviceChange(phoneConnected_, changed))
        scanExistingDevices();
}

} // namespace oap
