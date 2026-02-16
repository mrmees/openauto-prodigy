#include <QtTest/QtTest>
#include <QSignalSpy>
#include <memory>
#include "ui/ThemeController.hpp"

using namespace oap;

class TestThemeController : public QObject {
    Q_OBJECT

private slots:
    void testDefaultsToDay();
    void testToggleMode();
    void testColorsChangeOnToggle();
    void testSignalsEmitted();
    void testSetModeSameNoSignal();
};

void TestThemeController::testDefaultsToDay()
{
    auto config = std::make_shared<Configuration>();
    ThemeController theme(config);

    QCOMPARE(theme.mode(), ThemeMode::Day);
    QCOMPARE(theme.backgroundColor(), config->backgroundColor(ThemeMode::Day));
    QCOMPARE(theme.highlightColor(), config->highlightColor(ThemeMode::Day));
    QCOMPARE(theme.specialFontColor(), config->specialFontColor(ThemeMode::Day));
    QCOMPARE(theme.normalFontColor(), config->normalFontColor(ThemeMode::Day));
    QCOMPARE(theme.iconColor(), config->iconColor(ThemeMode::Day));
}

void TestThemeController::testToggleMode()
{
    auto config = std::make_shared<Configuration>();
    ThemeController theme(config);

    QCOMPARE(theme.mode(), ThemeMode::Day);

    theme.toggleMode();
    QCOMPARE(theme.mode(), ThemeMode::Night);

    theme.toggleMode();
    QCOMPARE(theme.mode(), ThemeMode::Day);
}

void TestThemeController::testColorsChangeOnToggle()
{
    auto config = std::make_shared<Configuration>();
    ThemeController theme(config);

    QColor dayBg = theme.backgroundColor();
    QColor dayHighlight = theme.highlightColor();

    theme.toggleMode();

    QColor nightBg = theme.backgroundColor();
    QColor nightHighlight = theme.highlightColor();

    // Day and night colors should differ (they do with default config)
    QVERIFY(dayBg != nightBg);
    QVERIFY(dayHighlight != nightHighlight);

    // Night colors should match config night colors
    QCOMPARE(nightBg, config->backgroundColor(ThemeMode::Night));
    QCOMPARE(nightHighlight, config->highlightColor(ThemeMode::Night));
}

void TestThemeController::testSignalsEmitted()
{
    auto config = std::make_shared<Configuration>();
    ThemeController theme(config);

    QSignalSpy modeSpy(&theme, &ThemeController::modeChanged);
    QSignalSpy colorsSpy(&theme, &ThemeController::colorsChanged);

    QVERIFY(modeSpy.isValid());
    QVERIFY(colorsSpy.isValid());

    theme.toggleMode();

    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(colorsSpy.count(), 1);

    theme.toggleMode();

    QCOMPARE(modeSpy.count(), 2);
    QCOMPARE(colorsSpy.count(), 2);
}

void TestThemeController::testSetModeSameNoSignal()
{
    auto config = std::make_shared<Configuration>();
    ThemeController theme(config);

    QSignalSpy modeSpy(&theme, &ThemeController::modeChanged);

    // Setting same mode should not emit
    theme.setMode(ThemeMode::Day);
    QCOMPARE(modeSpy.count(), 0);
}

QTEST_GUILESS_MAIN(TestThemeController)
#include "test_theme_controller.moc"
