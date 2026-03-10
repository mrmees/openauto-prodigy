// tests/test_widget_config.cpp
#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "core/YamlConfig.hpp"
#include "core/widget/WidgetTypes.hpp"

class TestWidgetConfig : public QObject {
    Q_OBJECT
private slots:
    void testLoadPages();
    void testLoadPlacements();
    void testSavePlacements();
    void testDefaultsWhenEmpty();
    void testConfigVersion();
    void testPlacementValidation();
};

void TestWidgetConfig::testLoadPages() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    auto pages = config.widgetPages();
    QCOMPARE(pages.size(), 1);
    QCOMPARE(pages[0].id, "home");
    QCOMPARE(pages[0].layoutTemplate, "standard-3pane");
    QCOMPARE(pages[0].order, 0);
}

void TestWidgetConfig::testLoadPlacements() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    auto placements = config.widgetPlacements();
    QCOMPARE(placements.size(), 3);
    QCOMPARE(placements[0].instanceId, "clock-main");
    QCOMPARE(placements[0].widgetId, "org.openauto.clock");
    QCOMPARE(placements[0].pageId, "home");
    QCOMPARE(placements[0].paneId, "main");
}

void TestWidgetConfig::testSavePlacements() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    QString tmpPath = tmpFile.fileName();
    tmpFile.close();

    // Load, modify, save, reload
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    auto placements = config.widgetPlacements();
    placements[0].widgetId = "org.openauto.weather";
    config.setWidgetPlacements(placements);
    config.save(tmpPath);

    oap::YamlConfig reloaded;
    reloaded.load(tmpPath);
    auto reloadedPlacements = reloaded.widgetPlacements();
    QCOMPARE(reloadedPlacements[0].widgetId, "org.openauto.weather");
}

void TestWidgetConfig::testDefaultsWhenEmpty() {
    QTemporaryFile tmpFile;
    tmpFile.open();
    tmpFile.write("# empty config\n");
    tmpFile.close();

    oap::YamlConfig config;
    config.load(tmpFile.fileName());

    auto pages = config.widgetPages();
    QCOMPARE(pages.size(), 1);
    QCOMPARE(pages[0].id, "home");

    auto placements = config.widgetPlacements();
    QCOMPARE(placements.size(), 3); // default: clock, aa-status, now-playing
}

void TestWidgetConfig::testConfigVersion() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");
    QCOMPARE(config.widgetConfigVersion(), 1);
}

void TestWidgetConfig::testPlacementValidation() {
    oap::YamlConfig config;
    config.load(TEST_DATA_DIR "/test_widget_config.yaml");

    // Try to set placements with duplicate composite key
    QList<oap::WidgetPlacement> bad;
    oap::WidgetPlacement p1;
    p1.instanceId = "a";
    p1.widgetId = "w1";
    p1.pageId = "home";
    p1.paneId = "main";
    bad.append(p1);

    oap::WidgetPlacement p2;
    p2.instanceId = "b";
    p2.widgetId = "w2";
    p2.pageId = "home";
    p2.paneId = "main"; // duplicate page:pane
    bad.append(p2);

    // Should keep only first placement per composite key
    config.setWidgetPlacements(bad);
    auto result = config.widgetPlacements();
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0].instanceId, "a");
}

QTEST_GUILESS_MAIN(TestWidgetConfig)
#include "test_widget_config.moc"
