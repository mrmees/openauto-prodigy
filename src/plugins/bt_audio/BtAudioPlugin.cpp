#include "BtAudioPlugin.hpp"
#include "BtAudioTap.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/AudioService.hpp"
#include "core/services/EqualizerService.hpp"
#include <QQmlContext>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <optional>

namespace {

QVariant unwrapDbusVariant(const QVariant& value)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        return value.value<QDBusVariant>().variant();
    return value;
}

QVariantMap demarshalVariantMap(const QVariant& value)
{
    const QVariant unwrapped = unwrapDbusVariant(value);
    if (unwrapped.metaType().id() == QMetaType::QVariantMap)
        return unwrapped.toMap();

    QVariantMap result;
    if (!unwrapped.canConvert<QDBusArgument>())
        return result;

    const QDBusArgument arg = unwrapped.value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QString key;
        QDBusVariant dbusValue;
        arg >> key >> dbusValue;
        result.insert(key, dbusValue.variant());
        arg.endMapEntry();
    }
    arg.endMap();
    return result;
}

std::optional<qint64> bluezMilliseconds(const QVariant& value)
{
    // org.bluez.MediaPlayer1 declares Position and Track.Duration as uint32
    // milliseconds. Require that D-Bus type instead of accepting signed or
    // textual coercions, then widen before storing so values above INT_MAX do
    // not wrap through QVariant::toInt().
    const QVariant unwrapped = unwrapDbusVariant(value);
    if (unwrapped.metaType().id() != QMetaType::UInt)
        return std::nullopt;
    return static_cast<qint64>(unwrapped.toUInt());
}

} // namespace

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

    // BT A2DP loopback tap (Task 7): route phone audio through the app EQ,
    // master volume, and focus arbitration. Wires only when BOTH concrete
    // services resolve AND PipeWire is up; a failed start leaves BT audio on
    // the direct (un-EQ'd) path and is never fatal.
    if (context) {
        auto* audio = dynamic_cast<oap::AudioService*>(context->audioService());
        auto* eq = dynamic_cast<oap::EqualizerService*>(context->equalizerService());
        if (audio && eq && audio->isAvailable()) {
            tap_ = new BtAudioTap(audio, eq, this);
            if (tap_->start()) {
                connect(this, &BtAudioPlugin::transportActiveChanged,
                        tap_, &BtAudioTap::setTransportActive);
                if (transportActive_)
                    tap_->setTransportActive(true);  // late-start catch-up
                if (hostContext_)
                    hostContext_->log(LogLevel::Info,
                        QStringLiteral("BtAudio: EQ tap running (openauto-bt-eq-in)"));
            } else if (hostContext_) {
                hostContext_->log(LogLevel::Warning,
                    QStringLiteral("BtAudio: EQ tap failed to start — BT audio direct (un-EQ'd)"));
            }
        }
    }

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Bluetooth Audio plugin initialized"));

    return true;
}

void BtAudioPlugin::shutdown()
{
    // Capture-first teardown BEFORE dropping D-Bus monitoring so no transport
    // edge can fire into a half-torn-down tap.
    if (tap_)
        tap_->stop();
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

    connect(bluezWatcher_, &QDBusServiceWatcher::serviceUnregistered,
            this, &BtAudioPlugin::onBluezServiceUnregistered);

    // ObjectManager InterfacesAdded is a{sa{sv}} — QtDBus refuses to deliver it
    // into a QVariantMap slot, so the connect below fails silently unless the
    // QMap<QString,QVariantMap> metatype is registered first (bench 2026-07-10
    // root cause). The named registration lets QMetaObject::invokeMethod resolve
    // the type too.
    qDBusRegisterMetaType<BtInterfaceMap>();
    qRegisterMetaType<oap::plugins::BtInterfaceMap>("oap::plugins::BtInterfaceMap");

    // Listen for InterfacesAdded/Removed on the BlueZ ObjectManager
    const bool okAdded = bus.connect(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath,BtInterfaceMap)));

    const bool okRemoved = bus.connect(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this,
        SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));

    // Listen for PropertiesChanged on any BlueZ object; the trailing
    // QDBusMessage carries the sender path so onPropertiesChanged can filter to
    // the currently-tracked transport/player.
    const bool okProps = bus.connect(
        QStringLiteral("org.bluez"),
        QString(),  // any path
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));

    if (hostContext_)
        hostContext_->log(LogLevel::Info,
            QStringLiteral("BtAudio D-Bus subscriptions: InterfacesAdded=%1 InterfacesRemoved=%2 "
                           "PropertiesChanged=%3")
                .arg(okAdded).arg(okRemoved).arg(okProps));

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
        this, SLOT(onInterfacesAdded(QDBusObjectPath,BtInterfaceMap)));
    bus.disconnect(
        QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    bus.disconnect(
        QStringLiteral("org.bluez"), QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));

    delete bluezWatcher_;
    bluezWatcher_ = nullptr;
    monitoring_ = false;
}

