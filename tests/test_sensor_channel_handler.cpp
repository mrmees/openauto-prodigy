#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/SensorChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/sensor/SensorStartRequestMessage.pb.h"
#include "oaa/sensor/SensorTypeEnum.pb.h"

class TestSensorChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oaa::hu::SensorChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Sensor);
    }

    void testSensorStartRequestEmitsResponse() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        oaa::proto::messages::SensorStartRequestMessage req;
        req.set_sensor_type(oaa::proto::enums::SensorType::NIGHT_DATA);
        req.set_refresh_interval(1000);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onChannelOpened();
        handler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

        // Should send: SensorStartResponse + initial NightMode event
        QCOMPARE(sendSpy.count(), 2);
        // First signal: start response
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Sensor);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_START_RESPONSE));
        // Second signal: initial night mode event
        QCOMPARE(sendSpy[1][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_EVENT_INDICATION));
    }

    void testNightModeUpdate() {
        oaa::hu::SensorChannelHandler handler;
        handler.onChannelOpened();

        // Subscribe to NIGHT_DATA first
        oaa::proto::messages::SensorStartRequestMessage req;
        req.set_sensor_type(oaa::proto::enums::SensorType::NIGHT_DATA);
        req.set_refresh_interval(1000);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());
        handler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.pushNightMode(true);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Sensor);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_EVENT_INDICATION));
    }

    void testNightModeNotSentWhenClosed() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        // Channel not opened — pushNightMode should be silently ignored
        handler.pushNightMode(true);
        QCOMPARE(sendSpy.count(), 0);
    }

    void testNightModeNotSentWithoutSubscription() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        // Channel open but no SENSOR_START_REQUEST for NIGHT_DATA
        handler.onChannelOpened();
        handler.pushNightMode(true);
        QCOMPARE(sendSpy.count(), 0);
    }

    void testParkingBrakeStartRequestSendsResponseAndInitialData() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.onChannelOpened();

        oaa::proto::messages::SensorStartRequestMessage req;
        req.set_sensor_type(oaa::proto::enums::SensorType::PARKING_BRAKE);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

        // Should send: start response + initial parking brake data
        QCOMPARE(sendSpy.count(), 2);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_START_RESPONSE));
        QCOMPARE(sendSpy[1][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_EVENT_INDICATION));
    }

    void testDrivingStatusUpdate() {
        oaa::hu::SensorChannelHandler handler;
        handler.onChannelOpened();

        // Subscribe to DRIVING_STATUS first
        oaa::proto::messages::SensorStartRequestMessage req;
        req.set_sensor_type(oaa::proto::enums::SensorType::DRIVING_STATUS);
        req.set_refresh_interval(1000);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());
        handler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.pushDrivingStatus(0); // UNRESTRICTED

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Sensor);
    }
};

QTEST_MAIN(TestSensorChannelHandler)
#include "test_sensor_channel_handler.moc"
