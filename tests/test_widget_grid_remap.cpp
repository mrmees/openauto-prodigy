// tests/test_widget_grid_remap.cpp -- Proportional remap algorithm tests
#include <QtTest>
#include "ui/WidgetGridModel.hpp"
#include "core/widget/WidgetRegistry.hpp"

class TestWidgetGridRemap : public QObject {
    Q_OBJECT
private:
    oap::WidgetRegistry* makeRegistry() {
        auto* reg = new oap::WidgetRegistry(this);
        {
            oap::WidgetDescriptor d;
            d.id = "clock";
            d.displayName = "Clock";
            d.qmlComponent = QUrl("qrc:/ClockWidget.qml");
            d.minCols = 1; d.minRows = 1;
            d.maxCols = 6; d.maxRows = 4;
            d.defaultCols = 2; d.defaultRows = 2;
            reg->registerWidget(d);
        }
        {
            oap::WidgetDescriptor d;
            d.id = "tiny";
            d.displayName = "Tiny";
            d.qmlComponent = QUrl("qrc:/TinyWidget.qml");
            d.minCols = 1; d.minRows = 1;
            d.maxCols = 2; d.maxRows = 2;
            d.defaultCols = 1; d.defaultRows = 1;
            reg->registerWidget(d);
        }
        {
            // Widget with large minSpan (for min-span-exceeds-grid test)
            oap::WidgetDescriptor d;
            d.id = "bigmin";
            d.displayName = "BigMin";
            d.qmlComponent = QUrl("qrc:/BigMinWidget.qml");
            d.minCols = 4; d.minRows = 3;
            d.maxCols = 6; d.maxRows = 4;
            d.defaultCols = 4; d.defaultRows = 3;
            reg->registerWidget(d);
        }
        return reg;
    }

    // Helper: create a model with base placements and saved dims, then trigger remap
    oap::WidgetGridModel* setupForRemap(oap::WidgetRegistry* reg,
                                         const QList<oap::GridPlacement>& base,
                                         int savedCols, int savedRows) {
        auto* model = new oap::WidgetGridModel(reg, this);
        model->setPlacements(base, reg);
        model->setSavedDimensions(savedCols, savedRows);
        return model;
    }

private slots:
    // Same dims: live = base (identity copy, no remap)
    void testSameDimsIdentity();
    // Grow 6x4 -> 8x5: widget at (3,2) proportionally repositioned
    void testGrowProportional();
    // Grow: spans stay same size (2x2 stays 2x2)
    void testGrowPreservesSpans();
    // Shrink 8x5 -> 6x4: widget clamped within bounds
    void testShrinkClampsPosition();
    // Overlap after remap: second widget nudged to adjacent free cell
    void testOverlapNudge();
    // No fit on page: widget spilled to next page at (0,0)
    void testPageSpill();
    // Min span exceeds grid: widget marked visible=false
    void testMinSpanExceedsGrid();
    // Repeated resize (small->large->small): same layout as direct small (base snapshot prevents drift)
    void testNoDrift();
    // Edit mode: setGridDimensions during edit mode does NOT remap; remap applies on exit
    void testEditModeDeferral();
    // Save updates base snapshot and saved dims
    void testSaveUpdatesBase();
    // Boot guard: setGridDimensions before setPlacements/setSavedDimensions just stores dims
    void testBootGuardNoCrash();
    // Boot guard: setPlacements + setSavedDimensions(0,0) + setGridDimensions adopts current dims
    void testBootGuardFirstTimeSetup();
};

void TestWidgetGridRemap::testSameDimsIdentity()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0"; p.widgetId = "clock";
        p.col = 2; p.row = 1; p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(6, 4); // same dims

    auto live = model->placements();
    QCOMPARE(live.size(), 1);
    QCOMPARE(live[0].col, 2);
    QCOMPARE(live[0].row, 1);
    QCOMPARE(live[0].colSpan, 2);
    QCOMPARE(live[0].rowSpan, 2);
    QVERIFY(live[0].visible);
}

