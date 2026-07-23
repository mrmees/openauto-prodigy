#include "BtAudioPlugin.hpp"
#include "BtAudioTap.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/AudioService.hpp"
#include "core/services/EqualizerService.hpp"
#include <QQmlContext>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <optional>
#include <utility>

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

QString bluezDeviceLabel(const QVariantMap& properties)
{
    const QString alias =
        unwrapDbusVariant(properties.value(QStringLiteral("Alias"))).toString();
    if (!alias.isEmpty())
        return alias;
    return unwrapDbusVariant(properties.value(QStringLiteral("Name"))).toString();
}

std::optional<oap::plugins::BtManagedObjectMap>
parseManagedObjects(const QDBusMessage& reply, QString& failure)
{
    if (reply.type() != QDBusMessage::ReplyMessage) {
        failure = reply.errorMessage().isEmpty()
            ? QStringLiteral("D-Bus call failed") : reply.errorMessage();
        return std::nullopt;
    }

    constexpr auto kManagedObjectsSignature = "a{oa{sa{sv}}}";
    if (reply.arguments().size() != 1
        || reply.signature() != QLatin1String(kManagedObjectsSignature)
        || !reply.arguments().constFirst().canConvert<QDBusArgument>()) {
        failure = QStringLiteral("expected one %1 argument, received signature '%2' with %3 arguments")
                      .arg(QLatin1String(kManagedObjectsSignature), reply.signature())
                      .arg(reply.arguments().size());
        return std::nullopt;
    }

    oap::plugins::BtManagedObjectMap objects;
    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath objectPath;
        arg >> objectPath;

        oap::plugins::BtInterfaceMap interfaces;
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

} // namespace

namespace oap {
namespace plugins {

BtAudioPlugin::BtAudioPlugin(QObject* parent)
    : BtAudioPlugin(QDBusConnection::systemBus(), parent)
{
}

BtAudioPlugin::BtAudioPlugin(const QDBusConnection& bus, QObject* parent)
    : QObject(parent)
    , bus_(bus)
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

    if (!bus_.isConnected()) {
        if (hostContext_)
            hostContext_->log(LogLevel::Warning,
                QStringLiteral("BtAudio: Cannot connect to system D-Bus — BT monitoring disabled"));
        return;
    }

    // Watch for BlueZ service availability
    bluezWatcher_ = new QDBusServiceWatcher(
        QStringLiteral("org.bluez"),
        bus_,
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
    const bool okAdded = bus_.connect(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath,BtInterfaceMap)));

    const bool okRemoved = bus_.connect(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this,
        SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));

    // Listen for PropertiesChanged on any BlueZ object; the trailing
    // QDBusMessage carries the sender path so onPropertiesChanged can filter to
    // the currently-tracked transport/player.
    const bool okProps = bus_.connect(
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

    bus_.disconnect(
        QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,BtInterfaceMap)));
    bus_.disconnect(
        QStringLiteral("org.bluez"), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    bus_.disconnect(
        QStringLiteral("org.bluez"), QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));

    delete bluezWatcher_;
    bluezWatcher_ = nullptr;
    monitoring_ = false;
    scanPending_ = false;
    pendingPropertyChanges_.clear();
}

void BtAudioPlugin::onBluezServiceUnregistered()
{
    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("BtAudio: BlueZ disappeared from D-Bus"));
    if (scanInFlight_)
        scanPending_ = true;
    transportActiveByPath_.clear();
    transportDeviceByPath_.clear();
    devicePropertiesByPath_.clear();
    transportPath_.clear();
    playerPath_.clear();
    // A later transport can reconnect before its MediaPlayer1 snapshot. Clear
    // player data now so the composition-root catch-up cannot republish stale
    // time validity or metadata from the vanished BlueZ session.
    updatePlayerProperties({}, true);
    // BlueZ gone => no transports => no audio activity. Recompute both edges
    // so the tap releases focus and the displayed device is cleared together.
    recomputeTransportState();
}

void BtAudioPlugin::scanExistingObjects()
{
    if (!monitoring_)
        return;
    if (scanInFlight_) {
        scanPending_ = true;
        return;
    }

    scanInFlight_ = true;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("GetManagedObjects"));

    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(msg, 2000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                finishExistingObjectScan(reply);
            });
}

