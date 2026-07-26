#include <QTest>

#include "core/YamlConfig.hpp"
#include "core/aa/ProjectedDisplayConfig.hpp"

class TestProjectedDisplayConfig : public QObject {
    Q_OBJECT

private slots:
    void missingValuesUseSafeDefaults()
    {
        oap::YamlConfig yaml;
        const auto config = oap::aa::resolveProjectedClusterConfig(yaml);
        QVERIFY(!config.enabled);
        QCOMPARE(config.setupFocus,
                 oap::aa::ProjectedSetupFocus::ProjectedNoInput);
    }

    void readsDottedPluginIdThroughPluginValue()
    {
        oap::YamlConfig yaml;
        yaml.setPluginValue("org.openauto.android-auto",
                            "experimental_cluster_display", true);
        yaml.setPluginValue("org.openauto.android-auto",
                            "experimental_cluster_setup_focus", "projected");

        const auto config = oap::aa::resolveProjectedClusterConfig(yaml);
        QVERIFY(config.enabled);
        QCOMPARE(config.setupFocus, oap::aa::ProjectedSetupFocus::Projected);
    }

    void invalidFocusFallsBackToNoInput()
    {
        oap::YamlConfig yaml;
        yaml.setPluginValue("org.openauto.android-auto",
                            "experimental_cluster_setup_focus", "invalid");

        QCOMPARE(oap::aa::resolveProjectedClusterConfig(yaml).setupFocus,
                 oap::aa::ProjectedSetupFocus::ProjectedNoInput);
    }

    void clusterViewportGeometryIsCenteredAndFlagIndependent()
    {
        constexpr auto geometry = oap::aa::kClusterViewportGeometry;
        QCOMPARE(geometry.encodedWidth, 800);
        QCOMPARE(geometry.encodedHeight, 480);
        QCOMPARE(geometry.contentWidth, 300);
        QCOMPARE(geometry.contentHeight, 300);
        QCOMPARE(geometry.marginWidth(), 500);
        QCOMPARE(geometry.marginHeight(), 180);
        QCOMPARE(geometry.contentX(), 250);
        QCOMPARE(geometry.contentY(), 90);
        QVERIFY(geometry.isValid());

        oap::YamlConfig yaml;
        QVERIFY(!oap::aa::resolveProjectedClusterConfig(yaml).enabled);

        yaml.setPluginValue("org.openauto.android-auto",
                            "experimental_cluster_display", true);
        QVERIFY(oap::aa::resolveProjectedClusterConfig(yaml).enabled);
        QCOMPARE(oap::aa::kClusterViewportGeometry.encodedWidth, 800);
        QCOMPARE(oap::aa::kClusterViewportGeometry.encodedHeight, 480);
        QCOMPARE(oap::aa::kClusterViewportGeometry.contentWidth, 300);
        QCOMPARE(oap::aa::kClusterViewportGeometry.contentHeight, 300);
        QCOMPARE(oap::aa::kClusterViewportGeometry.contentX(), 250);
        QCOMPARE(oap::aa::kClusterViewportGeometry.contentY(), 90);
    }

    void runtimeProfileDefaultsToAcceptedSquare()
    {
        const oap::aa::ProjectedClusterProfile profile;
        QCOMPARE(profile.galVersion, oap::aa::kGalVersion1_7);
        QCOMPARE(profile.galVersion.toString(), QStringLiteral("1.7"));
        QCOMPARE(profile.resolution, QStringLiteral("480p"));
        QCOMPARE(profile.dpi, 140);
        QCOMPARE(profile.contentWidth, 300);
        QCOMPARE(profile.contentHeight, 300);
        QVERIFY(!profile.nativeTurnCardAvailable);
        QCOMPARE(profile.geometry(), oap::aa::kClusterViewportGeometry);
    }

    void galVersionsCompareAsNumericPairs()
    {
        const oap::aa::GalVersion oneSeven{1, 7};
        const oap::aa::GalVersion oneTen{1, 10};
        const oap::aa::GalVersion fourThree{4, 3};

        QVERIFY(oneSeven < oneTen);
        QVERIFY(oneTen < fourThree);
        QVERIFY(fourThree > oneSeven);
        QCOMPARE(fourThree.toString(), QStringLiteral("4.3"));
    }

    void validRuntimeProfileUpdateIsNormalizedAtomically()
    {
        const oap::aa::ProjectedClusterProfile baseline;
        oap::aa::ProjectedClusterProfile updated;
        QString error;
        const QVariantMap update{
            {QStringLiteral("gal_version"), QStringLiteral("4.3")},
            {QStringLiteral("resolution"), QStringLiteral("720p")},
            {QStringLiteral("dpi"), 160},
            {QStringLiteral("content_width"), 600},
            {QStringLiteral("content_height"), 400},
            {QStringLiteral("native_turn_card_available"), true},
        };

        QVERIFY(oap::aa::applyProjectedClusterProfileUpdate(
            baseline, update, &updated, &error));
        QVERIFY(error.isEmpty());
        QCOMPARE(updated.galVersion, oap::aa::kGalVersion4_3);
        QCOMPARE(updated.resolution, QStringLiteral("720p"));
        QCOMPARE(updated.dpi, 160);
        QCOMPARE(updated.contentWidth, 600);
        QCOMPARE(updated.contentHeight, 400);
        QVERIFY(updated.nativeTurnCardAvailable);
        QCOMPARE(updated.geometry(),
                 (oap::aa::ProjectedViewportGeometry{1280, 720, 600, 400}));
        QCOMPARE(updated.geometry().marginWidth(), 680);
        QCOMPARE(updated.geometry().marginHeight(), 320);
    }

    void runtimeProfileRejectsUnsupportedAndIncompatibleGalAtomically()
    {
        const oap::aa::ProjectedClusterProfile baseline;
        oap::aa::ProjectedClusterProfile updated = baseline;
        updated.dpi = 200;
        QString error;

        QVERIFY(!oap::aa::applyProjectedClusterProfileUpdate(
            baseline,
            {{QStringLiteral("gal_version"), QStringLiteral("5.0")},
             {QStringLiteral("dpi"), 160}},
            &updated, &error));
        QCOMPARE(updated.dpi, 200);
        QCOMPARE(updated.galVersion, oap::aa::kGalVersion1_7);

        QVERIFY(!oap::aa::applyProjectedClusterProfileUpdate(
            baseline,
            {{QStringLiteral("native_turn_card_available"), true}},
            &updated, &error));
        QCOMPARE(updated.dpi, 200);
        QCOMPARE(updated.galVersion, oap::aa::kGalVersion1_7);
        QVERIFY(!updated.nativeTurnCardAvailable);
    }

    void invalidRuntimeProfileUpdateDoesNotPartiallyMutate()
    {
        const oap::aa::ProjectedClusterProfile baseline;
        oap::aa::ProjectedClusterProfile updated{
            QStringLiteral("720p"), 200, 500, 400, true};
        QString error;

        QVERIFY(!oap::aa::applyProjectedClusterProfileUpdate(
            baseline,
            {{QStringLiteral("dpi"), 160},
             {QStringLiteral("content_width"), 301}},
            &updated, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(updated,
                 (oap::aa::ProjectedClusterProfile{
                     QStringLiteral("720p"), 200, 500, 400, true}));

        QVERIFY(!oap::aa::applyProjectedClusterProfileUpdate(
            baseline, {}, &updated, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(TestProjectedDisplayConfig)
#include "test_projected_display_config.moc"
