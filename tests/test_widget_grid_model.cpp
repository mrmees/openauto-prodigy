// tests/test_widget_grid_model.cpp -- WidgetGridModel occupancy, collision, clamping tests
#include <QtTest>
#include <QSignalSpy>
#include "ui/WidgetGridModel.hpp"
#include "core/widget/WidgetRegistry.hpp"

class TestWidgetGridModel : public QObject {
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
            d.id = "status";
            d.displayName = "Status";
            d.qmlComponent = QUrl("qrc:/StatusWidget.qml");
            d.minCols = 1; d.minRows = 1;
            d.maxCols = 3; d.maxRows = 2;
            d.defaultCols = 2; d.defaultRows = 1;
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
        return reg;
    }

private slots:
    void testPlaceWidgetSuccess();
    void testPlaceWidgetOccupiedFails();
    void testPlaceWidgetOutOfBounds();
    void testMoveWidgetSuccess();
    void testMoveWidgetOccupiedFails();
    void testResizeWidgetSuccess();
    void testResizeWidgetBeyondMaxFails();
    void testResizeWidgetOutOfBoundsFails();
    void testRemoveWidget();
    void testCanPlace();
    void testCanPlaceWithExclude();
    void testRowCountMatchesPlacements();
    void testDataRoles();
    void testSetWidgetOpacity();
    void testPlacementsChangedSignal();
    void testSetGridDimensionsClampsPosition();
    void testSetGridDimensionsClampsSpan();
    void testSetGridDimensionsNudgesOverlapping();
    void testNudgedWidgetsOccupyNewCells();
    void testGridDimensionsProperties();
    // Constraint role tests
    void testConstraintRoles();
    void testConstraintRolesFallback();
    void testDefaultSpanRoles();
    // findFirstAvailableCell tests
    void testFindFirstAvailableCellEmpty();
    void testFindFirstAvailableCellSkipsOccupied();
    void testFindFirstAvailableCellNoFitWide();
    void testFindFirstAvailableCellGridFull();
    // Page-scoped tests
    void testPlaceWidgetOnDifferentPages();
    void testCanPlacePageScoped();
    void testFindFirstAvailableCellPageScoped();
    void testAddPage();
    void testRemovePageShiftsDown();
    void testRemovePageZeroRejected();
    void testWidgetCountOnPage();
    void testPageRole();
};

void TestWidgetGridModel::testPlaceWidgetSuccess() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    bool ok = model.placeWidget("clock", 0, 0, 2, 2);
    QVERIFY(ok);
    QCOMPARE(model.rowCount(), 1);
}

void TestWidgetGridModel::testPlaceWidgetOccupiedFails() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    // Overlapping placement should fail
    bool ok = model.placeWidget("status", 1, 1, 2, 1);
    QVERIFY(!ok);
    QCOMPARE(model.rowCount(), 1); // unchanged
}

void TestWidgetGridModel::testPlaceWidgetOutOfBounds() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    // col + colSpan exceeds grid
    bool ok = model.placeWidget("clock", 5, 0, 2, 2);
    QVERIFY(!ok);
    QCOMPARE(model.rowCount(), 0);
}

void TestWidgetGridModel::testMoveWidgetSuccess() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    bool ok = model.moveWidget(instanceId, 4, 0);
    QVERIFY(ok);

    placements = model.placements();
    QCOMPARE(placements[0].col, 4);
    QCOMPARE(placements[0].row, 0);

    // Old cells should be free
    QVERIFY(model.canPlace(0, 0, 2, 2));
}

void TestWidgetGridModel::testMoveWidgetOccupiedFails() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    model.placeWidget("status", 3, 0, 2, 1);

    auto placements = model.placements();
    QString clockId = placements[0].instanceId;

    // Move clock to overlap with status
    bool ok = model.moveWidget(clockId, 2, 0);
    QVERIFY(!ok);

    // Clock should stay at original position
    placements = model.placements();
    QCOMPARE(placements[0].col, 0);
    QCOMPARE(placements[0].row, 0);
}

void TestWidgetGridModel::testResizeWidgetSuccess() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    bool ok = model.resizeWidget(instanceId, 3, 3);
    QVERIFY(ok);

    placements = model.placements();
    QCOMPARE(placements[0].colSpan, 3);
    QCOMPARE(placements[0].rowSpan, 3);
}

