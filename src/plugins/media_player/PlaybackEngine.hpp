#pragma once

#include <QAudioBufferOutput>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaPlayer>
#include <QObject>
#include <QString>

namespace oap {

class IAudioService;
struct AudioStreamHandle;
class EqualizerEngine;

namespace plugins {

/// Transport + decode for the local media player. QMediaPlayer drives
/// decode/clock/seek; decoded PCM is tapped via QAudioBufferOutput
/// (48 kHz / S16 / stereo, converted by Qt) and pushed into an AudioService
/// stream — the exact mechanics AA media audio uses (createStream +
/// writeAudio + eqEngine), so EQ / master volume / ducking / focus all apply.
/// No QAudioOutput device sink is attached (spike-verified, Task 1).
class PlaybackEngine : public QObject {
    Q_OBJECT

public:
    explicit PlaybackEngine(QObject* parent = nullptr);
    ~PlaybackEngine() override;

    void setAudioService(IAudioService* service) { audioService_ = service; }
    void setEqEngine(EqualizerEngine* engine) { eqEngine_ = engine; }
    void setBufferMs(int ms) { bufferMs_ = ms; }

    void playFile(const QString& path);
    void restorePaused(const QString& path, qint64 positionMs);
    void play();
    void pause();
    void stop();
    void seek(qint64 ms);

    /// Fully release AudioService resources (focus + stream). Idempotent:
    /// stream_ is nulled, so a later destructor call is a no-op. Must run at
    /// plugin shutdown because AudioService is an earlier app child and is
    /// destroyed first — releasing only in ~PlaybackEngine is a use-after-free.
    void releaseAudioResources();

    /// 0=Stopped, 1=Playing, 2=Paused (the MediaPlayer source convention).
    int playbackState() const;
    qint64 position() const { return player_.position(); }
    qint64 duration() const { return player_.duration(); }
    QString title() const { return title_; }
    QString artist() const { return artist_; }
    QString album() const { return album_; }
    QImage coverArt() const { return coverArt_; }

signals:
    void playbackStateChanged();
    void progressChanged(qint64 positionMs, qint64 durationMs);
    void metadataChanged();
    void trackFinished();
    void errorOccurred(const QString& message);

private:
    void ensureStream();
    void onAudioBuffer(const QAudioBuffer& buffer);
    void onMediaStatus(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void readMetadata();

    QMediaPlayer player_;
    QAudioBufferOutput tap_;
    IAudioService* audioService_ = nullptr;
    AudioStreamHandle* stream_ = nullptr;
    EqualizerEngine* eqEngine_ = nullptr;
    int bufferMs_ = 50;

    QString title_, artist_, album_;
    QImage coverArt_;
    qint64 pendingSeekMs_ = -1;
    bool pauseAfterLoad_ = false;
    QElapsedTimer progressEmitTimer_;
};

} // namespace plugins
} // namespace oap
