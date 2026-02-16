#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "core/Configuration.hpp"

using namespace oap;

class TestConfiguration : public QObject {
    Q_OBJECT

private slots:
    void testLoadDefaults();
    void testLoadFromFile();
    void testDayNightColors();
    void testSaveRoundTrip();
};

void TestConfiguration::testLoadDefaults() {
    Configuration cfg;

    // AndroidAuto defaults
    QCOMPARE(cfg.dayNightModeController(), true);
    QCOMPARE(cfg.showClockInAndroidAuto(), true);
    QCOMPARE(cfg.showTopBar(), true);

    // Display defaults
    QCOMPARE(cfg.screenType(), ScreenType::Standard);
    QCOMPARE(cfg.handednessOfTraffic(), Handedness::LHD);
    QCOMPARE(cfg.screenDpi(), 140);

    // Audio defaults
    QCOMPARE(cfg.volumeStep(), 5);

    // Bluetooth defaults
    QCOMPARE(cfg.bluetoothAdapterType(), BluetoothAdapterType::Local);

    // System defaults
    QCOMPARE(cfg.language(), QStringLiteral("en_US"));
    QCOMPARE(cfg.timeFormat(), TimeFormat::Format12H);

    // Day colors should be valid
    QVERIFY(cfg.backgroundColor(ThemeMode::Day).isValid());
    QCOMPARE(cfg.backgroundColor(ThemeMode::Day), QColor(QStringLiteral("#1a1a2e")));

    // Night colors should be valid
    QVERIFY(cfg.backgroundColor(ThemeMode::Night).isValid());
    QCOMPARE(cfg.backgroundColor(ThemeMode::Night), QColor(QStringLiteral("#0a0a1a")));
}

void TestConfiguration::testLoadFromFile() {
    Configuration cfg;
    cfg.load(QStringLiteral(TEST_DATA_DIR "/test_config.ini"));

    QCOMPARE(cfg.dayNightModeController(), true);
    QCOMPARE(cfg.showClockInAndroidAuto(), true);
    QCOMPARE(cfg.showTopBar(), true);

    QCOMPARE(cfg.screenType(), ScreenType::Standard);
    QCOMPARE(cfg.handednessOfTraffic(), Handedness::LHD);
    QCOMPARE(cfg.screenDpi(), 140);

    QCOMPARE(cfg.volumeStep(), 5);
    QCOMPARE(cfg.bluetoothAdapterType(), BluetoothAdapterType::Local);

    QCOMPARE(cfg.language(), QStringLiteral("en_US"));
    QCOMPARE(cfg.timeFormat(), TimeFormat::Format12H);

    QCOMPARE(cfg.backgroundColor(ThemeMode::Day), QColor(QStringLiteral("#1a1a2e")));
    QCOMPARE(cfg.highlightColor(ThemeMode::Day), QColor(QStringLiteral("#e94560")));
    QCOMPARE(cfg.controlBackgroundColor(ThemeMode::Day), QColor(QStringLiteral("#16213e")));

    QCOMPARE(cfg.backgroundColor(ThemeMode::Night), QColor(QStringLiteral("#0a0a1a")));
    QCOMPARE(cfg.highlightColor(ThemeMode::Night), QColor(QStringLiteral("#c73650")));
}

