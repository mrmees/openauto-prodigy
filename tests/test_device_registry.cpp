#include <QTest>
#include <QSignalSpy>
#include "core/audio/PipeWireDeviceRegistry.hpp"

class TestDeviceRegistry : public QObject {
    Q_OBJECT

private slots:
    void constructionDoesNotCrash()
    {
        oap::PipeWireDeviceRegistry registry;
        QVERIFY(true);
    }

    void addDeviceEmitsSignal()
    {
        oap::PipeWireDeviceRegistry registry;
        QSignalSpy spy(&registry, &oap::PipeWireDeviceRegistry::deviceAdded);

        oap::AudioDeviceInfo info;
        info.registryId = 42;
        info.nodeName = "test.sink";
        info.description = "Test Sink";
        info.mediaClass = "Audio/Sink";
        registry.testAddDevice(info);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(registry.outputDevices().size(), 1);
        QCOMPARE(registry.outputDevices().first().nodeName, QString("test.sink"));
    }

    void removeDeviceEmitsSignal()
    {
        oap::PipeWireDeviceRegistry registry;
        QSignalSpy addSpy(&registry, &oap::PipeWireDeviceRegistry::deviceAdded);
        QSignalSpy removeSpy(&registry, &oap::PipeWireDeviceRegistry::deviceRemoved);

        oap::AudioDeviceInfo info;
        info.registryId = 42;
        info.nodeName = "test.sink";
        info.description = "Test Sink";
        info.mediaClass = "Audio/Sink";
        registry.testAddDevice(info);
        registry.testRemoveDevice(42);

        QCOMPARE(removeSpy.count(), 1);
        QCOMPARE(registry.outputDevices().size(), 0);
    }

    void separatesOutputsAndInputs()
    {
        oap::PipeWireDeviceRegistry registry;

        oap::AudioDeviceInfo sink;
        sink.registryId = 1;
        sink.nodeName = "out.sink";
        sink.description = "Speaker";
        sink.mediaClass = "Audio/Sink";
        registry.testAddDevice(sink);

        oap::AudioDeviceInfo source;
        source.registryId = 2;
        source.nodeName = "in.source";
        source.description = "Mic";
        source.mediaClass = "Audio/Source";
        registry.testAddDevice(source);

        QCOMPARE(registry.outputDevices().size(), 1);
        QCOMPARE(registry.inputDevices().size(), 1);
        QCOMPARE(registry.outputDevices().first().nodeName, QString("out.sink"));
        QCOMPARE(registry.inputDevices().first().nodeName, QString("in.source"));
    }

    void duplexDeviceAppearsInBoth()
    {
        oap::PipeWireDeviceRegistry registry;

        oap::AudioDeviceInfo duplex;
        duplex.registryId = 3;
        duplex.nodeName = "usb.headset";
        duplex.description = "USB Headset";
        duplex.mediaClass = "Audio/Duplex";
        registry.testAddDevice(duplex);

        QCOMPARE(registry.outputDevices().size(), 1);
        QCOMPARE(registry.inputDevices().size(), 1);
    }
};

QTEST_MAIN(TestDeviceRegistry)
#include "test_device_registry.moc"