void BtAudioPlugin::finishExistingObjectScan(const QDBusMessage& reply)
{
    scanInFlight_ = false;
    const bool needsRescan = scanPending_;
    scanPending_ = false;
    if (monitoring_ && !needsRescan) {
        QString failure;
        auto objects = parseManagedObjects(reply, failure);
        if (objects) {
            mergePendingPropertyChanges(*objects);
            applyManagedObjectsSnapshot(*objects);
        } else {
            if (hostContext_) {
                hostContext_->log(LogLevel::Warning,
                                  QStringLiteral("BtAudio: GetManagedObjects rejected: %1")
                                      .arg(failure));
            }
            // A failed or malformed topology reply is not an empty snapshot.
            // Preserve the last completed state and replay property deltas
            // received while the request was outstanding.
            const auto pendingChanges = std::exchange(pendingPropertyChanges_, {});
            for (const auto& change : pendingChanges)
                applyPropertiesChanged(change.interface, change.changed,
                                       change.invalidated, change.message);
        }
    }

    if (needsRescan) {
        // The trailing topology snapshot was requested after the relevant
        // signals, so it supersedes property deltas collected for this reply.
        pendingPropertyChanges_.clear();
    }

    if (monitoring_ && needsRescan)
        scanExistingObjects();
}

void BtAudioPlugin::mergePendingPropertyChanges(BtManagedObjectMap& objects)
{
    const auto pendingChanges = std::exchange(pendingPropertyChanges_, {});
    for (const auto& change : pendingChanges) {
        auto object = objects.find(change.message.path());
        if (object == objects.end())
            continue;
        auto properties = object->find(change.interface);
        if (properties == object->end())
            continue;
        for (auto it = change.changed.cbegin(); it != change.changed.cend(); ++it)
            properties->insert(it.key(), it.value());
        for (const QString& property : change.invalidated)
            properties->remove(property);
    }
}

void BtAudioPlugin::onInterfacesAdded(const QDBusObjectPath& path, const BtInterfaceMap& interfaces)
{
    if (scanInFlight_)
        scanPending_ = true;
    const QString pathStr = path.path();
    applyInterfaces(pathStr, interfaces);
}

void BtAudioPlugin::applyInterfaces(const QString& path, const BtInterfaceMap& interfaces)
{
    if (interfaces.contains(QStringLiteral("org.bluez.Device1")))
        adoptDevice(path, interfaces.value(QStringLiteral("org.bluez.Device1")));

    if (interfaces.contains(QStringLiteral("org.bluez.MediaTransport1"))) {
        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                              QStringLiteral("BtAudio: A2DP transport appeared: %1").arg(path));
        adoptTransport(path, interfaces.value(QStringLiteral("org.bluez.MediaTransport1")));
    }

    if (interfaces.contains(QStringLiteral("org.bluez.MediaPlayer1"))) {
        if (hostContext_)
            hostContext_->log(LogLevel::Info,
                              QStringLiteral("BtAudio: AVRCP player appeared: %1").arg(path));
        adoptPlayer(path, interfaces.value(QStringLiteral("org.bluez.MediaPlayer1")));
    }
}

void BtAudioPlugin::adoptDevice(const QString& path, const QVariantMap& props)
{
    devicePropertiesByPath_.insert(path, props);
    recomputeTransportState();
}

void BtAudioPlugin::adoptTransport(const QString& path, const QVariantMap& props)
{
    transportPath_ = path;
    const QString devicePath =
        unwrapDbusVariant(props.value(QStringLiteral("Device")))
            .value<QDBusObjectPath>().path();
    if (devicePath.isEmpty())
        transportDeviceByPath_.remove(path);
    else
        transportDeviceByPath_.insert(path, devicePath);
    updateTransportState(path,
                         unwrapDbusVariant(props.value(QStringLiteral("State"))).toString());
}

void BtAudioPlugin::applyManagedObjectsSnapshot(const BtManagedObjectMap& objects)
{
    QMap<QString, QVariantMap> newDeviceProperties;
    QMap<QString, bool> newTransportActivity;
    QMap<QString, QString> newTransportDevices;
    QString newTransportPath;
    QString newPlayerPath;
    QVariantMap newPlayerProperties;

    // Resolve Device1 names before transports so startup never needs a
    // follow-up property read merely to label an already-connected phone.
    for (auto objectIt = objects.cbegin(); objectIt != objects.cend(); ++objectIt) {
        const auto deviceIt = objectIt.value().constFind(QStringLiteral("org.bluez.Device1"));
        if (deviceIt == objectIt.value().cend())
            continue;
        newDeviceProperties.insert(objectIt.key(), deviceIt.value());
    }

    for (auto objectIt = objects.cbegin(); objectIt != objects.cend(); ++objectIt) {
        const auto transportIt =
            objectIt.value().constFind(QStringLiteral("org.bluez.MediaTransport1"));
        if (transportIt != objectIt.value().cend()) {
            newTransportPath = objectIt.key();
            const QString state =
                unwrapDbusVariant(transportIt->value(QStringLiteral("State"))).toString();
            newTransportActivity.insert(objectIt.key(), state == QLatin1String("active"));
            const QString devicePath =
                unwrapDbusVariant(transportIt->value(QStringLiteral("Device")))
                    .value<QDBusObjectPath>().path();
            if (!devicePath.isEmpty())
                newTransportDevices.insert(objectIt.key(), devicePath);
        }

        const auto playerIt =
            objectIt.value().constFind(QStringLiteral("org.bluez.MediaPlayer1"));
        if (playerIt != objectIt.value().cend()) {
            // Preserve the existing single-player policy: the last object in
            // ObjectManager map order wins, matching the former startup scan.
            newPlayerPath = objectIt.key();
            newPlayerProperties = playerIt.value();
        }
    }

    devicePropertiesByPath_ = newDeviceProperties;
    transportActiveByPath_ = newTransportActivity;
    transportDeviceByPath_ = newTransportDevices;
    transportPath_ = newTransportPath;
    recomputeTransportState();

    playerPath_ = newPlayerPath;
    // A complete snapshot is authoritative. Missing initial properties remain
    // unknown/stopped until PropertiesChanged supplies them, rather than
    // retaining values from an earlier player/session.
    updatePlayerProperties(newPlayerProperties, true);
}

