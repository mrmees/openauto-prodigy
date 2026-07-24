#include "ui/ScreenDpiBinding.hpp"

#include "ui/DisplayInfo.hpp"

#include <QScreen>
#include <QWindow>

namespace oap {

ScreenDpiBinding::ScreenDpiBinding(DisplayInfo* displayInfo, QObject* parent)
    : QObject(parent), displayInfo_(displayInfo)
{
}

void ScreenDpiBinding::bindWindow(QWindow* window)
{
    if (window_ != window) {
        disconnectWindow();
        window_ = window;
        if (window_) {
            windowConnection_ = connect(window_, &QWindow::screenChanged,
                                        this, [this](QScreen*) {
                bindCurrentScreen();
            });
        }
    }

    bindCurrentScreen();
}

void ScreenDpiBinding::bindDpiSource(QObject* source, qreal initialDpi,
                                     const QMetaMethod& dpiChangedSignal)
{
    if (dpiSource_ != source) {
        disconnectDpiSource();
        dpiSource_ = source;

        if (dpiSource_ && dpiChangedSignal.isValid()) {
            const int slotIndex = metaObject()->indexOfSlot("acceptDpi(qreal)");
            const QMetaMethod slot = metaObject()->method(slotIndex);
            dpiConnection_ = QObject::connect(dpiSource_, dpiChangedSignal,
                                              this, slot);
        }
    }

    if (dpiSource_ && displayInfo_)
        displayInfo_->setQScreenDpi(initialDpi);
}

void ScreenDpiBinding::acceptDpi(qreal dpi)
{
    if (displayInfo_)
        displayInfo_->setQScreenDpi(dpi);
}

void ScreenDpiBinding::bindCurrentScreen()
{
    auto* screen = window_ ? window_->screen() : nullptr;
    if (!screen) {
        disconnectDpiSource();
        return;
    }

    bindDpiSource(screen, screen->physicalDotsPerInch(),
                  QMetaMethod::fromSignal(&QScreen::physicalDotsPerInchChanged));
}

void ScreenDpiBinding::disconnectWindow()
{
    QObject::disconnect(windowConnection_);
    windowConnection_ = {};
    window_.clear();
    disconnectDpiSource();
}

void ScreenDpiBinding::disconnectDpiSource()
{
    QObject::disconnect(dpiConnection_);
    dpiConnection_ = {};
    dpiSource_.clear();
}

} // namespace oap