void BtAudioPlugin::onBluezServiceUnregistered()
{
    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("BtAudio: BlueZ disappeared from D-Bus"));
    transportActiveByPath_.clear();
    transportPath_.clear();
    playerPath_.clear();
    // A later transport can reconnect before its MediaPlayer1 snapshot. Clear
    // player data now so the composition-root catch-up cannot republish stale
    // time validity or metadata from the vanished BlueZ session.
    updatePlayerProperties({}, true);
    // BlueZ gone => no transports => no audio activity. Force the edge false
    // before the UI-state resets below so the tap releases focus.
    setTransportActive(false);
    if (connectionState_ != Disconnected) {
        connectionState_ = Disconnected;
        emit connectionStateChanged();
    }
    if (playbackState_ != Stopped) {
        playbackState_ = Stopped;
        emit playbackStateChanged();
    }
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
                transportPath_ = path;  // most-recent transport (device-name display)
                const QString state = props.value(QStringLiteral("State")).toString();

                // Try to get device name from the Device property (set BEFORE the
                // recompute below so its connectionStateChanged also notifies the
                // deviceName property binding).
                QString devicePath = props.value(QStringLiteral("Device")).value<QDBusObjectPath>().path();
                if (!devicePath.isEmpty()) {
                    QDBusInterface deviceIface(
                        QStringLiteral("org.bluez"), devicePath,
                        QStringLiteral("org.bluez.Device1"),
                        QDBusConnection::systemBus());
                    if (deviceIface.isValid()) {
                        QString alias = deviceIface.property("Alias").toString();
                        if (!alias.isEmpty())
                            deviceName_ = alias;
                    }
                }

                // Register this transport + its activity, then recompute edges.
                updateTransportState(path, state);
            }

            if (iface == QLatin1String("org.bluez.MediaPlayer1")) {
                adoptPlayer(path, props);
            }
        }
        ifacesArg.endMap();

        arg.endMapEntry();
    }
    arg.endMap();
}

void BtAudioPlugin::onInterfacesAdded(const QDBusObjectPath& path, const BtInterfaceMap& interfaces)
{
    const QString pathStr = path.path();

    if (interfaces.contains(QStringLiteral("org.bluez.MediaTransport1"))) {
        transportPath_ = pathStr;  // most-recent transport (device-name display)
        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                QStringLiteral("BtAudio: A2DP transport appeared: %1").arg(pathStr));

        // Prefer the State/Device already carried in this InterfacesAdded payload
        // — a separate synchronous property read can fail or race BlueZ, and an
        // already-active transport recorded inactive would have no later
        // PropertiesChanged to correct it. Only fall back to a synchronous read
        // when the payload lacks the State key. A read-back failure (e.g. no live
        // bus) leaves state empty ⇒ the transport is still tracked as present
        // (not active) so connectionState reads Connected while its edge stays false.
        const QVariantMap transportProps =
            interfaces.value(QStringLiteral("org.bluez.MediaTransport1"));

        QString state;
        QString devicePath;
        if (transportProps.contains(QStringLiteral("State"))) {
            state = transportProps.value(QStringLiteral("State")).toString();
            devicePath = transportProps.value(QStringLiteral("Device"))
                             .value<QDBusObjectPath>().path();
        } else {
            QDBusInterface iface(
                QStringLiteral("org.bluez"), pathStr,
                QStringLiteral("org.bluez.MediaTransport1"),
                QDBusConnection::systemBus());
            if (iface.isValid()) {
                state = iface.property("State").toString();
                devicePath = iface.property("Device").value<QDBusObjectPath>().path();
            }
        }

        // Resolve the human-readable device name (Device1.Alias is never in the
        // transport payload, so this stays a D-Bus read; skipped without a path).
        if (!devicePath.isEmpty()) {
            QDBusInterface deviceIface(
                QStringLiteral("org.bluez"), devicePath,
                QStringLiteral("org.bluez.Device1"),
                QDBusConnection::systemBus());
            if (deviceIface.isValid()) {
                const QString alias = deviceIface.property("Alias").toString();
                if (!alias.isEmpty())
                    deviceName_ = alias;
            }
        }

        // Track this transport + its activity, then recompute connection/edge
        // (Connected because the map is now non-empty).
        updateTransportState(pathStr, state);
    }

    if (interfaces.contains(QStringLiteral("org.bluez.MediaPlayer1"))) {
        QVariantMap playerProps =
            interfaces.value(QStringLiteral("org.bluez.MediaPlayer1"));
        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                QStringLiteral("BtAudio: AVRCP player appeared: %1").arg(pathStr));

        // ObjectManager already carries the initial property snapshot. Only
        // retain the legacy synchronous read-back for a genuinely empty map.
        if (playerProps.isEmpty()) {
            QDBusInterface iface(
                QStringLiteral("org.bluez"), pathStr,
                QStringLiteral("org.bluez.MediaPlayer1"),
                QDBusConnection::systemBus());
            if (iface.isValid()) {
                QVariantMap props;
                props[QStringLiteral("Status")] = iface.property("Status");
                props[QStringLiteral("Track")] = iface.property("Track");
                props[QStringLiteral("Position")] = iface.property("Position");
                playerProps = props;
            }
        }
        adoptPlayer(pathStr, playerProps);
    }
}