void TestWidgetGridModel::testResizeWidgetBeyondMaxFails() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("status", 0, 0, 2, 1);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    // status maxCols=3, maxRows=2 -- try 4x1
    bool ok = model.resizeWidget(instanceId, 4, 1);
    QVERIFY(!ok);
}

void TestWidgetGridModel::testResizeWidgetOutOfBoundsFails() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 4, 0, 2, 2);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    // Resizing to 3 cols would push past grid edge (4+3=7 > 6)
    bool ok = model.resizeWidget(instanceId, 3, 2);
    QVERIFY(!ok);
}

void TestWidgetGridModel::testRemoveWidget() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    model.removeWidget(instanceId);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.canPlace(0, 0, 2, 2));
}

void TestWidgetGridModel::testCanPlace() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    QVERIFY(model.canPlace(0, 0, 2, 2));
    model.placeWidget("clock", 0, 0, 2, 2);
    QVERIFY(!model.canPlace(0, 0, 2, 2));
    QVERIFY(model.canPlace(2, 0, 2, 2)); // adjacent free
}

void TestWidgetGridModel::testCanPlaceWithExclude() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    // Excluding self should see its own cells as free
    QVERIFY(model.canPlace(0, 0, 2, 2, instanceId));
    QVERIFY(model.canPlace(0, 0, 3, 3, instanceId));
}

void TestWidgetGridModel::testRowCountMatchesPlacements() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    QCOMPARE(model.rowCount(), 0);
    model.placeWidget("clock", 0, 0, 2, 2);
    QCOMPARE(model.rowCount(), 1);
    model.placeWidget("status", 3, 0, 2, 1);
    QCOMPARE(model.rowCount(), 2);
}

void TestWidgetGridModel::testDataRoles() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 1, 2, 2, 2);

    QModelIndex idx = model.index(0);
    QCOMPARE(idx.data(oap::WidgetGridModel::WidgetIdRole).toString(), QString("clock"));
    QVERIFY(!idx.data(oap::WidgetGridModel::InstanceIdRole).toString().isEmpty());
    QCOMPARE(idx.data(oap::WidgetGridModel::ColumnRole).toInt(), 1);
    QCOMPARE(idx.data(oap::WidgetGridModel::RowRole).toInt(), 2);
    QCOMPARE(idx.data(oap::WidgetGridModel::ColSpanRole).toInt(), 2);
    QCOMPARE(idx.data(oap::WidgetGridModel::RowSpanRole).toInt(), 2);
    QCOMPARE(idx.data(oap::WidgetGridModel::OpacityRole).toDouble(), 0.25);
    QCOMPARE(idx.data(oap::WidgetGridModel::QmlComponentRole).toUrl(), QUrl("qrc:/ClockWidget.qml"));
}

void TestWidgetGridModel::testSetWidgetOpacity() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    QSignalSpy spy(&model, &QAbstractListModel::dataChanged);
    model.setWidgetOpacity(instanceId, 0.75);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.placements()[0].opacity, 0.75);
}

void TestWidgetGridModel::testPlacementsChangedSignal() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    QSignalSpy spy(&model, &oap::WidgetGridModel::placementsChanged);

    model.placeWidget("clock", 0, 0, 2, 2);
    QCOMPARE(spy.count(), 1);

    auto placements = model.placements();
    QString id = placements[0].instanceId;

    model.moveWidget(id, 2, 0);
    QCOMPARE(spy.count(), 2);

    model.resizeWidget(id, 3, 3);
    QCOMPARE(spy.count(), 3);

    model.removeWidget(id);
    QCOMPARE(spy.count(), 4);
}

void TestWidgetGridModel::testSetGridDimensionsClampsPosition() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("tiny", 5, 0, 1, 1);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    // Shrink grid from 6 cols to 5 -- widget at col 5 should clamp to col 4
    model.setGridDimensions(5, 3);

    placements = model.placements();
    QCOMPARE(placements[0].col, 4);
}

void TestWidgetGridModel::testSetGridDimensionsClampsSpan() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 4, 3);

    // Shrink to 3x2 -- colSpan 4 exceeds 3 cols, rowSpan 3 exceeds 2 rows
    // Remap clamps spans to fit the new grid
    model.setGridDimensions(3, 2);

    auto placements = model.placements();
    QVERIFY(placements[0].colSpan <= 3);
    QVERIFY(placements[0].rowSpan <= 2);
    QVERIFY(placements[0].visible); // still visible (clock minCols=1)
}

