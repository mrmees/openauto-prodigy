#include <QFile>
#include <QTest>

class TestSettingsMenuStructure : public QObject {
    Q_OBJECT

private:
    static QString sourceFor(const QString& relativePath)
    {
        QFile file(QStringLiteral(TEST_SOURCE_DIR "/") + relativePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    }

private slots:
    void testBackHoldUsesOverlayAboveStackView()
    {
        const QString source = sourceFor(QStringLiteral("qml/applications/settings/SettingsMenu.qml"));
        QVERIFY2(!source.isEmpty(), "Failed to read SettingsMenu.qml");

        const int stackViewPos = source.indexOf(QStringLiteral("id: settingsStack"));
        QVERIFY2(stackViewPos >= 0, "Expected SettingsMenu to define settingsStack");

        const int overlayPos = source.indexOf(QStringLiteral("objectName: \"backHoldOverlay\""));
        QVERIFY2(overlayPos >= 0,
                 "Expected SettingsMenu to define a dedicated back-hold overlay");
        QVERIFY2(overlayPos > stackViewPos,
                 "Back-hold overlay should be declared after StackView so it renders above it");

        QVERIFY2(source.indexOf(QStringLiteral("anchors.fill: parent"), overlayPos) >= 0,
                 "Back-hold overlay must cover the full SettingsMenu surface");
        QVERIFY2(source.indexOf(QStringLiteral("z: 1000"), overlayPos) >= 0,
                 "Back-hold overlay must sit above StackView content");
        QVERIFY2(source.indexOf(QStringLiteral("id: backHoldTouch"), overlayPos) >= 0,
                 "Touch TapHandler should live under the back-hold overlay");
        QVERIFY2(source.indexOf(QStringLiteral("id: backHoldMouse"), overlayPos) >= 0,
                 "Mouse TapHandler should live under the back-hold overlay");
        QVERIFY2(source.indexOf(QStringLiteral("function showHoldIndicator(")) >= 0,
                 "SettingsMenu should expose a shared ripple show helper");
        QVERIFY2(source.indexOf(QStringLiteral("function hideHoldIndicator()")) >= 0,
                 "SettingsMenu should expose a shared ripple hide helper");
    }

    void testInteractiveControlsOwnBackHold()
    {
        const QString holdAreaSource = sourceFor(QStringLiteral("qml/controls/SettingsHoldArea.qml"));
        QVERIFY2(!holdAreaSource.isEmpty(), "Failed to read SettingsHoldArea.qml");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("pressAndHoldInterval: 500")) >= 0,
                 "SettingsHoldArea should trigger long hold at 500ms");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("ApplicationController.requestBack()")) >= 0,
                 "SettingsHoldArea should request back on long hold");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("showHoldIndicator")) >= 0,
                 "SettingsHoldArea should show the shared ripple on press");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("hideHoldIndicator")) >= 0,
                 "SettingsHoldArea should hide the shared ripple on release/cancel");

        const QString toggleSource = sourceFor(QStringLiteral("qml/controls/SettingsToggle.qml"));
        QVERIFY2(!toggleSource.isEmpty(), "Failed to read SettingsToggle.qml");
        QVERIFY2(toggleSource.indexOf(QStringLiteral("readonly property bool blocksBackHold: true")) >= 0,
                 "SettingsToggle should own back-hold on its interactive surface");
        QVERIFY2(toggleSource.indexOf(QStringLiteral("SettingsHoldArea")) >= 0,
                 "SettingsToggle should use the shared hold-aware surface");

        const QString sliderSource = sourceFor(QStringLiteral("qml/controls/SettingsSlider.qml"));
        QVERIFY2(!sliderSource.isEmpty(), "Failed to read SettingsSlider.qml");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("readonly property bool blocksBackHold: true")) >= 0,
                 "SettingsSlider should own back-hold on its interactive surface");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("id: holdTimer")) >= 0,
                 "SettingsSlider should define a dedicated long-hold timer");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("interval: 500")) >= 0,
                 "SettingsSlider long-hold timer should use 500ms");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("ApplicationController.requestBack()")) >= 0,
                 "SettingsSlider should request back on long hold");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("showHoldIndicator")) >= 0,
                 "SettingsSlider should show the shared ripple when hold starts");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("hideHoldIndicator")) >= 0,
                 "SettingsSlider should hide the shared ripple when hold ends");

        const QString pickerSource = sourceFor(QStringLiteral("qml/controls/FullScreenPicker.qml"));
        QVERIFY2(!pickerSource.isEmpty(), "Failed to read FullScreenPicker.qml");
        QVERIFY2(pickerSource.indexOf(QStringLiteral("readonly property bool blocksBackHold: true")) >= 0,
                 "FullScreenPicker should own back-hold on its interactive surface");
        QVERIFY2(pickerSource.indexOf(QStringLiteral("SettingsHoldArea")) >= 0,
                 "FullScreenPicker should use the shared hold-aware surface");

        const QString listItemSource = sourceFor(QStringLiteral("qml/controls/SettingsListItem.qml"));
        QVERIFY2(!listItemSource.isEmpty(), "Failed to read SettingsListItem.qml");
        QVERIFY2(listItemSource.indexOf(QStringLiteral("SettingsHoldArea")) >= 0,
                 "SettingsListItem should use the shared hold-aware surface");

        const QString segmentedSource = sourceFor(QStringLiteral("qml/controls/SegmentedButton.qml"));
        QVERIFY2(!segmentedSource.isEmpty(), "Failed to read SegmentedButton.qml");
        QVERIFY2(segmentedSource.indexOf(QStringLiteral("SettingsHoldArea")) >= 0,
                 "SegmentedButton should use the shared hold-aware surface");

        const QString rowSource = sourceFor(QStringLiteral("qml/controls/SettingsRow.qml"));
        QVERIFY2(!rowSource.isEmpty(), "Failed to read SettingsRow.qml");
        QVERIFY2(rowSource.indexOf(QStringLiteral("SettingsHoldArea")) >= 0,
                 "Interactive SettingsRow should use the shared hold-aware surface");
    }
};

QTEST_MAIN(TestSettingsMenuStructure)

#include "test_settings_menu_structure.moc"