void BtAudioPlugin::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces)
{
    const QString pathStr = path.path();

    if (interfaces.contains(QStringLiteral("org.bluez.MediaTransport1"))
        && transportActiveByPath_.contains(pathStr)) {
        transportActiveByPath_.remove(pathStr);
        // transportPath_ tracks the most-recent transport for device-name
        // display; hand it off to a survivor, else clear.
        if (pathStr == transportPath_)
            transportPath_ = transportActiveByPath_.isEmpty()
                ? QString() : transportActiveByPath_.lastKey();

        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                QStringLiteral("BtAudio: A2DP transport removed: %1").arg(pathStr));

        const bool nowEmpty = transportActiveByPath_.isEmpty();

        // Recompute connection (Disconnected iff no transports remain) + the
        // audio-activity edge (still true if another transport is active).
        recomputeTransportState();

        // Playback state follows the AVRCP player, but the last transport
        // leaving means no phone is streaming — stop playback for the UI.
        if (nowEmpty && playbackState_ != Stopped) {
            playbackState_ = Stopped;
            emit playbackStateChanged();
        }
    }

    if (interfaces.contains(QStringLiteral("org.bluez.MediaPlayer1"))
        && pathStr == playerPath_) {
        playerPath_.clear();
        updatePlayerProperties({}, true);
    }
}

void BtAudioPlugin::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                         const QStringList& invalidated,
                                         const QDBusMessage& message)
{
    // PropertiesChanged is subscribed on ANY BlueZ path; the sender path rides
    // in the QDBusMessage. Ignore updates from objects other than the ones we
    // currently track, or a foreign transport/player would stomp our state. For
    // transports, ANY path we track (not just the most-recent one) is accepted
    // so a second phone's State updates are honoured; the player filter stays
    // single-path.
    const QString sender = message.path();
    if (interface == QLatin1String("org.bluez.MediaTransport1")
        && !transportActiveByPath_.contains(sender)) return;
    if (interface == QLatin1String("org.bluez.MediaPlayer1") && sender != playerPath_) return;

    if (interface == QLatin1String("org.bluez.MediaTransport1")) {
        if (changed.contains(QStringLiteral("State")))
            updateTransportState(sender, changed.value(QStringLiteral("State")).toString());
    }

    if (interface == QLatin1String("org.bluez.MediaPlayer1")) {
        QVariantMap effective = changed;
        // Invalidated D-Bus properties changed without carrying replacement
        // values. Treat time-bearing fields as unknown until BlueZ reports a
        // fresh value; never keep stale data marked valid.
        if (invalidated.contains(QStringLiteral("Track"))
            && !effective.contains(QStringLiteral("Track")))
            effective.insert(QStringLiteral("Track"), QVariant{});
        if (invalidated.contains(QStringLiteral("Position"))
            && !effective.contains(QStringLiteral("Position")))
            effective.insert(QStringLiteral("Position"), QVariant{});
        updatePlayerProperties(effective);
    }
}

void BtAudioPlugin::updateTransportState(const QString& path, const QString& state)
{
    // BlueZ MediaTransport1.State: "idle", "pending", "active". Only "active" is
    // real playback; idle/pending are connected-but-silent. Record this
    // transport's activity under its own path (inserting it if newly seen — the
    // transport's mere presence is what makes connectionState read Connected),
    // then recompute the aggregate connection/activity edges.
    transportActiveByPath_[path] = (state == QLatin1String("active"));
    recomputeTransportState();
}

