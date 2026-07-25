#include <QFile>
#include <QTest>

#include "core/widget/WidgetRegistry.hpp"
#include "plugins/android_auto/AAClusterWidgetRegistration.hpp"

class TestAAClusterWidget : public QObject {
    Q_OBJECT

private:
    static QString qmlSource()
    {
        QFile file(QStringLiteral(TEST_SOURCE_DIR
                                  "/qml/widgets/AAClusterWidget.qml"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    }

private slots:
    void disabledConfigRegistersNothing()
    {
        oap::WidgetRegistry registry;
        QVERIFY(!oap::plugins::registerAAClusterWidget(
            registry, {false, {}}));
        QVERIFY(!registry.descriptor(QStringLiteral("org.openauto.aa-cluster"))
                     .has_value());
        QVERIFY(registry.widgetsFittingSpace(2, 2).isEmpty());
    }

    void enabledConfigRegistersFixedSquare()
    {
        oap::WidgetRegistry registry;
        QVERIFY(oap::plugins::registerAAClusterWidget(registry, {true, {}}));

        const auto descriptor =
            registry.descriptor(QStringLiteral("org.openauto.aa-cluster"));
        QVERIFY(descriptor.has_value());
        QCOMPARE(descriptor->minCols, 2);
        QCOMPARE(descriptor->maxCols, 2);
        QCOMPARE(descriptor->minRows, 2);
        QCOMPARE(descriptor->maxRows, 2);
        QCOMPARE(descriptor->defaultCols, 2);
        QCOMPARE(descriptor->defaultRows, 2);
        QCOMPARE(descriptor->category, QStringLiteral("navigation"));
        QCOMPARE(descriptor->qmlComponent,
                 QUrl(QStringLiteral(
                     "qrc:/OpenAutoProdigy/AAClusterWidget.qml")));
        QCOMPARE(registry.allDescriptors().size(), 1);
        const auto pickerEntries = registry.widgetsFittingSpace(2, 2);
        QCOMPARE(pickerEntries.size(), 1);
        QCOMPARE(pickerEntries.first().id,
                 QStringLiteral("org.openauto.aa-cluster"));
        QVERIFY(registry.widgetsFittingSpace(1, 2).isEmpty());
    }

    void qmlOwnsOneNonInteractivePreserveAspectSink()
    {
        const QString source = qmlSource();
        QVERIFY2(!source.isEmpty(), "Failed to read AAClusterWidget.qml");

        QVERIFY(source.contains(QStringLiteral("import QtMultimedia")));
        QCOMPARE(source.count(QStringLiteral("VideoOutput {")), 1);
        QVERIFY(source.contains(
            QStringLiteral("fillMode: VideoOutput.PreserveAspectFit")));
        QVERIFY(source.contains(QStringLiteral("AAClusterDisplay.rendering")));
        QVERIFY(source.contains(QStringLiteral(
            "attachVideoSink(videoOutput.videoSink)")));
        QVERIFY(source.contains(QStringLiteral(
            "detachVideoSink(videoOutput.videoSink)")));
        QVERIFY(source.contains(QStringLiteral("already in use"),
                                Qt::CaseInsensitive));

        const QStringList forbidden = {
            QStringLiteral("MouseArea"),
            QStringLiteral("TapHandler"),
            QStringLiteral("ShaderEffect"),
            QStringLiteral("sourceRect"),
            QStringLiteral("transform:"),
            QStringLiteral("AAClusterDisplay.Rendering"),
        };
        for (const QString& token : forbidden) {
            QVERIFY2(!source.contains(token),
                     qPrintable(QStringLiteral("Forbidden QML token: %1")
                                    .arg(token)));
        }
    }
};

QTEST_MAIN(TestAAClusterWidget)
#include "test_aa_cluster_widget.moc"
