#include "PlaybackPolicy.hpp"

namespace oap {
namespace plugins {

PlaybackPolicy::UnplayableVerdict PlaybackPolicy::onUnplayableEdge() {
    if (restoring_) {
        // No auto-skip at boot — nothing may auto-play (design §10).
        restoring_ = false;
        return UnplayableVerdict::StayStopped;
    }
    ++consecutiveErrors_;
    if (consecutiveErrors_ >= kMaxConsecutiveErrors) {
        // A dead USB stick must not machine-gun skips (design §11).
        // Counter is NOT reset here — caller toasts it, then resetStrikes().
        return UnplayableVerdict::StopAndNotify;
    }
    return UnplayableVerdict::SkipNext;
}

void PlaybackPolicy::onProgress(qint64 posMs) {
    lastProgressMs_ = qMax(lastProgressMs_, posMs);
    if (posMs > kMinAudibleMs)
        consecutiveErrors_ = 0;  // decode demonstrably working
    // restoring_ deliberately NOT cleared here — restore-seek echoes the
    // position before any decode (bench 2026-07-09 row 11 addendum).
}

} // namespace plugins
} // namespace oap
