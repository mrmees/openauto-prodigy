#include <QtTest>
#include <QTemporaryDir>
#include "core/widget/WebWidgetScanner.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "core/webwidget/WebWidgetContentResolver.hpp"

class TestWebWidgetScanner : public QObject {
    Q_OBJECT

    void writePackage(const QString& root, const QString& dirName, const QByteArray& yaml) {
        QDir().mkpath(root + u'/' + dirName);
        QFile f(root + u'/' + dirName + QStringLiteral("/widget.yaml"));
        f.open(QIODevice::WriteOnly);
        f.write(yaml);
    }

private slots:
    void testScanRegistersValidSkipsBad() {
        QTemporaryDir dir;
        writePackage(dir.path(), "speedo",
            "id: com.test.speedo\nname: Speedo\nentry: main.html\n"
            "size: {defaultCols: 2, defaultRows: 2}\n");
        writePackage(dir.path(), "clockish",
            "id: com.test.clockish\nname: Clockish\n");
        writePackage(dir.path(), "broken", "name: NoId\n");          // invalid
        QDir().mkpath(dir.path() + QStringLiteral("/nomanifest"));    // no widget.yaml

        oap::WidgetRegistry registry;
        oap::WebWidgetContentResolver resolver;
        const int n = oap::WebWidgetScanner::scan(dir.path(), registry, &resolver);
        QCOMPARE(n, 2);

        const auto d = registry.descriptor(QStringLiteral("com.test.speedo"));
        QVERIFY(d.has_value());
        QCOMPARE(d->displayName, QStringLiteral("Speedo"));
        QCOMPARE(d->contributionKind, oap::DashboardContributionKind::WebWidget);
        QCOMPARE(d->qmlComponent,
                 QUrl(QStringLiteral("qrc:/OpenAutoProdigy/WebWidgetHost.qml")));
        QCOMPARE(d->defaultConfig.value(QStringLiteral("url")).toString(),
                 QStringLiteral("prodigy://widgets/com.test.speedo/main.html"));
        QCOMPARE(d->defaultCols, 2);
        QVERIFY(!registry.descriptor(QStringLiteral("com.test.clockish"))->displayName.isEmpty());
        // resolver learned the package dir
        QVERIFY(resolver.resolve(QStringLiteral("com.test.speedo"),
                                 QStringLiteral("widget.yaml")).endsWith(
                                 QStringLiteral("speedo/widget.yaml")));
    }
    void testDuplicateIdSkippedFirstWins() {
        QTemporaryDir dir;
        writePackage(dir.path(), "a", "id: com.test.dup\nname: First\n");
        oap::WidgetRegistry registry;
        oap::WidgetDescriptor native;
        native.id = QStringLiteral("com.test.dup");
        native.displayName = QStringLiteral("Native");
        registry.registerWidget(native);
        QCOMPARE(oap::WebWidgetScanner::scan(dir.path(), registry, nullptr), 0);
        QCOMPARE(registry.descriptor(QStringLiteral("com.test.dup"))->displayName,
                 QStringLiteral("Native"));
    }
    void testMissingRootDirNoop() {
        oap::WidgetRegistry registry;
        QCOMPARE(oap::WebWidgetScanner::scan(QStringLiteral("/nonexistent-dir-xyz"),
                                             registry, nullptr), 0);
    }
};
QTEST_GUILESS_MAIN(TestWebWidgetScanner)
#include "test_web_widget_scanner.moc"
