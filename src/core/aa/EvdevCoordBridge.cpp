#include "core/aa/EvdevCoordBridge.hpp"

namespace oap { namespace aa {

EvdevCoordBridge::EvdevCoordBridge(TouchRouter* router, QObject* parent)
    : QObject(parent), router_(router) {}

void EvdevCoordBridge::updateZone(const std::string& /*id*/, int /*priority*/,
                                   float /*pixelX*/, float /*pixelY*/,
                                   float /*pixelW*/, float /*pixelH*/,
                                   std::function<void(int, float, float, TouchEvent)> /*callback*/) {
    // Stub -- will be implemented in GREEN phase
}

void EvdevCoordBridge::updateZone(const QString& /*id*/, int /*priority*/,
                                   qreal /*pixelX*/, qreal /*pixelY*/,
                                   qreal /*pixelW*/, qreal /*pixelH*/) {
    // Stub -- will be implemented in GREEN phase
}

void EvdevCoordBridge::removeZone(const QString& /*id*/) {
    // Stub
}

void EvdevCoordBridge::removeZone(const std::string& /*id*/) {
    // Stub
}

void EvdevCoordBridge::setDisplayMapping(int /*displayW*/, int /*displayH*/,
                                          int /*evdevMaxX*/, int /*evdevMaxY*/) {
    // Stub
}

float EvdevCoordBridge::pixelToEvdevX(float /*px*/) const {
    return 0.0f;
}

float EvdevCoordBridge::pixelToEvdevY(float /*py*/) const {
    return 0.0f;
}

void EvdevCoordBridge::rebuildAndPush() {
    // Stub
}

}} // namespace oap::aa
