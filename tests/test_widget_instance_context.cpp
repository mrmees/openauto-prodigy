// tests/test_widget_instance_context.cpp
#include <QtTest/QtTest>
#include "ui/WidgetInstanceContext.hpp"
#include "core/widget/WidgetTypes.hpp"

class TestWidgetInstanceContext : public QObject {
    Q_OBJECT
private slots:
    void testProperties();
    void testCellDimensions();
};

void TestWidgetInstanceContext::testProperties() {
    oap::GridPlacement placement;
    placement.instanceId = "clock-0";
    placement.widgetId = "org.openauto.clock";
    placement.col = 0;
    placement.row = 0;
    placement.colSpan = 2;
    placement.rowSpan = 2;

    oap::WidgetInstanceContext ctx(placement, 310, 290, nullptr, this);

    QCOMPARE(ctx.instanceId(), "clock-0");
    QCOMPARE(ctx.widgetId(), "org.openauto.clock");
    QCOMPARE(ctx.cellWidth(), 310);
    QCOMPARE(ctx.cellHeight(), 290);
}

void TestWidgetInstanceContext::testCellDimensions() {
    oap::GridPlacement placement;
    placement.instanceId = "test";
    placement.col = 1;
    placement.row = 1;

    oap::WidgetInstanceContext ctx(placement, 155, 145, nullptr, this);

    // Verify QML-accessible properties
    QCOMPARE(ctx.property("cellWidth").toInt(), 155);
    QCOMPARE(ctx.property("cellHeight").toInt(), 145);
}

QTEST_GUILESS_MAIN(TestWidgetInstanceContext)
#include "test_widget_instance_context.moc"
