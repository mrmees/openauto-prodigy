// tests/test_grid_dimensions.cpp
#include <QtTest>
#include <QSignalSpy>
#include "ui/DisplayInfo.hpp"

class TestGridDimensions : public QObject {
    Q_OBJECT
private slots:
    void testDefault1024x600();
    void test800x480();
    void test1280x720();
    void test1920x1080();
    void testClampMinimum();
    void testClampMaximum();
    void testSignalEmittedOnChange();
    void testNoSignalWhenUnchanged();
};

void TestGridDimensions::testDefault1024x600() {
    oap::DisplayInfo info;
    // Default is 1024x600 -> should be 6x4
    QCOMPARE(info.gridColumns(), 6);
    QCOMPARE(info.gridRows(), 4);
}

void TestGridDimensions::test800x480() {
    oap::DisplayInfo info;
    info.setWindowSize(800, 480);
    QCOMPARE(info.gridColumns(), 5);
    QCOMPARE(info.gridRows(), 3);
}

void TestGridDimensions::test1280x720() {
    oap::DisplayInfo info;
    info.setWindowSize(1280, 720);
    // 1240/150=8.26->8, 624/125=4.99->4
    QCOMPARE(info.gridColumns(), 8);
    QCOMPARE(info.gridRows(), 4);
}

void TestGridDimensions::test1920x1080() {
    oap::DisplayInfo info;
    info.setWindowSize(1920, 1080);
    // Clamped: columns max 8, rows max 6
    QCOMPARE(info.gridColumns(), 8);
    QCOMPARE(info.gridRows(), 6);
}

void TestGridDimensions::testClampMinimum() {
    oap::DisplayInfo info;
    // Tiny display -> minimum 3x2
    info.setWindowSize(300, 200);
    QCOMPARE(info.gridColumns(), 3);
    QCOMPARE(info.gridRows(), 2);
}

void TestGridDimensions::testClampMaximum() {
    oap::DisplayInfo info;
    // Huge display -> maximum 8x6
    info.setWindowSize(3840, 2160);
    QCOMPARE(info.gridColumns(), 8);
    QCOMPARE(info.gridRows(), 6);
}

void TestGridDimensions::testSignalEmittedOnChange() {
    oap::DisplayInfo info;
    QSignalSpy spy(&info, &oap::DisplayInfo::gridDimensionsChanged);

    // Change from default 1024x600 (6x4) to 800x480 (5x3) -- dimensions change
    info.setWindowSize(800, 480);
    QCOMPARE(spy.count(), 1);
}

void TestGridDimensions::testNoSignalWhenUnchanged() {
    oap::DisplayInfo info;
    QSignalSpy spy(&info, &oap::DisplayInfo::gridDimensionsChanged);

    // Set to a size that still results in 6x4 (same as default)
    // 1024x600 -> 6x4, try 1023x599 which should also be 6x4
    // usable = 1023-40=983, 983/155=6.3 -> 6
    // usable = 599-40-56=503, 503/145=3.4 -> 3 -- actually different!
    // Try 1050x620: usable=1010/155=6.5->6, usable=620-96=524/145=3.6->4
    info.setWindowSize(1050, 620);
    QCOMPARE(spy.count(), 0); // grid dims didn't change from default 6x4
}

QTEST_MAIN(TestGridDimensions)
#include "test_grid_dimensions.moc"
