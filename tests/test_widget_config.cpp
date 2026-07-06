// tests/test_widget_config.cpp -- YAML v4 dashboards() placement persistence tests
//
// Ported from the flat v3 gridPlacements()/gridNextInstanceId() API to the v4
// dashboards()/setDashboards() API (see docs/.../task-1-brief.md). Behavioral
// coverage preserved 1:1 except where explicitly noted as folded/dropped
// because it duplicated coverage added elsewhere (test_yaml_config.cpp's new
// v4 slots, or another test in this file).
#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "core/YamlConfig.hpp"
#include "core/widget/WidgetTypes.hpp"

class TestWidgetConfig : public QObject {
    Q_OBJECT
private slots:
    void testDashboardPlacementsFromLegacyFixture();
    void testDashboardsSetInMemory();
    void testDashboardsMultiPlacementRoundTrip();
    void testMissingWidgetGridDefaultsToHomeDashboard();
    void testOldWidgetConfigIgnoredByDashboards();
    void testDashboardsConfigMapRoundTrip();
    void testDashboardsEmptyConfigOmitted();
};

// Ported from testGridPlacementsFromYaml. The fixture was a literal pre-v4
// flat widget_grid file (version bumped 2->3 so migrateWidgetGridV3() picks
// it up -- v2 was never a recognized/migrated format, only v3 is). Exercises
// the *read*-side config-value type sniffing (string "12h", bool true) via a
// real on-disk file rather than a value round-tripped through setDashboards().
void TestWidgetConfig::testDashboardPlacementsFromLegacyFixture() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    auto ds = config.dashboards();
    QCOMPARE(ds.size(), 1);
    QCOMPARE(ds[0].id, QString("home"));
    auto placements = ds[0].placements;
    QCOMPARE(placements.size(), 2);
    QCOMPARE(placements[0].instanceId, QString("clock-0"));
    QCOMPARE(placements[0].widgetId, QString("org.openauto.clock"));
    QCOMPARE(placements[0].col, 0);
    QCOMPARE(placements[0].row, 0);
    QCOMPARE(placements[0].colSpan, 2);
    QCOMPARE(placements[0].rowSpan, 2);
    QCOMPARE(placements[0].opacity, 0.25);
    // Verify config was loaded from YAML
    QCOMPARE(placements[0].config["format"].toString(), QString("12h"));
    QCOMPARE(placements[0].config["showSeconds"].toBool(), true);

    QCOMPARE(placements[1].instanceId, QString("status-1"));
    QCOMPARE(placements[1].widgetId, QString("org.openauto.aa-status"));
    QCOMPARE(placements[1].col, 0);
    QCOMPARE(placements[1].row, 2);
    QCOMPARE(placements[1].colSpan, 2);
    QCOMPARE(placements[1].rowSpan, 1);
}

// Ported from testSetGridPlacements (in-memory set+get, no file round trip)
// and testGridNextInstanceId's "set then get" half (its default-value half
// is folded into test_yaml_config.cpp's testDashboardDefaultsNextInstanceIdZero).
void TestWidgetConfig::testDashboardsSetInMemory() {
    oap::YamlConfig config;

    oap::DashboardConfig home;
    home.id = "home";
    home.name = "Home";
    home.nextInstanceId = 42;
    oap::GridPlacement p;
    p.instanceId = "test-0";
    p.widgetId = "org.openauto.clock";
    p.col = 1;
    p.row = 2;
    p.colSpan = 3;
    p.rowSpan = 1;
    p.opacity = 0.5;
    home.placements.append(p);

    config.setDashboards({home});

    auto ds = config.dashboards();
    QCOMPARE(ds.size(), 1);
    QCOMPARE(ds[0].nextInstanceId, 42);
    QCOMPARE(ds[0].placements.size(), 1);
    QCOMPARE(ds[0].placements[0].instanceId, QString("test-0"));
    QCOMPARE(ds[0].placements[0].col, 1);
    QCOMPARE(ds[0].placements[0].row, 2);
    QCOMPARE(ds[0].placements[0].colSpan, 3);
    QCOMPARE(ds[0].placements[0].rowSpan, 1);
    QCOMPARE(ds[0].placements[0].opacity, 0.5);
}

