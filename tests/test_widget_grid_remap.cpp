// tests/test_widget_grid_remap.cpp -- Proportional remap algorithm tests
#include <QtTest>
#include <algorithm>
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
            d.id = "launcher";
            d.displayName = "Launcher";
            d.qmlComponent = QUrl("qrc:/LauncherWidget.qml");
            d.minCols = 1; d.minRows = 1;
            d.maxCols = 1; d.maxRows = 1;
            d.defaultCols = 1; d.defaultRows = 1;
            d.singleton = true;
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
    void testPageSpillExpandsReachablePages();
    void testPageSpillKeepsReservedPageLast();
    void testSpillRoundTripRestoresPageBaseline();
    void testRepeatedSpillCyclesDoNotGrowPages();
    void testMutationPromotesSpillPageBaseline();
    void testLoadNormalizesReservedPageTail();
    // Min span exceeds grid: widget marked visible=false
    void testMinSpanExceedsGrid();
    void testHiddenPlacementRecoversWhenGridGrows();
    // Repeated resize (small->large->small): same layout as direct small (base snapshot prevents drift)
    void testNoDrift();
    // Edit mode: setGridDimensions during edit mode does NOT remap; remap applies on exit
    void testEditModeDeferral();
    void testPendingRemapBeforeMutation_data();
    void testPendingRemapBeforeMutation();
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

void TestWidgetGridRemap::testPageSpillExpandsReachablePages()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    for (int i = 0; i < 5; ++i) {
        oap::GridPlacement p;
        p.instanceId = "tiny-" + QString::number(i);
        p.widgetId = "tiny";
        p.col = i; p.row = 0; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setPageCount(2);
    bool placementsObservedReachable = false;
    connect(model, &oap::WidgetGridModel::placementsChanged, model, [&] {
        placementsObservedReachable = true;
        for (const auto& p : model->placements()) {
            if (p.visible)
                QVERIFY(p.page >= 0 && p.page < model->pageCount());
        }
    });

    model->setGridDimensions(1, 1);

    QVERIFY(placementsObservedReachable);
    QCOMPARE(model->pageCount(), 5);
    for (const auto& p : model->placements())
        QVERIFY(!p.visible || (p.page >= 0 && p.page < model->pageCount()));
}

void TestWidgetGridRemap::testPageSpillKeepsReservedPageLast()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    for (int i = 0; i < 3; ++i) {
        oap::GridPlacement p;
        p.instanceId = "tiny-" + QString::number(i);
        p.widgetId = "tiny";
        p.col = i; p.row = 0; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }
    oap::GridPlacement launcher;
    launcher.instanceId = "launcher-reserved";
    launcher.widgetId = "launcher";
    launcher.col = 0; launcher.row = 0;
    launcher.colSpan = 1; launcher.rowSpan = 1;
    launcher.opacity = 0.25; launcher.page = 1; launcher.visible = true;
    base.append(launcher);

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setPageCount(2);
    model->setGridDimensions(2, 1);

    QCOMPARE(model->pageCount(), 3);
    const auto live = model->placements();
    const auto launcherIt = std::find_if(live.cbegin(), live.cend(), [](const auto& p) {
        return p.instanceId == QLatin1String("launcher-reserved");
    });
    QVERIFY(launcherIt != live.cend());
    QCOMPARE(launcherIt->page, model->pageCount() - 1);
    for (const auto& p : live)
        QVERIFY(!p.visible || (p.page >= 0 && p.page < model->pageCount()));
}

