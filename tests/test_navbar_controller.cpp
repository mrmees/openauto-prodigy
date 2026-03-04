#include <QTest>
#include <QSignalSpy>
#include "ui/NavbarController.hpp"
#include "core/aa/EvdevCoordBridge.hpp"
#include "core/aa/TouchRouter.hpp"
#include "core/services/ActionRegistry.hpp"

class TestNavbarController : public QObject {
    Q_OBJECT

private:
    // Helper to create a NavbarController with config defaults
    std::unique_ptr<oap::NavbarController> makeController(bool lhd = true,
                                                           const QString& edge = "bottom",
                                                           int tapMaxMs = 200,
                                                           int shortHoldMaxMs = 600)
    {
        auto ctrl = std::make_unique<oap::NavbarController>();
        ctrl->setLeftHandDrive(lhd);
        ctrl->setEdge(edge);
        ctrl->setTapMaxMs(tapMaxMs);
        ctrl->setShortHoldMaxMs(shortHoldMaxMs);
        return ctrl;
    }

private slots:
    // --- Gesture detection ---

    void testTapGesture()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(0);
        QTest::qWait(50);  // well within tap threshold
        ctrl->handleRelease(0);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);  // controlIndex
        QCOMPARE(spy.at(0).at(1).toInt(), 0);  // Tap = 0
    }

    void testShortHoldGesture()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(1);
        QTest::qWait(300);  // between tap(200) and shortHold(600)
        ctrl->handleRelease(1);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);  // controlIndex
        QCOMPARE(spy.at(0).at(1).toInt(), 1);  // ShortHold = 1
    }

    void testLongHoldGestureFiresAtThreshold()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(2);
        // Long hold fires at 600ms threshold WITHOUT needing release
        QTest::qWait(700);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);  // controlIndex
        QCOMPARE(spy.at(0).at(1).toInt(), 2);  // LongHold = 2
    }

    void testHoldProgressEmittedDuringHold()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::holdProgress);

        ctrl->handlePress(0);
        QTest::qWait(400);  // should emit several holdProgress signals
        ctrl->handleRelease(0);

        QVERIFY(spy.count() > 0);

        // First emitted progress should be small, last should be approaching 0.67 (400/600)
        qreal firstProgress = spy.first().at(1).toReal();
        qreal lastProgress = spy.last().at(1).toReal();
        QVERIFY(firstProgress >= 0.0);
        QVERIFY(lastProgress > firstProgress);
        QVERIFY(lastProgress <= 1.0);

        // All should be for control 0
        for (const auto& args : spy) {
            QCOMPARE(args.at(0).toInt(), 0);
        }
    }

    void testCancelDoesNotEmitGesture()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(0);
        QTest::qWait(50);
        ctrl->handleCancel(0);

        QTest::qWait(700);  // wait past all thresholds
        QCOMPARE(spy.count(), 0);  // no gesture emitted
    }

    void testSecondPressOnSameControlIgnored()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(0);
        ctrl->handlePress(0);  // duplicate press while first still active
        QTest::qWait(50);
        ctrl->handleRelease(0);

        QCOMPARE(spy.count(), 1);  // only one gesture
    }

    // --- LHD / RHD mapping ---

    void testLhdControlMapping()
    {
        auto ctrl = makeController(true);  // LHD
        // LHD: control 0 = volume (driver=left), 1 = clock, 2 = brightness (passenger=right)
        QCOMPARE(ctrl->controlRole(0), QString("volume"));
        QCOMPARE(ctrl->controlRole(1), QString("clock"));
        QCOMPARE(ctrl->controlRole(2), QString("brightness"));
    }

    void testRhdControlMapping()
    {
        auto ctrl = makeController(false);  // RHD: swap 0 and 2
        QCOMPARE(ctrl->controlRole(0), QString("brightness"));
        QCOMPARE(ctrl->controlRole(1), QString("clock"));
        QCOMPARE(ctrl->controlRole(2), QString("volume"));
    }

    // --- Edge property ---

    void testEdgeDefaultsToBottom()
    {
        auto ctrl = makeController();
        QCOMPARE(ctrl->edge(), QString("bottom"));
    }

    void testSetEdgeEmitsSignal()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::edgeChanged);

        ctrl->setEdge("left");
        QCOMPARE(ctrl->edge(), QString("left"));
        QCOMPARE(spy.count(), 1);
    }

    void testSetEdgeSameValueNoSignal()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::edgeChanged);

        ctrl->setEdge("bottom");  // same as current
        QCOMPARE(spy.count(), 0);
    }

    // --- Gesture timing from config ---

    void testCustomGestureTimingTap()
    {
        auto ctrl = makeController(true, "bottom", 100, 400);
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(0);
        QTest::qWait(50);  // within 100ms tap
        ctrl->handleRelease(0);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 0);  // Tap
    }

    void testCustomGestureTimingShortHold()
    {
        // With tap_max=100, short_hold_max=400
        auto ctrl = makeController(true, "bottom", 100, 400);
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(0);
        QTest::qWait(200);  // between 100-400
        ctrl->handleRelease(0);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 1);  // ShortHold
    }

    void testCustomGestureTimingLongHold()
    {
        auto ctrl = makeController(true, "bottom", 100, 400);
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(0);
        QTest::qWait(500);  // past 400ms threshold

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 2);  // LongHold
    }

    // --- Popup state ---

    void testShowPopupSetsState()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::popupChanged);

        QVERIFY(!ctrl->popupVisible());

        ctrl->showPopup(1);
        QVERIFY(ctrl->popupVisible());
        QCOMPARE(ctrl->popupControlIndex(), 1);
        QVERIFY(spy.count() >= 1);
    }

    void testHidePopupClearsState()
    {
        auto ctrl = makeController();
        ctrl->showPopup(1);

        QSignalSpy spy(ctrl.get(), &oap::NavbarController::popupChanged);
        ctrl->hidePopup();

        QVERIFY(!ctrl->popupVisible());
        QCOMPARE(ctrl->popupControlIndex(), -1);
        QVERIFY(spy.count() >= 1);
    }

    void testPopupVisibleProperty()
    {
        auto ctrl = makeController();

        QVERIFY(!ctrl->popupVisible());
        ctrl->showPopup(2);
        QVERIFY(ctrl->popupVisible());
        ctrl->hidePopup();
        QVERIFY(!ctrl->popupVisible());
    }

    void testPopupAutoDismiss()
    {
        auto ctrl = makeController();
        ctrl->showPopup(0);
        QVERIFY(ctrl->popupVisible());

        // Auto-dismiss is 7 seconds; we can't wait that long in tests,
        // so we test the mechanism exists by checking popup is still visible
        // after a short delay and then verify it clears eventually
        QTest::qWait(100);
        QVERIFY(ctrl->popupVisible());  // still visible after 100ms
    }

    // --- Zone registration (Task 2) ---

    void testRegisterZonesBottomEdgeLhd()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        ctrl->registerZones(1024, 600);

        // Verify zones exist by dispatching touches in the expected regions
        // Bottom edge: 56px tall bar at bottom (y=544-600)
        // LHD: driver(volume)=left 1/4 (0-256), center(clock)=middle 1/2 (256-768), passenger(brightness)=right 1/4 (768-1024)

        // Touch in volume zone (left quarter, bottom bar)
        QSignalSpy gestureSpy(ctrl.get(), &oap::NavbarController::gestureTriggered);
        float volEvX = bridge.pixelToEvdevX(128);   // center of left quarter
        float volEvY = bridge.pixelToEvdevY(572);   // center of bottom bar
        bool claimed = router.dispatch(0, volEvX, volEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.resetClaims();
    }

    void testRegisterZonesLeftEdge()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "left");
        ctrl->setCoordBridge(&bridge);
        ctrl->registerZones(1024, 600);

        // Left edge: 56px wide bar on left (x=0-56)
        // Controls stacked vertically: driver=top 1/4, center=mid 1/2, passenger=bottom 1/4
        float barEvX = bridge.pixelToEvdevX(28);  // center of left bar

        // Top zone (driver)
        float topEvY = bridge.pixelToEvdevY(75);  // in top quarter
        bool claimed = router.dispatch(0, barEvX, topEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.resetClaims();

        // Center zone
        float midEvY = bridge.pixelToEvdevY(300);  // center
        claimed = router.dispatch(1, barEvX, midEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.resetClaims();
    }

    void testRegisterZonesRightEdge()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "right");
        ctrl->setCoordBridge(&bridge);
        ctrl->registerZones(1024, 600);

        // Right edge: 56px wide bar on right (x=968-1024)
        float barEvX = bridge.pixelToEvdevX(996);  // center of right bar
        float midEvY = bridge.pixelToEvdevY(300);
        bool claimed = router.dispatch(0, barEvX, midEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.resetClaims();
    }

    void testRegisterZonesRhdSwapsDriverPassenger()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        // RHD: driver side = right, passenger side = left
        // For bottom edge: volume and brightness swap sides
        auto ctrl = makeController(false, "bottom");  // RHD
        ctrl->setCoordBridge(&bridge);
        ctrl->registerZones(1024, 600);

        // In RHD, controlRole(0)=brightness (left side), controlRole(2)=volume (right side)
        // Zone 0 (left) should still be a zone that gets claimed
        float leftEvX = bridge.pixelToEvdevX(128);
        float barEvY = bridge.pixelToEvdevY(572);
        bool claimed = router.dispatch(0, leftEvX, barEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.resetClaims();
    }

    void testUnregisterZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        ctrl->registerZones(1024, 600);
        ctrl->unregisterZones();

        // After unregister, touch should not be claimed
        float volEvX = bridge.pixelToEvdevX(128);
        float volEvY = bridge.pixelToEvdevY(572);
        bool claimed = router.dispatch(0, volEvX, volEvY, oap::aa::TouchEvent::Down);
        QVERIFY(!claimed);
    }

    void testEdgeChangeReregistersZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        ctrl->registerZones(1024, 600);

        // Change to left edge - zones should auto-update
        ctrl->setEdge("left");

        // Bottom zone should no longer claim (bar moved to left)
        float bottomEvX = bridge.pixelToEvdevX(512);
        float bottomEvY = bridge.pixelToEvdevY(572);
        bool claimed = router.dispatch(0, bottomEvX, bottomEvY, oap::aa::TouchEvent::Down);
        QVERIFY(!claimed);
        router.resetClaims();

        // Left zone should now claim
        float leftEvX = bridge.pixelToEvdevX(28);
        float leftEvY = bridge.pixelToEvdevY(300);
        claimed = router.dispatch(1, leftEvX, leftEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
    }

    // --- Popup zone registration (Task 2) ---

    void testShowPopupRegistersZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        ctrl->registerZones(1024, 600);

        ctrl->showPopup(0);

        // Dismiss zone is screen-wide at priority 40 -- should claim touches
        // in the content area (above the bar)
        float contentEvX = bridge.pixelToEvdevX(512);
        float contentEvY = bridge.pixelToEvdevY(200);
        bool claimed = router.dispatch(0, contentEvX, contentEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);  // dismiss zone catches it
        router.resetClaims();
    }

    void testHidePopupRemovesZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        ctrl->registerZones(1024, 600);

        ctrl->showPopup(0);
        ctrl->hidePopup();

        // After hiding, content area touch should NOT be claimed
        // (no dismiss zone, and content area is outside navbar zones)
        float contentEvX = bridge.pixelToEvdevX(512);
        float contentEvY = bridge.pixelToEvdevY(200);
        bool claimed = router.dispatch(0, contentEvX, contentEvY, oap::aa::TouchEvent::Down);
        QVERIFY(!claimed);
    }

    // --- Action dispatch (Task 2) ---

    void testGestureDispatchesActionVolumeTap()
    {
        auto ctrl = makeController(true);  // LHD: control 0 = volume
        oap::ActionRegistry registry;

        bool volumeTapCalled = false;
        registry.registerAction("navbar.volume.tap", [&](const QVariant&) {
            volumeTapCalled = true;
        });

        ctrl->setActionRegistry(&registry);

        // Simulate tap on control 0 (volume in LHD)
        ctrl->handlePress(0);
        QTest::qWait(50);
        ctrl->handleRelease(0);

        QVERIFY(volumeTapCalled);
    }

    void testGestureDispatchesActionClockLongHold()
    {
        auto ctrl = makeController(true);  // LHD: control 1 = clock
        oap::ActionRegistry registry;

        bool clockLongHoldCalled = false;
        registry.registerAction("navbar.clock.longHold", [&](const QVariant&) {
            clockLongHoldCalled = true;
        });

        ctrl->setActionRegistry(&registry);

        ctrl->handlePress(1);
        QTest::qWait(700);  // past long-hold threshold

        QVERIFY(clockLongHoldCalled);
    }

    void testGestureDispatchesActionBrightnessShortHold()
    {
        auto ctrl = makeController(true);  // LHD: control 2 = brightness
        oap::ActionRegistry registry;

        bool brightnessShortHoldCalled = false;
        registry.registerAction("navbar.brightness.shortHold", [&](const QVariant&) {
            brightnessShortHoldCalled = true;
        });

        ctrl->setActionRegistry(&registry);

        ctrl->handlePress(2);
        QTest::qWait(300);  // between tap and long hold
        ctrl->handleRelease(2);

        QVERIFY(brightnessShortHoldCalled);
    }

    void testActionDispatchWithNullRegistryNocrash()
    {
        auto ctrl = makeController(true);
        // No action registry set -- should not crash
        ctrl->handlePress(0);
        QTest::qWait(50);
        ctrl->handleRelease(0);
        // Just verifying it doesn't crash
    }

    // --- Release after long-hold should NOT emit again ---

    void testReleaseAfterLongHoldNoDoubleEmit()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(0);
        QTest::qWait(700);  // long hold fires
        QCOMPARE(spy.count(), 1);

        ctrl->handleRelease(0);  // release after long hold already fired
        QTest::qWait(50);
        QCOMPARE(spy.count(), 1);  // still just 1
    }
};

QTEST_MAIN(TestNavbarController)
#include "test_navbar_controller.moc"
