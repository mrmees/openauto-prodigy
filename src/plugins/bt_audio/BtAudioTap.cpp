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
        playback_ = audio_->createStreamWithOptions(po);
        return playback_ != nullptr;
    };

    // Step 3 (LAST): open the capture node that publishes the retarget target.
    // Its callback runs on the PW thread and ONLY writes to the playback ring
    // (any-thread-safe) — no Qt calls.
    fx.createCapture = [this]() -> bool {
        oap::AudioService::CaptureStreamOptions co;
        co.name = QStringLiteral("openauto-bt-eq-in");
        co.sampleRate = 48000;
        co.channels = 2;
        co.bitDepth = 16;
        co.autoconnect = false;
        co.callback = [this](const uint8_t* d, int n) {
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
    fx.activate = [this]() {
        audio_->resetStreamRing(playback_);
        audio_->setStreamActive(playback_, true);
        audio_->requestAudioFocus(playback_, oap::AudioFocusType::Gain);
    };
    fx.deactivate = [this]() {
        audio_->releaseAudioFocus(playback_);
        audio_->setStreamActive(playback_, false);
        audio_->resetStreamRing(playback_);
    };

    return fx;
}

} // namespace plugins
} // namespace oap
