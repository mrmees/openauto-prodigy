// tests/test_widget_instance_context.cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "ui/WidgetInstanceContext.hpp"
#include "ui/WidgetContextFactory.hpp"
#include "ui/WidgetGridModel.hpp"
#include "core/widget/WidgetTypes.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "core/plugin/HostContext.hpp"
#include "core/services/IProjectionStatusProvider.hpp"
#include "core/services/INavigationProvider.hpp"
#include "core/services/IMediaStatusProvider.hpp"

// Minimal test providers
class TestProjectionProvider : public oap::IProjectionStatusProvider {
    Q_OBJECT
public:
    using IProjectionStatusProvider::IProjectionStatusProvider;
    int projectionState() const override { return 3; }
    QString statusMessage() const override { return "Connected"; }
};

class TestNavigationProvider : public oap::INavigationProvider {
    Q_OBJECT
public:
    using INavigationProvider::INavigationProvider;
    bool navActive() const override { return true; }
    QString roadName() const override { return "Main St"; }
    int maneuverType() const override { return 1; }
    int turnDirection() const override { return 2; }
    QString formattedDistance() const override { return "500 ft"; }
    bool hasManeuverIcon() const override { return false; }
    int iconVersion() const override { return 0; }
};

class TestMediaProvider : public oap::IMediaStatusProvider {
    Q_OBJECT
public:
    using IMediaStatusProvider::IMediaStatusProvider;
    bool hasMedia() const override { return true; }
    QString title() const override { return "Song"; }
    QString artist() const override { return "Artist"; }
    QString album() const override { return "Album"; }
    int playbackState() const override { return 1; }
    QString source() const override { return "Bluetooth"; }
    QString appName() const override { return {}; }
    void playPause() override {}
    void next() override {}
    void previous() override {}
};

class TestWidgetInstanceContext : public QObject {
    Q_OBJECT
private slots:
    void testProperties();
    void testCellDimensions();
    void testProviderPropertiesWithHostContext();
    void testProviderPropertiesWithoutHostContext();
    void testColSpanDefaultsFromPlacement();
    void testRowSpanDefaultsFromPlacement();
    void testSetColSpanEmitsSignal();
    void testSetRowSpanEmitsSignal();
    void testSetColSpanNoopDoesNotEmit();
    void testIsCurrentPageDefaultsFalse();
    void testSetIsCurrentPageEmitsSignal();
    void testSetIsCurrentPageNoopDoesNotEmit();
    // effectiveConfig tests
    void testEffectiveConfigDefaults();
    void testEffectiveConfigOverride();
    void testEffectiveConfigMerge();
    void testSetInstanceConfigEmitsSignal();
    void testEffectiveConfigEmptyWhenNoSchema();
    void testContextReplacementDoesNotBreakLiveUpdates();
    void testMultipleLiveContextsReceiveLiveUpdates();
};

void TestWidgetInstanceContext::testProperties() {
    oap::GridPlacement placement;
    placement.instanceId = "clock-0";
    placement.widgetId = "org.openauto.clock";
    placement.col = 0;
    placement.row = 0;
    placement.colSpan = 2;
    placement.rowSpan = 2;

    oap::WidgetInstanceContext ctx(placement, 310, 290, nullptr, {}, {}, this);

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

    oap::WidgetInstanceContext ctx(placement, 155, 145, nullptr, {}, {}, this);

    // Verify QML-accessible properties
    QCOMPARE(ctx.property("cellWidth").toInt(), 155);
    QCOMPARE(ctx.property("cellHeight").toInt(), 145);
}

void TestWidgetInstanceContext::testProviderPropertiesWithHostContext() {
    auto hostContext = std::make_unique<oap::HostContext>();

    TestProjectionProvider projection;
    TestNavigationProvider navigation;
    TestMediaProvider media;

    hostContext->setProjectionStatusProvider(&projection);
    hostContext->setNavigationProvider(&navigation);
    hostContext->setMediaStatusProvider(&media);

    oap::GridPlacement placement;
    placement.instanceId = "test-providers";
    oap::WidgetInstanceContext ctx(placement, 100, 100, hostContext.get(), {}, {}, this);

    // Providers should be accessible via Q_PROPERTY
    QObject* proj = ctx.property("projectionStatus").value<QObject*>();
    QVERIFY(proj != nullptr);
    QCOMPARE(proj->property("projectionState").toInt(), 3);

    QObject* nav = ctx.property("navigationProvider").value<QObject*>();
    QVERIFY(nav != nullptr);
    QCOMPARE(nav->property("navActive").toBool(), true);

    QObject* ms = ctx.property("mediaStatus").value<QObject*>();
    QVERIFY(ms != nullptr);
    QCOMPARE(ms->property("title").toString(), "Song");
}

void TestWidgetInstanceContext::testProviderPropertiesWithoutHostContext() {
    oap::GridPlacement placement;
    placement.instanceId = "test-no-context";
    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);

    // Without host context, providers should be null
    QVERIFY(ctx.property("projectionStatus").value<QObject*>() == nullptr);
    QVERIFY(ctx.property("navigationProvider").value<QObject*>() == nullptr);
    QVERIFY(ctx.property("mediaStatus").value<QObject*>() == nullptr);
}

