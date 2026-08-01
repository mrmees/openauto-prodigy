#include <QSignalSpy>
#include <QTest>

#include <limits>

#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>

#include "core/aa/ManeuverIconProvider.hpp"
#include "core/aa/NavigationDataBridge.hpp"
#include "core/services/INavigationProvider.hpp"

namespace {

oaa::hu::NavigationDistanceData distance(int meters, int unit,
                                         const QString& displayText = {})
{
    oaa::hu::NavigationDistanceData result;
    result.hasValue = true;
    result.value = meters;
    result.hasUnit = true;
    result.unit = unit;
    result.hasDisplayText = !displayText.isEmpty();
    result.displayText = displayText;
    return result;
}

oaa::hu::NavigationNotificationSnapshot notification(
    const QString& upcomingRoad = QStringLiteral("Main St"),
    const QStringList& actionCues = {},
    const oaa::hu::NavigationLaneGuidance& lanes = {},
    const QStringList& destinations = {QStringLiteral("Stop One")})
{
    oaa::hu::NavigationNotificationSnapshot result;
    result.stepCount = 1;
    result.hasManeuver = true;
    result.maneuverType = 5;
    result.hasUpcomingRoad = !upcomingRoad.isEmpty();
    result.upcomingRoad = upcomingRoad;
    result.actionCues = actionCues;
    result.lanes = lanes;
    result.destinations = destinations;
    return result;
}

oaa::hu::NavigationPositionSnapshot position(
    const oaa::hu::NavigationDistanceData& stepDistance = distance(500, 1),
    qint64 timeToStepSeconds = 65,
    const QList<oaa::hu::NavigationDestinationDistanceData>& destinations = {})
{
    oaa::hu::NavigationPositionSnapshot result;
    result.hasStepDistance = true;
    result.stepDistance = stepDistance;
    result.hasTimeToStep = timeToStepSeconds != 0;
    result.timeToStepSeconds = timeToStepSeconds;
    result.destinationDistances = destinations;
    return result;
}

oaa::hu::NavigationDestinationDistanceData destinationDistance(
    int meters, int unit, const QString& eta, qint64 arrivalSeconds,
    const QString& displayText = {})
{
    oaa::hu::NavigationDestinationDistanceData result;
    result.hasDistance = true;
    result.distance = distance(meters, unit, displayText);
    result.hasEstimatedTimeOfArrival = !eta.isEmpty();
    result.estimatedTimeOfArrival = eta;
    result.hasTimeToArrival = arrivalSeconds != 0;
    result.timeToArrivalSeconds = arrivalSeconds;
    return result;
}

void makeActive(oaa::hu::NavigationChannelHandler& handler)
{
    emit handler.navigationStateSnapshotChanged(oaa::hu::NavigationState::Active);
}

void publishVisible(oaa::hu::NavigationChannelHandler& handler,
                    const oaa::hu::NavigationNotificationSnapshot& notificationSnapshot,
                    const oaa::hu::NavigationPositionSnapshot& positionSnapshot)
{
    makeActive(handler);
    emit handler.navigationNotificationChanged(notificationSnapshot);
    emit handler.navigationPositionChanged(positionSnapshot);
}

} // namespace

class TestNavigationDataBridge : public QObject {
    Q_OBJECT

private slots:
    void testDefaultsAndProviderCompatibility()
    {
        oap::aa::NavigationDataBridge bridge;
        oap::INavigationProvider* provider = &bridge;

        QCOMPARE(bridge.navigationState(),
                 static_cast<int>(oaa::hu::NavigationState::Unavailable));
        QVERIFY(!bridge.navActive());
        QVERIFY(!bridge.guidanceFresh());
        QVERIFY(!bridge.rerouting());
        QVERIFY(!bridge.hasActionCue());
        QVERIFY(!bridge.hasTimeToStep());
        QCOMPARE(bridge.destinationCount(), 0);
        QVERIFY(!bridge.hasDestinationDistance());
        QVERIFY(!bridge.hasDestinationEta());
        QVERIFY(!bridge.hasTimeToArrival());
        QCOMPARE(provider->formattedDistance(), QStringLiteral("0 m"));
        QVERIFY(bridge.laneModel() != nullptr);
    }

