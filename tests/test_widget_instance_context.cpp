// tests/test_widget_instance_context.cpp
#include <QtTest/QtTest>
#include "ui/WidgetInstanceContext.hpp"
#include "core/widget/WidgetTypes.hpp"

class TestWidgetInstanceContext : public QObject {
    Q_OBJECT
private slots:
    void testProperties();
    void testPaneSizeExposed();
};

void TestWidgetInstanceContext::testProperties() {
    oap::WidgetPlacement placement;
    placement.instanceId = "clock-main";
    placement.widgetId = "org.openauto.clock";
    placement.pageId = "home";
    placement.paneId = "main";

    oap::WidgetInstanceContext ctx(placement, oap::WidgetSize::Main, nullptr, this);

    QCOMPARE(ctx.instanceId(), "clock-main");
    QCOMPARE(ctx.widgetId(), "org.openauto.clock");
    QCOMPARE(ctx.paneSize(), oap::WidgetSize::Main);
}

void TestWidgetInstanceContext::testPaneSizeExposed() {
    oap::WidgetPlacement placement;
    placement.instanceId = "test";
    placement.paneId = "sub1";

    oap::WidgetInstanceContext ctx(placement, oap::WidgetSize::Sub, nullptr, this);

    // Verify QML-accessible paneSize property
    QCOMPARE(ctx.property("paneSize").toInt(), static_cast<int>(oap::WidgetSize::Sub));
}

QTEST_GUILESS_MAIN(TestWidgetInstanceContext)
#include "test_widget_instance_context.moc"
