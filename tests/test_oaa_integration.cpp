#include <QTest>
#include <QSignalSpy>
#include <oaa/Session/AASession.hpp>
#include <oaa/Transport/ReplayTransport.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/HU/Handlers/SensorChannelHandler.hpp>
#include <oaa/HU/Handlers/InputChannelHandler.hpp>
#include <oaa/HU/Handlers/BluetoothChannelHandler.hpp>
#include <oaa/HU/Handlers/WiFiChannelHandler.hpp>
#include "core/aa/ServiceDiscoveryBuilder.hpp"
#include "SensorStartRequestMessage.pb.h"
#include "SensorTypeEnum.pb.h"

class TestOAAIntegration : public QObject {
    Q_OBJECT
private slots:
    void testSessionBootWithAllHandlers() {
        oaa::ReplayTransport transport;
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();

        oaa::AASession session(&transport, config);

        oaa::hu::SensorChannelHandler sensorHandler;
        oaa::hu::InputChannelHandler inputHandler;
        oaa::hu::BluetoothChannelHandler btHandler;
        oaa::hu::WiFiChannelHandler wifiHandler("TestSSID", "TestPass");

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

        // All 12 channels: Video, MediaAudio, SpeechAudio, SystemAudio,
        // Input, Sensor, Bluetooth, WiFi, AVInput, Navigation, MediaStatus, PhoneStatus
        QCOMPARE(config.channels.size(), 12);
    }

    void testHandlerEmitsSendRequested() {
        oaa::ReplayTransport transport;
        oap::aa::ServiceDiscoveryBuilder builder;
        oaa::SessionConfig config = builder.build();
        oaa::AASession session(&transport, config);

        oaa::hu::SensorChannelHandler sensorHandler;
        session.registerChannel(oaa::ChannelId::Sensor, &sensorHandler);

        // Open the channel and subscribe to NIGHT_DATA
        sensorHandler.onChannelOpened();

        oaa::proto::messages::SensorStartRequestMessage req;
        req.set_sensor_type(oaa::proto::enums::SensorType::NIGHT_DATA);
        req.set_refresh_interval(1000);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());
        sensorHandler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

        // Now push night mode — should emit sendRequested
        QSignalSpy sendSpy(&sensorHandler, &oaa::IChannelHandler::sendRequested);
        sensorHandler.pushNightMode(true);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Sensor);
    }
};

QTEST_MAIN(TestOAAIntegration)
#include "test_oaa_integration.moc"
