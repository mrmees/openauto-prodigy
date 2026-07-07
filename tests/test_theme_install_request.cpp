#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "core/services/ThemeInstallRequest.hpp"
#include "core/services/ThemeService.hpp"

using namespace oap;

// A minimal valid light/dark scheme for tests.
static QVariantMap validScheme() { return QVariantMap{{"primary", "#112233"}, {"onPrimary", "#ffffff"}}; }

class TestThemeInstallRequest : public QObject {
    Q_OBJECT

    QString writeJpeg(const QString& dir, const QByteArray& bytes) {
        const QString p = dir + "/wp.jpg";
        QFile f(p); f.open(QIODevice::WriteOnly); f.write(bytes); f.close();
        return p;
    }
    static QByteArray jpeg(int extra = 16) { return QByteArray("\xff\xd8\xff", 3) + QByteArray(extra, '\0'); }

private slots:
    void happyColorOnly() {
        QVariantMap d{{"name", "Sunset Vibes"}, {"seed", "#ff8a65"},
                      {"light", validScheme()}, {"dark", validScheme()}};
        auto r = parseThemeInstall(d, "/tmp/oap-theme-upload");
        QVERIFY(r.ok);
        QCOMPARE(r.request.name, QString("Sunset Vibes"));
        QVERIFY(r.request.wallpaperJpeg.isEmpty());
    }

    void camelToHyphenKeys() {
        QVariantMap d{{"name", "X"},
                      {"light", QVariantMap{{"onPrimary", "#010101"}, {"surfaceContainerHigh", "#020202"}}},
                      {"dark", validScheme()}};
        auto r = parseThemeInstall(d, "/tmp/oap-theme-upload");
        QVERIFY(r.ok);
        QVERIFY(r.request.dayColors.contains("on-primary"));
        QVERIFY(r.request.dayColors.contains("surface-container-high"));
        QVERIFY(!r.request.dayColors.contains("onPrimary"));
    }

    void rejectsEmptyName() {
        QVariantMap d{{"name", "   "}, {"light", validScheme()}, {"dark", validScheme()}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void rejectsLongName() {
        QVariantMap d{{"name", QString(65, 'a')}, {"light", validScheme()}, {"dark", validScheme()}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void rejectsEmptyLight() {
        QVariantMap d{{"name", "X"}, {"light", QVariantMap{}}, {"dark", validScheme()}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void rejectsEmptyDark() {
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", QVariantMap{}}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void rejectsInvalidColor() {
        QVariantMap d{{"name", "X"}, {"light", QVariantMap{{"primary", "not-a-color"}}}, {"dark", validScheme()}};
        QVERIFY(!parseThemeInstall(d, "/tmp/oap-theme-upload").ok);
    }

    void wallpaperHappy() {
        QTemporaryDir dir;
        const QString p = writeJpeg(dir.path(), jpeg());
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", p}};
        auto r = parseThemeInstall(d, dir.path());
        QVERIFY(r.ok);
        QCOMPARE(r.request.wallpaperJpeg.size(), jpeg().size());
    }

    void wallpaperTooLarge() {
        QTemporaryDir dir;
        const QString p = writeJpeg(dir.path(), jpeg(64));
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", p}};
        auto r = parseThemeInstall(d, dir.path(), /*maxWallpaperBytes*/ 8);  // 3+64 > 8
        QVERIFY(!r.ok);
    }

    void wallpaperBadMagic() {
        QTemporaryDir dir;
        const QString p = writeJpeg(dir.path(), QByteArray("\x89PNG", 4));
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", p}};
        QVERIFY(!parseThemeInstall(d, dir.path()).ok);
    }

    void wallpaperPathOutsideAllowedDir() {
        QTemporaryDir allowed, elsewhere;
        const QString p = writeJpeg(elsewhere.path(), jpeg());
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", p}};
        QVERIFY(!parseThemeInstall(d, allowed.path()).ok);   // path is under a different dir
    }

    void wallpaperPathIsDirectory() {
        QTemporaryDir dir;
        QVariantMap d{{"name", "X"}, {"light", validScheme()}, {"dark", validScheme()}, {"wallpaper_path", dir.path()}};
        QVERIFY(!parseThemeInstall(d, dir.path()).ok);       // not a regular file
    }

    // --- slugify (extracted from importCompanionTheme; behavior-preserving) ---
    void slugifyBasic()      { QCOMPARE(ThemeService::slugify("Sunset Vibes"), QString("sunset-vibes")); }
    void slugifyPunctuation(){ QCOMPARE(ThemeService::slugify("  Hello!! World  "), QString("hello-world")); }
    void slugifyEmpty()      { QCOMPARE(ThemeService::slugify(""), QString("companion-theme")); }
    void slugifyAllPunct()   { QCOMPARE(ThemeService::slugify("---"), QString("companion-theme")); }

    // Refactor guard: importCompanionTheme's created dir must match slugify(name).
    void importUsesSlugify() {
        QTemporaryDir themes;
        ThemeService svc;
        svc.scanThemeDirectories({themes.path()});
        QMap<QString, QColor> day{{"primary", QColor("#112233")}};
        QMap<QString, QColor> night{{"primary", QColor("#445566")}};
        QVERIFY(svc.importCompanionTheme("Sunset Vibes", "#ff8a65", day, night, QByteArray()));
        QVERIFY(QFile::exists(themes.path() + "/sunset-vibes/theme.yaml"));
    }
};

QTEST_MAIN(TestThemeInstallRequest)
#include "test_theme_install_request.moc"
