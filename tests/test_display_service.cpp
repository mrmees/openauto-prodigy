#include <QTest>
#include <QSignalSpy>
#include <QVector>
#include "core/services/DisplayService.hpp"

class FakeDisplayService : public oap::DisplayService {
public:
    QVector<int> appliedBrightness;

protected:
    void applyBrightness(int value) override
    {
        appliedBrightness.append(value);
    }
};

class TestDisplayService : public QObject {
    Q_OBJECT

private slots:
    void defaultBrightness()
    {
        FakeDisplayService svc;
        QCOMPARE(svc.brightness(), 80);
    }

    void setBrightnessEmitsSignal()
    {
        FakeDisplayService svc;
        QSignalSpy spy(&svc, &oap::DisplayService::brightnessChanged);
        svc.setBrightness(50);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(svc.brightness(), 50);
    }

    void setBrightnessClampsLow()
    {
        FakeDisplayService svc;
        QSignalSpy spy(&svc, &oap::DisplayService::brightnessChanged);
        svc.setBrightness(2);
        QCOMPARE(svc.brightness(), 5);
        QCOMPARE(spy.count(), 1);
    }

    void setBrightnessClampsHigh()
    {
        FakeDisplayService svc;
        QSignalSpy spy(&svc, &oap::DisplayService::brightnessChanged);
        svc.setBrightness(200);
        QCOMPARE(svc.brightness(), 100);
        QCOMPARE(spy.count(), 1);
    }

    void initialDefaultBrightnessAppliesWithoutSignal()
    {
        FakeDisplayService svc;
        QSignalSpy spy(&svc, &oap::DisplayService::brightnessChanged);
        svc.setBrightness(80);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(svc.appliedBrightness, QVector<int>({80}));

        svc.setBrightness(80);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(svc.appliedBrightness, QVector<int>({80}));

        svc.setBrightness(50);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(svc.appliedBrightness, QVector<int>({80, 50}));

        svc.setBrightness(50);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(svc.appliedBrightness, QVector<int>({80, 50}));
    }

    void dimOverlayAtFull()
    {
        FakeDisplayService svc;
        svc.setBrightness(100);
        QCOMPARE(svc.dimOverlayOpacity(), 0.0);
    }

    void dimOverlayAtMin()
    {
        FakeDisplayService svc;
        svc.setBrightness(5);
        // (100 - 5) / 100.0 * 0.9 = 0.855
        QVERIFY(qFuzzyCompare(svc.dimOverlayOpacity(), 0.855));
    }

    void dimOverlayLinear()
    {
        FakeDisplayService svc;
        svc.setBrightness(50);
        // (100 - 50) / 100.0 * 0.9 = 0.45
        QVERIFY(qFuzzyCompare(svc.dimOverlayOpacity(), 0.45));
    }

};

QTEST_MAIN(TestDisplayService)
#include "test_display_service.moc"
