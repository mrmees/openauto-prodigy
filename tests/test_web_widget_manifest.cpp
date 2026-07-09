#include <QtTest>
#include <QTemporaryDir>
#include "core/widget/WebWidgetManifest.hpp"

class TestWebWidgetManifest : public QObject {
    Q_OBJECT

    QString writeManifest(QTemporaryDir& dir, const QByteArray& yaml) {
        const QString path = dir.filePath("widget.yaml");
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(yaml);
        f.close();
        return path;
    }

private slots:
    void testFullManifestParses() {
        QTemporaryDir dir;
        const auto path = writeManifest(dir,
            "id: com.example.speedo\n"
            "name: \"Speedometer\"\n"
            "entry: main.html\n"
            "category: navigation\n"
            "description: \"GPS speedometer\"\n"
            "icon: \"\\ue9e4\"\n"
            "size:\n"
            "  minCols: 2\n  minRows: 1\n  maxCols: 4\n  maxRows: 3\n"
            "  defaultCols: 2\n  defaultRows: 2\n");
        const auto m = oap::WebWidgetManifest::fromFile(path);
        QVERIFY(m.isValid());
        QCOMPARE(m.id, QStringLiteral("com.example.speedo"));
        QCOMPARE(m.name, QStringLiteral("Speedometer"));
        QCOMPARE(m.entry, QStringLiteral("main.html"));
        QCOMPARE(m.category, QStringLiteral("navigation"));
        QCOMPARE(m.minCols, 2); QCOMPARE(m.maxRows, 3);
        QCOMPARE(m.defaultCols, 2); QCOMPARE(m.defaultRows, 2);
        QCOMPARE(m.dirPath, dir.path());
    }
    void testDefaultsApplied() {
        QTemporaryDir dir;
        const auto m = oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: a.b\nname: Minimal\n"));
        QVERIFY(m.isValid());
        QCOMPARE(m.entry, QStringLiteral("index.html"));
        QCOMPARE(m.category, QStringLiteral("status"));
        QCOMPARE(m.minCols, 1); QCOMPARE(m.maxCols, 6);
        QCOMPARE(m.maxRows, 4); QCOMPARE(m.defaultCols, 1);
    }
    void testMissingIdInvalid() {
        QTemporaryDir dir;
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "name: NoId\n")).isValid());
    }
    void testUnsafeIdInvalid() {
        QTemporaryDir dir;
        // id becomes a prodigy:// URL segment — path chars are forbidden
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: \"../escape\"\nname: Evil\n")).isValid());
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: \"a/b\"\nname: Evil\n")).isValid());
    }
    void testUnsafeEntryInvalid() {
        QTemporaryDir dir;
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: a.b\nname: X\nentry: \"../../etc/passwd\"\n")).isValid());
    }
    void testMalformedYamlInvalid() {
        QTemporaryDir dir;
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: [unclosed\n\t: {{\n")).isValid());
    }
    void testMissingFileInvalid() {
        QVERIFY(!oap::WebWidgetManifest::fromFile(
            QStringLiteral("/nonexistent/widget.yaml")).isValid());
    }
    void testScalarSizeFallsBackToDefaults() {
        // A malformed `size:` node (scalar, not a map) must not discard the
        // whole manifest -- it should be treated as absent, same as any
        // other malformed field.
        QTemporaryDir dir;
        const auto m = oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: a.b\nname: X\nsize: 5\n"));
        QVERIFY(m.isValid());
        QCOMPARE(m.id, QStringLiteral("a.b"));
        QCOMPARE(m.name, QStringLiteral("X"));
        QCOMPARE(m.minCols, 1); QCOMPARE(m.minRows, 1);
        QCOMPARE(m.maxCols, 6); QCOMPARE(m.maxRows, 4);
        QCOMPARE(m.defaultCols, 1); QCOMPARE(m.defaultRows, 1);
    }
    void testIdWithTrailingNewlineInvalid() {
        // Double-quoted YAML scalar embeds a literal trailing newline in
        // the id. QRegularExpression's `$` matches before a trailing line
        // terminator, so this must NOT be accepted -- id becomes a
        // prodigy:// URL path segment and resolver key.
        QTemporaryDir dir;
        const auto m = oap::WebWidgetManifest::fromFile(
            writeManifest(dir, "id: \"abc\\n\"\nname: X\n"));
        QCOMPARE(m.id, QStringLiteral("abc\n"));
        QVERIFY(!m.isValid());
    }
};
QTEST_GUILESS_MAIN(TestWebWidgetManifest)
#include "test_web_widget_manifest.moc"
