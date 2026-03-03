#include <QtTest>
#include "core/aa/EvdevTouchReader.hpp"

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
};

QTEST_MAIN(TestSidebarZones)
#include "test_sidebar_zones.moc"