void TestWidgetGridModel::testSetGridDimensionsNudgesOverlapping() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    // Place two widgets side by side
    model.placeWidget("tiny", 0, 0, 1, 1); // first-placed
    model.placeWidget("tiny", 1, 0, 1, 1); // second-placed

    // Shrink to 1 column -- both map to col 0, second nudged (not hidden)
    model.setGridDimensions(1, 4);

    auto placements = model.placements();
    QCOMPARE(placements.size(), 2);
    QVERIFY(placements[0].visible);
    QVERIFY(placements[1].visible); // nudged to different row, not hidden
    // They should not overlap
    QVERIFY(placements[0].col != placements[1].col || placements[0].row != placements[1].row);
}

void TestWidgetGridModel::testNudgedWidgetsOccupyNewCells() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("tiny", 0, 0, 1, 1);
    model.placeWidget("tiny", 1, 0, 1, 1);

    // Shrink so second gets nudged
    model.setGridDimensions(1, 4);

    auto placements = model.placements();
    // Both visible (second was nudged, not hidden)
    QVERIFY(placements[0].visible);
    QVERIFY(placements[1].visible);
    // Nudged widget occupies a cell -- verify we can still place at a free cell
    QVERIFY(model.canPlace(0, 2, 1, 1));
}

void TestWidgetGridModel::testGridDimensionsProperties() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    QCOMPARE(model.gridColumns(), 6);
    QCOMPARE(model.gridRows(), 4);

    QSignalSpy spy(&model, &oap::WidgetGridModel::gridDimensionsChanged);
    model.setGridDimensions(5, 3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.gridColumns(), 5);
    QCOMPARE(model.gridRows(), 3);
}

void TestWidgetGridModel::testConstraintRoles() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("status", 0, 0, 2, 1);
    QModelIndex idx = model.index(0);

    // status: minCols=1, minRows=1, maxCols=3, maxRows=2
    QCOMPARE(idx.data(oap::WidgetGridModel::MinColsRole).toInt(), 1);
    QCOMPARE(idx.data(oap::WidgetGridModel::MinRowsRole).toInt(), 1);
    QCOMPARE(idx.data(oap::WidgetGridModel::MaxColsRole).toInt(), 3);
    QCOMPARE(idx.data(oap::WidgetGridModel::MaxRowsRole).toInt(), 2);
}

void TestWidgetGridModel::testConstraintRolesFallback() {
    // Registry with no descriptor for "unknown"
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    // Manually set placement with unknown widget ID
    QList<oap::GridPlacement> placements;
    oap::GridPlacement p;
    p.instanceId = "unknown-0";
    p.widgetId = "unknown";
    p.col = 0; p.row = 0;
    p.colSpan = 1; p.rowSpan = 1;
    p.opacity = 0.25; p.visible = true;
    placements.append(p);
    model.setPlacements(placements);

    QModelIndex idx = model.index(0);
    // Fallbacks: minCols=1, minRows=1, maxCols=6, maxRows=4
    QCOMPARE(idx.data(oap::WidgetGridModel::MinColsRole).toInt(), 1);
    QCOMPARE(idx.data(oap::WidgetGridModel::MinRowsRole).toInt(), 1);
    QCOMPARE(idx.data(oap::WidgetGridModel::MaxColsRole).toInt(), 6);
    QCOMPARE(idx.data(oap::WidgetGridModel::MaxRowsRole).toInt(), 4);
}

void TestWidgetGridModel::testDefaultSpanRoles() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    QModelIndex idx = model.index(0);

    // clock: defaultCols=2, defaultRows=2
    QCOMPARE(idx.data(oap::WidgetGridModel::DefaultColsRole).toInt(), 2);
    QCOMPARE(idx.data(oap::WidgetGridModel::DefaultRowsRole).toInt(), 2);
}

void TestWidgetGridModel::testFindFirstAvailableCellEmpty() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    QVariantMap result = model.findFirstAvailableCell(2, 2);
    QCOMPARE(result["col"].toInt(), 0);
    QCOMPARE(result["row"].toInt(), 0);
}

