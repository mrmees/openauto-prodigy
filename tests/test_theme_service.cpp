#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QImage>
#include "core/services/ThemeService.hpp"
#include "core/services/IConfigService.hpp"

class MockConfigService : public oap::IConfigService {
public:
    QVariant value(const QString&) const override { return {}; }
    void setValue(const QString& key, const QVariant& val) override {
        lastKey = key; lastValue = val; setValueCalls++;
    }
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}
    void save() override { saveCalls++; }

    QString lastKey;
    QVariant lastValue;
    int setValueCalls = 0;
    int saveCalls = 0;
};

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
        QCOMPARE(service.background(), QColor("#1a1a2e"));
        QCOMPARE(service.primary(), QColor("#e94560"));
        QCOMPARE(service.onSurface(), QColor("#e0e0e0"));
        QCOMPARE(service.surfaceContainerLow(), QColor("#0f3460"));
    }

    void nightModeColors()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        service.setNightMode(true);
        QVERIFY(service.nightMode());
        QCOMPARE(service.background(), QColor("#0a0a14"));
        QCOMPARE(service.primary(), QColor("#c73650"));
        QCOMPARE(service.onSurface(), QColor("#b0b0b0"));
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
        QCOMPARE(service.color("primary"), QColor("#e94560"));
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
            << "  primary: \"#222222\"\n"
            << "night:\n"
            << "  background: \"#333333\"\n";
        f.close();

        oap::ThemeService service;
        QVERIFY(service.loadThemeFile(f.fileName()));

        service.setNightMode(true);
        // background is defined in night
        QCOMPARE(service.color("background"), QColor("#333333"));
        // primary falls back to day
        QCOMPARE(service.color("primary"), QColor("#222222"));
    }

    // --- New AA token accessor tests ---

    void allBaseTokensPresent()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        // All 16 base accessors should return non-transparent colors
        QVERIFY(service.primary() != QColor(Qt::transparent));
        QVERIFY(service.onSurface() != QColor(Qt::transparent));
        QVERIFY(service.surface() != QColor(Qt::transparent));
        QVERIFY(service.surfaceVariant() != QColor(Qt::transparent));
        QVERIFY(service.surfaceContainerLow() != QColor(Qt::transparent));
        QVERIFY(service.inverseSurface() != QColor(Qt::transparent));
        QVERIFY(service.inverseOnSurface() != QColor(Qt::transparent));
        QVERIFY(service.outline() != QColor(Qt::transparent));
        QVERIFY(service.outlineVariant() != QColor(Qt::transparent));
        QVERIFY(service.background() != QColor(Qt::transparent));
    }

    void derivedColors()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        // scrim is now stored in YAML as opaque black (alpha applied at usage sites)
        QCOMPARE(service.scrim(), QColor(0, 0, 0, 255));

        // success is a fixed green
        QCOMPARE(service.success(), QColor("#4CAF50"));

        // onSuccess is white
        QCOMPARE(service.onSuccess(), QColor("#FFFFFF"));
    }

    void dayNightWithNewAccessors()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        // Day
        QColor dayPrimary = service.primary();
        QColor daySurface = service.surface();

        // Switch to night
        service.setNightMode(true);
        QColor nightPrimary = service.primary();
        QColor nightSurface = service.surface();

        QVERIFY(dayPrimary != nightPrimary);
        QVERIFY(daySurface != nightSurface);
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
        QCOMPARE(service.background(), QColor("#ff0000"));

        QSignalSpy colorsSpy(&service, &oap::ThemeService::colorsChanged);
        QVERIFY(service.setTheme("themeB"));
        QCOMPARE(service.background(), QColor("#0000ff"));
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
    // --- Wallpaper enumeration tests ---

    void wallpaperListHasFixedEntries()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("mytheme");
        {
            QFile f(tmpDir.filePath("mytheme/theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: mytheme\nname: My Theme\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});

        // Should always have at least "Theme Default" and "None"
        QVERIFY(service.availableWallpaperNames().size() >= 2);
        QCOMPARE(service.availableWallpaperNames()[0], QString("Theme Default"));
        QCOMPARE(service.availableWallpapers()[0], QString());      // "" = theme default
        QCOMPARE(service.availableWallpaperNames()[1], QString("None"));
        QCOMPARE(service.availableWallpapers()[1], QString("none"));
    }

    void wallpaperListScansUserDir()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a fake user wallpapers dir and put an image in it
        // (buildWallpaperList scans ~/.openauto/wallpapers/ — we can't
        // easily redirect that in tests, so just verify the fixed entries work)
        QDir(tmpDir.path()).mkpath("nowp");
        {
            QFile f(tmpDir.filePath("nowp/theme.yaml"));
            f.open(QIODevice::WriteOnly);
            QTextStream out(&f);
            out << "id: nowp\nname: No Wallpaper\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});

        // At minimum "Theme Default" + "None" (custom files depend on user's homedir)
        QVERIFY(service.availableWallpapers().size() >= 2);
        QCOMPARE(service.availableWallpaperNames()[0], QString("Theme Default"));
        QCOMPARE(service.availableWallpaperNames()[1], QString("None"));
    }

    void setWallpaperOverrideCustom()
    {
        oap::ThemeService service;
        service.setWallpaperOverride("file:///some/path.jpg");
        QCOMPARE(service.wallpaperSource(), QString("file:///some/path.jpg"));
    }

    void setWallpaperOverrideNone()
    {
        // Start with a custom wallpaper, then set "none"
        oap::ThemeService service;
        service.setWallpaperOverride("file:///some/path.jpg");
        QCOMPARE(service.wallpaperSource(), QString("file:///some/path.jpg"));

        service.setWallpaperOverride("none");
        QVERIFY(service.wallpaperSource().isEmpty());
    }

    void setWallpaperOverrideEmitsSignal()
    {
        oap::ThemeService service;
        QSignalSpy spy(&service, &oap::ThemeService::wallpaperChanged);
        service.setWallpaperOverride("file:///test.jpg");
        QCOMPARE(spy.count(), 1);
    }

    void setWallpaperOverrideSameNoSignal()
    {
        oap::ThemeService service;
        service.setWallpaperOverride("file:///test.jpg");

        QSignalSpy spy(&service, &oap::ThemeService::wallpaperChanged);
        service.setWallpaperOverride("file:///test.jpg");  // same value
        QCOMPARE(spy.count(), 0);
    }

    void setThemeWithOverrideKeepsCustomWallpaper()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Two themes, each with a wallpaper
        for (const auto& id : {"themeA", "themeB"}) {
            QDir(tmpDir.path()).mkpath(id);
            {
                QFile f(tmpDir.filePath(QString(id) + "/theme.yaml"));
                f.open(QIODevice::WriteOnly);
                QTextStream out(&f);
                out << "id: " << id << "\nname: " << id << "\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
                f.close();
            }
            {
                QFile f(tmpDir.filePath(QString(id) + "/wallpaper.jpg"));
                f.open(QIODevice::WriteOnly);
                f.write("fake-jpg-data");
                f.close();
            }
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        service.setTheme("themeA");

        // Set a custom override
        service.setWallpaperOverride("file:///custom/bg.jpg");
        QCOMPARE(service.wallpaperSource(), QString("file:///custom/bg.jpg"));

        // Switch theme — custom wallpaper should persist
        service.setTheme("themeB");
        QCOMPARE(service.wallpaperSource(), QString("file:///custom/bg.jpg"));
    }

    // --- Connected Device theme + applyAATokens tests ---

    void applyAATokensUpdatesColors()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a connected-device theme in temp dir
        QDir(tmpDir.path()).mkpath("connected-device");
        {
            QFile f(tmpDir.filePath("connected-device/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: connected-device\n"
                << "name: Connected Device\n"
                << "version: 2.0.0\n"
                << "font_family: \"Lato\"\n"
                << "day:\n"
                << "  primary: \"#e94560\"\n"
                << "  background: \"#1a1a2e\"\n"
                << "  on-surface: \"#e0e0e0\"\n"
                << "  surface: \"#16213e\"\n"
                << "  surface-variant: \"#16213e\"\n"
                << "  surface-container-low: \"#0f3460\"\n"
                << "  inverse-surface: \"#1a1a2e\"\n"
                << "  inverse-on-surface: \"#ffffff\"\n"
                << "  outline: \"#808080\"\n"
                << "  outline-variant: \"#ffffff26\"\n"
                << "  text-primary: \"#ffffff\"\n"
                << "  text-secondary: \"#a0a0a0\"\n"
                << "  red: \"#cc4444\"\n"
                << "  on-red: \"#ffffff\"\n"
                << "  yellow: \"#FF9800\"\n"
                << "  on-yellow: \"#000000\"\n"
                << "night:\n"
                << "  primary: \"#c73650\"\n"
                << "  background: \"#0a0a14\"\n"
                << "  on-surface: \"#b0b0b0\"\n"
                << "  surface: \"#0e1729\"\n"
                << "  surface-variant: \"#0e1729\"\n"
                << "  surface-container-low: \"#0a2240\"\n"
                << "  inverse-surface: \"#0a0a14\"\n"
                << "  inverse-on-surface: \"#c0c0c0\"\n"
                << "  outline: \"#606060\"\n"
                << "  outline-variant: \"#ffffff1a\"\n"
                << "  text-primary: \"#c0c0c0\"\n"
                << "  text-secondary: \"#707070\"\n"
                << "  red: \"#aa3333\"\n"
                << "  on-red: \"#ffffff\"\n"
                << "  yellow: \"#cc7a00\"\n"
                << "  on-yellow: \"#000000\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        QVERIFY(service.setTheme("connected-device"));

        // Apply AA tokens with red primary as day token (ARGB format: 0xFFFF0000)
        QMap<QString, uint32_t> dayTokens;
        dayTokens["primary"] = 0xFFFF0000;  // opaque red
        QMap<QString, uint32_t> nightTokens;
        service.applyAATokens(dayTokens, nightTokens);

        // Day primary should now be red
        QCOMPARE(service.primary(), QColor(255, 0, 0));
    }

    void applyAATokensCachesWhenNotActive()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a non-connected-device theme
        QDir(tmpDir.path()).mkpath("other");
        {
            QFile f(tmpDir.filePath("other/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: other\nname: Other\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        // Create connected-device theme directory for caching
        QDir(tmpDir.path()).mkpath("connected-device");
        {
            QFile f(tmpDir.filePath("connected-device/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: connected-device\nname: Connected Device\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        QVERIFY(service.setTheme("other"));
        QColor originalPrimary = service.primary();

        QMap<QString, uint32_t> dayTokens;
        dayTokens["primary"] = 0xFFFF0000;  // opaque red
        QMap<QString, uint32_t> nightTokens;
        nightTokens["primary"] = 0xFF00FF00;  // opaque green
        service.applyAATokens(dayTokens, nightTokens);

        // Live colors should be unchanged (theme is "other", not "connected-device")
        QCOMPARE(service.primary(), originalPrimary);

        // But connected-device YAML should have been updated with the cached tokens
        QFile f(tmpDir.filePath("connected-device/theme.yaml"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QString content = f.readAll();
        f.close();

        // The file should contain the red day color and green night color
        QVERIFY(content.contains("ff0000") || content.contains("FF0000"));
        QVERIFY(content.contains("00ff00") || content.contains("00FF00"));
    }

    void applyAATokensEmitsSignal()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("connected-device");
        {
            QFile f(tmpDir.filePath("connected-device/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: connected-device\nname: Connected Device\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        QVERIFY(service.setTheme("connected-device"));

        QSignalSpy spy(&service, &oap::ThemeService::colorsChanged);
        QMap<QString, uint32_t> dayTokens;
        dayTokens["primary"] = 0xFFFF0000;
        QMap<QString, uint32_t> nightTokens;
        service.applyAATokens(dayTokens, nightTokens);

        QCOMPARE(spy.count(), 1);
    }

    void persistConnectedDeviceThemeWritesYaml()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("connected-device");
        {
            QFile f(tmpDir.filePath("connected-device/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: connected-device\nname: Connected Device\nversion: 2.0.0\n"
                << "font_family: \"Lato\"\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});
        QVERIFY(service.setTheme("connected-device"));

        // Apply tokens with separate day/night maps
        QMap<QString, uint32_t> dayTokens;
        dayTokens["primary"] = 0xFF00FF00;  // opaque green
        QMap<QString, uint32_t> nightTokens;
        service.applyAATokens(dayTokens, nightTokens);

        // Read the YAML file back and verify it was updated
        QFile f(tmpDir.filePath("connected-device/theme.yaml"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QString content = f.readAll();
        f.close();

        // The file should contain the new green color
        // QColor::fromRgba(0xFF00FF00) = #00ff00
        QVERIFY(content.contains("00ff00") || content.contains("00FF00"));
    }

    // --- M3 34-role expansion tests ---

    void newM3PropertiesReturnValidColors()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        // New M3 properties should return non-transparent colors from loaded theme
        QVERIFY(service.onPrimary() != QColor(Qt::transparent));
        QVERIFY(service.primaryContainer() != QColor(Qt::transparent));
        QVERIFY(service.onPrimaryContainer() != QColor(Qt::transparent));
        QVERIFY(service.secondary() != QColor(Qt::transparent));
        QVERIFY(service.onSecondary() != QColor(Qt::transparent));
        QVERIFY(service.secondaryContainer() != QColor(Qt::transparent));
        QVERIFY(service.onSecondaryContainer() != QColor(Qt::transparent));
        QVERIFY(service.tertiary() != QColor(Qt::transparent));
        QVERIFY(service.onTertiary() != QColor(Qt::transparent));
        QVERIFY(service.tertiaryContainer() != QColor(Qt::transparent));
        QVERIFY(service.onTertiaryContainer() != QColor(Qt::transparent));
        QVERIFY(service.error() != QColor(Qt::transparent));
        QVERIFY(service.onError() != QColor(Qt::transparent));
        QVERIFY(service.errorContainer() != QColor(Qt::transparent));
        QVERIFY(service.onErrorContainer() != QColor(Qt::transparent));
        QVERIFY(service.surfaceDim() != QColor(Qt::transparent));
        QVERIFY(service.surfaceBright() != QColor(Qt::transparent));
        QVERIFY(service.surfaceContainerLowest() != QColor(Qt::transparent));
        QVERIFY(service.surfaceContainerHigh() != QColor(Qt::transparent));
        QVERIFY(service.surfaceContainerHighest() != QColor(Qt::transparent));
        QVERIFY(service.inversePrimary() != QColor(Qt::transparent));
        QVERIFY(service.onBackground() != QColor(Qt::transparent));
        QVERIFY(service.onSurfaceVariant() != QColor(Qt::transparent));
        QVERIFY(service.shadow() != QColor(Qt::transparent));

        // Verify specific expected values
        QCOMPARE(service.onPrimary(), QColor("#ffffff"));
        QCOMPARE(service.secondary(), QColor("#6c757d"));
        QCOMPARE(service.error(), QColor("#cc4444"));
    }

    void deprecatedKeysReturnTransparent()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        // Deprecated custom token keys should return transparent since they
        // are no longer in theme YAMLs
        QCOMPARE(service.color("text-primary"), QColor(Qt::transparent));
        QCOMPARE(service.color("text-secondary"), QColor(Qt::transparent));
        QCOMPARE(service.color("red"), QColor(Qt::transparent));
        QCOMPARE(service.color("on-red"), QColor(Qt::transparent));
        QCOMPARE(service.color("yellow"), QColor(Qt::transparent));
        QCOMPARE(service.color("on-yellow"), QColor(Qt::transparent));
    }

    void scrimIsStoredNotHardcoded()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        // scrim should come from the YAML (opaque black), not hardcoded #000000b4
        QColor scrim = service.scrim();
        QCOMPARE(scrim, QColor("#000000"));
        // Alpha should be 255 (opaque) since YAML stores opaque black
        QCOMPARE(scrim.alpha(), 255);
    }

    void importCompanionThemeCreatesTheme()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a default theme for fallback
        QDir(tmpDir.path()).mkpath("default");
        {
            QFile f(tmpDir.filePath("default/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: default\nname: Default\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        // Create user themes dir
        QString userThemesDir = tmpDir.filePath("user-themes");
        QDir().mkpath(userThemesDir);

        oap::ThemeService service;
        service.scanThemeDirectories({userThemesDir, tmpDir.path()});
        service.setTheme("default");

        // Prepare companion theme data
        QMap<QString, QColor> dayColors, nightColors;
        dayColors["primary"] = QColor("#4285f4");
        dayColors["on-primary"] = QColor("#ffffff");
        dayColors["background"] = QColor("#1c1b1f");
        nightColors["primary"] = QColor("#a8c7fa");
        nightColors["on-primary"] = QColor("#062e6f");
        nightColors["background"] = QColor("#0e0e11");

        QByteArray wallpaper("fake-jpeg-data");

        bool result = service.importCompanionTheme(
            "My Cool Theme", "#4285f4", dayColors, nightColors, wallpaper);
        QVERIFY(result);

        // Theme dir should exist
        QString slug = "my-cool-theme";
        QString themeDir = userThemesDir + "/" + slug;
        QVERIFY(QDir(themeDir).exists());

        // theme.yaml should exist
        QVERIFY(QFile::exists(themeDir + "/theme.yaml"));

        // wallpaper.jpg should exist
        QVERIFY(QFile::exists(themeDir + "/wallpaper.jpg"));

        // Should have auto-switched to the new theme
        QCOMPARE(service.currentThemeId(), slug);

        // Colors should be from the companion theme
        QCOMPARE(service.primary(), QColor("#4285f4"));
    }

    void importCompanionThemeOverwrites()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("default");
        {
            QFile f(tmpDir.filePath("default/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: default\nname: Default\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        QString userThemesDir = tmpDir.filePath("user-themes");
        QDir().mkpath(userThemesDir);

        oap::ThemeService service;
        service.scanThemeDirectories({userThemesDir, tmpDir.path()});
        service.setTheme("default");

        QMap<QString, QColor> dayColors1, nightColors1;
        dayColors1["primary"] = QColor("#ff0000");
        nightColors1["primary"] = QColor("#aa0000");

        service.importCompanionTheme("Test Theme", "#ff0000", dayColors1, nightColors1, {});

        // Now import again with different colors (same name = overwrite)
        QMap<QString, QColor> dayColors2, nightColors2;
        dayColors2["primary"] = QColor("#00ff00");
        nightColors2["primary"] = QColor("#00aa00");

        bool result = service.importCompanionTheme("Test Theme", "#00ff00", dayColors2, nightColors2, {});
        QVERIFY(result);

        // Should have the new color
        QCOMPARE(service.primary(), QColor("#00ff00"));
    }

    void deleteThemeRemovesUserTheme()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create default in bundled dir
        QDir(tmpDir.path()).mkpath("bundled/default");
        {
            QFile f(tmpDir.filePath("bundled/default/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: default\nname: Default\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        // Create user theme dir with a theme
        QString userThemesDir = tmpDir.filePath("user-themes");
        QDir().mkpath(userThemesDir + "/my-theme");
        {
            QFile f(userThemesDir + "/my-theme/theme.yaml");
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: my-theme\nname: My Theme\nversion: 2.0.0\nsource: companion\n"
                << "day:\n  primary: \"#4285f4\"\n  background: \"#1c1b1f\"\n"
                << "night:\n  primary: \"#a8c7fa\"\n  background: \"#0e0e11\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({userThemesDir, tmpDir.filePath("bundled")});
        QVERIFY(service.setTheme("default"));

        // Delete user theme should succeed
        bool result = service.deleteTheme("my-theme");
        QVERIFY(result);

        // Dir should be gone
        QVERIFY(!QDir(userThemesDir + "/my-theme").exists());
    }

    void deleteThemeRejectsBundledTheme()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // User themes dir (first search path, where deletes are allowed)
        QString userThemesDir = tmpDir.filePath("user-themes");
        QDir().mkpath(userThemesDir);

        // Bundled dir (second search path, protected from deletion)
        QDir(tmpDir.path()).mkpath("bundled/default");
        {
            QFile f(tmpDir.filePath("bundled/default/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: default\nname: Default\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({userThemesDir, tmpDir.filePath("bundled")});
        QVERIFY(service.setTheme("default"));

        // Delete bundled theme should fail (it's not under user-themes)
        bool result = service.deleteTheme("default");
        QVERIFY(!result);

        // Theme should still be available
        QVERIFY(service.availableThemes().contains("default"));
    }

    void deleteActiveThemeFallsBackToDefault()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create default in bundled dir
        QDir(tmpDir.path()).mkpath("bundled/default");
        {
            QFile f(tmpDir.filePath("bundled/default/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: default\nname: Default\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        // Create user theme
        QString userThemesDir = tmpDir.filePath("user-themes");
        QDir().mkpath(userThemesDir + "/active-theme");
        {
            QFile f(userThemesDir + "/active-theme/theme.yaml");
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: active-theme\nname: Active Theme\nversion: 2.0.0\nsource: companion\n"
                << "day:\n  primary: \"#4285f4\"\n  background: \"#1c1b1f\"\n"
                << "night:\n  primary: \"#a8c7fa\"\n  background: \"#0e0e11\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({userThemesDir, tmpDir.filePath("bundled")});
        QVERIFY(service.setTheme("active-theme"));
        QCOMPARE(service.currentThemeId(), QString("active-theme"));

        // Delete the active theme
        bool result = service.deleteTheme("active-theme");
        QVERIFY(result);

        // Should have fallen back to default
        QCOMPARE(service.currentThemeId(), QString("default"));
    }

    void companionThemesShowInPicker()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a companion theme
        QDir(tmpDir.path()).mkpath("my-palette");
        {
            QFile f(tmpDir.filePath("my-palette/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: my-palette\nname: My Palette\nversion: 2.0.0\nsource: companion\n"
                << "day:\n  primary: \"#4285f4\"\n  background: \"#1c1b1f\"\n"
                << "night:\n  primary: \"#a8c7fa\"\n  background: \"#0e0e11\"\n";
            f.close();
        }

        // Create connected-device theme (should be hidden)
        QDir(tmpDir.path()).mkpath("connected-device");
        {
            QFile f(tmpDir.filePath("connected-device/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: connected-device\nname: Connected Device\nversion: 2.0.0\n"
                << "day:\n  primary: \"#80cbc4\"\n  background: \"#1c1b1f\"\n"
                << "night:\n  primary: \"#4db6ac\"\n  background: \"#121215\"\n";
            f.close();
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});

        // Companion theme should be in picker
        QVERIFY(service.availableThemes().contains("my-palette"));
        // Connected-device should NOT be in picker
        QVERIFY(!service.availableThemes().contains("connected-device"));
    }

    void forceDarkModeOverridesNightMode()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        QVERIFY(!service.nightMode());
        service.setForceDarkMode(true);
        QVERIFY(service.nightMode());

        // Even setting night mode off doesn't override force dark
        service.setNightMode(false);
        QVERIFY(service.nightMode());

        // Disabling force dark restores real state
        service.setForceDarkMode(false);
        QVERIFY(!service.nightMode());
    }

    void forceDarkModeRealNightState()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        service.setForceDarkMode(true);
        QVERIFY(service.nightMode()); // QML sees dark

        // Real state is still day
        QVERIFY(!service.realNightMode());

        // Set real night via setNightMode
        service.setNightMode(true);
        QVERIFY(service.realNightMode());

        // QML still sees dark (unchanged)
        QVERIFY(service.nightMode());
    }

    void forceDarkModeSignals()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        QSignalSpy forceSpy(&service, &oap::ThemeService::forceDarkModeChanged);
        QSignalSpy modeSpy(&service, &oap::ThemeService::modeChanged);
        QSignalSpy colorsSpy(&service, &oap::ThemeService::colorsChanged);

        service.setForceDarkMode(true);
        QCOMPARE(forceSpy.count(), 1);
        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(colorsSpy.count(), 1);

        // Setting same value again should not emit
        service.setForceDarkMode(true);
        QCOMPARE(forceSpy.count(), 1);
    }

    void forceDarkModeToggle()
    {
        oap::ThemeService service;
        service.loadThemeFile(QFINDTESTDATA("data/themes/default/theme.yaml"));

        service.setForceDarkMode(true);
        QVERIFY(service.nightMode());

        // toggleMode disables force dark for the session
        service.toggleMode();
        QVERIFY(!service.forceDarkMode());
        QVERIFY(!service.nightMode());

        // toggleMode now acts normally
        service.toggleMode();
        QVERIFY(service.nightMode());
    }

    void setThemeDefaultWallpaperChangesWithTheme()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Two themes, each with a wallpaper
        for (const auto& id : {"themeA", "themeB"}) {
            QDir(tmpDir.path()).mkpath(id);
            {
                QFile f(tmpDir.filePath(QString(id) + "/theme.yaml"));
                f.open(QIODevice::WriteOnly);
                QTextStream out(&f);
                out << "id: " << id << "\nname: " << id << "\nday:\n  background: \"#111111\"\nnight:\n  background: \"#222222\"\n";
                f.close();
            }
            {
                QFile f(tmpDir.filePath(QString(id) + "/wallpaper.jpg"));
                f.open(QIODevice::WriteOnly);
                f.write("fake-jpg-data");
                f.close();
            }
        }

        oap::ThemeService service;
        service.scanThemeDirectories({tmpDir.path()});

        // No override — theme default
        service.setTheme("themeA");
        QString wpA = service.wallpaperSource();
        QVERIFY(wpA.contains("themeA"));

        service.setTheme("themeB");
        QString wpB = service.wallpaperSource();
        QVERIFY(wpB.contains("themeB"));

        // Wallpapers should be different (different theme dirs)
        QVERIFY(wpA != wpB);
    }

    // --- Config persistence tests ---

    void setThemePersistsToConfig()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("test-theme");
        {
            QFile f(tmpDir.filePath("test-theme/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: test-theme\nname: Test Theme\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        MockConfigService mockConfig;
        oap::ThemeService service;
        service.setConfigService(&mockConfig);
        service.scanThemeDirectories({tmpDir.path()});

        QVERIFY(service.setTheme("test-theme"));
        QCOMPARE(mockConfig.lastKey, QString("display.theme"));
        QCOMPARE(mockConfig.lastValue.toString(), QString("test-theme"));
        QCOMPARE(mockConfig.setValueCalls, 1);
        QCOMPARE(mockConfig.saveCalls, 1);
    }

    void setThemeWithoutConfigService()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("test-theme");
        {
            QFile f(tmpDir.filePath("test-theme/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: test-theme\nname: Test Theme\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        oap::ThemeService service;
        // No setConfigService call — configService_ is nullptr
        service.scanThemeDirectories({tmpDir.path()});

        // Should work without crash
        QVERIFY(service.setTheme("test-theme"));
        QCOMPARE(service.currentThemeId(), QString("test-theme"));
    }

    void setThemeInvalidDoesNotPersist()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("valid-theme");
        {
            QFile f(tmpDir.filePath("valid-theme/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: valid-theme\nname: Valid Theme\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        MockConfigService mockConfig;
        oap::ThemeService service;
        service.setConfigService(&mockConfig);
        service.scanThemeDirectories({tmpDir.path()});

        // Try setting a nonexistent theme
        QVERIFY(!service.setTheme("nonexistent"));
        QCOMPARE(mockConfig.setValueCalls, 0);
        QCOMPARE(mockConfig.saveCalls, 0);
    }
    void setWallpaperOverridePersistsToConfig()
    {
        oap::ThemeService service;
        MockConfigService mockConfig;
        service.setConfigService(&mockConfig);

        // Set a wallpaper override
        service.setWallpaperOverride("file:///tmp/test.jpg");
        QCOMPARE(mockConfig.lastKey, QString("display.wallpaper_override"));
        QCOMPARE(mockConfig.lastValue.toString(), QString("file:///tmp/test.jpg"));
        QCOMPARE(mockConfig.setValueCalls, 1);
        QCOMPARE(mockConfig.saveCalls, 1);

        // Clear it
        service.setWallpaperOverride("");
        QCOMPARE(mockConfig.lastKey, QString("display.wallpaper_override"));
        QCOMPARE(mockConfig.lastValue.toString(), QString(""));
        QCOMPARE(mockConfig.setValueCalls, 2);
        QCOMPARE(mockConfig.saveCalls, 2);
    }

    void importCompanionThemeClearsWallpaperOverrideInConfig()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        QDir(tmpDir.path()).mkpath("default");
        {
            QFile f(tmpDir.filePath("default/theme.yaml"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            QTextStream out(&f);
            out << "id: default\nname: Default\nversion: 2.0.0\n"
                << "day:\n  primary: \"#e94560\"\n  background: \"#1a1a2e\"\n"
                << "night:\n  primary: \"#c73650\"\n  background: \"#0a0a14\"\n";
            f.close();
        }

        QString userThemesDir = tmpDir.filePath("user-themes");
        QDir().mkpath(userThemesDir);

        MockConfigService mockConfig;
        oap::ThemeService service;
        service.setConfigService(&mockConfig);
        service.scanThemeDirectories({userThemesDir, tmpDir.path()});
        service.setTheme("default");

        // Set a wallpaper override first
        service.setWallpaperOverride("file:///tmp/old-wallpaper.jpg");
        mockConfig.setValueCalls = 0;
        mockConfig.saveCalls = 0;

        // Import companion theme — should clear wallpaper override
        QMap<QString, QColor> dayColors, nightColors;
        dayColors["primary"] = QColor("#4285f4");
        nightColors["primary"] = QColor("#a8c7fa");

        service.importCompanionTheme("Test", "#4285f4", dayColors, nightColors, {});

        // importCompanionTheme calls setTheme(slug) then setWallpaperOverride("")
        // Both persist to config. The last setValue call should be the wallpaper clear.
        QCOMPARE(mockConfig.lastKey, QString("display.wallpaper_override"));
        QCOMPARE(mockConfig.lastValue.toString(), QString(""));
        QVERIFY(mockConfig.setValueCalls >= 2); // at least theme + wallpaper
    }
};

QTEST_MAIN(TestThemeService)
#include "test_theme_service.moc"
