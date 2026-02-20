#pragma once

#include <QString>
#include <QVector>

namespace oap {

class InputDeviceScanner {
public:
    struct DeviceInfo {
        QString path;   // e.g. "/dev/input/event4"
        QString name;   // e.g. "DFRobot USB Multi Touch"
        bool isTouchscreen = false;  // has INPUT_PROP_DIRECT
    };

    /// Scan /dev/input/event0..event31 and return info for each accessible device.
    static QVector<DeviceInfo> listInputDevices();

    /// Find first device with INPUT_PROP_DIRECT capability.
    /// Returns empty string if none found.
    static QString findTouchDevice();
};

} // namespace oap
