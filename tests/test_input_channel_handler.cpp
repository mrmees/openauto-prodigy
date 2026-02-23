#include <QTest>
#include <QSignalSpy>
#include "core/aa/handlers/InputChannelHandler.hpp"
#include <oaa/Channel/ChannelId.hpp>
#include "BindingRequestMessage.pb.h"
#include "InputEventIndicationMessage.pb.h"

class TestInputChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oap::aa::InputChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Input);
    }

    void testSendTouchEvent() {
        oap::aa::InputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        oap::aa::InputChannelHandler::Pointer pt{640, 360, 0};
        handler.sendTouchIndication(1, &pt, 0, 0, 12345); // action=PRESS(0)

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Input);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::InputMessageId::INPUT_EVENT_INDICATION));

        // Verify the serialized payload contains correct touch data
        QByteArray payload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::InputEventIndication indication;
        QVERIFY(indication.ParseFromArray(payload.constData(), payload.size()));
        QCOMPARE(indication.timestamp(), static_cast<uint64_t>(12345));
        QVERIFY(indication.has_touch_event());
        QCOMPARE(indication.touch_event().touch_location_size(), 1);
        QCOMPARE(indication.touch_event().touch_location(0).x(), 640u);
        QCOMPARE(indication.touch_event().touch_location(0).y(), 360u);
    }

    void testMultiTouchEvent() {
        oap::aa::InputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        oap::aa::InputChannelHandler::Pointer pts[2] = {
            {100, 200, 0},
            {300, 400, 1}
        };
        handler.sendTouchIndication(2, pts, 1, 5, 99999); // POINTER_DOWN on second finger

        QCOMPARE(sendSpy.count(), 1);
        QByteArray payload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::InputEventIndication indication;
        QVERIFY(indication.ParseFromArray(payload.constData(), payload.size()));
        QCOMPARE(indication.touch_event().touch_location_size(), 2);
        QCOMPARE(indication.touch_event().action_index(), 1u);
        QCOMPARE(indication.touch_event().touch_action(), 5); // POINTER_DOWN
    }

    void testBindingRequestResponds() {
        oap::aa::InputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        oaa::proto::messages::BindingRequest req;
        req.add_scan_codes(3);  // HOME
        req.add_scan_codes(4);  // BACK
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::InputMessageId::BINDING_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::InputMessageId::BINDING_RESPONSE));
    }

    void testTouchNotSentWhenClosed() {
        oap::aa::InputChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        // Channel not opened
        oap::aa::InputChannelHandler::Pointer pt{640, 360, 0};
        handler.sendTouchIndication(1, &pt, 0, 0, 12345);
        QCOMPARE(sendSpy.count(), 0);
    }
};

QTEST_MAIN(TestInputChannelHandler)
#include "test_input_channel_handler.moc"
