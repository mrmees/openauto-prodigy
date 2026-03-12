// tests/test_widget_registry.cpp
#include <QtTest/QtTest>
#include "core/widget/WidgetRegistry.hpp"

class TestWidgetRegistry : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndLookup();
    void testDuplicateIdRejected();
    void testWidgetsFittingSpace();
    void testWidgetsFittingSpaceFiltersStubs();
    void testUnregister();
    void testDescriptorCount();
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

QTEST_GUILESS_MAIN(TestWidgetRegistry)
#include "test_widget_registry.moc"
