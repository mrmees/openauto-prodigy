#include <QtTest>
#include <QSignalSpy>
#include "core/aa/EvdevTouchReader.hpp"
#include "core/aa/TouchRouter.hpp"

using namespace oap::aa;

// Helper: convert pixel position to evdev coordinate (0-based, screenMax = evdev axis max)
static float pixelToEvdev(int pixel, int displaySize, int screenMax) {
    return static_cast<float>(pixel) * screenMax / displaySize;
}

class TestSidebarZones : public QObject {
    Q_OBJECT

private slots:
    // Vertical right sidebar at 1024x600: home zone starts near bottom (~527px), not at 75% (450px)
    void verticalRight_1024x600_homeZonePosition()
    {
        // Create reader with nullptr handler and dummy device (won't open device or start thread)
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);

        reader.setSidebar(true, 150, "right");

        // scale = min(1024/1024, 600/600) = 1.0
        // spacingPx = round(8 * 1.0) = 8
        // touchMinPx = round(56 * 1.0) = 56
        // homeStartPx = 600 - 8 - 56 = 536
        // volEndPx = 536 - 8 - 1 - 8 = 519
        const int screenMax = 4095;
        const int displayH = 600;

        float homeStartEvdev = pixelToEvdev(536, displayH, screenMax);
        float volEndEvdev = pixelToEvdev(519, displayH, screenMax);

        // Home zone should start at ~536px (not the old 75% = 450px)
        QVERIFY2(reader.sidebarHomeY0() > pixelToEvdev(500, displayH, screenMax),
                 qPrintable(QString("Home zone starts too high: %1 evdev (expected >%2)")
                     .arg(reader.sidebarHomeY0()).arg(pixelToEvdev(500, displayH, screenMax))));

        // Home zone should be near pixel 536
        QVERIFY2(qAbs(reader.sidebarHomeY0() - homeStartEvdev) < pixelToEvdev(10, displayH, screenMax),
                 qPrintable(QString("Home zone start %1 not near expected %2")
                     .arg(reader.sidebarHomeY0()).arg(homeStartEvdev)));

