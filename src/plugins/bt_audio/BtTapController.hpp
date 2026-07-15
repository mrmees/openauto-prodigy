#pragma once

#include <functional>
#include <utility>

namespace oap {
namespace plugins {

/// Pure, PipeWire-free sequencing state machine for the BT A2DP loopback tap
/// (Task 7 / design §3.1, §3.2, §4.3). All side effects are injected as
/// std::functions so the ordering contract is unit-testable without Qt,
/// PipeWire, or BlueZ.
///
/// Safety property it enforces: the capture node (effect `createCapture`)
/// publishes the WirePlumber retarget target "openauto-bt-eq-in". It must
/// never exist while its downstream (ring → playback → EQ engine) is broken,
/// or BT audio is silently eaten. So capture is created LAST on bring-up and
/// destroyed FIRST on teardown/error, always.
///
///   Stopped --start()--> Ready --onTransportActive(true)--> Active
///   Active  --onTransportActive(false)--> Ready
///   {Ready,Active} --stop()/onStreamError()--> Stopped   (capture-first)
///
/// Transport edges toggle stream ACTIVITY only (activate/deactivate); they
/// never create or destroy the capture node (that would race the graph link).
class BtTapController {
public:
    enum class State { Stopped, Ready, Active };

    struct Effects {
        std::function<bool()> acquireEngine;   ///< step 1
        std::function<bool()> createPlayback;  ///< step 2 (inactive)
        std::function<bool()> createCapture;   ///< step 3 (LAST — publishes target)
        std::function<void()> destroyCapture;  ///< teardown FIRST
        std::function<void()> destroyPlayback;
        std::function<void()> releaseEngine;
        std::function<void()> activate;        ///< drain quiesced ring → set active → open capture gate → request focus
        std::function<void()> deactivate;      ///< release focus → close capture gate → set inactive
    };

    explicit BtTapController(Effects fx) : fx_(std::move(fx)) {}

    /// Stopped → Ready: acquireEngine → createPlayback → createCapture. On the
    /// FIRST failing step, unwind exactly the steps that succeeded in reverse
    /// teardown order (capture never exists yet by construction; playback is
    /// destroyed before the engine is released), stay Stopped, return false.
    /// A no-op returning false if already started.
    bool start()
    {
        if (state_ != State::Stopped)
            return false;

        if (!fx_.acquireEngine())
            return false;  // nothing acquired — nothing to unwind

        if (!fx_.createPlayback()) {
            fx_.releaseEngine();
            return false;
        }

        if (!fx_.createCapture()) {
            fx_.destroyPlayback();
            fx_.releaseEngine();
            return false;
        }

        state_ = State::Ready;
        return true;
    }

    /// Any → Stopped, capture-first. From Active it first deactivates.
    void stop() { teardownToStopped(); }

    /// Ready ↔ Active activity toggle. Ignored when Stopped; duplicate edges
    /// are no-ops (they do not re-run activate/deactivate).
    void onTransportActive(bool active)
    {
        if (active && state_ == State::Ready) {
            fx_.activate();
            state_ = State::Active;
        } else if (!active && state_ == State::Active) {
            fx_.deactivate();
            state_ = State::Ready;
        }
        // Stopped, or a duplicate edge in the current state: no effect.
    }

    /// Playback stream error → capture-first teardown to Stopped (identical to
    /// stop(); wired from AudioService's onStreamError hook).
    void onStreamError() { teardownToStopped(); }

    State state() const { return state_; }

private:
    void teardownToStopped()
    {
        if (state_ == State::Stopped)
            return;

        if (state_ == State::Active)
            fx_.deactivate();

        fx_.destroyCapture();   // FIRST — un-publish the retarget target
        fx_.destroyPlayback();
        fx_.releaseEngine();
        state_ = State::Stopped;
    }

    Effects fx_;
    State state_ = State::Stopped;
};

} // namespace plugins
} // namespace oap