void BtAudioPlugin::recomputeTransportState()
{
    // UI connection: Connected while ANY transport interface exists (audio
    // activity is a separate edge). Clearing to Disconnected also clears the
    // device name (whose property notifies via connectionStateChanged).
    const ConnectionState newConn =
        transportActiveByPath_.isEmpty() ? Disconnected : Connected;
    if (newConn != connectionState_) {
        connectionState_ = newConn;
        if (newConn == Disconnected)
            deviceName_.clear();
        emit connectionStateChanged();
    }

    // Audio-activity edge: true iff ANY tracked transport reports "active" — a
    // single idle phone can no longer force it false while another is playing.
    // This is the signal the BT loopback tap (Task 7) grabs audio focus off of.
    bool anyActive = false;
    for (auto it = transportActiveByPath_.cbegin(); it != transportActiveByPath_.cend(); ++it) {
        if (it.value()) { anyActive = true; break; }
    }
    setTransportActive(anyActive);
}

void BtAudioPlugin::setTransportActive(bool active)
{
    if (active == transportActive_) return;
    transportActive_ = active;
    emit transportActiveChanged(transportActive_);
}

void BtAudioPlugin::adoptPlayer(const QString& path, const QVariantMap& props)
{
    const bool replacingPlayer = !playerPath_.isEmpty() && playerPath_ != path;
    playerPath_ = path;
    if (!props.isEmpty() || replacingPlayer)
        updatePlayerProperties(props, replacingPlayer);
}

void BtAudioPlugin::updatePlayerProperties(const QVariantMap& props, bool resetBeforeApply)
{
    const QString oldTitle = trackTitle_;
    const QString oldArtist = trackArtist_;
    const QString oldAlbum = trackAlbum_;
    const qint64 oldDuration = trackDuration_;
    const qint64 oldPosition = trackPosition_;
    const bool oldPositionKnown = trackPositionKnown_;

    if (resetBeforeApply) {
        trackTitle_.clear();
        trackArtist_.clear();
        trackAlbum_.clear();
        trackDuration_ = 0;
        trackPosition_ = 0;
        trackPositionKnown_ = false;
    }

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
        // Track is a{sv}; QtDBus may leave it as QDBusArgument or provide an
        // already-converted QVariantMap depending on the delivery path.
        const QVariantMap track = demarshalVariantMap(props.value(QStringLiteral("Track")));

        const QString title = unwrapDbusVariant(track.value(QStringLiteral("Title"))).toString();
        const QString artist = unwrapDbusVariant(track.value(QStringLiteral("Artist"))).toString();
        const QString album = unwrapDbusVariant(track.value(QStringLiteral("Album"))).toString();
        const auto duration = bluezMilliseconds(track.value(QStringLiteral("Duration")));
        const qint64 newDuration = duration.value_or(0);

        if (title != trackTitle_ || artist != trackArtist_ || album != trackAlbum_) {
            trackTitle_ = title;
            trackArtist_ = artist;
            trackAlbum_ = album;
        }
        if (newDuration != trackDuration_)
            trackDuration_ = newDuration;
    }

    if (props.contains(QStringLiteral("Position"))) {
        const auto position = bluezMilliseconds(props.value(QStringLiteral("Position")));
        const qint64 newPosition = position.value_or(0);
        const bool newPositionKnown = position.has_value();
        if (newPosition != trackPosition_)
            trackPosition_ = newPosition;
        trackPositionKnown_ = newPositionKnown;
    }

    const bool metaChanged = oldTitle != trackTitle_ || oldArtist != trackArtist_
                             || oldAlbum != trackAlbum_;
    const bool durationValueChanged = oldDuration != trackDuration_;
    const bool positionValueChanged = oldPosition != trackPosition_;
    const bool progressStateChanged = durationValueChanged || positionValueChanged
                                      || oldPositionKnown != trackPositionKnown_;
    if (metaChanged)
        emit metadataChanged();
    if (durationValueChanged)
        emit durationChanged();
    if (positionValueChanged)
        emit positionChanged();
    if (progressStateChanged)
        emit progressChanged(trackPositionKnown_ ? trackPosition_ : -1,
                             trackDuration_);
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
    return {};  // Font-based icons — see MaterialIcon.qml (\uf01f headphones)
}

QList<oap::WidgetDescriptor> BtAudioPlugin::widgetDescriptors() const
{
    // Unified NowPlayingWidget registered in main.cpp handles both BT + AA sources.
    // BtAudioPlugin no longer registers its own widget descriptor.
    return {};
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
