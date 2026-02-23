#include "GpioNightMode.hpp"
#include <QFile>
#include <QTextStream>
#include <QDebug>

namespace oap {
namespace aa {

GpioNightMode::GpioNightMode(int gpioPin, bool activeHigh, QObject* parent)
    : NightModeProvider(parent)
    , gpioPin_(gpioPin)
    , activeHigh_(activeHigh)
{
    connect(&timer_, &QTimer::timeout, this, &GpioNightMode::poll);
}

bool GpioNightMode::isNight() const
{
    return currentState_;
}

void GpioNightMode::start()
{
    qInfo() << "[GpioNightMode] Starting — pin=" << gpioPin_
                            << " activeHigh=" << (activeHigh_ ? "true" : "false");

    if (!exportGpio()) {
        qCritical() << "[GpioNightMode] Failed to export GPIO " << gpioPin_
                                 << " — night mode will remain " << (currentState_ ? "NIGHT" : "DAY");
        return;
    }

    poll();  // Initial read
    timer_.start(1000);  // Poll every 1 second
}

void GpioNightMode::stop()
{
    timer_.stop();
    unexportGpio();
}

void GpioNightMode::poll()
{
    QString valuePath = QString("/sys/class/gpio/gpio%1/value").arg(gpioPin_);
    QFile file(valuePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[GpioNightMode] Cannot read " << valuePath;
        return;
    }

    QTextStream in(&file);
    QString val = in.readLine().trimmed();
    file.close();

    bool pinHigh = (val == "1");
    bool night = activeHigh_ ? pinHigh : !pinHigh;

    if (night != currentState_) {
        currentState_ = night;
        qInfo() << "[GpioNightMode] Pin " << gpioPin_ << " = " << val
                                << " -> " << (night ? "NIGHT" : "DAY");
        emit nightModeChanged(night);
    }
}

bool GpioNightMode::exportGpio()
{
    // Check if already exported
    QString dirPath = QString("/sys/class/gpio/gpio%1").arg(gpioPin_);
    if (QFile::exists(dirPath + "/value")) {
        qDebug() << "[GpioNightMode] GPIO " << gpioPin_ << " already exported";
        exported_ = true;
    } else {
        // Export the GPIO
        QFile exportFile("/sys/class/gpio/export");
        if (!exportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCritical() << "[GpioNightMode] Cannot open /sys/class/gpio/export (permission denied?)";
            return false;
        }
        QTextStream out(&exportFile);
        out << gpioPin_;
        exportFile.close();
        exported_ = true;
    }

    // Set direction to input
    QString directionPath = QString("/sys/class/gpio/gpio%1/direction").arg(gpioPin_);
    QFile dirFile(directionPath);
    if (dirFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&dirFile);
        out << "in";
        dirFile.close();
    } else {
        qWarning() << "[GpioNightMode] Cannot set direction for GPIO " << gpioPin_;
    }

    return true;
}

void GpioNightMode::unexportGpio()
{
    if (!exported_) return;

    QFile unexportFile("/sys/class/gpio/unexport");
    if (unexportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&unexportFile);
        out << gpioPin_;
        unexportFile.close();
    }
    exported_ = false;
}

} // namespace aa
} // namespace oap
