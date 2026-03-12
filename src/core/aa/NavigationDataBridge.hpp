#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

namespace oaa { namespace hu { class NavigationChannelHandler; } }

namespace oap {
namespace aa {

class ManeuverIconProvider;

class NavigationDataBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool navActive READ navActive NOTIFY navActiveChanged)
    Q_PROPERTY(QString roadName READ roadName NOTIFY turnDataChanged)
    Q_PROPERTY(int maneuverType READ maneuverType NOTIFY turnDataChanged)
    Q_PROPERTY(int turnDirection READ turnDirection NOTIFY turnDataChanged)
    Q_PROPERTY(QString formattedDistance READ formattedDistance NOTIFY turnDataChanged)
    Q_PROPERTY(int distanceMeters READ distanceMeters NOTIFY turnDataChanged)
    Q_PROPERTY(bool hasManeuverIcon READ hasManeuverIcon NOTIFY turnDataChanged)
    Q_PROPERTY(int iconVersion READ iconVersion NOTIFY turnDataChanged)

public:
    explicit NavigationDataBridge(QObject* parent = nullptr);

    void connectToHandler(oaa::hu::NavigationChannelHandler* handler);
    void setManeuverIconProvider(ManeuverIconProvider* provider);

    bool navActive() const { return navActive_; }
    QString roadName() const { return roadName_; }
    int maneuverType() const { return maneuverType_; }
    int turnDirection() const { return turnDirection_; }
    QString formattedDistance() const;
    int distanceMeters() const { return distanceMeters_; }
    bool hasManeuverIcon() const { return !currentIcon_.isEmpty(); }
    int iconVersion() const { return iconVersion_; }

signals:
    void navActiveChanged();
    void turnDataChanged();

private slots:
    void onNavigationStateChanged(bool active);
    void onNavigationTurnEvent(const QString& roadName, int maneuverType,
                               int turnDirection, const QByteArray& turnIcon,
                               int distanceMeters, int distanceUnit);

private:
    bool navActive_ = false;
    QString roadName_;
    int maneuverType_ = 0;
    int turnDirection_ = 0;
    int distanceMeters_ = 0;
    int distanceUnit_ = 0;
    QByteArray currentIcon_;
    int iconVersion_ = 0;
    ManeuverIconProvider* iconProvider_ = nullptr;
};

} // namespace aa
} // namespace oap
