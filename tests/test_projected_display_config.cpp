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
};

QTEST_MAIN(TestProjectedDisplayConfig)
#include "test_projected_display_config.moc"
