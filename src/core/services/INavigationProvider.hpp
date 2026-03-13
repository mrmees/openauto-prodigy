#pragma once

#include <QObject>
#include <QString>

namespace oap {

class INavigationProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool navActive READ navActive NOTIFY navActiveChanged)
    Q_PROPERTY(QString roadName READ roadName NOTIFY turnDataChanged)
    Q_PROPERTY(int maneuverType READ maneuverType NOTIFY turnDataChanged)
    Q_PROPERTY(int turnDirection READ turnDirection NOTIFY turnDataChanged)
    Q_PROPERTY(QString formattedDistance READ formattedDistance NOTIFY distanceChanged)
    Q_PROPERTY(bool hasManeuverIcon READ hasManeuverIcon NOTIFY turnDataChanged)
    Q_PROPERTY(int iconVersion READ iconVersion NOTIFY turnDataChanged)
public:
    using QObject::QObject;

    virtual bool navActive() const = 0;
    virtual QString roadName() const = 0;
    virtual int maneuverType() const = 0;
    virtual int turnDirection() const = 0;
    virtual QString formattedDistance() const = 0;
    virtual bool hasManeuverIcon() const = 0;
    virtual int iconVersion() const = 0;

signals:
    void navActiveChanged();
    void turnDataChanged();
    void distanceChanged();
};

} // namespace oap