void BtAudioPlugin::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces)
{
    if (scanInFlight_)
        scanPending_ = true;
    const QString pathStr = path.path();

    if (interfaces.contains(QStringLiteral("org.bluez.MediaTransport1"))
        && transportActiveByPath_.contains(pathStr)) {
        transportActiveByPath_.remove(pathStr);
        transportDeviceByPath_.remove(pathStr);
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

    if (interfaces.contains(QStringLiteral("org.bluez.Device1"))) {
        devicePropertiesByPath_.remove(pathStr);
        recomputeTransportState();
    }
}

void BtAudioPlugin::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                         const QStringList& invalidated,
                                         const QDBusMessage& message)
{
    if (scanInFlight_)
        pendingPropertyChanges_.append({interface, changed, invalidated, message});
    if (scanInFlight_)
        return;
    applyPropertiesChanged(interface, changed, invalidated, message);
}

void BtAudioPlugin::applyPropertiesChanged(const QString& interface,
                                           const QVariantMap& changed,
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
    if (interface == QLatin1String("org.bluez.Device1")
        && !transportDeviceByPath_.values().contains(sender)) return;

    if (interface == QLatin1String("org.bluez.MediaTransport1")) {
        if (changed.contains(QStringLiteral("State")))
            updateTransportState(sender, changed.value(QStringLiteral("State")).toString());
        else if (invalidated.contains(QStringLiteral("State")))
            updateTransportState(sender, {});
        if (changed.contains(QStringLiteral("Device"))) {
            const QString devicePath = unwrapDbusVariant(changed.value(QStringLiteral("Device")))
                                           .value<QDBusObjectPath>().path();
            if (devicePath.isEmpty())
                transportDeviceByPath_.remove(sender);
            else
                transportDeviceByPath_.insert(sender, devicePath);
            recomputeTransportState();
        } else if (invalidated.contains(QStringLiteral("Device"))) {
            transportDeviceByPath_.remove(sender);
            recomputeTransportState();
        }
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
        if (invalidated.contains(QStringLiteral("Status"))
            && !effective.contains(QStringLiteral("Status")))
            effective.insert(QStringLiteral("Status"), QVariant{});
        updatePlayerProperties(effective);
    }

    if (interface == QLatin1String("org.bluez.Device1")) {
        QVariantMap properties = devicePropertiesByPath_.value(sender);
        for (const QString& property : invalidated)
            properties.remove(property);
        for (auto it = changed.cbegin(); it != changed.cend(); ++it)
            properties.insert(it.key(), it.value());
        devicePropertiesByPath_.insert(sender, properties);
        recomputeTransportState();
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
    // activity is a separate edge). The most-recent transport's carried
    // Device1 name shares this notifier with connection state.
    const ConnectionState newConn =
        transportActiveByPath_.isEmpty() ? Disconnected : Connected;
    QString newDeviceName;
    if (newConn == Connected)
        newDeviceName = bluezDeviceLabel(
            devicePropertiesByPath_.value(transportDeviceByPath_.value(transportPath_)));
    if (newConn != connectionState_ || newDeviceName != deviceName_) {
        connectionState_ = newConn;
        deviceName_ = newDeviceName;
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
    const bool replacingPlayer = playerPath_ != path;
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
    const PlaybackState oldPlaybackState = playbackState_;

    if (resetBeforeApply) {
        playbackState_ = Stopped;
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

        playbackState_ = newState;
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
    if (oldPlaybackState != playbackState_)
        emit playbackStateChanged();
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

    bus_.call(msg, QDBus::NoBlock);
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
