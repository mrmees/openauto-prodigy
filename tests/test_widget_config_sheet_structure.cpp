#include <QFile>
#include <QTest>

class TestWidgetConfigSheetStructure : public QObject {
    Q_OBJECT

    static QString sourceFor(const QString& relativePath)
    {
        QFile file(QStringLiteral(TEST_SOURCE_DIR "/") + relativePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    }

private slots:
    void testConfigurationUsesFullScreenDraftAndExplicitCommit()
    {
        const QString source = sourceFor(
            QStringLiteral("qml/components/WidgetConfigSheet.qml"));
        QVERIFY2(!source.isEmpty(), "Failed to read WidgetConfigSheet.qml");
        QVERIFY(source.contains(QStringLiteral("property var draftConfig")));
        QVERIFY(source.contains(QStringLiteral("function updateDraft(")));
        QCOMPARE(source.count(QStringLiteral("WidgetGridModel.setWidgetConfig")), 1);
        QVERIFY(!source.contains(QStringLiteral("parent.height * 0.6")));
        QVERIFY(source.contains(QStringLiteral("height: parent ? parent.height : 0")));
        QVERIFY(source.contains(QStringLiteral("objectName: \"widgetConfigSave\"")));
        QVERIFY(source.contains(QStringLiteral("objectName: \"widgetConfigCancel\"")));
        QVERIFY(source.contains(QStringLiteral("buttonEnabled: root.draftValid")));
        QVERIFY(source.contains(QStringLiteral("No items installed")));
        QVERIFY(source.contains(QStringLiteral("Missing: ")));
        QVERIFY(source.indexOf(QStringLiteral("Missing: "))
                < source.indexOf(QStringLiteral("No items installed")));
        QVERIFY(source.contains(QStringLiteral("fieldData.type === \"collection\"")));
        QVERIFY(source.contains(QStringLiteral("FullScreenPicker")));
        QVERIFY(source.contains(QStringLiteral("FilledButton")));
        QVERIFY(source.contains(QStringLiteral("OutlinedButton")));
        QVERIFY(source.contains(QStringLiteral("objectName: \"widgetConfigClose\"")));
        QVERIFY(source.contains(QStringLiteral("dim: false")));
        QVERIFY(!source.contains(QStringLiteral("\n                Button {")));
    }

    void testConfigureOnAddUsesPlacedInstance()
    {
        const QString source = sourceFor(
            QStringLiteral("qml/applications/home/HomeMenu.qml"));
        QVERIFY2(!source.isEmpty(), "Failed to read HomeMenu.qml");
        QVERIFY(source.contains(QStringLiteral("placeWidgetAndReturnInstance")));
        QVERIFY(source.contains(QStringLiteral("meta.configureOnAdd")));
        QVERIFY(source.contains(QStringLiteral("Qt.callLater")));
    }
};

QTEST_GUILESS_MAIN(TestWidgetConfigSheetStructure)
#include "test_widget_config_sheet_structure.moc"
