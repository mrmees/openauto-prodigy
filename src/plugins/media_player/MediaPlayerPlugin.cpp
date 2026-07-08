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
constexpr int kMaxConsecutiveErrors = 3;
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

    // Auto-advance on track end.
    connect(engine_, &PlaybackEngine::trackFinished, this, [this]() {
        if (queue_->advance(false))
            engine_->playFile(queue_->currentTrack());
        // else: end of queue, repeat off — remain stopped on the last track.
    });

    // Unplayable-file policy (spec §11): skip forward; after 3 consecutive
    // failures stop and toast (a dead USB stick must not machine-gun skips).
    connect(engine_, &PlaybackEngine::errorOccurred, this, [this](const QString& msg) {
        if (restoring_) {
            restoring_ = false;
            qCWarning(lcMediaPlayerPlugin) << "restored track failed to load; staying stopped:" << msg;
            return;  // no auto-skip at boot — nothing may auto-play (spec §10)
        }
        ++consecutiveErrors_;
        if (consecutiveErrors_ >= kMaxConsecutiveErrors) {
            engine_->stop();
            if (hostContext_ && hostContext_->notificationService())
                hostContext_->notificationService()->post({
                    {"kind", "toast"},
                    {"message", QStringLiteral("Media Player: Playback stopped: %1 unplayable files in a row")
                                    .arg(consecutiveErrors_)},
                    {"sourcePluginId", kPluginId}
                });
            consecutiveErrors_ = 0;
            return;
        }
        qCWarning(lcMediaPlayerPlugin) << "skipping unplayable file:" << msg;
        if (queue_->advance(true))          // manual semantics: never re-loop one broken file
            engine_->playFile(queue_->currentTrack());
    });

    connect(engine_, &PlaybackEngine::playbackStateChanged, this, [this]() {
        const bool playing = (engine_->playbackState() == 1);
        if (playing && !wasPlaying_)
            emit playbackStarted();
        wasPlaying_ = playing;
        emit playbackStateChanged();
    });

    connect(engine_, &PlaybackEngine::metadataChanged, this, [this]() {
        if (artProvider_)
            artProvider_->setCurrentArt(engine_->coverArt());
        emit metadataChanged();
    });

    connect(engine_, &PlaybackEngine::progressChanged, this,
            [this](qint64 pos, qint64 dur) {
        if (pos > 500) {
            consecutiveErrors_ = 0;  // decode demonstrably working
            restoring_ = false;
        }
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
    saveState();
    if (engine_) engine_->stop();
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

void MediaPlayerPlugin::playFileFromFolder(const QString& path) {
    restoring_ = false;  // user action supersedes restore state
    const QStringList files = folderModel_->audioFilesInCurrentDir();
    const int idx = files.indexOf(path);
    if (idx < 0) return;
    consecutiveErrors_ = 0;
    queue_->setTracks(files, idx);
    setHasTrack(true);
    engine_->playFile(path);
}

void MediaPlayerPlugin::playPause() {
    restoring_ = false;  // user action supersedes restore state
    if (!hasTrack_) return;
    switch (engine_->playbackState()) {
    case 1:  engine_->pause(); break;
    case 2:  engine_->play(); break;
    default: engine_->playFile(queue_->currentTrack()); break;  // stopped: restart
    }
}

void MediaPlayerPlugin::next() {
    if (!hasTrack_) return;
    if (queue_->advance(true))
        engine_->playFile(queue_->currentTrack());
}

void MediaPlayerPlugin::previous() {
    if (!hasTrack_) return;
    // Classic head-unit behavior: >3 s into the track = restart it.
    if (engine_->position() > 3000) {
        engine_->seek(0);
    } else if (queue_->retreat()) {
        engine_->playFile(queue_->currentTrack());
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
    restoring_ = true;
    engine_->restorePaused(queue_->currentTrack(), pos);
}

} // namespace plugins
} // namespace oap
