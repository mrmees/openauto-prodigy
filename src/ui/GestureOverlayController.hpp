#pragma once

#include <QObject>
#include <QQuickItem>
#include <QString>
#include <vector>
#include <string>

namespace oap {

class ActionRegistry;

namespace aa {
class EvdevCoordBridge;
}

/// Manages the three-finger gesture overlay: zone registration, slider handling,
/// and coordinate mapping. Extracted from main.cpp to keep assembler code minimal.
class GestureOverlayController : public QObject {
    Q_OBJECT

public:
    explicit GestureOverlayController(QObject* parent = nullptr);

    /// Set the evdev coordinate bridge for zone registration.
    void setCoordBridge(aa::EvdevCoordBridge* bridge);

    /// Set the audio service (for volume slider).
    void setAudioService(QObject* svc);

    /// Set the display service (for brightness slider).
    void setDisplayService(QObject* svc);

    /// Set the action registry (for home/theme actions).
    void setActionRegistry(ActionRegistry* registry);

    /// Whether the overlay is currently visible.
    bool isOverlayVisible() const { return overlayVisible_; }

    /// Returns the list of zone IDs this controller manages.
    std::vector<std::string> managedZoneIds() const;

    /// Show the overlay: register blocking zone, then per-control zones.
    /// overlayItem must be a QQuickItem* (the GestureOverlay QML element).
    void showOverlay(QQuickItem* overlayItem, int displayWidth, int displayHeight);

    /// Clean up all zones (called on overlay dismiss).
    void cleanupZones();

private:
    void registerControlZones(QQuickItem* overlayItem);

    aa::EvdevCoordBridge* coordBridge_ = nullptr;
    QObject* audioService_ = nullptr;
    QObject* displayService_ = nullptr;
    ActionRegistry* actionRegistry_ = nullptr;
    bool overlayVisible_ = false;
    QMetaObject::Connection visibilityConnection_;
};

} // namespace oap
