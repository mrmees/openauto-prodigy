#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QtEndian>
#include <oaa/Channel/ControlChannel.hpp>

#include "PingRequestMessage.pb.h"
#include "PingResponseMessage.pb.h"
#include "ChannelOpenRequestMessage.pb.h"
#include "ChannelOpenResponseMessage.pb.h"
#include "ShutdownRequestMessage.pb.h"
#include "StatusEnum.pb.h"
#include "ShutdownReasonEnum.pb.h"

class TestControlChannel : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oaa::ControlChannel ctrl;
        QCOMPARE(ctrl.channelId(), uint8_t(0));
    }

    void testSendVersionRequest() {
        oaa::ControlChannel ctrl;
        QSignalSpy sendSpy(&ctrl, &oaa::IChannelHandler::sendRequested);

        ctrl.sendVersionRequest(1, 7);

        QCOMPARE(sendSpy.count(), 1);
        auto args = sendSpy.first();
        QCOMPARE(args[0].value<uint8_t>(), uint8_t(0));
        QCOMPARE(args[1].value<uint16_t>(), uint16_t(0x0001));
        QByteArray payload = args[2].toByteArray();
        QCOMPARE(payload.size(), 4);
        QCOMPARE(uint8_t(payload[0]), uint8_t(0x00));
        QCOMPARE(uint8_t(payload[1]), uint8_t(0x01)); // major=1
        QCOMPARE(uint8_t(payload[2]), uint8_t(0x00));
        QCOMPARE(uint8_t(payload[3]), uint8_t(0x07)); // minor=7
    }

    void testReceiveVersionResponseMatch() {
        oaa::ControlChannel ctrl;
        QSignalSpy versionSpy(&ctrl, &oaa::ControlChannel::versionReceived);

        // major=1, minor=7, status=MATCH(0x0000)
        QByteArray payload(6, '\0');
        qToBigEndian<uint16_t>(1, reinterpret_cast<uchar*>(payload.data()));
        qToBigEndian<uint16_t>(7, reinterpret_cast<uchar*>(payload.data() + 2));
        qToBigEndian<uint16_t>(0x0000, reinterpret_cast<uchar*>(payload.data() + 4));

        ctrl.onMessage(0x0002, payload);

        QCOMPARE(versionSpy.count(), 1);
        QCOMPARE(versionSpy[0][0].value<uint16_t>(), uint16_t(1));
        QCOMPARE(versionSpy[0][1].value<uint16_t>(), uint16_t(7));
        QCOMPARE(versionSpy[0][2].toBool(), true);
    }

    void testReceiveVersionResponseMismatch() {
        oaa::ControlChannel ctrl;
        QSignalSpy versionSpy(&ctrl, &oaa::ControlChannel::versionReceived);

        QByteArray payload(6, '\0');
        qToBigEndian<uint16_t>(1, reinterpret_cast<uchar*>(payload.data()));
        qToBigEndian<uint16_t>(7, reinterpret_cast<uchar*>(payload.data() + 2));
        qToBigEndian<uint16_t>(0xFFFF, reinterpret_cast<uchar*>(payload.data() + 4));

        ctrl.onMessage(0x0002, payload);

        QCOMPARE(versionSpy.count(), 1);
        QCOMPARE(versionSpy[0][2].toBool(), false);
    }

    void testReceiveVersionResponseTooShort() {
        oaa::ControlChannel ctrl;
        QSignalSpy versionSpy(&ctrl, &oaa::ControlChannel::versionReceived);

        ctrl.onMessage(0x0002, QByteArray(2, '\0')); // too short

        QCOMPARE(versionSpy.count(), 1);
        QCOMPARE(versionSpy[0][2].toBool(), false); // mismatch
    }

    void testPingAutoResponse() {
        oaa::ControlChannel ctrl;
        QSignalSpy sendSpy(&ctrl, &oaa::IChannelHandler::sendRequested);
        QSignalSpy pingSpy(&ctrl, &oaa::ControlChannel::pingReceived);

        // Build PingRequest protobuf
        oaa::proto::messages::PingRequest req;
        req.set_timestamp(12345);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        ctrl.onMessage(0x000b, payload);

        // Should auto-respond with PingResponse
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(), uint16_t(0x000c));

        // Verify response contains same timestamp
        QByteArray respPayload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::PingResponse resp;
        QVERIFY(resp.ParseFromArray(respPayload.constData(), respPayload.size()));
        QCOMPARE(resp.timestamp(), int64_t(12345));

        // Should also emit pingReceived
        QCOMPARE(pingSpy.count(), 1);
        QCOMPARE(pingSpy[0][0].value<int64_t>(), int64_t(12345));
    }

    void testPongReceived() {
        oaa::ControlChannel ctrl;
        QSignalSpy pongSpy(&ctrl, &oaa::ControlChannel::pongReceived);

        oaa::proto::messages::PingResponse resp;
        resp.set_timestamp(99999);
        QByteArray payload(resp.ByteSizeLong(), '\0');
        resp.SerializeToArray(payload.data(), payload.size());

        ctrl.onMessage(0x000c, payload);

        QCOMPARE(pongSpy.count(), 1);
        QCOMPARE(pongSpy[0][0].value<int64_t>(), int64_t(99999));
    }

    void testChannelOpenRequest() {
        oaa::ControlChannel ctrl;
        QSignalSpy openSpy(&ctrl, &oaa::ControlChannel::channelOpenRequested);

        oaa::proto::messages::ChannelOpenRequest req;
        req.set_priority(1);
        req.set_channel_id(3);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        ctrl.onMessage(0x0007, payload);

        QCOMPARE(openSpy.count(), 1);
        QCOMPARE(openSpy[0][0].value<uint8_t>(), uint8_t(3));
    }

    void testSendChannelOpenResponseOK() {
        oaa::ControlChannel ctrl;
        QSignalSpy sendSpy(&ctrl, &oaa::IChannelHandler::sendRequested);

        ctrl.sendChannelOpenResponse(3, true);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(), uint16_t(0x0008));

        QByteArray respPayload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::ChannelOpenResponse resp;
        QVERIFY(resp.ParseFromArray(respPayload.constData(), respPayload.size()));
        QCOMPARE(resp.status(), oaa::proto::enums::Status::OK);
    }

    void testSendChannelOpenResponseFail() {
        oaa::ControlChannel ctrl;
        QSignalSpy sendSpy(&ctrl, &oaa::IChannelHandler::sendRequested);

        ctrl.sendChannelOpenResponse(9, false);

        QCOMPARE(sendSpy.count(), 1);
        QByteArray respPayload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::ChannelOpenResponse resp;
        QVERIFY(resp.ParseFromArray(respPayload.constData(), respPayload.size()));
        QCOMPARE(resp.status(), oaa::proto::enums::Status::FAIL);
    }

    void testShutdownRequest() {
        oaa::ControlChannel ctrl;
        QSignalSpy shutdownSpy(&ctrl, &oaa::ControlChannel::shutdownRequested);

        oaa::proto::messages::ShutdownRequest req;
        req.set_reason(oaa::proto::enums::ShutdownReason::QUIT);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        ctrl.onMessage(0x000f, payload);

        QCOMPARE(shutdownSpy.count(), 1);
        QCOMPARE(shutdownSpy[0][0].toInt(), int(oaa::proto::enums::ShutdownReason::QUIT));
    }

    void testShutdownAcknowledged() {
        oaa::ControlChannel ctrl;
        QSignalSpy ackSpy(&ctrl, &oaa::ControlChannel::shutdownAcknowledged);

        ctrl.onMessage(0x0010, QByteArray());
        QCOMPARE(ackSpy.count(), 1);
    }

    void testServiceDiscoveryRequest() {
        oaa::ControlChannel ctrl;
        QSignalSpy sdSpy(&ctrl, &oaa::ControlChannel::serviceDiscoveryRequested);

        QByteArray payload("some-protobuf-data");
        ctrl.onMessage(0x0005, payload);

        QCOMPARE(sdSpy.count(), 1);
        QCOMPARE(sdSpy[0][0].toByteArray(), payload);
    }

    void testUnknownMessage() {
        oaa::ControlChannel ctrl;
        QSignalSpy unknownSpy(&ctrl, &oaa::IChannelHandler::unknownMessage);

        QByteArray payload("mystery");
        ctrl.onMessage(0x9999, payload);

        QCOMPARE(unknownSpy.count(), 1);
        QCOMPARE(unknownSpy[0][0].value<uint16_t>(), uint16_t(0x9999));
        QCOMPARE(unknownSpy[0][1].toByteArray(), payload);
    }

    void testSendAuthComplete() {
        oaa::ControlChannel ctrl;
        QSignalSpy sendSpy(&ctrl, &oaa::IChannelHandler::sendRequested);

        ctrl.sendAuthComplete(true);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), uint8_t(0));
        QCOMPARE(sendSpy[0][1].value<uint16_t>(), uint16_t(0x0004));
    }

    void testSendShutdownRequest() {
        oaa::ControlChannel ctrl;
        QSignalSpy sendSpy(&ctrl, &oaa::IChannelHandler::sendRequested);

        ctrl.sendShutdownRequest(1); // QUIT

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(), uint16_t(0x000f));

        QByteArray respPayload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::ShutdownRequest req;
        QVERIFY(req.ParseFromArray(respPayload.constData(), respPayload.size()));
        QCOMPARE(req.reason(), oaa::proto::enums::ShutdownReason::QUIT);
    }

    void testAudioFocusRequested() {
        oaa::ControlChannel ctrl;
        QSignalSpy afSpy(&ctrl, &oaa::ControlChannel::audioFocusRequested);

        QByteArray payload("audio-focus-data");
        ctrl.onMessage(0x0012, payload);

        QCOMPARE(afSpy.count(), 1);
        QCOMPARE(afSpy[0][0].toByteArray(), payload);
    }

    void testVoiceSessionRequested() {
        oaa::ControlChannel ctrl;
        QSignalSpy vsSpy(&ctrl, &oaa::ControlChannel::voiceSessionRequested);

        QByteArray payload("voice-session-data");
        ctrl.onMessage(0x0011, payload);

        QCOMPARE(vsSpy.count(), 1);
        QCOMPARE(vsSpy[0][0].toByteArray(), payload);
    }
};

QTEST_MAIN(TestControlChannel)
#include "test_control_channel.moc"
