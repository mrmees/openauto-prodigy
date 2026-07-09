# Multi-Dashboards Implementation Plan

Status: COMPLETED 2026-07-06

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Multiple named dashboards, each its own widget grid with pages, switchable via actions and an edit-mode pill UI, persisted as YAML v4 `dashboards[]`.

**Architecture:** `DashboardManager` owns one (`WidgetGridModel` + `WidgetContextFactory`) pair per dashboard and re-points the existing `WidgetGridModel`/`WidgetContextFactory` root context properties on switch (context properties are dynamic — bindings re-resolve; zero mechanical QML churn). YAML `widget_grid` migrates v3→v4 (`dashboards[]`, v3 becomes `dashboards[0]` "Home"). Spec: `docs/superpowers/specs/2026-07-05-dashboards-overlays-design.md` §3 — read it first, plus §6 Executor Guidance.

**Tech Stack:** Qt 6.8, yaml-cpp, CMake/ctest.

## Global Constraints

- v3 per-placement field names are reused verbatim inside `dashboards[]` — do not rename any placement key.
- `DashboardContributionKind::Widget/LiveSurfaceWidget` order is frozen; `WebWidget` appends after.
- Load-before-connect ordering per dashboard (spec §6.4): placements load BEFORE save signals connect, or boot wipes user config.
- `removeDashboard("home")` refused always (singleton/reserved page lives there).
- All mutation via ActionRegistry actions or service invokables (rail R4).
- Migration idempotent: only `widget_grid.version == 3` triggers the wrap; v4 files pass through untouched.
- Build: `cd build && cmake .. && make -j$(nproc)`; test: `ctest --output-on-failure`; final gate adds `./cross-build.sh`.
- Commit per task, existing message style.

---

### Task 1: YAML v4 — `DashboardConfig`, accessors, v3→v4 migration

**Files:**
- Modify: `src/core/widget/WidgetTypes.hpp` (add struct)
- Modify: `src/core/YamlConfig.hpp`, `src/core/YamlConfig.cpp`
- Modify: `src/core/services/ConfigService.*` only if it proxies grid accessors (verify: `grep -n "gridPlacements\|gridPageCount" src/core/services/ConfigService.*` — expected: no hits, no change)
- Test: `tests/test_yaml_config.cpp` (extend)

**Interfaces:**
- Produces (Tasks 2, 4 rely on these exact names):
```cpp
// WidgetTypes.hpp
struct DashboardConfig {
    QString id;                       // stable slug ("home")
    QString name;                     // display label ("Home")
    int nextInstanceId = 0;
    int pageCount = 2;
    QList<GridPlacement> placements;
};
// YamlConfig
QList<DashboardConfig> dashboards() const;
void setDashboards(const QList<DashboardConfig>& list);
QString activeDashboardId() const;                 // default "home"
void setActiveDashboardId(const QString& id);
// gridSavedCols()/gridSavedRows()/setGridSavedDims() remain unchanged (global).
```
- **Deletes** the flat v3 accessors `gridPlacements/setGridPlacements/gridNextInstanceId/setGridNextInstanceId/gridPageCount/setGridPageCount` (their only consumer is the main.cpp block Task 4 rewrites; verify with `grep -rn "gridPlacements\|gridPageCount\|gridNextInstanceId" src/ tests/ web-config/` first — if anything else consumes them, stop and reassess).

