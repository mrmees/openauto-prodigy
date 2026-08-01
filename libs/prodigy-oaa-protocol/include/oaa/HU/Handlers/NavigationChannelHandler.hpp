#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace oaa {
namespace hu {

struct NavigationLaneDirectionData {
    int shape = 0;
    bool recommended = false;
};

struct NavigationLaneData {
    QList<NavigationLaneDirectionData> directions;
};

using NavigationLaneGuidance = QList<NavigationLaneData>;

enum class NavigationState {
    Unavailable = 0,
    Active = 1,
    Inactive = 2,
    Rerouting = 3,
};

struct NavigationDistanceData {
    bool hasValue = false;
    int value = 0;
    bool hasDisplayText = false;
    QString displayText;
    bool hasUnit = false;
    int unit = 0;
};

struct NavigationNotificationSnapshot {
    int stepCount = 0;
    bool hasManeuver = false;
    int maneuverType = 0;
    bool hasUpcomingRoad = false;
    QString upcomingRoad;
    QStringList actionCues;
    NavigationLaneGuidance lanes;
    QStringList destinations;
};

struct NavigationDestinationDistanceData {
    bool hasDistance = false;
    NavigationDistanceData distance;
    bool hasEstimatedTimeOfArrival = false;
    QString estimatedTimeOfArrival;
    bool hasTimeToArrival = false;
    qint64 timeToArrivalSeconds = 0;
};

struct NavigationPositionSnapshot {
    bool hasStepDistance = false;
    NavigationDistanceData stepDistance;
    bool hasTimeToStep = false;
    qint64 timeToStepSeconds = 0;
    QList<NavigationDestinationDistanceData> destinationDistances;
    bool hasCurrentRoad = false;
    QString currentRoad;
};

class NavigationChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit NavigationChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::Navigation; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

signals:
    void navigationStateSnapshotChanged(oaa::hu::NavigationState state);

    void navigationNotificationChanged(
        const oaa::hu::NavigationNotificationSnapshot& snapshot);

    void navigationPositionChanged(
        const oaa::hu::NavigationPositionSnapshot& snapshot);

    /// Emitted when navigation starts (active=true) or ends (active=false)
    void navigationStateChanged(bool active);

    /// Emitted with turn-by-turn instruction update
    void navigationStepChanged(const QString& instruction, const QString& destination,
                                int maneuverType);

    /// Emitted with distance/ETA update
    void navigationDistanceChanged(const QString& distance, int unit);

    /// Emitted with NavigationTurnEvent (~1Hz during active navigation)
    void navigationTurnEvent(const QString& roadName, int maneuverType,
                             int turnDirection, const QByteArray& turnIcon,
                             int distanceMeters, int distanceUnit);

    /// Emitted with enhanced NavigationNotification (multi-step, lanes, destination)
    void navigationNotificationReceived(int stepCount, int laneCount,
                                         const QString& destination, const QString& eta);

    void navigationLaneGuidanceChanged(
        const oaa::hu::NavigationLaneGuidance& lanes);

    void vehicleEnergyForecastReceived(bool innerParsed,
                                       const QString& boundedSummary);

private:
    void handleNavState(const QByteArray& payload);
    void handleNavStep(const QByteArray& payload);
    void handleNavDistance(const QByteArray& payload);
    void handleTurnEvent(const QByteArray& payload);
    void handleVehicleEnergyForecast(const QByteArray& payload);
    NavigationState navigationState_ = NavigationState::Unavailable;
};

} // namespace hu
} // namespace oaa

Q_DECLARE_METATYPE(oaa::hu::NavigationLaneGuidance)
Q_DECLARE_METATYPE(oaa::hu::NavigationState)
Q_DECLARE_METATYPE(oaa::hu::NavigationNotificationSnapshot)
Q_DECLARE_METATYPE(oaa::hu::NavigationPositionSnapshot)
