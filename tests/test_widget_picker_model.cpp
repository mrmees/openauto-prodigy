// tests/test_widget_picker_model.cpp
#include <QtTest/QtTest>
#include "core/widget/WidgetRegistry.hpp"
#include "ui/WidgetPickerModel.hpp"

class TestWidgetPickerModel : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void testCategoryRoleReturnsId();
    void testCategoryLabelRole();
    void testDescriptionRole();
    void testSortOrderNoWidgetFirst();
    void testSortOrderCategoryPriority();
    void testUncategorizedSortLast();
    void testUnknownCategoryCapitalized();

private:
    oap::WidgetRegistry* registry_ = nullptr;

    void registerTestWidget(const QString& id, const QString& name,
                            const QString& category, const QString& description);
};

void TestWidgetPickerModel::init() {
    registry_ = new oap::WidgetRegistry(this);
}

void TestWidgetPickerModel::cleanup() {
    delete registry_;
    registry_ = nullptr;
}

void TestWidgetPickerModel::registerTestWidget(const QString& id, const QString& name,
                                                const QString& category, const QString& description) {
    oap::WidgetDescriptor desc;
    desc.id = id;
    desc.displayName = name;
    desc.iconName = "\ue000";
    desc.category = category;
    desc.description = description;
    desc.qmlComponent = QUrl("qrc:/test.qml");
    desc.minCols = 1; desc.minRows = 1;
    registry_->registerWidget(desc);
}

void TestWidgetPickerModel::testCategoryRoleReturnsId() {
    registerTestWidget("test.clock", "Clock", "status", "Current time");

    oap::WidgetPickerModel model(registry_);
    model.filterByAvailableSpace(6, 4);

    // Row 0 = "No Widget", Row 1 = Clock
    QModelIndex idx = model.index(1, 0);
    QCOMPARE(model.data(idx, oap::WidgetPickerModel::CategoryRole).toString(), "status");
}

void TestWidgetPickerModel::testCategoryLabelRole() {
    registerTestWidget("test.clock", "Clock", "status", "Current time");
    registerTestWidget("test.np", "Now Playing", "media", "Track info");

    oap::WidgetPickerModel model(registry_);
    model.filterByAvailableSpace(6, 4);

    // Find the status widget
    for (int i = 0; i < model.rowCount(); ++i) {
        QModelIndex idx = model.index(i, 0);
        if (model.data(idx, oap::WidgetPickerModel::WidgetIdRole).toString() == "test.clock") {
            QCOMPARE(model.data(idx, oap::WidgetPickerModel::CategoryLabelRole).toString(), "Status");
            return;
        }
    }
    QFAIL("Clock widget not found in model");
}

void TestWidgetPickerModel::testDescriptionRole() {
    registerTestWidget("test.clock", "Clock", "status", "Current time");

    oap::WidgetPickerModel model(registry_);
    model.filterByAvailableSpace(6, 4);

    QModelIndex idx = model.index(1, 0);
    QCOMPARE(model.data(idx, oap::WidgetPickerModel::DescriptionRole).toString(), "Current time");
}

void TestWidgetPickerModel::testSortOrderNoWidgetFirst() {
    registerTestWidget("test.np", "Now Playing", "media", "Track info");
    registerTestWidget("test.clock", "Clock", "status", "Current time");

    oap::WidgetPickerModel model(registry_);
    model.filterByAvailableSpace(6, 4);

    // First item must be "No Widget" (empty id)
    QModelIndex first = model.index(0, 0);
    QVERIFY(model.data(first, oap::WidgetPickerModel::WidgetIdRole).toString().isEmpty());
    QCOMPARE(model.data(first, oap::WidgetPickerModel::DisplayNameRole).toString(), "No Widget");
    // Its categoryLabel should be empty (not part of any category group)
    QVERIFY(model.data(first, oap::WidgetPickerModel::CategoryLabelRole).toString().isEmpty());
}

void TestWidgetPickerModel::testSortOrderCategoryPriority() {
    // Register in reverse order — media before status
    registerTestWidget("test.np", "Now Playing", "media", "Track info");
    registerTestWidget("test.nav", "Navigation", "navigation", "Turn-by-turn");
    registerTestWidget("test.clock", "Clock", "status", "Current time");

    oap::WidgetPickerModel model(registry_);
    model.filterByAvailableSpace(6, 4);

    // Skip row 0 ("No Widget")
    // Expect: status (Clock) before media (Now Playing) before navigation (Navigation)
    QStringList order;
    for (int i = 1; i < model.rowCount(); ++i) {
        QModelIndex idx = model.index(i, 0);
        order << model.data(idx, oap::WidgetPickerModel::CategoryRole).toString();
    }
    QCOMPARE(order, QStringList({"status", "media", "navigation"}));
}

void TestWidgetPickerModel::testUncategorizedSortLast() {
    registerTestWidget("test.clock", "Clock", "status", "Current time");
    registerTestWidget("test.custom", "Custom Widget", "", "A custom widget");

    oap::WidgetPickerModel model(registry_);
    model.filterByAvailableSpace(6, 4);

    // Last item should be the uncategorized widget
    QModelIndex last = model.index(model.rowCount() - 1, 0);
    QCOMPARE(model.data(last, oap::WidgetPickerModel::WidgetIdRole).toString(), "test.custom");
    QCOMPARE(model.data(last, oap::WidgetPickerModel::CategoryLabelRole).toString(), "Other");
}

void TestWidgetPickerModel::testUnknownCategoryCapitalized() {
    registerTestWidget("test.foo", "Foo Widget", "utilities", "A utility widget");

    oap::WidgetPickerModel model(registry_);
    model.filterByAvailableSpace(6, 4);

    QModelIndex idx = model.index(1, 0);
    QCOMPARE(model.data(idx, oap::WidgetPickerModel::CategoryLabelRole).toString(), "Utilities");
}

QTEST_GUILESS_MAIN(TestWidgetPickerModel)
#include "test_widget_picker_model.moc"