- [ ] **Step 1: Write failing tests** (append to `tests/test_yaml_config.cpp`, add slots to the class):
```cpp
void TestYamlConfig::testV4DefaultsOneHomeDashboard() {
    oap::YamlConfig cfg;
    auto ds = cfg.dashboards();
    QCOMPARE(ds.size(), 1);
    QCOMPARE(ds[0].id, QString("home"));
    QCOMPARE(ds[0].name, QString("Home"));
    QCOMPARE(ds[0].pageCount, 2);
    QVERIFY(ds[0].placements.isEmpty());
    QCOMPARE(cfg.activeDashboardId(), QString("home"));
}

void TestYamlConfig::testDashboardsRoundTrip() {
    oap::YamlConfig cfg;
    oap::DashboardConfig home{ "home", "Home", 5, 2, {} };
    oap::GridPlacement p;
    p.instanceId = "org.openauto.clock-0"; p.widgetId = "org.openauto.clock";
    p.col = 1; p.row = 2; p.colSpan = 2; p.rowSpan = 1; p.opacity = 0.5; p.page = 0;
    p.config["style"] = QString("analog");
    home.placements.append(p);
    oap::DashboardConfig work{ "work", "Work", 0, 1, {} };
    cfg.setDashboards({home, work});
    cfg.setActiveDashboardId("work");

    const QString path = QDir::temp().filePath("oap_test_dash_v4.yaml");
    cfg.save(path);
    oap::YamlConfig loaded;
    loaded.load(path);
    auto ds = loaded.dashboards();
    QCOMPARE(ds.size(), 2);
    QCOMPARE(ds[0].nextInstanceId, 5);
    QCOMPARE(ds[0].placements.size(), 1);
    QCOMPARE(ds[0].placements[0].colSpan, 2);
    QCOMPARE(ds[0].placements[0].config.value("style").toString(), QString("analog"));
    QCOMPARE(ds[1].id, QString("work"));
    QCOMPARE(ds[1].pageCount, 1);
    QCOMPARE(loaded.activeDashboardId(), QString("work"));
}

void TestYamlConfig::testV3MigratesToV4() {
    // Literal v3 file — exactly what a pre-migration install has on disk.
    const QString path = QDir::temp().filePath("oap_test_dash_v3.yaml");
    {
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("widget_grid:\n"
                "  version: 3\n"
                "  next_instance_id: 7\n"
                "  page_count: 3\n"
                "  saved_cols: 8\n"
                "  saved_rows: 4\n"
                "  placements:\n"
                "    - instance_id: org.openauto.clock-4\n"
                "      widget_id: org.openauto.clock\n"
                "      col: 0\n      row: 0\n      col_span: 2\n      row_span: 2\n"
                "      opacity: 0.25\n      page: 1\n");
    }
    oap::YamlConfig cfg;
    cfg.load(path);
    auto ds = cfg.dashboards();
    QCOMPARE(ds.size(), 1);
    QCOMPARE(ds[0].id, QString("home"));
    QCOMPARE(ds[0].nextInstanceId, 7);
    QCOMPARE(ds[0].pageCount, 3);
    QCOMPARE(ds[0].placements.size(), 1);
    QCOMPARE(ds[0].placements[0].page, 1);
    QCOMPARE(cfg.activeDashboardId(), QString("home"));
    QCOMPARE(cfg.gridSavedCols(), 8);            // global key untouched
    // Idempotence: save then reload — still one dashboard, still v4 shape.
    cfg.save(path);
    oap::YamlConfig again; again.load(path);
    QCOMPARE(again.dashboards().size(), 1);
    QCOMPARE(again.dashboards()[0].nextInstanceId, 7);
}
```

- [ ] **Step 2: Verify failure** — `cd build && cmake .. && make -j$(nproc) 2>&1 | tail -5` → compile FAIL.

- [ ] **Step 3: Implement.**

`initDefaults()` — replace the v3 block (`YamlConfig.cpp:158-161`) with:
```cpp
    root_["widget_grid"]["version"] = 4;
    root_["widget_grid"]["active_dashboard"] = "home";
    {
        YAML::Node home;
        home["id"] = "home";
        home["name"] = "Home";
        home["next_instance_id"] = 0;
        home["page_count"] = 2;
        home["placements"] = YAML::Node(YAML::NodeType::Sequence);
        YAML::Node seq(YAML::NodeType::Sequence);
        seq.push_back(home);
        root_["widget_grid"]["dashboards"] = seq;
    }
```

Refactor the placement (de)serialization out of the deleted flat accessors into two private helpers (move the loop bodies verbatim from `gridPlacements()`/`setGridPlacements()`, `YamlConfig.cpp:822-918` — including the scalar type-sniffing for config values):
```cpp
static QList<GridPlacement> placementsFromNode(const YAML::Node& placements);
static YAML::Node placementsToNode(const QList<GridPlacement>& placements);
```

