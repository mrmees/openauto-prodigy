#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include "oaa/navigation/NavigationTurnEventMessage.pb.h"
#include "oaa/navigation/NavigationNotificationMessage.pb.h"
#include "oaa/navigation/NavigationStateMessage.pb.h"
#include "oaa/navigation/VehicleEnergyForecastMessage.pb.h"
// NavigationFocusIndicationMessage.pb.h removed — retracted in proto v1.1

class TestNavigationChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oaa::hu::NavigationChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Navigation);
    }

    void testNavigationStateSnapshotPreservesExactStateAndCloseTransition() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy stateSpy(
            &handler,
            &oaa::hu::NavigationChannelHandler::navigationStateSnapshotChanged);
        handler.onChannelOpened();

        const auto sendState = [&handler](
                                   oaa::proto::messages::NavigationStateType state) {
            oaa::proto::messages::NavigationState message;
            message.set_state(state);
            QByteArray payload(message.ByteSizeLong(), '\0');
            QVERIFY(message.SerializeToArray(payload.data(), payload.size()));
            handler.onMessage(oaa::NavigationMessageId::NAV_STATE, payload);
        };

        sendState(oaa::proto::messages::NAV_STATE_REROUTING);
        sendState(oaa::proto::messages::NAV_STATE_ACTIVE);
        sendState(oaa::proto::messages::NAV_STATE_ACTIVE);
        sendState(oaa::proto::messages::NAV_STATE_INACTIVE);
        sendState(oaa::proto::messages::NAV_STATE_UNAVAILABLE);

        QCOMPARE(stateSpy.count(), 4);
        QCOMPARE(qvariant_cast<oaa::hu::NavigationState>(stateSpy[0][0]),
                 oaa::hu::NavigationState::Rerouting);
        QCOMPARE(qvariant_cast<oaa::hu::NavigationState>(stateSpy[1][0]),
                 oaa::hu::NavigationState::Active);
        QCOMPARE(qvariant_cast<oaa::hu::NavigationState>(stateSpy[2][0]),
                 oaa::hu::NavigationState::Inactive);
        QCOMPARE(qvariant_cast<oaa::hu::NavigationState>(stateSpy[3][0]),
                 oaa::hu::NavigationState::Unavailable);

        sendState(oaa::proto::messages::NAV_STATE_ACTIVE);
        handler.onChannelClosed();
        QCOMPARE(stateSpy.count(), 6);
        QCOMPARE(qvariant_cast<oaa::hu::NavigationState>(stateSpy[4][0]),
                 oaa::hu::NavigationState::Active);
        QCOMPARE(qvariant_cast<oaa::hu::NavigationState>(stateSpy[5][0]),
                 oaa::hu::NavigationState::Unavailable);
    }

    void testTurnEventFullPayload() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy spy(&handler, &oaa::hu::NavigationChannelHandler::navigationTurnEvent);

        oaa::proto::messages::NavigationTurnEvent msg;
        msg.set_road_name("Main St");
        msg.set_maneuver_type(static_cast<oaa::proto::enums::ManeuverType::Enum>(1));
        msg.set_turn_direction(static_cast<oaa::proto::enums::TurnSide::Enum>(1));
        msg.set_turn_icon("\x89PNG", 4);
        msg.set_distance_meters(200);
        msg.set_distance_unit(1);

        QByteArray payload(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::NavigationMessageId::NAV_TURN_EVENT, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toString(), QString("Main St"));
        QCOMPARE(spy[0][1].toInt(), 1);        // maneuver_type
        QCOMPARE(spy[0][2].toInt(), 1);        // turn_direction
        QCOMPARE(spy[0][3].toByteArray(), QByteArray("\x89PNG", 4)); // turn_icon
        QCOMPARE(spy[0][4].toInt(), 200);      // distance_meters
        QCOMPARE(spy[0][5].toInt(), 1);        // distance_unit
    }

    void testTurnEventPartialPayload() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy spy(&handler, &oaa::hu::NavigationChannelHandler::navigationTurnEvent);

        oaa::proto::messages::NavigationTurnEvent msg;
        msg.set_road_name("Highway 101");
        // All other fields left unset

        QByteArray payload(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::NavigationMessageId::NAV_TURN_EVENT, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toString(), QString("Highway 101"));
        QCOMPARE(spy[0][1].toInt(), 0);        // default maneuver_type
        QCOMPARE(spy[0][2].toInt(), 0);        // default turn_direction
        QCOMPARE(spy[0][3].toByteArray(), QByteArray()); // empty icon
        QCOMPARE(spy[0][4].toInt(), -1);       // default distance_meters
        QCOMPARE(spy[0][5].toInt(), 0);        // default distance_unit
    }

    void testTurnEventInvalidPayload() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy spy(&handler, &oaa::hu::NavigationChannelHandler::navigationTurnEvent);

        // Send garbage data
        QByteArray garbage("\xff\xfe\xfd\xfc\xfb\xfa", 6);
        handler.onMessage(oaa::NavigationMessageId::NAV_TURN_EVENT, garbage);

        // Proto2 is lenient -- invalid payloads may still parse as empty messages.
        // The key invariant is no crash. We accept either 0 or 1 emissions.
        QVERIFY(spy.count() <= 1);
    }

    void testNotificationSnapshotPreservesCurrentStepAndClearsOnEmptyReplacement() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy notificationSpy(
            &handler,
            &oaa::hu::NavigationChannelHandler::navigationNotificationChanged);

        oaa::proto::messages::NavigationNotification notification;

        auto* step1 = notification.add_steps();
        auto* maneuver1 = step1->mutable_maneuver();
        maneuver1->set_type(static_cast<oaa::proto::enums::ManeuverType::Enum>(1));
        step1->mutable_instruction()->set_text("Turn right onto Main St");
        auto* lane1 = step1->add_lanes();
        auto* dir1 = lane1->add_directions();
        dir1->set_shape(static_cast<oaa::proto::enums::LaneShape::Enum>(1));
        dir1->set_is_recommended(true);
        auto* lane2 = step1->add_lanes();
        auto* dir2 = lane2->add_directions();
        dir2->set_shape(static_cast<oaa::proto::enums::LaneShape::Enum>(5));
        dir2->set_is_recommended(false);
        auto* road1 = step1->mutable_road_info();
        road1->add_road_names("US-75 North");
        road1->add_road_names("Downtown");

        auto* step2 = notification.add_steps();
        auto* maneuver2 = step2->mutable_maneuver();
        maneuver2->set_type(static_cast<oaa::proto::enums::ManeuverType::Enum>(2));
        step2->mutable_instruction()->set_text("Second step must not leak");
        step2->add_lanes()->add_directions()->set_shape(
            static_cast<oaa::proto::enums::LaneShape::Enum>(2));
        step2->mutable_road_info()->add_road_names("Second step road");

        notification.add_destinations()->set_address("Stop One");
        notification.add_destinations()->set_address("Stop Two");

        QByteArray payload(notification.ByteSizeLong(), '\0');
        QVERIFY(notification.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::NavigationMessageId::NAV_STEP, payload);

        oaa::proto::messages::NavigationNotification emptyNotification;
        payload = QByteArray(emptyNotification.ByteSizeLong(), '\0');
        QVERIFY(emptyNotification.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::NavigationMessageId::NAV_STEP, payload);

        QCOMPARE(notificationSpy.count(), 2);
        const auto first = qvariant_cast<oaa::hu::NavigationNotificationSnapshot>(
            notificationSpy[0][0]);
        QCOMPARE(first.stepCount, 2);
        QVERIFY(first.hasManeuver);
        QCOMPARE(first.maneuverType, 1);
        QVERIFY(first.hasUpcomingRoad);
        QCOMPARE(first.upcomingRoad, QStringLiteral("Turn right onto Main St"));
        QCOMPARE(first.actionCues,
                 QStringList({QStringLiteral("US-75 North"),
                              QStringLiteral("Downtown")}));
        QCOMPARE(first.lanes.size(), 2);
        QCOMPARE(first.lanes[0].directions.size(), 1);
        QCOMPARE(first.lanes[0].directions[0].shape, 1);
        QVERIFY(first.lanes[0].directions[0].recommended);
        QCOMPARE(first.lanes[1].directions.size(), 1);
        QCOMPARE(first.lanes[1].directions[0].shape, 5);
        QVERIFY(!first.lanes[1].directions[0].recommended);
        QCOMPARE(first.destinations,
                 QStringList({QStringLiteral("Stop One"),
                              QStringLiteral("Stop Two")}));

        const auto second = qvariant_cast<oaa::hu::NavigationNotificationSnapshot>(
            notificationSpy[1][0]);
        QCOMPARE(second.stepCount, 0);
        QVERIFY(!second.hasManeuver);
        QCOMPARE(second.maneuverType, 0);
        QVERIFY(!second.hasUpcomingRoad);
        QVERIFY(second.upcomingRoad.isEmpty());
        QVERIFY(second.actionCues.isEmpty());
        QVERIFY(second.destinations.isEmpty());
        QVERIFY(second.lanes.isEmpty());
    }

    void testPositionSnapshotPreservesOptionalFieldsAndClearsOnEmptyReplacement() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy positionSpy(
            &handler,
            &oaa::hu::NavigationChannelHandler::navigationPositionChanged);

        oaa::proto::messages::NavigationNextTurnDistanceEvent message;
        auto* distance = message.mutable_step_distance()->mutable_distance();
        distance->set_distance_value(300);
        distance->set_display_text("0.3");
        distance->set_distance_unit(
            oaa::proto::messages::DISTANCE_UNIT_MILES_P1);
        message.mutable_step_distance()->set_time_to_step_seconds(-7);

        auto* firstDestination = message.add_destination_distances();
        firstDestination->mutable_distance()->set_distance_value(1200);
        firstDestination->mutable_distance()->set_display_text("0.7 mi");
        firstDestination->mutable_distance()->set_distance_unit(
            oaa::proto::messages::DISTANCE_UNIT_MILES_P1);
        firstDestination->set_estimated_time_of_arrival("4:42 PM");
        firstDestination->set_time_to_arrival_seconds(250);

        auto* secondDestination = message.add_destination_distances();
        secondDestination->mutable_distance()->set_distance_value(2400);
        secondDestination->mutable_distance()->set_display_text("1.5 mi");
        secondDestination->mutable_distance()->set_distance_unit(
            oaa::proto::messages::DISTANCE_UNIT_MILES_P1);
        secondDestination->set_estimated_time_of_arrival("4:48 PM");
        secondDestination->set_time_to_arrival_seconds(0);
        message.mutable_current_road()->set_text("I-35 South");

        QByteArray payload(message.ByteSizeLong(), '\0');
        QVERIFY(message.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(0x8007, payload);

        oaa::proto::messages::NavigationNextTurnDistanceEvent emptyMessage;
        payload = QByteArray(emptyMessage.ByteSizeLong(), '\0');
        QVERIFY(emptyMessage.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(0x8007, payload);

        QCOMPARE(positionSpy.count(), 2);
        const auto first = qvariant_cast<oaa::hu::NavigationPositionSnapshot>(
            positionSpy[0][0]);
        QVERIFY(first.hasStepDistance);
        QVERIFY(first.stepDistance.hasValue);
        QCOMPARE(first.stepDistance.value, 300);
        QVERIFY(first.stepDistance.hasDisplayText);
        QCOMPARE(first.stepDistance.displayText, QStringLiteral("0.3"));
        QVERIFY(first.stepDistance.hasUnit);
        QCOMPARE(first.stepDistance.unit,
                 static_cast<int>(oaa::proto::messages::DISTANCE_UNIT_MILES_P1));
        QVERIFY(first.hasTimeToStep);
        QCOMPARE(first.timeToStepSeconds, qint64(-7));
        QCOMPARE(first.destinationDistances.size(), 2);
        QVERIFY(first.destinationDistances[0].hasDistance);
        QVERIFY(first.destinationDistances[0].distance.hasValue);
        QCOMPARE(first.destinationDistances[0].distance.value, 1200);
        QVERIFY(first.destinationDistances[0].distance.hasDisplayText);
        QCOMPARE(first.destinationDistances[0].distance.displayText,
                 QStringLiteral("0.7 mi"));
        QVERIFY(first.destinationDistances[0].distance.hasUnit);
        QCOMPARE(first.destinationDistances[0].distance.unit,
                 static_cast<int>(oaa::proto::messages::DISTANCE_UNIT_MILES_P1));
        QVERIFY(first.destinationDistances[0].hasEstimatedTimeOfArrival);
        QCOMPARE(first.destinationDistances[0].estimatedTimeOfArrival,
                 QStringLiteral("4:42 PM"));
        QVERIFY(first.destinationDistances[0].hasTimeToArrival);
        QCOMPARE(first.destinationDistances[0].timeToArrivalSeconds, qint64(250));
        QVERIFY(first.destinationDistances[1].hasDistance);
        QVERIFY(first.destinationDistances[1].distance.hasValue);
        QCOMPARE(first.destinationDistances[1].distance.value, 2400);
        QVERIFY(first.destinationDistances[1].distance.hasDisplayText);
        QCOMPARE(first.destinationDistances[1].distance.displayText,
                 QStringLiteral("1.5 mi"));
        QVERIFY(first.destinationDistances[1].distance.hasUnit);
        QCOMPARE(first.destinationDistances[1].distance.unit,
                 static_cast<int>(oaa::proto::messages::DISTANCE_UNIT_MILES_P1));
        QVERIFY(first.destinationDistances[1].hasEstimatedTimeOfArrival);
        QCOMPARE(first.destinationDistances[1].estimatedTimeOfArrival,
                 QStringLiteral("4:48 PM"));
        QVERIFY(first.destinationDistances[1].hasTimeToArrival);
        QCOMPARE(first.destinationDistances[1].timeToArrivalSeconds, qint64(0));
        QVERIFY(first.hasCurrentRoad);
        QCOMPARE(first.currentRoad, QStringLiteral("I-35 South"));

        const auto second = qvariant_cast<oaa::hu::NavigationPositionSnapshot>(
            positionSpy[1][0]);
        QVERIFY(!second.hasStepDistance);
        QVERIFY(!second.hasTimeToStep);
        QVERIFY(second.destinationDistances.isEmpty());
        QVERIFY(!second.hasCurrentRoad);
        QVERIFY(second.currentRoad.isEmpty());
    }

    void testEmptyVehicleEnergyForecastOuterEmitsOnce() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy forecastSpy(
            &handler,
            &oaa::hu::NavigationChannelHandler::vehicleEnergyForecastReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        oaa::proto::messages::VehicleEnergyForecastMessage outer;
        const QByteArray payload = QByteArray::fromStdString(
            outer.SerializeAsString());
        handler.onMessage(0x8008, payload);

        QCOMPARE(forecastSpy.count(), 1);
        QCOMPARE(forecastSpy[0][0].toBool(), false);
        QVERIFY(forecastSpy[0][1].toString().isEmpty());
        QCOMPARE(sendSpy.count(), 0);
    }

    void testVehicleEnergyForecastParsesInnerAndBoundsStructuralSummary() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy forecastSpy(
            &handler,
            &oaa::hu::NavigationChannelHandler::vehicleEnergyForecastReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        oaa::proto::messages::VehicleEnergyForecast inner;
        auto* nextStop = inner.mutable_energy_at_next_stop();
        nextStop->set_distance_meters(1200);
        nextStop->set_arrival_battery_energy_wh(42000);
        nextStop->set_time_to_arrival_seconds(180);
        inner.mutable_distance_to_empty()->set_distance_meters(250000);
        inner.set_forecast_quality(
            oaa::proto::messages::FORECAST_QUALITY_HIGH);
        auto* charging = inner.mutable_next_charging_stop();
        charging->set_min_departure_energy_wh(30000);
        charging->set_max_rated_power_watts(150000);
        charging->set_estimated_charging_time_seconds(900);
        inner.add_stop_details()
            ->mutable_expected_arrival_energy()
            ->set_arrival_battery_energy_wh(28000);
        inner.add_data_authorizations()->set_id("charger-consent");
        inner.add_data_authorizations()->set_id(std::string(800, 'x'));

        oaa::proto::messages::VehicleEnergyForecastMessage outer;
        outer.set_vehicle_energy_forecast(inner.SerializeAsString());
        const QByteArray payload = QByteArray::fromStdString(
            outer.SerializeAsString());
        handler.onMessage(0x8008, payload);

        QCOMPARE(forecastSpy.count(), 1);
        QCOMPARE(forecastSpy[0][0].toBool(), true);
        const QString summary = forecastSpy[0][1].toString();
        QCOMPARE(summary, QString::fromStdString(
                              inner.ShortDebugString()).left(512));
        QCOMPARE(summary.size(), 512);
        QVERIFY(summary.contains(QStringLiteral("energy_at_next_stop")));
        QVERIFY(summary.contains(QStringLiteral("distance_meters: 1200")));
        QVERIFY(summary.contains(QStringLiteral("forecast_quality")));
        QCOMPARE(sendSpy.count(), 0);
    }

    void testMalformedInnerDoesNotInvalidateParseableForecastOuter() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy forecastSpy(
            &handler,
            &oaa::hu::NavigationChannelHandler::vehicleEnergyForecastReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        oaa::proto::messages::VehicleEnergyForecastMessage outer;
        outer.set_vehicle_energy_forecast("\x0a\x05\x08", 3);
        const QByteArray payload = QByteArray::fromStdString(
            outer.SerializeAsString());
        handler.onMessage(0x8008, payload);

        QCOMPARE(forecastSpy.count(), 1);
        QCOMPARE(forecastSpy[0][0].toBool(), false);
        QCOMPARE(forecastSpy[0][1].toString(),
                 QString::fromStdString(
                     outer.ShortDebugString()).left(512));
        QVERIFY(!forecastSpy[0][1].toString().isEmpty());
        QVERIFY(forecastSpy[0][1].toString().size() <= 512);
        QCOMPARE(sendSpy.count(), 0);
    }

    void testMalformedVehicleEnergyForecastOuterIsRejected() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy forecastSpy(
            &handler,
            &oaa::hu::NavigationChannelHandler::vehicleEnergyForecastReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onMessage(0x8008, QByteArray::fromHex("0a0508"));

        QCOMPARE(forecastSpy.count(), 0);
        QCOMPARE(sendSpy.count(), 0);
    }

    // testFocusIndicationEmitsSignal / testFocusIndicationUpdatesState removed —
    // NavigationFocusIndication proto retracted in v1.1 (nav focus is on Control channel)
};

QTEST_GUILESS_MAIN(TestNavigationChannelHandler)
#include "test_navigation_channel_handler.moc"
