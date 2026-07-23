#pragma once

#include "NightModeProvider.hpp"
#include <QString>
#include <QTimer>
#include <atomic>

namespace oap {
namespace aa {

class GpioNightModeTestAccess;

/// Night mode provider that reads a GPIO pin (sysfs interface).
/// Polls <sysfsRoot>/gpioN/value every 1 second.
/// Exports and configures the GPIO on start(), unexports on stop().
class GpioNightMode : public NightModeProvider {
    Q_OBJECT
public:
    /// @param gpioPin    GPIO pin number (e.g. 17)
    /// @param activeHigh If true, pin value 1 = night. If false, pin value 0 = night.
    explicit GpioNightMode(int gpioPin, bool activeHigh = true,
                           QObject* parent = nullptr,
                           QString sysfsRoot = QStringLiteral("/sys/class/gpio"));

    bool isNight() const override;
    bool hasValidState() const override;
    void start() override;
    void stop() override;

private:
    friend class GpioNightModeTestAccess;

    void poll();
    void applyValue(const QString& value);
    void invalidateState();
    bool ensureConfigured();
    bool exportGpio();
    bool setInputDirection();
    void unexportGpio();
    QString gpioPath(const QString& fileName = {}) const;

    int gpioPin_;
    bool activeHigh_;
    QString sysfsRoot_;
    QTimer timer_;
    std::atomic<bool> currentState_{false};
    std::atomic<bool> hasValidState_{false};
    bool exported_ = false;
    bool ownsExport_ = false;
    bool configured_ = false;
};

} // namespace aa
} // namespace oap
