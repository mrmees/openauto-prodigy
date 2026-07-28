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

    void testReroutingRemainsNavigationActive() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy stateSpy(
            &handler,
            &oaa::hu::NavigationChannelHandler::navigationStateChanged);
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
        QCOMPARE(stateSpy.count(), 1);
        QCOMPARE(stateSpy[0][0].toBool(), true);

        sendState(oaa::proto::messages::NAV_STATE_ACTIVE);
        sendState(oaa::proto::messages::NAV_STATE_REROUTING);
        QCOMPARE(stateSpy.count(), 1);

        sendState(oaa::proto::messages::NAV_STATE_INACTIVE);
        QCOMPARE(stateSpy.count(), 2);
        QCOMPARE(stateSpy[1][0].toBool(), false);

        sendState(oaa::proto::messages::NAV_STATE_UNAVAILABLE);
        QCOMPARE(stateSpy.count(), 2);

        sendState(oaa::proto::messages::NAV_STATE_ACTIVE);
        sendState(oaa::proto::messages::NAV_STATE_UNAVAILABLE);
        QCOMPARE(stateSpy.count(), 4);
        QCOMPARE(stateSpy[2][0].toBool(), true);
        QCOMPARE(stateSpy[3][0].toBool(), false);
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

    void testNotificationMultiStepWithLanes() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy spy(&handler, &oaa::hu::NavigationChannelHandler::navigationNotificationReceived);

        oaa::proto::messages::NavigationNotification msg;

        // Step 1: with lanes and road info
        auto* step1 = msg.add_steps();
        auto* maneuver1 = step1->mutable_maneuver();
        maneuver1->set_type(static_cast<oaa::proto::enums::ManeuverType::Enum>(1));
        auto* instr1 = step1->mutable_instruction();
        instr1->set_text("Turn right onto Main St");
        auto* lane1 = step1->add_lanes();
        auto* dir1 = lane1->add_directions();
        dir1->set_shape(static_cast<oaa::proto::enums::LaneShape::Enum>(1));
        dir1->set_is_recommended(true);
        auto* road1 = step1->mutable_road_info();
        road1->add_road_names("Main St");

        // Step 2: with lanes
        auto* step2 = msg.add_steps();
        auto* maneuver2 = step2->mutable_maneuver();
        maneuver2->set_type(static_cast<oaa::proto::enums::ManeuverType::Enum>(2));
        auto* instr2 = step2->mutable_instruction();
        instr2->set_text("Continue onto Highway 101");
        auto* lane2 = step2->add_lanes();
        auto* dir2 = lane2->add_directions();
        dir2->set_shape(static_cast<oaa::proto::enums::LaneShape::Enum>(2));
        dir2->set_is_recommended(false);

        // Destination
        auto* dest = msg.add_destinations();
        dest->set_address("123 Elm Street");

        QByteArray payload(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::NavigationMessageId::NAV_STEP, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toInt(), 2);         // stepCount
        QCOMPARE(spy[0][1].toInt(), 2);         // total lane count across steps
        QCOMPARE(spy[0][2].toString(), QString("123 Elm Street")); // destination
    }

    void testAuditedCurrentPositionDistanceUsesFieldOne() {
        oaa::hu::NavigationChannelHandler handler;
        QSignalSpy spy(
            &handler,
            &oaa::hu::NavigationChannelHandler::navigationDistanceChanged);

        oaa::proto::messages::NavigationNextTurnDistanceEvent message;
        auto* distance = message.mutable_step_distance()->mutable_distance();
        distance->set_display_text("0.3");
        distance->set_distance_unit(
            oaa::proto::messages::DISTANCE_UNIT_MILES_P1);

        QByteArray payload(message.ByteSizeLong(), '\0');
        QVERIFY(message.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(0x8007, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toString(), QStringLiteral("0.3"));
        QCOMPARE(spy[0][1].toInt(),
                 static_cast<int>(
                     oaa::proto::messages::DISTANCE_UNIT_MILES_P1));
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

QTEST_MAIN(TestNavigationChannelHandler)
#include "test_navigation_channel_handler.moc"
