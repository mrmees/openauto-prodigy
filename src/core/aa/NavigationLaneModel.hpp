#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace oap {
namespace aa {

struct LaneDirectionPresentation {
    QString shape;
    bool recommended = false;

    bool operator==(const LaneDirectionPresentation& other) const
    {
        return shape == other.shape && recommended == other.recommended;
    }
};

using LanePresentation = QList<LaneDirectionPresentation>;
using LanePresentationList = QList<LanePresentation>;

class NavigationLaneModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        DirectionsRole = Qt::UserRole + 1,
    };

    explicit NavigationLaneModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool replaceLanes(const LanePresentationList& lanes);
    bool clear();

private:
    LanePresentationList lanes_;
};

} // namespace aa
} // namespace oap