void TestWidgetGridRemap::testSpillRoundTripRestoresPageBaseline()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    for (int i = 0; i < 3; ++i) {
        oap::GridPlacement p;
        p.instanceId = "tiny-" + QString::number(i);
        p.widgetId = "tiny";
        p.col = i; p.row = 0; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }
    oap::GridPlacement launcher;
    launcher.instanceId = "launcher-reserved";
    launcher.widgetId = "launcher";
    launcher.col = 0; launcher.row = 0;
    launcher.colSpan = 1; launcher.rowSpan = 1;
    launcher.opacity = 0.25; launcher.page = 1; launcher.visible = true;
    base.append(launcher);

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setPageCount(2);
    model->setGridDimensions(6, 4);
    model->setGridDimensions(2, 1);
    QCOMPARE(model->pageCount(), 3);
    model->setActivePage(2);

    QSignalSpy pageSpy(model, &oap::WidgetGridModel::pageCountChanged);
    QSignalSpy activeSpy(model, &oap::WidgetGridModel::activePageChanged);
    model->setGridDimensions(6, 4);

    QCOMPARE(model->pageCount(), 2);
    QCOMPARE(model->activePage(), 1);
    QCOMPARE(pageSpy.count(), 1);
    QCOMPARE(activeSpy.count(), 1);
    const auto restored = model->placements();
    const auto launcherIt = std::find_if(restored.cbegin(), restored.cend(), [](const auto& p) {
        return p.instanceId == QLatin1String("launcher-reserved");
    });
    QVERIFY(launcherIt != restored.cend());
    QCOMPARE(launcherIt->page, 1);
    for (const auto& p : restored)
        QVERIFY(p.page >= 0 && p.page < model->pageCount());

    model->addPage();
    QCOMPARE(model->pageCount(), 3);
    const auto afterAdd = model->placements();
    const auto movedLauncher = std::find_if(afterAdd.cbegin(), afterAdd.cend(), [](const auto& p) {
        return p.instanceId == QLatin1String("launcher-reserved");
    });
    QVERIFY(movedLauncher != afterAdd.cend());
    QCOMPARE(movedLauncher->page, 2);

    model->setGridDimensions(2, 1);
    model->setGridDimensions(6, 4);
    QCOMPARE(model->pageCount(), 3); // explicitly added empty page is baseline state

    QVERIFY(model->removePage(1));
    QCOMPARE(model->pageCount(), 2);
    model->setGridDimensions(2, 1);
    model->setGridDimensions(6, 4);
    QCOMPARE(model->pageCount(), 2); // explicit removal updates the baseline too
}

void TestWidgetGridRemap::testRepeatedSpillCyclesDoNotGrowPages()
{
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    for (int i = 0; i < 5; ++i) {
        oap::GridPlacement p;
        p.instanceId = "tiny-" + QString::number(i);
        p.widgetId = "tiny";
        p.col = i; p.row = 0; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        base.append(p);
    }

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setPageCount(3); // preserve two intentionally empty user pages
    model->setGridDimensions(6, 4);

    for (int cycle = 0; cycle < 3; ++cycle) {
        model->setGridDimensions(1, 1);
        QCOMPARE(model->pageCount(), 5);
        for (const auto& p : model->placements())
            QVERIFY(p.page >= 0 && p.page < model->pageCount());

        model->setGridDimensions(6, 4);
        QCOMPARE(model->pageCount(), 3);
        for (const auto& p : model->placements())
            QVERIFY(p.page >= 0 && p.page < model->pageCount());
    }
}

void TestWidgetGridRemap::testMutationPromotesSpillPageBaseline()
{
    auto* reg = makeRegistry();
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
    model->setPageCount(1);
    model->setGridDimensions(6, 4);
    model->setGridDimensions(2, 1);
    QCOMPARE(model->pageCount(), 2);

    model->setWidgetOpacity("tiny-0", 0.5); // promotes the constrained layout
    model->setGridDimensions(6, 4);

    QCOMPARE(model->pageCount(), 2);
    bool retainedPromotedPage = false;
    for (const auto& p : model->placements()) {
        QVERIFY(p.page >= 0 && p.page < model->pageCount());
        retainedPromotedPage = retainedPromotedPage || p.page == 1;
    }
    QVERIFY(retainedPromotedPage);
}

