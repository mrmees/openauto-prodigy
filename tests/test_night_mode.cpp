#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "core/aa/GpioNightMode.hpp"
#include "core/aa/TimedNightMode.hpp"
#include "core/services/NightModeService.hpp"
#include "core/services/ThemeService.hpp"

namespace oap::aa {

class TimedNightModeTestAccess {
public:
    static void evaluate(TimedNightMode& provider) { provider.evaluate(); }
};

class GpioNightModeTestAccess {
public:
    static void applyValue(GpioNightMode& provider, const QString& value)
    {
        provider.applyValue(value);
    }

    static void poll(GpioNightMode& provider) { provider.poll(); }
};

class CountingNightModeProvider : public NightModeProvider {
public:
    bool isNight() const override { return state_; }
    bool hasValidState() const override { return valid_; }
    void start() override { ++startCount_; }
    void stop() override { ++stopCount_; }

    void publish(bool state)
    {
        state_ = state;
        valid_ = true;
        emit nightModeChanged(state);
    }

    int startCount() const { return startCount_; }
    int stopCount() const { return stopCount_; }

private:
    bool state_ = false;
    bool valid_ = false;
    int startCount_ = 0;
    int stopCount_ = 0;
};

} // namespace oap::aa

namespace {

void writeFile(const QString& path, const QByteArray& contents = {})
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
             qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
}

void createControlFiles(const QString& root)
{
    writeFile(QDir(root).filePath(QStringLiteral("export")));
    writeFile(QDir(root).filePath(QStringLiteral("unexport")));
}

QString createGpioDirectory(const QString& root)
{
    const QString path = QDir(root).filePath(QStringLiteral("gpio17"));
    return QDir().mkpath(path) ? path : QString();
}

} // namespace

class TestNightMode : public QObject {
    Q_OBJECT

private slots:
    void timedNightMode_boundaries_data()
    {
        QTest::addColumn<QString>("dayStart");
        QTest::addColumn<QString>("nightStart");
        QTest::addColumn<QTime>("now");
        QTest::addColumn<bool>("expectedNight");

        QTest::newRow("normal-before-day") << "07:00" << "19:00" << QTime(6, 59) << true;
        QTest::newRow("normal-at-day") << "07:00" << "19:00" << QTime(7, 0) << false;
        QTest::newRow("normal-before-night") << "07:00" << "19:00" << QTime(18, 59) << false;
        QTest::newRow("normal-at-night") << "07:00" << "19:00" << QTime(19, 0) << true;
        QTest::newRow("inverted-before-night") << "10:00" << "02:00" << QTime(1, 59) << false;
        QTest::newRow("inverted-at-night") << "10:00" << "02:00" << QTime(2, 0) << true;
        QTest::newRow("inverted-before-day") << "10:00" << "02:00" << QTime(9, 59) << true;
        QTest::newRow("inverted-at-day") << "10:00" << "02:00" << QTime(10, 0) << false;
    }

    void timedNightMode_boundaries()
    {
        QFETCH(QString, dayStart);
        QFETCH(QString, nightStart);
        QFETCH(QTime, now);
        QFETCH(bool, expectedNight);

        oap::aa::TimedNightMode provider(
            dayStart, nightStart, nullptr, [&now] { return now; });
        QVERIFY(!provider.hasValidState());
        provider.start();
        QVERIFY(provider.hasValidState());
        QCOMPARE(provider.isNight(), expectedNight);
        provider.stop();
    }

    void timedNightMode_invalidTimeFallsBack()
    {
        const QTime noon(12, 0);
        oap::aa::TimedNightMode provider(
            "invalid", "also-invalid", nullptr, [noon] { return noon; });
        provider.start();
        QVERIFY(provider.hasValidState());
        QCOMPARE(provider.isNight(), false);
        provider.stop();
    }

    void timedNightMode_signalsOnlyOnChange()
    {
        QTime now(23, 0);
        oap::aa::TimedNightMode provider(
            "07:00", "19:00", nullptr, [&now] { return now; });
        QSignalSpy spy(&provider, &oap::aa::NightModeProvider::nightModeChanged);

        provider.start();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toBool(), true);

        oap::aa::TimedNightModeTestAccess::evaluate(provider);
        QCOMPARE(spy.count(), 1);

