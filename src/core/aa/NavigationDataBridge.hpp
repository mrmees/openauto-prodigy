#pragma once

#include <memory>

#include <QString>
#include <QByteArray>
#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>

#include "NavigationLaneModel.hpp"
#include "core/services/INavigationProvider.hpp"

namespace oap {
namespace aa {

class ManeuverIconProvider;

class NavigationDataBridge : public INavigationProvider {
    Q_OBJECT
    Q_PROPERTY(bool navActive READ navActive NOTIFY navActiveChanged)
    Q_PROPERTY(QString roadName READ roadName NOTIFY turnDataChanged)
    Q_PROPERTY(int maneuverType READ maneuverType NOTIFY turnDataChanged)
    Q_PROPERTY(int turnDirection READ turnDirection NOTIFY turnDataChanged)
    Q_PROPERTY(QString formattedDistance READ formattedDistance NOTIFY distanceChanged)
    Q_PROPERTY(bool hasManeuverIcon READ hasManeuverIcon NOTIFY turnDataChanged)
    Q_PROPERTY(int iconVersion READ iconVersion NOTIFY turnDataChanged)
    Q_PROPERTY(QAbstractItemModel* laneModel READ laneModel CONSTANT)
    Q_PROPERTY(bool hasLaneGuidance READ hasLaneGuidance
               NOTIFY laneGuidanceChanged)
    Q_PROPERTY(bool hasDistance READ hasDistance NOTIFY distanceChanged)

public:
    explicit NavigationDataBridge(QObject* parent = nullptr);

    void connectToHandler(oaa::hu::NavigationChannelHandler* handler);
    void setManeuverIconProvider(ManeuverIconProvider* provider);

    bool navActive() const override { return navActive_; }
    QString roadName() const override { return roadName_; }
    int maneuverType() const override { return maneuverType_; }
    int turnDirection() const override { return turnDirection_; }
    QString formattedDistance() const override;
    int distanceMeters() const override { return distanceMeters_; }
    QString instruction() const { return instruction_; }
    bool hasManeuverIcon() const override { return !currentIcon_.isEmpty(); }
    int iconVersion() const override { return iconVersion_; }
    QAbstractItemModel* laneModel() const override { return laneModel_.get(); }
    bool hasLaneGuidance() const override;
    bool hasDistance() const override { return hasDistance_; }

signals:
    void navActiveChanged();
    void turnDataChanged();
    void distanceChanged();
    void laneGuidanceChanged();

private slots:
    void onNavigationStateChanged(bool active);
    void onNavigationTurnEvent(const QString& roadName, int maneuverType,
                               int turnDirection, const QByteArray& turnIcon,
                               int distanceMeters, int distanceUnit);
    void onNavigationStepChanged(const QString& instruction, const QString& destination,
                                  int maneuverType);
    void onNavigationDistanceChanged(const QString& displayText, int unit);
    void onNavigationLaneGuidanceChanged(
        const oaa::hu::NavigationLaneGuidance& lanes);

private:
    bool navActive_ = false;
    QString roadName_;
    int maneuverType_ = 0;
    int turnDirection_ = 0;
    int distanceMeters_ = 0;
    int distanceUnit_ = 0;
    QString instruction_;
    QString phoneDistanceText_;  // Pre-formatted from NavigationNextTurnDistanceEvent
    QByteArray currentIcon_;
    int iconVersion_ = 0;
    ManeuverIconProvider* iconProvider_ = nullptr;
    std::unique_ptr<NavigationLaneModel> laneModel_;
    bool hasDistance_ = false;

    static QString unitSuffix(int distanceUnit);
    static QString laneShapeToken(int shape);
};

} // namespace aa
} // namespace oap