// Ported from testGridPlacementsRoundTrip (+ testGridNextInstanceIdRoundTrip's
// persistence check, folded in here via nextInstanceId). Two placements in a
// single dashboard survive save/reload with all fields intact.
void TestWidgetConfig::testDashboardsMultiPlacementRoundTrip() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    oap::YamlConfig config;

    oap::DashboardConfig home;
    home.id = "home";
    home.name = "Home";
    home.nextInstanceId = 5;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0";
        p.widgetId = "org.openauto.clock";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.3;
        home.placements.append(p);
    }
    {
        oap::GridPlacement p;
        p.instanceId = "np-1";
        p.widgetId = "org.openauto.bt-now-playing";
        p.col = 2; p.row = 0;
        p.colSpan = 3; p.rowSpan = 2;
        p.opacity = 0.25;
        home.placements.append(p);
    }

    config.setDashboards({home});
    config.save(tmpPath);

    oap::YamlConfig reloaded;
    reloaded.load(tmpPath);
    auto ds = reloaded.dashboards();
    QCOMPARE(ds.size(), 1);
    auto result = ds[0].placements;
    QCOMPARE(result.size(), 2);
    QCOMPARE(result[0].instanceId, QString("clock-0"));
    QCOMPARE(result[0].widgetId, QString("org.openauto.clock"));
    QCOMPARE(result[0].col, 0);
    QCOMPARE(result[0].row, 0);
    QCOMPARE(result[0].colSpan, 2);
    QCOMPARE(result[0].rowSpan, 2);
    QCOMPARE(result[0].opacity, 0.3);

    QCOMPARE(result[1].instanceId, QString("np-1"));
    QCOMPARE(result[1].col, 2);
    QCOMPARE(result[1].colSpan, 3);
    QCOMPARE(result[1].rowSpan, 2);

    QCOMPARE(ds[0].nextInstanceId, 5);
}

// Ported from testMissingWidgetGridReturnsEmpty. A config file with no
// widget_grid section at all should fall back cleanly to the default single
// "home" dashboard with no placements (not crash, not fabricate garbage).
void TestWidgetConfig::testMissingWidgetGridDefaultsToHomeDashboard() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    tmpFile.write("# empty config\n");
    tmpFile.close();

    oap::YamlConfig config;
    config.load(tmpFile.fileName());

    auto ds = config.dashboards();
    QCOMPARE(ds.size(), 1);
    QCOMPARE(ds[0].id, QString("home"));
    QVERIFY(ds[0].placements.isEmpty());
}

// Ported from testOldWidgetConfigIgnored. A file carrying only the legacy
// pane-based widget_config key (no widget_grid key) must not be misread as
// grid data, and must not crash the loader/migration.
void TestWidgetConfig::testOldWidgetConfigIgnoredByDashboards() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    tmpFile.write(
        "widget_config:\n"
        "  version: 1\n"
        "  pages:\n"
        "    - id: home\n"
        "  placements:\n"
        "    - instanceId: old-1\n"
        "      widgetId: org.openauto.clock\n"
        "      pageId: home\n"
        "      paneId: main\n"
    );
    tmpFile.close();

    oap::YamlConfig config;
    config.load(tmpFile.fileName());

    // Should fall back to the default single home dashboard with no placements.
    auto ds = config.dashboards();
    QCOMPARE(ds.size(), 1);
    QVERIFY(ds[0].placements.isEmpty());
}

// Ported from testGridPlacementsConfigRoundTrip, extended to also cover the
// int/double branches of the config-value type sniffing (originally only
// string+bool were exercised here). This hits the *write*-side type switch
// in placementsToNode(), complementing the read-side coverage in
// testDashboardPlacementsFromLegacyFixture above.
void TestWidgetConfig::testDashboardsConfigMapRoundTrip() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    oap::YamlConfig config;

    oap::DashboardConfig home;
    home.id = "home";
    home.name = "Home";
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0";
        p.widgetId = "org.openauto.clock";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25;
        p.config = {{"format", "12h"}, {"showSeconds", true},
                    {"refreshMinutes", 5}, {"gain", 1.5}};
        home.placements.append(p);
    }

    config.setDashboards({home});
    config.save(tmpPath);

    oap::YamlConfig reloaded;
    reloaded.load(tmpPath);
    auto result = reloaded.dashboards()[0].placements;
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].config.size(), 4);
    QCOMPARE(result[0].config["format"].toString(), QString("12h"));
    QCOMPARE(result[0].config["showSeconds"].toBool(), true);
    QCOMPARE(result[0].config["refreshMinutes"].toInt(), 5);
    QCOMPARE(result[0].config["gain"].toDouble(), 1.5);
}

// Ported from testGridPlacementsEmptyConfigOmitted.
void TestWidgetConfig::testDashboardsEmptyConfigOmitted() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    oap::YamlConfig config;

    oap::DashboardConfig home;
    home.id = "home";
    home.name = "Home";
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0";
        p.widgetId = "org.openauto.clock";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 2;
        // config is empty (default)
        home.placements.append(p);
    }

    config.setDashboards({home});
    config.save(tmpPath);

    // Read raw YAML text and verify no "config:" key within the widget_grid section
    QFile file(tmpPath);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QString text = QString::fromUtf8(file.readAll());

    // Extract the widget_grid section only (between "widget_grid:" and next top-level key)
    int gridStart = text.indexOf("widget_grid:");
    QVERIFY(gridStart >= 0);
    // Find next top-level key (line starting without spaces after widget_grid section)
    int gridEnd = text.indexOf(QRegularExpression("\n[a-z]"), gridStart + 12);
    QString gridSection = (gridEnd > 0) ? text.mid(gridStart, gridEnd - gridStart)
                                        : text.mid(gridStart);
    QVERIFY(!gridSection.contains("config:"));
}

QTEST_GUILESS_MAIN(TestWidgetConfig)
#include "test_widget_config.moc"
