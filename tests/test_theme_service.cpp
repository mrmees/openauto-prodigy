#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "core/services/ThemeService.hpp"

class TestThemeService : public QObject {
    Q_OBJECT

private slots:
    void loadThemeFromFile()
    {
        oap::ThemeService service;
        QVERIFY(service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml")));
        QCOMPARE(service.currentThemeId(), "default");
        QCOMPARE(service.fontFamily(), "Lato");
    }

    void dayModeColors()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        // Day mode is the default
        QVERIFY(!service.nightMode());
        QCOMPARE(service.backgroundColor(), QColor("#1a1a2e"));
        QCOMPARE(service.highlightColor(), QColor("#e94560"));
        QCOMPARE(service.normalFontColor(), QColor("#ffffff"));
        QCOMPARE(service.barBackgroundColor(), QColor("#0f3460"));
    }

    void nightModeColors()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        service.setNightMode(true);
        QVERIFY(service.nightMode());
        QCOMPARE(service.backgroundColor(), QColor("#0a0a14"));
        QCOMPARE(service.highlightColor(), QColor("#c73650"));
        QCOMPARE(service.normalFontColor(), QColor("#c0c0c0"));
    }

    void toggleModeFlips()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        QVERIFY(!service.nightMode());
        service.toggleMode();
        QVERIFY(service.nightMode());
        service.toggleMode();
        QVERIFY(!service.nightMode());
    }

    void colorByName()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        // IThemeService::color() uses the same key lookup
        QCOMPARE(service.color("highlight"), QColor("#e94560"));
        QCOMPARE(service.color("nonexistent"), QColor(Qt::transparent));
    }

    void signalsOnModeChange()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        QSignalSpy modeSpy(&service, &oap::ThemeService::modeChanged);
        QSignalSpy colorsSpy(&service, &oap::ThemeService::colorsChanged);

        service.setNightMode(true);
        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(colorsSpy.count(), 1);

        // Setting same mode again should not emit
        service.setNightMode(true);
        QCOMPARE(modeSpy.count(), 1);
    }

    void loadFromDirectory()
    {
        oap::ThemeService service;
        QString themeDir = QFINDTESTDATA("data/themes/default");
        QVERIFY(service.loadTheme(themeDir));
        QCOMPARE(service.currentThemeId(), "default");
    }

    void loadNonexistentFails()
    {
        oap::ThemeService service;
        QVERIFY(!service.loadThemeFile("/nonexistent/path/theme.yaml"));
        QVERIFY(!service.loadTheme("/nonexistent/dir"));
    }

    void nightFallbackToDay()
    {
        // If night mode is missing a key, it should fall back to day
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QFile f(tmpDir.filePath("theme.yaml"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        QTextStream out(&f);
        out << "id: partial\n"
            << "name: Partial Theme\n"
            << "day:\n"
            << "  background: \"#111111\"\n"
            << "  highlight: \"#222222\"\n"
            << "night:\n"
            << "  background: \"#333333\"\n";
        f.close();

        oap::ThemeService service;
        QVERIFY(service.loadThemeFile(f.fileName()));

        service.setNightMode(true);
        // background is defined in night
        QCOMPARE(service.color("background"), QColor("#333333"));
        // highlight falls back to day
        QCOMPARE(service.color("highlight"), QColor("#222222"));
    }
};

QTEST_MAIN(TestThemeService)
#include "test_theme_service.moc"
