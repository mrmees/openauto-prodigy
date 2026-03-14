#include <QtTest>
#include <QSignalSpy>
#include "ui/DisplayInfo.hpp"

class TestDisplayInfo : public QObject {
    Q_OBJECT
private slots:
    // Existing window/screen properties (preserved)
    void testDefaultDimensions();
    void testSetWindowSizeUpdates();
    void testSetWindowSizeEmitsSignal();
    void testSetWindowSizeZeroIgnored();
    void testSetWindowSizeNegativeIgnored();
    void testSetWindowSizeSameValuesNoSignal();
    void testDefaultScreenSizeInches();
    void testSetScreenSizeInchesUpdates();
    void testSetScreenSizeInchesZeroIgnored();
    void testSetScreenSizeInchesNegativeIgnored();
    void testSetScreenSizeInchesSameValueNoSignal();
    void testComputedDpi1024x600At7();
    void testComputedDpi1280x720At10_1();

    // New cellSide tests
    void testCellSideDefault1024x600();
    void testCellSide800x480();
    void testCellSide1920x1080();
    void testDensityBiasNegative();
    void testDensityBiasPositive();
    void testCellSideMinimum();
    void testCellSideChangedSignal();
    void testDensityBiasClamp();
    void testSetFullscreen();
    void testSetQScreenDpi();
    void testSetConfigScreenSizeOverride();
};

void TestDisplayInfo::testDefaultDimensions()
{
    oap::DisplayInfo info;
    QCOMPARE(info.windowWidth(), 1024);
    QCOMPARE(info.windowHeight(), 600);
}

void TestDisplayInfo::testSetWindowSizeUpdates()
{
    oap::DisplayInfo info;
    info.setWindowSize(1280, 720);
    QCOMPARE(info.windowWidth(), 1280);
    QCOMPARE(info.windowHeight(), 720);
}

void TestDisplayInfo::testSetWindowSizeEmitsSignal()
{
    oap::DisplayInfo info;
    QSignalSpy spy(&info, &oap::DisplayInfo::windowSizeChanged);
    info.setWindowSize(800, 480);
    QCOMPARE(spy.count(), 1);
}

void TestDisplayInfo::testSetWindowSizeZeroIgnored()
{
    oap::DisplayInfo info;
    info.setWindowSize(0, 0);
    QCOMPARE(info.windowWidth(), 1024);
    QCOMPARE(info.windowHeight(), 600);

    info.setWindowSize(800, 0);
    QCOMPARE(info.windowWidth(), 1024);
    QCOMPARE(info.windowHeight(), 600);

    info.setWindowSize(0, 480);
    QCOMPARE(info.windowWidth(), 1024);
    QCOMPARE(info.windowHeight(), 600);
}

void TestDisplayInfo::testSetWindowSizeNegativeIgnored()
{
    oap::DisplayInfo info;
    info.setWindowSize(-1, -1);
    QCOMPARE(info.windowWidth(), 1024);
    QCOMPARE(info.windowHeight(), 600);
}

void TestDisplayInfo::testSetWindowSizeSameValuesNoSignal()
{
    oap::DisplayInfo info;
    QSignalSpy spy(&info, &oap::DisplayInfo::windowSizeChanged);
    info.setWindowSize(1024, 600);
    QCOMPARE(spy.count(), 0);
}

void TestDisplayInfo::testDefaultScreenSizeInches()
{
    oap::DisplayInfo info;
    QCOMPARE(info.screenSizeInches(), 7.0);
}

void TestDisplayInfo::testSetScreenSizeInchesUpdates()
{
    oap::DisplayInfo info;
    info.setScreenSizeInches(10.1);
    QCOMPARE(info.screenSizeInches(), 10.1);
}

void TestDisplayInfo::testSetScreenSizeInchesZeroIgnored()
{
    oap::DisplayInfo info;
    info.setScreenSizeInches(0);
    QCOMPARE(info.screenSizeInches(), 7.0);
}

void TestDisplayInfo::testSetScreenSizeInchesNegativeIgnored()
{
    oap::DisplayInfo info;
    info.setScreenSizeInches(-1);
    QCOMPARE(info.screenSizeInches(), 7.0);
}

void TestDisplayInfo::testSetScreenSizeInchesSameValueNoSignal()
{
    oap::DisplayInfo info;
    QSignalSpy spy(&info, &oap::DisplayInfo::screenSizeChanged);
    info.setScreenSizeInches(7.0);
    QCOMPARE(spy.count(), 0);
}