void TestWidgetInstanceContext::testColSpanDefaultsFromPlacement() {
    oap::GridPlacement placement;
    placement.instanceId = "test-span";
    placement.colSpan = 3;
    placement.rowSpan = 2;

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);
    QCOMPARE(ctx.colSpan(), 3);
}

void TestWidgetInstanceContext::testRowSpanDefaultsFromPlacement() {
    oap::GridPlacement placement;
    placement.instanceId = "test-span";
    placement.colSpan = 3;
    placement.rowSpan = 2;

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);
    QCOMPARE(ctx.rowSpan(), 2);
}

void TestWidgetInstanceContext::testSetColSpanEmitsSignal() {
    oap::GridPlacement placement;
    placement.instanceId = "test-signal";
    placement.colSpan = 2;
    placement.rowSpan = 1;

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);
    QSignalSpy spy(&ctx, &oap::WidgetInstanceContext::colSpanChanged);
    ctx.setColSpan(4);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctx.colSpan(), 4);
}

void TestWidgetInstanceContext::testSetRowSpanEmitsSignal() {
    oap::GridPlacement placement;
    placement.instanceId = "test-signal";
    placement.colSpan = 2;
    placement.rowSpan = 1;

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);
    QSignalSpy spy(&ctx, &oap::WidgetInstanceContext::rowSpanChanged);
    ctx.setRowSpan(3);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctx.rowSpan(), 3);
}

void TestWidgetInstanceContext::testSetColSpanNoopDoesNotEmit() {
    oap::GridPlacement placement;
    placement.instanceId = "test-noop";
    placement.colSpan = 2;
    placement.rowSpan = 1;

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);
    QSignalSpy spy(&ctx, &oap::WidgetInstanceContext::colSpanChanged);
    ctx.setColSpan(2);  // same value
    QCOMPARE(spy.count(), 0);
}

void TestWidgetInstanceContext::testIsCurrentPageDefaultsFalse() {
    oap::GridPlacement placement;
    placement.instanceId = "test-page";

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);
    QCOMPARE(ctx.isCurrentPage(), false);
}

void TestWidgetInstanceContext::testSetIsCurrentPageEmitsSignal() {
    oap::GridPlacement placement;
    placement.instanceId = "test-page";

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);
    QSignalSpy spy(&ctx, &oap::WidgetInstanceContext::isCurrentPageChanged);
    ctx.setIsCurrentPage(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctx.isCurrentPage(), true);
}

void TestWidgetInstanceContext::testSetIsCurrentPageNoopDoesNotEmit() {
    oap::GridPlacement placement;
    placement.instanceId = "test-page";

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, {}, {}, this);
    QSignalSpy spy(&ctx, &oap::WidgetInstanceContext::isCurrentPageChanged);
    ctx.setIsCurrentPage(false);  // same as default
    QCOMPARE(spy.count(), 0);
}

void TestWidgetInstanceContext::testEffectiveConfigDefaults() {
    oap::GridPlacement placement;
    placement.instanceId = "test-config";

    QVariantMap defaults = {{"format", "24h"}, {"showSeconds", false}};
    QVariantMap instance; // empty

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, defaults, instance, this);

    QVariantMap effective = ctx.effectiveConfig();
    QCOMPARE(effective["format"].toString(), QString("24h"));
    QCOMPARE(effective["showSeconds"].toBool(), false);
}

void TestWidgetInstanceContext::testEffectiveConfigOverride() {
    oap::GridPlacement placement;
    placement.instanceId = "test-config";

    QVariantMap defaults = {{"format", "24h"}};
    QVariantMap instance = {{"format", "12h"}};

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, defaults, instance, this);

    QVariantMap effective = ctx.effectiveConfig();
    QCOMPARE(effective["format"].toString(), QString("12h"));
}

void TestWidgetInstanceContext::testEffectiveConfigMerge() {
    oap::GridPlacement placement;
    placement.instanceId = "test-config";

    QVariantMap defaults = {{"keyA", "defaultA"}, {"keyB", "defaultB"}};
    QVariantMap instance = {{"keyB", "overrideB"}};

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, defaults, instance, this);

    QVariantMap effective = ctx.effectiveConfig();
    QCOMPARE(effective["keyA"].toString(), QString("defaultA")); // from defaults
    QCOMPARE(effective["keyB"].toString(), QString("overrideB")); // from instance
}

void TestWidgetInstanceContext::testSetInstanceConfigEmitsSignal() {
    oap::GridPlacement placement;
    placement.instanceId = "test-config";

    QVariantMap defaults = {{"format", "24h"}};
    QVariantMap instance;

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, defaults, instance, this);

    QSignalSpy spy(&ctx, &oap::WidgetInstanceContext::effectiveConfigChanged);
    ctx.setInstanceConfig({{"format", "12h"}});
    QCOMPARE(spy.count(), 1);

    QVariantMap effective = ctx.effectiveConfig();
    QCOMPARE(effective["format"].toString(), QString("12h"));
}

