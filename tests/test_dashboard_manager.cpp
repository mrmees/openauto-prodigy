#include <QtTest/QtTest>
#include <memory>
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

static oap::DashboardConfig makeSpillDashboard(const QString& id, int placementCount) {
    oap::DashboardConfig dashboard;
    dashboard.id = id;
    dashboard.name = id;
    dashboard.pageCount = 2; // retain one intentionally empty baseline page
    for (int i = 0; i < placementCount; ++i) {
        oap::GridPlacement p;
        p.instanceId = id + "-clock-" + QString::number(i);
        p.widgetId = "org.openauto.clock";
        p.col = i; p.row = 0; p.colSpan = 1; p.rowSpan = 1;
        p.opacity = 0.25; p.page = 0; p.visible = true;
        dashboard.placements.append(p);
    }
    dashboard.nextInstanceId = placementCount;
    return dashboard;
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
    void testSlugFallbackAndCollision();
    void testNavPersistSemantics();
    void testConfigOutlivesManagerViaSharedPtr();
    void testSwitchClearsOutgoingSelection();
    void testAutomaticRemapPersistsBaselineAcrossRestart();
    void testRemappedMutationSynchronizesDashboardBaselines();
};

void TestDashboardManager::testFreshLoadSeedsHome() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    auto cfg = std::make_shared<oap::YamlConfig>();
    oap::DashboardManager dm(&reg, nullptr, cfg, path_);
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
    auto cfg = std::make_shared<oap::YamlConfig>();
    oap::DashboardManager dm(&reg, nullptr, cfg, path_);
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
        auto cfg = std::make_shared<oap::YamlConfig>();
        oap::DashboardManager dm(&reg, nullptr, cfg, path_);
        dm.loadFromConfig(6, 4);
        const QString id = dm.addDashboard("Trip");
        dm.switchTo(id);
        dm.activeModel()->placeWidget("org.openauto.clock", 0, 0, 1, 1); // triggers save
    }
    auto cfg2 = std::make_shared<oap::YamlConfig>(); cfg2->load(path_);
    oap::DashboardManager dm2(&reg, nullptr, cfg2, path_);
    dm2.loadFromConfig(6, 4);
    QCOMPARE(dm2.count(), 2);
    QCOMPARE(dm2.activeDashboardId(), QString("trip"));   // restored
    QCOMPARE(dm2.activeModel()->placements().size(), 1);
    // load-before-connect: reload must NOT have clobbered home's seeds
    QCOMPARE(dm2.modelForId("home")->placements().size(), 2);
}

void TestDashboardManager::testRemoveHomeRefused() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    auto cfg = std::make_shared<oap::YamlConfig>();
    oap::DashboardManager dm(&reg, nullptr, cfg, path_);
    dm.loadFromConfig(6, 4);
    QVERIFY(!dm.removeDashboard("home"));
    const QString id = dm.addDashboard("X");
    QVERIFY(!id.isEmpty());
    QVERIFY(!dm.removeDashboard("home"));  // still refused with others present
}

void TestDashboardManager::testWrapNavigation() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    auto cfg = std::make_shared<oap::YamlConfig>();
    oap::DashboardManager dm(&reg, nullptr, cfg, path_);
    dm.loadFromConfig(6, 4);
    dm.addDashboard("B");
    QCOMPARE(dm.activeIndex(), 0);
    dm.nextDashboard(); QCOMPARE(dm.activeIndex(), 1);
    dm.nextDashboard(); QCOMPARE(dm.activeIndex(), 0);   // wraps
    dm.previousDashboard(); QCOMPARE(dm.activeIndex(), 1);
}

void TestDashboardManager::testSlugFallbackAndCollision() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    auto cfg = std::make_shared<oap::YamlConfig>();
    oap::DashboardManager dm(&reg, nullptr, cfg, path_);
    dm.loadFromConfig(6, 4);

    const int before = dm.count();

    // All-punctuation name slugifies to empty -> falls back to "dashboard".
    const QString fallbackId = dm.addDashboard("!!!");
    QCOMPARE(fallbackId, QString("dashboard"));
    QCOMPARE(dm.count(), before + 1);

    const QString firstTripId = dm.addDashboard("Road Trip");
    QCOMPARE(firstTripId, QString("road-trip"));
    QCOMPARE(dm.count(), before + 2);

    // Same name again -> same slug base, must not collide with the first.
    const QString secondTripId = dm.addDashboard("Road Trip");
    QVERIFY(!secondTripId.isEmpty());
    QVERIFY(secondTripId != firstTripId);
    QVERIFY(secondTripId.startsWith(QStringLiteral("road-trip")));
    QCOMPARE(dm.count(), before + 3);

    // idAt / dashboardNames consistency across all three additions.
    QCOMPARE(dm.dashboardNames().size(), dm.count());
    int fallbackIdx = -1, firstIdx = -1, secondIdx = -1;
    for (int i = 0; i < dm.count(); ++i) {
        const QString thisId = dm.idAt(i);
        if (thisId == fallbackId) fallbackIdx = i;
        if (thisId == firstTripId) firstIdx = i;
        if (thisId == secondTripId) secondIdx = i;
    }
    QVERIFY(fallbackIdx >= 0);
    QVERIFY(firstIdx >= 0);
    QVERIFY(secondIdx >= 0);
    QCOMPARE(dm.dashboardNames().at(fallbackIdx), QString("!!!"));
    QCOMPARE(dm.dashboardNames().at(firstIdx), QString("Road Trip"));
    QCOMPARE(dm.dashboardNames().at(secondIdx), QString("Road Trip"));
}