void TestWidgetGridRemap::testGrowProportional()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0"; p.widgetId = "clock";
        p.col = 3; p.row = 2; p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(8, 5); // grow

    auto live = model->placements();
    QCOMPARE(live.size(), 1);
    // Proportional: newCol = round(3 * 8/6) = round(4.0) = 4
    // Proportional: newRow = round(2 * 5/4) = round(2.5) = 3  (qRound rounds 0.5 up)
    QCOMPARE(live[0].col, 4);
    QCOMPARE(live[0].row, 3); // 2*5/4 = 2.5 -> 3
    QVERIFY(live[0].visible);
}

void TestWidgetGridRemap::testGrowPreservesSpans()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0"; p.widgetId = "clock";
        p.col = 0; p.row = 0; p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(10, 8);

    auto live = model->placements();
    QCOMPARE(live[0].colSpan, 2); // NOT scaled
    QCOMPARE(live[0].rowSpan, 2); // NOT scaled
}

void TestWidgetGridRemap::testShrinkClampsPosition()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0"; p.widgetId = "clock";
        p.col = 7; p.row = 4; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 8, 5);
    model->setGridDimensions(6, 4); // shrink

    auto live = model->placements();
    QCOMPARE(live.size(), 1);
    // Proportional: newCol = round(7*6/8) = round(5.25) = 5
    // Clamp: col = min(5, max(0, 6-1)) = min(5, 5) = 5
    QCOMPARE(live[0].col, 5);
    // Proportional: newRow = round(4*4/5) = round(3.2) = 3
    // Clamp: row = min(3, max(0, 4-1)) = min(3, 3) = 3
    QCOMPARE(live[0].row, 3);
    QVERIFY(live[0].visible);
}

void TestWidgetGridRemap::testOverlapNudge()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    // Two widgets that will map to the same position after shrink
    {
        oap::GridPlacement p;
        p.instanceId = "tiny-0"; p.widgetId = "tiny";
        p.col = 0; p.row = 0; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }
    {
        oap::GridPlacement p;
        p.instanceId = "tiny-1"; p.widgetId = "tiny";
        p.col = 1; p.row = 0; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    // Both proportional to col 0 on a 3-col grid:
    // tiny-0: round(0*3/6) = 0
    // tiny-1: round(1*3/6) = round(0.5) = 1 (qRound rounds 0.5 up to 1)
    // Actually both fit at different positions with 3 cols, so use 2 cols
    model->setGridDimensions(2, 4);
    // tiny-0: round(0*2/6) = 0
    // tiny-1: round(1*2/6) = round(0.333) = 0 -> overlap -> nudge

    auto live = model->placements();
    QCOMPARE(live.size(), 2);
    QVERIFY(live[0].visible);
    QVERIFY(live[1].visible);
    // First widget at (0,0), second nudged away
    QCOMPARE(live[0].col, 0);
    QCOMPARE(live[0].row, 0);
    // Second should be nudged to a different position (not at 0,0)
    QVERIFY(live[1].col != 0 || live[1].row != 0);
}

void TestWidgetGridRemap::testPageSpill()
{
    auto* reg = makeRegistry();
    // Fill a 2x1 grid with 3 widgets -- third must spill to next page
    QList<oap::GridPlacement> base;
    for (int i = 0; i < 3; ++i) {
        oap::GridPlacement p;
        p.instanceId = "tiny-" + QString::number(i);
        p.widgetId = "tiny";
        p.col = i; p.row = 0; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(2, 1); // very small grid

    auto live = model->placements();
    QCOMPARE(live.size(), 3);
    // At least one must be on page > 0
    bool anySpilled = false;
    for (const auto& p : live) {
        if (p.page > 0) {
            anySpilled = true;
            QVERIFY(p.visible);
        }
    }
    QVERIFY(anySpilled);
}

void TestWidgetGridRemap::testMinSpanExceedsGrid()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "bigmin-0"; p.widgetId = "bigmin";
        p.col = 0; p.row = 0; p.colSpan = 4; p.rowSpan = 3;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(3, 2); // smaller than minCols=4, minRows=3

    auto live = model->placements();
    QCOMPARE(live.size(), 1);
    QVERIFY(!live[0].visible); // hidden, not deleted
}

