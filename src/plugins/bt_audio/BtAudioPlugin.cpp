#include "BtAudioPlugin.hpp"
#include "core/plugin/IHostContext.hpp"
#include <QQmlContext>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusArgument>
#include <QDBusVariant>

namespace oap {
namespace plugins {

BtAudioPlugin::BtAudioPlugin(QObject* parent)
    : QObject(parent)
{
}

BtAudioPlugin::~BtAudioPlugin()
{
    shutdown();
}

bool BtAudioPlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    startDBusMonitoring();

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Bluetooth Audio plugin initialized"));

    return true;
}

void BtAudioPlugin::shutdown()
{
    stopDBusMonitoring();
}

void BtAudioPlugin::startDBusMonitoring()
{
    if (monitoring_) return;

    auto bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        if (hostContext_)
            hostContext_->log(LogLevel::Warning,
                QStringLiteral("BtAudio: Cannot connect to system D-Bus — BT monitoring disabled"));
        return;
    }

    // Watch for BlueZ service availability
    bluezWatcher_ = new QDBusServiceWatcher(
        QStringLiteral("org.bluez"),
        bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
        this);

    connect(bluezWatcher_, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        if (hostContext_)
            hostContext_->log(LogLevel::Info, QStringLiteral("BtAudio: BlueZ appeared on D-Bus"));
        scanExistingObjects();
    });

    connect(bluezWatcher_, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        if (hostContext_)
            hostContext_->log(LogLevel::Info, QStringLiteral("BtAudio: BlueZ disappeared from D-Bus"));
        transportPath_.clear();
        playerPath_.clear();
        if (connectionState_ != Disconnected) {
            connectionState_ = Disconnected;
            emit connectionStateChanged();
        }
        if (playbackState_ != Stopped) {
            playbackState_ = Stopped;
            emit playbackStateChanged();
        }
    });

    // Listen for InterfacesAdded/Removed on the BlueZ ObjectManager
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

    // Listen for PropertiesChanged on any BlueZ object (match by sender)
    bus.connect(
        QStringLiteral("org.bluez"),
        QString(),  // any path
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));

    monitoring_ = true;

    // Scan for already-connected devices
    scanExistingObjects();
}

void BtAudioPlugin::stopDBusMonitoring()
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

void BtAudioPlugin::scanExistingObjects()
{
    // Call GetManagedObjects on BlueZ to find existing transports/players
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("GetManagedObjects"));

    QDBusMessage reply = QDBusConnection::systemBus().call(msg, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage) return;

    // Reply is a{oa{sa{sv}}} — path -> interface -> properties
    // Use QDBusArgument manual deserialization for the nested map types
    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();

        QDBusObjectPath objPath;
        arg >> objPath;
        QString path = objPath.path();

        // Inner map: interface name -> properties
        const QDBusArgument ifacesArg = qvariant_cast<QDBusArgument>(arg.asVariant());
        ifacesArg.beginMap();
        while (!ifacesArg.atEnd()) {
            ifacesArg.beginMapEntry();

            QString iface;
            ifacesArg >> iface;

            // Properties map: a{sv}
            const QDBusArgument propsArg = qvariant_cast<QDBusArgument>(ifacesArg.asVariant());
            QVariantMap props;
            propsArg.beginMap();
            while (!propsArg.atEnd()) {
                propsArg.beginMapEntry();
                QString key;
                QVariant val;
                propsArg >> key;
                QDBusVariant dbusVal;
                propsArg >> dbusVal;
                props[key] = dbusVal.variant();
                propsArg.endMapEntry();
            }
            propsArg.endMap();

            ifacesArg.endMapEntry();

            if (iface == QLatin1String("org.bluez.MediaTransport1")) {
                transportPath_ = path;
                QString state = props.value(QStringLiteral("State")).toString();
                updateTransportState(state);

                // Try to get device name from the Device property
                QString devicePath = props.value(QStringLiteral("Device")).value<QDBusObjectPath>().path();
                if (!devicePath.isEmpty()) {
                    QDBusInterface deviceIface(
                        QStringLiteral("org.bluez"), devicePath,
                        QStringLiteral("org.bluez.Device1"),
                        QDBusConnection::systemBus());
                    if (deviceIface.isValid()) {
                        QString alias = deviceIface.property("Alias").toString();
                        if (!alias.isEmpty() && alias != deviceName_) {
                            deviceName_ = alias;
                            emit connectionStateChanged();
                        }
                    }
                }
            }

            if (iface == QLatin1String("org.bluez.MediaPlayer1")) {
                playerPath_ = path;
                updatePlayerProperties(props);
            }
        }
        ifacesArg.endMap();

        arg.endMapEntry();
    }
    arg.endMap();
}

void BtAudioPlugin::onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces)
{
    const QString pathStr = path.path();

    if (interfaces.contains(QStringLiteral("org.bluez.MediaTransport1"))) {
        transportPath_ = pathStr;
        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                QStringLiteral("BtAudio: A2DP transport appeared: %1").arg(pathStr));

        // Read transport properties
        QDBusInterface iface(
            QStringLiteral("org.bluez"), pathStr,
            QStringLiteral("org.bluez.MediaTransport1"),
            QDBusConnection::systemBus());
        if (iface.isValid()) {
            updateTransportState(iface.property("State").toString());

            QString devicePath = iface.property("Device").value<QDBusObjectPath>().path();
            if (!devicePath.isEmpty()) {
                QDBusInterface deviceIface(
                    QStringLiteral("org.bluez"), devicePath,
                    QStringLiteral("org.bluez.Device1"),
                    QDBusConnection::systemBus());
                if (deviceIface.isValid()) {
                    deviceName_ = deviceIface.property("Alias").toString();
                }
            }
        }

        if (connectionState_ != Connected) {
            connectionState_ = Connected;
            emit connectionStateChanged();
        }
    }

    if (interfaces.contains(QStringLiteral("org.bluez.MediaPlayer1"))) {
        playerPath_ = pathStr;
        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                QStringLiteral("BtAudio: AVRCP player appeared: %1").arg(pathStr));

        QDBusInterface iface(
            QStringLiteral("org.bluez"), pathStr,
            QStringLiteral("org.bluez.MediaPlayer1"),
            QDBusConnection::systemBus());
        if (iface.isValid()) {
            QVariantMap props;
            props[QStringLiteral("Status")] = iface.property("Status");
            props[QStringLiteral("Track")] = iface.property("Track");
            props[QStringLiteral("Position")] = iface.property("Position");
            updatePlayerProperties(props);
        }
    }
}

