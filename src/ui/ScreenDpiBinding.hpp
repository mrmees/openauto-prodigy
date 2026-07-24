#pragma once

#include <QMetaMethod>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

class QWindow;

namespace oap {

class DisplayInfo;

/// Owns the window/current-screen signal lifetime used to publish QScreen DPI.
///
/// bindDpiSource() is the lower-level signal seam used by bindWindow(); keeping
/// it QObject-based makes the replacement contract testable without requiring
/// a particular desktop or multiple physical screens.
class ScreenDpiBinding : public QObject {
    Q_OBJECT
public:
    explicit ScreenDpiBinding(DisplayInfo* displayInfo, QObject* parent = nullptr);

    void bindWindow(QWindow* window);
    void bindDpiSource(QObject* source, qreal initialDpi,
                       const QMetaMethod& dpiChangedSignal);

private slots:
    void acceptDpi(qreal dpi);

private:
    void bindCurrentScreen();
    void disconnectWindow();
    void disconnectDpiSource();

    QPointer<DisplayInfo> displayInfo_;
    QPointer<QWindow> window_;
    QPointer<QObject> dpiSource_;
    QMetaObject::Connection windowConnection_;
    QMetaObject::Connection dpiConnection_;
};

} // namespace oap
