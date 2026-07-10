#pragma once

#include "core/plugin/IPlugin.hpp"
#include "PlaybackPolicy.hpp"
#include "MediaScanner.hpp"   // MediaScanner::Root (currentRoots_ member) + MediaLibrary
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class QQmlContext;

namespace oap {

class IHostContext;

namespace plugins {

class FolderModel;
class MediaArtProvider;
class MediaLibrary;
class PlaybackEngine;
class PlayQueue;
class UsbMediaWatcher;

/// Local media player: folder-browse playback of USB / ~/Music files.
/// Composition of PlaybackEngine (transport + PCM tap into AudioService),
/// PlayQueue (order/shuffle/repeat) and FolderModel (browse). Feeds
/// MediaStatusService as source "MediaPlayer" via main.cpp wiring.
/// Spec: docs/plans/2026-07-08-media-player-design.md
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
    // Library (Tasks 3/4) surfaces — models are stable QObjects created in
    // initialize() before QML reads them, hence CONSTANT.
    Q_PROPERTY(QObject* artistsModel READ artistsModel CONSTANT)
    Q_PROPERTY(QObject* albumsModel READ albumsModel CONSTANT)
    Q_PROPERTY(QObject* tracksModel READ tracksModel CONSTANT)
    Q_PROPERTY(bool libraryScanning READ libraryScanning NOTIFY libraryScanningChanged)
    Q_PROPERTY(int libraryTrackCount READ libraryTrackCount NOTIFY libraryChanged)

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
    QObject* artistsModel() const;
    QObject* albumsModel() const;
    QObject* tracksModel() const;
    bool libraryScanning() const;
    int libraryTrackCount() const;

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

    // Library playback + drill-down (Task 5). Guards validate BEFORE mutating
    // any transport state (Codex P2).
    Q_INVOKABLE void playAlbum(const QString& albumKey, int startIndex);
    /// Play an album track by PATH, not by a stale drill-down snapshot index:
    /// a rescan/yank can reorder the library between the QVariantList snapshot
    /// and the tap, so index N would play the WRONG track (Codex gate P2).
    /// Resolves path -> current album index, then runs the exact playAlbum()
    /// guard sequence; a path no longer in the album is a silent no-op.
    Q_INVOKABLE void playAlbumFromPath(const QString& albumKey, const QString& path);
    Q_INVOKABLE void playAllTracks(int startIndex);
    Q_INVOKABLE QVariantList albumsForArtist(const QString& artistKey) const;
    Q_INVOKABLE QVariantList tracksForAlbum(const QString& albumKey) const;
    Q_INVOKABLE void rescanLibrary();

    /// Safe-eject a removable volume (Task 6). Cleanup runs BEFORE the async
    /// unmount so QMediaPlayer never holds a file open into Unmount().
    Q_INVOKABLE void ejectVolume(const QString& mountPath);

signals:
    void playbackStateChanged();
    void metadataChanged();
    void progressChanged(qint64 positionMs, qint64 durationMs);
    void progressUpdated();   ///< argument-free companion for Q_PROPERTY NOTIFY
    void modesChanged();
    void hasTrackChanged();
    void playbackStarted();   ///< false->true playing edge, for pause-others wiring
    void libraryScanningChanged();   ///< both edges of a scan (Codex P1)
    void libraryChanged();           ///< library rebuilt (also after removeVolume, Codex P2)

private:
    void setHasTrack(bool has);
    void saveState();
    void restoreState();
    void retryPendingRestore();   ///< re-attempt a boot restore after a late USB mount (P1)
    void clearPendingRestore();   ///< drop stashed restore state (user took over)
    void startTrack(const QString& path);       ///< playFile + reset progress watermark
    void handleUnplayable(const QString& reason);  ///< spec §11 skip/stop policy
    /// Shared yank/eject cleanup (design §9): drop the volume from library +
    /// queue and recover playback. Runs for volumeRemoved, the playback-error
    /// yank path, AND the eject sequence. Idempotent via mountKeys_.take().
    void purgeVolume(const QString& mount);

    IHostContext* hostContext_ = nullptr;
    PlaybackEngine* engine_ = nullptr;
    PlayQueue* queue_ = nullptr;
    FolderModel* folderModel_ = nullptr;
    MediaLibrary* library_ = nullptr;
    MediaScanner* scanner_ = nullptr;
    UsbMediaWatcher* watcher_ = nullptr;   // udisks2 hot-plug + safe eject (Task 6)
    MediaArtProvider* artProvider_ = nullptr;  // non-owning
    bool hasTrack_ = false;
    bool wasPlaying_ = false;
    bool queueDirty_ = false;  // set from PlayQueue::queueChanged; gates ONLY the
                               // last_queue write in saveState() (Codex P2 scale guard)
    PlaybackPolicy policy_;  // extracted state machine; invariants locked
                             // by tests/test_media_playback_policy.cpp
    // Roots captured by refreshSources() so Task 6 can rescan/purge per-root;
    // keys captured at ADD time, never recomputed from a dead mount (Codex P1).
    QVector<MediaScanner::Root> currentRoots_;
    QHash<QString, QString> mountKeys_;  // mount path -> volume key (captured at mount time)
    // Mounts mid-eject: refreshSources() SKIPS these so the purge's rescan
    // cannot re-enumerate a still-mounted volume and reopen its files (Codex P1).
    QSet<QString> ejectingMounts_;
    // Boot-restore retry (Codex gate P1): USB volumes mount ASYNCHRONOUSLY
    // after initialize()'s restoreState(), so a queue saved on a not-yet-mounted
    // stick would be dropped (or a mixed queue would restore the wrong
    // survivor). Stash the RAW saved state whenever the saved current path is
    // absent at boot and re-attempt on each volumeMounted until it appears or
    // the user starts a new queue (cleared in the onNewQueue() paths).
    QStringList pendingRestoreQueue_;
    QString pendingRestoreCurrent_;
    qint64 pendingRestorePosMs_ = 0;
    bool hasPendingRestore_ = false;
};

} // namespace plugins
} // namespace oap
