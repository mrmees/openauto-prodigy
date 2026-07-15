#pragma once

#include "BtTapController.hpp"
#include <QObject>

namespace oap {

class AudioService;
class EqualizerService;
class EqualizerEngine;
struct AudioStreamHandle;

namespace plugins {

/// Owns the BT A2DP loopback tap: an EQ'd, focus-arbitrated playback stream fed
/// by a capture node named "openauto-bt-eq-in" (the WirePlumber retarget target
/// installed by Task 8). The pure BtTapController sequences bring-up/teardown;
/// this class binds its effects to the real AudioService/EqualizerService with
/// the capture-LAST / capture-FIRST ordering the safety contract requires.
///
/// Threading: all lifecycle methods run on the Qt main thread. The ONLY
/// PW-thread caller is the capture callback, which does nothing but
/// AudioService::writeAudio() (a documented any-thread ring write) — no Qt
/// calls, ever. Capture-first teardown guarantees the callback has stopped
/// before `playback_` is cleared, so the PW-thread read of `playback_` never
/// races the Qt-thread clear.
class BtAudioTap : public QObject {
public:
    BtAudioTap(oap::AudioService* audio, oap::EqualizerService* eq,
               QObject* parent = nullptr);
    ~BtAudioTap() override;

    /// Ordered bring-up (acquire engine → inactive playback → capture LAST).
    /// Returns false and leaves nothing half-built on any step failure.
    bool start();

    /// Capture-first teardown to Stopped. Idempotent (no-op when not running).
    void stop();

    /// Transport activity edge from BtAudioPlugin::transportActiveChanged.
    /// Toggles playback ACTIVITY + focus only; never touches the capture node.
    void setTransportActive(bool active);

    bool isRunning() const;

private:
    BtTapController::Effects makeEffects();

    oap::AudioService* audio_ = nullptr;
    oap::EqualizerService* eq_ = nullptr;
    oap::EqualizerEngine* engine_ = nullptr;
    oap::AudioStreamHandle* playback_ = nullptr;
    oap::AudioStreamHandle* capture_ = nullptr;
    BtTapController controller_;
};

} // namespace plugins
} // namespace oap
