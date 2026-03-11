// tests/test_widget_types.cpp
#include <QtTest/QtTest>
#include "core/widget/WidgetTypes.hpp"

class TestWidgetTypes : public QObject {
    Q_OBJECT
private slots:
    void testWidgetSizeFlags();
    void testWidgetDescriptorDefaults();
    void testWidgetPlacementCompositeKey();
    void testPageDescriptorDefaults();
};

void TestWidgetTypes::testWidgetSizeFlags() {
    using namespace oap;
    WidgetSizeFlags flags = WidgetSize::Main | WidgetSize::Sub;
    QVERIFY(flags.testFlag(WidgetSize::Main));
    QVERIFY(flags.testFlag(WidgetSize::Sub));

    WidgetSizeFlags mainOnly = WidgetSize::Main;
    QVERIFY(mainOnly.testFlag(WidgetSize::Main));
    QVERIFY(!mainOnly.testFlag(WidgetSize::Sub));
}

void TestWidgetTypes::testWidgetDescriptorDefaults() {
    oap::WidgetDescriptor desc;
    QVERIFY(desc.id.isEmpty());
    QVERIFY(desc.pluginId.isEmpty());
    QCOMPARE(desc.supportedSizes, oap::WidgetSizeFlags{});
    QVERIFY(desc.defaultConfig.isEmpty());
}

void TestWidgetTypes::testWidgetPlacementCompositeKey() {
    oap::WidgetPlacement p;
    p.pageId = "home";
    p.paneId = "main";
    QCOMPARE(p.compositeKey(), QStringLiteral("home:main"));
}

void TestWidgetTypes::testPageDescriptorDefaults() {
    oap::PageDescriptor page;
    QVERIFY(page.id.isEmpty());
    QCOMPARE(page.layoutTemplate, QStringLiteral("standard-3pane"));
    QCOMPARE(page.order, 0);
    QVERIFY(page.flags.isEmpty());
}

QTEST_GUILESS_MAIN(TestWidgetTypes)
#include "test_widget_types.moc"
