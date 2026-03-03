#include <QTest>
#include <QSignalSpy>
#include "core/services/DisplayService.hpp"

class TestDisplayService : public QObject {
    Q_OBJECT

private slots:
    void defaultBrightness()
    {
        oap::DisplayService svc;
        QCOMPARE(svc.brightness(), 80);
    }

    void setBrightnessEmitsSignal()
    {
        oap::DisplayService svc;
        QSignalSpy spy(&svc, &oap::DisplayService::brightnessChanged);
        svc.setBrightness(50);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(svc.brightness(), 50);
    }

    void setBrightnessClampsLow()
    {
        oap::DisplayService svc;
        QSignalSpy spy(&svc, &oap::DisplayService::brightnessChanged);
        svc.setBrightness(2);
        QCOMPARE(svc.brightness(), 5);
        QCOMPARE(spy.count(), 1);
    }

    void setBrightnessClampsHigh()
    {
        oap::DisplayService svc;
        QSignalSpy spy(&svc, &oap::DisplayService::brightnessChanged);
        svc.setBrightness(200);
        QCOMPARE(svc.brightness(), 100);
        QCOMPARE(spy.count(), 1);
    }

    void setBrightnessSameNoSignal()
    {
        oap::DisplayService svc;
        // Default is 80, setting to 80 should not emit
        QSignalSpy spy(&svc, &oap::DisplayService::brightnessChanged);
        svc.setBrightness(80);
        QCOMPARE(spy.count(), 0);
    }

    void dimOverlayAtFull()
    {
        oap::DisplayService svc;
        svc.setBrightness(100);
        QCOMPARE(svc.dimOverlayOpacity(), 0.0);
    }

    void dimOverlayAtMin()
    {
        oap::DisplayService svc;
        svc.setBrightness(5);
        // (100 - 5) / 100.0 * 0.9 = 0.855
        QVERIFY(qFuzzyCompare(svc.dimOverlayOpacity(), 0.855));
    }

    void dimOverlayLinear()
    {
        oap::DisplayService svc;
        svc.setBrightness(50);
        // (100 - 50) / 100.0 * 0.9 = 0.45
        QVERIFY(qFuzzyCompare(svc.dimOverlayOpacity(), 0.45));
    }

};

QTEST_MAIN(TestDisplayService)
#include "test_display_service.moc"