        now = QTime(12, 0);
        oap::aa::TimedNightModeTestAccess::evaluate(provider);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy[1][0].toBool(), false);

        oap::aa::TimedNightModeTestAccess::evaluate(provider);
        QCOMPARE(spy.count(), 2);
        provider.stop();
    }

    void timedNightMode_startStopIdempotent()
    {
        const QTime noon(12, 0);
        oap::aa::TimedNightMode provider(
            "07:00", "19:00", nullptr, [noon] { return noon; });
        provider.start();
        provider.start();
        provider.stop();
        provider.stop();
        provider.start();
        provider.stop();
    }

    void gpioNightMode_firstValidAndChangeOnly()
    {
        oap::aa::GpioNightMode provider(17, true);
        QSignalSpy spy(&provider, &oap::aa::NightModeProvider::nightModeChanged);

        oap::aa::GpioNightModeTestAccess::applyValue(provider, "invalid");
        QVERIFY(!provider.hasValidState());
        QCOMPARE(spy.count(), 0);

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

    void gpioNightMode_exportFailureRecovers()
    {
        QTemporaryDir sysfs;
        QVERIFY(sysfs.isValid());
        oap::aa::GpioNightMode provider(17, true, nullptr, sysfs.path());
        QSignalSpy spy(&provider, &oap::aa::NightModeProvider::nightModeChanged);

        provider.start();
        QVERIFY(!provider.hasValidState());

        createControlFiles(sysfs.path());
        const QString gpio = createGpioDirectory(sysfs.path());
        QVERIFY(!gpio.isEmpty());
        writeFile(QDir(gpio).filePath(QStringLiteral("direction")));
        writeFile(QDir(gpio).filePath(QStringLiteral("value")), "0\n");
        oap::aa::GpioNightModeTestAccess::poll(provider);

        QVERIFY(provider.hasValidState());
        QCOMPARE(provider.isNight(), false);
        QCOMPARE(spy.count(), 1);
        oap::aa::GpioNightModeTestAccess::poll(provider);
        QCOMPARE(spy.count(), 1);
        provider.stop();
    }

    void gpioNightMode_directionFailureRecovers()
    {
        QTemporaryDir sysfs;
        QVERIFY(sysfs.isValid());
        createControlFiles(sysfs.path());
        const QString gpio = createGpioDirectory(sysfs.path());
        QVERIFY(!gpio.isEmpty());
        writeFile(QDir(gpio).filePath(QStringLiteral("value")), "1\n");
        oap::aa::GpioNightMode provider(17, true, nullptr, sysfs.path());
        QSignalSpy spy(&provider, &oap::aa::NightModeProvider::nightModeChanged);

        provider.start();
        QVERIFY(!provider.hasValidState());

        writeFile(QDir(gpio).filePath(QStringLiteral("direction")));
        oap::aa::GpioNightModeTestAccess::poll(provider);
        QVERIFY(provider.hasValidState());
        QCOMPARE(provider.isNight(), true);
        QCOMPARE(spy.count(), 1);
        provider.stop();
    }

    void gpioNightMode_valueFailureRecovers()
    {
        QTemporaryDir sysfs;
        QVERIFY(sysfs.isValid());
        createControlFiles(sysfs.path());
        const QString gpio = createGpioDirectory(sysfs.path());
        QVERIFY(!gpio.isEmpty());
        writeFile(QDir(gpio).filePath(QStringLiteral("direction")));
        oap::aa::GpioNightMode provider(17, false, nullptr, sysfs.path());
        QSignalSpy spy(&provider, &oap::aa::NightModeProvider::nightModeChanged);

        provider.start();
        QVERIFY(!provider.hasValidState());

        writeFile(QDir(gpio).filePath(QStringLiteral("value")), "1\n");
        oap::aa::GpioNightModeTestAccess::poll(provider);
        QVERIFY(provider.hasValidState());
        QCOMPARE(provider.isNight(), false);
        QCOMPARE(spy.count(), 1);

        // A later read failure invalidates the source without replacing state;
        // recovery republishes the first valid value once even when unchanged.
        QVERIFY(QFile::remove(QDir(gpio).filePath(QStringLiteral("value"))));
        oap::aa::GpioNightModeTestAccess::poll(provider);
        QVERIFY(!provider.hasValidState());
        QCOMPARE(provider.isNight(), false);
        writeFile(QDir(gpio).filePath(QStringLiteral("value")), "1\n");
        oap::aa::GpioNightModeTestAccess::poll(provider);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy[1][0].toBool(), false);
        provider.stop();
    }

    void nightModeService_drivesThemeAndChangeOnlyState()
    {
        QTime now(12, 0);
        auto provider = std::make_unique<oap::aa::TimedNightMode>(
            "07:00", "19:00", nullptr, [&now] { return now; });
        auto* providerPtr = provider.get();
        oap::ThemeService theme;
        theme.setForceDarkMode(true);
        oap::NightModeService service(std::move(provider), &theme);
        QSignalSpy spy(&service, &oap::NightModeService::nightModeChanged);

        service.start();
        QVERIFY(service.hasValidState());
        QCOMPARE(service.isNight(), false);
        QCOMPARE(theme.realNightMode(), false);
        QCOMPARE(theme.nightMode(), true); // force-dark remains a shell-only override
        QCOMPARE(spy.count(), 1);          // initial valid day is still published

        oap::aa::TimedNightModeTestAccess::evaluate(*providerPtr);
        QCOMPARE(spy.count(), 1);
        now = QTime(23, 0);
        oap::aa::TimedNightModeTestAccess::evaluate(*providerPtr);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(service.isNight(), true);
        QCOMPARE(theme.realNightMode(), true);
        oap::aa::TimedNightModeTestAccess::evaluate(*providerPtr);
        QCOMPARE(spy.count(), 2);
        service.stop();
    }

    void nightModeService_ownsProviderForApplicationLifetime()
    {
        auto provider = std::make_unique<oap::aa::CountingNightModeProvider>();
        auto* providerPtr = provider.get();
        oap::NightModeService service(std::move(provider), nullptr);

        service.start();
        service.start();
        QCOMPARE(providerPtr->startCount(), 1);
        providerPtr->publish(false);
        QVERIFY(service.hasValidState());
        QCOMPARE(service.isNight(), false);
        service.stop();
        service.stop();
        QCOMPARE(providerPtr->stopCount(), 1);
    }
};

QTEST_MAIN(TestNightMode)
#include "test_night_mode.moc"
