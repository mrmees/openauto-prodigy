#include "ui/NavbarController.hpp"
#include "core/aa/TouchRouter.hpp"
#include "core/aa/EvdevCoordBridge.hpp"
#include "core/services/ActionRegistry.hpp"
#include <QMetaObject>
#include <QVariantMap>
#include <algorithm>
#include <array>
#include <cmath>

namespace oap {

namespace {

constexpr int MAX_TOUCH_SLOTS = 10;

struct PopupButtonPressState {
    struct Slot {
        bool pressed = false;
        float x = 0.0f;
        float y = 0.0f;
    };
    std::array<Slot, MAX_TOUCH_SLOTS> presses;
};

} // namespace

NavbarController::NavbarController(QObject* parent)
    : QObject(parent)
{
    // Initialize per-control timers
    for (int i = 0; i < 3; ++i) {
        longHoldTimers_[i] = new QTimer(this);
        longHoldTimers_[i]->setSingleShot(true);

        progressTimers_[i] = new QTimer(this);
        progressTimers_[i]->setInterval(16);  // ~60fps for smooth feedback

        // Long hold fires at threshold (no release needed)
        connect(longHoldTimers_[i], &QTimer::timeout, this, [this, i]() {
            if (controls_[i].pressed) {
                controls_[i].longHoldFired = true;
                progressTimers_[i]->stop();
                emit gestureTriggered(i, LongHold);
                emit holdProgress(i, 0.0);
            }
        });

        // Progress updates during hold
        connect(progressTimers_[i], &QTimer::timeout, this, [this, i]() {
            if (controls_[i].pressed && !controls_[i].longHoldFired) {
                qint64 elapsed = controls_[i].pressTimer.elapsed();
                qreal progress = qBound(0.0, static_cast<qreal>(elapsed) / shortHoldMaxMs_, 1.0);
                emit holdProgress(i, progress);
            }
        });
    }

    // Popup auto-dismiss timer (7 seconds)
    popupDismissTimer_ = new QTimer(this);
    popupDismissTimer_->setSingleShot(true);
    popupDismissTimer_->setInterval(7000);
    connect(popupDismissTimer_, &QTimer::timeout, this, &NavbarController::hidePopup);

    // Wire gesture signals to action dispatch
    connect(this, &NavbarController::gestureTriggered,
            this, &NavbarController::dispatchAction);
}

NavbarController::~NavbarController() = default;

// --- QML input handlers ---

void NavbarController::handlePress(int controlIndex)
{
    if (controlIndex < 0 || controlIndex >= 3)
        return;

    // In widget interaction mode, side controls are tap-only -- no hold timers or progress
    if (widgetInteractionMode_ && controlIndex != 1) {
        auto& state = controls_[controlIndex];
        if (state.pressed) return;
        state.pressed = true;
        state.longHoldFired = false;
        state.pressedInWidgetMode = true;
        state.pressTimer.start();
        // Do NOT start longHoldTimers_ or progressTimers_
        return;
    }

    auto& state = controls_[controlIndex];

    // Ignore if already pressed (duplicate press)
    if (state.pressed)
        return;

    state.pressed = true;
    state.longHoldFired = false;
    state.pressedInWidgetMode = false;
    state.pressTimer.start();

    // Start long-hold timer
    longHoldTimers_[controlIndex]->start(shortHoldMaxMs_);

    // Start progress updates
    progressTimers_[controlIndex]->start();
}

void NavbarController::handleRelease(int controlIndex)
{
    if (controlIndex < 0 || controlIndex >= 3)
        return;

    auto& state = controls_[controlIndex];
    if (!state.pressed)
        return;

    qint64 elapsed = state.pressTimer.elapsed();

    // Stop timers
    longHoldTimers_[controlIndex]->stop();
    progressTimers_[controlIndex]->stop();

    // Only emit if long hold hasn't already fired
    if (!state.longHoldFired) {
        // Widget interaction mode: side controls are ALWAYS tap, never shortHold
        // Use pressedInWidgetMode (captured at press time) to handle mode transitions during press
        if (state.pressedInWidgetMode && controlIndex != 1) {
            emit gestureTriggered(controlIndex, Tap);
        } else if (elapsed <= tapMaxMs_) {
            emit gestureTriggered(controlIndex, Tap);
        } else {
            // Between tap and long-hold thresholds = short hold
            emit gestureTriggered(controlIndex, ShortHold);
        }
    }

    resetControlState(controlIndex);
}

void NavbarController::handleCancel(int controlIndex)
{
    if (controlIndex < 0 || controlIndex >= 3)
        return;

    longHoldTimers_[controlIndex]->stop();
    progressTimers_[controlIndex]->stop();
    resetControlState(controlIndex);
}

void NavbarController::onZoneTouch(int controlIndex, qint64 geometryGeneration,
                                    int slot, float x, float y,
                                    oap::aa::TouchEvent event)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    if (controlIndex < 0 || controlIndex >= static_cast<int>(controls_.size()))
        return;

