#pragma once

#include <QSize>
#include <QString>

namespace oap {

class IDisplayService {
public:
    virtual ~IDisplayService() = default;

    /// Physical screen size in pixels.
    /// Thread-safe.
    virtual QSize screenSize() const = 0;

    /// Current backlight brightness (0-100).
    /// Thread-safe.
    virtual int brightness() const = 0;

    /// Set backlight brightness (0-100).
    /// Must be called from the main thread.
    virtual void setBrightness(int value) = 0;

    /// Current orientation: "landscape" or "portrait".
    /// Thread-safe.
    virtual QString orientation() const = 0;
};

} // namespace oap
