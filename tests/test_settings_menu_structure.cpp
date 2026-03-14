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
    void testLongPressBackUsesBoundaryItem()
    {
        const QString source = sourceFor(QStringLiteral("qml/applications/settings/SettingsMenu.qml"));
        QVERIFY2(!source.isEmpty(), "Failed to read SettingsMenu.qml");

        QVERIFY2(source.indexOf(QStringLiteral("SettingsInputBoundary")) >= 0,
                 "SettingsMenu should use a SettingsInputBoundary wrapper for subtree-wide hold detection");
        QVERIFY2(source.indexOf(QStringLiteral("id: backHoldTouch")) < 0,
                 "SettingsMenu should no longer define root-level backHoldTouch TapHandlers");
        QVERIFY2(source.indexOf(QStringLiteral("id: backHoldMouse")) < 0,
                 "SettingsMenu should no longer define root-level backHoldMouse TapHandlers");

        // Shared ripple helpers
        QVERIFY2(source.indexOf(QStringLiteral("function showHoldIndicator(")) >= 0,
                 "SettingsMenu should expose a shared ripple show helper");
        QVERIFY2(source.indexOf(QStringLiteral("function hideHoldIndicator()")) >= 0,
                 "SettingsMenu should expose a shared ripple hide helper");
    }

    void testTopLevelCategoryRowsUseMouseArea()
    {
        const QString source = sourceFor(QStringLiteral("qml/applications/settings/SettingsMenu.qml"));
        QVERIFY2(!source.isEmpty(), "Failed to read SettingsMenu.qml");

        QVERIFY2(source.indexOf(QStringLiteral("id: delegateArea")) >= 0,
                 "Top-level settings categories should use a MouseArea for clicks");
        QVERIFY2(source.indexOf(QStringLiteral("onClicked: openPage(model.pageId)")) >= 0,
                 "Category-row MouseArea should open the selected settings page on click");
    }

    void testBoundaryOwnsBackHoldInsteadOfRowsAndControls()
    {
        const QString holdAreaSource = sourceFor(QStringLiteral("qml/controls/SettingsHoldArea.qml"));
        QVERIFY2(!holdAreaSource.isEmpty(), "Failed to read SettingsHoldArea.qml");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("property bool enableBackHold")) < 0,
                 "SettingsHoldArea should no longer own back-hold behavior");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("holdTriggered")) < 0,
                 "SettingsHoldArea should not track long-hold trigger state");
        QVERIFY2(holdAreaSource.indexOf(QStringLiteral("ApplicationController.requestBack()")) < 0,
                 "SettingsHoldArea should not request back directly");

        const QString sliderSource = sourceFor(QStringLiteral("qml/controls/SettingsSlider.qml"));
        QVERIFY2(!sliderSource.isEmpty(), "Failed to read SettingsSlider.qml");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("function _findSettingsRow()")) < 0,
                 "SettingsSlider should not locate SettingsRow for back-hold coordination");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("_settingsRow")) < 0,
                 "SettingsSlider should not keep row-level back-hold state");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("cancelBackHold()")) < 0,
                 "SettingsSlider should not cancel row-owned back-hold anymore");
        QVERIFY2(sliderSource.indexOf(QStringLiteral("consumeBackHoldTrigger()")) < 0,
                 "SettingsSlider should not suppress commits based on row-owned back-hold state");

        const QString rowSource = sourceFor(QStringLiteral("qml/controls/SettingsRow.qml"));
        QVERIFY2(!rowSource.isEmpty(), "Failed to read SettingsRow.qml");
        QVERIFY2(rowSource.indexOf(QStringLiteral("backHoldOverlay")) < 0,
                 "SettingsRow should not define a back-hold overlay");
        QVERIFY2(rowSource.indexOf(QStringLiteral("function cancelBackHold()")) < 0,
                 "SettingsRow should not expose row-level back-hold cancel helpers");
        QVERIFY2(rowSource.indexOf(QStringLiteral("function consumeBackHoldTrigger()")) < 0,
                 "SettingsRow should not expose row-level back-hold trigger state");
        QVERIFY2(rowSource.indexOf(QStringLiteral("ApplicationController.requestBack()")) < 0,
                 "SettingsRow should not request back directly");
    }

    void testSubsettingsPagesUseSharedHorizontalInset()
    {
        const QString metricsSource = sourceFor(QStringLiteral("qml/controls/UiMetrics.qml"));
        QVERIFY2(!metricsSource.isEmpty(), "Failed to read UiMetrics.qml");
        QVERIFY2(metricsSource.indexOf(QStringLiteral("readonly property int settingsPageInset")) >= 0,
                 "UiMetrics should expose a shared inset token for stacked settings subpages");

        const QStringList pagePaths = {
            QStringLiteral("qml/applications/settings/AASettings.qml"),
            QStringLiteral("qml/applications/settings/AudioSettings.qml"),
            QStringLiteral("qml/applications/settings/CompanionSettings.qml"),
            QStringLiteral("qml/applications/settings/ConnectionSettings.qml"),
            QStringLiteral("qml/applications/settings/DebugSettings.qml"),
            QStringLiteral("qml/applications/settings/DisplaySettings.qml"),
            QStringLiteral("qml/applications/settings/InformationSettings.qml"),
            QStringLiteral("qml/applications/settings/SystemSettings.qml"),
            QStringLiteral("qml/applications/settings/ThemeSettings.qml")
        };

        for (const QString& pagePath : pagePaths) {
            const QString pageSource = sourceFor(pagePath);
            QVERIFY2(!pageSource.isEmpty(), qPrintable(QStringLiteral("Failed to read %1").arg(pagePath)));
            QVERIFY2(pageSource.indexOf(QStringLiteral("anchors.leftMargin: UiMetrics.settingsPageInset")) >= 0,
                     qPrintable(QStringLiteral("%1 should apply the shared left inset").arg(pagePath)));
            QVERIFY2(pageSource.indexOf(QStringLiteral("anchors.rightMargin: UiMetrics.settingsPageInset")) >= 0,
                     qPrintable(QStringLiteral("%1 should apply the shared right inset").arg(pagePath)));
        }
    }

    void testThemeDeleteUsesOutlinedActionButton()
    {
        const QString themeSource = sourceFor(QStringLiteral("qml/applications/settings/ThemeSettings.qml"));
        QVERIFY2(!themeSource.isEmpty(), "Failed to read ThemeSettings.qml");
        QVERIFY2(themeSource.indexOf(QStringLiteral("id: deleteThemeButtonText")) >= 0,
                 "ThemeSettings delete row should define a dedicated delete button label");
        QVERIFY2(themeSource.indexOf(QStringLiteral("border.color: ThemeService.error")) >= 0,
                 "ThemeSettings delete button should use a destructive outlined button border");
        QVERIFY2(themeSource.indexOf(QStringLiteral("text: deleteThemeState.confirmPending ? \"Confirm\" : \"Delete\"")) >= 0,
                 "ThemeSettings delete button should show Delete/Confirm labels");
        QVERIFY2(themeSource.indexOf(QStringLiteral("SettingsHoldArea")) >= 0,
                 "ThemeSettings delete button should use SettingsHoldArea for button clicks");
        QVERIFY2(themeSource.indexOf(QStringLiteral("interactive: true")) < 0,
                 "ThemeSettings delete row should not rely on whole-row interaction anymore");
        QVERIFY2(themeSource.indexOf(QStringLiteral("icon: \"\\ue872\"")) < 0,
                 "ThemeSettings delete row should no longer use the leading trash icon");
    }

    void testDebugSettingsUsesProvidersNotGlobals()
    {
        const QString source = sourceFor(QStringLiteral("qml/applications/settings/DebugSettings.qml"));
        QVERIFY2(!source.isEmpty(), "Failed to read DebugSettings.qml");

        // Should use ProjectionStatus provider, not AAOrchestrator global
        QVERIFY2(source.indexOf(QStringLiteral("ProjectionStatus")) >= 0,
                 "DebugSettings should reference ProjectionStatus provider");
        QVERIFY2(source.indexOf(QStringLiteral("AAOrchestrator")) < 0,
                 "DebugSettings should not reference AAOrchestrator global");

        // Button presses should route through ActionRegistry
        QVERIFY2(source.indexOf(QStringLiteral("ActionRegistry.dispatch")) >= 0,
                 "DebugSettings AA buttons should route through ActionRegistry");
    }

    void testSettingsUseSharedScrollHints()
    {
        const QString hintSource = sourceFor(QStringLiteral("qml/controls/SettingsScrollHints.qml"));
        QVERIFY2(!hintSource.isEmpty(), "Failed to read SettingsScrollHints.qml");
        QVERIFY2(hintSource.indexOf(QStringLiteral("property Flickable flickable")) >= 0,
                 "SettingsScrollHints should target an arbitrary Flickable/ListView");
        QVERIFY2(hintSource.indexOf(QStringLiteral("flickable.contentY")) >= 0,
                 "SettingsScrollHints should derive hint visibility from scroll position");
        QVERIFY2(hintSource.indexOf(QStringLiteral("flickable.contentHeight")) >= 0,
                 "SettingsScrollHints should derive hint visibility from content height");
        QVERIFY2(hintSource.indexOf(QStringLiteral("property real hintOpacity: 0.8")) >= 0,
                 "SettingsScrollHints should use stronger opacity for Pi viewing distance");
        QVERIFY2(hintSource.indexOf(QStringLiteral("size: UiMetrics.iconSize")) >= 0,
                 "SettingsScrollHints should use larger chevrons for distant readability");

        const QString menuSource = sourceFor(QStringLiteral("qml/applications/settings/SettingsMenu.qml"));
        QVERIFY2(!menuSource.isEmpty(), "Failed to read SettingsMenu.qml");
        QVERIFY2(menuSource.indexOf(QStringLiteral("id: settingsListView")) >= 0,
                 "SettingsMenu should give the top-level settings ListView a stable id for scroll hints");
        QVERIFY2(menuSource.indexOf(QStringLiteral("SettingsScrollHints")) >= 0,
                 "SettingsMenu should attach shared scroll hints to the top-level settings list");
        QVERIFY2(menuSource.indexOf(QStringLiteral("flickable: settingsListView")) >= 0,
                 "SettingsMenu scroll hints should follow the top-level settings ListView");

        const QStringList pagePaths = {
            QStringLiteral("qml/applications/settings/AASettings.qml"),
            QStringLiteral("qml/applications/settings/AudioSettings.qml"),
            QStringLiteral("qml/applications/settings/CompanionSettings.qml"),
            QStringLiteral("qml/applications/settings/ConnectionSettings.qml"),
            QStringLiteral("qml/applications/settings/DebugSettings.qml"),
            QStringLiteral("qml/applications/settings/DisplaySettings.qml"),
            QStringLiteral("qml/applications/settings/InformationSettings.qml"),
            QStringLiteral("qml/applications/settings/SystemSettings.qml"),
            QStringLiteral("qml/applications/settings/ThemeSettings.qml")
        };

        for (const QString& pagePath : pagePaths) {
            const QString pageSource = sourceFor(pagePath);
            QVERIFY2(!pageSource.isEmpty(), qPrintable(QStringLiteral("Failed to read %1").arg(pagePath)));
            QVERIFY2(pageSource.indexOf(QStringLiteral("SettingsScrollHints")) >= 0,
                     qPrintable(QStringLiteral("%1 should attach the shared scroll-hint overlay").arg(pagePath)));
            QVERIFY2(pageSource.indexOf(QStringLiteral("flickable: root")) >= 0,
                     qPrintable(QStringLiteral("%1 scroll hints should target the page Flickable").arg(pagePath)));
        }
    }

};

QTEST_MAIN(TestSettingsMenuStructure)

#include "test_settings_menu_structure.moc"
