#pragma once

#include "core/plugin/IPlugin.hpp"
#include "core/plugin/PluginDiscovery.hpp"
#include "core/widget/WidgetTypes.hpp"
// Pulls Q_DECLARE_METATYPE(oap::InterfaceMap) — the SAME underlying
// QMap<QString,QVariantMap> that BtInterfaceMap aliases below. openauto-core
// aggregates every class's MOC into one translation unit, and moc_BtAudioPlugin
// is compiled before moc_TelephonyClient (alphabetical): unless the explicit
// specialization is visible here, this slot's map parameter implicitly
// instantiates the built-in QMap metatype first and TelephonyClient's
// declaration becomes a "specialization of QMetaTypeId after instantiation"
// error. Reusing the existing specialization (per Task-1 resolution) avoids a
// duplicate Q_DECLARE_METATYPE for the identical type.
#include "core/services/TelephonyClient.hpp"
#include <QObject>
#include <QString>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QMap>
#include <QVector>
#include <QVariantMap>

class QQmlContext;
class QDBusServiceWatcher;

namespace oap {

class IHostContext;

namespace plugins {

class BtAudioTap;  // BT A2DP loopback tap (Task 7) — owned + wired in initialize()

/// BlueZ ObjectManager InterfacesAdded carries a{sa{sv}}; QtDBus refuses to
/// deliver it into a QVariantMap slot, so the connect fails at runtime and
/// hot-plug goes silently deaf (same fix family as UsbInterfaceMap /
/// BluezInterfaceMap, bench 2026-07-10). Its metatype is the shared
/// QMap<QString,QVariantMap> already declared via TelephonyClient's
/// Q_DECLARE_METATYPE(oap::InterfaceMap); re-registered in startDBusMonitoring().
using BtInterfaceMap = QMap<QString, QVariantMap>;
using BtManagedObjectMap = QMap<QString, BtInterfaceMap>;

/// Bluetooth A2DP audio sink plugin.
///
/// PipeWire + BlueZ handle A2DP endpoint negotiation and codec decode.
/// This plugin provides:
///   - UI for track metadata (AVRCP)
///   - Playback controls (play/pause/next/prev via AVRCP)
///   - Connection state monitoring via BlueZ D-Bus
///
/// D-Bus interfaces used:
///   org.bluez.MediaTransport1  — A2DP connection state
///   org.bluez.MediaPlayer1     — AVRCP metadata + playback control
///   org.freedesktop.DBus.ObjectManager — interface add/remove signals
class BtAudioPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)

    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY metadataChanged)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY metadataChanged)
    Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY metadataChanged)
    Q_PROPERTY(qint64 trackDuration READ trackDuration NOTIFY durationChanged)
    Q_PROPERTY(qint64 trackPosition READ trackPosition NOTIFY positionChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY connectionStateChanged)

