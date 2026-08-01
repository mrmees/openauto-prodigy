#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QString>

namespace oap {

class INavigationProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool navActive READ navActive NOTIFY navActiveChanged)
    Q_PROPERTY(QString roadName READ roadName NOTIFY turnDataChanged)
    Q_PROPERTY(QString destination READ destination NOTIFY turnDataChanged)
    Q_PROPERTY(int maneuverType READ maneuverType NOTIFY turnDataChanged)
    Q_PROPERTY(int turnDirection READ turnDirection NOTIFY turnDataChanged)
    Q_PROPERTY(QString formattedDistance READ formattedDistance NOTIFY distanceChanged)
    Q_PROPERTY(int distanceMeters READ distanceMeters NOTIFY distanceChanged)
    Q_PROPERTY(bool hasManeuverIcon READ hasManeuverIcon NOTIFY turnDataChanged)
    Q_PROPERTY(int iconVersion READ iconVersion NOTIFY turnDataChanged)
    Q_PROPERTY(QAbstractItemModel* laneModel READ laneModel CONSTANT)
    Q_PROPERTY(bool hasLaneGuidance READ hasLaneGuidance
               NOTIFY laneGuidanceChanged)
    Q_PROPERTY(bool hasDistance READ hasDistance NOTIFY distanceChanged)
public:
    using QObject::QObject;

    virtual bool navActive() const = 0;
    virtual QString roadName() const = 0;
    virtual QString destination() const { return {}; }
    virtual int maneuverType() const = 0;
    virtual int turnDirection() const = 0;
    virtual QString formattedDistance() const = 0;
    virtual int distanceMeters() const = 0;
    virtual bool hasManeuverIcon() const = 0;
    virtual int iconVersion() const = 0;
    virtual QAbstractItemModel* laneModel() const { return nullptr; }
    virtual bool hasLaneGuidance() const { return false; }
    virtual bool hasDistance() const { return false; }

signals:
    void navActiveChanged();
    void turnDataChanged();
    void distanceChanged();
    void laneGuidanceChanged();
};

} // namespace oap
