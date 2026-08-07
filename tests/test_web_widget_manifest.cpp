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
        QVERIFY(m.configSchema.isEmpty());
        QVERIFY(!m.configureOnAdd);
    }
    void testConfigurationFieldsParse() {
        QTemporaryDir dir;
        const auto m = oap::WebWidgetManifest::fromFile(writeManifest(dir,
            "id: a.b\n"
            "name: Configurable\n"
            "configuration:\n"
            "  configureOnAdd: true\n"
            "  fields:\n"
            "    - key: style\n"
            "      label: Style\n"
            "      type: enum\n"
            "      options:\n"
            "        - { label: Compact, value: compact }\n"
            "        - { label: Count, value: 2 }\n"
            "        - { label: Enabled, value: true }\n"
            "        - { label: Literal True, value: \"true\" }\n"
            "    - key: showLabel\n"
            "      label: Show Label\n"
            "      type: bool\n"
            "    - key: brightness\n"
            "      label: Brightness\n"
            "      type: intRange\n"
            "      min: 10\n"
            "      max: 90\n"
            "      step: 5\n"
            "    - key: profileId\n"
            "      label: Profile\n"
            "      type: collection\n"
            "      collection: profiles\n"
            "      required: true\n"));

        QVERIFY(m.isValid());
        QVERIFY(m.configureOnAdd);
        QCOMPARE(m.configSchema.size(), 4);

        const auto& enumField = m.configSchema.at(0);
        QCOMPARE(enumField.type, oap::ConfigFieldType::Enum);
        QCOMPARE(enumField.options,
                 QStringList({QStringLiteral("Compact"), QStringLiteral("Count"),
                              QStringLiteral("Enabled"), QStringLiteral("Literal True")}));
        QCOMPARE(enumField.values.at(0).toString(), QStringLiteral("compact"));
        QCOMPARE(enumField.values.at(1).toLongLong(), 2);
        QCOMPARE(enumField.values.at(2).toBool(), true);
        QCOMPARE(enumField.values.at(3).typeId(), QMetaType::QString);
        QCOMPARE(enumField.values.at(3).toString(), QStringLiteral("true"));

        QCOMPARE(m.configSchema.at(1).type, oap::ConfigFieldType::Bool);
        const auto& rangeField = m.configSchema.at(2);
        QCOMPARE(rangeField.type, oap::ConfigFieldType::IntRange);
        QCOMPARE(rangeField.rangeMin, 10);
        QCOMPARE(rangeField.rangeMax, 90);
        QCOMPARE(rangeField.rangeStep, 5);

        const auto& collectionField = m.configSchema.at(3);
        QCOMPARE(collectionField.type, oap::ConfigFieldType::Collection);
        QCOMPARE(collectionField.collection, QStringLiteral("profiles"));
        QVERIFY(collectionField.required);
    }
    void testInvalidConfigurationFieldsAreDroppedIndividually() {
        QTemporaryDir dir;
        const auto m = oap::WebWidgetManifest::fromFile(writeManifest(dir,
            "id: a.b\n"
            "name: Partial\n"
            "configuration:\n"
            "  configureOnAdd: true\n"
            "  fields:\n"
            "    - { key: good, label: Good, type: bool }\n"
            "    - { key: mystery, label: Mystery, type: unknown }\n"
            "    - { key: 'bad/key', label: Unsafe, type: bool }\n"
            "    - { key: item, label: Item, type: collection, collection: '../bad' }\n"
            "    - { key: good, label: Duplicate, type: collection, collection: profiles }\n"
            "    - key: duplicateEnum\n"
            "      label: Duplicate Enum\n"
            "      type: enum\n"
            "      options:\n"
            "        - { label: First, value: same }\n"
            "        - { label: Second, value: same }\n"
            "    - { key: range, label: Range, type: intRange, min: 10, max: 5, step: 1 }\n"
            "    - 42\n"));

        QVERIFY(m.isValid());
        QVERIFY(m.configureOnAdd);
        QCOMPARE(m.configSchema.size(), 1);
        QCOMPARE(m.configSchema.first().key, QStringLiteral("good"));
        QCOMPARE(m.configSchema.first().type, oap::ConfigFieldType::Bool);
    }
    void testNoValidFieldsDisablesConfigureOnAdd() {
        QTemporaryDir dir;
        const auto m = oap::WebWidgetManifest::fromFile(writeManifest(dir,
            "id: a.b\n"
            "name: Still Visible\n"
            "configuration:\n"
            "  configureOnAdd: true\n"
            "  fields:\n"
            "    - { key: '', label: Missing Key, type: bool }\n"));
        QVERIFY(m.isValid());
        QVERIFY(m.configSchema.isEmpty());
        QVERIFY(!m.configureOnAdd);
    }
    void testMalformedConfigurationBlockIsIgnored() {
        QTemporaryDir dir;
        const auto m = oap::WebWidgetManifest::fromFile(writeManifest(dir,
            "id: a.b\nname: Still Visible\nconfiguration: invalid\n"));
        QVERIFY(m.isValid());
        QVERIFY(m.configSchema.isEmpty());
        QVERIFY(!m.configureOnAdd);
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
