#pragma once

#include <array>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace oap { namespace aa {

enum class TouchEvent { Down, Move, Up };

struct TouchZone {
    std::string id;
    int priority;  // higher wins
    float x0, y0, x1, y1;  // evdev coordinate space (inclusive bounds)
    std::function<void(int slot, float x, float y, TouchEvent event)> callback;
};

class TouchRouter {
public:
    // Thread-safe: called from main thread, takes effect on next dispatch
    void setZones(std::vector<TouchZone> zones);

    // Called from reader thread hot path. Returns true if touch was claimed by a zone.
    bool dispatch(int slot, float x, float y, TouchEvent event);

    // Clear all claims (e.g., on ungrab)
    void resetClaims();

private:
    static constexpr int MAX_SLOTS = 10;

    std::mutex mutex_;
    std::vector<TouchZone> zones_;
    std::array<std::string, MAX_SLOTS> claimedZone_;  // slot -> zone ID
};

}} // namespace oap::aa
