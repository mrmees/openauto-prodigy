#include "ui/SettingsInputBoundary.hpp"

#include <algorithm>
#include <QEvent>
#include <QEventPoint>
#include <QGuiApplication>
#include <QLineF>
#include <QMouseEvent>
#include <QStyleHints>
#include <QTouchEvent>

namespace oap {

namespace {

constexpr int kLongPressIntervalMs = 500;
constexpr qreal kMinimumDragThreshold = 12.0;

qreal dragThreshold()
{
    const auto* hints = QGuiApplication::styleHints();
    if (!hints)
        return kMinimumDragThreshold;
    return std::max<qreal>(hints->startDragDistance(), kMinimumDragThreshold);
}

} // namespace

SettingsInputBoundary::SettingsInputBoundary(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFiltersChildMouseEvents(true);
    setAcceptTouchEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);

    holdTimer_.setSingleShot(true);
    holdTimer_.setInterval(kLongPressIntervalMs);
    connect(&holdTimer_, &QTimer::timeout, this, &SettingsInputBoundary::triggerLongPress);
}

void SettingsInputBoundary::mousePressEvent(QMouseEvent* event)
{
    const bool swallow = handleMouseEvent(event);
    event->setAccepted(swallow || event->button() == Qt::LeftButton);
}

void SettingsInputBoundary::mouseMoveEvent(QMouseEvent* event)
{
    const bool swallow = handleMouseEvent(event);
    event->setAccepted(swallow);
}

void SettingsInputBoundary::mouseReleaseEvent(QMouseEvent* event)
{
    const bool swallow = handleMouseEvent(event);
    event->setAccepted(swallow);
}

void SettingsInputBoundary::mouseUngrabEvent()
{
    clearActivePress();
}

void SettingsInputBoundary::touchEvent(QTouchEvent* event)
{
    const bool swallow = handleTouchEvent(event);
    event->setAccepted(swallow || !event->points().isEmpty());
}

void SettingsInputBoundary::touchUngrabEvent()
{
    clearActivePress();
}

bool SettingsInputBoundary::childMouseEventFilter(QQuickItem*, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
        return handleMouseEvent(static_cast<QMouseEvent*>(event));
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        return handleTouchEvent(static_cast<QTouchEvent*>(event));
    case QEvent::UngrabMouse:
        clearActivePress();
        return false;
    default:
        return false;
    }
}

void SettingsInputBoundary::armMouse(const QPointF& scenePos)
{
    clearActivePress();
    activePointer_ = PointerKind::Mouse;
    activeTouchPointId_ = -1;
    pressScenePos_ = scenePos;
    pressActive_ = true;
    longPressTriggered_ = false;
    swallowPointerEvents_ = false;
    holdTimer_.start();
    emit pressStarted(mapFromScene(scenePos));
}

void SettingsInputBoundary::armTouch(int pointId, const QPointF& scenePos)
{
    clearActivePress();
    activePointer_ = PointerKind::Touch;
    activeTouchPointId_ = pointId;
    pressScenePos_ = scenePos;
    pressActive_ = true;
    longPressTriggered_ = false;
    swallowPointerEvents_ = false;
    holdTimer_.start();
    emit pressStarted(mapFromScene(scenePos));
}

void SettingsInputBoundary::clearActivePress()
{
    const bool hadActivePress = pressActive_ || holdTimer_.isActive() || swallowPointerEvents_;
    holdTimer_.stop();
    activePointer_ = PointerKind::None;
    activeTouchPointId_ = -1;
    pressScenePos_ = {};
    pressActive_ = false;
    longPressTriggered_ = false;
    swallowPointerEvents_ = false;

    if (hadActivePress)
        emit pressEnded();
}

void SettingsInputBoundary::maybeCancelForMovement(const QPointF& scenePos)
{
    if (!pressActive_ || longPressTriggered_)
        return;

    if (QLineF(scenePos, pressScenePos_).length() >= dragThreshold())
        clearActivePress();
}

void SettingsInputBoundary::triggerLongPress()
{
    if (!pressActive_ || longPressTriggered_)
        return;

    holdTimer_.stop();
    longPressTriggered_ = true;
    swallowPointerEvents_ = true;
    emit longPressed(mapFromScene(pressScenePos_));
}

bool SettingsInputBoundary::handleMouseEvent(QMouseEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (event->button() != Qt::LeftButton)
            return false;
        armMouse(event->scenePosition());
        return false;

    case QEvent::MouseMove:
        if (activePointer_ != PointerKind::Mouse)
            return swallowPointerEvents_;
        if (swallowPointerEvents_)
            return true;
        maybeCancelForMovement(event->scenePosition());
        return false;

    case QEvent::MouseButtonRelease: {
        if (activePointer_ != PointerKind::Mouse)
            return swallowPointerEvents_;

        const bool swallow = swallowPointerEvents_;
        clearActivePress();
        return swallow;
    }

    default:
        return false;
    }
}

bool SettingsInputBoundary::handleTouchEvent(QTouchEvent* event)
{
    if (event->type() == QEvent::TouchCancel) {
        clearActivePress();
        return false;
    }

    const QList<QEventPoint> points = event->points();
    if (points.size() > 1) {
        const bool swallow = swallowPointerEvents_;
        clearActivePress();
        return swallow;
    }

    if (points.isEmpty()) {
        const bool swallow = swallowPointerEvents_;
        clearActivePress();
        return swallow;
    }

    const QEventPoint& point = points.first();

    if (point.state() == QEventPoint::Pressed) {
        armTouch(point.id(), point.scenePosition());
        return false;
    }

    if (!isActiveTouchPoint(point))
        return swallowPointerEvents_;

    if (swallowPointerEvents_ && point.state() != QEventPoint::Released)
        return true;

    if (point.state() == QEventPoint::Released) {
        const bool swallow = swallowPointerEvents_;
        clearActivePress();
        return swallow;
    }

    maybeCancelForMovement(point.scenePosition());
    return false;
}

bool SettingsInputBoundary::isActiveTouchPoint(const QEventPoint& point) const
{
    return activePointer_ == PointerKind::Touch && point.id() == activeTouchPointId_;
}

} // namespace oap
