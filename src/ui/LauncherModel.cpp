#include "LauncherModel.hpp"
#include "core/YamlConfig.hpp"

namespace oap {

LauncherModel::LauncherModel(YamlConfig* config, QObject* parent)
    : QAbstractListModel(parent)
    , config_(config)
{
    refresh();
}

int LauncherModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return tiles_.size();
}

QVariant LauncherModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= tiles_.size())
        return {};

    const auto& tile = tiles_[index.row()];
    switch (role) {
    case TileIdRole:     return tile.value("id");
    case TileLabelRole:  return tile.value("label");
    case TileIconRole:   return tile.value("icon");
    case TileActionRole: return tile.value("action");
    default:             return {};
    }
}

QHash<int, QByteArray> LauncherModel::roleNames() const
{
    return {
        {TileIdRole,     "tileId"},
        {TileLabelRole,  "tileLabel"},
        {TileIconRole,   "tileIcon"},
        {TileActionRole, "tileAction"}
    };
}

void LauncherModel::refresh()
{
    beginResetModel();
    tiles_ = config_->launcherTiles();
    endResetModel();
}

} // namespace oap
