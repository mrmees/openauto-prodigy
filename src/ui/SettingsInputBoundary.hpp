#pragma once

#include <QPointF>
#include <QQuickItem>
#include <QTimer>
#include <QtQml/qqml.h>

namespace oap {

class SettingsInputBoundary : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SettingsInputBoundary(QQuickItem* parent = nullptr);

signals:
    void pressStarted(const QPointF& position);
    void pressEnded();
    void longPressed(const QPointF& position);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;
    void touchEvent(QTouchEvent* event) override;
    void touchUngrabEvent() override;
    bool childMouseEventFilter(QQuickItem* item, QEvent* event) override;

private:
    enum class PointerKind {
        None,
        Mouse,
        Touch,
    };

    void armMouse(const QPointF& scenePos);
    void armTouch(int pointId, const QPointF& scenePos);
    void clearActivePress();
    void maybeCancelForMovement(const QPointF& scenePos);
    void triggerLongPress();
    bool handleMouseEvent(QMouseEvent* event);
    bool handleTouchEvent(QTouchEvent* event);
    bool isActiveTouchPoint(const QEventPoint& point) const;

    PointerKind activePointer_ = PointerKind::None;
    int activeTouchPointId_ = -1;
    QPointF pressScenePos_;
    bool pressActive_ = false;
    bool longPressTriggered_ = false;
    bool swallowPointerEvents_ = false;
    QTimer holdTimer_;
};

} // namespace oap