New accessors:
```cpp
QList<DashboardConfig> YamlConfig::dashboards() const
{
    QList<DashboardConfig> result;
    auto seq = root_["widget_grid"]["dashboards"];
    if (!seq.IsDefined() || !seq.IsSequence()) return result;
    for (const auto& n : seq) {
        DashboardConfig d;
        d.id = QString::fromStdString(n["id"].as<std::string>(""));
        d.name = QString::fromStdString(n["name"].as<std::string>(d.id.toStdString()));
        d.nextInstanceId = n["next_instance_id"].as<int>(0);
        d.pageCount = n["page_count"].as<int>(1);
        d.placements = placementsFromNode(n["placements"]);
        if (!d.id.isEmpty()) result.append(d);
    }
    return result;
}

void YamlConfig::setDashboards(const QList<DashboardConfig>& list)
{
    root_["widget_grid"]["version"] = 4;
    YAML::Node seq(YAML::NodeType::Sequence);
    for (const auto& d : list) {
        YAML::Node n;
        n["id"] = d.id.toStdString();
        n["name"] = d.name.toStdString();
        n["next_instance_id"] = d.nextInstanceId;
        n["page_count"] = d.pageCount;
        n["placements"] = placementsToNode(d.placements);
        seq.push_back(n);
    }
    root_["widget_grid"]["dashboards"] = seq;
}

QString YamlConfig::activeDashboardId() const
{
    return QString::fromStdString(
        root_["widget_grid"]["active_dashboard"].as<std::string>("home"));
}

void YamlConfig::setActiveDashboardId(const QString& id)
{
    root_["widget_grid"]["active_dashboard"] = id.toStdString();
}
```

Migration — private `migrateWidgetGridV3()`, called at the END of `load()` (after `mergeYaml`):
```cpp
void YamlConfig::migrateWidgetGridV3()
{
    YAML::Node wg = root_["widget_grid"];
    if (!wg.IsDefined() || wg["version"].as<int>(4) != 3) return;

    YAML::Node home;
    home["id"] = "home";
    home["name"] = "Home";
    home["next_instance_id"] = wg["next_instance_id"].as<int>(0);
    home["page_count"] = wg["page_count"].as<int>(2);
    home["placements"] = (wg["placements"].IsDefined() && wg["placements"].IsSequence())
                             ? wg["placements"] : YAML::Node(YAML::NodeType::Sequence);
    YAML::Node seq(YAML::NodeType::Sequence);
    seq.push_back(home);
    wg["dashboards"] = seq;                 // replaces the defaults-merged empty v4 skeleton
    wg["active_dashboard"] = "home";
    wg["version"] = 4;
    wg.remove("next_instance_id");
    wg.remove("page_count");
    wg.remove("placements");
}
```
Caution: `mergeYaml(defaults, loaded)` runs first — a loaded v3 file's `version: 3` wins over the default 4 (loaded wins on scalars), which is exactly the migration trigger. Verify sequence-merge semantics don't duplicate: the test's literal v3 file is the proof.

Delete the six flat accessors from .hpp/.cpp (and any existing `testGridPlacements*` tests in `test_yaml_config.cpp` — port their assertions onto `dashboards()` round-trips if they exercise config-value type sniffing; do not lose that coverage).

