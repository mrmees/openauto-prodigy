#include "PluginModel.hpp"
#include "core/plugin/PluginManager.hpp"
#include "core/plugin/IPlugin.hpp"

namespace oap {

PluginModel::PluginModel(PluginManager* manager, QObject* parent)
    : QAbstractListModel(parent)
    , manager_(manager)
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
    case PluginQmlRole:         return plugin->qmlComponent();
    case IsActiveRole:          return plugin->id() == activePluginId_;
    case WantsFullscreenRole:   return plugin->wantsFullscreen();
    default:                    return {};
    }
}

QHash<int, QByteArray> PluginModel::roleNames() const
{
    return {
        {PluginIdRole,        "pluginId"},
        {PluginNameRole,      "pluginName"},
        {PluginIconRole,      "pluginIcon"},
        {PluginQmlRole,       "pluginQml"},
        {IsActiveRole,        "isActive"},
        {WantsFullscreenRole, "wantsFullscreen"}
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

    activePluginId_ = pluginId;
    emit activePluginChanged();
    emit dataChanged(index(0), index(rowCount() - 1), {IsActiveRole});
}

IPlugin* PluginModel::activePlugin() const
{
    if (activePluginId_.isEmpty()) return nullptr;
    return manager_->plugin(activePluginId_);
}

} // namespace oap
