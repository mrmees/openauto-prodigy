#include <QtTest>
#include "core/aa/EvdevCoordBridge.hpp"
#include "core/aa/TouchRouter.hpp"

using oap::aa::EvdevCoordBridge;
using oap::aa::TouchEvent;
using oap::aa::TouchRouter;

class TestEvdevCoordBridge : public QObject {
    Q_OBJECT

private slots:
    void pixelToEvdevX_conversion()
    {
        TouchRouter router;
        EvdevCoordBridge bridge(&router);
        // Default: 1024px display, 4095 evdev max
        // pixel 512 -> 512 * 4095 / 1024 = 2047.5
        float result = bridge.pixelToEvdevX(512.0f);
        QCOMPARE(result, 512.0f * 4095.0f / 1024.0f);
    }

    void pixelToEvdevY_conversion()
    {
        TouchRouter router;
        EvdevCoordBridge bridge(&router);
        // Default: 600px display, 4095 evdev max
        // pixel 300 -> 300 * 4095 / 600 = 2047.5
        float result = bridge.pixelToEvdevY(300.0f);
        QCOMPARE(result, 300.0f * 4095.0f / 600.0f);
    }

    void updateZone_pushesToRouter()
    {
        TouchRouter router;
        EvdevCoordBridge bridge(&router);
        // Register a zone spanning full display height, 100px wide at left
        bool hit = false;
        bridge.updateZone("vol", 5, 0.0f, 0.0f, 100.0f, 600.0f,
                          [&](int, float, float, TouchEvent) { hit = true; });

        // Touch inside converted evdev zone should be claimed
        // pixel 50, 300 -> evdev ~199.9, ~2047.5
        float evX = 50.0f * 4095.0f / 1024.0f;
        float evY = 300.0f * 4095.0f / 600.0f;
        bool claimed = router.dispatch(0, evX, evY, TouchEvent::Down);
        QVERIFY(claimed);
        QVERIFY(hit);
    }

    void removeZone_removesFromRouter()
    {
        TouchRouter router;
        EvdevCoordBridge bridge(&router);
        bool hit = false;
        bridge.updateZone("vol", 5, 0.0f, 0.0f, 100.0f, 600.0f,
                          [&](int, float, float, TouchEvent) { hit = true; });
        bridge.removeZone(std::string("vol"));

        float evX = 50.0f * 4095.0f / 1024.0f;
        float evY = 300.0f * 4095.0f / 600.0f;
        bool claimed = router.dispatch(0, evX, evY, TouchEvent::Down);
        QVERIFY(!claimed);
        QVERIFY(!hit);
    }

    void multipleZones_bothPresent()
    {
        TouchRouter router;
        EvdevCoordBridge bridge(&router);
        bool volHit = false;
        bool homeHit = false;
        // Volume zone: left 100px
        bridge.updateZone("vol", 5, 0.0f, 0.0f, 100.0f, 600.0f,
                          [&](int, float, float, TouchEvent) { volHit = true; });
        // Home zone: right 100px
        bridge.updateZone("home", 5, 924.0f, 0.0f, 100.0f, 600.0f,
                          [&](int, float, float, TouchEvent) { homeHit = true; });

        // Touch in vol zone
        float evX = 50.0f * 4095.0f / 1024.0f;
        float evY = 300.0f * 4095.0f / 600.0f;
        router.dispatch(0, evX, evY, TouchEvent::Down);

        // Touch in home zone
        float evX2 = 974.0f * 4095.0f / 1024.0f;
        router.dispatch(1, evX2, evY, TouchEvent::Down);

        QVERIFY(volHit);
        QVERIFY(homeHit);
    }

    void updateZone_replacesDuplicate()
    {
        TouchRouter router;
        EvdevCoordBridge bridge(&router);
        int count1 = 0;
        int count2 = 0;
        bridge.updateZone("vol", 5, 0.0f, 0.0f, 100.0f, 600.0f,
                          [&](int, float, float, TouchEvent) { count1++; });
        // Replace with different callback
        bridge.updateZone("vol", 5, 0.0f, 0.0f, 100.0f, 600.0f,
                          [&](int, float, float, TouchEvent) { count2++; });

        float evX = 50.0f * 4095.0f / 1024.0f;
        float evY = 300.0f * 4095.0f / 600.0f;
        router.dispatch(0, evX, evY, TouchEvent::Down);

        QCOMPARE(count1, 0);  // old callback NOT called
        QCOMPARE(count2, 1);  // new callback called
    }

    void displayMappingChange_affectsConversion()
    {
        TouchRouter router;
        EvdevCoordBridge bridge(&router);
        // Change to 1920x1080 display
        bridge.setDisplayMapping(1920, 1080, 4095, 4095);

        // pixel 960 on 1920 display -> 960 * 4095 / 1920 = 2047.5
        float result = bridge.pixelToEvdevX(960.0f);
        QCOMPARE(result, 960.0f * 4095.0f / 1920.0f);

        // Zones should use new mapping
        bool hit = false;
        bridge.updateZone("test", 5, 0.0f, 0.0f, 100.0f, 100.0f,
                          [&](int, float, float, TouchEvent) { hit = true; });

        // pixel 50 on 1920 display -> evdev ~106.6
        float evX = 50.0f * 4095.0f / 1920.0f;
        float evY = 50.0f * 4095.0f / 1080.0f;
        bool claimed = router.dispatch(0, evX, evY, TouchEvent::Down);
        QVERIFY(claimed);
        QVERIFY(hit);
    }
};

QTEST_MAIN(TestEvdevCoordBridge)
#include "test_evdev_coord_bridge.moc"
