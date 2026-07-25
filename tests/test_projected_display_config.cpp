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
};

QTEST_MAIN(TestProjectedDisplayConfig)
#include "test_projected_display_config.moc"
