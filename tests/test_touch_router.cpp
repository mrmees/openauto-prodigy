#include <QtTest>
#include "core/aa/TouchRouter.hpp"

using oap::aa::TouchEvent;
using oap::aa::TouchRouter;
using oap::aa::TouchZone;

class TestTouchRouter : public QObject {
    Q_OBJECT

private slots:
    void priorityDispatch_highPriorityWins()
    {
        TouchRouter router;
        bool zoneAHit = false;
        bool zoneBHit = false;
        router.setZones({
            {"zoneA", 1, 0, 0, 2048, 4095,
             [&](int, float, float, TouchEvent) { zoneAHit = true; }},
            {"zoneB", 10, 0, 0, 2048, 4095,
             [&](int, float, float, TouchEvent) { zoneBHit = true; }},
        });
        bool claimed = router.dispatch(0, 1000, 1000, TouchEvent::Down);
        QVERIFY(claimed);
        QVERIFY(zoneBHit);
        QVERIFY(!zoneAHit);
    }

    void noMatch_returnsFalse()
    {
        TouchRouter router;
        router.setZones({
            {"zone", 1, 0, 0, 100, 100,
             [](int, float, float, TouchEvent) {}},
        });
        bool claimed = router.dispatch(0, 500, 500, TouchEvent::Down);
        QVERIFY(!claimed);
    }

    void fingerStickiness_downClaimsSlot()
    {
        TouchRouter router;
        int hitCount = 0;
        router.setZones({
            {"zoneA", 5, 0, 0, 2048, 4095,
             [&](int, float, float, TouchEvent) { hitCount++; }},
        });
        router.dispatch(0, 1000, 1000, TouchEvent::Down);
        QCOMPARE(hitCount, 1);
    }

    void fingerStickiness_moveRoutesToClaimedZone()
    {
        TouchRouter router;
        int zoneAHits = 0;
        router.setZones({
            {"zoneA", 5, 0, 0, 2048, 2048,
             [&](int, float, float, TouchEvent) { zoneAHits++; }},
        });
        // DOWN inside zone
        router.dispatch(0, 1000, 1000, TouchEvent::Down);
        QCOMPARE(zoneAHits, 1);
        // MOVE outside zone -- should still route to zoneA (sticky)
        bool claimed = router.dispatch(0, 3000, 3000, TouchEvent::Move);
        QVERIFY(claimed);
        QCOMPARE(zoneAHits, 2);
    }

    void fingerStickiness_upRelasesClaim()
    {
        TouchRouter router;
        int zoneAHits = 0;
        router.setZones({
            {"zoneA", 5, 0, 0, 2048, 2048,
             [&](int, float, float, TouchEvent) { zoneAHits++; }},
        });
        router.dispatch(0, 1000, 1000, TouchEvent::Down);
        router.dispatch(0, 1000, 1000, TouchEvent::Up);
        QCOMPARE(zoneAHits, 2);
        // After UP, slot is unclaimed. DOWN outside zone should not match.
        bool claimed = router.dispatch(0, 3000, 3000, TouchEvent::Down);
        QVERIFY(!claimed);
    }

    void dynamicZoneAdd()
    {
        TouchRouter router;
        // No zones -- should not match
        bool claimed = router.dispatch(0, 1000, 1000, TouchEvent::Down);
        QVERIFY(!claimed);
        // Add zone
        int hitCount = 0;
        router.setZones({
            {"newZone", 5, 0, 0, 4095, 4095,
             [&](int, float, float, TouchEvent) { hitCount++; }},
        });
        claimed = router.dispatch(1, 1000, 1000, TouchEvent::Down);
        QVERIFY(claimed);
        QCOMPARE(hitCount, 1);
    }

    void dynamicZoneRemove_clearsStaleClaims()
    {
        TouchRouter router;
        int hitCount = 0;
        router.setZones({
            {"zoneA", 5, 0, 0, 4095, 4095,
             [&](int, float, float, TouchEvent) { hitCount++; }},
        });
        // DOWN claims zoneA
        router.dispatch(0, 1000, 1000, TouchEvent::Down);
        QCOMPARE(hitCount, 1);
        // Remove zone
        router.setZones({});
        // MOVE should now fall through (stale claim cleared)
        bool claimed = router.dispatch(0, 1000, 1000, TouchEvent::Move);
        QVERIFY(!claimed);
    }

    void staleClaim_fallthroughAfterRemoval()
    {
        TouchRouter router;
        int hitCount = 0;
        router.setZones({
            {"zoneA", 5, 0, 0, 4095, 4095,
             [&](int, float, float, TouchEvent) { hitCount++; }},
        });
        router.dispatch(0, 1000, 1000, TouchEvent::Down);
        // Remove zoneA, MOVE should fall through
        router.setZones({});
        bool claimed = router.dispatch(0, 1000, 1000, TouchEvent::Move);
        QVERIFY(!claimed);
        QCOMPARE(hitCount, 1);  // only the initial DOWN
    }

    void multipleSlots_independentClaims()
    {
        TouchRouter router;
        int zoneAHits = 0;
        int zoneBHits = 0;
        router.setZones({
            {"zoneA", 5, 0, 0, 2048, 4095,
             [&](int, float, float, TouchEvent) { zoneAHits++; }},
            {"zoneB", 5, 2049, 0, 4095, 4095,
             [&](int, float, float, TouchEvent) { zoneBHits++; }},
        });
        // Slot 0 claims zoneA
        router.dispatch(0, 1000, 1000, TouchEvent::Down);
        // Slot 1 claims zoneB
        router.dispatch(1, 3000, 1000, TouchEvent::Down);
        QCOMPARE(zoneAHits, 1);
        QCOMPARE(zoneBHits, 1);
        // MOVE both
        router.dispatch(0, 1000, 2000, TouchEvent::Move);
        router.dispatch(1, 3000, 2000, TouchEvent::Move);
        QCOMPARE(zoneAHits, 2);
        QCOMPARE(zoneBHits, 2);
    }

    void zoneBoundary_edgesInclusive()
    {
        TouchRouter router;
        int hitCount = 0;
        router.setZones({
            {"zone", 5, 100, 200, 300, 400,
             [&](int, float, float, TouchEvent) { hitCount++; }},
        });
        // Exact corner (x0, y0) -- should match
        bool claimed = router.dispatch(0, 100, 200, TouchEvent::Down);
        QVERIFY(claimed);
        router.dispatch(0, 100, 200, TouchEvent::Up);  // release

        // Exact corner (x1, y1) -- should match
        claimed = router.dispatch(0, 300, 400, TouchEvent::Down);
        QVERIFY(claimed);
        router.dispatch(0, 300, 400, TouchEvent::Up);

        // Just outside
        claimed = router.dispatch(0, 301, 200, TouchEvent::Down);
        QVERIFY(!claimed);

        QCOMPARE(hitCount, 4);  // two successful DOWN + UP pairs
    }
};

QTEST_MAIN(TestTouchRouter)
#include "test_touch_router.moc"
