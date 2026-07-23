#include "TelephonyClient.hpp"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusMetaType>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTel, "oap.telephony")

namespace {
const QString kService = QStringLiteral("org.pipewire.Telephony");
const QString kRoot = QStringLiteral("/org/pipewire/Telephony");
const QString kAgIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
const QString kTransportIface = QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1");
const QString kCallIface = QStringLiteral("org.pipewire.Telephony.Call1");
const QString kPropsIface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kObjMgrIface = QStringLiteral("org.freedesktop.DBus.ObjectManager");

// Transport Codec is a BYTE (HFP codec ID), not a string — live-verified L1.
QString codecName(const QVariant& v)
{
    switch (v.toUInt()) {
    case 0: return QStringLiteral("none");
    case 1: return QStringLiteral("CVSD");
    case 2: return QStringLiteral("mSBC");
    case 3: return QStringLiteral("LC3-SWB");
    default: return QStringLiteral("codec-%1").arg(v.toUInt());
    }
}
} // namespace

namespace oap {

TelephonyClient::TelephonyClient(QObject* parent) : QObject(parent) {}

TelephonyClient::~TelephonyClient() { stop(); }

void TelephonyClient::start()
{
    if (started_) return;
    qDBusRegisterMetaType<oap::InterfaceMap>();
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcTel) << "No session bus — telephony disabled";
        return;
    }

    watcher_ = new QDBusServiceWatcher(kService, bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
        this);
    connect(watcher_, &QDBusServiceWatcher::serviceRegistered, this, [this]() { onServiceUp(); });
    connect(watcher_, &QDBusServiceWatcher::serviceUnregistered, this, [this]() { onServiceDown(); });

    // Empty path = all objects of the service. REQUIRED for InterfacesAdded/
    // InterfacesRemoved: Call1 children are announced by a PER-AG ObjectManager
    // at /org/pipewire/Telephony/agN, not by the root (live-verified, L2).
    // Slot takes oap::InterfaceMap (registered above) — QtDBus cannot deliver
    // a{sa{sv}} into a QVariantMap slot and drops the signal silently. The
    // explicit wire signature makes the type match unambiguous; the bool
    // return is logged so a future signature mismatch cannot fail silently
    // again (this bug cost a live debugging session — 2026-07-05).
    const bool okAdded = bus.connect(kService, QString(), kObjMgrIface,
        QStringLiteral("InterfacesAdded"), QStringLiteral("oa{sa{sv}}"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,oap::InterfaceMap)));
    const bool okRemoved = bus.connect(kService, QString(), kObjMgrIface,
        QStringLiteral("InterfacesRemoved"), QStringLiteral("oas"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    const bool okProps = bus.connect(kService, QString(), kPropsIface,
        QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    if (!okAdded || !okRemoved || !okProps)
        qCWarning(lcTel) << "D-Bus signal subscription failed:" << "InterfacesAdded" << okAdded
                         << "InterfacesRemoved" << okRemoved << "PropertiesChanged" << okProps;

    started_ = true;

    if (bus.interface() && bus.interface()->isServiceRegistered(kService))
        onServiceUp();
}

void TelephonyClient::stop()
{
    if (!started_) return;
    auto bus = QDBusConnection::sessionBus();
    bus.disconnect(kService, QString(), kObjMgrIface, QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,oap::InterfaceMap)));
    bus.disconnect(kService, QString(), kObjMgrIface, QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    bus.disconnect(kService, QString(), kPropsIface, QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    delete watcher_;
    watcher_ = nullptr;
    started_ = false;
    onServiceDown();
}

void TelephonyClient::onServiceUp()
{
    if (serviceUp_) return;
    serviceUp_ = true;
    qCInfo(lcTel) << "org.pipewire.Telephony is up";
    scanExistingObjects();
}

void TelephonyClient::onServiceDown()
{
    const bool wasAvailable = available();
    serviceUp_ = false;
    if (!callPath_.isEmpty()) {
        callPath_.clear();
        emit callSetupEnded();
    }
    agPath_.clear();
    agAddress_.clear();
    clearTransport();
    if (wasAvailable)
        emit availableChanged(false);
}

void TelephonyClient::scanExistingObjects()
{
    // Enumerates AudioGateway/transport objects ONLY — Call1 children are
    // never listed by GetManagedObjects (decision record §6.4).
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, kRoot, kObjMgrIface, QStringLiteral("GetManagedObjects"));
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcTel) << "GetManagedObjects failed:" << reply.errorMessage();
        return;
    }

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath objPath;
        arg >> objPath;

        QVariantMap agProps;
        QVariantMap transportProps;
        bool hasAg = false;
        bool hasTransport = false;
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
                QDBusVariant val;
                propsArg >> key >> val;
                props[key] = val.variant();
                propsArg.endMapEntry();
            }
            propsArg.endMap();
            ifacesArg.endMapEntry();

            if (iface == kAgIface) {
                hasAg = true;
                agProps = props;
            } else if (iface == kTransportIface) {
                hasTransport = true;
                transportProps = props;
            }
        }
        ifacesArg.endMap();
        // The selected transport is co-located with its AudioGateway. Adopt
        // the gateway first regardless of D-Bus map ordering, then apply only
        // that same object's carried transport state.
        if (hasAg)
            adoptAg(objPath.path(), agProps);
        if (hasTransport)
            adoptTransport(objPath.path(), transportProps);
        arg.endMapEntry();
    }
    arg.endMap();
}

