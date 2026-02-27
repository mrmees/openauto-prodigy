#include "PluginModel.hpp"
#include "PluginRuntimeContext.hpp"
#include "PluginViewHost.hpp"
#include "core/plugin/PluginManager.hpp"
#include "core/plugin/IPlugin.hpp"
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
        viewHost_->clearView();  // destroy QML item before context
        if (activeContext_) {
            activeContext_->deactivate();
            delete activeContext_;
            activeContext_ = nullptr;
        }
        activePluginId_.clear();
        manager_->deactivateCurrentPlugin();
        emit activePluginChanged();
        emit dataChanged(index(0), index(rowCount() - 1), {IsActiveRole});
        return;
    }

    // Validate: only update state if manager accepts the activation
    if (!manager_->activatePlugin(pluginId)) return;

    // Deactivate current: clear view first, then context
    viewHost_->clearView();
    if (activeContext_) {
        activeContext_->deactivate();
        delete activeContext_;
        activeContext_ = nullptr;
    }

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
                activeContext_->deactivate();
                delete activeContext_;
                activeContext_ = nullptr;
                activePluginId_.clear();
                manager_->deactivateCurrentPlugin();
            }
        }
    }

    emit activePluginChanged();
    emit dataChanged(index(0), index(rowCount() - 1), {IsActiveRole});
}

IPlugin* PluginModel::activePlugin() const
{
    if (activePluginId_.isEmpty()) return nullptr;
    return manager_->plugin(activePluginId_);
}

} // namespace oap
