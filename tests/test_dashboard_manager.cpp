#include <QtTest/QtTest>
#include "ui/DashboardManager.hpp"
#include "ui/WidgetGridModel.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "core/YamlConfig.hpp"

static void registerSeedWidgets(oap::WidgetRegistry& reg) {
    // Seeding targets these singleton ids (moved from main.cpp) — the test
    // registry must know them or setPlacements drops the seeds.
    for (const char* id : {"org.openauto.aa-launcher", "org.openauto.settings-launcher"}) {
        oap::WidgetDescriptor d;
        d.id = id; d.displayName = id;
        d.qmlComponent = QUrl("qrc:/x/Stub.qml");
        d.singleton = true;
        reg.registerWidget(d);
    }
    oap::WidgetDescriptor clock;
    clock.id = "org.openauto.clock"; clock.displayName = "Clock";
    clock.qmlComponent = QUrl("qrc:/x/Clock.qml");
    reg.registerWidget(clock);
}

class TestDashboardManager : public QObject {
    Q_OBJECT
    QString path_;
private slots:
    void init() { path_ = QDir::temp().filePath("oap_test_dm.yaml"); QFile::remove(path_); }
    void testFreshLoadSeedsHome();
    void testAddSwitchRemove();
    void testPersistAcrossReload();
    void testRemoveHomeRefused();
    void testWrapNavigation();
};

void TestDashboardManager::testFreshLoadSeedsHome() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    oap::YamlConfig cfg;
    oap::DashboardManager dm(&reg, nullptr, &cfg, path_);
    dm.loadFromConfig(6, 4);
    QCOMPARE(dm.count(), 1);
    QCOMPARE(dm.activeDashboardId(), QString("home"));
    QVERIFY(dm.activeModel() != nullptr);
    QVERIFY(dm.activeFactory() != nullptr);
    // Fresh install: reserved launcher page seeded (2 singletons on last page)
    QCOMPARE(dm.activeModel()->placements().size(), 2);
}

void TestDashboardManager::testAddSwitchRemove() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    oap::YamlConfig cfg;
    oap::DashboardManager dm(&reg, nullptr, &cfg, path_);
    dm.loadFromConfig(6, 4);
    QSignalSpy dashSpy(&dm, &oap::DashboardManager::dashboardsChanged);
    QSignalSpy activeSpy(&dm, &oap::DashboardManager::activeDashboardChanged);

    const QString id = dm.addDashboard("Road Trip");
    QVERIFY(!id.isEmpty());
    QCOMPARE(dm.count(), 2);
    QVERIFY(dashSpy.count() >= 1);
    auto* homeModel = dm.activeModel();
    QVERIFY(dm.switchTo(id));
    QVERIFY(activeSpy.count() >= 1);
    QVERIFY(dm.activeModel() != homeModel);
    QCOMPARE(dm.activeModel()->placements().size(), 0);
    QVERIFY(!dm.switchTo("nope"));

    QVERIFY(dm.removeDashboard(id));       // removing the ACTIVE one falls back to home
    QCOMPARE(dm.count(), 1);
    QCOMPARE(dm.activeDashboardId(), QString("home"));
}

void TestDashboardManager::testPersistAcrossReload() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    {
        oap::YamlConfig cfg;
        oap::DashboardManager dm(&reg, nullptr, &cfg, path_);
        dm.loadFromConfig(6, 4);
        const QString id = dm.addDashboard("Trip");
        dm.switchTo(id);
        dm.activeModel()->placeWidget("org.openauto.clock", 0, 0, 1, 1); // triggers save
    }
    oap::YamlConfig cfg2; cfg2.load(path_);
    oap::DashboardManager dm2(&reg, nullptr, &cfg2, path_);
    dm2.loadFromConfig(6, 4);
    QCOMPARE(dm2.count(), 2);
    QCOMPARE(dm2.activeDashboardId(), QString("trip"));   // restored
    QCOMPARE(dm2.activeModel()->placements().size(), 1);
    // load-before-connect: reload must NOT have clobbered home's seeds
    QCOMPARE(dm2.modelForId("home")->placements().size(), 2);
}

void TestDashboardManager::testRemoveHomeRefused() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    oap::YamlConfig cfg;
    oap::DashboardManager dm(&reg, nullptr, &cfg, path_);
    dm.loadFromConfig(6, 4);
    QVERIFY(!dm.removeDashboard("home"));
    const QString id = dm.addDashboard("X");
    QVERIFY(!id.isEmpty());
    QVERIFY(!dm.removeDashboard("home"));  // still refused with others present
}

void TestDashboardManager::testWrapNavigation() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    oap::YamlConfig cfg;
    oap::DashboardManager dm(&reg, nullptr, &cfg, path_);
    dm.loadFromConfig(6, 4);
    dm.addDashboard("B");
    QCOMPARE(dm.activeIndex(), 0);
    dm.nextDashboard(); QCOMPARE(dm.activeIndex(), 1);
    dm.nextDashboard(); QCOMPARE(dm.activeIndex(), 0);   // wraps
    dm.previousDashboard(); QCOMPARE(dm.activeIndex(), 1);
}

QTEST_MAIN(TestDashboardManager)
#include "test_dashboard_manager.moc"