- [ ] **Step 4: Run** `ctest -R test_yaml_config --output-on-failure` → PASS; then full `ctest --output-on-failure` — expect main.cpp compile failure? No: main.cpp still references deleted accessors. **This task must land together with a minimal main.cpp bridge OR compile is broken.** To keep the task self-contained and green: in this task, patch `main.cpp:673-720` minimally — replace the deleted calls with the v4 equivalent for a single dashboard:
```cpp
    auto dashList = yamlConfig->dashboards();
    oap::DashboardConfig homeDash = dashList.isEmpty()
        ? oap::DashboardConfig{ "home", "Home", 0, 2, {} } : dashList.first();
    widgetGridModel->setPageCount(homeDash.pageCount);
    if (!homeDash.placements.isEmpty()) {
        widgetGridModel->setPlacements(homeDash.placements, widgetRegistry);
        widgetGridModel->setNextInstanceId(homeDash.nextInstanceId);
    }
    widgetGridModel->setSavedDimensions(yamlConfig->gridSavedCols(), yamlConfig->gridSavedRows());

    auto saveGridState = [yamlConfig = yamlConfig.get(), widgetGridModel, yamlPath]() {
        oap::DashboardConfig d{ "home", "Home",
            widgetGridModel->nextInstanceId(), widgetGridModel->pageCount(),
            widgetGridModel->placements() };
        yamlConfig->setDashboards({d});
        yamlConfig->setGridSavedDims(widgetGridModel->gridColumns(), widgetGridModel->gridRows());
        yamlConfig->save(yamlPath);
    };
```
(The seeding block's `savedPlacements.isEmpty()` check becomes `homeDash.placements.isEmpty()`.) Task 4 replaces this bridge wholesale — it exists so every commit builds and boots.

- [ ] **Step 5: Full suite + commit**
```bash
git add src/core/widget/WidgetTypes.hpp src/core/YamlConfig.hpp src/core/YamlConfig.cpp \
        src/main.cpp tests/test_yaml_config.cpp
git commit -m "feat(config): widget_grid v4 — dashboards[] with v3 migration"
```

---

### Task 2: `DashboardManager`

**Files:**
- Create: `src/ui/DashboardManager.hpp`, `src/ui/DashboardManager.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES, next to `ui/WidgetGridModel.cpp`)
- Test: `tests/test_dashboard_manager.cpp`; Modify: `tests/CMakeLists.txt` (`oap_add_test(test_dashboard_manager SOURCES test_dashboard_manager.cpp)`)

**Interfaces:**
- Consumes: Task 1 accessors; `WidgetGridModel(registry, parent)`, `setGridDimensions/setPageCount/setPlacements/setNextInstanceId/setSavedDimensions/placements()/nextInstanceId()/pageCount()/placementsChanged/pageCountChanged`; `WidgetContextFactory(gridModel, hostContext, parent)`.
- Produces (Tasks 4, 5 rely on exact names):
```cpp
namespace oap {
class DashboardManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activeDashboardId READ activeDashboardId NOTIFY activeDashboardChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeDashboardChanged)
    Q_PROPERTY(int count READ count NOTIFY dashboardsChanged)
    Q_PROPERTY(QStringList dashboardNames READ dashboardNames NOTIFY dashboardsChanged)
public:
    DashboardManager(WidgetRegistry* registry, IHostContext* hostContext,
                     YamlConfig* config, const QString& configPath,
                     QObject* parent = nullptr);
    void loadFromConfig(int initialCols, int initialRows);  // builds models; seeds home; connects saves LAST
    WidgetGridModel* activeModel() const;
    WidgetContextFactory* activeFactory() const;
    WidgetGridModel* modelForId(const QString& id) const;   // nullptr if unknown
    QString activeDashboardId() const; int activeIndex() const;
    int count() const; QStringList dashboardNames() const;
    Q_INVOKABLE bool switchTo(const QString& id);
    Q_INVOKABLE bool switchToIndex(int index);
    Q_INVOKABLE void nextDashboard();      // wraps
    Q_INVOKABLE void previousDashboard();  // wraps
    Q_INVOKABLE QString addDashboard(const QString& name);  // "" on refusal (cap 8 / empty name)
    Q_INVOKABLE bool removeDashboard(const QString& id);    // refuses "home" and last-remaining
    Q_INVOKABLE bool renameDashboard(const QString& id, const QString& name);
    Q_INVOKABLE QString idAt(int index) const;
    Q_INVOKABLE void setGridDimensions(int cols, int rows); // fans out to ALL models
signals:
    void activeDashboardChanged();
    void dashboardsChanged();
};
}
```

- [ ] **Step 1: Write failing tests** — `tests/test_dashboard_manager.cpp`:
```cpp
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
```

- [ ] **Step 2: Verify failure.** **Step 3: Implement.**

Implementation notes (complete the header per the Interfaces block; .cpp guidance):
- `Entry { QString id; QString name; WidgetGridModel* model; WidgetContextFactory* factory; }`, `QList<Entry> entries_`, `int active_ = 0`, `bool loading_ = false`.
- `loadFromConfig(cols, rows)`: read `config_->dashboards()` (guaranteed ≥1 by defaults/migration; if empty anyway, synthesize home). Per entry: `new WidgetGridModel(registry_, this)` → `setGridDimensions(cols, rows)` → `setPageCount` → `setPlacements(d.placements, registry_)` (skip when empty) → `setNextInstanceId` → `setSavedDimensions(config_->gridSavedCols(), config_->gridSavedRows())` → `new WidgetContextFactory(model, hostContext_, this)`. **Seed home** if its placements are empty: the two reserved singleton placements verbatim from the old `main.cpp:696-720` block (instanceIds `aa-launcher-reserved`/`settings-launcher-reserved`, page = `pageCount-1`, col 0 / rows 0,1). Restore `active_` from `config_->activeDashboardId()` (fallback 0). **Then, last**, connect every model's `placementsChanged` + `pageCountChanged` to `saveAll()` (guarded by `loading_` during this function).
- `saveAll()`: build `QList<DashboardConfig>` from `entries_` (`model->placements()/nextInstanceId()/pageCount()`), `config_->setDashboards(list)`, `config_->setActiveDashboardId(activeDashboardId())`, `config_->setGridSavedDims(activeModel()->gridColumns(), activeModel()->gridRows())`, `config_->save(configPath_)`.
- `switchTo`: find id, set `active_`, `saveAll()`, emit `activeDashboardChanged`. `addDashboard`: refuse when `entries_.size() >= 8` or trimmed name empty; id = lowercased name, non-alnum → `-`, collapse repeats, numeric suffix on collision; new model gets `setGridDimensions(activeModel()->gridColumns(), activeModel()->gridRows())`, `setPageCount(1)`; connect saves; `saveAll()`; emit `dashboardsChanged`. `removeDashboard`: refuse `"home"` or `entries_.size()==1`; if removing active, switch to home first; `deleteLater()` model+factory; `saveAll()`. `renameDashboard`: name only (id stable), `saveAll()`, emit `dashboardsChanged`. `setGridDimensions`: forward to every model.
- Includes: `WidgetGridModel.hpp`, `WidgetContextFactory.hpp`, `core/widget/WidgetRegistry.hpp`, `core/YamlConfig.hpp` in .cpp; forward-declare in .hpp.

- [ ] **Step 4: Run** `ctest -R test_dashboard_manager --output-on-failure` → PASS. Full suite → PASS.
- [ ] **Step 5: Commit** — `git add src/ui/DashboardManager.* src/CMakeLists.txt tests/test_dashboard_manager.cpp tests/CMakeLists.txt && git commit -m "feat(ui): DashboardManager — named dashboards over per-dashboard grid models"`

---

### Task 3: `WebWidget` contribution kind

**Files:** Modify `src/core/widget/WidgetTypes.hpp`, `src/core/widget/WidgetRegistry.cpp`; Test: extend the existing registry test (`grep -l WidgetRegistry tests/*.cpp` — expected `tests/test_widget_registry.cpp`).

- [ ] **Step 1: Failing test** (in the registry test file):
```cpp
void TestWidgetRegistry::testWebWidgetKindFitsSpace() {
    oap::WidgetRegistry reg;
    oap::WidgetDescriptor d;
    d.id = "com.example.webclock"; d.displayName = "Web Clock";
    d.qmlComponent = QUrl("qrc:/OpenAutoProdigy/qml/widgets/WebWidgetHost.qml");
    d.contributionKind = oap::DashboardContributionKind::WebWidget;
    reg.registerWidget(d);
    auto fits = reg.widgetsFittingSpace(6, 4);
    bool found = false;
    for (const auto& w : fits) if (w.id == d.id) found = true;
    QVERIFY(found);
}
```
- [ ] **Step 2: Verify failure** (enum value missing). **Step 3: Implement** — `WidgetTypes.hpp`: append `WebWidget,` after `LiveSurfaceWidget` (order frozen); `WidgetRegistry.cpp` filter becomes:
```cpp
            && (desc.contributionKind == DashboardContributionKind::Widget
                || desc.contributionKind == DashboardContributionKind::WebWidget)
```
- [ ] **Step 4: Run registry test + full suite → PASS. Step 5: Commit** — `git commit -am "feat(widgets): WebWidget contribution kind accepted by the grid picker path"`

---

### Task 4: main.cpp — DashboardManager wiring, actions, context re-pointing

**Files:** Modify `src/main.cpp`.

- [ ] **Step 1: Replace the grid block.** Delete the Task-1 bridge (the whole `widgetGridModel` construction + load + save + seeding region, originally `main.cpp:660-727`) and the `widgetContextFactory` construction (`main.cpp:923-925`). Insert (same position as the old grid block; include `"ui/DashboardManager.hpp"`):
```cpp
    // --- Dashboards: per-dashboard widget grids (design 2026-07-05 §3) ---
    auto dashboardManager = new oap::DashboardManager(
        widgetRegistry, hostContext.get(), yamlConfig.get(), yamlPath, &app);
    {
        qreal cs = displayInfo->cellSide();
        int initCols = qMax(3, static_cast<int>(std::floor(displayInfo->windowWidth() / cs)));
        int initRows = qMax(2, static_cast<int>(std::floor(displayInfo->windowHeight() / cs)));
        dashboardManager->loadFromConfig(initCols, initRows);
    }
```
- [ ] **Step 2: Re-target every `widgetGridModel` capture.** `grep -n "widgetGridModel" src/main.cpp` — for each action lambda (`app.home`, `app.launchPlugin`, `app.openSettings`, all `navbar.*` at `main.cpp:763-870` region): capture `dashboardManager` instead and call `dashboardManager->activeModel()->setWidgetSelected(false)` (and any other member the lambda used, via `activeModel()`). After the sweep, `grep -n "widgetGridModel" src/main.cpp` MUST return zero hits.
- [ ] **Step 3: Register dashboard actions** (beside the `app.*` block at `main.cpp:763`):
```cpp
    actionRegistry->registerAction("app.dashboard.next",
        [dashboardManager](const QVariant&) { dashboardManager->nextDashboard(); });
    actionRegistry->registerAction("app.dashboard.previous",
        [dashboardManager](const QVariant&) { dashboardManager->previousDashboard(); });
    actionRegistry->registerAction("app.dashboard.select",
        [dashboardManager](const QVariant& v) { dashboardManager->switchTo(v.toString()); });
```
- [ ] **Step 4: Context properties** (where the old ones were set, `main.cpp:923-927`):
```cpp
    engine.rootContext()->setContextProperty("DashboardManager", dashboardManager);
    auto repointGridContext = [&engine, dashboardManager]() {
        engine.rootContext()->setContextProperty("WidgetGridModel", dashboardManager->activeModel());
        engine.rootContext()->setContextProperty("WidgetContextFactory", dashboardManager->activeFactory());
    };
    repointGridContext();
    QObject::connect(dashboardManager, &oap::DashboardManager::activeDashboardChanged,
                     &engine, repointGridContext);
```
- [ ] **Step 5: Build + full suite + boot check.** `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`. If a desktop session is available, run the binary briefly: home grid renders, widgets place, restart restores.
- [ ] **Step 6: Commit** — `git commit -am "feat(ui): wire DashboardManager — actions, context re-pointing, per-dashboard persistence"`

---

### Task 5: Dashboard switcher UI (edit-mode pills)

**Files:** Create `qml/components/DashboardSwitcher.qml`; Modify `qml/applications/home/HomeMenu.qml` (instantiate + one call-site swap); Modify `qml/CMakeLists.txt` or the qrc listing QML files (find the registration list: `grep -rn "WidgetPickerSheet" qml/CMakeLists.txt src/CMakeLists.txt *.qrc 2>/dev/null` and add `DashboardSwitcher.qml` the same way).

- [ ] **Step 1: Create `qml/components/DashboardSwitcher.qml`:**
```qml
import QtQuick
import QtQuick.Layouts

/// Dashboard pill row — visible only while a widget is selected (edit mode).
/// Tap a pill to switch (via action, rail R4); "+" adds; long-press renames/removes.
Item {
    id: root
    property bool editing: false      // bound by HomeMenu to its selection state
    visible: editing && DashboardManager.count > 0
    implicitHeight: UiMetrics.tileH * 0.35

    RowLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        height: parent.height
        spacing: UiMetrics.spacing

        Repeater {
            model: DashboardManager.dashboardNames
            delegate: Rectangle {
                Layout.preferredHeight: root.implicitHeight
                Layout.preferredWidth: label.implicitWidth + UiMetrics.spacing * 3
                radius: height / 2
                color: index === DashboardManager.activeIndex
                       ? ThemeService.primaryContainer : ThemeService.surfaceContainer
                border.width: 1
                border.color: ThemeService.outlineVariant
                NormalText {
                    id: label
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: UiMetrics.fontBody
                    color: index === DashboardManager.activeIndex
                           ? ThemeService.onPrimaryContainer : ThemeService.onSurface
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: ActionRegistry.dispatch("app.dashboard.select",
                                                       DashboardManager.idAt(index))
                    onPressAndHold: manageSheet.openFor(DashboardManager.idAt(index), modelData)
                }
            }
        }

        Rectangle {  // add-dashboard chip
            Layout.preferredHeight: root.implicitHeight
            Layout.preferredWidth: root.implicitHeight
            radius: height / 2
            visible: DashboardManager.count < 8
            color: ThemeService.surfaceContainer
            border.width: 1; border.color: ThemeService.outlineVariant
            NormalText { anchors.centerIn: parent; text: "+"; color: ThemeService.onSurface }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var id = DashboardManager.addDashboard(
                                 "Dash " + (DashboardManager.count + 1))
                    if (id !== "") ActionRegistry.dispatch("app.dashboard.select", id)
                }
            }
        }
    }

    // Rename / remove sheet
    Rectangle {
        id: manageSheet
        property string dashId: ""
        function openFor(id, name) { dashId = id; nameField.text = name; visible = true }
        visible: false
        anchors.centerIn: parent
        width: 360; height: col.implicitHeight + UiMetrics.marginPage * 2
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainerHigh
        border.width: 1; border.color: ThemeService.outline
        z: 50
        ColumnLayout {
            id: col
            anchors.centerIn: parent
            width: parent.width - UiMetrics.marginPage * 2
            spacing: UiMetrics.spacing
            TextField {
                id: nameField
                Layout.fillWidth: true
                onAccepted: {
                    DashboardManager.renameDashboard(manageSheet.dashId, text)
                    manageSheet.visible = false
                }
            }
            RowLayout {
                spacing: UiMetrics.spacing
                NormalText {
                    text: "Remove"
                    color: manageSheet.dashId === "home"
                           ? ThemeService.onSurfaceVariant : ThemeService.error
                    MouseArea {
                        anchors.fill: parent
                        enabled: manageSheet.dashId !== "home"
                        onClicked: {
                            DashboardManager.removeDashboard(manageSheet.dashId)
                            manageSheet.visible = false
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                NormalText {
                    text: "Done"
                    color: ThemeService.primary
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            DashboardManager.renameDashboard(manageSheet.dashId, nameField.text)
                            manageSheet.visible = false
                        }
                    }
                }
            }
        }
    }
}
```
(If `TextField` styling clashes with the app's controls, match whatever `WidgetConfigSheet.qml` uses for text input — follow the existing pattern, don't invent.)

- [ ] **Step 2: Integrate in HomeMenu.** (a) Instantiate near the picker sheet (`HomeMenu.qml:1262`):
```qml
    DashboardSwitcher {
        anchors { top: parent.top; topMargin: UiMetrics.spacing; left: parent.left; right: parent.right }
        editing: homeScreen.selectedInstanceId !== ""
        z: 150
    }
```
Verify the actual selection property name first: `grep -n "selectedInstanceId" qml/applications/home/HomeMenu.qml` — bind to whatever property tracks the selected widget (`HomeMenu.qml:231-242` uses `homeScreen.selectedInstanceId`). (b) Swap the dimension call: `HomeMenu.qml:79` `WidgetGridModel.setGridDimensions(gridCols, gridRows)` → `DashboardManager.setGridDimensions(gridCols, gridRows)`.

- [ ] **Step 3: Build, run on desktop if available** (pills appear when a widget is selected; add/switch/rename/remove work; `ActionRegistry.dispatch("app.dashboard.next")` from anywhere switches). **Step 4: Full suite. Step 5: Commit** — `git commit -am "feat(ui): dashboard switcher pills — add/rename/remove/switch via actions"`

---

### Task 6: Picker size presets

**Files:** Modify `src/ui/WidgetPickerModel.hpp/.cpp` (4 new roles), `qml/components/WidgetPickerSheet.qml`.

- [ ] **Step 1: Model roles.** Append to the `Roles` enum after `CategoryLabelRole`: `MinColsRole, MinRowsRole, MaxColsRole, MaxRowsRole` (numerically 265–268). In `data()`/`roleNames()`, return `desc.minCols/minRows/maxCols/maxRows` and names `"minCols"` etc. — mirror the existing role plumbing exactly. Add to the model's test (find with `grep -l WidgetPickerModel tests/*.cpp`): register a descriptor with `minCols=2, maxCols=3`, assert `data(idx, 265) == 2` and `data(idx, 267) == 3`.
- [ ] **Step 2: Sheet presets.** In `WidgetPickerSheet.qml`: (a) extend the card model items (`:195-202`) with `minCols/minRows/maxCols/maxRows` via roles 265–268; (b) replace the card `onClicked` (`:249`) with:
```qml
onClicked: {
    var presets = root.presetsFor(modelData)
    if (presets.length <= 1)
        root.widgetChosen(modelData.widgetId, modelData.defaultCols, modelData.defaultRows)
    else
        sizePopup.openFor(modelData, presets)
}
```
(c) add to the root item:
```qml
    // Paid-alternative-parity size options: fixed preset set clamped to the descriptor's bounds
    function presetsFor(w) {
        var candidates = [[1,1],[2,1],[2,2],[3,2]]
        var out = []
        for (var i = 0; i < candidates.length; ++i) {
            var c = candidates[i][0], r = candidates[i][1]
            if (c >= w.minCols && c <= w.maxCols && r >= w.minRows && r <= w.maxRows)
                out.push({cols: c, rows: r})
        }
        if (out.length === 0) out.push({cols: w.defaultCols, rows: w.defaultRows})
        return out
    }

    Rectangle {
        id: sizePopup
        property var widget: null
        property var presets: []
        function openFor(w, p) { widget = w; presets = p; visible = true }
        visible: false
        anchors.centerIn: parent
        width: presetRow.implicitWidth + UiMetrics.marginPage * 2
        height: presetRow.implicitHeight + UiMetrics.marginPage * 2
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainerHigh
        border.width: 1; border.color: ThemeService.outline
        z: 10
        Row {
            id: presetRow
            anchors.centerIn: parent
            spacing: UiMetrics.spacing
            Repeater {
                model: sizePopup.presets
                delegate: Rectangle {
                    width: UiMetrics.tileW * 0.3; height: UiMetrics.tileH * 0.3
                    radius: UiMetrics.radiusSmall
                    color: ThemeService.surfaceContainer
                    border.width: 1; border.color: ThemeService.outlineVariant
                    NormalText {
                        anchors.centerIn: parent
                        text: modelData.cols + "×" + modelData.rows
                        color: ThemeService.onSurface
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.widgetChosen(sizePopup.widget.widgetId,
                                              modelData.cols, modelData.rows)
                            sizePopup.visible = false
                        }
                    }
                }
            }
        }
    }
```
The HomeMenu handler (`HomeMenu.qml:1268-1281`) already receives `(widgetId, cols, rows)` and clamps — unchanged.
- [ ] **Step 3: Build + model test + full suite. Step 4: Commit** — `git commit -am "feat(ui): widget size presets in the picker (paid-alternative-parity size options)"`

---

### Task 7: Verification sweep + handoff

- [ ] Full gate: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure` then `./cross-build.sh`.
- [ ] Deploy to the Pi (CLAUDE.md workflow) and run the design §6 definition-of-done on-device pass: second dashboard, widgets, action-driven switch, reboot-restore, plus a v3→v4 migration check against the Pi's real config (back it up first: `ssh matt@192.168.1.152 'cp ~/.openauto/config.yaml /tmp/config-v3-backup.yaml'` — adjust to the actual config path from install.sh if different).
- [ ] Append results + any deviations to `docs/session-handoffs.md`; commit.
