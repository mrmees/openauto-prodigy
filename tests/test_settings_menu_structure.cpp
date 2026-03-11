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

    void testSettingsRowsOwnBackHoldWhileMenuKeepsWhitespaceOverlay()
    {
        const QString holdAreaSource = sourceFor(QStringLiteral("qml/controls/SettingsHoldArea.qml"));
        QVERIFY2(!holdAreaSource.isEmpty(), "Failed to read SettingsHoldArea.qml");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("property bool enableBackHold: true")) >= 0,
                 "SettingsHoldArea should be able to run in row-owned short-click-only mode");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("consumeBackHoldTrigger")) >= 0,
                 "SettingsHoldArea should suppress short clicks when the enclosing row long-hold already won");

        const QString sliderSource = sourceFor(QStringLiteral("qml/controls/SettingsSlider.qml"));
        QVERIFY2(!sliderSource.isEmpty(), "Failed to read SettingsSlider.qml");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("function _findSettingsRow()")) >= 0,
                 "SettingsSlider should locate the enclosing SettingsRow for row-owned hold coordination");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("cancelBackHold()")) >= 0,
                 "SettingsSlider should cancel row hold when real dragging starts");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("consumeBackHoldTrigger()")) >= 0,
                 "SettingsSlider should suppress commit behavior when the row long-hold already won");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("id: holdTimer")) < 0,
                 "SettingsSlider should no longer define a dedicated long-hold timer");

        const QString rowSource = sourceFor(QStringLiteral("qml/controls/SettingsRow.qml"));
        QVERIFY2(!rowSource.isEmpty(), "Failed to read SettingsRow.qml");
        QVERIFY2(rowSource.indexOf(QStringLiteral("readonly property bool blocksBackHold: true")) >= 0,
                 "Every SettingsRow should block the menu overlay and own row-level hold");
        QVERIFY2(rowSource.indexOf(QStringLiteral("function cancelBackHold()")) >= 0,
                 "SettingsRow should expose a cancel helper for drag-driven child controls");
        QVERIFY2(rowSource.indexOf(QStringLiteral("function consumeBackHoldTrigger()")) >= 0,
                 "SettingsRow should expose long-hold suppression state to child controls");
        QVERIFY2(rowSource.indexOf(QStringLiteral("ApplicationController.requestBack()")) >= 0,
                 "SettingsRow should request back when row-level long hold fires");
        QVERIFY2(rowSource.indexOf(QStringLiteral("showHoldIndicator")) >= 0,
                 "SettingsRow should drive the shared ripple through SettingsMenu");
    }
};

QTEST_MAIN(TestSettingsMenuStructure)

#include "test_settings_menu_structure.moc"
