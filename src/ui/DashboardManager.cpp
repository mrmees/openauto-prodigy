#include "ui/DashboardManager.hpp"

#include "ui/WidgetGridModel.hpp"
#include "ui/WidgetContextFactory.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include "core/widget/WidgetTypes.hpp"
#include "core/YamlConfig.hpp"

#include <QRegularExpression>

namespace oap {

DashboardManager::DashboardManager(WidgetRegistry* registry, IHostContext* hostContext,
                                   std::shared_ptr<YamlConfig> config, const QString& configPath,
                                   QObject* parent)
    : QObject(parent)
    , registry_(registry)
    , hostContext_(hostContext)
    , config_(std::move(config))
    , configPath_(configPath)
{
    persistTimer_.setSingleShot(true);
    persistTimer_.setInterval(750);
    connect(&persistTimer_, &QTimer::timeout, this, [this]() {
        config_->save(configPath_);
    });
}

DashboardManager::~DashboardManager()
{
    // Flush a pending debounced active-id write so quitting right after a
    // nav tap doesn't silently lose it. The in-memory config already has
    // the new active id set (schedulePersistActiveId sets it eagerly) —
    // this just performs the deferred file write synchronously.
    if (persistTimer_.isActive()) {
        persistTimer_.stop();
        config_->save(configPath_);
    }
}

void DashboardManager::flushPendingPersist()
{
    // Same flush as the dtor above, but invoked from aboutToQuit so the
    // write happens during normal event-loop teardown instead of relying
    // solely on destruction order (belt-and-suspenders with the shared_ptr
    // config lifetime fix — see header doc comment on the ctor).
    if (persistTimer_.isActive()) {
        persistTimer_.stop();
        config_->save(configPath_);
    }
}

void DashboardManager::loadFromConfig(int initialCols, int initialRows)
{
    loading_ = true;

    QList<DashboardConfig> dashList = config_->dashboards();
    if (dashList.isEmpty()) {
        dashList.append(DashboardConfig{ QStringLiteral("home"), QStringLiteral("Home"), 0, 2, {} });
    }

    entries_.clear();

    for (const auto& d : dashList) {
        Entry e;
        e.id = d.id;
        e.name = d.name;

        auto* model = new WidgetGridModel(registry_, this);
        model->setPageCount(d.pageCount);
        if (!d.placements.isEmpty()) {
            model->setPlacements(d.placements, registry_);
        }
        model->setNextInstanceId(d.nextInstanceId);
        model->setSavedDimensions(config_->gridSavedCols(), config_->gridSavedRows());
        model->setGridDimensions(initialCols, initialRows);

        auto* factory = new WidgetContextFactory(model, hostContext_, this);

        e.model = model;
        e.factory = factory;
        entries_.append(e);

        // Fresh-install seeding: place singleton launcher widgets on the
        // reserved (last) page of the "home" dashboard — verbatim from the
        // legacy main.cpp seeding block.
        if (e.id == QLatin1String("home") && model->placements().isEmpty()) {
            int reservedPage = model->pageCount() - 1;
            QList<GridPlacement> seedPlacements;
            {
                GridPlacement p;
                p.instanceId = QStringLiteral("aa-launcher-reserved");
                p.widgetId = QStringLiteral("org.openauto.aa-launcher");
                p.col = 0; p.row = 0;
                p.colSpan = 1; p.rowSpan = 1;
                p.opacity = 0.25;
                p.page = reservedPage;
                p.visible = true;
                seedPlacements.append(p);
            }
            {
                GridPlacement p;
                p.instanceId = QStringLiteral("settings-launcher-reserved");
                p.widgetId = QStringLiteral("org.openauto.settings-launcher");
                p.col = 0; p.row = 1;
                p.colSpan = 1; p.rowSpan = 1;
                p.opacity = 0.25;
                p.page = reservedPage;
                p.visible = true;
                seedPlacements.append(p);
            }
            model->setPlacements(seedPlacements, registry_);
            model->adoptLiveAsBaseline();
        }
    }

    active_ = 0;
    const int restoredIndex = indexOf(config_->activeDashboardId());
    if (restoredIndex >= 0) {
        active_ = restoredIndex;
    }

    // Load-before-connect (spec §6.4): only now, after every dashboard's
    // placements are fully loaded/seeded, wire up baseline persistence.
    // Automatic remap notifications intentionally do not write live topology.
    loading_ = false;

    for (const auto& e : entries_)
        connectModelPersistence(e.model);
}

WidgetGridModel* DashboardManager::activeModel() const
{
    if (active_ < 0 || active_ >= entries_.size()) return nullptr;
    return entries_[active_].model;
}

WidgetContextFactory* DashboardManager::activeFactory() const
{
    if (active_ < 0 || active_ >= entries_.size()) return nullptr;
    return entries_[active_].factory;
}

WidgetGridModel* DashboardManager::modelForId(const QString& id) const
{
    const int idx = indexOf(id);
    return idx >= 0 ? entries_[idx].model : nullptr;
}

QString DashboardManager::activeDashboardId() const
{
    if (active_ < 0 || active_ >= entries_.size()) return {};
    return entries_[active_].id;
}

int DashboardManager::activeIndex() const
{
    return active_;
}

int DashboardManager::count() const
{
    return entries_.size();
}

QStringList DashboardManager::dashboardNames() const
{
    QStringList names;
    names.reserve(entries_.size());
    for (const auto& e : entries_) {
        names.append(e.name);
    }
    return names;
}

bool DashboardManager::switchTo(const QString& id)
{
    const int idx = indexOf(id);
    if (idx < 0) return false;
    if (idx == active_) return true;  // already active: no-op
    // Clear the OUTGOING model's selection latch before repointing the
    // active dashboard (activeDashboardChanged causes QML to re-point its
    // WidgetGridModel context property to the new model). Doing this after
    // the emit would deselect the wrong (incoming) model and strand the
    // outgoing model's deferred grid remap. See DashboardManager class docs.
    if (auto* outgoing = activeModel()) outgoing->setWidgetSelected(false);
    active_ = idx;
    emit activeDashboardChanged();
    schedulePersistActiveId();
    return true;
}

bool DashboardManager::switchToIndex(int index)
{
    if (index < 0 || index >= entries_.size()) return false;
    if (index == active_) return true;  // already active: no-op
    if (auto* outgoing = activeModel()) outgoing->setWidgetSelected(false);
    active_ = index;
    emit activeDashboardChanged();
    schedulePersistActiveId();
    return true;
}

void DashboardManager::nextDashboard()
{
    if (entries_.size() <= 1) return;
    if (auto* outgoing = activeModel()) outgoing->setWidgetSelected(false);
    active_ = (active_ + 1) % entries_.size();
    emit activeDashboardChanged();
    schedulePersistActiveId();
}

void DashboardManager::previousDashboard()
{
    if (entries_.size() <= 1) return;
    if (auto* outgoing = activeModel()) outgoing->setWidgetSelected(false);
    active_ = (active_ - 1 + entries_.size()) % entries_.size();
    emit activeDashboardChanged();
    schedulePersistActiveId();
}

QString DashboardManager::addDashboard(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return {};
    if (entries_.size() >= 8) return {};

    QString baseId = slugify(trimmed);
    if (baseId.isEmpty()) baseId = QStringLiteral("dashboard");

    QString id = baseId;
    int suffix = 2;
    while (indexOf(id) >= 0) {
        id = baseId + QLatin1Char('-') + QString::number(suffix);
        ++suffix;
    }

    Entry e;
    e.id = id;
    e.name = trimmed;

    auto* model = new WidgetGridModel(registry_, this);
    if (auto* am = activeModel()) {
        model->setGridDimensions(am->gridColumns(), am->gridRows());
        model->setSavedDimensions(am->baselineGridColumns(), am->baselineGridRows());
    }
    model->setPageCount(1);

    auto* factory = new WidgetContextFactory(model, hostContext_, this);

    e.model = model;
    e.factory = factory;
    entries_.append(e);

    connectModelPersistence(model);

    saveAll();
    emit dashboardsChanged();
    return id;
}

bool DashboardManager::removeDashboard(const QString& id)
{
    if (id == QLatin1String("home")) return false;
    if (entries_.size() <= 1) return false;

    const int idx = indexOf(id);
    if (idx < 0) return false;

    const bool wasActive = (idx == active_);
    Entry removed = entries_.takeAt(idx);

    if (wasActive) {
        // Same outgoing-selection-clear as switchTo/etc — the removed
        // dashboard was the active one, so its model is the "outgoing"
        // model for the activeDashboardChanged emitted below.
        removed.model->setWidgetSelected(false);
        const int homeIdx = indexOf(QStringLiteral("home"));
        active_ = homeIdx >= 0 ? homeIdx : 0;
    } else if (idx < active_) {
        --active_;
    }

    removed.model->deleteLater();
    removed.factory->deleteLater();

    saveAll();
    emit dashboardsChanged();
    if (wasActive) emit activeDashboardChanged();
    return true;
}

bool DashboardManager::renameDashboard(const QString& id, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) return false;
    const int idx = indexOf(id);
    if (idx < 0) return false;

