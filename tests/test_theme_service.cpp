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

    // --- Multi-theme scanning and switching tests ---

    void scanFindsMultipleThemes()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create 3 theme subdirectories
        for (const auto& info : QList<QPair<QString,QString>>{
                {"alpha", "Alpha Theme"}, {"beta", "Beta Theme"}, {"gamma", "Gamma Theme"}}) {
            QDir(tmpDir.path()).mkpath(info.first);
            QFile f(tmpDir.filePath(info.first + "/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: " << info.first << "\n"
                << "name: " << info.second << "\n"
                << "day:\n"
                << "  background: \"#111111\"\n"
                << "night:\n"
                << "  background: \"#222222\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});

        QCOMPARE(service.availableThemes().size(), 3);
        QVERIFY(service.availableThemes().contains("alpha"));
        QVERIFY(service.availableThemes().contains("beta"));
        QVERIFY(service.availableThemes().contains("gamma"));
        QCOMPARE(service.availableThemeNames().size(), 3);
    }

    void scanSkipsMissingYaml()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Dir with theme.yaml
        QDir(tmpDir.path()).mkpath("valid");
        {
            QFile f(tmpDir.filePath("valid/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: valid\nname: Valid\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
            f.close();
        }

        // Dir without theme.yaml
        QDir(tmpDir.path()).mkpath("empty");

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});

        QCOMPARE(service.availableThemes().size(), 1);
        QCOMPARE(service.availableThemes().first(), QString("valid"));
    }

    void scanDeduplicatesById()
    {
        QTemporaryDir tmpDir1, tmpDir2;
        QVERIFY(tmpDir1.isValid());
        QVERIFY(tmpDir2.isValid());

        // Same ID "dupe" in both dirs, different names
        auto writeTheme = [](const QString& dir, const QString& id, const QString& name) {
            QFile f(QDir(dir).filePath("theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: " << id << "\nname: " << name << "\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
            f.close();
        };

        QDir(tmpDir1.path()).mkpath("mytheme");
        writeTheme(tmpDir1.filePath("mytheme"), "dupe", "First Wins");

        QDir(tmpDir2.path()).mkpath("mytheme");
        writeTheme(tmpDir2.filePath("mytheme"), "dupe", "Second Loses");

        oap::ThemeService service;
        // First search path wins
        service.scanThemeDirectories({tmpDir1.path(), tmpDir2.path()});

        QCOMPARE(service.availableThemes().size(), 1);
        // Verify the first-seen name is kept
        QCOMPARE(service.availableThemeNames().first(), QString("First Wins"));
    }

    void setThemeSwitchesColors()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Theme A with red background
        QDir(tmpDir.path()).mkpath("themeA");
        {
            QFile f(tmpDir.filePath("themeA/theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: themeA\nname: Theme A\nday:\n  background: \"#ff0000\"\nnight:\n  background: \"#aa0000\"\n";
            f.close();
        }

        // Theme B with blue background
        QDir(tmpDir.path()).mkpath("themeB");
        {
            QFile f(tmpDir.filePath("themeB/theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: themeB\nname: Theme B\nday:\n  background: \"#0000ff\"\nnight:\n  background: \"#0000aa\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        QVERIFY(service.setTheme("themeA"));
        QCOMPARE(service.backgroundColor(), QColor("#ff0000"));

        QSignalSpy colorsSpy(&service, &oap::ThemeService::colorsChanged);
        QVERIFY(service.setTheme("themeB"));
        QCOMPARE(service.backgroundColor(), QColor("#0000ff"));
        QVERIFY(colorsSpy.count() >= 1);
    }

    void setThemeInvalidReturnsFalse()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("real");
        {
            QFile f(tmpDir.filePath("real/theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: real\nname: Real\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        QVERIFY(service.setTheme("real"));

        // Now try invalid
        QVERIFY(!service.setTheme("nonexistent"));
        // Active theme should still be "real"
        QCOMPARE(service.currentThemeId(), QString("real"));
    }

    void wallpaperSourceResolvesFile()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("themed");
        {
            QFile f(tmpDir.filePath("themed/theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: themed\nname: Themed\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
            f.close();
        }
        // Create a wallpaper file
        {
            QFile f(tmpDir.filePath("themed/wallpaper.jpg"));
            f.open(QIODevice::WriteOnly);
            f.write("fake-jpg-data");
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        QVERIFY(service.setTheme("themed"));

        QString wp = service.wallpaperSource();
        QVERIFY(wp.startsWith("file://"));
        QVERIFY(wp.contains("wallpaper.jpg"));
    }

    void wallpaperSourceEmptyWhenNoImage()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("nowp");
        {
            QFile f(tmpDir.filePath("nowp/theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: nowp\nname: No WP\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        QVERIFY(service.setTheme("nowp"));

        QVERIFY(service.wallpaperSource().isEmpty());
    }

    void availableThemesChangedSignal()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("sig");
        {
            QFile f(tmpDir.filePath("sig/theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: sig\nname: Sig\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
            f.close();
        }

        oap::ThemeService service;
        QSignalSpy spy(&service, &oap::ThemeService::availableThemesChanged);
        service.scanThemeDirectories({tmpDir.path()});
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestThemeService)
#include "test_theme_service.moc"
