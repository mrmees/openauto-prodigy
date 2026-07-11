#pragma once

#include <QtGlobal>

namespace oap {
namespace plugins {

/// Pure playback-policy state machine extracted from MediaPlayerPlugin's
/// signal lambdas (wishlist promotion 2026-07-09). Holds NO Qt-object
/// state; every stage-1 bench invariant is locked by
/// tests/test_media_playback_policy.cpp. Spec: design §10/§11.
class PlaybackPolicy {
public:
    enum class TrackEndVerdict { Advance, Unplayable };
    enum class UnplayableVerdict { SkipNext, StopAndNotify, StayStopped };

    /// Track reached its end. Below the audibility watermark the "end" was
    /// FFmpeg misdetecting garbage as audio (bench row 12) — unplayable.
    TrackEndVerdict onTrackFinished() const {
        return (lastProgressMs_ < kMinAudibleMs) ? TrackEndVerdict::Unplayable
                                                 : TrackEndVerdict::Advance;
    }

    /// An unplayable edge (decode error, or no-audio track end).
    /// StopAndNotify does NOT reset the counter — the caller reads
    /// consecutiveErrors() for the toast text, then calls resetStrikes()
    /// (preserves the stage-1 side-effect order: stop -> toast -> reset).
    UnplayableVerdict onUnplayableEdge();
    void resetStrikes() { consecutiveErrors_ = 0; }

    void onProgress(qint64 posMs);   ///< watermark + strike clearing; never clears restoring
    void onTrackStarted() { lastProgressMs_ = 0; }
    void onUserAction()   { restoring_ = false; }
    void onNewQueue()     { consecutiveErrors_ = 0; }
    void onRestoreBegan() { restoring_ = true; }
    void onShutdownBegan(){ shuttingDown_ = true; }

    bool saveAllowed() const { return !restoring_ && !shuttingDown_; }
    bool restoring() const { return restoring_; }
    int consecutiveErrors() const { return consecutiveErrors_; }

private:
    static constexpr int kMaxConsecutiveErrors = 3;
    static constexpr qint64 kMinAudibleMs = 500;

    int consecutiveErrors_ = 0;
    qint64 lastProgressMs_ = 0;
    bool restoring_ = false;
    bool shuttingDown_ = false;
};

} // namespace plugins
} // namespace oap