        // Volume zone should end near pixel 519
        QVERIFY2(qAbs(reader.sidebarVolY1() - volEndEvdev) < pixelToEvdev(10, displayH, screenMax),
                 qPrintable(QString("Volume zone end %1 not near expected %2")
                     .arg(reader.sidebarVolY1()).arg(volEndEvdev)));
    }

    // Vertical right sidebar at 800x480: zones scale proportionally
    void verticalRight_800x480_zonesScale()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 800, 480);

        reader.setSidebar(true, 120, "right");

        // scale = min(800/1024, 480/600) = min(0.78125, 0.8) = 0.78125
        // spacingPx = round(8 * 0.78125) = round(6.25) = 6
        // touchMinPx = round(56 * 0.78125) = round(43.75) = 44
        // homeStartPx = 480 - 6 - 44 = 430
        // volEndPx = 430 - 6 - 1 - 6 = 417
        const int screenMax = 4095;
        const int displayH = 480;

        float homeStartEvdev = pixelToEvdev(430, displayH, screenMax);

        // Home zone should start near pixel 430 (not old 75% = 360px)
        QVERIFY2(reader.sidebarHomeY0() > pixelToEvdev(400, displayH, screenMax),
                 qPrintable(QString("Home zone starts too high at 800x480: %1")
                     .arg(reader.sidebarHomeY0())));

        QVERIFY2(qAbs(reader.sidebarHomeY0() - homeStartEvdev) < pixelToEvdev(10, displayH, screenMax),
                 qPrintable(QString("Home zone start %1 not near expected %2")
                     .arg(reader.sidebarHomeY0()).arg(homeStartEvdev)));
    }

    // Horizontal bottom sidebar at 1024x600: home zone on the right side, width matches touchMin
    void horizontalBottom_1024x600_homeZonePosition()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);

        reader.setSidebar(true, 150, "bottom");

        // scale = 1.0
        // spacingPx = 8, touchMinPx = 56
        // homeStartPx = 1024 - 8 - 56 = 960
        // volEndPx = 960 - 8 - 1 - 8 = 943
        const int screenMax = 4095;
        const int displayW = 1024;

        float homeStartEvdev = pixelToEvdev(960, displayW, screenMax);

        // Home zone should start near pixel 960
        QVERIFY2(qAbs(reader.sidebarHomeX0() - homeStartEvdev) < pixelToEvdev(10, displayW, screenMax),
                 qPrintable(QString("Home X zone start %1 not near expected %2")
                     .arg(reader.sidebarHomeX0()).arg(homeStartEvdev)));
    }

    // Volume zone covers the full visual volume bar range
    void volumeZoneCoversFullRange()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);

        reader.setSidebar(true, 150, "right");

        // Volume zone should start at 0
        QCOMPARE(reader.sidebarVolY0(), 0.0f);

        // Volume zone should cover most of the sidebar height
        // At 1024x600: volEndPx = 519, so vol covers 519/600 = 86.5% of height
        const int screenMax = 4095;
        float vol86pct = pixelToEvdev(static_cast<int>(600 * 0.80), 600, screenMax);

        QVERIFY2(reader.sidebarVolY1() > vol86pct,
                 qPrintable(QString("Volume zone too short: ends at %1, expected >%2")
                     .arg(reader.sidebarVolY1()).arg(vol86pct)));
    }

    // No large dead zone gap between volume and home zones
    void minimalGapBetweenVolumeAndHome()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);

        reader.setSidebar(true, 150, "right");

        // Gap between volume end and home start should be small (separator + spacing only)
        // At 1024x600: gap = homeStartPx - volEndPx = 536 - 519 = 17px
        float gap = reader.sidebarHomeY0() - reader.sidebarVolY1();
        float maxGapEvdev = pixelToEvdev(30, 600, 4095);  // 30px max acceptable gap

        QVERIFY2(gap > 0,
                 "Volume and home zones should not overlap");

        QVERIFY2(gap < maxGapEvdev,
                 qPrintable(QString("Gap between volume and home too large: %1 evdev (%2 px equivalent)")
                     .arg(gap).arg(gap * 600.0 / 4095.0)));
    }
    // TouchRouter dispatch: volume DOWN in vertical right sidebar claims the touch
    void touchRouter_volumeDownClaims()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);
        reader.setSidebar(true, 150, "right");

        TouchRouter& router = reader.router();

        // Touch in volume zone: X inside sidebar band, Y in volume range
        float midX = (reader.sidebarVolX0() + reader.sidebarVolX1()) / 2.0f;
        // Wait -- for vertical sidebar, X bounds are sidebarEvdevX0_/X1_ (no Vol accessors for X of vertical)
        // Volume zone: X = [sidebarEvdevX0_, sidebarEvdevX1_], Y = [sidebarVolY0_, sidebarVolY1_]
        // Use X in the sidebar band
        float sidebarX = pixelToEvdev(1024 - 75, 1024, 4095);  // mid-sidebar pixel
        float midVolY = reader.sidebarVolY1() / 2.0f;  // mid-volume zone

        bool claimed = router.dispatch(0, sidebarX, midVolY, TouchEvent::Down);
        QVERIFY2(claimed, "Volume zone DOWN should be claimed by router");

        // Subsequent MOVE on same slot should also be claimed (sticky finger)
        float newVolY = reader.sidebarVolY1() * 0.3f;
        bool moveClaimed = router.dispatch(0, sidebarX, newVolY, TouchEvent::Move);
        QVERIFY2(moveClaimed, "Volume zone MOVE (sticky finger) should be claimed");

        // UP releases the claim
        bool upClaimed = router.dispatch(0, sidebarX, newVolY, TouchEvent::Up);
        QVERIFY2(upClaimed, "Volume zone UP should be claimed (then released)");
    }

    // TouchRouter dispatch: home DOWN in vertical right sidebar claims and emits signal
    void touchRouter_homeDownEmitsSignal()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);
        reader.setSidebar(true, 150, "right");

        QSignalSpy homeSpy(&reader, &EvdevTouchReader::sidebarHome);
        QVERIFY(homeSpy.isValid());

        TouchRouter& router = reader.router();

        // Touch in home zone: X in sidebar band, Y in home range
        float sidebarX = pixelToEvdev(1024 - 75, 1024, 4095);
        float homeY = (reader.sidebarHomeY0() + reader.sidebarHomeY1()) / 2.0f;

        bool claimed = router.dispatch(0, sidebarX, homeY, TouchEvent::Down);
        QVERIFY2(claimed, "Home zone DOWN should be claimed");
        QCOMPARE(homeSpy.count(), 1);
    }

    // TouchRouter dispatch: volume drag emits sidebarVolumeSet signal
    void touchRouter_volumeDragEmitsSignal()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);
        reader.setSidebar(true, 150, "right");

        QSignalSpy volSpy(&reader, &EvdevTouchReader::sidebarVolumeSet);
        QVERIFY(volSpy.isValid());

        TouchRouter& router = reader.router();

        float sidebarX = pixelToEvdev(1024 - 75, 1024, 4095);

        // DOWN at top of volume zone (should be ~100%)
        float topVolY = reader.sidebarVolY0() + 10;
        router.dispatch(0, sidebarX, topVolY, TouchEvent::Down);
        QCOMPARE(volSpy.count(), 1);
        int vol = volSpy.at(0).at(0).toInt();
        QVERIFY2(vol > 90, qPrintable(QString("Top of volume zone should be near 100%, got %1").arg(vol)));

        // MOVE toward bottom (should decrease volume)
        float bottomVolY = reader.sidebarVolY1() - 10;
        router.dispatch(0, sidebarX, bottomVolY, TouchEvent::Move);
        QCOMPARE(volSpy.count(), 2);
        int vol2 = volSpy.at(1).at(0).toInt();
        QVERIFY2(vol2 < 10, qPrintable(QString("Bottom of volume zone should be near 0%, got %1").arg(vol2)));
    }

    // TouchRouter dispatch: touch outside sidebar zones is NOT claimed
    void touchRouter_outsideZoneNotClaimed()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);
        reader.setSidebar(true, 150, "right");

        TouchRouter& router = reader.router();

        // Touch in the middle of the screen (not in sidebar)
        float centerX = 2048;  // mid-evdev
        float centerY = 2048;

        bool claimed = router.dispatch(0, centerX, centerY, TouchEvent::Down);
        QVERIFY2(!claimed, "Touch outside sidebar should NOT be claimed (falls through to AA)");
    }

    // Disabling sidebar removes zones from router
    void disableSidebar_clearsRouterZones()
    {
        EvdevTouchReader reader(nullptr, "/dev/null", 1280, 720, 1024, 600);
        reader.setSidebar(true, 150, "right");

        TouchRouter& router = reader.router();

        // Verify sidebar zone is active
        float sidebarX = pixelToEvdev(1024 - 75, 1024, 4095);
        float homeY = (reader.sidebarHomeY0() + reader.sidebarHomeY1()) / 2.0f;
        QVERIFY(router.dispatch(0, sidebarX, homeY, TouchEvent::Down));
        router.dispatch(0, sidebarX, homeY, TouchEvent::Up);  // release claim

        // Disable sidebar
        reader.setSidebar(false, 0, "right");

        // Same touch should no longer be claimed
        bool claimed = router.dispatch(1, sidebarX, homeY, TouchEvent::Down);
        QVERIFY2(!claimed, "After disabling sidebar, touches should not be claimed");
    }
};

QTEST_MAIN(TestSidebarZones)
#include "test_sidebar_zones.moc"
