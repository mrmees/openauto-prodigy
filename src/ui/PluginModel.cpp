#include "PluginModel.hpp"
#include "PluginRuntimeContext.hpp"
#include "PluginViewHost.hpp"
#include "core/plugin/PluginManager.hpp"
#include "core/plugin/IPlugin.hpp"
#include <QPointer>
#include <QQmlEngine>

namespace oap {

PluginModel::PluginModel(PluginManager* manager, QQmlEngine* engine, QObject* parent)
    : QAbstractListModel(parent)
    , manager_(manager)
    , engine_(engine)
    , viewHost_(new PluginViewHost(engine, this))
{
    // Refresh model when plugins change
    connect(manager_, &PluginManager::pluginInitialized, this, [this]() {
        beginResetModel();
        endResetModel();
    });
}

PluginModel::~PluginModel()
{
    retireActiveContext();
    viewHost_->finishRetirements();
}

int PluginModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return manager_->plugins().size();
}

QVariant PluginModel::data(const QModelIndex& index, int role) const
{
    auto plugins = manager_->plugins();
    if (index.row() < 0 || index.row() >= plugins.size())
        return {};

    auto* plugin = plugins[index.row()];

    switch (role) {
    case PluginIdRole:          return plugin->id();
    case PluginNameRole:        return plugin->name();
    case PluginIconRole:        return plugin->iconSource();
    case PluginIconTextRole:    return plugin->iconText();
    case PluginQmlRole:         return plugin->qmlComponent();
    case IsActiveRole:          return plugin->id() == activePluginId_;
    case WantsFullscreenRole:   return plugin->wantsFullscreen();
    case SettingsQmlRole:       return plugin->settingsComponent();
    default:                    return {};
    }
}

QHash<int, QByteArray> PluginModel::roleNames() const
{
    return {
        {PluginIdRole,        "pluginId"},
        {PluginNameRole,      "pluginName"},
        {PluginIconRole,      "pluginIcon"},
        {PluginIconTextRole,  "pluginIconText"},
        {PluginQmlRole,       "pluginQml"},
        {IsActiveRole,        "isActive"},
        {WantsFullscreenRole, "wantsFullscreen"},
        {SettingsQmlRole,     "settingsQml"}
    };
}

QString PluginModel::activePluginId() const
{
    return activePluginId_;
}

QUrl PluginModel::activePluginQml() const
{
    auto* p = activePlugin();
    return p ? p->qmlComponent() : QUrl();
}

bool PluginModel::activePluginFullscreen() const
{
    auto* p = activePlugin();
    return p ? p->wantsFullscreen() : false;
}

void PluginModel::setActivePlugin(const QString& pluginId)
{
    if (activePluginId_ == pluginId) return;

    // Empty ID = go home (deactivate current, show launcher)
    if (pluginId.isEmpty()) {
        retireActiveContext();
        activePluginId_.clear();
        manager_->deactivateCurrentPlugin();
        emit activePluginChanged();
        emit dataChanged(index(0), index(rowCount() - 1), {IsActiveRole});
        return;
    }

    // Validate: only update state if manager accepts the activation
    if (!manager_->activatePlugin(pluginId)) return;

    // Logical replacement is immediate. The old runtime context retires only
    // after its deferred QML subtree has actually been destroyed.
    retireActiveContext();

    activePluginId_ = pluginId;

    // Activate new plugin's runtime context
    auto* plugin = manager_->plugin(pluginId);
    if (plugin && engine_) {
        activeContext_ = new PluginRuntimeContext(plugin, engine_, this);
        activeContext_->activate();

        // Load the plugin's QML view with the correct child context
        if (!plugin->qmlComponent().isEmpty()) {
            if (!viewHost_->loadView(plugin->qmlComponent(), activeContext_->qmlContext())) {
                // Fallback: deactivate and go home
                retireActiveContext();
                activePluginId_.clear();
                manager_->deactivateCurrentPlugin();
            }
        }
    }

    emit activePluginChanged();
    emit dataChanged(index(0), index(rowCount() - 1), {IsActiveRole});
}

void PluginModel::retireActiveContext()
{
    QPointer<PluginRuntimeContext> retiringContext = activeContext_;
    activeContext_ = nullptr;

    // The plugin hook owns runtime/focus teardown and must complete before a
    // rapid reactivation can call onActivated() again. Its child QQmlContext
    // remains alive for the outgoing view until the ordered retirement edge.
    if (retiringContext)
        retiringContext->deactivatePlugin();

    viewHost_->clearViewThen([retiringContext]() mutable {
        if (!retiringContext)
            return;
        retiringContext->retireContext();
        delete retiringContext.data();
        retiringContext.clear();
    });
}

IPlugin* PluginModel::activePlugin() const
{
    if (activePluginId_.isEmpty()) return nullptr;
    return manager_->plugin(activePluginId_);
}

} // namespace oap