void TestWidgetGridRemap::testNoDrift()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0"; p.widgetId = "clock";
        p.col = 3; p.row = 2; p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    // Resize from 6x4 -> 10x8 -> 6x4 should produce same result as direct 6x4
    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(10, 8);
    model->setGridDimensions(6, 4);

    auto live = model->placements();
    // Should match original base (identity: same dims as saved)
    QCOMPARE(live[0].col, 3);
    QCOMPARE(live[0].row, 2);
    QCOMPARE(live[0].colSpan, 2);
    QCOMPARE(live[0].rowSpan, 2);
}

void TestWidgetGridRemap::testEditModeDeferral()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0"; p.widgetId = "clock";
        p.col = 3; p.row = 2; p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(6, 4); // initial -- identity

    model->setWidgetSelected(true);
    model->setGridDimensions(8, 5); // should NOT remap yet

    // Placements should still be at original positions
    auto live = model->placements();
    QCOMPARE(live[0].col, 3);
    QCOMPARE(live[0].row, 2);

    model->setWidgetSelected(false); // should apply deferred remap

    live = model->placements();
    // Now should be proportionally remapped 6x4 -> 8x5
    QCOMPARE(live[0].col, 4); // round(3 * 8/6) = 4
    QCOMPARE(live[0].row, 3); // round(2 * 5/4) = 3 (2.5 rounds up)
}

void TestWidgetGridRemap::testSaveUpdatesBase()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0"; p.widgetId = "clock";
        p.col = 0; p.row = 0; p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(6, 4);

    // Simulate user edit: move widget
    model->moveWidget("clock-0", 3, 1);

    // Now resize -- should remap from NEW base (3,1), not original (0,0)
    model->setGridDimensions(8, 5);

    auto live = model->placements();
    // Proportional from (3,1) on 6x4 -> 8x5: col=round(3*8/6)=4, row=round(1*5/4)=1
    QCOMPARE(live[0].col, 4);
    QCOMPARE(live[0].row, 1);
}

void TestWidgetGridRemap::testBootGuardNoCrash()
{
    auto* reg = makeRegistry();
    auto* model = new oap::WidgetGridModel(reg, this);

    // setGridDimensions BEFORE setPlacements and setSavedDimensions
    // Should not crash, should not produce empty grid, should just store dims
    model->setGridDimensions(6, 4);

    QCOMPARE(model->gridColumns(), 6);
    QCOMPARE(model->gridRows(), 4);
    QCOMPARE(model->placements().size(), 0); // no placements yet -- that's fine
}

void TestWidgetGridRemap::testBootGuardFirstTimeSetup()
{
    auto* reg = makeRegistry();
    auto* model = new oap::WidgetGridModel(reg, this);

    // Simulate first-time setup: placements exist but savedCols=0 (never saved)
    QList<oap::GridPlacement> base;
    {
        oap::GridPlacement p;
        p.instanceId = "clock-0"; p.widgetId = "clock";
        p.col = 2; p.row = 1; p.colSpan = 2; p.rowSpan = 2;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }
    model->setPlacements(base, reg);
    model->setSavedDimensions(0, 0); // never saved

    model->setGridDimensions(6, 4); // QML provides actual dims

    auto live = model->placements();
    QCOMPARE(live.size(), 1);
    // Should adopt current dims as baseline -- no remap, positions unchanged
    QCOMPARE(live[0].col, 2);
    QCOMPARE(live[0].row, 1);
    QVERIFY(live[0].visible);
}

QTEST_GUILESS_MAIN(TestWidgetGridRemap)
#include "test_widget_grid_remap.moc"
