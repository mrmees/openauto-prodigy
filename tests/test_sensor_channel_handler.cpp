#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/SensorChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/sensor/SensorStartRequestMessage.pb.h"
#include "oaa/sensor/SensorEventIndicationMessage.pb.h"
#include "oaa/sensor/SensorTypeEnum.pb.h"

class TestSensorChannelHandler : public QObject {
    Q_OBJECT
private:
    static void subscribe(oaa::hu::SensorChannelHandler& handler,
                          oaa::proto::enums::SensorType::Enum sensorType)
    {
        oaa::proto::messages::SensorStartRequestMessage req;
        req.set_sensor_type(sensorType);
        req.set_refresh_interval(1000);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());
        handler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);
    }

    static void verifyNightIndication(const QList<QVariant>& emission, bool expected)
    {
        QCOMPARE(emission[0].value<uint8_t>(), oaa::ChannelId::Sensor);
        QCOMPARE(emission[1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_EVENT_INDICATION));

        const QByteArray payload = emission[2].toByteArray();
        oaa::proto::messages::SensorEventIndication indication;
        QVERIFY(indication.ParseFromArray(payload.constData(), payload.size()));
        QCOMPARE(indication.night_mode_size(), 1);
        QVERIFY(indication.night_mode(0).has_is_night());
        QCOMPARE(indication.night_mode(0).is_night(), expected);
        QCOMPARE(indication.driving_status_size(), 0);
        QCOMPARE(indication.parking_brake_size(), 0);
    }

private slots:
    void testChannelId() {
        oaa::hu::SensorChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Sensor);
    }

    void testInitialDayStateEncoded() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();
        subscribe(handler, oaa::proto::enums::SensorType::NIGHT_DATA);

        QCOMPARE(sendSpy.count(), 2);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Sensor);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_START_RESPONSE));
        verifyNightIndication(sendSpy[1], false);
    }

    void testInitialNightStateRetainedBeforeChannelOpen() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.pushNightMode(true);
        QCOMPARE(sendSpy.count(), 0);

        handler.onChannelOpened();
        QCOMPARE(sendSpy.count(), 0);
        subscribe(handler, oaa::proto::enums::SensorType::NIGHT_DATA);

        QCOMPARE(sendSpy.count(), 2);
        verifyNightIndication(sendSpy[1], true);
    }

    void testNightStateRetainedBeforeSubscription() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();
        handler.pushNightMode(true);
        QCOMPARE(sendSpy.count(), 0);

        subscribe(handler, oaa::proto::enums::SensorType::NIGHT_DATA);

        QCOMPARE(sendSpy.count(), 2);
        verifyNightIndication(sendSpy[1], true);
    }

    void testNightModeUpdate() {
        oaa::hu::SensorChannelHandler handler;
        handler.onChannelOpened();

        subscribe(handler, oaa::proto::enums::SensorType::NIGHT_DATA);

        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.pushNightMode(true);
        handler.pushNightMode(false);

        QCOMPARE(sendSpy.count(), 2);
        verifyNightIndication(sendSpy[0], true);
        verifyNightIndication(sendSpy[1], false);
    }

    void testNightModeNotSentWhenClosed() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        // Channel not opened — pushNightMode should be silently ignored
        handler.pushNightMode(true);
        QCOMPARE(sendSpy.count(), 0);
    }

    void testCloseReopenResubscriptionUsesLatestState() {
        oaa::hu::SensorChannelHandler handler;
        handler.onChannelOpened();
        subscribe(handler, oaa::proto::enums::SensorType::NIGHT_DATA);

        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.onChannelClosed();
        handler.pushNightMode(true);
        QCOMPARE(sendSpy.count(), 0);

        handler.onChannelOpened();
        QCOMPARE(sendSpy.count(), 0);
        subscribe(handler, oaa::proto::enums::SensorType::NIGHT_DATA);

        QCOMPARE(sendSpy.count(), 2);
        verifyNightIndication(sendSpy[1], true);
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

        const QByteArray eventPayload = sendSpy[1][2].toByteArray();
        oaa::proto::messages::SensorEventIndication indication;
        QVERIFY(indication.ParseFromArray(eventPayload.constData(), eventPayload.size()));
        QCOMPARE(indication.parking_brake_size(), 1);
        QCOMPARE(indication.parking_brake(0).parking_brake(), true);
        QCOMPARE(indication.night_mode_size(), 0);
        QCOMPARE(indication.driving_status_size(), 0);
    }

    void testDrivingStatusStartRequestSendsUnrestrictedInitialData() {
        oaa::hu::SensorChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.onChannelOpened();

        subscribe(handler, oaa::proto::enums::SensorType::DRIVING_STATUS);

        QCOMPARE(sendSpy.count(), 2);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_START_RESPONSE));
        QCOMPARE(sendSpy[1][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::SensorMessageId::SENSOR_EVENT_INDICATION));

        const QByteArray eventPayload = sendSpy[1][2].toByteArray();
        oaa::proto::messages::SensorEventIndication indication;
        QVERIFY(indication.ParseFromArray(eventPayload.constData(), eventPayload.size()));
        QCOMPARE(indication.driving_status_size(), 1);
        QCOMPARE(indication.driving_status(0).status(), 0);
        QCOMPARE(indication.night_mode_size(), 0);
        QCOMPARE(indication.parking_brake_size(), 0);
    }
};

QTEST_MAIN(TestSensorChannelHandler)
#include "test_sensor_channel_handler.moc"
