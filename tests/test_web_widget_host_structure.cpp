#include <QFile>
#include <QTest>

class TestWebWidgetHostStructure : public QObject {
    Q_OBJECT

private slots:
    void testHostPublishesOnlyPublicConfiguration()
    {
        QFile file(QStringLiteral(TEST_SOURCE_DIR "/qml/widgets/WebWidgetHost.qml"));
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                 "Failed to read WebWidgetHost.qml");
        const QString source = QString::fromUtf8(file.readAll());

        QVERIFY(source.contains(QStringLiteral("function publicConfigObject()")));
        QVERIFY(source.contains(QStringLiteral("config: publicConfigObject()")));
        QVERIFY(source.contains(QStringLiteral("function pushConfig()")));
        QVERIFY(source.contains(QStringLiteral("onEffectiveConfigChanged")));
        QVERIFY(source.contains(QStringLiteral("hostRoot.pushConfig()")));
        QVERIFY(source.contains(QStringLiteral("prodigy._updateConfig(")));
        QVERIFY(source.contains(QStringLiteral("key !== \"url\"")));
    }

    void testSuccessfulLoadRefreshesContextAndConfiguration()
    {
        QFile file(QStringLiteral(TEST_SOURCE_DIR "/qml/widgets/WebWidgetHost.qml"));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = QString::fromUtf8(file.readAll());
        const qsizetype success = source.indexOf(
            QStringLiteral("WebEngineView.LoadSucceededStatus"));
        QVERIFY(success >= 0);
        const QString successPath = source.mid(success, 600);
        QVERIFY(successPath.contains(QStringLiteral("hostRoot.pushContext()")));
        QVERIFY(successPath.contains(QStringLiteral("hostRoot.pushConfig()")));
    }

    void testLiveUpdateReadsFreshContextConfiguration()
    {
        QFile file(QStringLiteral(TEST_SOURCE_DIR "/qml/widgets/WebWidgetHost.qml"));
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                 "Failed to read WebWidgetHost.qml");
        const QString source = QString::fromUtf8(file.readAll());

        // effectiveCfg is a QML binding. A signal handler can run before that
        // binding has refreshed, so the live-update path must read the C++
        // property directly instead of serializing the cached binding.
        const qsizetype publicConfig = source.indexOf(
            QStringLiteral("function publicConfigObject()"));
        QVERIFY(publicConfig >= 0);
        const QString serializationPath = source.mid(publicConfig, 700);
        QVERIFY(serializationPath.contains(
            QStringLiteral("widgetContext.effectiveConfig")));
    }
};

QTEST_GUILESS_MAIN(TestWebWidgetHostStructure)
#include "test_web_widget_host_structure.moc"
