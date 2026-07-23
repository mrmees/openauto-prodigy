#include "GpioNightMode.hpp"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include "../Logging.hpp"
#include <utility>

namespace oap {
namespace aa {

GpioNightMode::GpioNightMode(int gpioPin, bool activeHigh, QObject* parent,
                             QString sysfsRoot)
    : NightModeProvider(parent)
    , gpioPin_(gpioPin)
    , activeHigh_(activeHigh)
    , sysfsRoot_(std::move(sysfsRoot))
{
    connect(&timer_, &QTimer::timeout, this, &GpioNightMode::poll);
}

bool GpioNightMode::isNight() const
{
    return currentState_;
}

bool GpioNightMode::hasValidState() const
{
    return hasValidState_;
}

void GpioNightMode::start()
{
    hasValidState_ = false;
    configured_ = false;
    qCInfo(lcCore) << "Starting — pin=" << gpioPin_
                            << " activeHigh=" << (activeHigh_ ? "true" : "false");

    poll();  // Initial setup/read. Failures remain retryable on the timer.
    timer_.start(1000);  // Poll every 1 second
}

void GpioNightMode::stop()
{
    timer_.stop();
    unexportGpio();
    configured_ = false;
    hasValidState_ = false;
}

void GpioNightMode::poll()
{
    if (!ensureConfigured()) {
        invalidateState();
        return;
    }

    const QString valuePath = gpioPath(QStringLiteral("value"));
    QFile file(valuePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcCore) << "Cannot read " << valuePath;
        configured_ = false;
        if (!QDir(gpioPath()).exists()) {
            exported_ = false;
            ownsExport_ = false;
        }
        invalidateState();
        return;
    }

    QTextStream in(&file);
    QString val = in.readLine().trimmed();
    file.close();

    applyValue(val);
}

void GpioNightMode::applyValue(const QString& val)
{
    if (val != "0" && val != "1") {
        qCWarning(lcCore) << "Invalid GPIO value for pin" << gpioPin_ << ":" << val;
        invalidateState();
        return;
    }

    bool pinHigh = (val == "1");
    bool night = activeHigh_ ? pinHigh : !pinHigh;
    const bool wasValid = hasValidState_.exchange(true);
    const bool stateChanged = (night != currentState_);
    currentState_ = night;

    if (!wasValid || stateChanged) {
        qCInfo(lcCore) << "Pin " << gpioPin_ << " = " << val
                                << " -> " << (night ? "NIGHT" : "DAY");
        emit nightModeChanged(night);
    }
}

void GpioNightMode::invalidateState()
{
    hasValidState_ = false;
}

bool GpioNightMode::ensureConfigured()
{
    if (configured_)
        return true;

    if (!exported_ && !exportGpio()) {
        qCWarning(lcCore) << "Failed to export GPIO" << gpioPin_
                          << "— will retry";
        return false;
    }

    if (!setInputDirection())
        return false;

    configured_ = true;
    return true;
}

bool GpioNightMode::exportGpio()
{
    // Check if already exported
    const QString dirPath = gpioPath();
    if (QDir(dirPath).exists()) {
        qCDebug(lcCore) << "GPIO " << gpioPin_ << " already exported";
        exported_ = true;
        ownsExport_ = false;
    } else {
        // Export the GPIO
        const QString exportPath = QDir(sysfsRoot_).filePath(QStringLiteral("export"));
        if (!QFile::exists(exportPath)) {
            qCWarning(lcCore) << "GPIO export control is missing:" << exportPath;
            return false;
        }
        QFile exportFile(exportPath);
        if (!exportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCWarning(lcCore) << "Cannot open" << exportPath << "(permission denied?)";
            return false;
        }
        const QByteArray pin = QByteArray::number(gpioPin_);
        if (exportFile.write(pin) != pin.size() || !exportFile.flush()) {
            qCWarning(lcCore) << "Cannot write GPIO export control" << exportPath;
            exportFile.close();
            return false;
        }
        exportFile.close();
        exported_ = true;
        ownsExport_ = true;
    }

    return true;
}

bool GpioNightMode::setInputDirection()
{
    // Set direction to input
    const QString directionPath = gpioPath(QStringLiteral("direction"));
    if (!QFile::exists(directionPath)) {
        qCWarning(lcCore) << "GPIO direction control is missing:" << directionPath;
        return false;
    }
    QFile dirFile(directionPath);
    if (dirFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        const QByteArray direction("in");
        if (dirFile.write(direction) != direction.size() || !dirFile.flush()) {
            qCWarning(lcCore) << "Cannot write direction for GPIO" << gpioPin_;
            dirFile.close();
            return false;
        }
        dirFile.close();
    } else {
        qCWarning(lcCore) << "Cannot set direction for GPIO " << gpioPin_;
        return false;
    }

    return true;
}

void GpioNightMode::unexportGpio()
{
    if (!exported_) return;

    if (ownsExport_) {
        QFile unexportFile(QDir(sysfsRoot_).filePath(QStringLiteral("unexport")));
        if (unexportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&unexportFile);
            out << gpioPin_;
            unexportFile.close();
        }
    }
    exported_ = false;
    ownsExport_ = false;
}

QString GpioNightMode::gpioPath(const QString& fileName) const
{
    const QString base = QDir(sysfsRoot_).filePath(QStringLiteral("gpio%1").arg(gpioPin_));
    return fileName.isEmpty() ? base : QDir(base).filePath(fileName);
}

} // namespace aa
} // namespace oap