void TestDashboardManager::testNavPersistSemantics() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    QString id;
    {
        auto cfg = std::make_shared<oap::YamlConfig>();
        oap::DashboardManager dm(&reg, nullptr, cfg, path_);
        dm.loadFromConfig(6, 4);

        id = dm.addDashboard("B");
        QVERIFY(!id.isEmpty());

        // addDashboard's saveAll() already ran a full synchronous write —
        // active is still "home" on disk (nav hasn't happened yet).
        oap::YamlConfig onDiskAfterAdd; onDiskAfterAdd.load(path_);
        QCOMPARE(onDiskAfterAdd.activeDashboardId(), QString("home"));

        QSignalSpy activeSpy(&dm, &oap::DashboardManager::activeDashboardChanged);
        QVERIFY(dm.switchTo(id));
        QCOMPARE(activeSpy.count(), 1);

        // Nav must NOT synchronously rewrite the file — only the debounced
        // persistTimer_ does that, and it hasn't fired yet.
        oap::YamlConfig onDiskAfterSwitch; onDiskAfterSwitch.load(path_);
        QCOMPARE(onDiskAfterSwitch.activeDashboardId(), QString("home"));  // still old

        // Already-active short-circuit: switching to the same id again is a
        // no-op — no additional emit, no additional persist scheduling.
        QVERIFY(dm.switchTo(id));
        QCOMPARE(activeSpy.count(), 1);
    } // dm destroyed here -> dtor flushes the pending debounced write

    oap::YamlConfig onDiskAfterDtor; onDiskAfterDtor.load(path_);
    QCOMPARE(onDiskAfterDtor.activeDashboardId(), id);  // now persisted
}

// Regression for the ~DashboardManager use-after-free: main.cpp's YamlConfig
// is declared after QGuiApplication, so stack unwinding at process-exit
// destroyed it BEFORE this app-lifetime manager, and the dtor's pending-persist
// flush (config_->save()) dereferenced freed memory. The fix makes the
// manager hold its own std::shared_ptr ref to the config, so even after an
// external owner drops its reference, the config stays alive until the
// manager itself is destroyed. Not run pre-fix: a UAF here would manifest as
// a crash or silent memory corruption rather than a clean assertion failure
// (undefined behavior isn't reliably reproducible), so RED is skipped as
// acceptable for this case per the task brief -- the shared_ptr type change
// itself is the fix, and this test only proves the happy path stays correct.
void TestDashboardManager::testConfigOutlivesManagerViaSharedPtr() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    QString id;
    {
        auto cfg = std::make_shared<oap::YamlConfig>();
        oap::DashboardManager dm(&reg, nullptr, cfg, path_);
        dm.loadFromConfig(6, 4);

        id = dm.addDashboard("Other");
        QVERIFY(!id.isEmpty());
        QVERIFY(dm.switchTo(id));   // starts the debounced pending persist

        cfg.reset();   // drop the external reference -- dm's own ref must
                        // be the only thing keeping YamlConfig alive now
    }   // dm destroyed here: dtor flush must not UAF against the dropped cfg

    oap::YamlConfig onDisk;
    onDisk.load(path_);
    QCOMPARE(onDisk.activeDashboardId(), id);  // dtor flush succeeded
}

// Regression for the deselect-on-switch bug: switchTo (and friends) must
// clear the OUTGOING model's selection latch before emitting
// activeDashboardChanged, not rely on QML to do it after the context
// re-point (by then QML's WidgetGridModel context property already points
// at the INCOMING model). widgetDeselectedFromCpp is the only observable
// proxy WidgetGridModel exposes for "selection was cleared" (there's no
// public getter for widgetSelected_), and it only fires on an actual
// true->false transition -- exactly what we need to prove happened on the
// home model specifically.
void TestDashboardManager::testSwitchClearsOutgoingSelection() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    auto cfg = std::make_shared<oap::YamlConfig>();
    oap::DashboardManager dm(&reg, nullptr, cfg, path_);
    dm.loadFromConfig(6, 4);

    auto* homeModel = dm.activeModel();
    QVERIFY(homeModel != nullptr);
    homeModel->setWidgetSelected(true);

    QSignalSpy deselectSpy(homeModel, &oap::WidgetGridModel::widgetDeselectedFromCpp);

    const QString id = dm.addDashboard("Other");
    QVERIFY(!id.isEmpty());
    QVERIFY(dm.switchTo(id));

    QCOMPARE(deselectSpy.count(), 1);  // home (outgoing) model's latch was cleared
}