    entries_[idx].name = trimmed;
    saveAll();
    emit dashboardsChanged();
    return true;
}

QString DashboardManager::idAt(int index) const
{
    if (index < 0 || index >= entries_.size()) return {};
    return entries_[index].id;
}

void DashboardManager::setGridDimensions(int cols, int rows)
{
    for (auto& e : entries_) {
        e.model->setGridDimensions(cols, rows);
    }
}

void DashboardManager::saveAll()
{
    if (loading_) return;

    // A full save always persists the current active id too (see below),
    // so any pending debounced nav-only persist is now redundant.
    if (persistTimer_.isActive()) {
        persistTimer_.stop();
    }

    QList<DashboardConfig> list;
    list.reserve(entries_.size());
    for (const auto& e : entries_) {
        DashboardConfig d;
        d.id = e.id;
        d.name = e.name;
        d.nextInstanceId = e.model->nextInstanceId();
        d.pageCount = e.model->baselinePageCount();
        d.placements = e.model->baselinePlacements();
        list.append(d);
    }
    config_->setDashboards(list);
    config_->setActiveDashboardId(activeDashboardId());
    if (auto* am = activeModel()) {
        config_->setGridSavedDims(am->baselineGridColumns(), am->baselineGridRows());
    }
    config_->save(configPath_);
}