void TelephonyClient::adoptAg(const QString& path, const QVariantMap& props)
{
    if (!agPath_.isEmpty() && agPath_ != path) {
        qCWarning(lcTel) << "Second AudioGateway ignored (single-AG v1):" << path;
        return;
    }
    const bool wasAvailable = available();
    agPath_ = path;
    agAddress_ = props.value(QStringLiteral("Address")).toString();
    qCInfo(lcTel) << "AudioGateway appeared:" << path << agAddress_;
    if (available() != wasAvailable)
        emit availableChanged(available());
}

void TelephonyClient::adoptTransport(const QString& path, const QVariantMap& props)
{
    if (agPath_.isEmpty() || path != agPath_) {
        qCWarning(lcTel) << "Transport ignored for non-selected AudioGateway:" << path;
        return;
    }
    transportPath_ = path;
    const QString state = props.value(QStringLiteral("State")).toString();
    const QString codec = props.contains(QStringLiteral("Codec"))
        ? codecName(props.value(QStringLiteral("Codec"))) : QString();
    if (!state.isEmpty() && state != transportState_) {
        transportState_ = state;
        emit transportStateChanged(state);
    }
    if (!codec.isEmpty() && codec != codec_) {
        codec_ = codec;
        qCInfo(lcTel) << "HFP codec:" << codec;
        emit codecChanged(codec);
    }
    applyRejectSco();
}

void TelephonyClient::clearTransport()
{
    transportPath_.clear();
    if (!transportState_.isEmpty()) {
        transportState_.clear();
        emit transportStateChanged({});
    }
    if (!codec_.isEmpty()) {
        codec_.clear();
        emit codecChanged({});
    }
}

void TelephonyClient::adoptCall(const QString& path, const QVariantMap& props)
{
    if (!callBelongsToSelectedAg(path)) {
        qCWarning(lcTel) << "Call ignored for non-selected AudioGateway:" << path;
        return;
    }
    if (!callPath_.isEmpty()) {
        // Second concurrent Call1 (call-waiting): still emit — the state
        // machine decides (it ignores setup while Active, design §5).
        qCInfo(lcTel) << "Additional Call1 during setup:" << path;
    }
    callPath_ = path;
    const QString state = props.value(QStringLiteral("State")).toString();
    const QString line = props.value(QStringLiteral("LineIdentification")).toString();
    qCInfo(lcTel) << "Call setup:" << state << line;
    emit callSetupStarted(state, line, props.value(QStringLiteral("Name")).toString());
}

bool TelephonyClient::callBelongsToSelectedAg(const QString& path) const
{
    return !agPath_.isEmpty() && path.startsWith(agPath_ + QLatin1Char('/'));
}

void TelephonyClient::onInterfacesAdded(const QDBusObjectPath& path, const oap::InterfaceMap& interfaces)
{
    if (interfaces.contains(kAgIface))
        adoptAg(path.path(), interfaces.value(kAgIface));
    if (interfaces.contains(kTransportIface))
        adoptTransport(path.path(), interfaces.value(kTransportIface));
    if (interfaces.contains(kCallIface))
        adoptCall(path.path(), interfaces.value(kCallIface));
}

