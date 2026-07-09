#include "PlaybackEngine.hpp"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QLoggingCategory>
#include <QMediaMetaData>
#include <QUrl>

#include "core/services/IAudioService.hpp"
#include "core/services/AudioService.hpp"  // full AudioStreamHandle definition (eqEngine field)

Q_LOGGING_CATEGORY(lcMediaPlayer, "oap.mediaplayer")

namespace {
QAudioFormat tapFormat() {
    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);
    return fmt;
}
} // namespace

namespace oap {
namespace plugins {

PlaybackEngine::PlaybackEngine(QObject* parent)
    : QObject(parent)
    , tap_(tapFormat())
{
    player_.setAudioBufferOutput(&tap_);
    // NOTE (Task 1 verdict): if the spike required the muted-sink crutch,
    // add here:  audioOut_ = new QAudioOutput(this); audioOut_->setVolume(0);
    //            player_.setAudioOutput(audioOut_);
    // and declare QAudioOutput* audioOut_ in the header.

    connect(&tap_, &QAudioBufferOutput::audioBufferReceived,
            this, &PlaybackEngine::onAudioBuffer);
    connect(&player_, &QMediaPlayer::mediaStatusChanged,
            this, &PlaybackEngine::onMediaStatus);
    connect(&player_, &QMediaPlayer::playbackStateChanged,
            this, &PlaybackEngine::onPlaybackStateChanged);
    connect(&player_, &QMediaPlayer::metaDataChanged,
            this, &PlaybackEngine::readMetadata);
    connect(&player_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& msg) {
        qCWarning(lcMediaPlayer) << "playback error:" << msg << "file:" << player_.source();
        if (audioService_ && stream_)
            audioService_->releaseAudioFocus(stream_);
        emit errorOccurred(msg);
    });
    connect(&player_, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        if (!progressEmitTimer_.isValid() || progressEmitTimer_.elapsed() >= 500) {
            progressEmitTimer_.restart();
            emit progressChanged(pos, player_.duration());
        }
    });
    connect(&player_, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
        emit progressChanged(player_.position(), dur);
    });
}

PlaybackEngine::~PlaybackEngine() {
    releaseAudioResources();
}

void PlaybackEngine::releaseAudioResources() {
    player_.stop();
    if (audioService_ && stream_) {
        audioService_->releaseAudioFocus(stream_);
        audioService_->destroyStream(stream_);
        stream_ = nullptr;
    }
}

void PlaybackEngine::playFile(const QString& path) {
    pendingSeekMs_ = -1;
    pauseAfterLoad_ = false;
    player_.setSource(QUrl::fromLocalFile(path));
    player_.play();
}

void PlaybackEngine::restorePaused(const QString& path, qint64 positionMs) {
    pendingSeekMs_ = positionMs > 0 ? positionMs : -1;
    pauseAfterLoad_ = true;
    player_.setSource(QUrl::fromLocalFile(path));
    // Seek + pause complete in onMediaStatus(LoadedMedia).
}

void PlaybackEngine::play()  { player_.play(); }
void PlaybackEngine::pause() { player_.pause(); }

void PlaybackEngine::stop() {
    player_.stop();
    if (audioService_ && stream_)
        audioService_->releaseAudioFocus(stream_);
}

void PlaybackEngine::seek(qint64 ms) { player_.setPosition(ms); }

int PlaybackEngine::playbackState() const {
    switch (player_.playbackState()) {
    case QMediaPlayer::PlayingState: return 1;
    case QMediaPlayer::PausedState:  return 2;
    default:                         return 0;
    }
}

void PlaybackEngine::ensureStream() {
    if (stream_ || !audioService_) return;
    // Priority 51: one above AA Media (50) so a Gain-focus tie resolves to
    // the most recent user action (local play) instead of stream-creation
    // order muting us. Nav speech (60) still ducks/mutes local playback.
    stream_ = audioService_->createStream(QStringLiteral("Local Media"), 51,
                                          48000, 2, QStringLiteral("auto"), bufferMs_);
    if (!stream_) {
        qCWarning(lcMediaPlayer) << "AudioService stream creation failed";
        return;
    }
    stream_->eqEngine = eqEngine_;  // same attach pattern as AA media
}

void PlaybackEngine::onAudioBuffer(const QAudioBuffer& buffer) {
    if (!buffer.isValid() || buffer.byteCount() == 0)
        return;  // end-of-stream sentinel buffer — not audio
    ensureStream();
    if (!stream_ || !audioService_) return;
    const int written = audioService_->writeAudio(
        stream_, buffer.constData<uint8_t>(), int(buffer.byteCount()));
    if (written >= 0 && written < buffer.byteCount()) {
        static int dropWarnings = 0;
        if (++dropWarnings % 100 == 1)
            qCWarning(lcMediaPlayer) << "ring buffer overrun, dropped"
                                     << (buffer.byteCount() - written) << "bytes";
    }
}

void PlaybackEngine::onMediaStatus(QMediaPlayer::MediaStatus status) {
    switch (status) {
    case QMediaPlayer::LoadedMedia:
        if (pendingSeekMs_ >= 0) {
            player_.setPosition(pendingSeekMs_);
            pendingSeekMs_ = -1;
        }
        if (pauseAfterLoad_) {
            pauseAfterLoad_ = false;
            player_.pause();
        }
        break;
    case QMediaPlayer::EndOfMedia:
        if (audioService_ && stream_)
            audioService_->releaseAudioFocus(stream_);
        emit trackFinished();
        break;
    default:
        break;
    }
}

void PlaybackEngine::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    if (audioService_) {
        if (state == QMediaPlayer::PlayingState) {
            ensureStream();
            if (stream_) audioService_->requestAudioFocus(stream_, AudioFocusType::Gain);
        } else if (stream_) {
            audioService_->releaseAudioFocus(stream_);
        }
    }
    emit playbackStateChanged();
    emit progressChanged(player_.position(), player_.duration());
}

void PlaybackEngine::readMetadata() {
    const QMediaMetaData md = player_.metaData();
    title_ = md.value(QMediaMetaData::Title).toString();
    artist_ = md.value(QMediaMetaData::ContributingArtist).toString();
    if (artist_.isEmpty())
        artist_ = md.value(QMediaMetaData::AlbumArtist).toString();
    album_ = md.value(QMediaMetaData::AlbumTitle).toString();
    coverArt_ = md.value(QMediaMetaData::CoverArtImage).value<QImage>();
    if (coverArt_.isNull())
        coverArt_ = md.value(QMediaMetaData::ThumbnailImage).value<QImage>();
    if (title_.isEmpty())
        title_ = QUrl(player_.source()).fileName();
    emit metadataChanged();
}

} // namespace plugins
} // namespace oap