    // Marshal to main thread
    QMetaObject::invokeMethod(this, [this, controlIndex, geometryGeneration, slot, event]() {
        if (geometryGeneration != activeNavbarGeometryGeneration_)
            return;
        auto& state = controls_[controlIndex];
        switch (event) {
        case aa::TouchEvent::Down:
            if (state.pressed)
                return;
            handlePress(controlIndex);
            if (state.pressed)
                state.activeSlot = slot;
            break;
        case aa::TouchEvent::Up:
            if (state.activeSlot == slot)
                handleRelease(controlIndex);
            break;
        case aa::TouchEvent::Move:
            // Move events within the zone are fine; no action needed
            break;
        }
    }, Qt::QueuedConnection);
}

// --- Properties ---

QString NavbarController::edge() const
{
    return edge_;
}

void NavbarController::setEdge(const QString& edge)
{
    if (edge_ == edge)
        return;
    edge_ = edge;
    emit edgeChanged();

    // The old report describes the previous rendered edge. QML publishes the
    // replacement rectangles after its anchors and dimensions settle.
    beginNavbarGeometryUpdate();
}

bool NavbarController::leftHandDrive() const
{
    return leftHandDrive_;
}

void NavbarController::setLeftHandDrive(bool lhd)
{
    if (leftHandDrive_ == lhd)
        return;
    leftHandDrive_ = lhd;
    emit layoutChanged();
}

bool NavbarController::popupVisible() const
{
    return popupVisible_;
}

int NavbarController::popupControlIndex() const
{
    return popupControlIndex_;
}

// --- Popup management ---

void NavbarController::showPopup(int controlIndex)
{
    if (controlIndex < 0 || controlIndex >= 3)
        return;

    if (popupVisible_ && popupControlIndex_ == controlIndex) {
        popupDismissTimer_->start();
        return;
    }

    // Publish the incoming session before popupChanged can make the outgoing
    // QML item clear its regions. That stale clear must not hide this popup.
    activePopupGeneration_ = ++popupGeneration_;
    activePopupSessionControlIndex_ = controlIndex;
    unregisterPopupRegionZones();

    popupVisible_ = true;
    popupControlIndex_ = controlIndex;
    popupDismissTimer_->start();

    // Don't register hardcoded zones — QML will call setPopupRegions()
    // after receiving the popupChanged signal

    emit popupChanged();
}

void NavbarController::hidePopup()
{
    if (!popupVisible_)
        return;

    popupVisible_ = false;
    popupControlIndex_ = -1;
    activePopupGeneration_ = 0;
    activePopupSessionControlIndex_ = -1;
    popupDismissTimer_->stop();

    if (coordBridge_)
        unregisterPopupZones();

    emit popupChanged();
}

// --- Control role mapping ---

QString NavbarController::controlRole(int controlIndex) const
{
    // Center is always clock, regardless of mode
    if (controlIndex == 1)
        return QStringLiteral("clock");

    // Widget interaction mode: gear = driver side, trash = passenger side
    if (widgetInteractionMode_) {
        if (leftHandDrive_) {
            return (controlIndex == 0) ? QStringLiteral("gear") : QStringLiteral("trash");
        } else {
            return (controlIndex == 0) ? QStringLiteral("trash") : QStringLiteral("gear");
        }
    }

    // Normal mode: volume = driver side, brightness = passenger side
    // LHD: driver=left=volume, passenger=right=brightness
    // RHD: driver=left=brightness, passenger=right=volume (swap 0 and 2)
    if (leftHandDrive_) {
        return (controlIndex == 0) ? QStringLiteral("volume") : QStringLiteral("brightness");
    } else {
        return (controlIndex == 0) ? QStringLiteral("brightness") : QStringLiteral("volume");
    }
}

// --- Widget interaction mode ---

bool NavbarController::widgetInteractionMode() const
{
    return widgetInteractionMode_;
}

void NavbarController::setWidgetInteractionMode(bool mode)
{
    if (widgetInteractionMode_ == mode)
        return;
    widgetInteractionMode_ = mode;

    // Cancel any active press state on all controls when entering widget mode
    if (mode) {
        for (int i = 0; i < 3; ++i)
            handleCancel(i);
    }

    emit widgetInteractionModeChanged();
}

