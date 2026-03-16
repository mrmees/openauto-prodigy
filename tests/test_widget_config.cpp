// tests/test_widget_config.cpp -- YAML grid placement persistence tests
#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "core/YamlConfig.hpp"
#include "core/widget/WidgetTypes.hpp"

class TestWidgetConfig : public QObject {
    Q_OBJECT
private slots:
    void testGridPlacementsFromYaml();
    void testSetGridPlacements();
    void testGridPlacementsRoundTrip();
    void testMissingWidgetGridReturnsEmpty();
    void testOldWidgetConfigIgnored();
    void testGridNextInstanceId();
    void testGridNextInstanceIdRoundTrip();
    void testGridPlacementsConfigRoundTrip();
    void testGridPlacementsEmptyConfigOmitted();
};

void TestWidgetConfig::testGridPlacementsFromYaml() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    auto placements = config.gridPlacements();
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

void TestWidgetConfig::testSetGridPlacements() {
    oap::YamlConfig config;

    QList<oap::GridPlacement> placements;
    oap::GridPlacement p;
    p.instanceId = "test-0";
    p.widgetId = "org.openauto.clock";
    p.col = 1;
    p.row = 2;
    p.colSpan = 3;
    p.rowSpan = 1;
    p.opacity = 0.5;
    placements.append(p);

    config.setGridPlacements(placements);

    auto result = config.gridPlacements();
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].instanceId, QString("test-0"));
    QCOMPARE(result[0].col, 1);
    QCOMPARE(result[0].row, 2);
    QCOMPARE(result[0].colSpan, 3);
    QCOMPARE(result[0].rowSpan, 1);
    QCOMPARE(result[0].opacity, 0.5);
}

void TestWidgetConfig::testGridPlacementsRoundTrip() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    oap::YamlConfig config;

    QList<oap::GridPlacement> placements;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0";
        p.widgetId = "org.openauto.clock";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.3;
        placements.append(p);
    }
    {
        oap::GridPlacement p;
        p.instanceId = "np-1";
        p.widgetId = "org.openauto.bt-now-playing";
        p.col = 2; p.row = 0;
        p.colSpan = 3; p.rowSpan = 2;
        p.opacity = 0.25;
        placements.append(p);
    }

    config.setGridPlacements(placements);
    config.setGridNextInstanceId(5);
    config.save(tmpPath);

    oap::YamlConfig reloaded;
    reloaded.load(tmpPath);
    auto result = reloaded.gridPlacements();
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

    QCOMPARE(reloaded.gridNextInstanceId(), 5);
}

void TestWidgetConfig::testMissingWidgetGridReturnsEmpty() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    tmpFile.write("# empty config\n");
    tmpFile.close();

    oap::YamlConfig config;
    config.load(tmpFile.fileName());

    auto placements = config.gridPlacements();
    QVERIFY(placements.isEmpty());
}

void TestWidgetConfig::testOldWidgetConfigIgnored() {
    // Old widget_config key should not crash or produce grid placements
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

    // Should return empty (no widget_grid key)
    auto placements = config.gridPlacements();
    QVERIFY(placements.isEmpty());
}

void TestWidgetConfig::testGridNextInstanceId() {
    oap::YamlConfig config;
    QCOMPARE(config.gridNextInstanceId(), 0);

    config.setGridNextInstanceId(42);
    QCOMPARE(config.gridNextInstanceId(), 42);
}

void TestWidgetConfig::testGridNextInstanceIdRoundTrip() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    oap::YamlConfig config;
    config.setGridNextInstanceId(10);
    config.save(tmpPath);

    oap::YamlConfig reloaded;
    reloaded.load(tmpPath);
    QCOMPARE(reloaded.gridNextInstanceId(), 10);
}

void TestWidgetConfig::testGridPlacementsConfigRoundTrip() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    oap::YamlConfig config;

    QList<oap::GridPlacement> placements;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0";
        p.widgetId = "org.openauto.clock";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25;
        p.config = {{"format", "12h"}, {"showSeconds", true}};
        placements.append(p);
    }

    config.setGridPlacements(placements);
    config.save(tmpPath);

    oap::YamlConfig reloaded;
    reloaded.load(tmpPath);
    auto result = reloaded.gridPlacements();
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].config.size(), 2);
    QCOMPARE(result[0].config["format"].toString(), QString("12h"));
    QCOMPARE(result[0].config["showSeconds"].toBool(), true);
}

void TestWidgetConfig::testGridPlacementsEmptyConfigOmitted() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    oap::YamlConfig config;

    QList<oap::GridPlacement> placements;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0";
        p.widgetId = "org.openauto.clock";
        p.col = 0; p.row = 0;
        p.colSpan = 2; p.rowSpan = 2;
        // config is empty (default)
        placements.append(p);
    }

    config.setGridPlacements(placements);
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
