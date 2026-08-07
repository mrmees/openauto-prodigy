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
};

QTEST_GUILESS_MAIN(TestWebWidgetHostStructure)
#include "test_web_widget_host_structure.moc"