void TestWidgetGridModel::testFindFirstAvailableCellSkipsOccupied() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    // Occupy top-left 2x2
    model.placeWidget("clock", 0, 0, 2, 2);

    QVariantMap result = model.findFirstAvailableCell(2, 2);
    QCOMPARE(result["col"].toInt(), 2);
    QCOMPARE(result["row"].toInt(), 0);
}

void TestWidgetGridModel::testFindFirstAvailableCellNoFitWide() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    // Widget wider than grid
    QVariantMap result = model.findFirstAvailableCell(7, 1);
    QCOMPARE(result["col"].toInt(), -1);
    QCOMPARE(result["row"].toInt(), -1);
}

void TestWidgetGridModel::testFindFirstAvailableCellGridFull() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(2, 2);

    // Fill entire 2x2 grid
    model.placeWidget("tiny", 0, 0, 1, 1);
    model.placeWidget("tiny", 1, 0, 1, 1);
    model.placeWidget("tiny", 0, 1, 1, 1);
    model.placeWidget("tiny", 1, 1, 1, 1);

    QVariantMap result = model.findFirstAvailableCell(1, 1);
    QCOMPARE(result["col"].toInt(), -1);
    QCOMPARE(result["row"].toInt(), -1);
}

// --- Page-scoped tests ---

void TestWidgetGridModel::testPlaceWidgetOnDifferentPages() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    // Place on page 0 (default)
    bool ok = model.placeWidget("clock", 0, 0, 2, 2);
    QVERIFY(ok);

    // Switch to page 1, place at same coords
    model.setActivePage(1);
    ok = model.placeWidget("clock", 0, 0, 2, 2);
    QVERIFY(ok);
    QCOMPARE(model.rowCount(), 2);
}

void TestWidgetGridModel::testCanPlacePageScoped() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);

    // Same coords on page 1 should be available
    model.setActivePage(1);
    QVERIFY(model.canPlace(0, 0, 2, 2));
}

void TestWidgetGridModel::testFindFirstAvailableCellPageScoped() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    // Fill top-left on page 0
    model.placeWidget("clock", 0, 0, 2, 2);

    // Page 1 should find (0,0) available
    model.setActivePage(1);
    QVariantMap result = model.findFirstAvailableCell(2, 2);
    QCOMPARE(result["col"].toInt(), 0);
    QCOMPARE(result["row"].toInt(), 0);
}

void TestWidgetGridModel::testAddPage() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    QCOMPARE(model.pageCount(), 2); // default
    QSignalSpy spy(&model, &oap::WidgetGridModel::pageCountChanged);
    model.addPage();
    QCOMPARE(model.pageCount(), 3);
    QCOMPARE(spy.count(), 1);
}

void TestWidgetGridModel::testRemovePageShiftsDown() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);
    model.setPageCount(3);

    // Place widget on page 2
    model.setActivePage(2);
    model.placeWidget("clock", 0, 0, 2, 2);

    // Remove page 1 -- page 2 widget shifts to page 1
    bool ok = model.removePage(1);
    QVERIFY(ok);
    QCOMPARE(model.pageCount(), 2);

    auto placements = model.placements();
    QCOMPARE(placements[0].page, 1);
}

void TestWidgetGridModel::testRemovePageZeroRejected() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    bool ok = model.removePage(0);
    QVERIFY(!ok);
    QCOMPARE(model.pageCount(), 2);
}

void TestWidgetGridModel::testWidgetCountOnPage() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    model.placeWidget("tiny", 3, 0, 1, 1);

    model.setActivePage(1);
    model.placeWidget("tiny", 0, 0, 1, 1);

    QCOMPARE(model.widgetCountOnPage(0), 2);
    QCOMPARE(model.widgetCountOnPage(1), 1);
}

void TestWidgetGridModel::testPageRole() {
    auto* reg = makeRegistry();
    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);

    model.placeWidget("clock", 0, 0, 2, 2);
    model.setActivePage(1);
    model.placeWidget("tiny", 0, 0, 1, 1);

    QModelIndex idx0 = model.index(0);
    QModelIndex idx1 = model.index(1);
    QCOMPARE(idx0.data(oap::WidgetGridModel::PageRole).toInt(), 0);
    QCOMPARE(idx1.data(oap::WidgetGridModel::PageRole).toInt(), 1);
}

QTEST_GUILESS_MAIN(TestWidgetGridModel)
#include "test_widget_grid_model.moc"
