#include <QTest>
#include "core/aa/StatusIndicatorHelper.hpp"

class TestIndicatorMapping : public QObject {
    Q_OBJECT
private slots:
    // --- Battery icon mapping ---

    void testBatteryFull() {
        QCOMPARE(oap::aa::batteryIconForLevel(100), QString("\ue1a4"));
    }

    void testBattery96() {
        QCOMPARE(oap::aa::batteryIconForLevel(96), QString("\ue1a4"));
    }

    void testBattery6Bar() {
        QCOMPARE(oap::aa::batteryIconForLevel(85), QString("\uebd5"));
    }

    void testBattery6BarLow() {
        QCOMPARE(oap::aa::batteryIconForLevel(80), QString("\uebd5"));
    }

    void testBattery5Bar() {
        QCOMPARE(oap::aa::batteryIconForLevel(65), QString("\uebd4"));
    }

    void testBattery5BarLow() {
        QCOMPARE(oap::aa::batteryIconForLevel(60), QString("\uebd4"));
    }

    void testBattery4Bar() {
        QCOMPARE(oap::aa::batteryIconForLevel(45), QString("\uebd3"));
    }

    void testBattery4BarLow() {
        QCOMPARE(oap::aa::batteryIconForLevel(40), QString("\uebd3"));
    }

    void testBattery3Bar() {
        QCOMPARE(oap::aa::batteryIconForLevel(25), QString("\uebd2"));
    }

    void testBattery3BarLow() {
        QCOMPARE(oap::aa::batteryIconForLevel(20), QString("\uebd2"));
    }

    void testBattery1Bar() {
        QCOMPARE(oap::aa::batteryIconForLevel(10), QString("\uebd0"));
    }

    void testBattery1BarMin() {
        QCOMPARE(oap::aa::batteryIconForLevel(1), QString("\uebd0"));
    }

    void testBatteryAlert() {
        QCOMPARE(oap::aa::batteryIconForLevel(0), QString("\ue19c"));
    }

    void testBatteryNoData() {
        QCOMPARE(oap::aa::batteryIconForLevel(-1), QString());
    }

    void testBatteryInvalidNegative() {
        QCOMPARE(oap::aa::batteryIconForLevel(-50), QString());
    }

    void testBatteryOver100() {
        // Over 100 should still return full
        QCOMPARE(oap::aa::batteryIconForLevel(101), QString("\ue1a4"));
    }

    // --- Signal strength icon mapping ---

    void testSignal4Bar() {
        QCOMPARE(oap::aa::signalIconForStrength(4), QString("\uf0ac"));
    }

    void testSignal3Bar() {
        QCOMPARE(oap::aa::signalIconForStrength(3), QString("\uf0ab"));
    }

    void testSignal2Bar() {
        QCOMPARE(oap::aa::signalIconForStrength(2), QString("\uf0aa"));
    }

    void testSignal1Bar() {
        QCOMPARE(oap::aa::signalIconForStrength(1), QString("\uf0a9"));
    }

    void testSignal0Bar() {
        QCOMPARE(oap::aa::signalIconForStrength(0), QString("\uf0a8"));
    }

    void testSignalNoData() {
        QCOMPARE(oap::aa::signalIconForStrength(-1), QString());
    }

    void testSignalInvalidNegative() {
        QCOMPARE(oap::aa::signalIconForStrength(-5), QString());
    }

    void testSignalClampHigh() {
        // Values above 4 should clamp to 4-bar
        QCOMPARE(oap::aa::signalIconForStrength(5), QString("\uf0ac"));
    }

    void testSignalClampVeryHigh() {
        QCOMPARE(oap::aa::signalIconForStrength(100), QString("\uf0ac"));
    }
};

QTEST_MAIN(TestIndicatorMapping)
#include "test_indicator_mapping.moc"
