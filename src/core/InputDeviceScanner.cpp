#include "InputDeviceScanner.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <cstring>

#ifndef INPUT_PROP_DIRECT
#define INPUT_PROP_DIRECT 0x01
#endif

namespace oap {

QVector<InputDeviceScanner::DeviceInfo> InputDeviceScanner::listInputDevices()
{
    QVector<DeviceInfo> devices;

    for (int i = 0; i < 32; ++i) {
        QString path = QStringLiteral("/dev/input/event%1").arg(i);

        int fd = ::open(path.toUtf8().constData(), O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;

        DeviceInfo info;
        info.path = path;

        // Read device name
        char nameBuf[256] = {};
        if (::ioctl(fd, EVIOCGNAME(sizeof(nameBuf)), nameBuf) >= 0) {
            info.name = QString::fromUtf8(nameBuf);
        }

        // Check INPUT_PROP_DIRECT (touchscreen)
        unsigned long propBits[(INPUT_PROP_CNT + 8 * sizeof(unsigned long) - 1) / (8 * sizeof(unsigned long))] = {};
        if (::ioctl(fd, EVIOCGPROP(sizeof(propBits)), propBits) >= 0) {
            if (propBits[INPUT_PROP_DIRECT / (8 * sizeof(unsigned long))]
                & (1UL << (INPUT_PROP_DIRECT % (8 * sizeof(unsigned long))))) {
                info.isTouchscreen = true;
            }
        }

        ::close(fd);
        devices.append(info);
    }

    return devices;
}

QString InputDeviceScanner::findTouchDevice()
{
    auto devices = listInputDevices();
    for (const auto& dev : devices) {
        if (dev.isTouchscreen)
            return dev.path;
    }
    return {};
}

} // namespace oap