void TestWidgetInstanceContext::testEffectiveConfigEmptyWhenNoSchema() {
    oap::GridPlacement placement;
    placement.instanceId = "test-config";

    QVariantMap defaults; // empty
    QVariantMap instance; // empty

    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, defaults, instance, this);

    QVariantMap effective = ctx.effectiveConfig();
    QVERIFY(effective.isEmpty());
}

void TestWidgetInstanceContext::testContextReplacementDoesNotBreakLiveUpdates() {
    // Regression test: if context A is replaced by context B for the same instanceId,
    // A's deferred destruction must not remove B from the active context map.
    // Without the pointer check in the destroy hook, A's cleanup would remove B
    // and future widgetConfigChanged signals would stop reaching the live widget.

    auto* reg = new oap::WidgetRegistry(this);
    {
        oap::WidgetDescriptor d;
        d.id = "configurable";
        d.displayName = "Configurable";
        d.qmlComponent = QUrl("qrc:/Configurable.qml");
        d.defaultConfig = {{"format", "24h"}};
        d.configSchema = {
            oap::ConfigSchemaField{"format", "Format", oap::ConfigFieldType::Enum,
                {"12-hour", "24-hour"}, {"12h", "24h"}, 0, 0, 0}
        };
        d.minCols = 1; d.minRows = 1;
        reg->registerWidget(d);
    }

    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);
    model.placeWidget("configurable", 0, 0, 1, 1);
    auto placements = model.placements();
    QString instanceId = placements[0].instanceId;

    oap::WidgetContextFactory factory(&model, nullptr, this);

    // Create context A (simulates initial delegate creation)
    auto* parentA = new QObject(this);
    auto* ctxA = qobject_cast<oap::WidgetInstanceContext*>(
        factory.createContext(instanceId, parentA));
    QVERIFY(ctxA);

    // Create context B for the same instanceId (simulates delegate recreation after model reset)
    auto* parentB = new QObject(this);
    auto* ctxB = qobject_cast<oap::WidgetInstanceContext*>(
        factory.createContext(instanceId, parentB));
    QVERIFY(ctxB);
    QVERIFY(ctxA != ctxB);

    // Destroy context A (simulates old delegate cleanup)
    delete parentA;  // destroys ctxA via parent ownership

    // Now set config — widgetConfigChanged should still reach context B
    QSignalSpy spy(ctxB, &oap::WidgetInstanceContext::effectiveConfigChanged);
    model.setWidgetConfig(instanceId, {{"format", "12h"}});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctxB->effectiveConfig()["format"].toString(), QString("12h"));

    delete parentB;
}

void TestWidgetInstanceContext::testMultipleLiveContextsReceiveLiveUpdates() {
    // Regression test for the real QML structure in HomeMenu.qml: adjacent pages can
    // instantiate multiple delegates for the same widget instance at the same time.
    // All live contexts for that instance must receive config updates, not just the
    // most recently created one.

    auto* reg = new oap::WidgetRegistry(this);
    {
        oap::WidgetDescriptor d;
        d.id = "configurable";
        d.displayName = "Configurable";
        d.qmlComponent = QUrl("qrc:/Configurable.qml");
        d.defaultConfig = {{"format", "24h"}};
        d.configSchema = {
            oap::ConfigSchemaField{"format", "Format", oap::ConfigFieldType::Enum,
                {"12-hour", "24-hour"}, {"12h", "24h"}, 0, 0, 0}
        };
        d.minCols = 1;
        d.minRows = 1;
        reg->registerWidget(d);
    }

    oap::WidgetGridModel model(reg);
    model.setGridDimensions(6, 4);
    model.placeWidget("configurable", 0, 0, 1, 1);
    const QString instanceId = model.placements()[0].instanceId;

    oap::WidgetContextFactory factory(&model, nullptr, this);

    auto* parentA = new QObject(this);
    auto* parentB = new QObject(this);
    auto* ctxA = qobject_cast<oap::WidgetInstanceContext*>(
        factory.createContext(instanceId, parentA));
    auto* ctxB = qobject_cast<oap::WidgetInstanceContext*>(
        factory.createContext(instanceId, parentB));

    QVERIFY(ctxA);
    QVERIFY(ctxB);
    QVERIFY(ctxA != ctxB);

    QSignalSpy spyA(ctxA, &oap::WidgetInstanceContext::effectiveConfigChanged);
    QSignalSpy spyB(ctxB, &oap::WidgetInstanceContext::effectiveConfigChanged);

    model.setWidgetConfig(instanceId, {{"format", "12h"}});

    QCOMPARE(spyA.count(), 1);
    QCOMPARE(spyB.count(), 1);
    QCOMPARE(ctxA->effectiveConfig()["format"].toString(), QString("12h"));
    QCOMPARE(ctxB->effectiveConfig()["format"].toString(), QString("12h"));

    delete parentA;
    delete parentB;
}

QTEST_GUILESS_MAIN(TestWidgetInstanceContext)
#include "test_widget_instance_context.moc"