void TestConfiguration::testDayNightColors() {
    Configuration cfg;
    cfg.load(QStringLiteral(TEST_DATA_DIR "/test_config.ini"));

    // Day and night background colors should differ
    QVERIFY(cfg.backgroundColor(ThemeMode::Day) != cfg.backgroundColor(ThemeMode::Night));
    QVERIFY(cfg.highlightColor(ThemeMode::Day) != cfg.highlightColor(ThemeMode::Night));
    QVERIFY(cfg.normalFontColor(ThemeMode::Day) != cfg.normalFontColor(ThemeMode::Night));
    QVERIFY(cfg.barShadowColor(ThemeMode::Day) != cfg.barShadowColor(ThemeMode::Night));

    // All 13 color getters should return valid colors
    auto checkValid = [&](ThemeMode mode) {
        QVERIFY(cfg.backgroundColor(mode).isValid());
        QVERIFY(cfg.highlightColor(mode).isValid());
        QVERIFY(cfg.controlBackgroundColor(mode).isValid());
        QVERIFY(cfg.controlForegroundColor(mode).isValid());
        QVERIFY(cfg.normalFontColor(mode).isValid());
        QVERIFY(cfg.specialFontColor(mode).isValid());
        QVERIFY(cfg.descriptionFontColor(mode).isValid());
        QVERIFY(cfg.barBackgroundColor(mode).isValid());
        QVERIFY(cfg.controlBoxBackgroundColor(mode).isValid());
        QVERIFY(cfg.gaugeIndicatorColor(mode).isValid());
        QVERIFY(cfg.iconColor(mode).isValid());
        QVERIFY(cfg.sideWidgetBackgroundColor(mode).isValid());
        QVERIFY(cfg.barShadowColor(mode).isValid());
    };
    checkValid(ThemeMode::Day);
    checkValid(ThemeMode::Night);
}

void TestConfiguration::testSaveRoundTrip() {
    // Load from known config
    Configuration original;
    original.load(QStringLiteral(TEST_DATA_DIR "/test_config.ini"));

    // Save to temp file
    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(true);
    QVERIFY(tmpFile.open());
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    original.save(tmpPath);

    // Reload from saved file
    Configuration reloaded;
    reloaded.load(tmpPath);

    // Compare all values
    QCOMPARE(reloaded.dayNightModeController(), original.dayNightModeController());
    QCOMPARE(reloaded.showClockInAndroidAuto(), original.showClockInAndroidAuto());
    QCOMPARE(reloaded.showTopBar(), original.showTopBar());
    QCOMPARE(reloaded.screenType(), original.screenType());
    QCOMPARE(reloaded.handednessOfTraffic(), original.handednessOfTraffic());
    QCOMPARE(reloaded.screenDpi(), original.screenDpi());
    QCOMPARE(reloaded.volumeStep(), original.volumeStep());
    QCOMPARE(reloaded.bluetoothAdapterType(), original.bluetoothAdapterType());
    QCOMPARE(reloaded.language(), original.language());
    QCOMPARE(reloaded.timeFormat(), original.timeFormat());

    // Compare colors for both modes
    auto compareColors = [&](ThemeMode mode) {
        QCOMPARE(reloaded.backgroundColor(mode), original.backgroundColor(mode));
        QCOMPARE(reloaded.highlightColor(mode), original.highlightColor(mode));
        QCOMPARE(reloaded.controlBackgroundColor(mode), original.controlBackgroundColor(mode));
        QCOMPARE(reloaded.controlForegroundColor(mode), original.controlForegroundColor(mode));
        QCOMPARE(reloaded.normalFontColor(mode), original.normalFontColor(mode));
        QCOMPARE(reloaded.specialFontColor(mode), original.specialFontColor(mode));
        QCOMPARE(reloaded.descriptionFontColor(mode), original.descriptionFontColor(mode));
        QCOMPARE(reloaded.barBackgroundColor(mode), original.barBackgroundColor(mode));
        QCOMPARE(reloaded.controlBoxBackgroundColor(mode), original.controlBoxBackgroundColor(mode));
        QCOMPARE(reloaded.gaugeIndicatorColor(mode), original.gaugeIndicatorColor(mode));
        QCOMPARE(reloaded.iconColor(mode), original.iconColor(mode));
        QCOMPARE(reloaded.sideWidgetBackgroundColor(mode), original.sideWidgetBackgroundColor(mode));
        QCOMPARE(reloaded.barShadowColor(mode), original.barShadowColor(mode));
    };
    compareColors(ThemeMode::Day);
    compareColors(ThemeMode::Night);
}

QTEST_GUILESS_MAIN(TestConfiguration)
#include "test_configuration.moc"
