// tests/test_widget_registry.cpp
#include <QtTest/QtTest>
#include "core/widget/WidgetRegistry.hpp"

class TestWidgetRegistry : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndLookup();
    void testDuplicateIdRejected();
    void testFilterBySize();
    void testUnregister();
    void testDescriptorCount();
};

void TestWidgetRegistry::testRegisterAndLookup() {
    oap::WidgetRegistry registry;
    oap::WidgetDescriptor desc;
    desc.id = "org.test.clock";
    desc.displayName = "Clock";
    desc.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;

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

void TestWidgetRegistry::testFilterBySize() {
    oap::WidgetRegistry registry;

    oap::WidgetDescriptor mainOnly;
    mainOnly.id = "main-only";
    mainOnly.supportedSizes = oap::WidgetSize::Main;
    registry.registerWidget(mainOnly);

    oap::WidgetDescriptor subOnly;
    subOnly.id = "sub-only";
    subOnly.supportedSizes = oap::WidgetSize::Sub;
    registry.registerWidget(subOnly);

    oap::WidgetDescriptor both;
    both.id = "both";
    both.supportedSizes = oap::WidgetSize::Main | oap::WidgetSize::Sub;
    registry.registerWidget(both);

    auto mainWidgets = registry.widgetsForSize(oap::WidgetSize::Main);
    QCOMPARE(mainWidgets.size(), 2); // main-only + both

    auto subWidgets = registry.widgetsForSize(oap::WidgetSize::Sub);
    QCOMPARE(subWidgets.size(), 2); // sub-only + both
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
