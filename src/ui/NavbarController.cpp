#include "ui/NavbarController.hpp"
#include "core/aa/TouchRouter.hpp"
#include "core/aa/EvdevCoordBridge.hpp"
#include "core/services/ActionRegistry.hpp"
#include <QMetaObject>
#include <QVariantMap>
#include <algorithm>
#include <cmath>

namespace oap {

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

    auto& state = controls_[controlIndex];

    // Ignore if already pressed (duplicate press)
    if (state.pressed)
        return;

    state.pressed = true;
    state.longHoldFired = false;
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
        if (elapsed <= tapMaxMs_) {
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

void NavbarController::onZoneTouch(int controlIndex, int slot, float x, float y,
                                    oap::aa::TouchEvent event)
{
    // Marshal to main thread
    QMetaObject::invokeMethod(this, [this, controlIndex, slot, event]() {
        switch (event) {
        case aa::TouchEvent::Down:
            handlePress(controlIndex);
            controls_[controlIndex].activeSlot = slot;
            break;
        case aa::TouchEvent::Up:
            if (controls_[controlIndex].activeSlot == slot)
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

    // Re-register zones at new positions if they were registered
    if (zonesRegistered_ && coordBridge_) {
        registerZones(displayWidth_, displayHeight_);
    }
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

    popupVisible_ = true;
    popupControlIndex_ = controlIndex;
    activePopupGeneration_ = popupGeneration_;
    popupDismissTimer_->start();

    if (coordBridge_ && zonesRegistered_)
        registerPopupZones(controlIndex);

    emit popupChanged();
}

void NavbarController::hidePopup()
{
    if (!popupVisible_)
        return;

    popupVisible_ = false;
    popupControlIndex_ = -1;
    popupDismissTimer_->stop();

    if (coordBridge_)
        unregisterPopupZones();

    emit popupChanged();
}

// --- Control role mapping ---

QString NavbarController::controlRole(int controlIndex) const
{
    // Layout: driver side (0), center (1), passenger side (2)
    // LHD: driver=left=volume, passenger=right=brightness
    // RHD: driver=left=brightness, passenger=right=volume (swap 0 and 2)
    if (controlIndex == 1)
        return QStringLiteral("clock");

    if (leftHandDrive_) {
        return (controlIndex == 0) ? QStringLiteral("volume") : QStringLiteral("brightness");
    } else {
        return (controlIndex == 0) ? QStringLiteral("brightness") : QStringLiteral("volume");
    }
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

// --- Zone registration ---

void NavbarController::registerZones(int displayWidth, int displayHeight)
{
    if (!coordBridge_)
        return;

    displayWidth_ = displayWidth;
    displayHeight_ = displayHeight;
    zonesRegistered_ = true;

    // Zone IDs for 3 controls
    static const std::string zoneIds[3] = {
        "navbar-driver", "navbar-center", "navbar-passenger"
    };

    bool isVertical = (edge_ == "left" || edge_ == "right");

    for (int i = 0; i < 3; ++i) {
        float px, py, pw, ph;

        if (!isVertical) {
            // Horizontal bar (top/bottom)
            float totalW = static_cast<float>(displayWidth);
            float quarterW = totalW / 4.0f;
            float halfW = totalW / 2.0f;

            if (i == 0) {          // driver (left 1/4)
                px = 0; pw = quarterW;
            } else if (i == 1) {   // center (middle 1/2)
                px = quarterW; pw = halfW;
            } else {               // passenger (right 1/4)
                px = quarterW + halfW; pw = quarterW;
            }

            ph = static_cast<float>(BAR_THICK);
            if (edge_ == "bottom") {
                py = static_cast<float>(displayHeight - BAR_THICK);
            } else {  // top
                py = 0;
            }
        } else {
            // Vertical bar (left/right)
            float totalH = static_cast<float>(displayHeight);
            float quarterH = totalH / 4.0f;
            float halfH = totalH / 2.0f;

            if (i == 0) {          // driver (top 1/4)
                py = 0; ph = quarterH;
            } else if (i == 1) {   // center (middle 1/2)
                py = quarterH; ph = halfH;
            } else {               // passenger (bottom 1/4)
                py = quarterH + halfH; ph = quarterH;
            }

            pw = static_cast<float>(BAR_THICK);
            if (edge_ == "left") {
                px = 0;
            } else {  // right
                px = static_cast<float>(displayWidth - BAR_THICK);
            }
        }

        int controlIdx = i;
        coordBridge_->updateZone(
            zoneIds[i], 50, px, py, pw, ph,
            [this, controlIdx](int slot, float x, float y, aa::TouchEvent event) {
                onZoneTouch(controlIdx, slot, x, y, event);
            });
    }
}

void NavbarController::unregisterZones()
{
    if (!coordBridge_)
        return;

    coordBridge_->removeZone(std::string("navbar-driver"));
    coordBridge_->removeZone(std::string("navbar-center"));
    coordBridge_->removeZone(std::string("navbar-passenger"));
    unregisterPopupZones();
    zonesRegistered_ = false;
}

void NavbarController::registerPopupZones(int controlIndex)
{
    if (!coordBridge_)
        return;

    // Screen-wide dismiss zone at priority 40
    coordBridge_->updateZone(
        std::string("navbar-popup-dismiss"), 40,
        0, 0,
        static_cast<float>(displayWidth_), static_cast<float>(displayHeight_),
        [this](int /*slot*/, float /*x*/, float /*y*/, aa::TouchEvent event) {
            if (event == aa::TouchEvent::Up) {
                QMetaObject::invokeMethod(this, [this]() {
                    hidePopup();
                }, Qt::QueuedConnection);
            }
        });

    // Popup slider zone at priority 60 (higher than dismiss)
    // Positioned adjacent to the triggering control, extending into content area
    // Exact position will be refined by QML layout; for now use a reasonable area
    float popupX = 0, popupY = 0, popupW = 0, popupH = 0;
    bool isVertical = (edge_ == "left" || edge_ == "right");

    if (!isVertical) {
        float quarterW = static_cast<float>(displayWidth_) / 4.0f;
        if (controlIndex == 0) {
            popupX = 0;
        } else if (controlIndex == 1) {
            popupX = quarterW;
        } else {
            popupX = quarterW + static_cast<float>(displayWidth_) / 2.0f;
        }
        popupW = (controlIndex == 1) ? static_cast<float>(displayWidth_) / 2.0f : quarterW;
        popupH = 200;  // popup height into content
        if (edge_ == "bottom") {
            popupY = static_cast<float>(displayHeight_ - BAR_THICK) - popupH;
        } else {
            popupY = static_cast<float>(BAR_THICK);
        }
    } else {
        float quarterH = static_cast<float>(displayHeight_) / 4.0f;
        if (controlIndex == 0) {
            popupY = 0;
        } else if (controlIndex == 1) {
            popupY = quarterH;
        } else {
            popupY = quarterH + static_cast<float>(displayHeight_) / 2.0f;
        }
        popupH = (controlIndex == 1) ? static_cast<float>(displayHeight_) / 2.0f : quarterH;
        popupW = 200;  // popup width into content
        if (edge_ == "left") {
            popupX = static_cast<float>(BAR_THICK);
        } else {
            popupX = static_cast<float>(displayWidth_ - BAR_THICK) - popupW;
        }
    }

    int ctrlIdx = controlIndex;
    float pY = popupY, pH = popupH;
    coordBridge_->updateZone(
        std::string("navbar-popup-slider"), 60,
        popupX, popupY, popupW, popupH,
        [this, ctrlIdx, pY, pH](int /*slot*/, float /*x*/, float y, aa::TouchEvent event) {
            if (event == aa::TouchEvent::Move || event == aa::TouchEvent::Down) {
                // Convert raw evdev Y to a 0-1 slider value based on popup geometry
                float normalized = std::clamp((y - pY) / pH, 0.0f, 1.0f);
                // Invert: top of popup = max value, bottom = min (slider grows upward)
                if (edge_ == "bottom" || edge_ == "right")
                    normalized = 1.0f - normalized;

                QMetaObject::invokeMethod(this, [this, ctrlIdx, normalized]() {
                    // Direct service calls for EVIOCGRAB mode (QML sliders can't receive touches)
                    QString role = controlRole(ctrlIdx);
                    if (role == "volume" && audioService_) {
                        int volume = static_cast<int>(normalized * 100.0f);
                        QMetaObject::invokeMethod(audioService_, "setMasterVolume",
                                                  Qt::QueuedConnection, Q_ARG(int, volume));
                    } else if (role == "brightness" && displayService_) {
                        int brightness = 5 + static_cast<int>(normalized * 95.0f);
                        QMetaObject::invokeMethod(displayService_, "setBrightness",
                                                  Qt::QueuedConnection, Q_ARG(int, brightness));
                    }
                    // Also emit for QML popup visual updates in non-grab mode
                    emit popupDrag(ctrlIdx, normalized);
                }, Qt::QueuedConnection);
            }
        });
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
    Q_UNUSED(controlIndex)
    return ++popupGeneration_;
}

void NavbarController::setPopupRegions(int controlIndex, qint64 generation,
                                        const QVariantList& regions)
{
    if (!coordBridge_ || generation != activePopupGeneration_)
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
        [this](int /*slot*/, float /*x*/, float /*y*/, aa::TouchEvent event) {
            if (event == aa::TouchEvent::Up) {
                QMetaObject::invokeMethod(this, [this]() {
                    hidePopup();
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
                [this, target, minVal, maxVal, invertAxis, evY0, evRange]
                (int /*slot*/, float /*x*/, float evY, aa::TouchEvent event) {
                    if (event == aa::TouchEvent::Move || event == aa::TouchEvent::Down) {
                        float normalized = (evRange > 0)
                            ? std::clamp((evY - evY0) / evRange, 0.0f, 1.0f)
                            : 0.5f;
                        if (invertAxis)
                            normalized = 1.0f - normalized;

                        int value = minVal + static_cast<int>(normalized * (maxVal - minVal));

                        QMetaObject::invokeMethod(this, [this, target, value]() {
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

            coordBridge_->updateZone(
                zoneId, 60, x, y, w, h,
                [this, action, tapSlopEvdev]
                (int /*slot*/, float evX, float evY, aa::TouchEvent event) {
                    static float downX = 0, downY = 0;
                    if (event == aa::TouchEvent::Down) {
                        downX = evX;
                        downY = evY;
                    } else if (event == aa::TouchEvent::Up) {
                        float dx = evX - downX;
                        float dy = evY - downY;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        if (dist <= tapSlopEvdev) {
                            QMetaObject::invokeMethod(this, [this, action]() {
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
    Q_UNUSED(controlIndex)
    if (generation != activePopupGeneration_ && activePopupGeneration_ != 0)
        return;  // stale generation, ignore

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
    controls_[index].activeSlot = -1;
    emit holdProgress(index, 0.0);
}

} // namespace oap
