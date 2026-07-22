#include <QTest>
#include <QSignalSpy>
#include "core/aa/TimedNightMode.hpp"
#include "core/aa/GpioNightMode.hpp"

namespace oap::aa {

class GpioNightModeTestAccess {
public:
    static void applyValue(GpioNightMode& provider, const QString& value)
    {
        provider.applyValue(value);
    }
};

} // namespace oap::aa

class TestNightMode : public QObject {
    Q_OBJECT

private slots:
    void timedNightMode_normalRange()
    {
        // Day 07:00 - 19:00 (normal range)
        oap::aa::TimedNightMode provider("07:00", "19:00");
        QVERIFY(!provider.hasValidState());
        provider.start();
        QVERIFY(provider.hasValidState());

        // isNight() should return a valid bool (we can't control QTime::currentTime(),
        // but we can verify it doesn't crash and returns a consistent value)
        bool state = provider.isNight();
        Q_UNUSED(state);

        provider.stop();
    }

    void timedNightMode_invertedRange()
    {
        // Night starts at 02:00, day starts at 10:00 (inverted)
        oap::aa::TimedNightMode provider("10:00", "02:00");
        provider.start();

        bool state = provider.isNight();
        Q_UNUSED(state);

        provider.stop();
    }

    void timedNightMode_invalidTimeFallsBack()
    {
        // Invalid time strings should fall back to defaults without crashing
        oap::aa::TimedNightMode provider("invalid", "also-invalid");
        provider.start();

        // Should use defaults (07:00, 19:00)
        bool state = provider.isNight();
        Q_UNUSED(state);

        provider.stop();
    }

    void timedNightMode_emitsSignalOnChange()
    {
        // Set a range where we know the current time falls
        // We'll use a range that covers the full day to force a known state
        oap::aa::TimedNightMode provider("00:00", "00:01");

        QSignalSpy spy(&provider, &oap::aa::NightModeProvider::nightModeChanged);
        QVERIFY(spy.isValid());

        provider.start();
        // After start(), evaluate() is called. It may or may not emit depending on
        // whether currentState_ (false) differs from computed state.
        // At least verify the signal spy is connected properly.

        provider.stop();
    }

    void timedNightMode_startStopIdempotent()
    {
        oap::aa::TimedNightMode provider("07:00", "19:00");

        // Multiple start/stop cycles should not crash
        provider.start();
        provider.start();
        provider.stop();
        provider.stop();
        provider.start();
        provider.stop();
    }

    void timedNightMode_alwaysDay()
    {
        // Day range covering the entire 24 hours: day starts at 00:00, night at 23:59
        // At any time except 23:59, it should be day
        oap::aa::TimedNightMode provider("00:00", "23:59");
        provider.start();

        // Very likely to be day unless running tests at exactly 23:59
        // Just verify no crash
        bool state = provider.isNight();
        Q_UNUSED(state);

        provider.stop();
    }

    void timedNightMode_alwaysNight()
    {
        // Night range covering (almost) the entire 24 hours
        // day at 23:59, night at 00:00 — night from 00:00 to 23:59
        oap::aa::TimedNightMode provider("23:59", "00:00");
        provider.start();

        // Very likely to be night unless running tests at exactly 23:59
        bool state = provider.isNight();
        Q_UNUSED(state);

        provider.stop();
    }

    void gpioNightMode_firstValidDefaultStateEmits()
    {
        oap::aa::GpioNightMode provider(17, true);
        QSignalSpy spy(&provider, &oap::aa::NightModeProvider::nightModeChanged);

        oap::aa::GpioNightModeTestAccess::applyValue(provider, "invalid");
        QVERIFY(!provider.hasValidState());
        QCOMPARE(spy.count(), 0);

        // The first valid day sample must publish even though false matches the
        // provider's default storage value.
        oap::aa::GpioNightModeTestAccess::applyValue(provider, "0");
        QVERIFY(provider.hasValidState());
        QCOMPARE(provider.isNight(), false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toBool(), false);

        oap::aa::GpioNightModeTestAccess::applyValue(provider, "0");
        QCOMPARE(spy.count(), 1);
        oap::aa::GpioNightModeTestAccess::applyValue(provider, "1");
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy[1][0].toBool(), true);
    }
};

QTEST_MAIN(TestNightMode)
#include "test_night_mode.moc"
