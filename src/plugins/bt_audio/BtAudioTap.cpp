#include "BtAudioTap.hpp"

#include "core/Logging.hpp"
#include "core/services/AudioService.hpp"
#include "core/services/EqualizerService.hpp"

#include <QMetaObject>
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
        // Error surfacing parity with playback: a capture node that dies after
        // connect routes back into the controller for capture-first teardown so
        // a dead node never stays published eating BT audio (design §3.1).
        co.onStreamError = [this]() { controller_.onStreamError(); };
        co.errorContext = this;  // queued error dispatch dies with this tap
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
        // If activation fails, do NOT open the capture gate or take focus — a
        // gate over a dead playback stream would eat BT audio into a stream that
        // never runs. Post a QUEUED self-invocation of the error path: the
        // teardown must run AFTER this transition completes (direct re-entry into
        // the state machine mid-transition is forbidden). The queued teardown
        // then runs from Active → onStreamError → capture-first teardown → Stopped.
        if (!audio_->setStreamActive(playback_, true)) {
            qCWarning(lcBT) << "BtAudioTap: playback activation failed — tearing down tap";
            QMetaObject::invokeMethod(this, [this]() { controller_.onStreamError(); },
                                      Qt::QueuedConnection);
            return;
        }
        captureEnabled_.store(true);
        audio_->requestAudioFocus(playback_, oap::AudioFocusType::Gain);
    };
    // Deactivate order: release focus, close the capture gate, stop playback. No
    // drain here — the next activate drains before use, and a single stale
    // in-flight capture write landing just after the gate clears is harmless
    // (it lands in a ring that is drained before it is read again). A failed
    // deactivate is log-only: teardown must proceed regardless.
    fx.deactivate = [this]() {
        audio_->releaseAudioFocus(playback_);
        captureEnabled_.store(false);
        if (!audio_->setStreamActive(playback_, false))
            qCWarning(lcBT) << "BtAudioTap: playback deactivation failed (continuing teardown)";
    };

    return fx;
}

} // namespace plugins
} // namespace oap
