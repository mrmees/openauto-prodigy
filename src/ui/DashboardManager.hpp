#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QTimer>

namespace oap {

class WidgetRegistry;
class WidgetGridModel;
class WidgetContextFactory;
class IHostContext;
class YamlConfig;

// Manages a set of named dashboards, each backed by its own WidgetGridModel
// and WidgetContextFactory. Owns load/save orchestration so that all
// dashboard placements are restored before any save signal is connected
// (see spec §6.4) — otherwise a boot-time signal could persist an empty
// grid over user data mid-load.
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
    ~DashboardManager() override;

    // Builds a WidgetGridModel + WidgetContextFactory per configured dashboard,
    // seeds "home" with the reserved launcher placements on fresh installs,
    // restores the active dashboard, then (last) wires up auto-save.
    void loadFromConfig(int initialCols, int initialRows);

    WidgetGridModel* activeModel() const;
    WidgetContextFactory* activeFactory() const;
    WidgetGridModel* modelForId(const QString& id) const;   // nullptr if unknown

    QString activeDashboardId() const;
    int activeIndex() const;
    int count() const;
    QStringList dashboardNames() const;

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

private:
    struct Entry {
        QString id;
        QString name;
        WidgetGridModel* model = nullptr;
        WidgetContextFactory* factory = nullptr;
    };

    void saveAll();
    // Pure navigation (no placement/page-count change): persists only the
    // active dashboard id, debounced via persistTimer_, instead of
    // reserializing every dashboard through saveAll(). See class docs.
    void schedulePersistActiveId();
    QString slugify(const QString& name) const;
    int indexOf(const QString& id) const;

    WidgetRegistry* registry_;
    IHostContext* hostContext_;
    YamlConfig* config_;
    QString configPath_;

    QList<Entry> entries_;
    int active_ = 0;
    bool loading_ = false;

    // Debounced persistence for nav-only active-id changes (switchTo/
    // switchToIndex/next/previousDashboard). Nav taps update active_ and
    // config_'s in-memory active id immediately (so it's never lost to a
    // concurrent saveAll from another dashboard's edits), but the actual
    // file write is coalesced: rapid taps restart this single-shot timer
    // rather than issuing a synchronous full-config write per tap. See
    // CLAUDE.md gotcha: QTimer needs a real #include, not a fwd-decl.
    QTimer persistTimer_;
};

} // namespace oap
