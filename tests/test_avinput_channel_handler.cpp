#include <QTest>
#include <QSignalSpy>
#include <QtEndian>
#include <oaa/HU/Handlers/AVInputChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/av/AVInputOpenRequestMessage.pb.h"
#include "oaa/av/AVInputOpenResponseMessage.pb.h"

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

        handler.onChannelOpened();

        oaa::proto::messages::AVInputOpenRequest req;
        req.set_open(true);
        req.set_anc(false);
        req.set_ec(true);
        req.set_max_unacked(3);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);

        // Should send INPUT_OPEN_RESPONSE
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::INPUT_OPEN_RESPONSE));

        // Should emit micCaptureRequested(true)
        QCOMPARE(captureSpy.count(), 1);
        QCOMPARE(captureSpy[0][0].toBool(), true);
    }

    void testInputOpenRequestStopsCapture() {
        oaa::hu::AVInputChannelHandler handler;
        handler.onChannelOpened();

        // First open
        oaa::proto::messages::AVInputOpenRequest openReq;
        openReq.set_open(true);
        QByteArray openPayload(openReq.ByteSizeLong(), '\0');
        openReq.SerializeToArray(openPayload.data(), openPayload.size());
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, openPayload);

        QSignalSpy captureSpy(&handler, &oaa::hu::AVInputChannelHandler::micCaptureRequested);

        // Then close
        oaa::proto::messages::AVInputOpenRequest closeReq;
        closeReq.set_open(false);
        QByteArray closePayload(closeReq.ByteSizeLong(), '\0');
        closeReq.SerializeToArray(closePayload.data(), closePayload.size());
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, closePayload);

        QCOMPARE(captureSpy.count(), 1);
        QCOMPARE(captureSpy[0][0].toBool(), false);
    }

    void testSendMicData() {
        oaa::hu::AVInputChannelHandler handler;
        handler.onChannelOpened();

        // Open mic
        oaa::proto::messages::AVInputOpenRequest req;
        req.set_open(true);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);

        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        QByteArray micData(320, '\x42'); // 320 bytes of mic PCM
        uint64_t timestamp = 1234567890;
        handler.sendMicData(micData, timestamp);

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
        handler.sendMicData(QByteArray(320, '\x42'), 0);
        QCOMPARE(sendSpy.count(), 0);
    }

    void testChannelCloseStopsCapture() {
        oaa::hu::AVInputChannelHandler handler;
        handler.onChannelOpened();

        // Open mic
        oaa::proto::messages::AVInputOpenRequest req;
        req.set_open(true);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);

        QSignalSpy captureSpy(&handler, &oaa::hu::AVInputChannelHandler::micCaptureRequested);
        handler.onChannelClosed();

        QCOMPARE(captureSpy.count(), 1);
        QCOMPARE(captureSpy[0][0].toBool(), false);
    }
};

QTEST_MAIN(TestAVInputChannelHandler)
#include "test_avinput_channel_handler.moc"