QString NavbarController::widgetDisplayName() const
{
    return widgetDisplayName_;
}

void NavbarController::setWidgetDisplayName(const QString& name)
{
    if (widgetDisplayName_ == name)
        return;
    widgetDisplayName_ = name;
    emit widgetInteractionModeChanged();
}

bool NavbarController::selectedWidgetHasConfig() const
{
    return selectedWidgetHasConfig_;
}

void NavbarController::setSelectedWidgetHasConfig(bool has)
{
    if (selectedWidgetHasConfig_ == has)
        return;
    selectedWidgetHasConfig_ = has;
    emit widgetInteractionModeChanged();
}

bool NavbarController::selectedWidgetIsSingleton() const
{
    return selectedWidgetIsSingleton_;
}

void NavbarController::setSelectedWidgetIsSingleton(bool is)
{
    if (selectedWidgetIsSingleton_ == is)
        return;
    selectedWidgetIsSingleton_ = is;
    emit widgetInteractionModeChanged();
}

// --- Gesture timing ---

int NavbarController::tapMaxMs() const { return tapMaxMs_; }
void NavbarController::setTapMaxMs(int ms) { tapMaxMs_ = ms; }
int NavbarController::shortHoldMaxMs() const { return shortHoldMaxMs_; }
void NavbarController::setShortHoldMaxMs(int ms) { shortHoldMaxMs_ = ms; }

// --- External dependencies ---

void NavbarController::setCoordBridge(oap::aa::EvdevCoordBridge* bridge)
{
    coordBridge_ = bridge;
}

void NavbarController::setActionRegistry(ActionRegistry* registry)
{
    actionRegistry_ = registry;
}

void NavbarController::setAudioService(QObject* svc)
{
    audioService_ = svc;
}

void NavbarController::setDisplayService(QObject* svc)
{
    displayService_ = svc;
}

// --- Rendered navbar geometry ---

qint64 NavbarController::beginNavbarGeometryUpdate()
{
    activeNavbarGeometryGeneration_ = ++navbarGeometryGeneration_;
    unregisterNavbarZones();
    return activeNavbarGeometryGeneration_;
}

void NavbarController::setNavbarGeometry(qint64 generation,
                                         int displayWidth, int displayHeight,
                                         const QVariantList& regions)
{
    if (generation != activeNavbarGeometryGeneration_ || !coordBridge_)
        return;

    struct Rect {
        float x;
        float y;
        float w;
        float h;
    };
    std::array<Rect, 3> parsed{};
    bool valid = displayWidth > 0 && displayHeight > 0 && regions.size() == 3;
    constexpr double BOUNDS_EPSILON = 0.5;
    for (int i = 0; valid && i < static_cast<int>(parsed.size()); ++i) {
        const QVariantMap region = regions.at(i).toMap();
        bool xOk = false;
        bool yOk = false;
        bool wOk = false;
        bool hOk = false;
        const double x = region.value(QStringLiteral("x")).toDouble(&xOk);
        const double y = region.value(QStringLiteral("y")).toDouble(&yOk);
        const double w = region.value(QStringLiteral("w")).toDouble(&wOk);
        const double h = region.value(QStringLiteral("h")).toDouble(&hOk);
        valid = xOk && yOk && wOk && hOk
             && std::isfinite(x) && std::isfinite(y)
             && std::isfinite(w) && std::isfinite(h)
             && x >= 0.0 && y >= 0.0 && w > 0.0 && h > 0.0
             && x + w <= static_cast<double>(displayWidth) + BOUNDS_EPSILON
             && y + h <= static_cast<double>(displayHeight) + BOUNDS_EPSILON;
        if (valid) {
            parsed[i] = {static_cast<float>(x), static_cast<float>(y),
                         static_cast<float>(w), static_cast<float>(h)};
        }
    }

    if (!valid) {
        unregisterNavbarZones();
        return;
    }

    displayWidth_ = displayWidth;
    displayHeight_ = displayHeight;
    static const std::array<std::string, 3> zoneIds = {
        "navbar-driver", "navbar-center", "navbar-passenger"
    };
    for (int i = 0; i < static_cast<int>(parsed.size()); ++i) {
        const Rect& rect = parsed[i];
        const qint64 geometryGeneration = generation;
        coordBridge_->updateZone(
            zoneIds[i], 50, rect.x, rect.y, rect.w, rect.h,
            [this, i, geometryGeneration]
            (int slot, float x, float y, aa::TouchEvent event) {
                onZoneTouch(i, geometryGeneration, slot, x, y, event);
            });
    }
}

