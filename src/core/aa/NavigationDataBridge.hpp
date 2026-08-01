#pragma once

#include <memory>

#include <QByteArray>
#include <QString>

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
    Q_PROPERTY(QString destination READ destination NOTIFY turnDataChanged)
    Q_PROPERTY(int maneuverType READ maneuverType NOTIFY turnDataChanged)
    Q_PROPERTY(int turnDirection READ turnDirection NOTIFY turnDataChanged)
    Q_PROPERTY(QString formattedDistance READ formattedDistance NOTIFY distanceChanged)
    Q_PROPERTY(bool hasManeuverIcon READ hasManeuverIcon NOTIFY turnDataChanged)
    Q_PROPERTY(int iconVersion READ iconVersion NOTIFY turnDataChanged)
    Q_PROPERTY(QAbstractItemModel* laneModel READ laneModel CONSTANT)
    Q_PROPERTY(bool hasLaneGuidance READ hasLaneGuidance NOTIFY laneGuidanceChanged)
    Q_PROPERTY(bool hasDistance READ hasDistance NOTIFY distanceChanged)
    Q_PROPERTY(int navigationState READ navigationState
               NOTIFY navigationPresentationChanged)
    Q_PROPERTY(bool guidanceFresh READ guidanceFresh
               NOTIFY navigationPresentationChanged)
    Q_PROPERTY(bool rerouting READ rerouting NOTIFY navigationPresentationChanged)
    Q_PROPERTY(bool hasActionCue READ hasActionCue NOTIFY turnDataChanged)
    Q_PROPERTY(QString actionCue READ actionCue NOTIFY turnDataChanged)
    Q_PROPERTY(bool hasTimeToStep READ hasTimeToStep NOTIFY distanceChanged)
    Q_PROPERTY(QString formattedTimeToStep READ formattedTimeToStep
               NOTIFY distanceChanged)
    Q_PROPERTY(int destinationCount READ destinationCount NOTIFY tripDataChanged)
    Q_PROPERTY(bool hasDestinationDistance READ hasDestinationDistance
               NOTIFY tripDataChanged)
    Q_PROPERTY(QString formattedDestinationDistance
               READ formattedDestinationDistance NOTIFY tripDataChanged)
    Q_PROPERTY(bool hasDestinationEta READ hasDestinationEta NOTIFY tripDataChanged)
    Q_PROPERTY(QString destinationEta READ destinationEta NOTIFY tripDataChanged)
    Q_PROPERTY(bool hasTimeToArrival READ hasTimeToArrival NOTIFY tripDataChanged)
    Q_PROPERTY(QString formattedTimeToArrival READ formattedTimeToArrival
               NOTIFY tripDataChanged)

public:
    explicit NavigationDataBridge(QObject* parent = nullptr);

    void connectToHandler(oaa::hu::NavigationChannelHandler* handler);
    void setManeuverIconProvider(ManeuverIconProvider* provider);

    bool navActive() const override;
    QString roadName() const override;
    QString destination() const override;
    int maneuverType() const override;
    int turnDirection() const override;
    QString formattedDistance() const override;
    int distanceMeters() const override;
    QString instruction() const { return roadName(); }
    bool hasManeuverIcon() const override;
    int iconVersion() const override { return iconVersion_; }
    QAbstractItemModel* laneModel() const override { return laneModel_.get(); }
    bool hasLaneGuidance() const override;
    bool hasDistance() const override;

    int navigationState() const { return static_cast<int>(navigationState_); }
    bool guidanceFresh() const;
    bool rerouting() const;
    bool hasActionCue() const;
    QString actionCue() const;
    bool hasTimeToStep() const;
    QString formattedTimeToStep() const;
    int destinationCount() const;
    bool hasDestinationDistance() const;
    QString formattedDestinationDistance() const;
    bool hasDestinationEta() const;
    QString destinationEta() const;
    bool hasTimeToArrival() const;
    QString formattedTimeToArrival() const;

signals:
    void tripDataChanged();
    void navigationPresentationChanged();

private slots:
    void onNavigationStateChanged(oaa::hu::NavigationState state);
    void onNavigationNotificationChanged(
        const oaa::hu::NavigationNotificationSnapshot& snapshot);
    void onNavigationPositionChanged(
        const oaa::hu::NavigationPositionSnapshot& snapshot);
    void onNavigationTurnEvent(const QString& roadName, int maneuverType,
                               int turnDirection, const QByteArray& turnIcon,
                               int distanceMeters, int distanceUnit);

private:
    bool guidanceVisible() const;
    bool tripVisible() const;
    void clearSnapshotData();
    void clearIcon();
    void replaceLanes(const oaa::hu::NavigationLaneGuidance& lanes);
    const oaa::hu::NavigationDestinationDistanceData* firstDestinationDistance() const;

    oaa::hu::NavigationState navigationState_ = oaa::hu::NavigationState::Unavailable;
    bool receivedState_ = false;
    bool notificationFresh_ = false;
    bool positionFresh_ = false;
    bool awaitingPostReroute_ = false;

    QString roadName_;
    QString actionCue_;
    QStringList destinations_;
    int maneuverType_ = 0;
    int turnDirection_ = 0;
    bool hasStepDistance_ = false;
    oaa::hu::NavigationDistanceData stepDistance_;
    bool hasTimeToStep_ = false;
    qint64 timeToStepSeconds_ = 0;
    QList<oaa::hu::NavigationDestinationDistanceData> destinationDistances_;
    bool legacyDistance_ = false;
    int legacyDistanceMeters_ = 0;
    int legacyDistanceUnit_ = 0;
    QByteArray currentIcon_;
    int iconVersion_ = 0;
    ManeuverIconProvider* iconProvider_ = nullptr;
    std::unique_ptr<NavigationLaneModel> laneModel_;

    static QString formatDistance(const oaa::hu::NavigationDistanceData& distance);
    static QString formatLegacyDistance(int distanceMeters, int distanceUnit);
    static QString formatDuration(qint64 seconds);
    static QString unitSuffix(int distanceUnit);
    static QString formatMiles(double miles);
    static QString laneShapeToken(int shape);
};

} // namespace aa
} // namespace oap
