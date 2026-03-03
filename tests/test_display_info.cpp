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

QTEST_MAIN(TestDisplayInfo)
#include "test_display_info.moc"
