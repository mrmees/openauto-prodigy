#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QtEndian>
#include <oaa/Transport/ReplayTransport.hpp>
#include <oaa/Session/AASession.hpp>
#include <oaa/Channel/IChannelHandler.hpp>

#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/control/ChannelOpenResponseMessage.pb.h"
#include "oaa/control/ShutdownRequestMessage.pb.h"
#include "oaa/control/ShutdownResponseMessage.pb.h"
#include "oaa/control/ShutdownReasonEnum.pb.h"
#include "oaa/common/StatusEnum.pb.h"
#include "oaa/control/ServiceDiscoveryRequestMessage.pb.h"
#include "oaa/control/ServiceDiscoveryResponseMessage.pb.h"

// Minimal mock channel handler for testing
class MockChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit MockChannelHandler(uint8_t id, QObject* parent = nullptr)
        : IChannelHandler(parent), id_(id) {}

    uint8_t channelId() const override { return id_; }
    void onChannelOpened() override { opened = true; openCount++; }
    void onChannelClosed() override { closed = true; closeCount++; }
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override {
        lastMessageId = messageId;
        lastPayload = payload.mid(dataOffset);
        messageCount++;
    }

    bool opened = false;
    bool closed = false;
    int openCount = 0;
    int closeCount = 0;
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

    void advanceToActive(oaa::AASession& session) {
        QByteArray versionResponse(6, '\0');
        qToBigEndian<uint16_t>(1, reinterpret_cast<uchar*>(versionResponse.data()));
        qToBigEndian<uint16_t>(7, reinterpret_cast<uchar*>(versionResponse.data() + 2));
        qToBigEndian<uint16_t>(0, reinterpret_cast<uchar*>(versionResponse.data() + 4));
        session.messenger()->messageReceived(0, 0x0002, versionResponse, 0);
        QCOMPARE(session.state(), oaa::SessionState::TLSHandshake);
        session.messenger()->handshakeComplete();
        QCOMPARE(session.state(), oaa::SessionState::ServiceDiscovery);
        session.messenger()->messageReceived(0, 0x0005, QByteArray(), 0);
        QCOMPARE(session.state(), oaa::SessionState::Active);
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

    void testDestructorDoesNotWriteOrCallExternalHandlers() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        MockChannelHandler handler(3);
        auto* session = new oaa::AASession(&transport, config);
        session->registerChannel(3, &handler);

        transport.simulateConnect();
        session->start();
        advanceToActive(*session);
        transport.clearWritten();

        delete session;

        QCOMPARE(transport.writtenData().size(), 0);
        QCOMPARE(handler.closeCount, 0);
    }

    void testFinalizeClosesOnceWithoutProtocolWrite() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        MockChannelHandler handler(3);
        auto* session = new oaa::AASession(&transport, config);
        session->registerChannel(3, &handler);

        transport.simulateConnect();
        session->start();
        advanceToActive(*session);
        transport.clearWritten();

        session->finalize();
        QCOMPARE(session->state(), oaa::SessionState::Disconnected);
        QCOMPARE(handler.closeCount, 1);
        QCOMPARE(transport.writtenData().size(), 0);

        session->finalize();
        delete session;
        QCOMPARE(handler.closeCount, 1);
    }

    void testRestartReconnectsHandlerSendPathForOneCycle() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        MockChannelHandler handler(3);
        oaa::AASession session(&transport, config);
        session.registerChannel(3, &handler);

        session.start();
        session.stop();
        QCOMPARE(handler.closeCount, 1);
        transport.clearWritten();

        emit handler.sendRequested(3, 0x1234, QByteArray("stale"));
        QCOMPARE(transport.writtenData().size(), 0);

        session.start();
        emit handler.sendRequested(3, 0x1234, QByteArray("fresh"));
        QCOMPARE(transport.writtenData().size(), 1);

        session.stop();
        QCOMPARE(handler.closeCount, 2);
    }

    void testFinalizedSessionCannotReceiveReplacementHandlerSends() {
        oaa::ReplayTransport oldTransport;
        oaa::ReplayTransport replacementTransport;
        oaa::SessionConfig config;
        MockChannelHandler handler(3);
        oaa::AASession oldSession(&oldTransport, config);
        oaa::AASession replacementSession(&replacementTransport, config);

        oldSession.registerChannel(3, &handler);
        oldSession.start();
        oldSession.finalize();
        oldTransport.clearWritten();

        replacementSession.registerChannel(3, &handler);
        replacementSession.start();
        emit handler.sendRequested(3, 0x1234, QByteArray("replacement"));

        QCOMPARE(oldTransport.writtenData().size(), 0);
        QCOMPARE(replacementTransport.writtenData().size(), 1);
    }

    void testFatalHandshakeDisconnectsImmediately() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        config.handshakeTimeout = 1000;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        transport.feedData(makeVersionResponseFrame(1, 7, 0x0000));
        QCOMPARE(session.state(), oaa::SessionState::TLSHandshake);

        QByteArray payload;
        const uint16_t handshakeId = qToBigEndian(uint16_t(0x0003));
        payload.append(reinterpret_cast<const char*>(&handshakeId), 2);
        payload.append(QByteArray(64, 'X'));
        QByteArray frame(4, '\0');
        frame[0] = 0x00;
        frame[1] = 0x03;
        qToBigEndian<uint16_t>(static_cast<uint16_t>(payload.size()),
                               reinterpret_cast<uchar*>(frame.data() + 2));
        frame.append(payload);
        transport.feedData(frame);

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::HandshakeError);
        QTest::qWait(20);
        QCOMPARE(disconnectSpy.count(), 1);
    }

    void testChannelOpenResponseUsesTargetChannelForBothDispatchPaths() {
        for (const uint8_t incomingChannel : {uint8_t(0), uint8_t(3)}) {
            oaa::ReplayTransport transport;
            oaa::SessionConfig config;
            oaa::AASession session(&transport, config);
            MockChannelHandler handler(3);
            session.registerChannel(3, &handler);

            transport.simulateConnect();
            session.start();
            advanceToActive(session);
            transport.clearWritten();

            oaa::proto::messages::ChannelOpenRequest request;
            request.set_channel_id(3);
            request.set_priority(1);
            QByteArray payload(request.ByteSizeLong(), '\0');
            QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
            session.messenger()->messageReceived(
                incomingChannel, 0x0007, payload, 0);

            QCOMPARE(handler.openCount, 1);
            QCOMPARE(transport.writtenData().size(), 1);
            const QByteArray response = transport.writtenData().first();
            QCOMPARE(static_cast<uint8_t>(response[0]), uint8_t(3));
            QCOMPARE(static_cast<uint8_t>(response[1]), uint8_t(0x07));
        }
    }
};

QTEST_MAIN(TestSessionFSM)
#include "test_session_fsm.moc"
