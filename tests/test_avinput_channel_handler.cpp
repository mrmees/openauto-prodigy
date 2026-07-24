#include <QTest>
#include <QSignalSpy>
#include <QtEndian>
#include <oaa/HU/Handlers/AVInputChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/av/AVInputOpenRequestMessage.pb.h"
#include "oaa/av/AVInputOpenResponseMessage.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"

namespace {

QByteArray serialize(const google::protobuf::MessageLite& message)
{
    QByteArray payload(static_cast<qsizetype>(message.ByteSizeLong()), '\0');
    message.SerializeToArray(payload.data(), payload.size());
    return payload;
}

oaa::proto::messages::AVInputOpenResponse responseAt(const QSignalSpy& spy, int index)
{
    oaa::proto::messages::AVInputOpenResponse response;
    const QByteArray payload = spy.at(index).at(2).toByteArray();
    response.ParseFromArray(payload.constData(), payload.size());
    return response;
}

} // namespace

class TestAVInputChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oaa::hu::AVInputChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::AVInput);
    }

    void testInputOpenRequestStartsCapture() {
        oaa::hu::AVInputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        QSignalSpy captureSpy(&handler, &oaa::hu::AVInputChannelHandler::micCaptureRequested);
        bool controllerCalled = false;
        handler.setCaptureController([&controllerCalled](bool open) {
            controllerCalled = open;
            return true;
        });

        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest req;
        req.set_open(true);
        req.set_anc(false);
        req.set_ec(true);
        req.set_max_unacked(3);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(req));

        // Should send INPUT_OPEN_RESPONSE
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::INPUT_OPEN_RESPONSE));
        QCOMPARE(responseAt(sendSpy, 0).value(), 0);
        QVERIFY(controllerCalled);

        // Should emit micCaptureRequested(true)
        QCOMPARE(captureSpy.count(), 1);
        QCOMPARE(captureSpy[0][0].toBool(), true);
    }

    void testInputRequestBeforeChannelOpenIsIgnored() {
        oaa::hu::AVInputChannelHandler handler;
        int controllerCalls = 0;
        handler.setCaptureController([&controllerCalls](bool) {
            ++controllerCalls;
            return true;
        });
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));

        QCOMPARE(controllerCalls, 0);
        QCOMPARE(sendSpy.count(), 0);
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 1));
    }

    void testDuplicateChannelOpenStopsExistingCapture() {
        oaa::hu::AVInputChannelHandler handler;
        QList<bool> controllerCalls;
        handler.setCaptureController([&controllerCalls](bool open) {
            controllerCalls.append(open);
            return true;
        });
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));
        handler.onChannelOpened();

        QCOMPARE(controllerCalls, QList<bool>({true, false}));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 1));
    }

    void testInputOpenRequestStopsCapture() {
        oaa::hu::AVInputChannelHandler handler;
        QList<bool> controllerCalls;
        handler.setCaptureController([&controllerCalls](bool open) {
            controllerCalls.append(open);
            return true;
        });
        handler.onChannelOpened();

        // First open
        oaa::proto::messages::AVInputOpenRequest openReq;
        openReq.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(openReq));

        QSignalSpy captureSpy(&handler, &oaa::hu::AVInputChannelHandler::micCaptureRequested);

        // Then close
        oaa::proto::messages::AVInputOpenRequest closeReq;
        closeReq.set_open(false);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(closeReq));

        QCOMPARE(captureSpy.count(), 1);
        QCOMPARE(captureSpy[0][0].toBool(), false);
        QCOMPARE(controllerCalls, QList<bool>({true, false}));
    }

    void testOpenFailsClosedWithoutCaptureController() {
        oaa::hu::AVInputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        QSignalSpy captureSpy(&handler, &oaa::hu::AVInputChannelHandler::micCaptureRequested);
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(responseAt(sendSpy, 0).value(), 1);
        QCOMPARE(captureSpy.count(), 0);
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 1));
    }

    void testControllerFailurePrecedesFailureResponse() {
        oaa::hu::AVInputChannelHandler handler;
        QStringList order;
        handler.setCaptureController([&order](bool) {
            order.append(QStringLiteral("controller"));
            return false;
        });
        connect(&handler, &oaa::IChannelHandler::sendRequested, &handler,
                [&order](uint8_t, uint16_t, const QByteArray&) {
                    order.append(QStringLiteral("response"));
                });
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));

        QCOMPARE(order, QStringList({QStringLiteral("controller"),
                                     QStringLiteral("response")}));
    }

    void testRepeatedOpenStartsFreshCaptureGeneration() {
        oaa::hu::AVInputChannelHandler handler;
        QList<bool> calls;
        handler.setCaptureController([&calls](bool open) {
            calls.append(open);
            return true;
        });
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));

        QCOMPARE(calls, QList<bool>({true, false, true}));
    }

    void testCloseWhileInactiveIsIdempotentSuccess() {
        oaa::hu::AVInputChannelHandler handler;
        int controllerCalls = 0;
        handler.setCaptureController([&controllerCalls](bool) {
            ++controllerCalls;
            return true;
        });
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(false);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));

        QCOMPARE(controllerCalls, 0);
        QCOMPARE(responseAt(sendSpy, 0).value(), 0);
    }

    void testSendMicData() {
        oaa::hu::AVInputChannelHandler handler;
        handler.setCaptureController([](bool) { return true; });
        handler.onChannelOpened();

        // Open mic
        oaa::proto::messages::AVInputOpenRequest req;
        req.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(req));

        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        QByteArray micData(320, '\x42'); // 320 bytes of mic PCM
        uint64_t timestamp = 1234567890;
        QVERIFY(handler.sendMicData(micData, timestamp));

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::AVInput);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::AV_MEDIA_WITH_TIMESTAMP));

        // Verify timestamp is prepended as 8-byte big-endian
        QByteArray sentPayload = sendSpy[0][2].toByteArray();
        QCOMPARE(sentPayload.size(), 8 + 320);
        uint64_t tsBE;
        memcpy(&tsBE, sentPayload.constData(), 8);
        QCOMPARE(qFromBigEndian(tsBE), timestamp);
        // Verify audio data follows the timestamp
        QCOMPARE(sentPayload.mid(8), micData);
    }

    void testMicDataIgnoredWhenNotCapturing() {
        oaa::hu::AVInputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();
        // No INPUT_OPEN_REQUEST — mic data should be ignored
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 0));
        QCOMPARE(sendSpy.count(), 0);
    }

    void testAbortCaptureDisablesSendingWithoutControllerOrResponse() {
        oaa::hu::AVInputChannelHandler handler;
        QList<bool> controllerCalls;
        handler.setCaptureController([&controllerCalls](bool open) {
            controllerCalls.append(open);
            return true;
        });
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        QSignalSpy captureSpy(&handler, &oaa::hu::AVInputChannelHandler::micCaptureRequested);

        handler.abortCapture();
        QCOMPARE(controllerCalls, QList<bool>({true}));
        QCOMPARE(sendSpy.count(), 0);
        QCOMPARE(captureSpy.count(), 1);
        QCOMPARE(captureSpy[0][0].toBool(), false);
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 1));

        handler.abortCapture();
        QCOMPARE(captureSpy.count(), 1);
    }

    void testTransmitWindowBlocksUntilValidAck() {
        oaa::hu::AVInputChannelHandler handler;
        handler.setCaptureController([](bool) { return true; });
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        request.set_max_unacked(2);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));

        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        QSignalSpy windowSpy(&handler, &oaa::hu::AVInputChannelHandler::sendWindowAvailable);
        QVERIFY(handler.sendMicData(QByteArray(320, '\x01'), 1));
        QVERIFY(handler.sendMicData(QByteArray(320, '\x02'), 2));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x03'), 3));
        QCOMPARE(sendSpy.count(), 2);

        oaa::proto::messages::AVMediaAckIndication stale;
        stale.set_session_id(1);
        stale.set_ack_count(1);
        handler.onMessage(oaa::AVMessageId::ACK_INDICATION, serialize(stale));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x04'), 4));

        oaa::proto::messages::AVMediaAckIndication ack;
        ack.set_session_id(0);
        ack.set_ack_count(1);
        handler.onMessage(oaa::AVMessageId::ACK_INDICATION, serialize(ack));
        QCOMPARE(windowSpy.count(), 1);
        QVERIFY(handler.sendMicData(QByteArray(320, '\x05'), 5));
    }

    void testAckTimestampsTakePrecedenceAndOvercountIsBounded() {
        oaa::hu::AVInputChannelHandler handler;
        handler.setCaptureController([](bool) { return true; });
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        request.set_max_unacked(2);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));
        QVERIFY(handler.sendMicData(QByteArray(320, '\x01'), 1));
        QVERIFY(handler.sendMicData(QByteArray(320, '\x02'), 2));

        oaa::proto::messages::AVMediaAckIndication oneTimestamp;
        oneTimestamp.set_session_id(0);
        oneTimestamp.set_ack_count(99);
        oneTimestamp.add_receive_timestamps(100);
        handler.onMessage(oaa::AVMessageId::ACK_INDICATION, serialize(oneTimestamp));
        QVERIFY(handler.sendMicData(QByteArray(320, '\x03'), 3));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x04'), 4));

        oaa::proto::messages::AVMediaAckIndication overcount;
        overcount.set_session_id(0);
        overcount.set_ack_count(99);
        handler.onMessage(oaa::AVMessageId::ACK_INDICATION, serialize(overcount));
        QVERIFY(handler.sendMicData(QByteArray(320, '\x05'), 5));
        QVERIFY(handler.sendMicData(QByteArray(320, '\x06'), 6));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x07'), 7));
    }

    void testMalformedAndZeroAckDoNotReleaseWindow() {
        oaa::hu::AVInputChannelHandler handler;
        handler.setCaptureController([](bool) { return true; });
        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest request;
        request.set_open(true);
        request.set_max_unacked(1);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));
        QVERIFY(handler.sendMicData(QByteArray(320, '\x01'), 1));

        handler.onMessage(oaa::AVMessageId::ACK_INDICATION,
                          QByteArray(1, static_cast<char>(0x80)));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x02'), 2));

        oaa::proto::messages::AVMediaAckIndication zero;
        zero.set_session_id(0);
        zero.set_ack_count(0);
        handler.onMessage(oaa::AVMessageId::ACK_INDICATION, serialize(zero));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x03'), 3));
    }

    void testMissingAndNonPositiveWindowFallBackToOne() {
        for (const auto value : {QVariant(), QVariant(0), QVariant(-4)}) {
            oaa::hu::AVInputChannelHandler handler;
            handler.setCaptureController([](bool) { return true; });
            handler.onChannelOpened();

            oaa::proto::messages::AVInputOpenRequest request;
            request.set_open(true);
            if (value.isValid())
                request.set_max_unacked(value.toInt());
            handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(request));

            QVERIFY(handler.sendMicData(QByteArray(320, '\x01'), 1));
            QVERIFY(!handler.sendMicData(QByteArray(320, '\x02'), 2));
        }
    }

    void testChannelCloseStopsCapture() {
        oaa::hu::AVInputChannelHandler handler;
        QList<bool> controllerCalls;
        handler.setCaptureController([&controllerCalls](bool open) {
            controllerCalls.append(open);
            return true;
        });
        handler.onChannelOpened();

        // Open mic
        oaa::proto::messages::AVInputOpenRequest req;
        req.set_open(true);
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, serialize(req));

        QSignalSpy captureSpy(&handler, &oaa::hu::AVInputChannelHandler::micCaptureRequested);
        handler.onChannelClosed();

        QCOMPARE(captureSpy.count(), 1);
        QCOMPARE(captureSpy[0][0].toBool(), false);
        QCOMPARE(controllerCalls, QList<bool>({true, false}));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 2));
    }
};

QTEST_MAIN(TestAVInputChannelHandler)
#include "test_avinput_channel_handler.moc"
