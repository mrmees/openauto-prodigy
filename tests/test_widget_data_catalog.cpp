#include <QtTest>
#include <QTemporaryDir>

#include "core/widget/WidgetDataCatalog.hpp"

class TestWidgetDataCatalog : public QObject {
    Q_OBJECT

    static void writeFile(const QString& path, const QByteArray& content)
    {
        QFileInfo info(path);
        QDir().mkpath(info.absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(content), content.size());
    }

private slots:
    void testDiscoversAndSortsDirectItems()
    {
        QTemporaryDir dir;
        const QString base = dir.filePath("com.test.widget/profiles");
        writeFile(base + "/zulu/item.yaml",
                  "id: zulu\nname: Zulu\ndescription: Last item\nunknown: ignored\n");
        writeFile(base + "/gamma/item.yaml", "id: gamma\nname: alpha\n");
        writeFile(base + "/beta/item.yaml", "id: beta\nname: Alpha\n");

        oap::WidgetDataCatalog catalog(dir.path());
        const auto items = catalog.items("com.test.widget", "profiles");
        QCOMPARE(items.size(), 3);
        QCOMPARE(items.at(0).id, QStringLiteral("beta"));
        QCOMPARE(items.at(1).id, QStringLiteral("gamma"));
        QCOMPARE(items.at(2).id, QStringLiteral("zulu"));
        QCOMPARE(items.at(2).description, QStringLiteral("Last item"));
    }

    void testBadItemsDoNotHideValidSibling()
    {
        QTemporaryDir dir;
        const QString base = dir.filePath("com.test.widget/profiles");
        writeFile(base + "/valid/item.yaml", "id: valid\nname: Valid\n");
        writeFile(base + "/mismatch/item.yaml", "id: other\nname: Wrong\n");
        writeFile(base + "/empty-name/item.yaml", "id: empty-name\nname: '   '\n");
        writeFile(base + "/malformed/item.yaml", "id: [unterminated\n");
        writeFile(base + "/bad item/item.yaml", "id: bad item\nname: Unsafe\n");
        QDir().mkpath(base + "/missing-metadata");
        writeFile(base + "/group/nested/item.yaml", "id: nested\nname: Nested\n");

        oap::WidgetDataCatalog catalog(dir.path());
        const auto items = catalog.items("com.test.widget", "profiles");
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().id, QStringLiteral("valid"));
    }

    void testRejectsSymlinkedItemsAndMetadata()
    {
        QTemporaryDir dir;
        const QString base = dir.filePath("data/com.test.widget/profiles");
        const QString outside = dir.filePath("outside");
        writeFile(outside + "/item.yaml", "id: linked-dir\nname: Linked Dir\n");
        QDir().mkpath(base);
        QVERIFY(QFile::link(outside, base + "/linked-dir"));

        QDir().mkpath(base + "/linked-file");
        writeFile(dir.filePath("outside-item.yaml"),
                  "id: linked-file\nname: Linked File\n");
        QVERIFY(QFile::link(dir.filePath("outside-item.yaml"),
                            base + "/linked-file/item.yaml"));

        writeFile(base + "/regular/item.yaml", "id: regular\nname: Regular\n");
        oap::WidgetDataCatalog catalog(dir.filePath("data"));
        const auto items = catalog.items("com.test.widget", "profiles");
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().id, QStringLiteral("regular"));
    }

    void testMissingAndUnsafeRequestsReturnEmpty()
    {
        QTemporaryDir dir;
        oap::WidgetDataCatalog catalog(dir.path());
        QVERIFY(catalog.items("missing", "profiles").isEmpty());
        QVERIFY(catalog.items("../escape", "profiles").isEmpty());
        QVERIFY(catalog.items("com.test.widget", "../escape").isEmpty());
        QVERIFY(catalog.items("", "profiles").isEmpty());
    }

    void testRootCreatedAfterConstructionIsScanned()
    {
        QTemporaryDir dir;
        const QString root = dir.filePath("later");
        oap::WidgetDataCatalog catalog(root);
        QVERIFY(catalog.items("com.test.widget", "profiles").isEmpty());

        writeFile(root + "/com.test.widget/profiles/new/item.yaml",
                  "id: new\nname: New\n");
        const auto items = catalog.items("com.test.widget", "profiles");
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().id, QStringLiteral("new"));
    }
};

QTEST_GUILESS_MAIN(TestWidgetDataCatalog)
#include "test_widget_data_catalog.moc"