void TestDashboardManager::testAutomaticRemapPersistsBaselineAcrossRestart() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    {
        oap::YamlConfig initial;
        initial.setDashboards({makeSpillDashboard("home", 3)});
        initial.setGridSavedDims(6, 4);
        QVERIFY(initial.save(path_));
    }

    {
        auto cfg = std::make_shared<oap::YamlConfig>();
        cfg->load(path_);
        oap::DashboardManager dm(&reg, nullptr, cfg, path_);
        dm.loadFromConfig(6, 4);
        dm.setGridDimensions(1, 1);
        QCOMPARE(dm.activeModel()->pageCount(), 3); // live spill is reachable
        QVERIFY(dm.renameDashboard("home", "Renamed")); // unrelated full save stays baseline-only

        oap::YamlConfig onDisk;
        onDisk.load(path_);
        QCOMPARE(onDisk.gridSavedCols(), 6);
        QCOMPARE(onDisk.gridSavedRows(), 4);
        const auto persisted = onDisk.dashboards().constFirst();
        QCOMPARE(persisted.pageCount, 2);
        for (const auto& p : persisted.placements)
            QCOMPARE(p.page, 0);
    }

    auto restartedConfig = std::make_shared<oap::YamlConfig>();
    restartedConfig->load(path_);
    oap::DashboardManager restarted(&reg, nullptr, restartedConfig, path_);
    restarted.loadFromConfig(1, 1); // restart while still constrained
    QCOMPARE(restarted.activeModel()->pageCount(), 3);

    restarted.setGridDimensions(6, 4);
    QCOMPARE(restarted.activeModel()->pageCount(), 2);
    const auto restored = restarted.activeModel()->placements();
    QCOMPARE(restored.size(), 3);
    for (int i = 0; i < restored.size(); ++i) {
        QCOMPARE(restored[i].page, 0);
        QCOMPARE(restored[i].col, i);
    }
}

void TestDashboardManager::testRemappedMutationSynchronizesDashboardBaselines() {
    oap::WidgetRegistry reg; registerSeedWidgets(reg);
    {
        oap::YamlConfig initial;
        initial.setDashboards({makeSpillDashboard("home", 3),
                               makeSpillDashboard("work", 3)});
        initial.setGridSavedDims(6, 4);
        QVERIFY(initial.save(path_));
    }

    {
        auto cfg = std::make_shared<oap::YamlConfig>();
        cfg->load(path_);
        oap::DashboardManager dm(&reg, nullptr, cfg, path_);
        dm.loadFromConfig(6, 4);
        dm.setGridDimensions(1, 1);
        QCOMPARE(dm.modelForId("home")->pageCount(), 3);
        QCOMPARE(dm.modelForId("work")->pageCount(), 3);

        dm.modelForId("home")->setWidgetOpacity("home-clock-0", 0.5);

        oap::YamlConfig committed;
        committed.load(path_);
        QCOMPARE(committed.gridSavedCols(), 1);
        QCOMPARE(committed.gridSavedRows(), 1);
        const auto dashboards = committed.dashboards();
        QCOMPARE(dashboards.size(), 2);
        for (const auto& dashboard : dashboards) {
            QCOMPARE(dashboard.pageCount, 3);
            bool hasSpillPage = false;
            for (const auto& p : dashboard.placements)
                hasSpillPage = hasSpillPage || p.page == 2;
            QVERIFY(hasSpillPage);
        }

        dm.modelForId("home")->addPage();
        oap::YamlConfig afterAdd;
        afterAdd.load(path_);
        QCOMPARE(afterAdd.dashboards().constFirst().pageCount, 4);
        QVERIFY(dm.modelForId("home")->removePage(3));
        oap::YamlConfig afterRemove;
        afterRemove.load(path_);
        QCOMPARE(afterRemove.dashboards().constFirst().pageCount, 3);
    }

    auto reloadedConfig = std::make_shared<oap::YamlConfig>();
    reloadedConfig->load(path_);
    oap::DashboardManager reloaded(&reg, nullptr, reloadedConfig, path_);
    reloaded.loadFromConfig(1, 1);
    QCOMPARE(reloaded.modelForId("home")->pageCount(), 3);
    QCOMPARE(reloaded.modelForId("work")->pageCount(), 3);
    reloaded.setGridDimensions(6, 4);
    QCOMPARE(reloaded.modelForId("home")->pageCount(), 3);
    QCOMPARE(reloaded.modelForId("work")->pageCount(), 3);
}

QTEST_MAIN(TestDashboardManager)
#include "test_dashboard_manager.moc"
