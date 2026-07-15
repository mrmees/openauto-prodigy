#include "BtAudioTap.hpp"

#include "core/services/AudioService.hpp"
#include "core/services/EqualizerService.hpp"

#include <QString>
#include <cstdint>

namespace oap {
namespace plugins {

BtAudioTap::BtAudioTap(oap::AudioService* audio, oap::EqualizerService* eq,
                       QObject* parent)
    : QObject(parent), audio_(audio), eq_(eq), controller_(makeEffects())
{
}

BtAudioTap::~BtAudioTap()
{
    stop();
}

bool BtAudioTap::start()
{
    return controller_.start();
}

void BtAudioTap::stop()
{
    controller_.stop();
}

void BtAudioTap::setTransportActive(bool active)
{
    controller_.onTransportActive(active);
}

bool BtAudioTap::isRunning() const
{
    return controller_.state() != BtTapController::State::Stopped;
}

BtTapController::Effects BtAudioTap::makeEffects()
{
    BtTapController::Effects fx;

    // Step 1: acquire a private Media-curve EQ engine (BT follows the Media
    // curve — Task 5 kept the id literal Media).
    fx.acquireEngine = [this]() -> bool {
        engine_ = eq_->acquireEngine(oap::StreamId::Media, 48000.0f, 2);
        return engine_ != nullptr;
    };

    // Step 2: create the playback stream INACTIVE, EQ attached pre-connect.
    // A PW error routes back into the controller for capture-first teardown.
    fx.createPlayback = [this]() -> bool {
        oap::AudioService::PlaybackStreamOptions po;
        po.name = QStringLiteral("BT Audio");
        po.priority = 50;
        po.sampleRate = 48000;
        po.channels = 2;
        po.bufferMs = 200;
        po.eqEngine = engine_;
        po.startInactive = true;
        po.disableRateMatching = true;
        po.onStreamError = [this]() { controller_.onStreamError(); };
        po.errorContext = this;  // queued error dispatch dies with this tap

        playback_ = audio_->createStreamWithOptions(po);
        return playback_ != nullptr;
    };

    // Step 3 (LAST): open the capture node that publishes the retarget target.
    // Its callback runs on the PW thread and ONLY writes to the playback ring
    // (any-thread-safe) — no Qt calls. The captureEnabled_ gate drops writes
    // while inactive, so the ring has no writer outside the active window and a
    // never-drained ring can never sit permanently full/overflowing.
    fx.createCapture = [this]() -> bool {
        oap::AudioService::CaptureStreamOptions co;
        co.name = QStringLiteral("openauto-bt-eq-in");
        co.sampleRate = 48000;
        co.channels = 2;
        co.bitDepth = 16;
        co.autoconnect = false;
        co.callback = [this](const uint8_t* d, int n) {
            if (!captureEnabled_.load(std::memory_order_relaxed)) return;
            audio_->writeAudio(playback_, d, n);
        };
        capture_ = audio_->openCaptureStreamWithOptions(co);
        return capture_ != nullptr;
    };

    // Teardown — capture FIRST (un-publish the retarget target and stop the
    // PW-thread callback before the playback handle is cleared).
    fx.destroyCapture = [this]() {
        audio_->closeCaptureStreamHandle(capture_);
        capture_ = nullptr;
    };
    fx.destroyPlayback = [this]() {
        audio_->destroyStream(playback_);
        playback_ = nullptr;
    };
    fx.releaseEngine = [this]() {
        eq_->releaseEngine(engine_);
        engine_ = nullptr;
    };

    // Activity toggle (transport edges) — playback only, never the capture node.
    // Activate order: drain the ring while it is FULLY quiescent (writer gated
    // off from the prior deactivate/Ready state AND playback still inactive ⇒ no
    // concurrent ring mutators, race-free), then start playback, then open the
    // capture gate, then take focus.
    fx.activate = [this]() {
        audio_->resetStreamRing(playback_);
        audio_->setStreamActive(playback_, true);
        captureEnabled_.store(true);
        audio_->requestAudioFocus(playback_, oap::AudioFocusType::Gain);
    };
    // Deactivate order: release focus, close the capture gate, stop playback. No
    // drain here — the next activate drains before use, and a single stale
    // in-flight capture write landing just after the gate clears is harmless
    // (it lands in a ring that is drained before it is read again).
    fx.deactivate = [this]() {
        audio_->releaseAudioFocus(playback_);
        captureEnabled_.store(false);
        audio_->setStreamActive(playback_, false);
    };

    return fx;
}

} // namespace plugins
} // namespace oap
