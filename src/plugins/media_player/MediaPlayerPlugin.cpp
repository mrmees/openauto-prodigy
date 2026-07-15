#include "MediaPlayerPlugin.hpp"

#include <QDir>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QSet>
#include <QStorageInfo>
#include <QUrl>

#include <algorithm>
#include <utility>

#include "FolderModel.hpp"
#include "MediaArtProvider.hpp"
#include "MediaLibrary.hpp"
#include "MediaScanner.hpp"
#include "PlaybackEngine.hpp"
#include "PlayQueue.hpp"
#include "UsbMediaWatcher.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/EqualizerService.hpp"
#include "core/services/IConfigService.hpp"
#include "core/services/INotificationService.hpp"

Q_LOGGING_CATEGORY(lcMediaPlayerPlugin, "oap.mediaplayer.plugin")

namespace {
const QString kPluginId = QStringLiteral("org.openauto.media-player");
} // namespace

namespace oap {
namespace plugins {

MediaPlayerPlugin::MediaPlayerPlugin(QObject* parent) : QObject(parent) {}
MediaPlayerPlugin::~MediaPlayerPlugin() = default;

bool MediaPlayerPlugin::initialize(IHostContext* context) {
    hostContext_ = context;
    engine_ = new PlaybackEngine(this);
    queue_ = new PlayQueue(this);
    folderModel_ = new FolderModel(this);
    library_ = new MediaLibrary(this);
    scanner_ = new MediaScanner(this);

    engine_->setAudioService(context->audioService());
    // Acquire a dedicated Media-curve EQ engine instance (fanned out from the
    // shared Media gains — local playback no longer shares one engine with AA
    // media). Released in shutdown() after PlaybackEngine tears down its stream
    // (RT ordering contract §4.4).
    if (auto* eqService = dynamic_cast<oap::EqualizerService*>(context->equalizerService())) {
        eqService_ = eqService;
        eqEngine_ = eqService->acquireEngine(oap::StreamId::Media, 48000.0f, 2);
        engine_->setEqEngine(eqEngine_);
    }

    // Auto-advance on track end. A track that "finishes" without ever
    // producing audio is unplayable in disguise: FFmpeg misdetects random
    // bytes as MP3 with zero decodable frames — load "succeeds", EndOfMedia
    // fires instantly, and no error is ever emitted (bench 2026-07-09 row
    // 12: three garbage files walked the queue silently, no skip, no toast).
    connect(engine_, &PlaybackEngine::trackFinished, this, [this]() {
        if (policy_.onTrackFinished() == PlaybackPolicy::TrackEndVerdict::Unplayable) {
            handleUnplayable(QStringLiteral("track ended with no audio (misdetected format?)"));
            return;
        }
        if (queue_->advance(false))
            startTrack(queue_->currentTrack());
        // else: end of queue, repeat off — remain stopped on the last track.
    });

    connect(engine_, &PlaybackEngine::errorOccurred, this,
            [this](const QString& msg) { handleUnplayable(msg); });

    connect(engine_, &PlaybackEngine::playbackStateChanged, this, [this]() {
        const bool playing = (engine_->playbackState() == 1);
        if (playing && !wasPlaying_)
            emit playbackStarted();
        wasPlaying_ = playing;
        emit playbackStateChanged();
        // Persist on every play/pause/stop/track edge — a head unit's normal
        // death is a power cut, so waiting for shutdown() loses everything
        // (bench 2026-07-09 row 11). Playing edges persist the new queue and
        // index; position stays from the last pause (start-of-track on cut).
        if (policy_.saveAllowed()) saveState();
    });

    connect(engine_, &PlaybackEngine::metadataChanged, this, [this]() {
        if (artProvider_)
            artProvider_->setCurrentArt(engine_->coverArt());
        emit metadataChanged();
    });

    connect(engine_, &PlaybackEngine::progressChanged, this,
            [this](qint64 pos, qint64 dur) {
        policy_.onProgress(pos);
        emit progressChanged(pos, dur);
        emit progressUpdated();
    });

    connect(queue_, &PlayQueue::shuffleChanged, this, &MediaPlayerPlugin::modesChanged);
    connect(queue_, &PlayQueue::repeatModeChanged, this, &MediaPlayerPlugin::modesChanged);
    // The queue path list only changes on setTracks()/clear(); mark dirty so
    // saveState() writes the (potentially huge) last_queue only when it moved.
    connect(queue_, &PlayQueue::queueChanged, this, [this]() { queueDirty_ = true; });

    // Scan results land here on the owner thread. Filter to keys still present
    // in currentRoots_ so a stale in-flight result cannot resurrect a yanked
    // volume (belt to the scanner's coalescing braces — Codex P1).
    connect(scanner_, &MediaScanner::finished, this,
            [this](QVector<MediaTrackRecord> records) {
        QSet<QString> liveKeys;
        for (const MediaScanner::Root& r : currentRoots_) liveKeys.insert(r.key);
        records.erase(std::remove_if(records.begin(), records.end(),
                          [&](const MediaTrackRecord& rec) {
                              return !liveKeys.contains(rec.volumeKey);
                          }),
                      records.end());
        library_->setTracks(std::move(records));
    });
    // BOTH edges: the QML "Scanning…" indicator turns on at scan start too.
    connect(scanner_, &MediaScanner::scanningChanged, this,
            &MediaPlayerPlugin::libraryScanningChanged);
    // libraryTrackCount stays true after removeVolume() as well (Codex P2).
    connect(library_, &MediaLibrary::libraryChanged, this,
            &MediaPlayerPlugin::libraryChanged);

    // udisks2 hot-plug watcher (Task 6). Created after the scanner; start()
    // scans already-mounted volumes and (on a host with udisks2) emits
    // volumeMounted synchronously — which upgrades mountKeys_ to uuid keys.
    watcher_ = new UsbMediaWatcher(this);
    connect(watcher_, &UsbMediaWatcher::volumeMounted, this,
            [this](const QString& mount, const QString& /*label*/, const QString& uuid) {
        // Key captured at mount time — never recomputed from a dead mount
        // later (Codex P1). Store BEFORE refreshSources() rescans.
        mountKeys_[mount] = uuid.isEmpty()
            ? MediaScanner::rootKeyForPath(mount)
            : (QStringLiteral("uuid-") + uuid);
        refreshSources();
        // A stick carrying the saved queue's current track may have just
        // finished mounting — finish the boot restore that initialize() could
        // not (Codex gate P1). No-op unless a restore is pending.
        retryPendingRestore();
    });
    connect(watcher_, &UsbMediaWatcher::volumeRemoved, this, [this](const QString& mount) {
        // Eject-initiated removal already ran purgeVolume() in ejectVolume();
        // just clear the guard. A real yank runs the shared purge.
        if (ejectingMounts_.remove(mount)) return;
        purgeVolume(mount);
    });
    connect(watcher_, &UsbMediaWatcher::ejectCompleted, this,
            [this](const QString& mount, bool ok) {
        if (ok) return;   // success removal is handled by volumeRemoved above
        // Unmount failed (drive busy). ejectVolume() already ran purgeVolume()
        // BEFORE the unmount, tearing down this source's library rows, queue
        // tracks, mountKeys_ entry, and (if it held the current track) playback.
        // All we do here is drop the eject guard and RE-LIST the still-mounted
        // volume via refreshSources() — it reappears as a source and rescans.
        // The prior queue, current index, playback state, and mountKeys_ entry
        // are NOT restored; the source returns emptied of its old session.
        // (Full queue/playback restore-on-eject-failure is wishlisted.)
        ejectingMounts_.remove(mount);
        refreshSources();
        if (hostContext_ && hostContext_->notificationService())
            hostContext_->notificationService()->post({
                {"kind", "toast"},
                {"message", QStringLiteral("Media Player: Eject failed — drive still in use")},
                {"sourcePluginId", kPluginId}
            });
    });
    watcher_->start();

    refreshSources();
    restoreState();
    return true;
}

void MediaPlayerPlugin::shutdown() {
    policy_.onShutdownBegan();
    if (watcher_) watcher_->stop();
    saveState();  // must precede the stop — saveState reads engine position
    // Fully release the PipeWire stream now: AudioService is an earlier app
    // child and dies first at teardown, so leaving the release to
    // ~PlaybackEngine ran use-after-free on every clean quit.
    if (engine_) engine_->releaseAudioResources();
    // Stream is gone — now release the EQ engine (RT ordering contract §4.4:
    // the stream must be destroyed before its engine pointer is freed).
    if (eqService_ && eqEngine_) {
        eqService_->releaseEngine(eqEngine_);
        eqEngine_ = nullptr;
    }
}

void MediaPlayerPlugin::onActivated(QQmlContext* context) {
    if (!context) return;
    context->setContextProperty("MediaPlayerPlugin", this);
    refreshSources();  // volumes may have been (un)mounted since last visit
}

void MediaPlayerPlugin::onDeactivated() {
    // Playback continues in the background; child context destroyed by host.
}

QUrl MediaPlayerPlugin::qmlComponent() const {
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/MediaPlayerView.qml"));
}

int MediaPlayerPlugin::playbackState() const { return engine_ ? engine_->playbackState() : 0; }
QString MediaPlayerPlugin::trackTitle() const { return engine_ ? engine_->title() : QString(); }
QString MediaPlayerPlugin::trackArtist() const { return engine_ ? engine_->artist() : QString(); }
QString MediaPlayerPlugin::trackAlbum() const { return engine_ ? engine_->album() : QString(); }
qint64 MediaPlayerPlugin::trackPosition() const { return engine_ ? engine_->position() : 0; }
qint64 MediaPlayerPlugin::trackDuration() const { return engine_ ? engine_->duration() : 0; }
QString MediaPlayerPlugin::artUrl() const { return artProvider_ ? artProvider_->currentUrl() : QString(); }
bool MediaPlayerPlugin::shuffle() const { return queue_ && queue_->shuffle(); }
int MediaPlayerPlugin::repeatMode() const { return queue_ ? queue_->repeatMode() : 0; }
QObject* MediaPlayerPlugin::folderModelObject() const { return folderModel_; }
QObject* MediaPlayerPlugin::artistsModel() const { return library_ ? library_->artistsModel() : nullptr; }
QObject* MediaPlayerPlugin::albumsModel() const { return library_ ? library_->albumsModel() : nullptr; }
QObject* MediaPlayerPlugin::tracksModel() const { return library_ ? library_->tracksModel() : nullptr; }
bool MediaPlayerPlugin::libraryScanning() const { return scanner_ && scanner_->scanning(); }
int MediaPlayerPlugin::libraryTrackCount() const { return library_ ? library_->trackCount() : 0; }

void MediaPlayerPlugin::setHasTrack(bool has) {
    if (hasTrack_ == has) return;
    hasTrack_ = has;
    emit hasTrackChanged();
}

void MediaPlayerPlugin::startTrack(const QString& path) {
    policy_.onTrackStarted();
    engine_->playFile(path);
}

// Unplayable-file policy (spec §11): skip forward; after 3 consecutive
// failures stop and toast (a dead USB stick must not machine-gun skips).
// Reached from errorOccurred AND from zero-progress trackFinished.
void MediaPlayerPlugin::handleUnplayable(const QString& reason) {
    // USB-yank detection (design §9, re-run P2): BEFORE consuming any strike,
    // check whether the failing current track lives under a known mount that
    // has vanished from the mount table. A mount DIRECTORY usually survives an
    // unmount, so QFileInfo::exists is the wrong test — compare against the
    // live mount table. On a hit this is a volume removal, not a decode error:
    // purge and return, consuming no strike (so a third strike cannot take the
    // StopAndNotify path). Idempotent with the watcher signal via mountKeys_.take().
    const QString cur = queue_ ? queue_->currentTrack() : QString();
    if (!cur.isEmpty()) {
        QString vanishedMount;
        for (auto it = mountKeys_.constBegin(); it != mountKeys_.constEnd(); ++it) {
            const QString mount = it.key();
            if (!cur.startsWith(mount + QLatin1Char('/'))) continue;
            bool stillMounted = false;
            for (const QStorageInfo& vol : QStorageInfo::mountedVolumes()) {
                if (vol.rootPath() == mount) { stillMounted = true; break; }
            }
            if (!stillMounted) vanishedMount = mount;
            break;   // the track matched this mount — done either way
        }
        if (!vanishedMount.isEmpty()) {
            qCWarning(lcMediaPlayerPlugin)
                << "current track's volume vanished — treating as yank, not a decode error:"
                << vanishedMount;
            purgeVolume(vanishedMount);
            return;
        }
    }

    switch (policy_.onUnplayableEdge()) {
    case PlaybackPolicy::UnplayableVerdict::StayStopped:
        qCWarning(lcMediaPlayerPlugin) << "restored track failed to load; staying stopped:" << reason;
        return;  // no auto-skip at boot — nothing may auto-play (spec §10)
    case PlaybackPolicy::UnplayableVerdict::StopAndNotify:
        engine_->stop();
        if (hostContext_ && hostContext_->notificationService())
            hostContext_->notificationService()->post({
                {"kind", "toast"},
                {"message", QStringLiteral("Media Player: Playback stopped: %1 unplayable files in a row")
                                .arg(policy_.consecutiveErrors())},
                {"sourcePluginId", kPluginId}
            });
        policy_.resetStrikes();
        return;
    case PlaybackPolicy::UnplayableVerdict::SkipNext:
        qCWarning(lcMediaPlayerPlugin) << "skipping unplayable file:" << reason;
        if (queue_->advance(true))          // manual semantics: never re-loop one broken file
            startTrack(queue_->currentTrack());
        return;
    }
}

void MediaPlayerPlugin::playFileFromFolder(const QString& path) {
    policy_.onUserAction();  // user action supersedes restore state
    const QStringList files = folderModel_->audioFilesInCurrentDir();
    const int idx = files.indexOf(path);
    if (idx < 0) return;
    policy_.onNewQueue();
    clearPendingRestore();  // user chose a queue — kill any pending boot restore
    queue_->setTracks(files, idx);
    setHasTrack(true);
    startTrack(path);
}

void MediaPlayerPlugin::playAlbum(const QString& albumKey, int startIndex) {
    // Validate BEFORE mutating any transport state (Codex P2): resolve the
    // paths, bail on an empty/unknown album, then clamp the tap index.
    const QStringList paths = library_->trackPathsForAlbum(albumKey);
    if (paths.isEmpty()) return;
    const int start = qBound(0, startIndex, paths.size() - 1);
    policy_.onUserAction();
    policy_.onNewQueue();
    clearPendingRestore();  // user chose a queue — kill any pending boot restore
    queue_->setTracks(paths, start);
    setHasTrack(true);
    startTrack(queue_->currentTrack());
}

void MediaPlayerPlugin::playAlbumFromPath(const QString& albumKey, const QString& path) {
    // Drill-down snapshots (LibraryAlbumsTab/LibraryArtistsTab) are QVariantList
    // copies; a rescan/yank can reorder the album between snapshot and tap, so
    // playing by the snapshot row index would hit the WRONG track (Codex gate
    // P2). Resolve the tapped PATH against the CURRENT album ordering instead.
    if (!library_) return;
    const int idx = library_->trackPathsForAlbum(albumKey).indexOf(path);
    if (idx < 0) return;   // stale row (track gone) — no state change
    playAlbum(albumKey, idx);   // exact existing guard sequence
}

void MediaPlayerPlugin::playAllTracks(int startIndex) {
    const QStringList paths = library_->allTrackPathsSorted();
    if (paths.isEmpty()) return;
    const int start = qBound(0, startIndex, paths.size() - 1);
    policy_.onUserAction();
    policy_.onNewQueue();
    clearPendingRestore();  // user chose a queue — kill any pending boot restore
    queue_->setTracks(paths, start);
    setHasTrack(true);
    startTrack(queue_->currentTrack());
}

QVariantList MediaPlayerPlugin::albumsForArtist(const QString& artistKey) const {
    return library_ ? library_->albumsForArtist(artistKey) : QVariantList();
}

QVariantList MediaPlayerPlugin::tracksForAlbum(const QString& albumKey) const {
    return library_ ? library_->tracksForAlbum(albumKey) : QVariantList();
}

void MediaPlayerPlugin::rescanLibrary() {
    // Recompute roots (picks up newly mounted volumes) and re-scan.
    refreshSources();
}

// Shared yank/eject cleanup (design §9). Runs for a real yank (volumeRemoved),
// the playback-error yank path, and the eject sequence. Idempotent: mountKeys_
// .take() means a second call for the same mount finds no key and no tracks.
void MediaPlayerPlugin::purgeVolume(const QString& mount) {
    // 1. Key captured at mount time; NEVER recomputed from a dead mount
    //    (Codex P1). Fallback for volumes that predate the watcher.
    QString key = mountKeys_.take(mount);
    if (key.isEmpty()) key = MediaScanner::rootKeyForPath(mount);

    // 2. Drop the volume's tracks from the library.
    if (library_) library_->removeVolume(key);

    // 3. Capture playback state BEFORE the purge mutates the queue.
    const bool wasPlaying = (engine_ && engine_->playbackState() == 1);
    const QString prefix = mount + QStringLiteral("/");
    const bool currentGone = queue_ && queue_->currentTrack().startsWith(prefix);

    // 4. Purge the queue (order-preserving; selects the survivor).
    if (queue_) queue_->removeTracksUnder(prefix);

    // 5. Recover playback per §9.
    if (currentGone) {
        if (queue_->count() == 0) {
            // No survivor to start: fully release the media source, not just
            // stop() — a bare stop() keeps the purged USB file open and can
            // fail the pending Unmount() with EBUSY (Codex gate re-run P1).
            engine_->unload();
            setHasTrack(false);
        } else if (wasPlaying) {
            // Mid-session playing yank: continue with what remains.
            policy_.onNewQueue();
            startTrack(queue_->currentTrack());
        } else {
            // Paused / stopped / restored-at-boot: the queue points at the
            // survivor but NOTHING auto-plays (stage-1 no-autoplay invariant).
            // unload() (not stop()) so the purged USB file is released before
            // the pending Unmount() — stop() alone leaves it open (Codex P1).
            engine_->unload();
        }
    }

    // 6. Refresh sources (drops the dead root, rescans survivors).
    refreshSources();
}

void MediaPlayerPlugin::ejectVolume(const QString& mountPath) {
    if (mountPath.isEmpty()) return;
    // 0. Validate the mount is udisks-managed BEFORE the disruptive purge (Codex
    //    gate re-run P2). Tapping eject on an fstab/NAS mount under /mnt would
    //    otherwise stop playback and clear the queue, then fail the unmount.
    //    isKnownMount() is a synchronous local lookup (no D-Bus) and returns
    //    false when the watcher is inactive — so this path never disrupts state.
    if (!watcher_ || !watcher_->isKnownMount(mountPath)) {
        if (hostContext_ && hostContext_->notificationService())
            hostContext_->notificationService()->post({
                {"kind", "toast"},
                {"message", QStringLiteral("Media Player: Eject not available for this source")},
                {"sourcePluginId", kPluginId}
            });
        return;
    }
    // 1. Guard the mount so purgeVolume()'s refresh cannot re-enumerate it.
    ejectingMounts_.insert(mountPath);
    // 2. Cleanup FIRST (stops playback if the current track lives here) so
    //    QMediaPlayer never holds the file open into Unmount().
    purgeVolume(mountPath);
    // 3. Ask the watcher to unmount (+ power off if supported). The signal
    //    handlers wired in initialize() drop the guard and, on failure, restore.
    if (watcher_) watcher_->ejectMount(mountPath);
}

void MediaPlayerPlugin::playPause() {
    policy_.onUserAction();  // user action supersedes restore state
    if (!hasTrack_) return;
    switch (engine_->playbackState()) {
    case 1:  engine_->pause(); break;
    case 2:  engine_->play(); break;
    default: startTrack(queue_->currentTrack()); break;  // stopped: restart
    }
}

void MediaPlayerPlugin::next() {
    policy_.onUserAction();  // user action supersedes restore state
    if (!hasTrack_) return;
    if (queue_->advance(true))
        startTrack(queue_->currentTrack());
}

void MediaPlayerPlugin::previous() {
    policy_.onUserAction();  // user action supersedes restore state
    if (!hasTrack_) return;
    // Classic head-unit behavior: >3 s into the track = restart it.
    if (engine_->position() > 3000) {
        engine_->seek(0);
    } else if (queue_->retreat()) {
        startTrack(queue_->currentTrack());
    } else {
        engine_->seek(0);
    }
}

void MediaPlayerPlugin::pauseIfPlaying() {
    if (engine_ && engine_->playbackState() == 1)
        engine_->pause();
}

void MediaPlayerPlugin::seekTo(qint64 positionMs) {
    if (hasTrack_) engine_->seek(positionMs);
}

void MediaPlayerPlugin::toggleShuffle() { queue_->setShuffle(!queue_->shuffle()); }

void MediaPlayerPlugin::cycleRepeat() {
    queue_->setRepeatMode((queue_->repeatMode() + 1) % 3);
}

void MediaPlayerPlugin::refreshSources() {
    // Build the canonical root set once (with scanner keys), then derive the
    // FolderModel's (label,path) view from it. currentRoots_ is captured so
    // Task 6 can rescan/purge per-root; each key is captured at ADD time and
    // never recomputed from a possibly-dead mount later (Codex P1).
    QVector<MediaScanner::Root> roots;

    // Canonical dedupe (Codex gate re-run P2): identical roots listed twice in
    // music_dirs, or a mount reached via BOTH config and volume enumeration,
    // would otherwise scan twice and duplicate every library row. Skip a root
    // whose canonical path already appeared. Exact-duplicate only — nested-root
    // precedence is deliberately out of scope (wishlisted).
    QSet<QString> seenCanonical;
    auto alreadySeen = [&seenCanonical](const QString& path) {
        const QString canon = QFileInfo(path).canonicalFilePath();
        if (canon.isEmpty()) return false;   // unresolvable — never dedupe blindly
        if (seenCanonical.contains(canon)) return true;
        seenCanonical.insert(canon);
        return false;
    };

    QStringList musicDirs;
    if (hostContext_ && hostContext_->configService()) {
        const QVariant v = hostContext_->configService()->pluginValue(
            kPluginId, QStringLiteral("music_dirs"));
        musicDirs = v.toStringList();
    }
    if (musicDirs.isEmpty())
        musicDirs << QStringLiteral("~/Music");
    for (QString dir : musicDirs) {
        if (dir.startsWith(QLatin1String("~/")))
            dir = QDir::homePath() + dir.mid(1);
        if (QDir(dir).exists() && !alreadySeen(dir))
            roots.append({QFileInfo(dir).fileName(), dir,
                          MediaScanner::rootKeyForPath(dir)});
    }

    // Already-mounted removable volumes (stage 1: snapshot; stage 2 adds
    // udisks2 hot-plug + automount).
    for (const QStorageInfo& vol : QStorageInfo::mountedVolumes()) {
        if (!vol.isValid() || !vol.isReady() || vol.isRoot()) continue;
        const QString mount = vol.rootPath();
        if (!mount.startsWith(QLatin1String("/media/"))
            && !mount.startsWith(QLatin1String("/run/media/"))
            && !mount.startsWith(QLatin1String("/mnt/")))
            continue;
        // Mid-eject: do NOT re-enumerate/rescan a still-mounted volume — that
        // would reopen its files and defeat the pre-unmount cleanup (Codex P1).
        if (ejectingMounts_.contains(mount)) continue;
        QString label = vol.displayName();
        if (label.isEmpty() || label == mount)
            label = QFileInfo(mount).fileName();
        // Prefer a udisks-derived key (Task 6's watcher); fall back to the path
        // hash for volumes that predate the watcher.
        const QString key = mountKeys_.value(mount, MediaScanner::rootKeyForPath(mount));
        // Skip a volume already added via config (or listed twice) — same
        // canonical path, one scan (Codex gate re-run P2).
        if (alreadySeen(mount)) continue;
        roots.append({label, mount, key});
    }

    currentRoots_ = roots;

    QVector<QPair<QString, QString>> folderRoots;
    folderRoots.reserve(roots.size());
    for (const MediaScanner::Root& r : roots)
        folderRoots.append({r.label, r.path});
    folderModel_->setRoots(folderRoots);

    // The scanner coalesces when busy, so this is always safe to fire.
    scanner_->scan(currentRoots_);
}

void MediaPlayerPlugin::saveState() {
    if (!hostContext_ || !hostContext_->configService()) return;
    auto* cfg = hostContext_->configService();
    // Scale guard (Codex P2): last_queue is the full path list — a
    // playAllTracks queue can be thousands of paths. It only changes on
    // setTracks()/clear(), so rewrite it ONLY when the queue actually moved.
    if (queueDirty_) {
        cfg->setPluginValue(kPluginId, QStringLiteral("last_queue"), queue_->tracks());
        queueDirty_ = false;
    }
    // Index/position/modes still persist on every edge — advance()/retreat()/
    // jumpTo() emit only currentChanged, so gating last_index on queueDirty_
    // would persist a stale index against the current queue.
    cfg->setPluginValue(kPluginId, QStringLiteral("last_index"), queue_->currentIndex());
    cfg->setPluginValue(kPluginId, QStringLiteral("last_position_ms"),
                        hasTrack_ ? engine_->position() : qint64(0));
    cfg->setPluginValue(kPluginId, QStringLiteral("shuffle"), queue_->shuffle());
    cfg->setPluginValue(kPluginId, QStringLiteral("repeat_mode"), queue_->repeatMode());
    cfg->save();
}

void MediaPlayerPlugin::restoreState() {
    if (!hostContext_ || !hostContext_->configService()) return;
    auto* cfg = hostContext_->configService();
    queue_->setShuffle(cfg->pluginValue(kPluginId, QStringLiteral("shuffle")).toBool());
    queue_->setRepeatMode(cfg->pluginValue(kPluginId, QStringLiteral("repeat_mode")).toInt());

    const QStringList saved = cfg->pluginValue(kPluginId, QStringLiteral("last_queue")).toStringList();
    if (saved.isEmpty()) return;
    const QString savedCurrent =
        saved.value(cfg->pluginValue(kPluginId, QStringLiteral("last_index")).toInt());
    const qint64 pos = cfg->pluginValue(kPluginId, QStringLiteral("last_position_ms")).toLongLong();

    // USB volumes mount ASYNCHRONOUSLY after initialize() runs this, so a queue
    // saved on an inserted-but-not-yet-mounted stick would be dropped below (or
    // a mixed queue would restore the wrong survivor). Whenever the SAVED
    // CURRENT path is absent right now, stash the RAW saved state and let
    // volumeMounted retry the restore once its stick appears (Codex gate P1).
    // Stashed regardless of whether the partial restore below proceeds.
    if (!QFileInfo::exists(savedCurrent)) {
        pendingRestoreQueue_ = saved;
        pendingRestoreCurrent_ = savedCurrent;
        pendingRestorePosMs_ = pos;
        hasPendingRestore_ = true;
    }

    // Files may have vanished (USB stick removed) — keep only what exists.
    QStringList existing;
    for (const QString& p : saved)
        if (QFileInfo::exists(p)) existing << p;
    if (existing.isEmpty()) return;
    int startIdx = existing.indexOf(savedCurrent);
    if (startIdx < 0) startIdx = 0;
    queue_->setTracks(existing, startIdx);
    setHasTrack(true);
    // Restore PAUSED at the saved position — nothing auto-plays on boot
    // (spec §10, Matthew's explicit call). One tap on play resumes.
    policy_.onRestoreBegan();
    // Reset the audibility watermark before EVERY restorePaused (Codex gate
    // re-run P1): the restore path bypasses startTrack(), which normally does
    // this. A prior partial restore's seek-echo can leave the watermark above
    // the audible floor, so a corrupt just-mounted track that "ends" silently
    // reads as playable and auto-advances into REAL playback — violating the
    // no-autoplay invariant (spec §10).
    policy_.onTrackStarted();
    engine_->restorePaused(queue_->currentTrack(), pos);
}

void MediaPlayerPlugin::retryPendingRestore() {
    if (!hasPendingRestore_) return;
    // Only while the boot restore still owns the transport: policy_.restoring()
    // is true from restorePaused() until the first user action (covers the
    // partial-restore case); !hasTrack_ covers the nothing-restored case. Once
    // the user has taken over, never clobber their choice (Codex gate P1).
    if (!(policy_.restoring() || !hasTrack_)) return;

    // Re-filter the RAW saved list against what exists now. Rebuild only once
    // the saved current is actually present; otherwise wait for the next mount.
    QStringList existing;
    for (const QString& p : std::as_const(pendingRestoreQueue_))
        if (QFileInfo::exists(p)) existing << p;
    const int idx = existing.indexOf(pendingRestoreCurrent_);
    if (idx < 0) return;

    // Rebuild exactly like restoreState() — restore PAUSED, nothing auto-plays.
    queue_->setTracks(existing, idx);
    setHasTrack(true);
    policy_.onRestoreBegan();
    // Reset the audibility watermark before restorePaused (Codex gate re-run
    // P1): this path bypasses startTrack() too, and a prior partial restore's
    // seek-echo can otherwise leave the watermark above the audible floor — a
    // corrupt just-mounted track would then read as playable and auto-advance
    // into REAL playback, violating the no-autoplay invariant (spec §10).
    policy_.onTrackStarted();
    engine_->restorePaused(queue_->currentTrack(), pendingRestorePosMs_);
    clearPendingRestore();
}

void MediaPlayerPlugin::clearPendingRestore() {
    hasPendingRestore_ = false;
    pendingRestoreQueue_.clear();
    pendingRestoreCurrent_.clear();
    pendingRestorePosMs_ = 0;
}

} // namespace plugins
} // namespace oap
