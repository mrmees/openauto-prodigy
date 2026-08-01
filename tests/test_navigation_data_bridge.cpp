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
            distance(16093, 4), 65,
            {destinationDistance(19312, 4, QStringLiteral("4:42 PM"), 3900)});

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
            {Event::State, oaa::hu::NavigationState::Active, true},
            {Event::Notification, {}, true, true, false,
             QStringLiteral("Main St"), {}, true},
            {Event::Position, {}, true, true, false,
             QStringLiteral("Main St"), QStringLiteral("10 mi"), true,
             QStringLiteral("Stop One")},
            {Event::State, oaa::hu::NavigationState::Rerouting, true, false, true},
            {Event::State, oaa::hu::NavigationState::Active, true, false, true},
            {Event::Position, {}, true, false, true},
            {Event::Notification, {}, true, true, false,
             QStringLiteral("Main St"), QStringLiteral("10 mi"), true,
             QStringLiteral("Stop One")},
            {Event::State, oaa::hu::NavigationState::Inactive},
            {Event::State, oaa::hu::NavigationState::Unavailable},
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
            {destinationDistance(19312, 4, QStringLiteral("4:42 PM"), 3900),
             destinationDistance(1609, 4, QStringLiteral("4:48 PM"), 60)}));

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
        emit handler.navigationPositionChanged(position(distance(1609, 4)));
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("1.0 mi"));

        emit handler.navigationPositionChanged(position(distance(17059, 4)));
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("11 mi"));

        emit handler.navigationPositionChanged(position(distance(500, 1)));
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("500 m"));

        emit handler.navigationTurnEvent(QStringLiteral("Legacy Rd"), 3, 1,
                                         QByteArray(), 1609, 4);
        QCOMPARE(bridge.formattedDistance(), QStringLiteral("1.0 mi"));
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
