// tests/test_grid_dimensions.cpp
// Tests cellSide across display configurations and density bias
#include <QtTest>
#include <QSignalSpy>
#include <cmath>
#include "ui/DisplayInfo.hpp"

class TestGridDimensions : public QObject {
    Q_OBJECT
private slots:
    void testCellSide800x480();
    void testCellSide1024x600();
    void testCellSide1280x720();
    void testCellSide1920x1080();
    void testDensityBiasProducesDifferentValues();
    void testReasonableGridCounts();
    void testCellSideChangedOnResize();
    void testNoSignalWhenUnchanged();
};

void TestGridDimensions::testCellSide800x480()
{
    oap::DisplayInfo info;
    info.setWindowSize(800, 480);
    qreal expected = std::hypot(800, 480) / 9.0;
    QVERIFY(qAbs(info.cellSide() - expected) < 0.1);
}

void TestGridDimensions::testCellSide1024x600()
{
    oap::DisplayInfo info;
    qreal expected = std::hypot(1024, 600) / 9.0;
    QVERIFY(qAbs(info.cellSide() - expected) < 0.1);
}

void TestGridDimensions::testCellSide1280x720()
{
    oap::DisplayInfo info;
    info.setWindowSize(1280, 720);
    qreal expected = std::hypot(1280, 720) / 9.0;
    QVERIFY(qAbs(info.cellSide() - expected) < 0.1);
}

void TestGridDimensions::testCellSide1920x1080()
{
    oap::DisplayInfo info;
    info.setWindowSize(1920, 1080);
    qreal expected = std::hypot(1920, 1080) / 9.0;
    QVERIFY(qAbs(info.cellSide() - expected) < 0.1);
}

void TestGridDimensions::testDensityBiasProducesDifferentValues()
{
    oap::DisplayInfo info;
    qreal base = info.cellSide();

    info.setDensityBias(-1);
    qreal larger = info.cellSide();
    QVERIFY(larger > base); // smaller divisor = larger cells

    info.setDensityBias(1);
    qreal smaller = info.cellSide();
    QVERIFY(smaller < base); // larger divisor = smaller cells
}

void TestGridDimensions::testReasonableGridCounts()
{
    // Verify that cellSide produces reasonable grid counts for common displays
    // (This is informational -- grid cols/rows are computed in QML, not DisplayInfo)
    struct TestCase {
        int w, h;
        int minCols, maxCols;
        int minRows, maxRows;
    };
    TestCase cases[] = {
        {800, 480, 5, 9, 3, 6},
        {1024, 600, 5, 9, 3, 6},
        {1280, 720, 6, 12, 3, 7},
        {1920, 1080, 6, 12, 3, 7},
    };

    for (const auto& tc : cases) {
        oap::DisplayInfo info;
        info.setWindowSize(tc.w, tc.h);
        int cols = static_cast<int>(std::floor(tc.w / info.cellSide()));
        int rows = static_cast<int>(std::floor(tc.h / info.cellSide()));
        QVERIFY2(cols >= tc.minCols && cols <= tc.maxCols,
                 qPrintable(QString("%1x%2: cols=%3").arg(tc.w).arg(tc.h).arg(cols)));
        QVERIFY2(rows >= tc.minRows && rows <= tc.maxRows,
                 qPrintable(QString("%1x%2: rows=%3").arg(tc.w).arg(tc.h).arg(rows)));
    }
}

void TestGridDimensions::testCellSideChangedOnResize()
{
    oap::DisplayInfo info;
    QSignalSpy spy(&info, &oap::DisplayInfo::cellSideChanged);
    info.setWindowSize(800, 480);
    QCOMPARE(spy.count(), 1);
}

void TestGridDimensions::testNoSignalWhenUnchanged()
{
    oap::DisplayInfo info;
    QSignalSpy spy(&info, &oap::DisplayInfo::cellSideChanged);
    // Same as defaults -- no change
    info.setWindowSize(1024, 600);
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestGridDimensions)
#include "test_grid_dimensions.moc"