void TelephonyClient::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces)
{
    const QString p = path.path();
    if (p == callPath_ && interfaces.contains(kCallIface)) {
        callPath_.clear();
        emit callSetupEnded();
    }
    if (p == transportPath_ && interfaces.contains(kTransportIface)) {
        clearTransport();
        emit transportRemoved();
    }
    if (p == agPath_ && interfaces.contains(kAgIface)) {
        const bool wasAvailable = available();
        agPath_.clear();
        agAddress_.clear();
        clearTransport();
        if (!callPath_.isEmpty()) {
            callPath_.clear();
            emit callSetupEnded();
        }
        qCInfo(lcTel) << "AudioGateway removed:" << p;
        if (available() != wasAvailable)
            emit availableChanged(available());
    }
}

void TelephonyClient::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                          const QStringList& invalidated,
                                          const QDBusMessage& msg)
{
    const QString path = msg.path();
    if (interface == kCallIface && path == callPath_) {
        if (changed.contains(QStringLiteral("State")))
            emit callSetupChanged(changed.value(QStringLiteral("State")).toString());
    } else if (interface == kTransportIface && path == transportPath_) {
        if (changed.contains(QStringLiteral("State"))) {
            const QString state = changed.value(QStringLiteral("State")).toString();
            if (transportState_ != state) {
                transportState_ = state;
                emit transportStateChanged(transportState_);
            }
        } else if (invalidated.contains(QStringLiteral("State"))
                   && !transportState_.isEmpty()) {
            transportState_.clear();
            emit transportStateChanged({});
        }
        if (changed.contains(QStringLiteral("Codec"))) {
            const QString codec = codecName(changed.value(QStringLiteral("Codec")));
            if (codec_ != codec) {
                codec_ = codec;
                qCInfo(lcTel) << "HFP codec:" << codec_;
                emit codecChanged(codec_);
            }
        } else if (invalidated.contains(QStringLiteral("Codec")) && !codec_.isEmpty()) {
            codec_.clear();
            emit codecChanged({});
        }
    }
}

void TelephonyClient::asyncCall(const QString& op, QDBusMessage msg)
{
    QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(msg);
    auto* w = new QDBusPendingCallWatcher(pending, this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, op](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        if (w->isError()) {
            qCWarning(lcTel) << op << "failed:" << w->error().message();
            emit commandFailed(op, w->error().message());
        }
    });
}

void TelephonyClient::dial(const QString& number)
{
    if (!available()) { emit commandFailed(QStringLiteral("Dial"), QStringLiteral("no audio gateway")); return; }
    auto msg = QDBusMessage::createMethodCall(kService, agPath_, kAgIface, QStringLiteral("Dial"));
    msg << number;
    asyncCall(QStringLiteral("Dial"), msg);
}

void TelephonyClient::answer()
{
    if (callPath_.isEmpty()) { emit commandFailed(QStringLiteral("Answer"), QStringLiteral("no call in setup")); return; }
    asyncCall(QStringLiteral("Answer"),
        QDBusMessage::createMethodCall(kService, callPath_, kCallIface, QStringLiteral("Answer")));
}

void TelephonyClient::hangupAll()
{
    if (!available()) { emit commandFailed(QStringLiteral("HangupAll"), QStringLiteral("no audio gateway")); return; }
    asyncCall(QStringLiteral("HangupAll"),
        QDBusMessage::createMethodCall(kService, agPath_, kAgIface, QStringLiteral("HangupAll")));
}

void TelephonyClient::sendTones(const QString& tones)
{
    if (!available()) { emit commandFailed(QStringLiteral("SendTones"), QStringLiteral("no audio gateway")); return; }
    auto msg = QDBusMessage::createMethodCall(kService, agPath_, kAgIface, QStringLiteral("SendTones"));
    msg << tones;
    asyncCall(QStringLiteral("SendTones"), msg);
}

void TelephonyClient::setRejectSco(bool reject)
{
    rejectSco_ = reject;
    applyRejectSco();
}

void TelephonyClient::applyRejectSco()
{
    if (transportPath_.isEmpty() || !serviceUp_) return;
    auto msg = QDBusMessage::createMethodCall(kService, transportPath_, kPropsIface, QStringLiteral("Set"));
    msg << kTransportIface << QStringLiteral("RejectSCO")
        << QVariant::fromValue(QDBusVariant(rejectSco_));
    asyncCall(QStringLiteral("Set RejectSCO"), msg);
    qCInfo(lcTel) << "RejectSCO →" << rejectSco_;
}

} // namespace oap