void NavbarController::unregisterZones()
{
    activeNavbarGeometryGeneration_ = ++navbarGeometryGeneration_;
    unregisterNavbarZones();
    unregisterPopupZones();
}

void NavbarController::unregisterNavbarZones()
{
    for (int i = 0; i < static_cast<int>(controls_.size()); ++i) {
        if (controls_[i].pressed)
            handleCancel(i);
    }
    if (!coordBridge_)
        return;
    coordBridge_->removeZone(std::string("navbar-driver"));
    coordBridge_->removeZone(std::string("navbar-center"));
    coordBridge_->removeZone(std::string("navbar-passenger"));
}

void NavbarController::unregisterPopupZones()
{
    if (!coordBridge_)
        return;
    coordBridge_->removeZone(std::string("navbar-popup-dismiss"));
    coordBridge_->removeZone(std::string("navbar-popup-slider"));
    unregisterPopupRegionZones();
}

// --- Popup session API ---

qint64 NavbarController::beginPopupSession(int controlIndex)
{
    if (popupVisible_ && controlIndex == popupControlIndex_
        && controlIndex == activePopupSessionControlIndex_) {
        return activePopupGeneration_;
    }

    unregisterPopupRegionZones();
    activePopupGeneration_ = ++popupGeneration_;
    activePopupSessionControlIndex_ = controlIndex;
    return activePopupGeneration_;
}

void NavbarController::setPopupRegions(int controlIndex, qint64 generation,
                                        const QVariantList& regions)
{
    if (!coordBridge_ || !popupVisible_
        || controlIndex != popupControlIndex_
        || controlIndex != activePopupSessionControlIndex_
        || generation != activePopupGeneration_)
        return;

    // Region types matching QML enum
    static constexpr int REGION_TYPE_SLIDER = 0;
    static constexpr int REGION_TYPE_BUTTON = 1;
    static constexpr float TAP_SLOP_PX = 15.0f;

    // Remove any previously registered popup region zones
    unregisterPopupRegionZones();

    // Also remove the hardcoded popup zones from showPopup (Pass 1 fallback)
    coordBridge_->removeZone(std::string("navbar-popup-slider"));
    coordBridge_->removeZone(std::string("navbar-popup-dismiss"));

    // Register dismiss zone (full screen, priority 40, fires on Up)
    std::string dismissId = "popup." + std::to_string(controlIndex) + "."
                           + std::to_string(generation) + ".dismiss";
    popupRegionZoneIds_.push_back(dismissId);
    coordBridge_->updateZone(
        dismissId, 40,
        0, 0,
        static_cast<float>(displayWidth_), static_cast<float>(displayHeight_),
        [this, controlIndex, generation]
        (int /*slot*/, float /*x*/, float /*y*/, aa::TouchEvent event) {
            if (event == aa::TouchEvent::Up) {
                QMetaObject::invokeMethod(this, [this, controlIndex, generation]() {
                    if (popupVisible_ && popupControlIndex_ == controlIndex
                        && activePopupGeneration_ == generation) {
                        hidePopup();
                    }
                }, Qt::QueuedConnection);
            }
        });

    // Register each interactive region
    for (const QVariant& regionVar : regions) {
        QVariantMap region = regionVar.toMap();

        if (!region.contains("id") || !region.contains("type") ||
            !region.contains("x") || !region.contains("y") ||
            !region.contains("w") || !region.contains("h")) {
            qWarning("NavbarController: invalid popup region (missing fields), skipping");
            continue;
        }

        QString id = region["id"].toString();
        int type = region["type"].toInt();
        float x = region["x"].toFloat();
        float y = region["y"].toFloat();
        float w = region["w"].toFloat();
        float h = region["h"].toFloat();

        std::string zoneId = "popup." + std::to_string(controlIndex) + "."
                            + std::to_string(generation) + "." + id.toStdString();
        popupRegionZoneIds_.push_back(zoneId);

        if (type == REGION_TYPE_SLIDER) {
            int target = region.value("target", 0).toInt();
            int minVal = region.value("min", 0).toInt();
            int maxVal = region.value("max", 100).toInt();
            bool invertAxis = region.value("invertAxis", true).toBool();

            // Pre-compute evdev-space bounds for normalization
            float evY0 = coordBridge_->pixelToEvdevY(y);
            float evY1 = coordBridge_->pixelToEvdevY(y + h);
            float evRange = evY1 - evY0;

            coordBridge_->updateZone(
                zoneId, 60, x, y, w, h,
                [this, controlIndex, generation, target, minVal, maxVal,
                 invertAxis, evY0, evRange]
                (int /*slot*/, float /*x*/, float evY, aa::TouchEvent event) {
                    if (event == aa::TouchEvent::Move || event == aa::TouchEvent::Down) {
                        float normalized = (evRange > 0)
                            ? std::clamp((evY - evY0) / evRange, 0.0f, 1.0f)
                            : 0.5f;
                        if (invertAxis)
                            normalized = 1.0f - normalized;

                        int value = minVal + static_cast<int>(normalized * (maxVal - minVal));

                        QMetaObject::invokeMethod(
                            this, [this, controlIndex, generation, target, value]() {
                            if (!popupVisible_ || popupControlIndex_ != controlIndex
                                || activePopupGeneration_ != generation) {
                                return;
                            }
                            if (target == 0 && audioService_) {
                                QMetaObject::invokeMethod(audioService_, "setMasterVolume",
                                                          Qt::QueuedConnection, Q_ARG(int, value));
                            } else if (target == 1 && displayService_) {
                                QMetaObject::invokeMethod(displayService_, "setBrightness",
                                                          Qt::QueuedConnection, Q_ARG(int, value));
                            }
                            bumpPopupDismissTimer();
                        }, Qt::QueuedConnection);
                    }
                });

        } else if (type == REGION_TYPE_BUTTON) {
            QString action = region.value("action").toString();
            float tapSlopEvdev = coordBridge_->pixelToEvdevY(TAP_SLOP_PX);
            auto pressState = std::make_shared<PopupButtonPressState>();

            coordBridge_->updateZone(
                zoneId, 60, x, y, w, h,
                [this, action, controlIndex, generation, tapSlopEvdev, pressState]
                (int slot, float evX, float evY, aa::TouchEvent event) {
                    if (slot < 0 || slot >= static_cast<int>(pressState->presses.size()))
                        return;
                    auto& press = pressState->presses[slot];
                    if (event == aa::TouchEvent::Down) {
                        press.pressed = true;
                        press.x = evX;
                        press.y = evY;
                    } else if (event == aa::TouchEvent::Up) {
                        if (!press.pressed)
                            return;
                        press.pressed = false;
                        float dx = evX - press.x;
                        float dy = evY - press.y;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        if (dist <= tapSlopEvdev) {
                            QMetaObject::invokeMethod(
                                this, [this, action, controlIndex, generation]() {
                                if (!popupVisible_ || popupControlIndex_ != controlIndex
                                    || activePopupGeneration_ != generation) {
                                    return;
                                }
                                hidePopup();
                                if (actionRegistry_) {
                                    actionRegistry_->dispatch(
                                        QStringLiteral("app.%1").arg(action));
                                }
                            }, Qt::QueuedConnection);
                        }
                    }
                });
        }
    }
}

