#pragma once

#include "IMediaStatusService.hpp"
#include <array>
#include <functional>

namespace oap {

/// Core service merging media status from AA, BT, and the local MediaPlayer.
/// Arbitration is "playing wins" (spec 2026-07-08 §6): the actively-playing
/// source owns the display; most-recently-started-playing breaks ties; when
/// nothing plays, the current source keeps the display while connected, else
/// the most recently active connected source takes over.
class MediaStatusService : public IMediaStatusService {
    Q_OBJECT
public:
    explicit MediaStatusService(QObject* parent = nullptr);

    // IMediaStatusProvider
    bool hasMedia() const override;
    QString title() const override;
    QString artist() const override;
    QString album() const override;
    int playbackState() const override;
    bool isPlaying() const override;
    QString source() const override;
    QString appName() const override;
    qint64 position() const override;
    qint64 duration() const override;
    bool hasPosition() const override;
    QString artUrl() const override;
    void playPause() override;
    void next() override;
    void previous() override;

    /// Set playback control callbacks (delegated to active source by the
    /// main.cpp wiring, which branches on source()).
    using Callback = std::function<void()>;
    void setPlaybackCallbacks(Callback play, Callback next, Callback prev);

    // AA source (raw states: 1=stopped, 2=playing, 3=paused)
    void setAaConnected(bool connected);
    void updateAaMetadata(const QString& title, const QString& artist, const QString& album);
    void updateAaPlaybackState(int state, const QString& appName = {});

    // BT source (raw states: 0=stopped, 1=playing, 2=paused)
    void setBtConnected(bool connected);
    void updateBtMetadata(const QString& title, const QString& artist, const QString& album);
    void updateBtPlaybackState(int state);
    void updateBtProgress(qint64 positionMs, qint64 durationMs);

    // Local MediaPlayer source (raw states: 0=stopped, 1=playing, 2=paused)
    void setMediaPlayerConnected(bool connected);
    void updateMediaPlayerMetadata(const QString& title, const QString& artist, const QString& album);
    void updateMediaPlayerPlaybackState(int state);
    void updateMediaPlayerProgress(qint64 positionMs, qint64 durationMs);
    void updateMediaPlayerArt(const QString& artUrl);

private:
    enum SourceId { SrcBluetooth = 0, SrcAndroidAuto = 1, SrcMediaPlayer = 2, SrcCount = 3 };
    static constexpr int SrcNone = -1;

    struct SourceState {
        bool connected = false;
        bool playing = false;
        QString title, artist, album, appName, artUrl;
        int playbackState = 0;
        qint64 position = -1;
        qint64 duration = 0;
        quint64 seq = 0;   ///< bumped on connect and on transition-to-playing
    };

    static bool isPlayingState(int sourceId, int rawState);
    void setConnected(int id, bool connected);
    void applyPlaybackState(int id, int rawState);
    void recompute(bool emitEvenIfUnchanged);
    void clearSource(int id);
    const SourceState* active() const;

    std::array<SourceState, SrcCount> src_;
    int active_ = SrcNone;
    quint64 seq_ = 0;
    Callback playCallback_, nextCallback_, prevCallback_;
};

} // namespace oap
