#pragma once

#include <QObject>
#include <QString>

namespace oap {

class INavigationProvider : public QObject {
    Q_OBJECT
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
