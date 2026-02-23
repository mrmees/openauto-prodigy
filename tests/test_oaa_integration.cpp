#include <QTest>
#include <QSignalSpy>
#include <oaa/Session/AASession.hpp>
#include <oaa/Transport/ReplayTransport.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "core/aa/handlers/SensorChannelHandler.hpp"
#include "core/aa/handlers/InputChannelHandler.hpp"
#include "core/aa/handlers/BluetoothChannelHandler.hpp"
#include "core/aa/handlers/WiFiChannelHandler.hpp"
#include "core/aa/ServiceDiscoveryBuilder.hpp"

class TestOAAIntegration : public QObject {
    Q_OBJECT
private slots:
    void testSessionBootWithAllHandlers() {
        oaa::ReplayTransport transport;
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();

        oaa::AASession session(&transport, config);

        oap::aa::SensorChannelHandler sensorHandler;
        oap::aa::InputChannelHandler inputHandler;
        oap::aa::BluetoothChannelHandler btHandler;
        oap::aa::WiFiChannelHandler wifiHandler("TestSSID", "TestPass");

        session.registerChannel(oaa::ChannelId::Sensor, &sensorHandler);
        session.registerChannel(oaa::ChannelId::Input, &inputHandler);
        session.registerChannel(oaa::ChannelId::Bluetooth, &btHandler);
        session.registerChannel(oaa::ChannelId::WiFi, &wifiHandler);

        // Session should be idle before start
        QCOMPARE(session.state(), oaa::SessionState::Idle);
    }

    void testSessionStartAdvancesState() {
        oaa::ReplayTransport transport;
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();
        oaa::AASession session(&transport, config);

        QSignalSpy stateSpy(&session, &oaa::AASession::stateChanged);

        transport.simulateConnect();
        session.start();

        // After connect + start, should be in VersionExchange (waiting for phone)
        QVERIFY(stateSpy.count() >= 1);
        // First state change should be Connecting or VersionExchange
        auto firstState = stateSpy[0][0].value<oaa::SessionState>();
        QVERIFY(firstState == oaa::SessionState::Connecting ||
                firstState == oaa::SessionState::VersionExchange);
    }

    void testConfigHasNineChannels() {
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();

        // All 9 channels: Video, MediaAudio, SpeechAudio, SystemAudio,
        // Input, Sensor, Bluetooth, WiFi, AVInput
        QCOMPARE(config.channels.size(), 9);
    }

    void testHandlerSendRoutesToTransport() {
        oaa::ReplayTransport transport;
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();
        oaa::AASession session(&transport, config);

        oap::aa::SensorChannelHandler sensorHandler;
        session.registerChannel(oaa::ChannelId::Sensor, &sensorHandler);

        // Open the channel and push data
        sensorHandler.onChannelOpened();
        QSignalSpy sendSpy(&sensorHandler, &oaa::IChannelHandler::sendRequested);
        sensorHandler.pushNightMode(true);

        // Handler should have emitted sendRequested
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Sensor);
    }
};

QTEST_MAIN(TestOAAIntegration)
#include "test_oaa_integration.moc"
