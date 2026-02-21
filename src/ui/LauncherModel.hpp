#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

namespace oap {

class YamlConfig;

/// Exposes launcher tiles to QML. Each tile has id, label, icon, action.
/// Backed by YamlConfig::launcherTiles().
class LauncherModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        TileIdRole = Qt::UserRole + 1,
        TileLabelRole,
        TileIconRole,
        TileActionRole
    };

    explicit LauncherModel(YamlConfig* config, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Reload tiles from config.
    Q_INVOKABLE void refresh();

private:
    YamlConfig* config_;
    QList<QVariantMap> tiles_;
};

} // namespace oap
