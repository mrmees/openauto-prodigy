#pragma once

#include "core/plugin/IPlugin.hpp"
#include <QObject>
#include <QString>

class QQmlContext;

namespace oap {

class IHostContext;

namespace plugins {

class FolderModel;
class MediaArtProvider;
class PlaybackEngine;
class PlayQueue;

/// Local media player: folder-browse playback of USB / ~/Music files.
/// Composition of PlaybackEngine (transport + PCM tap into AudioService),
/// PlayQueue (order/shuffle/repeat) and FolderModel (browse). Feeds
/// MediaStatusService as source "MediaPlayer" via main.cpp wiring.
/// Spec: docs/superpowers/specs/2026-07-08-media-player-design.md
class MediaPlayerPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)

    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY metadataChanged)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY metadataChanged)
    Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY metadataChanged)
    Q_PROPERTY(qint64 trackPosition READ trackPosition NOTIFY progressUpdated)
    Q_PROPERTY(qint64 trackDuration READ trackDuration NOTIFY progressUpdated)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY metadataChanged)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY modesChanged)
    Q_PROPERTY(int repeatMode READ repeatMode NOTIFY modesChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY hasTrackChanged)
    Q_PROPERTY(QObject* folderModel READ folderModelObject CONSTANT)

public:
    explicit MediaPlayerPlugin(QObject* parent = nullptr);
    ~MediaPlayerPlugin() override;

    // IPlugin — Identity
    QString id() const override { return QStringLiteral("org.openauto.media-player"); }
    QString name() const override { return QStringLiteral("Media Player"); }
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
    QUrl iconSource() const override { return {}; }
    QString iconText() const override { return QString(QChar(0xe030)); }  // library_music
    QStringList requiredServices() const override { return {}; }

    /// Non-owning; the QML engine owns the provider (main.cpp registers it).
    void setArtProvider(MediaArtProvider* provider) { artProvider_ = provider; }

    // Properties
    int playbackState() const;
    bool isPlaying() const { return playbackState() == 1; }
    QString trackTitle() const;
    QString trackArtist() const;
    QString trackAlbum() const;
    qint64 trackPosition() const;
    qint64 trackDuration() const;
    QString artUrl() const;
    bool shuffle() const;
    int repeatMode() const;
    bool hasTrack() const { return hasTrack_; }
    QObject* folderModelObject() const;

    // Controls (QML + main.cpp callback routing)
    Q_INVOKABLE void playFileFromFolder(const QString& path);
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void pauseIfPlaying();
    Q_INVOKABLE void seekTo(qint64 positionMs);
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void refreshSources();

signals:
    void playbackStateChanged();
    void metadataChanged();
    void progressChanged(qint64 positionMs, qint64 durationMs);
    void progressUpdated();   ///< argument-free companion for Q_PROPERTY NOTIFY
    void modesChanged();
    void hasTrackChanged();
    void playbackStarted();   ///< false->true playing edge, for pause-others wiring

private:
    void setHasTrack(bool has);
    void saveState();
    void restoreState();

    IHostContext* hostContext_ = nullptr;
    PlaybackEngine* engine_ = nullptr;
    PlayQueue* queue_ = nullptr;
    FolderModel* folderModel_ = nullptr;
    MediaArtProvider* artProvider_ = nullptr;  // non-owning
    bool hasTrack_ = false;
    bool wasPlaying_ = false;
    int consecutiveErrors_ = 0;
    bool restoring_ = false;
    bool shuttingDown_ = false;  // gate: engine_->stop() in shutdown() fires a
                                 // stopped edge whose save would clobber the
                                 // just-saved position with 0
};

} // namespace plugins
} // namespace oap