    void testReroutingFreshnessSequence()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        QSignalSpy activeSpy(&bridge, &oap::aa::NavigationDataBridge::navActiveChanged);
        QSignalSpy turnSpy(&bridge, &oap::aa::NavigationDataBridge::turnDataChanged);
        QSignalSpy distanceSpy(&bridge, &oap::aa::NavigationDataBridge::distanceChanged);
        QSignalSpy laneSpy(&bridge, &oap::aa::NavigationDataBridge::laneGuidanceChanged);
        QSignalSpy tripSpy(&bridge, &oap::aa::NavigationDataBridge::tripDataChanged);
        QSignalSpy presentationSpy(
            &bridge, &oap::aa::NavigationDataBridge::navigationPresentationChanged);

        const auto freshNotification = notification(
            QStringLiteral("Main St"), {QStringLiteral("Downtown")},
            {{{{1, true}}}});
        const auto freshPosition = position(
            distance(16093, 4, QStringLiteral("10")), 65,
            {destinationDistance(19312, 4, QStringLiteral("4:42 PM"), 3900,
                                 QStringLiteral("12"))});

        enum class Event { State, Notification, Position };
        struct Step {
            Event event;
            oaa::hu::NavigationState state = oaa::hu::NavigationState::Unavailable;
            bool navActive = false;
            bool guidanceFresh = false;
            bool rerouting = false;
            QString road;
            QString distance;
            bool lanes = false;
            QString destination;
        };
        const QList<Step> steps{
            {Event::State, oaa::hu::NavigationState::Active, true, false, false,
             {}, QStringLiteral("0 m")},
            {Event::Notification, {}, true, true, false,
             QStringLiteral("Main St"), QStringLiteral("0 m"), true},
            {Event::Position, {}, true, true, false,
             QStringLiteral("Main St"), QStringLiteral("10 mi"), true,
             QStringLiteral("Stop One")},
            {Event::State, oaa::hu::NavigationState::Rerouting, true, false, true,
             {}, QStringLiteral("0 m")},
            {Event::State, oaa::hu::NavigationState::Active, true, false, true,
             {}, QStringLiteral("0 m")},
            {Event::Position, {}, true, false, true, {}, QStringLiteral("0 m")},
            {Event::Notification, {}, true, true, false,
             QStringLiteral("Main St"), QStringLiteral("10 mi"), true,
             QStringLiteral("Stop One")},
            {Event::State, oaa::hu::NavigationState::Inactive, false, false, false,
             {}, QStringLiteral("0 m")},
            {Event::State, oaa::hu::NavigationState::Unavailable, false, false, false,
             {}, QStringLiteral("0 m")},
        };

        for (const Step& step : steps) {
            switch (step.event) {
            case Event::State:
                emit handler.navigationStateSnapshotChanged(step.state);
                break;
            case Event::Notification:
                emit handler.navigationNotificationChanged(freshNotification);
                break;
            case Event::Position:
                emit handler.navigationPositionChanged(freshPosition);
                break;
            }
            QCOMPARE(bridge.navActive(), step.navActive);
            QCOMPARE(bridge.guidanceFresh(), step.guidanceFresh);
            QCOMPARE(bridge.rerouting(), step.rerouting);
            QCOMPARE(bridge.roadName(), step.road);
            QCOMPARE(bridge.formattedDistance(), step.distance);
            QCOMPARE(bridge.hasLaneGuidance(), step.lanes);
            QCOMPARE(bridge.destination(), step.destination);
        }