void TestDisplayInfo::testComputedDpi1024x600At7()
{
    oap::DisplayInfo info;
    // hypot(1024, 600) / 7.0 = 1186.7 / 7.0 = 169.5 -> round = 170
    QCOMPARE(info.computedDpi(), 170);
}

void TestDisplayInfo::testComputedDpi1280x720At10_1()
{
    oap::DisplayInfo info;
    info.setWindowSize(1280, 720);
    info.setScreenSizeInches(10.1);
    // hypot(1280, 720) / 10.1 = 1468.6 / 10.1 = 145.4 -> round = 145
    QCOMPARE(info.computedDpi(), 145);
}

// --- New cellSide tests ---

void TestDisplayInfo::testCellSideDefault1024x600()
{
    oap::DisplayInfo info;
    // diagPx = hypot(1024, 600) = 1186.7, cellSide = 1186.7 / 9.0 = ~131.9
    QVERIFY(qAbs(info.cellSide() - 131.9) < 1.0);
}

void TestDisplayInfo::testCellSide800x480()
{
    oap::DisplayInfo info;
    info.setWindowSize(800, 480);
    // diagPx = hypot(800, 480) = 933.0, cellSide = 933.0 / 9.0 = ~103.7
    QVERIFY(qAbs(info.cellSide() - 103.7) < 1.0);
}

void TestDisplayInfo::testCellSide1920x1080()
{
    oap::DisplayInfo info;
    info.setWindowSize(1920, 1080);
    // diagPx = hypot(1920, 1080) = 2203.4, cellSide = 2203.4 / 9.0 = ~244.8
    QVERIFY(qAbs(info.cellSide() - 244.8) < 1.0);
}

void TestDisplayInfo::testDensityBiasNegative()
{
    oap::DisplayInfo info;
    info.setDensityBias(-1);
    // divisor = 9.0 + (-1 * 0.8) = 8.2
    // cellSide = 1186.7 / 8.2 = ~144.7
    QVERIFY(qAbs(info.cellSide() - 144.7) < 1.0);
    QCOMPARE(info.densityBias(), -1);
}

void TestDisplayInfo::testDensityBiasPositive()
{
    oap::DisplayInfo info;
    info.setDensityBias(1);
    // divisor = 9.0 + (1 * 0.8) = 9.8
    // cellSide = 1186.7 / 9.8 = ~121.1
    QVERIFY(qAbs(info.cellSide() - 121.1) < 1.0);
    QCOMPARE(info.densityBias(), 1);
}

void TestDisplayInfo::testCellSideMinimum()
{
    oap::DisplayInfo info;
    // Tiny window: 100x100, diagPx = 141.4, cellSide = 141.4 / 9.0 = 15.7
    // Should be clamped to minimum 80
    info.setWindowSize(100, 100);
    QVERIFY(qAbs(info.cellSide() - 80.0) < 0.1);
}

void TestDisplayInfo::testCellSideChangedSignal()
{
    oap::DisplayInfo info;
    QSignalSpy spy(&info, &oap::DisplayInfo::cellSideChanged);
    info.setWindowSize(800, 480);
    QVERIFY(spy.count() >= 1);
}

void TestDisplayInfo::testDensityBiasClamp()
{
    oap::DisplayInfo info;
    // Values outside [-1, +1] should be clamped
    info.setDensityBias(5);
    QCOMPARE(info.densityBias(), 1);
    info.setDensityBias(-5);
    QCOMPARE(info.densityBias(), -1);
}

void TestDisplayInfo::testSetFullscreen()
{
    oap::DisplayInfo info;
    // Just ensure it doesn't crash and accepts the value
    info.setFullscreen(true);
    info.setFullscreen(false);
    // cellSide should still be valid
    QVERIFY(info.cellSide() >= 80.0);
}

void TestDisplayInfo::testSetQScreenDpi()
{
    oap::DisplayInfo info;
    // Set a valid DPI -- structural for future use
    info.setQScreenDpi(150.0);
    // cellSide should still be the same (resolution-independent)
    QVERIFY(qAbs(info.cellSide() - 131.9) < 1.0);
}

void TestDisplayInfo::testSetConfigScreenSizeOverride()
{
    oap::DisplayInfo info;
    info.setConfigScreenSizeOverride(10.0);
    // cellSide unchanged (resolution-independent by design)
    QVERIFY(qAbs(info.cellSide() - 131.9) < 1.0);
}

QTEST_MAIN(TestDisplayInfo)
#include "test_display_info.moc"