void TestWidgetGridRemap::testLoadNormalizesReservedPageTail()
{
    auto* reg = makeRegistry();
    auto* model = new oap::WidgetGridModel(reg, this);
    model->setGridDimensions(6, 4);
    model->setPageCount(3); // load ordering: page count precedes placements

    oap::GridPlacement normal;
    normal.instanceId = "tiny-0"; normal.widgetId = "tiny";
    normal.col = 0; normal.row = 0; normal.colSpan = 1; normal.rowSpan = 1;
    normal.opacity = 0.25; normal.page = 0; normal.visible = true;
    oap::GridPlacement launcher;
    launcher.instanceId = "launcher-reserved"; launcher.widgetId = "launcher";
    launcher.col = 0; launcher.row = 0; launcher.colSpan = 1; launcher.rowSpan = 1;
    launcher.opacity = 0.25; launcher.page = 1; launcher.visible = true;
    model->setPlacements({normal, launcher}, reg);
    model->setSavedDimensions(6, 4);

    QCOMPARE(model->pageCount(), 3);
    const auto loaded = model->placements();
    const auto launcherIt = std::find_if(loaded.cbegin(), loaded.cend(), [](const auto& p) {
        return p.instanceId == QLatin1String("launcher-reserved");
    });
    QVERIFY(launcherIt != loaded.cend());
    QCOMPARE(launcherIt->page, 2);

    model->setGridDimensions(2, 1);
    model->setGridDimensions(6, 4);
    QCOMPARE(model->pageCount(), 3);
    const auto restored = model->placements();
    const auto restoredLauncher = std::find_if(restored.cbegin(), restored.cend(), [](const auto& p) {
        return p.instanceId == QLatin1String("launcher-reserved");
    });
    QVERIFY(restoredLauncher != restored.cend());
    QCOMPARE(restoredLauncher->page, 2);
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

void TestWidgetGridRemap::testHiddenPlacementRecoversWhenGridGrows()
{
    auto* reg = makeRegistry();
    oap::GridPlacement p;
    p.instanceId = "bigmin-0"; p.widgetId = "bigmin";
    p.col = 1; p.row = 1; p.colSpan = 4; p.rowSpan = 3;
    p.opacity = 0.25; p.page = 1; p.visible = true;

    auto* model = setupForRemap(reg, {p}, 6, 4);
    model->setGridDimensions(3, 2);
    QVERIFY(!model->placements().constFirst().visible);
    QCOMPARE(model->totalWidgetCountOnPage(1), 1);

    model->setGridDimensions(8, 5);
    const auto recovered = model->placements().constFirst();
    QVERIFY(recovered.visible);
    QCOMPARE(recovered.page, 1);
    QVERIFY(recovered.page < model->pageCount());
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

void TestWidgetGridRemap::testPendingRemapBeforeMutation_data()
{
    QTest::addColumn<QString>("operation");
    for (const auto& operation : {QStringLiteral("move"), QStringLiteral("resize"),
                                  QStringLiteral("edge-resize"), QStringLiteral("opacity"),
                                  QStringLiteral("config"), QStringLiteral("remove")}) {
        QTest::newRow(qPrintable(operation)) << operation;
    }
}

void TestWidgetGridRemap::testPendingRemapBeforeMutation()
{
    QFETCH(QString, operation);
    auto* reg = makeRegistry();
    QList<oap::GridPlacement> base;
    oap::GridPlacement target;
    target.instanceId = "clock-0"; target.widgetId = "clock";
    target.col = 0; target.row = 0; target.colSpan = 1; target.rowSpan = 1;
    target.opacity = 0.25; target.page = 0; target.visible = true;
    base.append(target);
    oap::GridPlacement witness;
    witness.instanceId = "tiny-1"; witness.widgetId = "tiny";
    witness.col = 3; witness.row = 2; witness.colSpan = 1; witness.rowSpan = 1;
    witness.opacity = 0.25; witness.page = 0; witness.visible = true;
    base.append(witness);

    auto* model = setupForRemap(reg, base, 6, 4);
    model->setGridDimensions(6, 4);
    model->setWidgetSelected(true);
    model->setGridDimensions(8, 5);

    if (operation == QLatin1String("move"))
        QVERIFY(model->moveWidget("clock-0", 1, 0));
    else if (operation == QLatin1String("resize"))
        QVERIFY(model->resizeWidget("clock-0", 2, 1));
    else if (operation == QLatin1String("edge-resize"))
        QVERIFY(model->resizeWidgetFromEdge("clock-0", 0, 0, 2, 1));
    else if (operation == QLatin1String("opacity"))
        model->setWidgetOpacity("clock-0", 0.5);
    else if (operation == QLatin1String("config"))
        model->setWidgetConfig("clock-0", {});
    else if (operation == QLatin1String("remove"))
        model->removeWidget("clock-0");

    const auto live = model->placements();
    const auto witnessIt = std::find_if(live.cbegin(), live.cend(), [](const auto& p) {
        return p.instanceId == QLatin1String("tiny-1");
    });
    QVERIFY(witnessIt != live.cend());
    QCOMPARE(witnessIt->col, 4);
    QCOMPARE(witnessIt->row, 3);

    model->setWidgetSelected(false);
    const auto afterDeselect = model->placements();
    const auto stableWitness = std::find_if(afterDeselect.cbegin(), afterDeselect.cend(), [](const auto& p) {
        return p.instanceId == QLatin1String("tiny-1");
    });
    QVERIFY(stableWitness != afterDeselect.cend());
    QCOMPARE(stableWitness->col, 4);
    QCOMPARE(stableWitness->row, 3);
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
