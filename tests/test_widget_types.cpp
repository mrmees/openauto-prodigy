// tests/test_widget_types.cpp
#include <QtTest/QtTest>
#include "core/widget/WidgetTypes.hpp"

class TestWidgetTypes : public QObject {
    Q_OBJECT
private slots:
    void testWidgetDescriptorDefaults();
    void testWidgetDescriptorGridSpans();
    void testGridPlacementDefaults();
    void testGridPlacementFields();
    void testLegacyWidgetPlacementCompositeKey();
    void testLegacyPageDescriptorDefaults();
    void testContributionKindDefaultsToWidget();
    void testContributionKindCanBeSet();
};

void TestWidgetTypes::testWidgetDescriptorDefaults() {
    oap::WidgetDescriptor desc;
    QVERIFY(desc.id.isEmpty());
    QVERIFY(desc.pluginId.isEmpty());
    QVERIFY(desc.defaultConfig.isEmpty());
    QCOMPARE(desc.minCols, 1);
    QCOMPARE(desc.minRows, 1);
    QCOMPARE(desc.maxCols, 6);
    QCOMPARE(desc.maxRows, 4);
    QCOMPARE(desc.defaultCols, 1);
    QCOMPARE(desc.defaultRows, 1);
}

void TestWidgetTypes::testWidgetDescriptorGridSpans() {
    oap::WidgetDescriptor desc;
    desc.id = "org.test.clock";
    desc.minCols = 1; desc.minRows = 1;
    desc.maxCols = 6; desc.maxRows = 4;
    desc.defaultCols = 2; desc.defaultRows = 2;

    QCOMPARE(desc.minCols, 1);
    QCOMPARE(desc.minRows, 1);
    QCOMPARE(desc.maxCols, 6);
    QCOMPARE(desc.maxRows, 4);
    QCOMPARE(desc.defaultCols, 2);
    QCOMPARE(desc.defaultRows, 2);
}

void TestWidgetTypes::testGridPlacementDefaults() {
    oap::GridPlacement p;
    QVERIFY(p.instanceId.isEmpty());
    QVERIFY(p.widgetId.isEmpty());
    QCOMPARE(p.col, 0);
    QCOMPARE(p.row, 0);
    QCOMPARE(p.colSpan, 1);
    QCOMPARE(p.rowSpan, 1);
    QCOMPARE(p.opacity, 0.25);
    QCOMPARE(p.visible, true);
}

void TestWidgetTypes::testGridPlacementFields() {
    oap::GridPlacement p;
    p.instanceId = "clock-0";
    p.widgetId = "org.openauto.clock";
    p.col = 2;
    p.row = 1;
    p.colSpan = 3;
    p.rowSpan = 2;
    p.opacity = 0.5;
    p.visible = false;

    QCOMPARE(p.instanceId, "clock-0");
    QCOMPARE(p.widgetId, "org.openauto.clock");
    QCOMPARE(p.col, 2);
    QCOMPARE(p.row, 1);
    QCOMPARE(p.colSpan, 3);
    QCOMPARE(p.rowSpan, 2);
    QCOMPARE(p.opacity, 0.5);
    QCOMPARE(p.visible, false);
}

void TestWidgetTypes::testLegacyWidgetPlacementCompositeKey() {
    oap::WidgetPlacement p;
    p.pageId = "home";
    p.paneId = "main";
    QCOMPARE(p.compositeKey(), QStringLiteral("home:main"));
}

void TestWidgetTypes::testLegacyPageDescriptorDefaults() {
    oap::PageDescriptor page;
    QVERIFY(page.id.isEmpty());
    QCOMPARE(page.layoutTemplate, QStringLiteral("standard-3pane"));
    QCOMPARE(page.order, 0);
    QVERIFY(page.flags.isEmpty());
}

void TestWidgetTypes::testContributionKindDefaultsToWidget() {
    oap::WidgetDescriptor desc;
    QCOMPARE(desc.contributionKind, oap::DashboardContributionKind::Widget);
}

void TestWidgetTypes::testContributionKindCanBeSet() {
    oap::WidgetDescriptor desc;
    desc.contributionKind = oap::DashboardContributionKind::LiveSurfaceWidget;
    QCOMPARE(desc.contributionKind, oap::DashboardContributionKind::LiveSurfaceWidget);
}

QTEST_GUILESS_MAIN(TestWidgetTypes)
#include "test_widget_types.moc"
