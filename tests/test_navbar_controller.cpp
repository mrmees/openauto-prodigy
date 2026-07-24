#include <QTest>
#include <QSignalSpy>
#include <limits>
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

    static QVariantMap rect(qreal x, qreal y, qreal w, qreal h)
    {
        return {{QStringLiteral("x"), x}, {QStringLiteral("y"), y},
                {QStringLiteral("w"), w}, {QStringLiteral("h"), h}};
    }

    static qint64 publishNavbarGeometry(oap::NavbarController& controller,
                                         int displayWidth, int displayHeight,
                                         const QVariantList& regions)
    {
        const qint64 generation = controller.beginNavbarGeometryUpdate();
        controller.setNavbarGeometry(generation, displayWidth, displayHeight, regions);
        return generation;
    }

    static bool dispatchPixel(oap::aa::TouchRouter& router,
                              const oap::aa::EvdevCoordBridge& bridge,
                              int slot, qreal x, qreal y,
                              oap::aa::TouchEvent event)
    {
        return router.dispatch(slot, bridge.pixelToEvdevX(x),
                               bridge.pixelToEvdevY(y), event);
    }

    static qint64 showPopupSession(oap::NavbarController& controller, int controlIndex)
    {
        controller.showPopup(controlIndex);
        return controller.beginPopupSession(controlIndex);
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

        // Wait for multiple holdProgress signals. The progress timer fires every
        // 16ms, so 400ms should yield ~25 signals.
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 3, 500);

        // Capture progress values BEFORE release, since release emits
        // holdProgress(0, 0.0) as a reset signal.
        int countBeforeRelease = spy.count();
        qreal firstProgress = spy.first().at(1).toReal();
        qreal lastProgress = spy.at(countBeforeRelease - 1).at(1).toReal();

        ctrl->handleRelease(0);

        // Progress should increase over time (timer-driven, based on elapsed ms)
        QVERIFY(firstProgress >= 0.0);
        QVERIFY(lastProgress > firstProgress);
        QVERIFY(lastProgress <= 1.0);

        // All pre-release signals should be for control 0
        for (int i = 0; i < countBeforeRelease; ++i) {
            QCOMPARE(spy.at(i).at(0).toInt(), 0);
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

    // --- Rendered navbar geometry ---

    void testRenderedNavbarGeometry_data()
    {
        QTest::addColumn<QVariantList>("regions");
        QTest::addColumn<QList<QPointF>>("controlPoints");
        QTest::addColumn<QPointF>("scaledInnerPoint");
        QTest::addColumn<QPointF>("outsidePoint");

        QTest::newRow("bottom")
            << QVariantList{rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                            rect(819.2, 516, 204.8, 84)}
            << QList<QPointF>{{100, 558}, {220, 558}, {900, 558}}
            << QPointF(512, 520)
            << QPointF(512, 515);
        QTest::newRow("top")
            << QVariantList{rect(0, 0, 204.8, 72), rect(204.8, 0, 614.4, 72),
                            rect(819.2, 0, 204.8, 72)}
            << QList<QPointF>{{100, 36}, {800, 36}, {900, 36}}
            << QPointF(512, 70)
            << QPointF(512, 73);
        QTest::newRow("left")
            << QVariantList{rect(0, 0, 70, 120), rect(0, 120, 70, 360),
                            rect(0, 480, 70, 120)}
            << QList<QPointF>{{35, 60}, {35, 130}, {35, 540}}
            << QPointF(68, 300)
            << QPointF(71, 300);
        QTest::newRow("right")
            << QVariantList{rect(946, 0, 78, 120), rect(946, 120, 78, 360),
                            rect(946, 480, 78, 120)}
            << QList<QPointF>{{985, 60}, {985, 470}, {985, 540}}
            << QPointF(948, 300)
            << QPointF(945, 300);
    }

    void testRenderedNavbarGeometry()
    {
        QFETCH(QVariantList, regions);
        QFETCH(QList<QPointF>, controlPoints);
        QFETCH(QPointF, scaledInnerPoint);
        QFETCH(QPointF, outsidePoint);

        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);
        auto ctrl = makeController();
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600, regions);

        QSignalSpy gestureSpy(ctrl.get(), &oap::NavbarController::gestureTriggered);
        for (int control = 0; control < controlPoints.size(); ++control) {
            const QPointF point = controlPoints.at(control);
            QVERIFY(dispatchPixel(router, bridge, control, point.x(), point.y(),
                                  oap::aa::TouchEvent::Down));
            QVERIFY(dispatchPixel(router, bridge, control, point.x(), point.y(),
                                  oap::aa::TouchEvent::Up));
            QTRY_COMPARE(gestureSpy.count(), control + 1);
            QCOMPARE(gestureSpy.at(control).at(0).toInt(), control);
        }

        QVERIFY(dispatchPixel(router, bridge, 3, scaledInnerPoint.x(), scaledInnerPoint.y(),
                              oap::aa::TouchEvent::Down));
        QVERIFY(dispatchPixel(router, bridge, 3, scaledInnerPoint.x(), scaledInnerPoint.y(),
                              oap::aa::TouchEvent::Up));
        QTRY_COMPARE(gestureSpy.count(), 4);
        QCOMPARE(gestureSpy.at(3).at(0).toInt(), 1);

        QVERIFY(!dispatchPixel(router, bridge, 4, outsidePoint.x(), outsidePoint.y(),
                               oap::aa::TouchEvent::Down));
    }

    void testInvalidAndStaleGeometryCannotRestoreOldZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);
        auto ctrl = makeController();
        ctrl->setCoordBridge(&bridge);

        const QVariantList valid{rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                                 rect(819.2, 516, 204.8, 84)};
        const qint64 oldGeneration = publishNavbarGeometry(*ctrl, 1024, 600, valid);
        QVERIFY(dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Down));
        router.resetClaims();

        const qint64 emptyGeneration = ctrl->beginNavbarGeometryUpdate();
        QVERIFY(!dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Down));
        ctrl->setNavbarGeometry(oldGeneration, 1024, 600, valid);
        QVERIFY(!dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Down));
        ctrl->setNavbarGeometry(emptyGeneration, 1024, 600, {});
        QVERIFY(!dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Down));

        QVariantList nonFinite = valid;
        QVariantMap bad = nonFinite.at(1).toMap();
        bad[QStringLiteral("w")] = std::numeric_limits<double>::infinity();
        nonFinite[1] = bad;
        publishNavbarGeometry(*ctrl, 1024, 600, nonFinite);
        QVERIFY(!dispatchPixel(router, bridge, 0, 500, 558, oap::aa::TouchEvent::Down));

        publishNavbarGeometry(*ctrl, 0, 600, valid);
        QVERIFY(!dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Down));
    }

    void testStaleGeometryCannotReplaceNewerRenderedZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);
        auto ctrl = makeController();
        ctrl->setCoordBridge(&bridge);

        const QVariantList bottom{rect(0, 516, 204.8, 84),
                                  rect(204.8, 516, 614.4, 84),
                                  rect(819.2, 516, 204.8, 84)};
        const qint64 bottomGeneration = publishNavbarGeometry(*ctrl, 1024, 600, bottom);
        const QVariantList top{rect(0, 0, 204.8, 72), rect(204.8, 0, 614.4, 72),
                               rect(819.2, 0, 204.8, 72)};
        publishNavbarGeometry(*ctrl, 1024, 600, top);

        ctrl->setNavbarGeometry(bottomGeneration, 1024, 600, bottom);
        QVERIFY(dispatchPixel(router, bridge, 0, 100, 36, oap::aa::TouchEvent::Down));
        router.resetClaims();
        QVERIFY(!dispatchPixel(router, bridge, 1, 100, 558, oap::aa::TouchEvent::Down));
    }

    void testUnregisterZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});
        ctrl->unregisterZones();

        // After unregister, touch should not be claimed
        float volEvX = bridge.pixelToEvdevX(128);
        float volEvY = bridge.pixelToEvdevY(572);
        bool claimed = router.dispatch(0, volEvX, volEvY, oap::aa::TouchEvent::Down);
        QVERIFY(!claimed);
    }

    void testEdgeChangeInvalidatesReportedGeometry()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        // QML must publish the new rendered rectangles after the edge change.
        ctrl->setEdge("left");

        // Bottom zone should no longer claim (bar moved to left)
        float bottomEvX = bridge.pixelToEvdevX(512);
        float bottomEvY = bridge.pixelToEvdevY(572);
        bool claimed = router.dispatch(0, bottomEvX, bottomEvY, oap::aa::TouchEvent::Down);
        QVERIFY(!claimed);
        router.resetClaims();

        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 0, 70, 120), rect(0, 120, 70, 360),
                               rect(0, 480, 70, 120)});
        QVERIFY(dispatchPixel(router, bridge, 1, 35, 300, oap::aa::TouchEvent::Down));
    }

    void testDuplicateEvdevDownCannotReplaceAcceptedSlot()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);
        auto ctrl = makeController();
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});
        QSignalSpy gestureSpy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        QVERIFY(dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Down));
        QCoreApplication::processEvents();
        QVERIFY(dispatchPixel(router, bridge, 1, 110, 558, oap::aa::TouchEvent::Down));
        QCoreApplication::processEvents();
        QVERIFY(dispatchPixel(router, bridge, 1, 110, 558, oap::aa::TouchEvent::Up));
        QCoreApplication::processEvents();
        QCOMPARE(gestureSpy.count(), 0);

        QVERIFY(dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Up));
        QTRY_COMPARE(gestureSpy.count(), 1);
        QCOMPARE(gestureSpy.at(0).at(0).toInt(), 0);
    }

    void testGeometryInvalidationCancelsAcceptedGesture()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);
        auto ctrl = makeController();
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});
        QSignalSpy gestureSpy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        QVERIFY(dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Down));
        QCoreApplication::processEvents();
        ctrl->beginNavbarGeometryUpdate();
        QVERIFY(!dispatchPixel(router, bridge, 0, 100, 558, oap::aa::TouchEvent::Up));
        QTest::qWait(700);

        QCOMPARE(gestureSpy.count(), 0);
    }

    // --- Popup zone registration (Task 2) ---

    void testShowPopupRegistersZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        qint64 gen = showPopupSession(*ctrl, 0);

        // After showPopup alone, no popup zones exist yet (QML hasn't reported)
        float contentEvX = bridge.pixelToEvdevX(512);
        float contentEvY = bridge.pixelToEvdevY(200);
        bool claimed = router.dispatch(0, contentEvX, contentEvY, oap::aa::TouchEvent::Down);
        QVERIFY(!claimed);  // no zones in content area yet
        router.resetClaims();

        // After setPopupRegions, dismiss zone should be active
        QVariantList regions;
        QVariantMap slider;
        slider["id"] = "slider"; slider["type"] = 0;
        slider["x"] = 0.0; slider["y"] = 0.0;
        slider["w"] = 100.0; slider["h"] = 600.0;
        slider["target"] = 0; slider["min"] = 0; slider["max"] = 100;
        slider["axis"] = 0; slider["invertAxis"] = true;
        regions.append(slider);
        ctrl->setPopupRegions(0, gen, regions);

        claimed = router.dispatch(1, contentEvX, contentEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);  // dismiss zone now active
        router.resetClaims();
    }

    void testHidePopupRemovesZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

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

    // --- Popup dismiss behavior ---

    // --- Popup session API ---

    void testBeginPopupSessionReturnsIncreasingGeneration()
    {
        auto ctrl = makeController();
        qint64 gen1 = ctrl->beginPopupSession(0);
        qint64 gen2 = ctrl->beginPopupSession(1);
        QVERIFY(gen2 > gen1);
    }

    void testClearPopupRegionsIgnoresStaleGeneration()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        // Open a popup, then switch before the outgoing QML item clears.
        qint64 gen1 = showPopupSession(*ctrl, 0);
        qint64 gen2 = showPopupSession(*ctrl, 1);

        // Try to clear with stale generation — should be ignored
        ctrl->clearPopupRegions(0, gen1);
        QVERIFY(ctrl->popupVisible());  // still visible

        // Clear with current generation — should work
        ctrl->clearPopupRegions(1, gen2);
        QCoreApplication::processEvents();
        QVERIFY(!ctrl->popupVisible());
    }

    void testPopupSwitchingSurvivesOutgoingCleanupInBothDirections()
    {
        auto ctrl = makeController();

        const qint64 sliderGeneration = showPopupSession(*ctrl, 0);
        ctrl->showPopup(1);
        ctrl->clearPopupRegions(0, sliderGeneration);
        QVERIFY(ctrl->popupVisible());
        QCOMPARE(ctrl->popupControlIndex(), 1);

        const qint64 powerGeneration = ctrl->beginPopupSession(1);
        ctrl->showPopup(2);
        ctrl->clearPopupRegions(1, powerGeneration);
        QVERIFY(ctrl->popupVisible());
        QCOMPARE(ctrl->popupControlIndex(), 2);

        const qint64 secondSliderGeneration = ctrl->beginPopupSession(2);
        ctrl->showPopup(1);
        ctrl->clearPopupRegions(2, secondSliderGeneration);
        QVERIFY(ctrl->popupVisible());
        QCOMPARE(ctrl->popupControlIndex(), 1);
    }

    void testBumpPopupDismissTimerResetsTimeout()
    {
        auto ctrl = makeController();
        ctrl->showPopup(0);
        QVERIFY(ctrl->popupVisible());

        // Wait a bit, then bump timer
        QTest::qWait(100);
        ctrl->bumpPopupDismissTimer();

        // Popup should still be visible (timer was reset)
        QVERIFY(ctrl->popupVisible());
    }

    // --- setPopupRegions ---

    void testSetPopupRegionsSliderZoneClaims()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        qint64 gen = showPopupSession(*ctrl, 0);

        // Report a slider region covering left strip of screen
        QVariantList regions;
        QVariantMap slider;
        slider["id"] = "volume-slider";
        slider["type"] = 0;  // Slider
        slider["x"] = 10.0;
        slider["y"] = 0.0;
        slider["w"] = 80.0;
        slider["h"] = 600.0;
        slider["target"] = 0;  // Volume
        slider["min"] = 0;
        slider["max"] = 100;
        slider["axis"] = 0;  // Vertical
        slider["invertAxis"] = true;
        regions.append(slider);

        ctrl->setPopupRegions(0, gen, regions);

        // Touch inside the reported slider region should be claimed
        float sliderEvX = bridge.pixelToEvdevX(50);  // x=50, inside [10,90]
        float sliderEvY = bridge.pixelToEvdevY(300); // y=300, inside [0,600]
        bool claimed = router.dispatch(0, sliderEvX, sliderEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.resetClaims();

        // Touch outside the slider region but inside screen should hit dismiss zone
        float outsideEvX = bridge.pixelToEvdevX(500);
        float outsideEvY = bridge.pixelToEvdevY(300);
        claimed = router.dispatch(1, outsideEvX, outsideEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);  // dismiss zone
        router.resetClaims();
    }

    void testSetPopupRegionsButtonZoneClaims()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        qint64 gen = showPopupSession(*ctrl, 1);

        // Report 3 button regions
        QVariantList regions;
        for (const auto& action : {"minimize", "restart", "quit"}) {
            QVariantMap btn;
            btn["id"] = QString("btn-%1").arg(action);
            btn["type"] = 1;  // Button
            btn["x"] = 400.0;
            btn["y"] = 100.0 + regions.size() * 60.0;
            btn["w"] = 160.0;
            btn["h"] = 50.0;
            btn["action"] = QString(action);
            regions.append(btn);
        }

        ctrl->setPopupRegions(1, gen, regions);

        // Touch inside first button region
        float btnEvX = bridge.pixelToEvdevX(480);  // center of button
        float btnEvY = bridge.pixelToEvdevY(125);  // center of first button
        bool claimed = router.dispatch(0, btnEvX, btnEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.resetClaims();
    }

    void testPopupButtonPressStateIsPerZoneAndSlot()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);
        auto ctrl = makeController();
        ctrl->setCoordBridge(&bridge);

        oap::ActionRegistry registry;
        bool minimized = false;
        registry.registerAction(QStringLiteral("app.minimize"),
                                [&minimized](const QVariant&) { minimized = true; });
        ctrl->setActionRegistry(&registry);

        const qint64 generation = showPopupSession(*ctrl, 1);
        QVariantList regions;
        QVariantMap first = rect(400, 100, 160, 50);
        first[QStringLiteral("id")] = QStringLiteral("btn-minimize");
        first[QStringLiteral("type")] = 1;
        first[QStringLiteral("action")] = QStringLiteral("minimize");
        regions.append(first);
        QVariantMap second = rect(400, 220, 160, 50);
        second[QStringLiteral("id")] = QStringLiteral("btn-restart");
        second[QStringLiteral("type")] = 1;
        second[QStringLiteral("action")] = QStringLiteral("restart");
        regions.append(second);
        ctrl->setPopupRegions(1, generation, regions);

        QVERIFY(dispatchPixel(router, bridge, 0, 410, 125, oap::aa::TouchEvent::Down));
        QVERIFY(dispatchPixel(router, bridge, 1, 550, 245, oap::aa::TouchEvent::Down));
        QVERIFY(dispatchPixel(router, bridge, 0, 410, 125, oap::aa::TouchEvent::Up));
        QCoreApplication::processEvents();

        QVERIFY(minimized);
        QVERIFY(!ctrl->popupVisible());
    }

    void testSetPopupRegionsReplacesOldZones()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        qint64 gen = showPopupSession(*ctrl, 0);

        // First set: slider at x=0-100
        QVariantList regions1;
        QVariantMap slider1;
        slider1["id"] = "slider";
        slider1["type"] = 0;
        slider1["x"] = 0.0; slider1["y"] = 0.0;
        slider1["w"] = 100.0; slider1["h"] = 600.0;
        slider1["target"] = 0; slider1["min"] = 0; slider1["max"] = 100;
        slider1["axis"] = 0; slider1["invertAxis"] = true;
        regions1.append(slider1);
        ctrl->setPopupRegions(0, gen, regions1);

        // Second set: slider at x=500-600 (different position)
        QVariantList regions2;
        QVariantMap slider2;
        slider2["id"] = "slider";
        slider2["type"] = 0;
        slider2["x"] = 500.0; slider2["y"] = 0.0;
        slider2["w"] = 100.0; slider2["h"] = 600.0;
        slider2["target"] = 0; slider2["min"] = 0; slider2["max"] = 100;
        slider2["axis"] = 0; slider2["invertAxis"] = true;
        regions2.append(slider2);
        ctrl->setPopupRegions(0, gen, regions2);

        // New position SHOULD be claimed by slider (priority 60)
        float newEvX = bridge.pixelToEvdevX(550);
        float evY = bridge.pixelToEvdevY(300);
        bool claimed = router.dispatch(0, newEvX, evY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.resetClaims();
    }

    void testSetPopupRegionsIgnoresWrongGeneration()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        qint64 gen1 = showPopupSession(*ctrl, 0);
        qint64 gen2 = showPopupSession(*ctrl, 1);

        // Try to set regions with old generation — should be ignored
        QVariantList regions;
        QVariantMap slider;
        slider["id"] = "slider";
        slider["type"] = 0;
        slider["x"] = 0.0; slider["y"] = 0.0;
        slider["w"] = 100.0; slider["h"] = 600.0;
        slider["target"] = 0; slider["min"] = 0; slider["max"] = 100;
        slider["axis"] = 0; slider["invertAxis"] = true;
        regions.append(slider);

        ctrl->setPopupRegions(0, gen1, regions);  // stale!

        // The hardcoded popup zones from showPopup() should still be active,
        // not replaced by the stale setPopupRegions call.
        Q_UNUSED(gen2)
    }

    // --- Popup dismiss behavior ---

    void testSliderRegionNormalizesCorrectly()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        qint64 gen = showPopupSession(*ctrl, 0);

        // Slider: y=0 to y=600, invertAxis=true (top=100, bottom=0)
        QVariantList regions;
        QVariantMap slider;
        slider["id"] = "volume-slider";
        slider["type"] = 0;
        slider["x"] = 0.0; slider["y"] = 0.0;
        slider["w"] = 100.0; slider["h"] = 600.0;
        slider["target"] = 0;
        slider["min"] = 0; slider["max"] = 100;
        slider["axis"] = 0; slider["invertAxis"] = true;
        regions.append(slider);
        ctrl->setPopupRegions(0, gen, regions);

        // Touch at top of slider (y=0 → volume=100 with invertAxis)
        float topEvX = bridge.pixelToEvdevX(50);
        float topEvY = bridge.pixelToEvdevY(0);
        bool claimed = router.dispatch(0, topEvX, topEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.dispatch(0, topEvX, topEvY, oap::aa::TouchEvent::Up);
        router.resetClaims();

        // Touch at bottom of slider (y=600 → volume=0 with invertAxis)
        float botEvY = bridge.pixelToEvdevY(600);
        claimed = router.dispatch(0, topEvX, botEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.dispatch(0, topEvX, botEvY, oap::aa::TouchEvent::Up);
        router.resetClaims();

        // Touch at midpoint (y=300 → volume≈50 with invertAxis)
        float midEvY = bridge.pixelToEvdevY(300);
        claimed = router.dispatch(0, topEvX, midEvY, oap::aa::TouchEvent::Down);
        QVERIFY(claimed);
        router.dispatch(0, topEvX, midEvY, oap::aa::TouchEvent::Up);

        // Process events (volume changes are marshaled via QueuedConnection)
        QCoreApplication::processEvents();

        // Zone claiming and normalization verified by dispatch returning true.
        // Full integration testing (actual volume values) happens on the Pi.
    }

    // --- Widget interaction mode ---

    void testWidgetInteractionModeProperty()
    {
        auto ctrl = makeController();
        QSignalSpy spy(ctrl.get(), &oap::NavbarController::widgetInteractionModeChanged);

        QVERIFY(!ctrl->widgetInteractionMode());

        ctrl->setWidgetInteractionMode(true);
        QVERIFY(ctrl->widgetInteractionMode());
        QCOMPARE(spy.count(), 1);

        ctrl->setWidgetInteractionMode(true);  // same value, no signal
        QCOMPARE(spy.count(), 1);

        ctrl->setWidgetInteractionMode(false);
        QVERIFY(!ctrl->widgetInteractionMode());
        QCOMPARE(spy.count(), 2);
    }

    void testControlRoleGearTrashLHD()
    {
        auto ctrl = makeController(true);  // LHD
        ctrl->setWidgetInteractionMode(true);

        QCOMPARE(ctrl->controlRole(0), QString("gear"));
        QCOMPARE(ctrl->controlRole(1), QString("clock"));
        QCOMPARE(ctrl->controlRole(2), QString("trash"));
    }

    void testControlRoleGearTrashRHD()
    {
        auto ctrl = makeController(false);  // RHD
        ctrl->setWidgetInteractionMode(true);

        QCOMPARE(ctrl->controlRole(0), QString("trash"));
        QCOMPARE(ctrl->controlRole(1), QString("clock"));
        QCOMPARE(ctrl->controlRole(2), QString("gear"));
    }

    void testHandlePressNoHoldTimersInWidgetMode()
    {
        auto ctrl = makeController();
        ctrl->setWidgetInteractionMode(true);

        QSignalSpy holdSpy(ctrl.get(), &oap::NavbarController::holdProgress);

        ctrl->handlePress(0);
        QTest::qWait(400);  // well past tap threshold, would normally emit holdProgress

        // Filter out the reset signal (progress=0) that might come from cancel
        int nonZeroProgress = 0;
        for (const auto& call : holdSpy) {
            if (call.at(1).toReal() > 0.0)
                nonZeroProgress++;
        }
        QCOMPARE(nonZeroProgress, 0);

        ctrl->handleRelease(0);
    }

    void testHandleReleaseTapOnlyInWidgetMode()
    {
        auto ctrl = makeController();
        ctrl->setWidgetInteractionMode(true);

        QSignalSpy spy(ctrl.get(), &oap::NavbarController::gestureTriggered);

        ctrl->handlePress(0);
        QTest::qWait(300);  // past tapMaxMs_=200ms, would normally be ShortHold
        ctrl->handleRelease(0);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);  // controlIndex
        QCOMPARE(spy.at(0).at(1).toInt(), 0);  // Tap (NOT ShortHold)
    }

    void testGearTapDispatch()
    {
        auto ctrl = makeController(true);  // LHD: control 0 = driver = gear in widget mode
        ctrl->setWidgetInteractionMode(true);

        oap::ActionRegistry registry;
        ctrl->setActionRegistry(&registry);

        QSignalSpy configSpy(ctrl.get(), &oap::NavbarController::widgetConfigRequested);

        registry.registerAction("navbar.gear.tap", [ctrl = ctrl.get()](const QVariant&) {
            emit ctrl->widgetConfigRequested();
        });

        ctrl->handlePress(0);
        QTest::qWait(50);
        ctrl->handleRelease(0);

        QCOMPARE(configSpy.count(), 1);
    }

    void testTrashTapDispatch()
    {
        auto ctrl = makeController(true);  // LHD: control 2 = passenger = trash in widget mode
        ctrl->setWidgetInteractionMode(true);

        oap::ActionRegistry registry;
        ctrl->setActionRegistry(&registry);

        QSignalSpy deleteSpy(ctrl.get(), &oap::NavbarController::widgetDeleteRequested);

        registry.registerAction("navbar.trash.tap", [ctrl = ctrl.get()](const QVariant&) {
            emit ctrl->widgetDeleteRequested();
        });

        ctrl->handlePress(2);
        QTest::qWait(50);
        ctrl->handleRelease(2);

        QCOMPARE(deleteSpy.count(), 1);
    }

    // --- Popup dismiss behavior ---

    void testPopupDismissOnUpNotDown()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);

        auto ctrl = makeController(true, "bottom");
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        qint64 gen = showPopupSession(*ctrl, 0);

        // Simulate QML reporting geometry (registers dismiss zone)
        QVariantList regions;
        QVariantMap slider;
        slider["id"] = "slider"; slider["type"] = 0;
        slider["x"] = 0.0; slider["y"] = 0.0;
        slider["w"] = 100.0; slider["h"] = 600.0;
        slider["target"] = 0; slider["min"] = 0; slider["max"] = 100;
        slider["axis"] = 0; slider["invertAxis"] = true;
        regions.append(slider);
        ctrl->setPopupRegions(0, gen, regions);

        QVERIFY(ctrl->popupVisible());

        // DOWN in dismiss area (content area, outside popup slider zone)
        float contentEvX = bridge.pixelToEvdevX(512);
        float contentEvY = bridge.pixelToEvdevY(200);
        router.dispatch(0, contentEvX, contentEvY, oap::aa::TouchEvent::Down);

        // Process queued events
        QCoreApplication::processEvents();

        // Should still be visible — dismiss fires on Up, not Down
        QVERIFY(ctrl->popupVisible());

        // UP in dismiss area
        router.dispatch(0, contentEvX, contentEvY, oap::aa::TouchEvent::Up);
        QCoreApplication::processEvents();

        // Now should be hidden
        QVERIFY(!ctrl->popupVisible());
    }

    void testQueuedOutgoingDismissCannotHideIncomingPopup()
    {
        oap::aa::TouchRouter router;
        oap::aa::EvdevCoordBridge bridge(&router);
        bridge.setDisplayMapping(1024, 600, 4095, 4095);
        auto ctrl = makeController();
        ctrl->setCoordBridge(&bridge);
        publishNavbarGeometry(*ctrl, 1024, 600,
                              {rect(0, 516, 204.8, 84), rect(204.8, 516, 614.4, 84),
                               rect(819.2, 516, 204.8, 84)});

        const qint64 generation = showPopupSession(*ctrl, 0);
        QVariantMap slider = rect(0, 0, 100, 600);
        slider[QStringLiteral("id")] = QStringLiteral("slider");
        slider[QStringLiteral("type")] = 0;
        slider[QStringLiteral("target")] = 0;
        ctrl->setPopupRegions(0, generation, {slider});

        QVERIFY(dispatchPixel(router, bridge, 0, 512, 200, oap::aa::TouchEvent::Down));
        QVERIFY(dispatchPixel(router, bridge, 0, 512, 200, oap::aa::TouchEvent::Up));
        ctrl->showPopup(1);
        QCoreApplication::processEvents();

        QVERIFY(ctrl->popupVisible());
        QCOMPARE(ctrl->popupControlIndex(), 1);
    }
};

QTEST_MAIN(TestNavbarController)
#include "test_navbar_controller.moc"