void BtAudioPlugin::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces)
{
    const QString pathStr = path.path();

    if (interfaces.contains(QStringLiteral("org.bluez.MediaTransport1"))
        && pathStr == transportPath_) {
        transportPath_.clear();
        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                QStringLiteral("BtAudio: A2DP transport removed"));

        connectionState_ = Disconnected;
        deviceName_.clear();
        emit connectionStateChanged();

        playbackState_ = Stopped;
        emit playbackStateChanged();
    }

    if (interfaces.contains(QStringLiteral("org.bluez.MediaPlayer1"))
        && pathStr == playerPath_) {
        playerPath_.clear();
        trackTitle_.clear();
        trackArtist_.clear();
        trackAlbum_.clear();
        trackDuration_ = 0;
        trackPosition_ = 0;
        emit metadataChanged();
        emit positionChanged();
    }
}

void BtAudioPlugin::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                         const QStringList& /*invalidated*/)
{
    if (interface == QLatin1String("org.bluez.MediaTransport1")) {
        if (changed.contains(QStringLiteral("State")))
            updateTransportState(changed.value(QStringLiteral("State")).toString());
    }

    if (interface == QLatin1String("org.bluez.MediaPlayer1")) {
        updatePlayerProperties(changed);
    }
}

void BtAudioPlugin::updateTransportState(const QString& state)
{
    // BlueZ MediaTransport1.State: "idle", "pending", "active"
    ConnectionState newState = (state == QLatin1String("active") || state == QLatin1String("idle")
                                || state == QLatin1String("pending"))
                               ? Connected : Disconnected;

    if (newState != connectionState_) {
        connectionState_ = newState;
        emit connectionStateChanged();
    }
}

void BtAudioPlugin::updatePlayerProperties(const QVariantMap& props)
{
    bool metaChanged = false;

    if (props.contains(QStringLiteral("Status"))) {
        QString status = props.value(QStringLiteral("Status")).toString();
        PlaybackState newState = Stopped;
        if (status == QLatin1String("playing"))
            newState = Playing;
        else if (status == QLatin1String("paused"))
            newState = Paused;

        if (newState != playbackState_) {
            playbackState_ = newState;
            emit playbackStateChanged();
        }
    }

    if (props.contains(QStringLiteral("Track"))) {
        QVariant trackVar = props.value(QStringLiteral("Track"));
        QVariantMap track;

        // Track can come as QDBusArgument or QVariantMap depending on context
        if (trackVar.canConvert<QDBusArgument>()) {
            QDBusArgument arg = trackVar.value<QDBusArgument>();
            arg >> track;
        } else if (trackVar.canConvert<QVariantMap>()) {
            track = trackVar.toMap();
        }

        QString title = track.value(QStringLiteral("Title")).toString();
        QString artist = track.value(QStringLiteral("Artist")).toString();
        QString album = track.value(QStringLiteral("Album")).toString();
        int duration = track.value(QStringLiteral("Duration")).toInt();  // microseconds from BlueZ

        if (title != trackTitle_ || artist != trackArtist_ || album != trackAlbum_) {
            trackTitle_ = title;
            trackArtist_ = artist;
            trackAlbum_ = album;
            trackDuration_ = duration / 1000;  // convert to milliseconds
            metaChanged = true;
        }
    }

    if (props.contains(QStringLiteral("Position"))) {
        int pos = props.value(QStringLiteral("Position")).toInt() / 1000;  // us -> ms
        if (pos != trackPosition_) {
            trackPosition_ = pos;
            emit positionChanged();
        }
    }

    if (metaChanged)
        emit metadataChanged();
}

void BtAudioPlugin::sendPlayerCommand(const QString& command)
{
    if (playerPath_.isEmpty()) return;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.bluez"),
        playerPath_,
        QStringLiteral("org.bluez.MediaPlayer1"),
        command);

    QDBusConnection::systemBus().call(msg, QDBus::NoBlock);
}

void BtAudioPlugin::onActivated(QQmlContext* context)
{
    if (!context) return;
    context->setContextProperty("BtAudioPlugin", this);
}

void BtAudioPlugin::onDeactivated()
{
    // Child context destroyed by PluginRuntimeContext
}

QUrl BtAudioPlugin::qmlComponent() const
{
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/BtAudioView.qml"));
}

QUrl BtAudioPlugin::iconSource() const
{
    return QUrl(QStringLiteral("qrc:/icons/bluetooth-audio.svg"));
}

void BtAudioPlugin::play()
{
    sendPlayerCommand(QStringLiteral("Play"));
}

void BtAudioPlugin::pause()
{
    sendPlayerCommand(QStringLiteral("Pause"));
}

void BtAudioPlugin::next()
{
    sendPlayerCommand(QStringLiteral("Next"));
}

void BtAudioPlugin::previous()
{
    sendPlayerCommand(QStringLiteral("Previous"));
}

} // namespace plugins
} // namespace oap
