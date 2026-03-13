#include "GestureOverlayController.hpp"
#include "core/aa/EvdevCoordBridge.hpp"
#include "core/services/ActionRegistry.hpp"
#include <QTimer>
#include <QGuiApplication>
#include <algorithm>

namespace oap {

GestureOverlayController::GestureOverlayController(QObject* parent)
    : QObject(parent)
{
}

void GestureOverlayController::setCoordBridge(aa::EvdevCoordBridge* bridge)
{
    coordBridge_ = bridge;
}

void GestureOverlayController::setAudioService(QObject* svc)
{
    audioService_ = svc;
}

void GestureOverlayController::setDisplayService(QObject* svc)
{
    displayService_ = svc;
}

void GestureOverlayController::setActionRegistry(ActionRegistry* registry)
{
    actionRegistry_ = registry;
}

std::vector<std::string> GestureOverlayController::managedZoneIds() const
{
    return {
        "gesture-overlay",
        "overlay-volume",
        "overlay-brightness",
        "overlay-home",
        "overlay-theme",
        "overlay-close"
    };
}

void GestureOverlayController::showOverlay(QQuickItem* overlayItem,
                                            int displayWidth, int displayHeight)
{
    if (!overlayItem) return;

    QMetaObject::invokeMethod(overlayItem, "show");
    overlayVisible_ = true;

    // Register full-screen evdev zone at priority 200 to consume all touches
    // during overlay visibility (prevents AA forwarding while overlay is up).
    if (!coordBridge_) return;

    coordBridge_->updateZone(
        std::string("gesture-overlay"), 200,
        0.0f, 0.0f,
        static_cast<float>(displayWidth), static_cast<float>(displayHeight),
        [](int, float, float, aa::TouchEvent) {
            // Consume all touches to block AA forwarding.
            // Per-control zones at priority 210 handle interactions.
        });

    // Wait one frame for QML layout to stabilize, then register per-control zones
    QTimer::singleShot(16, overlayItem, [this, overlayItem]() {
        if (!overlayItem || !overlayItem->isVisible()) return;
        registerControlZones(overlayItem);
    });

    // Deregister all zones when overlay becomes invisible
    if (visibilityConnection_)
        QObject::disconnect(visibilityConnection_);

    visibilityConnection_ = QObject::connect(overlayItem, &QQuickItem::visibleChanged,
        overlayItem, [this, overlayItem]() {
            if (!overlayItem->isVisible()) {
                cleanupZones();
            }
        });
}

void GestureOverlayController::registerControlZones(QQuickItem* overlayItem)
{
    if (!coordBridge_ || !overlayItem || !overlayItem->isVisible()) return;

    // Helper: get window-space rect for a named child
    auto getRect = [overlayItem](const char* name, float& x, float& y, float& w, float& h) -> bool {
        auto* item = overlayItem->findChild<QQuickItem*>(QString(name));
        if (!item || item->width() <= 0) return false;
        QPointF pos = item->mapToScene(QPointF(0, 0));
        x = pos.x(); y = pos.y();
        w = item->width(); h = item->height();
        return true;
    };

    float x, y, w, h;

    // Volume slider row
    if (audioService_ && getRect("overlayVolumeRow", x, y, w, h)) {
        float evX0 = coordBridge_->pixelToEvdevX(x);
        float evX1 = coordBridge_->pixelToEvdevX(x + w);
        auto* audioSvc = audioService_;
        coordBridge_->updateZone("overlay-volume", 210, x, y, w, h,
            [audioSvc, evX0, evX1](int, float rawX, float, aa::TouchEvent event) {
                if (event == aa::TouchEvent::Down || event == aa::TouchEvent::Move) {
                    float norm = std::clamp((rawX - evX0) / (evX1 - evX0), 0.0f, 1.0f);
                    int volume = static_cast<int>(norm * 100.0f);
                    QMetaObject::invokeMethod(audioSvc, "setMasterVolume",
                                              Qt::QueuedConnection, Q_ARG(int, volume));
                }
            });
    }

    // Brightness slider row
    if (displayService_ && getRect("overlayBrightnessRow", x, y, w, h)) {
        float evX0 = coordBridge_->pixelToEvdevX(x);
        float evX1 = coordBridge_->pixelToEvdevX(x + w);
        auto* dispSvc = displayService_;
        coordBridge_->updateZone("overlay-brightness", 210, x, y, w, h,
            [dispSvc, evX0, evX1](int, float rawX, float, aa::TouchEvent event) {
                if (event == aa::TouchEvent::Down || event == aa::TouchEvent::Move) {
                    float norm = std::clamp((rawX - evX0) / (evX1 - evX0), 0.0f, 1.0f);
                    int brightness = 5 + static_cast<int>(norm * 95.0f);
                    QMetaObject::invokeMethod(dispSvc, "setBrightness",
                                              Qt::QueuedConnection, Q_ARG(int, brightness));
                }
            });
    }

    // Home button
    if (getRect("overlayHomeBtn", x, y, w, h)) {
        auto* actReg = actionRegistry_;
        auto* overlayItemPtr = overlayItem;
        coordBridge_->updateZone("overlay-home", 210, x, y, w, h,
            [actReg, overlayItemPtr](int, float, float, aa::TouchEvent event) {
                if (event == aa::TouchEvent::Down) {
                    QMetaObject::invokeMethod(qApp, [actReg]() {
                        if (actReg) actReg->dispatch("app.home");
                    }, Qt::QueuedConnection);
                    QMetaObject::invokeMethod(overlayItemPtr, "dismiss", Qt::QueuedConnection);
                }
            });
    }

    // Theme toggle button
    if (getRect("overlayThemeBtn", x, y, w, h)) {
        auto* actReg = actionRegistry_;
        coordBridge_->updateZone("overlay-theme", 210, x, y, w, h,
            [actReg](int, float, float, aa::TouchEvent event) {
                if (event == aa::TouchEvent::Down) {
                    QMetaObject::invokeMethod(qApp, [actReg]() {
                        if (actReg) actReg->dispatch("theme.toggle");
                    }, Qt::QueuedConnection);
                }
            });
    }

    // Close button
    if (getRect("overlayCloseBtn", x, y, w, h)) {
        auto* overlayItemPtr = overlayItem;
        coordBridge_->updateZone("overlay-close", 210, x, y, w, h,
            [overlayItemPtr](int, float, float, aa::TouchEvent event) {
                if (event == aa::TouchEvent::Down) {
                    QMetaObject::invokeMethod(overlayItemPtr, "dismiss", Qt::QueuedConnection);
                }
            });
    }
}

void GestureOverlayController::cleanupZones()
{
    overlayVisible_ = false;
    if (!coordBridge_) return;

    for (const auto& id : managedZoneIds()) {
        coordBridge_->removeZone(id);
    }
}

} // namespace oap
