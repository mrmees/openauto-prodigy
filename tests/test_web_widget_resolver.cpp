#include <QtTest>
#include <QTemporaryDir>
#include "core/webwidget/WebWidgetContentResolver.hpp"

class TestWebWidgetResolver : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
    oap::WebWidgetContentResolver resolver_;

    void touch(const QString& rel, const QByteArray& content = "x") {
        QFileInfo fi(dir_.filePath(rel));
        QDir().mkpath(fi.absolutePath());
        QFile f(fi.absoluteFilePath());
        f.open(QIODevice::WriteOnly);
        f.write(content);
    }

private slots:
    void initTestCase() {
        touch("pkg/index.html", "<html/>");
        touch("pkg/assets/app.js");
        touch("pkg/__data__/shadow.json", "package shadow");
        touch("outside.txt", "secret");
        touch("data/com.test.pkg/profiles/sample/gauge.json", "{\"ok\":true}");
        touch("data/com.test.pkg/profiles/sample/internal.txt", "inside");
        touch("data/com.test.other/secret.txt", "other");
        resolver_.registerPackage("com.test.pkg", dir_.filePath("pkg"));
        resolver_.setDataRoot(dir_.filePath("data"));
    }
    void testResolvesEntryFile() {
        const auto p = resolver_.resolve("com.test.pkg", "index.html");
        QVERIFY(!p.isEmpty());
        QVERIFY(p.endsWith(QStringLiteral("pkg/index.html")));
    }
    void testResolvesNestedAsset() {
        QVERIFY(!resolver_.resolve("com.test.pkg", "assets/app.js").isEmpty());
    }
    void testUnknownIdEmpty() {
        QVERIFY(resolver_.resolve("nope", "index.html").isEmpty());
    }
    void testMissingFileEmpty() {
        QVERIFY(resolver_.resolve("com.test.pkg", "missing.html").isEmpty());
    }
    void testTraversalBlocked() {
        QVERIFY(resolver_.resolve("com.test.pkg", "../outside.txt").isEmpty());
        QVERIFY(resolver_.resolve("com.test.pkg", "assets/../../outside.txt").isEmpty());
    }
    void testSymlinkEscapeBlocked() {
        QFile::link(dir_.filePath("outside.txt"), dir_.filePath("pkg/sneaky.txt"));
        QVERIFY(resolver_.resolve("com.test.pkg", "sneaky.txt").isEmpty());
    }
    void testResolvesOwnedWidgetData() {
        const auto path = resolver_.resolve(
            "com.test.pkg", "__data__/profiles/sample/gauge.json");
        QVERIFY(!path.isEmpty());
        QVERIFY(path.endsWith(QStringLiteral(
            "data/com.test.pkg/profiles/sample/gauge.json")));
    }
    void testDataRouteRequiresRegisteredPackage() {
        QVERIFY(resolver_.resolve(
            "com.test.other", "__data__/secret.txt").isEmpty());
    }
    void testDataRouteRejectsMissingEmptyAbsoluteAndTraversal() {
        QVERIFY(resolver_.resolve("com.test.pkg", "__data__/").isEmpty());
        QVERIFY(resolver_.resolve(
            "com.test.pkg", "__data__/profiles/sample/missing.json").isEmpty());
        QVERIFY(resolver_.resolve(
            "com.test.pkg", "__data__/../com.test.other/secret.txt").isEmpty());
        QVERIFY(resolver_.resolve(
            "com.test.pkg", "/__data__/profiles/sample/gauge.json").isEmpty());
    }
    void testPackageLocalDataCollisionIsReserved() {
        QVERIFY(resolver_.resolve(
            "com.test.pkg", "__data__/shadow.json").isEmpty());
    }
    void testDataRouteRejectsExternalAndInternalSymlinks() {
        QVERIFY(QFile::link(dir_.filePath("outside.txt"),
                            dir_.filePath("data/com.test.pkg/profiles/sample/external.txt")));
        QVERIFY(QFile::link(
            dir_.filePath("data/com.test.pkg/profiles/sample/internal.txt"),
            dir_.filePath("data/com.test.pkg/profiles/sample/internal-link.txt")));
        QVERIFY(resolver_.resolve(
            "com.test.pkg", "__data__/profiles/sample/external.txt").isEmpty());
        QVERIFY(resolver_.resolve(
            "com.test.pkg", "__data__/profiles/sample/internal-link.txt").isEmpty());
    }
    void testDataRootMayAppearAfterConfiguration() {
        QTemporaryDir lateDir;
        oap::WebWidgetContentResolver resolver;
        resolver.registerPackage("com.test.late", dir_.filePath("pkg"));
        const QString root = lateDir.filePath("later");
        resolver.setDataRoot(root);
        QVERIFY(resolver.resolve(
            "com.test.late", "__data__/profiles/item/value.json").isEmpty());
        QFileInfo info(root + "/com.test.late/profiles/item/value.json");
        QDir().mkpath(info.absolutePath());
        QFile file(info.absoluteFilePath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{}");
        file.close();
        QVERIFY(!resolver.resolve(
            "com.test.late", "__data__/profiles/item/value.json").isEmpty());
    }
    void testContentTypes() {
        using R = oap::WebWidgetContentResolver;
        QCOMPARE(R::contentTypeFor("a/index.html"), QByteArray("text/html"));
        QCOMPARE(R::contentTypeFor("a/x.js"), QByteArray("application/javascript"));
        QCOMPARE(R::contentTypeFor("a/x.css"), QByteArray("text/css"));
        QCOMPARE(R::contentTypeFor("a/x.svg"), QByteArray("image/svg+xml"));
        QCOMPARE(R::contentTypeFor("a/x.png"), QByteArray("image/png"));
        QCOMPARE(R::contentTypeFor("a/x.jpg"), QByteArray("image/jpeg"));
        QCOMPARE(R::contentTypeFor("a/x.woff2"), QByteArray("font/woff2"));
        QCOMPARE(R::contentTypeFor("a/x.json"), QByteArray("application/json"));
        QCOMPARE(R::contentTypeFor("a/x.bin"), QByteArray("application/octet-stream"));
    }
};
QTEST_GUILESS_MAIN(TestWebWidgetResolver)
#include "test_web_widget_resolver.moc"
