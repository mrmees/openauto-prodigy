// tests/test_widget_instance_context.cpp
#include <QtTest/QtTest>
#include "ui/WidgetInstanceContext.hpp"
#include "core/widget/WidgetTypes.hpp"
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
    oap::WidgetInstanceContext ctx(placement, 100, 100, hostContext.get(), this);

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
    oap::WidgetInstanceContext ctx(placement, 100, 100, nullptr, this);

    // Without host context, providers should be null
    QVERIFY(ctx.property("projectionStatus").value<QObject*>() == nullptr);
    QVERIFY(ctx.property("navigationProvider").value<QObject*>() == nullptr);
    QVERIFY(ctx.property("mediaStatus").value<QObject*>() == nullptr);
}

QTEST_GUILESS_MAIN(TestWidgetInstanceContext)
#include "test_widget_instance_context.moc"