void DashboardManager::connectModelPersistence(WidgetGridModel* model)
{
    connect(model, &WidgetGridModel::baselineChanged,
            this, &DashboardManager::commitSharedBaseline);
}

void DashboardManager::commitSharedBaseline()
{
    if (loading_ || committingBaseline_)
        return;

    auto* changedModel = qobject_cast<WidgetGridModel*>(sender());
    if (!changedModel)
        return;

    committingBaseline_ = true;
    const int cols = changedModel->gridColumns();
    const int rows = changedModel->gridRows();

    // Grid dimensions are global in YAML. Before one model's user mutation can
    // commit them, make every dashboard adopt its live topology at those exact
    // dimensions so persistence never mixes coordinate systems.
    for (const auto& e : entries_) {
        if (e.model->gridColumns() != cols || e.model->gridRows() != rows)
            e.model->setGridDimensions(cols, rows);
    }
    for (const auto& e : entries_)
        e.model->adoptLiveAsBaseline();

    saveAll();
    committingBaseline_ = false;
}

void DashboardManager::schedulePersistActiveId()
{
    // Set the active id in-memory immediately (so a concurrent saveAll
    // triggered by another dashboard's edits, or the dtor flush, always
    // sees the latest value), but defer the actual file write — restart
    // the single-shot timer so a burst of nav taps collapses into one
    // write instead of one synchronous full-config write per tap.
    config_->setActiveDashboardId(activeDashboardId());
    persistTimer_.start();
}

QString DashboardManager::slugify(const QString& name) const
{
    QString result;
    result.reserve(name.size());
    const QString lower = name.toLower();
    for (const QChar& c : lower) {
        if (c.isLetterOrNumber()) {
            result.append(c);
        } else {
            result.append(QLatin1Char('-'));
        }
    }

    static const QRegularExpression collapse(QStringLiteral("-{2,}"));
    result.replace(collapse, QStringLiteral("-"));

    while (result.startsWith(QLatin1Char('-'))) result.remove(0, 1);
    while (result.endsWith(QLatin1Char('-'))) result.chop(1);

    return result;
}

int DashboardManager::indexOf(const QString& id) const
{
    for (int i = 0; i < entries_.size(); ++i) {
        if (entries_[i].id == id) return i;
    }
    return -1;
}

} // namespace oap
