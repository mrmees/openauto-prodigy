#include <QTest>
#include <QSignalSpy>
#include "core/aa/handlers/WiFiChannelHandler.hpp"
#include <oaa/Channel/ChannelId.hpp>
#include "WifiSecurityResponseMessage.pb.h"

class TestWiFiChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oap::aa::WiFiChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::WiFi);
    }

    void testSecurityRequestSendsCredentials() {
        oap::aa::WiFiChannelHandler handler("OpenAutoProdigy", "secretpass123");
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();
        handler.onMessage(oaa::WiFiMessageId::CREDENTIALS_REQUEST, QByteArray());

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::WiFi);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::WiFiMessageId::CREDENTIALS_RESPONSE));

        QByteArray respData = sendSpy[0][2].toByteArray();
        oaa::proto::messages::WifiSecurityResponse resp;
        QVERIFY(resp.ParseFromArray(respData.constData(), respData.size()));
        QCOMPARE(QString::fromStdString(resp.ssid()), QString("OpenAutoProdigy"));
        QCOMPARE(QString::fromStdString(resp.key()), QString("secretpass123"));
        QCOMPARE(resp.security_mode(),
                 oaa::proto::messages::WifiSecurityResponse::WPA2_PERSONAL);
    }

    void testChannelIdValue() {
        // WiFi channel is 14 on the wire
        QCOMPARE(oaa::ChannelId::WiFi, static_cast<uint8_t>(14));
    }
};

QTEST_MAIN(TestWiFiChannelHandler)
#include "test_wifi_channel_handler.moc"
