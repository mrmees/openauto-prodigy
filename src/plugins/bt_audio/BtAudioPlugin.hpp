#pragma once

#include "core/plugin/IPlugin.hpp"
#include <QObject>
#include <QString>
#include <QDBusObjectPath>

class QQmlContext;
class QDBusServiceWatcher;

namespace oap {

class IHostContext;

namespace plugins {

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
    Q_PROPERTY(int trackDuration READ trackDuration NOTIFY metadataChanged)
    Q_PROPERTY(int trackPosition READ trackPosition NOTIFY positionChanged)
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
    ~BtAudioPlugin() override;

    // IPlugin — Identity
    QString id() const override { return QStringLiteral("org.openauto.bt-audio"); }
    QString name() const override { return QStringLiteral("Bluetooth Audio"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    int apiVersion() const override { return 1; }

    // IPlugin — Lifecycle
    bool initialize(IHostContext* context) override;
    void shutdown() override;

    // IPlugin — Activation
    void onActivated(QQmlContext* context) override;
    void onDeactivated() override;

    // IPlugin — UI
    QUrl qmlComponent() const override;
    QUrl iconSource() const override;
    QUrl settingsComponent() const override { return {}; }

    // IPlugin — Capabilities
    QStringList requiredServices() const override { return {}; }
    bool wantsFullscreen() const override { return false; }

    // Properties
    int connectionState() const { return connectionState_; }
    int playbackState() const { return playbackState_; }
    QString trackTitle() const { return trackTitle_; }
    QString trackArtist() const { return trackArtist_; }
    QString trackAlbum() const { return trackAlbum_; }
    int trackDuration() const { return trackDuration_; }
    int trackPosition() const { return trackPosition_; }
    QString deviceName() const { return deviceName_; }

    // Playback controls (invokable from QML)
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

signals:
    void connectionStateChanged();
    void playbackStateChanged();
    void metadataChanged();
    void positionChanged();

private:
    void startDBusMonitoring();
    void stopDBusMonitoring();
    void scanExistingObjects();
    void onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated);
    void updateTransportState(const QString& state);
    void updatePlayerProperties(const QVariantMap& props);
    void sendPlayerCommand(const QString& command);

    IHostContext* hostContext_ = nullptr;
    QDBusServiceWatcher* bluezWatcher_ = nullptr;
    bool monitoring_ = false;

    ConnectionState connectionState_ = Disconnected;
    PlaybackState playbackState_ = Stopped;

    QString trackTitle_;
    QString trackArtist_;
    QString trackAlbum_;
    int trackDuration_ = 0;   // milliseconds
    int trackPosition_ = 0;   // milliseconds
    QString deviceName_;

    // D-Bus object paths for the active transport and player
    QString transportPath_;
    QString playerPath_;
};

} // namespace plugins
} // namespace oap
