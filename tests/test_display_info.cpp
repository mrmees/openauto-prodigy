#include <QtTest>
#include <QSignalSpy>
#include "ui/DisplayInfo.hpp"

class TestDisplayInfo : public QObject {
    Q_OBJECT
private slots:
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

    // One dimension zero
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
    // Set same as defaults -- should not emit
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
    // Set same as default -- should not emit
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

QTEST_MAIN(TestDisplayInfo)
#include "test_display_info.moc"
