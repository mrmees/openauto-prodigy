// tests/test_widget_registry.cpp
#include <QtTest/QtTest>
#include "core/widget/WidgetRegistry.hpp"
#include "ui/WidgetPickerModel.hpp"

class TestWidgetRegistry : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndLookup();
    void testDuplicateIdRejected();
    void testWidgetsFittingSpace();
    void testWidgetsFittingSpaceFiltersStubs();
    void testUnregister();
    void testDescriptorCount();
    void testFilterByContributionKind();
    void testWidgetsFittingSpaceExcludesLiveSurface();
    void testContributionKindPreservedOnRegister();
    void testPickerModelExcludesLiveSurface();
};

void TestWidgetRegistry::testRegisterAndLookup() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor desc;
    desc.id = "org.test.clock";
    desc.displayName = "Clock";
    desc.minCols = 1; desc.minRows = 1;
    desc.maxCols = 6; desc.maxRows = 4;

    QVERIFY(registry.registerWidget(desc));
    auto result = registry.descriptor("org.test.clock");
    QVERIFY(result.has_value());
    QCOMPARE(result->displayName, "Clock");
}

void TestWidgetRegistry::testDuplicateIdRejected() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor desc;
    desc.id = "org.test.dup";
    QVERIFY(registry.registerWidget(desc));
    QVERIFY(!registry.registerWidget(desc));
}

void TestWidgetRegistry::testWidgetsFittingSpace() {
    oap::WidgetRegistry registry;

    // Small widget: min 1x1
    oap::WidgetDescriptor small;
    small.id = "small";
    small.minCols = 1; small.minRows = 1;
    small.qmlComponent = QUrl("qrc:/widgets/Small.qml");
    registry.registerWidget(small);

    // Large widget: min 3x2
    oap::WidgetDescriptor large;
    large.id = "large";
    large.minCols = 3; large.minRows = 2;
    large.qmlComponent = QUrl("qrc:/widgets/Large.qml");
    registry.registerWidget(large);

    // Wide widget: min 4x1
    oap::WidgetDescriptor wide;
    wide.id = "wide";
    wide.minCols = 4; wide.minRows = 1;
    wide.qmlComponent = QUrl("qrc:/widgets/Wide.qml");
    registry.registerWidget(wide);

    // 2x2 space: only small fits
    auto fits2x2 = registry.widgetsFittingSpace(2, 2);
    QCOMPARE(fits2x2.size(), 1);
    QCOMPARE(fits2x2[0].id, "small");

    // 4x2 space: small and large and wide fit
    auto fits4x2 = registry.widgetsFittingSpace(4, 2);
    QCOMPARE(fits4x2.size(), 3);

    // 6x1 space: small and wide fit (large needs 2 rows)
    auto fits6x1 = registry.widgetsFittingSpace(6, 1);
    QCOMPARE(fits6x1.size(), 2);
}

void TestWidgetRegistry::testWidgetsFittingSpaceFiltersStubs() {
    oap::WidgetRegistry registry;

    // Stub widget (no qmlComponent)
    oap::WidgetDescriptor stub;
    stub.id = "stub";
    stub.minCols = 1; stub.minRows = 1;
    // qmlComponent intentionally empty
    registry.registerWidget(stub);

    // Real widget
    oap::WidgetDescriptor real;
    real.id = "real";
    real.minCols = 1; real.minRows = 1;
    real.qmlComponent = QUrl("qrc:/widgets/Real.qml");
    registry.registerWidget(real);

    auto fits = registry.widgetsFittingSpace(6, 4);
    QCOMPARE(fits.size(), 1);
    QCOMPARE(fits[0].id, "real");
}

void TestWidgetRegistry::testUnregister() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor desc;
    desc.id = "org.test.removable";
    registry.registerWidget(desc);

    registry.unregisterWidget("org.test.removable");
    QVERIFY(!registry.descriptor("org.test.removable").has_value());
}

void TestWidgetRegistry::testDescriptorCount() {
    oap::WidgetRegistry registry;
    QCOMPARE(registry.allDescriptors().size(), 0);

    oap::WidgetDescriptor d1;
    d1.id = "w1";
    registry.registerWidget(d1);

    oap::WidgetDescriptor d2;
    d2.id = "w2";
    registry.registerWidget(d2);

    QCOMPARE(registry.allDescriptors().size(), 2);
}

