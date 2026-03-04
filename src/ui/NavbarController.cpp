#include "ui/NavbarController.hpp"
#include "core/aa/TouchRouter.hpp"
#include <QMetaObject>

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
                emit gestureTriggered(i, LongHold);
                progressTimers_[i]->stop();
                emit holdProgress(i, 1.0);
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
    popupDismissTimer_->start();
    emit popupChanged();
}

void NavbarController::hidePopup()
{
    if (!popupVisible_)
        return;

    popupVisible_ = false;
    popupControlIndex_ = -1;
    popupDismissTimer_->stop();
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

// --- Private ---

void NavbarController::resetControlState(int index)
{
    controls_[index].pressed = false;
    controls_[index].longHoldFired = false;
    controls_[index].activeSlot = -1;
}

} // namespace oap