public:
    enum ConnectionState {
        Disconnected = 0,
        Connected
    };
    Q_ENUM(ConnectionState)

    enum PlaybackState {
        Stopped = 0,
        Playing,
        Paused
    };
    Q_ENUM(PlaybackState)

    explicit BtAudioPlugin(QObject* parent = nullptr);
    BtAudioPlugin(const QDBusConnection& bus, QObject* parent = nullptr);
    ~BtAudioPlugin() override;

    // IPlugin — Identity
    QString id() const override { return QStringLiteral("org.openauto.bt-audio"); }
    QString name() const override { return QStringLiteral("Bluetooth Audio"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    int apiVersion() const override { return PluginDiscovery::HOST_API_VERSION; }

    // IPlugin — Lifecycle
    bool initialize(IHostContext* context) override;
    void shutdown() override;

    // IPlugin — Activation
    void onActivated(QQmlContext* context) override;
    void onDeactivated() override;

    // IPlugin — UI
    QUrl qmlComponent() const override;
    QUrl iconSource() const override;
    QString iconText() const override { return QString(QChar(0xf032)); }  // media_bluetooth_on
    QUrl settingsComponent() const override { return {}; }

    // IPlugin — Capabilities
    QStringList requiredServices() const override { return {}; }
    bool wantsFullscreen() const override { return false; }

    // IPlugin — Widgets
    QList<oap::WidgetDescriptor> widgetDescriptors() const override;

    // Properties
    int connectionState() const { return connectionState_; }
    int playbackState() const { return playbackState_; }
    QString trackTitle() const { return trackTitle_; }
    QString trackArtist() const { return trackArtist_; }
    QString trackAlbum() const { return trackAlbum_; }
    qint64 trackDuration() const { return trackDuration_; }
    qint64 trackPosition() const { return trackPosition_; }
    bool hasTrackPosition() const { return trackPositionKnown_; }
    QString deviceName() const { return deviceName_; }

    /// True iff the tracked A2DP transport's MediaTransport1.State == "active".
    /// This is audio activity, NOT interface presence: a connected-but-silent
    /// phone leaves the transport idle/pending, which reads false here even
    /// though connectionState() reports Connected. The BT loopback tap (Task 7)
    /// grabs audio focus off this edge, so a false positive would mute Android
    /// Auto while the phone is silent — precision matters.
    bool transportActive() const { return transportActive_; }

    /// Apply one complete ObjectManager snapshot. Production reaches this
    /// through the asynchronous GetManagedObjects reply; tests use it to pin
    /// startup adoption without a live bluetoothd.
    void applyManagedObjectsSnapshot(const BtManagedObjectMap& objects);

    // Playback controls (invokable from QML)
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

signals:
    void connectionStateChanged();
    void playbackStateChanged();
    void metadataChanged();
    void durationChanged();
    void positionChanged();
    /// Coherent millisecond snapshot emitted once after a delivered property
    /// batch changes duration, position, or position validity.
    void progressChanged(qint64 positionMs, qint64 durationMs);
    /// Edge-only: emitted just when the tracked transport's audio activity
    /// flips (idle/pending/removed/BlueZ-loss -> false, active -> true).
    void transportActiveChanged(bool active);

private slots:
    // D-Bus signal handlers — MUST be slots or every string-based bus.connect()
    // in startDBusMonitoring() fails at startup and the plugin runs blind.
    void onInterfacesAdded(const QDBusObjectPath& path, const BtInterfaceMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    // Sender path arrives in the trailing QDBusMessage (UsbMediaWatcher
    // onDrivePropertiesChanged pattern); filtered against transportPath_/
    // playerPath_ so a foreign BlueZ object's update cannot stomp our state.
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated, const QDBusMessage& message);
    // BlueZ vanished from the bus (QDBusServiceWatcher::serviceUnregistered).
    // A slot so the meta-object test seam can drive it without a live bus.
    void onBluezServiceUnregistered();

private:
    void startDBusMonitoring();
    void stopDBusMonitoring();
    void scanExistingObjects();
    void finishExistingObjectScan(const QDBusMessage& reply, quint64 requestEpoch);
    void applyInterfaces(const QString& path, const BtInterfaceMap& interfaces);
    void adoptTransport(const QString& path, const QVariantMap& props);
    void adoptDevice(const QString& path, const QVariantMap& props);
    // Record one transport's audio activity (State=="active") under its own
    // path, then recompute the aggregate connection/activity edges.
    void updateTransportState(const QString& path, const QString& state);
    // Derive connectionState_ (Connected while ANY transport exists) and the
    // audio-activity edge (true while ANY tracked transport is active) from
    // transportActiveByPath_, emitting only on real changes.
    void recomputeTransportState();
    void adoptPlayer(const QString& path, const QVariantMap& props);
    void updatePlayerProperties(const QVariantMap& props, bool resetBeforeApply = false);
    void applyPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                const QStringList& invalidated, const QDBusMessage& message);
    enum class PendingEventKind { InterfacesAdded, InterfacesRemoved, PropertiesChanged };
    struct PendingDbusEvent {
        PendingEventKind kind = PendingEventKind::PropertiesChanged;
        QString path;
        BtInterfaceMap addedInterfaces;
        QStringList removedInterfaces;
        QString interface;
        QVariantMap changed;
        QStringList invalidated;
    };
    static void applyEventToObjects(BtManagedObjectMap& objects,
                                    const PendingDbusEvent& event);
    void replayPendingEvents(BtManagedObjectMap& objects);
    void sendPlayerCommand(const QString& command);
    // Edge-emits transportActiveChanged only when the value actually flips.
    void setTransportActive(bool active);

    IHostContext* hostContext_ = nullptr;
    QDBusConnection bus_;
    QDBusServiceWatcher* bluezWatcher_ = nullptr;
    bool monitoring_ = false;
    bool scanInFlight_ = false;
    bool scanPending_ = false;
    quint64 bluezServiceEpoch_ = 0;
    QVector<PendingDbusEvent> pendingEventJournal_;
    BtManagedObjectMap lastKnownObjects_;

    // BT A2DP loopback tap — non-owning raw pointer; parented to this QObject.
    // Null when PipeWire is down or the concrete services don't resolve.
    BtAudioTap* tap_ = nullptr;

    ConnectionState connectionState_ = Disconnected;
    PlaybackState playbackState_ = Stopped;
    bool transportActive_ = false;

    QString trackTitle_;
    QString trackArtist_;
    QString trackAlbum_;
    qint64 trackDuration_ = 0;   // milliseconds; 0 = unknown
    qint64 trackPosition_ = 0;   // milliseconds; zero-safe UI fallback
    bool trackPositionKnown_ = false;
    QString deviceName_;

    // Per-transport audio activity: path -> (MediaTransport1.State == "active").
    // A second, idle phone must not force the aggregate edge false while the
    // first is still playing, so activity is tracked per transport and the edge
    // derives from "ANY tracked transport active" (not last-writer-wins).
    QMap<QString, bool> transportActiveByPath_;
    QMap<QString, QString> transportDeviceByPath_;
    // Authoritative carried Device1 properties. PropertiesChanged deltas are
    // merged into this map so Alias/Name precedence never depends on which
    // individual key happened to arrive in the latest signal.
    QMap<QString, QVariantMap> devicePropertiesByPath_;

    // D-Bus object paths: transportPath_ is the MOST-RECENT transport (used for
    // device-name display only); playerPath_ is the tracked AVRCP player.
    QString transportPath_;
    QString playerPath_;
};

} // namespace plugins
} // namespace oap
