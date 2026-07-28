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
            QStringLiteral("qml/applications/settings/ApiSettings.qml"),
            QStringLiteral("qml/applications/settings/AudioSettings.qml"),
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
        const QString aaSource = sourceFor(
            QStringLiteral("qml/applications/settings/AASettings.qml"));
        QVERIFY2(!aaSource.isEmpty(), "Failed to read AASettings.qml");
        const QString pluginSource = sourceFor(
            QStringLiteral("src/plugins/android_auto/AndroidAutoPlugin.cpp"));
        QVERIFY2(!pluginSource.isEmpty(), "Failed to read AndroidAutoPlugin.cpp");

        const QString mainSource = sourceFor(QStringLiteral("src/main.cpp"));
        QVERIFY2(!mainSource.isEmpty(), "Failed to read main.cpp");

        // Should use ProjectionStatus provider, not AAOrchestrator global
        QVERIFY2(source.indexOf(QStringLiteral("ProjectionStatus")) >= 0,
                 "DebugSettings should reference ProjectionStatus provider");
        QVERIFY2(source.indexOf(QStringLiteral("AAOrchestrator")) < 0,
                 "DebugSettings should not reference AAOrchestrator global");

        // Production GAL is durable Android Auto configuration, not CLUSTER
        // laboratory state.
        QVERIFY2(aaSource.indexOf(QStringLiteral("label: \"GAL Version\"")) >= 0,
                 "AASettings should own the production GAL picker");
        QVERIFY2(aaSource.indexOf(QStringLiteral(
                     "configPath: \"connection.gal_version\"")) >= 0,
                 "The production GAL picker should persist through ConfigService");
        QVERIFY2(aaSource.indexOf(QStringLiteral(
                     "options: [\"1.7\", \"4.3\", \"5.0\", \"5.1\", \"6.0\"]")) >= 0,
                 "The production picker should expose only accepted versions");
        QVERIFY2(source.indexOf(QStringLiteral("clusterGalVersion")) < 0,
                 "DebugSettings must not retain a CLUSTER GAL picker");
        QVERIFY2(source.indexOf(QStringLiteral("gal_version")) < 0,
                 "The CLUSTER lab must not dispatch the production GAL key");
        QVERIFY2(source.indexOf(QStringLiteral("requestedGalVersion")) < 0,
                 "DebugSettings must not retain CLUSTER-owned GAL diagnostics");
        QVERIFY2(pluginSource.indexOf(QStringLiteral(
                     "connection.gal_version")) >= 0,
                 "Changing production GAL should trigger AA renegotiation");
        QVERIFY2(aaSource.indexOf(QStringLiteral(
                     "label: \"Dashboard Navigation\"")) >= 0,
                 "AASettings should expose the secondary content picker");
        QVERIFY2(aaSource.indexOf(QStringLiteral(
                     "configPath: \"video.secondary_display_content\"")) >= 0,
                 "The secondary content picker should persist through ConfigService");
        QVERIFY2(aaSource.indexOf(QStringLiteral(
                     "options: [\"Map\", \"Turn card\"]")) >= 0,
                 "The secondary content picker should expose the accepted modes");
        QVERIFY2(aaSource.indexOf(QStringLiteral(
                     "values: [\"map\", \"turn_card\"]")) >= 0,
                 "The picker should persist stable protocol-independent values");
        QVERIFY2(pluginSource.indexOf(QStringLiteral(
                     "video.secondary_display_content")) >= 0,
                 "Changing secondary content should trigger AA renegotiation");

        // One apply dispatch carries the complete, normalized CLUSTER profile.
        const int applyFunction = source.indexOf(
            QStringLiteral("function applyClusterProfile()"));
        const int syncFunction = source.indexOf(
            QStringLiteral("function syncClusterProfileControls()"));
        QVERIFY2(applyFunction >= 0 && syncFunction > applyFunction,
                 "DebugSettings should define bounded apply and sync helpers");
        const QString applySource = source.mid(applyFunction,
                                               syncFunction - applyFunction);
        QCOMPARE(applySource.count(QStringLiteral(
                     "ActionRegistry.dispatch(\"aa.cluster.applyProfile\"")), 1);
        QVERIFY2(applySource.indexOf(QStringLiteral("gal_version")) < 0,
                 "The CLUSTER profile payload must not carry session GAL");
        QVERIFY2(applySource.indexOf(QStringLiteral(
                     "\"native_turn_card_available\":")) >= 0,
                 "The profile payload should use the native turn-card key");
        QVERIFY2(source.indexOf(QStringLiteral(
                     "Advertise native HU turn card (lab)")) >= 0,
                 "DebugSettings should describe the native HU declaration honestly");
        QVERIFY2(source.indexOf(QStringLiteral("turn_data_available")) < 0,
                 "The retired turn_data_available action key must not return");
        QVERIFY2(source.indexOf(QStringLiteral("bit 16")) < 0,
                 "The retired session-bit-16 wording must not return");

        // Button presses should route through ActionRegistry
        QVERIFY2(source.indexOf(QStringLiteral("ActionRegistry.dispatch")) >= 0,
                 "DebugSettings AA buttons should route through ActionRegistry");
        QVERIFY2(source.indexOf(QStringLiteral("aa.cluster.applyProfile")) >= 0,
                 "DebugSettings should expose the runtime CLUSTER profile action");
        QVERIFY2(source.indexOf(QStringLiteral("aa.cluster.resetProfile")) >= 0,
                 "DebugSettings should expose the CLUSTER baseline reset action");
        QVERIFY2(source.indexOf(QStringLiteral("AAClusterDisplay.applyClusterProfile")) < 0,
                 "DebugSettings must not bypass ActionRegistry for profile changes");
        QVERIFY2(source.indexOf(QStringLiteral("AAClusterDisplay.resetClusterProfile")) < 0,
                 "DebugSettings must not bypass ActionRegistry for profile reset");
        QVERIFY2(mainSource.indexOf(QStringLiteral(
                     "\"aa.cluster.applyProfile\"")) >= 0,
                 "main.cpp should retain the apply-profile ActionRegistry adapter");
        QVERIFY2(mainSource.indexOf(QStringLiteral(
                     "clusterController->applyClusterProfile")) >= 0,
                 "The apply-profile action should route to the CLUSTER controller");
        QVERIFY2(mainSource.indexOf(QStringLiteral(
                     "\"aa.cluster.resetProfile\"")) >= 0,
                 "main.cpp should retain the reset-profile ActionRegistry adapter");
        QVERIFY2(mainSource.indexOf(QStringLiteral(
                     "clusterController->resetClusterProfile")) >= 0,
                 "The reset-profile action should route to the CLUSTER controller");

        // Remaining CLUSTER diagnostics come from the provider exposed by main.
        QVERIFY2(source.indexOf(QStringLiteral("AAClusterDisplay.profileGeneration")) >= 0,
                 "DebugSettings should show provider-owned profile generation");
        QVERIFY2(source.indexOf(QStringLiteral("AAClusterDisplay.viewportContentWidth")) >= 0,
                 "DebugSettings should show provider-owned active crop width");
        QVERIFY2(source.indexOf(QStringLiteral("AAClusterDisplay.viewportContentHeight")) >= 0,
                 "DebugSettings should show provider-owned active crop height");
        QVERIFY2(source.indexOf(QStringLiteral("AAClusterDisplay.profileStatusText")) >= 0,
                 "DebugSettings should show controller-owned CLUSTER diagnostics");
        QVERIFY2(mainSource.indexOf(QStringLiteral("\"AAClusterDisplay\"")) >= 0,
                 "main.cpp should expose the CLUSTER diagnostics provider to QML");
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
            QStringLiteral("qml/applications/settings/ApiSettings.qml"),
            QStringLiteral("qml/applications/settings/AudioSettings.qml"),
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

    void testThemeSettingsWallpaperToggleAndLayout()
    {
        const QString source = sourceFor(QStringLiteral("qml/applications/settings/ThemeSettings.qml"));
        QVERIFY2(!source.isEmpty(), "Failed to read ThemeSettings.qml");

        // Toggle label exists
        QVERIFY2(source.indexOf(QStringLiteral("Custom Wallpaper")) >= 0,
                 "ThemeSettings should have a Custom Wallpaper toggle label");

        // Delete Theme appears AFTER force_dark_mode in file order
        int deletePos = source.indexOf(QStringLiteral("Delete Theme"));
        int darkModePos = source.indexOf(QStringLiteral("force_dark_mode"));
        QVERIFY2(deletePos >= 0, "ThemeSettings should contain Delete Theme");
        QVERIFY2(darkModePos >= 0, "ThemeSettings should contain force_dark_mode");
        QVERIFY2(deletePos > darkModePos,
                 "Delete Theme should appear after force_dark_mode (at bottom of settings)");

        // Wallpaper picker is gated by visibility
        QVERIFY2(source.indexOf(QStringLiteral("visible: wpToggle.checked")) >= 0,
                 "Wallpaper picker row should be gated by wpToggle.checked visibility");

        // Config path for wallpaper override is referenced
        QVERIFY2(source.indexOf(QStringLiteral("wallpaper_override")) >= 0,
                 "ThemeSettings should reference wallpaper_override config path");
    }

    void testFullScreenPickerDialogBlocksBackHold()
    {
        const QString source = sourceFor(QStringLiteral("qml/controls/FullScreenPicker.qml"));
        QVERIFY2(!source.isEmpty(), "Failed to read FullScreenPicker.qml");

        const int dialogPos = source.indexOf(QStringLiteral("id: pickerDialog"));
        QVERIFY2(dialogPos >= 0, "FullScreenPicker should define pickerDialog");

        const int blockedPos = source.indexOf(QStringLiteral("blocksBackHold: true"), dialogPos);
        QVERIFY2(blockedPos > dialogPos,
                 "FullScreenPicker dialog subtree should opt out of SettingsInputBoundary long-press handling");
    }
    // 2026-07-14 Companion/External-API merge contract: exactly one menu
    // entry ("Companion" -> pageId api), no legacy companion page or
    // controls, and the merged page carries all three sections.
    void testCompanionApiMergeContract()
    {
        const QString menuSource = sourceFor(QStringLiteral("qml/applications/settings/SettingsMenu.qml"));
        QVERIFY2(!menuSource.isEmpty(), "Failed to read SettingsMenu.qml");

        QVERIFY2(menuSource.indexOf(QStringLiteral("name: \"Companion\"; icon: \"\\ue324\"; pageId: \"api\"")) >= 0,
                 "Menu should have a single Companion entry mapped to the api page");
        QVERIFY2(menuSource.indexOf(QStringLiteral("pageId: \"companion\"")) < 0,
                 "The legacy companion pageId must not come back");
        QVERIFY2(menuSource.indexOf(QStringLiteral("CompanionSettings")) < 0,
                 "SettingsMenu must not reference the deleted CompanionSettings page");
        QVERIFY2(menuSource.indexOf(QStringLiteral("\"External API\"")) < 0,
                 "The separate External API menu title is gone (merged into Companion)");

        const QString pageSource = sourceFor(QStringLiteral("qml/applications/settings/ApiSettings.qml"));
        QVERIFY2(!pageSource.isEmpty(), "Failed to read ApiSettings.qml");

        // Three sections, in spirit: pairing, status, advanced.
        QVERIFY2(pageSource.indexOf(QStringLiteral("text: \"Remote Client Pairing\"")) >= 0,
                 "Merged page should keep the pairing section");
        QVERIFY2(pageSource.indexOf(QStringLiteral("text: \"Phone Status\"")) >= 0,
                 "Merged page should carry the phone status section");
        QVERIFY2(pageSource.indexOf(QStringLiteral("text: \"Advanced\"")) >= 0,
                 "Merged page should keep the API toggles under Advanced");
        QVERIFY2(pageSource.indexOf(QStringLiteral("configPath: \"api.enabled\"")) >= 0,
                 "Merged page should expose api.enabled");
        QVERIFY2(pageSource.indexOf(QStringLiteral("configPath: \"api.expose_lan\"")) >= 0,
                 "Merged page should expose api.expose_lan");
        QVERIFY2(pageSource.indexOf(QStringLiteral("CompanionState.connected")) >= 0,
                 "Merged page should bind the live companion state");

        // Retired pairing controls must not resurface (B2 teardown 2026-07-14).
        QVERIFY2(pageSource.indexOf(QStringLiteral("companion.enabled")) < 0,
                 "The retired companion.enabled toggle must not resurface (B2 teardown 2026-07-14)");
        QVERIFY2(pageSource.indexOf(QStringLiteral("generatePairingPin")) < 0,
                 "The legacy CompanionService pairing flow must not resurface");
    }
};

QTEST_MAIN(TestSettingsMenuStructure)

#include "test_settings_menu_structure.moc"