void NavbarController::clearPopupRegions(int controlIndex, qint64 generation)
{
    if (!popupVisible_ || controlIndex != popupControlIndex_
        || controlIndex != activePopupSessionControlIndex_
        || generation != activePopupGeneration_) {
        return;
    }
    hidePopup();
}

void NavbarController::bumpPopupDismissTimer()
{
    if (popupVisible_ && popupDismissTimer_)
        popupDismissTimer_->start();  // restart the 7-second timer
}

void NavbarController::unregisterPopupRegionZones()
{
    if (!coordBridge_)
        return;
    for (const auto& zoneId : popupRegionZoneIds_) {
        coordBridge_->removeZone(zoneId);
    }
    popupRegionZoneIds_.clear();
}

// --- Action dispatch ---

void NavbarController::dispatchAction(int controlIndex, int gesture)
{
    if (!actionRegistry_)
        return;

    QString role = controlRole(controlIndex);
    QString gestureStr;
    switch (gesture) {
    case Tap:       gestureStr = QStringLiteral("tap"); break;
    case ShortHold: gestureStr = QStringLiteral("shortHold"); break;
    case LongHold:  gestureStr = QStringLiteral("longHold"); break;
    default: return;
    }

    QString actionId = QStringLiteral("navbar.%1.%2").arg(role, gestureStr);
    actionRegistry_->dispatch(actionId);
}

// --- Private ---

void NavbarController::resetControlState(int index)
{
    controls_[index].pressed = false;
    controls_[index].longHoldFired = false;
    controls_[index].pressedInWidgetMode = false;
    controls_[index].activeSlot = -1;
    emit holdProgress(index, 0.0);
}

} // namespace oap
