#include "MediaPlayerPlugin.hpp"

#include <QDir>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QStorageInfo>
#include <QUrl>

#include "FolderModel.hpp"
#include "MediaArtProvider.hpp"
#include "PlaybackEngine.hpp"
#include "PlayQueue.hpp"
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

    engine_->setAudioService(context->audioService());
    // Same EQ attach pattern as AndroidAutoPlugin.cpp:51 — the concrete
    // service exposes engineForStream(); local playback shares the Media EQ.
    if (auto* eqService = dynamic_cast<oap::EqualizerService*>(context->equalizerService()))
        engine_->setEqEngine(eqService->engineForStream(oap::StreamId::Media));

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

    refreshSources();
    restoreState();
    return true;
}

void MediaPlayerPlugin::shutdown() {
    policy_.onShutdownBegan();
    saveState();  // must precede the stop — saveState reads engine position
    // Fully release the PipeWire stream now: AudioService is an earlier app
    // child and dies first at teardown, so leaving the release to
    // ~PlaybackEngine ran use-after-free on every clean quit.
    if (engine_) engine_->releaseAudioResources();
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
    queue_->setTracks(files, idx);
    setHasTrack(true);
    startTrack(path);
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
    QVector<QPair<QString, QString>> roots;

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
        if (QDir(dir).exists())
            roots.append({QFileInfo(dir).fileName(), dir});
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
        QString label = vol.displayName();
        if (label.isEmpty() || label == mount)
            label = QFileInfo(mount).fileName();
        roots.append({label, mount});
    }

    folderModel_->setRoots(roots);
}

void MediaPlayerPlugin::saveState() {
    if (!hostContext_ || !hostContext_->configService()) return;
    auto* cfg = hostContext_->configService();
    cfg->setPluginValue(kPluginId, QStringLiteral("last_queue"), queue_->tracks());
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
    // Files may have vanished (USB stick removed) — keep only what exists.
    QStringList existing;
    const QString savedCurrent =
        saved.value(cfg->pluginValue(kPluginId, QStringLiteral("last_index")).toInt());
    for (const QString& p : saved)
        if (QFileInfo::exists(p)) existing << p;
    if (existing.isEmpty()) return;
    int startIdx = existing.indexOf(savedCurrent);
    if (startIdx < 0) startIdx = 0;
    queue_->setTracks(existing, startIdx);
    setHasTrack(true);
    // Restore PAUSED at the saved position — nothing auto-plays on boot
    // (spec §10, Matthew's explicit call). One tap on play resumes.
    const qint64 pos = cfg->pluginValue(kPluginId, QStringLiteral("last_position_ms")).toLongLong();
    policy_.onRestoreBegan();
    engine_->restorePaused(queue_->currentTrack(), pos);
}

} // namespace plugins
} // namespace oap
