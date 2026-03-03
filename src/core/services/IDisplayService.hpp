#pragma once

namespace oap {

class IDisplayService {
public:
    virtual ~IDisplayService() = default;

    /// Current backlight brightness (0-100).
    /// Thread-safe.
    virtual int brightness() const = 0;

    /// Set backlight brightness (0-100).
    /// Must be called from the main thread.
    virtual void setBrightness(int value) = 0;
};

} // namespace oap
