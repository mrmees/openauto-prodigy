#include <QtTest>

#include "core/webwidget/WebWidgetSchemeHandler.hpp"

class TestWebWidgetScheme : public QObject {
    Q_OBJECT

private slots:
    void testPackageFetchIsAllowedWithoutFilesystemAccess()
    {
        const auto flags = oap::webWidgetSchemeFlags();
        QVERIFY(flags.testFlag(QWebEngineUrlScheme::SecureScheme));
        QVERIFY(flags.testFlag(QWebEngineUrlScheme::FetchApiAllowed));
        QVERIFY(!flags.testFlag(QWebEngineUrlScheme::LocalAccessAllowed));
    }
};

QTEST_GUILESS_MAIN(TestWebWidgetScheme)
#include "test_web_widget_scheme.moc"
