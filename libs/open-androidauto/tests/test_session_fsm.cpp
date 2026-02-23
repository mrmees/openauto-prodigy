#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QtEndian>
#include <oaa/Transport/ReplayTransport.hpp>
#include <oaa/Session/AASession.hpp>
#include <oaa/Channel/IChannelHandler.hpp>

#include "ChannelOpenRequestMessage.pb.h"
#include "ChannelOpenResponseMessage.pb.h"
#include "ShutdownRequestMessage.pb.h"
#include "ShutdownResponseMessage.pb.h"
#include "ShutdownReasonEnum.pb.h"
#include "StatusEnum.pb.h"
#include "ServiceDiscoveryRequestMessage.pb.h"
#include "ServiceDiscoveryResponseMessage.pb.h"

// Minimal mock channel handler for testing
class MockChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit MockChannelHandler(uint8_t id, QObject* parent = nullptr)
        : IChannelHandler(parent), id_(id) {}

    uint8_t channelId() const override { return id_; }
    void onChannelOpened() override { opened = true; }
    void onChannelClosed() override { closed = true; }
    void onMessage(uint16_t messageId, const QByteArray& payload) override {
        lastMessageId = messageId;
        lastPayload = payload;
        messageCount++;
    }

    bool opened = false;
    bool closed = false;
    uint16_t lastMessageId = 0;
    QByteArray lastPayload;
    int messageCount = 0;

private:
    uint8_t id_;
};

class TestSessionFSM : public QObject {
    Q_OBJECT

private:
    // Helper: build a VERSION_RESPONSE frame (plain, control, bulk)
    // The messenger expects raw transport data: header(2) + size(2) + messageId(2) + payload
    QByteArray makeVersionResponseFrame(uint16_t major, uint16_t minor, uint16_t status) {
        // Payload = messageId(0x0002) + major(2B) + minor(2B) + status(2B)
        QByteArray payload(8, '\0');
        qToBigEndian<uint16_t>(0x0002, reinterpret_cast<uchar*>(payload.data()));     // messageId
        qToBigEndian<uint16_t>(major, reinterpret_cast<uchar*>(payload.data() + 2));
        qToBigEndian<uint16_t>(minor, reinterpret_cast<uchar*>(payload.data() + 4));
        qToBigEndian<uint16_t>(status, reinterpret_cast<uchar*>(payload.data() + 6));

        // Frame: ch=0, flags=BULK|CONTROL|PLAIN=0x07, size=8
        QByteArray frame(4, '\0');
        frame[0] = 0x00; // channel 0
        frame[1] = 0x07; // BULK(0x03) | CONTROL(0x04) | PLAIN(0x00)
        qToBigEndian<uint16_t>(8, reinterpret_cast<uchar*>(frame.data() + 2));
        frame.append(payload);
        return frame;
    }

private slots:
    void testInitialState() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);

        QCOMPARE(session.state(), oaa::SessionState::Idle);
    }

    void testStartWhenAlreadyConnected() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy stateSpy(&session, &oaa::AASession::stateChanged);

        transport.simulateConnect();
        session.start();

        // Should go straight to VersionExchange (skipping Connecting)
        QCOMPARE(session.state(), oaa::SessionState::VersionExchange);

        // Should have sent VERSION_REQUEST
        QVERIFY(!transport.writtenData().isEmpty());
    }

    void testStartWhenNotConnected() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);

        session.start();
        QCOMPARE(session.state(), oaa::SessionState::Connecting);

        // Simulate connection
        transport.simulateConnect();

        // Should advance to VersionExchange
        QTRY_COMPARE(session.state(), oaa::SessionState::VersionExchange);
        QVERIFY(!transport.writtenData().isEmpty());
    }

    void testVersionMismatchDisconnects() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        QCOMPARE(session.state(), oaa::SessionState::VersionExchange);

        // Feed VERSION_RESPONSE with MISMATCH
        transport.feedData(makeVersionResponseFrame(1, 7, 0xFFFF));

        QTRY_COMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::VersionMismatch);
    }

    void testVersionMatchAdvancesToHandshake() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);

        transport.simulateConnect();
        session.start();
        QCOMPARE(session.state(), oaa::SessionState::VersionExchange);

        // Feed VERSION_RESPONSE with MATCH
        transport.feedData(makeVersionResponseFrame(1, 7, 0x0000));

        QTRY_COMPARE(session.state(), oaa::SessionState::TLSHandshake);
    }

    void testVersionTimeoutDisconnects() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        config.versionTimeout = 100; // 100ms for testing
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        QCOMPARE(session.state(), oaa::SessionState::VersionExchange);

        // Don't send any version response — wait for timeout
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), oaa::SessionState::Disconnected, 500);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::Timeout);
    }

    void testChannelRegistration() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);

        MockChannelHandler videoHandler(3);
        MockChannelHandler audioHandler(4);

        session.registerChannel(3, &videoHandler);
        session.registerChannel(4, &audioHandler);

        // Verify we can retrieve messenger
        QVERIFY(session.messenger() != nullptr);
        QVERIFY(session.controlChannel() != nullptr);
    }

    void testStopFromIdleIsNoop() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);

        session.stop(); // Should not crash or change state
        QCOMPARE(session.state(), oaa::SessionState::Idle);
    }

    void testStopFromConnectingDisconnects() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        session.start(); // Connecting state
        QCOMPARE(session.state(), oaa::SessionState::Connecting);

        session.stop();
        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::UserRequested);
    }

    void testTransportDisconnectDuringVersionExchange() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        QCOMPARE(session.state(), oaa::SessionState::VersionExchange);

        transport.simulateDisconnect();

        QTRY_COMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::TransportError);
    }
};

QTEST_MAIN(TestSessionFSM)
#include "test_session_fsm.moc"
