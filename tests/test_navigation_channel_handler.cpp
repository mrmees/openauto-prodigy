#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include "oaa/navigation/NavigationTurnEventMessage.pb.h"
#include "oaa/navigation/NavigationNotificationMessage.pb.h"
// NavigationFocusIndicationMessage.pb.h removed — retracted in proto v1.1

class TestNavigationChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oaa::hu::NavigationChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Navigation);
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

    // testFocusIndicationEmitsSignal / testFocusIndicationUpdatesState removed —
    // NavigationFocusIndication proto retracted in v1.1 (nav focus is on Control channel)
};

QTEST_MAIN(TestNavigationChannelHandler)
#include "test_navigation_channel_handler.moc"
