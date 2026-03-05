#include "core/aa/TouchRouter.hpp"

#include <algorithm>

namespace oap { namespace aa {

void TouchRouter::setZones(std::vector<TouchZone> zones) {
    // Sort by priority descending so dispatch can iterate in priority order
    std::sort(zones.begin(), zones.end(),
              [](const TouchZone& a, const TouchZone& b) {
                  return a.priority > b.priority;
              });

    std::lock_guard<std::mutex> lock(mutex_);
    zones_ = std::move(zones);

    // Clear stale claims: any claimed zone ID that no longer exists
    for (auto& claim : claimedZone_) {
        if (claim.empty()) continue;
        bool found = false;
        for (const auto& zone : zones_) {
            if (zone.id == claim) {
                found = true;
                break;
            }
        }
        if (!found) {
            claim.clear();
        }
    }
}

bool TouchRouter::dispatch(int slot, float x, float y, TouchEvent event) {
    if (slot < 0 || slot >= MAX_SLOTS) return false;

    std::lock_guard<std::mutex> lock(mutex_);

    // If slot has a claim (sticky finger), route to that zone
    const std::string& claimed = claimedZone_[slot];
    if (!claimed.empty()) {
        for (const auto& zone : zones_) {
            if (zone.id == claimed) {
                zone.callback(slot, x, y, event);
                if (event == TouchEvent::Up) {
                    claimedZone_[slot].clear();
                }
                return true;
            }
        }
        // Claimed zone was removed — clear stale claim
        claimedZone_[slot].clear();
    }

    // Only DOWN events can claim a new zone
    if (event != TouchEvent::Down) return false;

    // Find highest-priority zone containing the touch point
    // (zones_ is already sorted by priority descending)
    for (const auto& zone : zones_) {
        if (x >= zone.x0 && x <= zone.x1 && y >= zone.y0 && y <= zone.y1) {
            claimedZone_[slot] = zone.id;
            zone.callback(slot, x, y, event);
            return true;
        }
    }

    return false;
}

void TouchRouter::resetClaims() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& claim : claimedZone_) {
        claim.clear();
    }
}

}} // namespace oap::aa