void TestWidgetRegistry::testFilterByContributionKind() {
    oap::WidgetRegistry registry;

    oap::WidgetDescriptor widget;
    widget.id = "w1";
    widget.contributionKind = oap::DashboardContributionKind::Widget;
    widget.qmlComponent = QUrl("qrc:/widgets/W1.qml");
    registry.registerWidget(widget);

    oap::WidgetDescriptor liveSurface;
    liveSurface.id = "ls1";
    liveSurface.contributionKind = oap::DashboardContributionKind::LiveSurfaceWidget;
    liveSurface.qmlComponent = QUrl("qrc:/widgets/LS1.qml");
    registry.registerWidget(liveSurface);

    auto widgets = registry.descriptorsByKind(oap::DashboardContributionKind::Widget);
    QCOMPARE(widgets.size(), 1);
    QCOMPARE(widgets[0].id, "w1");

    auto liveSurfaces = registry.descriptorsByKind(oap::DashboardContributionKind::LiveSurfaceWidget);
    QCOMPARE(liveSurfaces.size(), 1);
    QCOMPARE(liveSurfaces[0].id, "ls1");
}

void TestWidgetRegistry::testWidgetsFittingSpaceExcludesLiveSurface() {
    oap::WidgetRegistry registry;

    oap::WidgetDescriptor widget;
    widget.id = "normal";
    widget.minCols = 1; widget.minRows = 1;
    widget.contributionKind = oap::DashboardContributionKind::Widget;
    widget.qmlComponent = QUrl("qrc:/widgets/Normal.qml");
    registry.registerWidget(widget);

    oap::WidgetDescriptor liveSurface;
    liveSurface.id = "live";
    liveSurface.minCols = 1; liveSurface.minRows = 1;
    liveSurface.contributionKind = oap::DashboardContributionKind::LiveSurfaceWidget;
    liveSurface.qmlComponent = QUrl("qrc:/widgets/Live.qml");
    registry.registerWidget(liveSurface);

    // widgetsFittingSpace should only return Widget-kind descriptors
    auto fits = registry.widgetsFittingSpace(6, 4);
    QCOMPARE(fits.size(), 1);
    QCOMPARE(fits[0].id, "normal");
}

void TestWidgetRegistry::testContributionKindPreservedOnRegister() {
    oap::WidgetRegistry registry;

    oap::WidgetDescriptor desc;
    desc.id = "org.test.live";
    desc.contributionKind = oap::DashboardContributionKind::LiveSurfaceWidget;
    registry.registerWidget(desc);

    auto result = registry.descriptor("org.test.live");
    QVERIFY(result.has_value());
    QCOMPARE(result->contributionKind, oap::DashboardContributionKind::LiveSurfaceWidget);
}

void TestWidgetRegistry::testPickerModelExcludesLiveSurface() {
    oap::WidgetRegistry registry;

    oap::WidgetDescriptor widget;
    widget.id = "normal";
    widget.displayName = "Normal Widget";
    widget.minCols = 1; widget.minRows = 1;
    widget.contributionKind = oap::DashboardContributionKind::Widget;
    widget.qmlComponent = QUrl("qrc:/widgets/Normal.qml");
    registry.registerWidget(widget);

    oap::WidgetDescriptor liveSurface;
    liveSurface.id = "live";
    liveSurface.displayName = "Live Surface";
    liveSurface.minCols = 1; liveSurface.minRows = 1;
    liveSurface.contributionKind = oap::DashboardContributionKind::LiveSurfaceWidget;
    liveSurface.qmlComponent = QUrl("qrc:/widgets/Live.qml");
    registry.registerWidget(liveSurface);

    oap::WidgetPickerModel picker(&registry);
    picker.filterByAvailableSpace(6, 4);

    // Row 0 is "No Widget", row 1+ are real widgets
    // LiveSurfaceWidget should NOT appear
    bool foundNormal = false;
    bool foundLive = false;
    for (int i = 0; i < picker.rowCount(); ++i) {
        auto idx = picker.index(i);
        QString id = picker.data(idx, oap::WidgetPickerModel::WidgetIdRole).toString();
        if (id == "normal") foundNormal = true;
        if (id == "live") foundLive = true;
    }
    QVERIFY(foundNormal);
    QVERIFY(!foundLive);
}

QTEST_GUILESS_MAIN(TestWidgetRegistry)
#include "test_widget_registry.moc"