        QVERIFY(!bridge.hasDestinationDistance());
        QVERIFY(activeSpy.count() >= 2);
        QVERIFY(turnSpy.count() >= 4);
        QVERIFY(distanceSpy.count() >= 4);
        QVERIFY(laneSpy.count() >= 2);
        QVERIFY(tripSpy.count() >= 4);
        QVERIFY(presentationSpy.count() >= 5);
    }

    void testNotificationReplacementSelectsOneDistinctCue()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        makeActive(handler);
        const auto full = notification(
            QStringLiteral("I-35 North"),
            {QString(), QStringLiteral(" I-35 North "),
             QStringLiteral(" US-77 North "), QStringLiteral("Downtown")},
            {{{{5, true}}}}, {QStringLiteral("Stop One")});
        emit handler.navigationNotificationChanged(full);
        QVERIFY(bridge.hasActionCue());
        QCOMPARE(bridge.actionCue(), QStringLiteral("US-77 North"));
        QVERIFY(bridge.hasLaneGuidance());

        emit handler.navigationNotificationChanged(notification({}, {}, {}, {}));
        QVERIFY(!bridge.hasActionCue());
        QCOMPARE(bridge.actionCue(), QString());
        QVERIFY(!bridge.hasLaneGuidance());
        QCOMPARE(bridge.destination(), QString());
    }

    void testDestinationPairsIndexZeroAndMultiStopHidesRemainingDuration()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        makeActive(handler);

        emit handler.navigationNotificationChanged(notification(
            QStringLiteral("Main St"), {}, {}, {QStringLiteral("Stop One")}));
        emit handler.navigationPositionChanged(position(
            distance(100, 1), 0,
            {destinationDistance(19312, 4, QStringLiteral("4:42 PM"), 3900,
                                 QStringLiteral("12")),
             destinationDistance(1609, 4, QStringLiteral("4:48 PM"), 60,
                                 QStringLiteral("1.0"))}));

        QCOMPARE(bridge.destination(), QStringLiteral("Stop One"));
        QCOMPARE(bridge.formattedDestinationDistance(), QStringLiteral("12 mi"));
        QCOMPARE(bridge.destinationEta(), QStringLiteral("4:42 PM"));
        QCOMPARE(bridge.formattedTimeToArrival(), QStringLiteral("1 h 5 min"));

        emit handler.navigationNotificationChanged(notification(
            QStringLiteral("Main St"), {}, {},
            {QStringLiteral("Stop One"), QStringLiteral("Stop Two")}));
        QCOMPARE(bridge.destinationCount(), 2);
        QCOMPARE(bridge.formattedDestinationDistance(), QStringLiteral("12 mi"));
        QCOMPARE(bridge.destinationEta(), QStringLiteral("4:42 PM"));
        QVERIFY(!bridge.hasTimeToArrival());

        emit handler.navigationNotificationChanged(notification({}, {}, {}, {}));
        QCOMPARE(bridge.destination(), QString());
        QVERIFY(!bridge.hasDestinationDistance());

        emit handler.navigationNotificationChanged(notification());
        emit handler.navigationPositionChanged({});
        QVERIFY(!bridge.hasDestinationDistance());
        QVERIFY(!bridge.hasDestinationEta());
    }

    void testDurationFormatting_data()
    {
        QTest::addColumn<qint64>("seconds");
        QTest::addColumn<bool>("present");
        QTest::addColumn<QString>("expected");
        QTest::newRow("absent") << qint64(0) << false << QString();
        QTest::newRow("one") << qint64(1) << true << QStringLiteral("<1 min");
        QTest::newRow("fifty-nine") << qint64(59) << true << QStringLiteral("<1 min");
        QTest::newRow("one-minute") << qint64(60) << true << QStringLiteral("1 min");
        QTest::newRow("fifty-nine-minutes") << qint64(3599) << true << QStringLiteral("59 min");
        QTest::newRow("one-hour") << qint64(3600) << true << QStringLiteral("1 h");
        QTest::newRow("hour-five") << qint64(3900) << true << QStringLiteral("1 h 5 min");
        QTest::newRow("maximum") << std::numeric_limits<qint64>::max()
                                  << true << QStringLiteral("2562047788015215 h 30 min");
    }

    void testDurationFormatting()
    {
        QFETCH(qint64, seconds);
        QFETCH(bool, present);
        QFETCH(QString, expected);
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        makeActive(handler);
        emit handler.navigationNotificationChanged(notification());
        auto snapshot = position();
        snapshot.hasTimeToStep = present;
        snapshot.timeToStepSeconds = seconds;
        snapshot.destinationDistances = {
            destinationDistance(1609, 4, QStringLiteral("4:42 PM"), seconds)};
        snapshot.destinationDistances[0].hasTimeToArrival = present;
        emit handler.navigationPositionChanged(snapshot);

        QCOMPARE(bridge.hasTimeToStep(), present && seconds > 0);
        QCOMPARE(bridge.formattedTimeToStep(), expected);
        QCOMPARE(bridge.hasTimeToArrival(), present && seconds > 0);
        QCOMPARE(bridge.formattedTimeToArrival(), expected);
    }

    void testNonPositiveDurationsAreAbsent_data()
    {
        QTest::addColumn<qint64>("seconds");
        QTest::newRow("zero") << qint64(0);
        QTest::newRow("negative") << qint64(-1);
    }

    void testNonPositiveDurationsAreAbsent()
    {
        QFETCH(qint64, seconds);
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        makeActive(handler);
        emit handler.navigationNotificationChanged(notification());
        auto snapshot = position();
        snapshot.hasTimeToStep = true;
        snapshot.timeToStepSeconds = seconds;
        snapshot.destinationDistances = {
            destinationDistance(1609, 4, QStringLiteral("4:42 PM"), seconds)};
        snapshot.destinationDistances[0].hasTimeToArrival = true;
        emit handler.navigationPositionChanged(snapshot);

        QVERIFY(!bridge.hasTimeToStep());
        QVERIFY(!bridge.hasTimeToArrival());
        QCOMPARE(bridge.formattedTimeToStep(), QString());
        QCOMPARE(bridge.formattedTimeToArrival(), QString());
    }

    void testModernAndLegacyDistanceFormatting()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        makeActive(handler);
        emit handler.navigationNotificationChanged(notification());
        emit handler.navigationPositionChanged(
            position(distance(1609, 4, QStringLiteral("1.0"))));
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("1.0 mi"));

        emit handler.navigationPositionChanged(
            position(distance(17059, 4, QStringLiteral("10.6"))));
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("11 mi"));

        emit handler.navigationPositionChanged(
            position(distance(500, 1, QStringLiteral("500"))));
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("500 m"));

        emit handler.navigationTurnEvent(QStringLiteral("Legacy Rd"), 3, 1,
                                         QByteArray(), 1609, 4);
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("1.0 mi"));
    }

    void testLegacyTurnEventPropertyAndIconRegressions()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        makeActive(handler);

        QSignalSpy turnSpy(&bridge, &oap::aa::NavigationDataBridge::turnDataChanged);
        const QByteArray icon("png", 3);
        emit handler.navigationTurnEvent(QStringLiteral("Main St"), 3, 1,
                                         icon, 500, 1);
        QCOMPARE(bridge.roadName(), QStringLiteral("Main St"));
        QCOMPARE(bridge.maneuverType(), 3);
        QCOMPARE(bridge.turnDirection(), 1);
        QCOMPARE(bridge.distanceMeters(), 500);
        QVERIFY(bridge.hasManeuverIcon());
        QCOMPARE(bridge.iconVersion(), 1);
        QCOMPARE(turnSpy.count(), 1);

        emit handler.navigationTurnEvent(QStringLiteral("Oak Ave"), 1, 2,
                                         icon, 200, 1);
        QCOMPARE(bridge.iconVersion(), 2);
        emit handler.navigationTurnEvent(QStringLiteral("No icon"), 1, 2,
                                         QByteArray(), 300, 1);
        QCOMPARE(bridge.iconVersion(), 2);
        QVERIFY(bridge.hasManeuverIcon());
    }

    void testLegacyIconProviderAndDeactivateClearing()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        oap::aa::ManeuverIconProvider provider;
        bridge.connectToHandler(&handler);
        bridge.setManeuverIconProvider(&provider);
        makeActive(handler);

        static const unsigned char png1x1[] = {
            0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,
            0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
            0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,0xde,0x00,0x00,0x00,
            0x0c,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0x00,
            0x00,0x03,0x01,0x01,0x00,0xc9,0xfe,0x92,0xef,0x00,0x00,0x00,
            0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82
        };
        emit handler.navigationTurnEvent(
            QStringLiteral("Test"), 1, 1,
            QByteArray(reinterpret_cast<const char*>(png1x1), sizeof(png1x1)),
            100, 1);
        QSize size;
        QVERIFY(!provider.requestImage(QStringLiteral("current"), &size, {}).isNull());
        QCOMPARE(size, QSize(1, 1));

        emit handler.navigationStateSnapshotChanged(oaa::hu::NavigationState::Inactive);
        QVERIFY(!bridge.navActive());
        QCOMPARE(bridge.roadName(), QString());
        QCOMPARE(bridge.maneuverType(), 0);
        QCOMPARE(bridge.turnDirection(), 0);
        QCOMPARE(bridge.distanceMeters(), 0);
        QVERIFY(!bridge.hasManeuverIcon());
        QVERIFY(!bridge.hasDistance());
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("0 m"));
    }

    void testLegacyDistanceFormatting_data()
    {
        QTest::addColumn<int>("meters");
        QTest::addColumn<int>("unit");
        QTest::addColumn<QString>("expected");
        QTest::newRow("meters") << 500 << 1 << QStringLiteral("500 m");
        QTest::newRow("meters-to-km") << 1500 << 1 << QStringLiteral("1.5 km");
        QTest::newRow("kilometers") << 500 << 2 << QStringLiteral("0.5 km");
        QTest::newRow("miles") << 1609 << 4 << QStringLiteral("1.0 mi");
        QTest::newRow("miles-p1") << 500 << 5 << QStringLiteral("0.3 mi");
        QTest::newRow("miles-whole") << 17059 << 4 << QStringLiteral("11 mi");
        QTest::newRow("feet") << 3 << 6 << QStringLiteral("10 ft");
        QTest::newRow("yards") << 1 << 7 << QStringLiteral("1 yd");
        QTest::newRow("unknown") << 250 << 99 << QStringLiteral("250 m");
    }

    void testLegacyDistanceFormatting()
    {
        QFETCH(int, meters);
        QFETCH(int, unit);
        QFETCH(QString, expected);
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        makeActive(handler);
        emit handler.navigationTurnEvent({}, 0, 0, {}, meters, unit);
        QCOMPARE(bridge.formattedDistance(), expected);
    }

    void testModernDistanceFormatting_data()
    {
        QTest::addColumn<QString>("displayText");
        QTest::addColumn<int>("unit");
        QTest::addColumn<QString>("expected");
        QTest::newRow("miles") << QStringLiteral("0.3") << 5 << QStringLiteral("0.3 mi");
        QTest::newRow("miles-9.9") << QStringLiteral("9.9") << 5 << QStringLiteral("9.9 mi");
        QTest::newRow("miles-whole") << QStringLiteral("10.6") << 5 << QStringLiteral("11 mi");
        QTest::newRow("feet") << QStringLiteral("500") << 6 << QStringLiteral("500 ft");
        QTest::newRow("yards") << QStringLiteral("200") << 7 << QStringLiteral("200 yd");
        QTest::newRow("kilometers") << QStringLiteral("1.5") << 2 << QStringLiteral("1.5 km");
        QTest::newRow("kilometers-p1") << QStringLiteral("0.3") << 3 << QStringLiteral("0.3 km");
    }

    void testModernDistanceFormatting()
    {
        QFETCH(QString, displayText);
        QFETCH(int, unit);
        QFETCH(QString, expected);
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        publishVisible(handler, notification(), position(distance(1609, unit, displayText)));
        QCOMPARE(bridge.formattedDistance(), expected);
    }

    void testModernDistanceRequiresPhoneDisplayText()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        publishVisible(handler, notification(), position(
            distance(1609, 4, QStringLiteral("1.0")), 0,
            {destinationDistance(19312, 4, QStringLiteral("4:42 PM"), 0,
                                 QStringLiteral("12"))}));
        QVERIFY(bridge.hasDistance());
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("1.0 mi"));
        QVERIFY(bridge.hasDestinationDistance());
        QCOMPARE(bridge.formattedDestinationDistance(), QStringLiteral("12 mi"));

        auto valueOnly = distance(1609, 4);
        valueOnly.hasDisplayText = false;
        emit handler.navigationPositionChanged(position(valueOnly, 0,
            {destinationDistance(19312, 4, QStringLiteral("4:42 PM"), 0)}));
        QVERIFY(!bridge.hasDistance());
        QCOMPARE(bridge.distanceMeters(), 0);
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("0 m"));
        QVERIFY(!bridge.hasDestinationDistance());
        QCOMPARE(bridge.formattedDestinationDistance(), QString());
    }

    void testLaneSnapshotsRetainPresentationAndAvoidDuplicatePulses()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        const oaa::hu::NavigationLaneGuidance lanes{
            {{{1, false}, {5, true}}},
            {{{8, true}}},
        };
        makeActive(handler);
        QSignalSpy guidanceSpy(&bridge, &oap::aa::NavigationDataBridge::laneGuidanceChanged);
        QSignalSpy resetSpy(bridge.laneModel(), &QAbstractItemModel::modelReset);
        emit handler.navigationNotificationChanged(notification({}, {}, lanes));
        QCOMPARE(guidanceSpy.count(), 1);
        QCOMPARE(resetSpy.count(), 1);
        QVERIFY(bridge.hasLaneGuidance());
        QCOMPARE(bridge.laneModel()->rowCount(), 2);

        const int directionsRole = bridge.laneModel()->roleNames().key(
            QByteArrayLiteral("directions"), -1);
        const QVariantList directions = bridge.laneModel()->data(
            bridge.laneModel()->index(0, 0), directionsRole).toList();
        QCOMPARE(directions[0].toMap().value(QStringLiteral("shape")).toString(),
                 QStringLiteral("straight"));
        QCOMPARE(directions[1].toMap().value(QStringLiteral("shape")).toString(),
                 QStringLiteral("normal_right"));

        emit handler.navigationNotificationChanged(notification({}, {}, lanes));
        QCOMPARE(guidanceSpy.count(), 1);
        QCOMPARE(resetSpy.count(), 1);

        emit handler.navigationNotificationChanged(notification({}, {}, {{{{1, true}}}}));
        QCOMPARE(guidanceSpy.count(), 2);
        QCOMPARE(resetSpy.count(), 2);

        emit handler.navigationNotificationChanged(notification({}, {}, {}));
        QCOMPARE(guidanceSpy.count(), 3);
        QCOMPARE(resetSpy.count(), 3);
        QVERIFY(!bridge.hasLaneGuidance());
    }

    void testLaneShapeTokensAreExhaustive()
    {
        const QStringList expected{
            QStringLiteral("unknown"), QStringLiteral("straight"),
            QStringLiteral("slight_left"), QStringLiteral("slight_right"),
            QStringLiteral("normal_left"), QStringLiteral("normal_right"),
            QStringLiteral("sharp_left"), QStringLiteral("sharp_right"),
            QStringLiteral("u_turn_left"), QStringLiteral("u_turn_right"),
            QStringLiteral("unknown_future"),
        };
        oaa::hu::NavigationLaneGuidance lanes;
        for (int shape = 0; shape < expected.size(); ++shape)
            lanes.append({{{shape, shape % 2 == 0}}});
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        makeActive(handler);
        emit handler.navigationNotificationChanged(notification({}, {}, lanes));
        const int directionsRole = bridge.laneModel()->roleNames().key(
            QByteArrayLiteral("directions"), -1);
        for (int row = 0; row < expected.size(); ++row) {
            const auto direction = bridge.laneModel()->data(
                bridge.laneModel()->index(row, 0), directionsRole).toList()[0].toMap();
            QCOMPARE(direction.value(QStringLiteral("shape")).toString(), expected[row]);
            QCOMPARE(direction.value(QStringLiteral("recommended")).toBool(), row % 2 == 0);
        }
    }

    void testDeactivateSignalsLaneClearOnlyWhenPopulated()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        QSignalSpy guidanceSpy(&bridge, &oap::aa::NavigationDataBridge::laneGuidanceChanged);
        QSignalSpy resetSpy(bridge.laneModel(), &QAbstractItemModel::modelReset);

        makeActive(handler);
        emit handler.navigationStateSnapshotChanged(oaa::hu::NavigationState::Inactive);
        QCOMPARE(guidanceSpy.count(), 0);
        QCOMPARE(resetSpy.count(), 0);

        makeActive(handler);
        emit handler.navigationNotificationChanged(notification({}, {}, {{{{1, true}}}}));
        QCOMPARE(guidanceSpy.count(), 1);
        QCOMPARE(resetSpy.count(), 1);

        emit handler.navigationStateSnapshotChanged(oaa::hu::NavigationState::Inactive);
        QCOMPARE(guidanceSpy.count(), 2);
        QCOMPARE(resetSpy.count(), 2);
        QVERIFY(!bridge.hasLaneGuidance());
    }

    void testModernDistanceSnapshotOverridesLegacyDistance()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);
        makeActive(handler);
        emit handler.navigationTurnEvent({}, 0, 0, {}, 1609, 4);
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("1.0 mi"));

        emit handler.navigationPositionChanged(
            position(distance(0, 5, QStringLiteral("0.8"))));
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("0.8 mi"));
    }

    void testProviderInterfaceAndLegacyDistancePresence()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        oap::INavigationProvider* provider = &bridge;
        bridge.connectToHandler(&handler);
        QVERIFY(provider != nullptr);
        QVERIFY(!provider->navActive());
        QVERIFY(!provider->hasDistance());
        QCOMPARE(provider->destination(), QString());

        makeActive(handler);
        emit handler.navigationTurnEvent({}, 0, 0, {}, -1, 1);
        QVERIFY(!provider->hasDistance());
        emit handler.navigationTurnEvent({}, 0, 0, {}, 0, 1);
        QVERIFY(provider->hasDistance());
    }

    void testLegacyTurnEventEndsRerouteAndKeepsIconProvider()
    {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        oap::aa::ManeuverIconProvider provider;
        bridge.connectToHandler(&handler);
        bridge.setManeuverIconProvider(&provider);

        makeActive(handler);
        emit handler.navigationStateSnapshotChanged(oaa::hu::NavigationState::Rerouting);
        makeActive(handler);
        const QByteArray icon("png", 3);
        emit handler.navigationTurnEvent(QStringLiteral("Legacy Rd"), 3, 1,
                                         icon, 500, 1);

        QVERIFY(bridge.guidanceFresh());
        QVERIFY(!bridge.rerouting());
        QCOMPARE(bridge.roadName(), QStringLiteral("Legacy Rd"));
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("500 m"));
        QVERIFY(bridge.hasManeuverIcon());
        QCOMPARE(bridge.iconVersion(), 1);
        QVERIFY(!bridge.hasActionCue());
        QVERIFY(!bridge.hasTimeToStep());
        QVERIFY(!bridge.hasDestinationDistance());
    }
};

QTEST_GUILESS_MAIN(TestNavigationDataBridge)
#include "test_navigation_data_bridge.moc"
