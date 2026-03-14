#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>
#include "core/aa/NavigationDataBridge.hpp"
#include "core/aa/ManeuverIconProvider.hpp"
#include "core/services/INavigationProvider.hpp"

class TestNavigationDataBridge : public QObject {
    Q_OBJECT
private slots:
    void testDefaults() {
        oap::aa::NavigationDataBridge bridge;
        QCOMPARE(bridge.navActive(), false);
        QCOMPARE(bridge.roadName(), QString());
        QCOMPARE(bridge.maneuverType(), 0);
        QCOMPARE(bridge.turnDirection(), 0);
        QCOMPARE(bridge.distanceMeters(), 0);
        QCOMPARE(bridge.hasManeuverIcon(), false);
        QCOMPARE(bridge.iconVersion(), 0);
        QCOMPARE(bridge.formattedDistance(), QString("0 m"));
    }

    void testNavActiveFromHandler() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        QSignalSpy spy(&bridge, &oap::aa::NavigationDataBridge::navActiveChanged);

        emit handler.navigationStateChanged(true);
        QCOMPARE(bridge.navActive(), true);
        QCOMPARE(spy.count(), 1);

        emit handler.navigationStateChanged(false);
        QCOMPARE(bridge.navActive(), false);
        QCOMPARE(spy.count(), 2);
    }

    void testTurnEventUpdatesProperties() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        QSignalSpy spy(&bridge, &oap::aa::NavigationDataBridge::turnDataChanged);

        emit handler.navigationTurnEvent("Main St", 3, 1, QByteArray(), 500, 1);

        QCOMPARE(bridge.roadName(), QString("Main St"));
        QCOMPARE(bridge.maneuverType(), 3);
        QCOMPARE(bridge.turnDirection(), 1);
        QCOMPARE(bridge.distanceMeters(), 500);
        QCOMPARE(bridge.hasManeuverIcon(), false);
        QCOMPARE(spy.count(), 1);
    }

    void testTurnEventWithIcon() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        QByteArray fakeIcon("\x89PNG\r\n\x1a\n", 8);
        emit handler.navigationTurnEvent("Oak Ave", 1, 2, fakeIcon, 200, 1);

        QCOMPARE(bridge.hasManeuverIcon(), true);
        QCOMPARE(bridge.iconVersion(), 1);
    }

    void testIconVersionIncrements() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        QByteArray icon("\x89PNG", 4);
        emit handler.navigationTurnEvent("A", 1, 1, icon, 100, 1);
        QCOMPARE(bridge.iconVersion(), 1);

        emit handler.navigationTurnEvent("B", 1, 1, icon, 200, 1);
        QCOMPARE(bridge.iconVersion(), 2);

        // No icon -> version does NOT increment
        emit handler.navigationTurnEvent("C", 1, 1, QByteArray(), 300, 1);
        QCOMPARE(bridge.iconVersion(), 2);
    }

    void testNavDeactivateClearsData() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        // Must activate nav first, then send turn data
        emit handler.navigationStateChanged(true);
        emit handler.navigationTurnEvent("Main St", 3, 1, QByteArray("icon", 4), 500, 1);
        QCOMPARE(bridge.roadName(), QString("Main St"));
        QCOMPARE(bridge.hasManeuverIcon(), true);

        emit handler.navigationStateChanged(false);
        QCOMPARE(bridge.navActive(), false);
        QCOMPARE(bridge.roadName(), QString());
        QCOMPARE(bridge.maneuverType(), 0);
        QCOMPARE(bridge.turnDirection(), 0);
        QCOMPARE(bridge.distanceMeters(), 0);
        QCOMPARE(bridge.hasManeuverIcon(), false);
    }

    void testIconProviderWiring() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        oap::aa::ManeuverIconProvider provider;

        bridge.connectToHandler(&handler);
        bridge.setManeuverIconProvider(&provider);

        // Minimal valid 1x1 red PNG
        static const unsigned char png1x1[] = {
            0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,
            0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
            0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,0xde,0x00,0x00,0x00,
            0x0c,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0x00,
            0x00,0x03,0x01,0x01,0x00,0xc9,0xfe,0x92,0xef,0x00,0x00,0x00,
            0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82
        };
        QByteArray pngData(reinterpret_cast<const char*>(png1x1), sizeof(png1x1));

        emit handler.navigationTurnEvent("Test", 1, 1, pngData, 100, 1);

        // Verify the icon provider got the data
        QSize size;
        QImage img = provider.requestImage("current", &size, QSize());
        QVERIFY(!img.isNull());
        QCOMPARE(size, QSize(1, 1));
    }

    // --- Distance formatting tests ---

    void testFormattedDistanceMetersUnder1000() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        emit handler.navigationTurnEvent("", 0, 0, QByteArray(), 500, 1);
        QCOMPARE(bridge.formattedDistance(), QString("500 m"));
    }

    void testFormattedDistanceMetersOver1000() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        emit handler.navigationTurnEvent("", 0, 0, QByteArray(), 1500, 1);
        QCOMPARE(bridge.formattedDistance(), QString("1.5 km"));
    }

    void testFormattedDistanceKm() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        emit handler.navigationTurnEvent("", 0, 0, QByteArray(), 500, 2);
        QCOMPARE(bridge.formattedDistance(), QString("0.5 km"));
    }

    void testFormattedDistanceMiles() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        emit handler.navigationTurnEvent("", 0, 0, QByteArray(), 1609, 3);
        QCOMPARE(bridge.formattedDistance(), QString("1.0 mi"));
    }

    void testFormattedDistanceMilesShort() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        emit handler.navigationTurnEvent("", 0, 0, QByteArray(), 500, 3);
        QCOMPARE(bridge.formattedDistance(), QString("0.3 mi"));
    }

    void testFormattedDistanceFeet() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        emit handler.navigationTurnEvent("", 0, 0, QByteArray(), 3, 4);
        QCOMPARE(bridge.formattedDistance(), QString("10 ft"));
    }

    void testFormattedDistanceYards() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        emit handler.navigationTurnEvent("", 0, 0, QByteArray(), 1, 5);
        QCOMPARE(bridge.formattedDistance(), QString("1 yd"));
    }

    void testFormattedDistanceUnknownUnitFallsBackToMeters() {
        oaa::hu::NavigationChannelHandler handler;
        oap::aa::NavigationDataBridge bridge;
        bridge.connectToHandler(&handler);

        emit handler.navigationTurnEvent("", 0, 0, QByteArray(), 250, 99);
        QCOMPARE(bridge.formattedDistance(), QString("250 m"));
    }

    void testImplementsINavigationProvider() {
        oap::aa::NavigationDataBridge bridge;
        oap::INavigationProvider* provider = &bridge;
        QVERIFY(provider != nullptr);
        QCOMPARE(provider->navActive(), false);
        QCOMPARE(provider->roadName(), QString());
        QCOMPARE(provider->turnDirection(), 0);
        QCOMPARE(provider->hasManeuverIcon(), false);
    }
};

QTEST_GUILESS_MAIN(TestNavigationDataBridge)
#include "test_navigation_data_bridge.moc"
