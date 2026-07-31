#include "NavigationLaneModel.hpp"

#include <QVariantList>
#include <QVariantMap>

namespace oap {
namespace aa {

NavigationLaneModel::NavigationLaneModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int NavigationLaneModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : lanes_.size();
}

QVariant NavigationLaneModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= lanes_.size()
        || role != DirectionsRole) {
        return {};
    }

    QVariantList directions;
    directions.reserve(lanes_[index.row()].size());
    for (const auto& direction : lanes_[index.row()]) {
        directions.append(QVariantMap{
            {QStringLiteral("shape"), direction.shape},
            {QStringLiteral("recommended"), direction.recommended},
        });
    }
    return directions;
}

QHash<int, QByteArray> NavigationLaneModel::roleNames() const
{
    return {{DirectionsRole, QByteArrayLiteral("directions")}};
}

bool NavigationLaneModel::replaceLanes(const LanePresentationList& lanes)
{
    if (lanes_ == lanes)
        return false;

    beginResetModel();
    lanes_ = lanes;
    endResetModel();
    return true;
}

bool NavigationLaneModel::clear()
{
    return replaceLanes({});
}

} // namespace aa
} // namespace oap
