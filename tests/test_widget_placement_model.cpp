#include <QtTest/QtTest>
#include <QSignalSpy>
#include "ui/WidgetPlacementModel.hpp"
#include "core/widget/WidgetRegistry.hpp"

class TestWidgetPlacementModel : public QObject {
    Q_OBJECT
private slots:
    void testPlacementForPane();
    void testSwapWidget();
    void testClearPane();
    void testActivePageFilter();
    void testQmlComponentUrl();
};

void TestWidgetPlacementModel::testPlacementForPane() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor clockDesc;
    clockDesc.id = "org.openauto.clock";
    clockDesc.displayName = "Clock";
    clockDesc.qmlComponent = QUrl("qrc:/widgets/ClockWidget.qml");
    clockDesc.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;
    registry.registerWidget(clockDesc);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "clock-main";
    p.widgetId = "org.openauto.clock";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    auto result = model.placementForPane("main");
    QVERIFY(result.has_value());
    QCOMPARE(result->widgetId, "org.openauto.clock");
}

void TestWidgetPlacementModel::testSwapWidget() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d1;
    d1.id = "w1";
    d1.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d1);

    oap::WidgetDescriptor d2;
    d2.id = "w2";
    d2.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d2);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    QSignalSpy spy(&model, &oap::WidgetPlacementModel::placementsChanged);
    model.swapWidget("main", "w2");

    QCOMPARE(spy.count(), 1);
    auto result = model.placementForPane("main");
    QVERIFY(result.has_value());
    QCOMPARE(result->widgetId, "w2");
}

void TestWidgetPlacementModel::testClearPane() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    model.clearPane("main");
    QVERIFY(!model.placementForPane("main").has_value());
}

void TestWidgetPlacementModel::testActivePageFilter() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;

    oap::WidgetPlacement p1;
    p1.instanceId = "w1-home";
    p1.widgetId = "w1";
    p1.pageId = "home";
    p1.paneId = "main";
    placements.append(p1);

    oap::WidgetPlacement p2;
    p2.instanceId = "w1-page2";
    p2.widgetId = "w1";
    p2.pageId = "page2";
    p2.paneId = "main";
    placements.append(p2);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);

    model.setActivePageId("home");
    QVERIFY(model.placementForPane("main").has_value());
    QCOMPARE(model.placementForPane("main")->pageId, "home");

    model.setActivePageId("page2");
    QVERIFY(model.placementForPane("main").has_value());
    QCOMPARE(model.placementForPane("main")->pageId, "page2");
}

void TestWidgetPlacementModel::testQmlComponentUrl() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor d;
    d.id = "w1";
    d.qmlComponent = QUrl("qrc:/widgets/TestWidget.qml");
    d.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(d);

    QList<oap::WidgetPlacement> placements;
    oap::WidgetPlacement p;
    p.instanceId = "w1-main";
    p.widgetId = "w1";
    p.pageId = "home";
    p.paneId = "main";
    placements.append(p);

    oap::WidgetPlacementModel model(&registry);
    model.setPlacements(placements);
    model.setActivePageId("home");

    QCOMPARE(model.qmlComponentForPane("main"), QUrl("qrc:/widgets/TestWidget.qml"));
}

QTEST_GUILESS_MAIN(TestWidgetPlacementModel)
#include "test_widget_placement_model.moc"
