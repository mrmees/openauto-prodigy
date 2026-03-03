#include "core/aa/EvdevCoordBridge.hpp"

#include <algorithm>

namespace oap { namespace aa {

EvdevCoordBridge::EvdevCoordBridge(TouchRouter* router, QObject* parent)
    : QObject(parent), router_(router) {}

void EvdevCoordBridge::updateZone(const std::string& id, int priority,
                                   float pixelX, float pixelY,
                                   float pixelW, float pixelH,
                                   std::function<void(int, float, float, TouchEvent)> callback) {
    // Replace existing zone with same ID, or add new
    auto it = std::find_if(zones_.begin(), zones_.end(),
                           [&id](const ZoneEntry& e) { return e.id == id; });
    if (it != zones_.end()) {
        it->priority = priority;
        it->px = pixelX;
        it->py = pixelY;
        it->pw = pixelW;
        it->ph = pixelH;
        it->cb = std::move(callback);
    } else {
        zones_.push_back({id, priority, pixelX, pixelY, pixelW, pixelH, std::move(callback)});
    }
    rebuildAndPush();
}

void EvdevCoordBridge::updateZone(const QString& id, int priority,
                                   qreal pixelX, qreal pixelY,
                                   qreal pixelW, qreal pixelH) {
    // QML overload without callback -- registers zone with no-op callback
    // (Future: accept QJSValue callback)
    updateZone(id.toStdString(), priority,
               static_cast<float>(pixelX), static_cast<float>(pixelY),
               static_cast<float>(pixelW), static_cast<float>(pixelH),
               [](int, float, float, TouchEvent) {});
}

void EvdevCoordBridge::removeZone(const QString& id) {
    removeZone(id.toStdString());
}

void EvdevCoordBridge::removeZone(const std::string& id) {
    zones_.erase(std::remove_if(zones_.begin(), zones_.end(),
                                [&id](const ZoneEntry& e) { return e.id == id; }),
                 zones_.end());
    rebuildAndPush();
}

void EvdevCoordBridge::setDisplayMapping(int displayW, int displayH,
                                          int evdevMaxX, int evdevMaxY) {
    displayW_ = displayW;
    displayH_ = displayH;
    evdevMaxX_ = evdevMaxX;
    evdevMaxY_ = evdevMaxY;
    // Don't rebuild -- existing zones keep their pixel coords,
    // new conversion factors apply on next updateZone or explicit rebuildAndPush
}

float EvdevCoordBridge::pixelToEvdevX(float px) const {
    if (displayW_ <= 0) return 0.0f;
    return px * static_cast<float>(evdevMaxX_) / static_cast<float>(displayW_);
}

float EvdevCoordBridge::pixelToEvdevY(float py) const {
    if (displayH_ <= 0) return 0.0f;
    return py * static_cast<float>(evdevMaxY_) / static_cast<float>(displayH_);
}

void EvdevCoordBridge::rebuildAndPush() {
    std::vector<TouchZone> routerZones;
    routerZones.reserve(zones_.size());

    for (const auto& entry : zones_) {
        TouchZone tz;
        tz.id = entry.id;
        tz.priority = entry.priority;
        tz.x0 = pixelToEvdevX(entry.px);
        tz.y0 = pixelToEvdevY(entry.py);
        tz.x1 = pixelToEvdevX(entry.px + entry.pw);
        tz.y1 = pixelToEvdevY(entry.py + entry.ph);
        tz.callback = entry.cb;
        routerZones.push_back(std::move(tz));
    }

    router_->setZones(std::move(routerZones));
}

}} // namespace oap::aa
