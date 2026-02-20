#include <QTest>
#include "core/InputDeviceScanner.hpp"

class TestInputDeviceScanner : public QObject {
    Q_OBJECT

private slots:
    void findTouchDevice_returnsStringPath()
    {
        // On dev VM there may or may not be a touchscreen.
        // Just verify the function runs without crashing and returns a QString.
        QString result = oap::InputDeviceScanner::findTouchDevice();
        // result is either empty (no touchscreen) or a /dev/input/eventN path
        if (!result.isEmpty()) {
            QVERIFY(result.startsWith("/dev/input/event"));
        }
    }

    void listInputDevices_returnsVector()
    {
        auto devices = oap::InputDeviceScanner::listInputDevices();
        // Should return some devices on any Linux system
        // Each entry has path and name
        for (const auto& dev : devices) {
            QVERIFY(dev.path.startsWith("/dev/input/event"));
            // name may be empty if permission denied, that's ok
        }
    }
};

QTEST_MAIN(TestInputDeviceScanner)
#include "test_input_device_scanner.moc"
