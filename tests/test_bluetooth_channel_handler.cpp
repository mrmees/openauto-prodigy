#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/BluetoothChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "BluetoothPairingRequestMessage.pb.h"
#include "BluetoothPairingMethodEnum.pb.h"
#include "BluetoothPairingResponseMessage.pb.h"

class TestBluetoothChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oaa::hu::BluetoothChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Bluetooth);
    }

    void testPairingRequestEmitsSignalAndResponds() {
        oaa::hu::BluetoothChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        QSignalSpy pairSpy(&handler, &oaa::hu::BluetoothChannelHandler::pairingRequested);

        handler.onChannelOpened();

        oaa::proto::messages::BluetoothPairingRequest req;
        req.set_phone_address("8C:C5:D0:DD:74:15");
        req.set_pairing_method(oaa::proto::enums::BluetoothPairingMethod::HFP);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::BluetoothMessageId::PAIRING_REQUEST, payload);

        // Should emit pairingRequested signal
        QCOMPARE(pairSpy.count(), 1);
        QCOMPARE(pairSpy[0][0].toString(), QString("8C:C5:D0:DD:74:15"));

        // Should send PairingResponse
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::Bluetooth);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::BluetoothMessageId::PAIRING_RESPONSE));

        // Verify response content
        QByteArray respPayload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::BluetoothPairingResponse resp;
        QVERIFY(resp.ParseFromArray(respPayload.constData(), respPayload.size()));
        QVERIFY(resp.already_paired());
    }
};

QTEST_MAIN(TestBluetoothChannelHandler)
#include "test_bluetooth_channel_handler.moc"
