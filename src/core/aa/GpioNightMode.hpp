#pragma once

#include "NightModeProvider.hpp"
#include <QTimer>
#include <atomic>

namespace oap {
namespace aa {

class GpioNightModeTestAccess;

/// Night mode provider that reads a GPIO pin (sysfs interface).
/// Polls /sys/class/gpio/gpioN/value every 1 second.
/// Exports and configures the GPIO on start(), unexports on stop().
class GpioNightMode : public NightModeProvider {
    Q_OBJECT
public:
    /// @param gpioPin    GPIO pin number (e.g. 17)
    /// @param activeHigh If true, pin value 1 = night. If false, pin value 0 = night.
    explicit GpioNightMode(int gpioPin, bool activeHigh = true, QObject* parent = nullptr);

    bool isNight() const override;
    bool hasValidState() const override;
    void start() override;
    void stop() override;

private:
    friend class GpioNightModeTestAccess;

    void poll();
    void applyValue(const QString& value);
    bool exportGpio();
    void unexportGpio();

    int gpioPin_;
    bool activeHigh_;
    QTimer timer_;
    std::atomic<bool> currentState_{false};
    std::atomic<bool> hasValidState_{false};
    bool exported_ = false;
};

} // namespace aa
} // namespace oap
