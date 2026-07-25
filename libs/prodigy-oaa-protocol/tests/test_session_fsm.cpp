#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QtEndian>
#include <oaa/Transport/ReplayTransport.hpp>
#include <oaa/Session/AASession.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <oaa/Channel/IChannelHandler.hpp>

#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/control/ChannelOpenResponseMessage.pb.h"
#include "oaa/control/ShutdownRequestMessage.pb.h"
#include "oaa/control/ShutdownResponseMessage.pb.h"
#include "oaa/control/ShutdownReasonEnum.pb.h"
#include "oaa/common/StatusEnum.pb.h"
#include "oaa/control/ServiceDiscoveryRequestMessage.pb.h"
#include "oaa/control/ServiceDiscoveryResponseMessage.pb.h"
#include "oaa/control/PingRequestMessage.pb.h"
#include <functional>

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

class InitialHandshakeFailureSession : public oaa::AASession {
public:
    using AASession::AASession;

protected:
    void startTlsHandshake() override {
        emit messenger()->handshakeFailed(QStringLiteral("forced initial failure"));
    }
};

class ReentrantErrorTransport : public oaa::ReplayTransport {
public:
    using ReplayTransport::ReplayTransport;

    void write(const QByteArray& data) override {
        ReplayTransport::write(data);
        ++writeCount;
        if (failOnWrite == writeCount)
            emit error(QStringLiteral("forced synchronous write failure"));
    }

    int writeCount = 0;
    int failOnWrite = -1;
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

    int64_t pingTimestamp(const QList<QVariant>& emission) {
        oaa::proto::messages::PingRequest ping;
        const QByteArray payload = emission[2].toByteArray();
        if (!ping.ParseFromArray(payload.constData(), payload.size()))
            return -1;
        return ping.timestamp();
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

    void testRestartReconnectsHandlerOnlyAfterChannelOpen() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        MockChannelHandler handler(3);
        oaa::AASession session(&transport, config);
        session.registerChannel(3, &handler);

        session.start();
        emit handler.sendRequested(3, 0x1234, QByteArray("too-early"));
        QCOMPARE(transport.writtenData().size(), 0);
        session.stop();
        QCOMPARE(handler.closeCount, 1);
        transport.clearWritten();

        emit handler.sendRequested(3, 0x1234, QByteArray("stale"));
        QCOMPARE(transport.writtenData().size(), 0);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);
        oaa::proto::messages::ChannelOpenRequest request;
        request.set_channel_id(3);
        request.set_priority(1);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        session.messenger()->messageReceived(
            3, 0x0007, payload, 0, oaa::MessageType::Control);
        transport.clearWritten();
        emit handler.sendRequested(3, 0x1234, QByteArray("fresh"));
        QCOMPARE(transport.writtenData().size(), 1);

        session.stop();
        emit session.messenger()->messageReceived(0, 0x0010, QByteArray(), 0);
        QCOMPARE(handler.closeCount, 2);
    }

    void testConnectingNotificationCannotSendServiceTrafficBeforeVersion() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        MockChannelHandler handler(3);
        session.registerChannel(3, &handler);
        connect(&session, &oaa::AASession::stateChanged, &session,
                [&handler](oaa::SessionState state) {
                    if (state == oaa::SessionState::Connecting)
                        emit handler.sendRequested(3, 0x1234, QByteArray("early"));
                }, Qt::DirectConnection);

        transport.simulateConnect();
        session.start();

        QCOMPARE(session.state(), oaa::SessionState::VersionExchange);
        QCOMPARE(transport.writtenData().size(), 1);
        QCOMPARE(static_cast<uint8_t>(transport.writtenData().first()[0]),
                 uint8_t(0));
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
        replacementTransport.simulateConnect();
        replacementSession.start();
        advanceToActive(replacementSession);
        oaa::proto::messages::ChannelOpenRequest request;
        request.set_channel_id(3);
        request.set_priority(1);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        replacementSession.messenger()->messageReceived(
            3, 0x0007, payload, 0, oaa::MessageType::Control);
        replacementTransport.clearWritten();
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

    void testInitialSynchronousHandshakeFailureDisconnectsImmediately() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        config.handshakeTimeout = 25;
        InitialHandshakeFailureSession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        transport.feedData(makeVersionResponseFrame(1, 7, 0x0000));

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::HandshakeError);
        QTest::qWait(50);
        QCOMPARE(disconnectSpy.count(), 1);
    }

    void testEncryptedFrameDuringHandshakeFailsImmediately() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        transport.feedData(makeVersionResponseFrame(1, 7, 0x0000));
        QCOMPARE(session.state(), oaa::SessionState::TLSHandshake);

        QByteArray frame(4, '\0');
        frame[0] = 0;
        frame[1] = char(0x0b); // BULK | ENCRYPTED | SPECIFIC
        qToBigEndian<uint16_t>(1, reinterpret_cast<uchar*>(frame.data() + 2));
        frame.append('X');
        transport.feedData(frame);

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::HandshakeError);
    }

    void testPostHandshakeTlsFailureDisconnectsExactlyOnce() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);

        emit session.messenger()->tlsFailed(QStringLiteral("forced runtime failure"));
        emit session.messenger()->tlsFailed(QStringLiteral("duplicate"));

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::TlsError);
    }

    void testProtocolFailureDisconnectsExactlyOnce() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);

        emit session.messenger()->protocolFailed(
            QStringLiteral("forced assembly failure"));
        emit session.messenger()->protocolFailed(QStringLiteral("duplicate"));

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::ProtocolError);
    }

    void testPingCadenceUsesIndependentConfiguredDeadline() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        config.pingInterval = 25;
        config.pingTimeout = 180;
        oaa::AASession session(&transport, config);
        QSignalSpy pingSpy(session.controlChannel(),
                           &oaa::IChannelHandler::sendRequested);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);
        QVERIFY(!pingSpy.isEmpty());
        QCOMPARE(pingSpy.last()[1].value<uint16_t>(), uint16_t(0x000b));
        pingSpy.clear();

        QTRY_VERIFY_WITH_TIMEOUT(pingSpy.count() >= 3, 120);
        for (const auto& emission : pingSpy)
            QCOMPARE(emission[1].value<uint16_t>(), uint16_t(0x000b));
        QTest::qWait(30);
        QCOMPARE(session.state(), oaa::SessionState::Active);

        QTRY_COMPARE_WITH_TIMEOUT(session.state(),
                                  oaa::SessionState::Disconnected, 120);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::PingTimeout);

        emit session.controlChannel()->pongReceived(123);
        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
    }

    void testNonPositiveLivenessIntervalsUseSafeDefaults() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        config.pingInterval = 0;
        config.pingTimeout = -1;
        oaa::AASession session(&transport, config);
        QSignalSpy pingSpy(session.controlChannel(),
                           &oaa::IChannelHandler::sendRequested);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);

        QVERIFY(!pingSpy.isEmpty());
        QCOMPARE(pingSpy.last()[1].value<uint16_t>(), uint16_t(0x000b));
        pingSpy.clear();
        QTest::qWait(30);
        QCOMPARE(pingSpy.count(), 0);
        QCOMPARE(session.state(), oaa::SessionState::Active);
        QCOMPARE(disconnectSpy.count(), 0);
    }

    void testActivePongClearsDeadlineAndNextPingRearmsIt() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        config.pingInterval = 60;
        config.pingTimeout = 80;
        oaa::AASession session(&transport, config);
        QSignalSpy pingSpy(session.controlChannel(),
                           &oaa::IChannelHandler::sendRequested);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);
        const int64_t firstTimestamp = pingTimestamp(pingSpy.last());
        QVERIFY(firstTimestamp >= 0);
        pingSpy.clear();

        QTest::qWait(55);
        emit session.controlChannel()->pongReceived(firstTimestamp);
        QTRY_VERIFY_WITH_TIMEOUT(!pingSpy.isEmpty(), 30);
        QCOMPARE(pingSpy.last()[1].value<uint16_t>(), uint16_t(0x000b));

        // A duplicate response to the prior ping cannot clear the newer
        // outstanding deadline.
        emit session.controlChannel()->pongReceived(firstTimestamp);
        QTest::qWait(55);
        QCOMPARE(session.state(), oaa::SessionState::Active);

        QTRY_COMPARE_WITH_TIMEOUT(session.state(),
                                  oaa::SessionState::Disconnected, 100);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::PingTimeout);
    }

    void testMismatchedPongCannotClearDeadline() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        config.pingInterval = 1000;
        config.pingTimeout = 40;
        oaa::AASession session(&transport, config);
        QSignalSpy pingSpy(session.controlChannel(),
                           &oaa::IChannelHandler::sendRequested);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);
        const int64_t timestamp = pingTimestamp(pingSpy.last());
        QVERIFY(timestamp >= 0);

        emit session.controlChannel()->pongReceived(timestamp + 1);
        QTRY_COMPARE_WITH_TIMEOUT(session.state(),
                                  oaa::SessionState::Disconnected, 100);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::PingTimeout);
    }

    void testGracefulShutdownCancelsPongDeadline() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        config.pingInterval = 1000;
        config.pingTimeout = 40;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);
        session.stop();
        QCOMPARE(session.state(), oaa::SessionState::ShuttingDown);

        QTest::qWait(70);
        QCOMPARE(session.state(), oaa::SessionState::ShuttingDown);
        QCOMPARE(disconnectSpy.count(), 0);

        emit session.controlChannel()->shutdownAcknowledged();
        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
        QCOMPARE(disconnectSpy[0][0].value<oaa::DisconnectReason>(),
                 oaa::DisconnectReason::Normal);
    }

    void testSynchronousVersionWriteFailureCannotResurrectSession() {
        ReentrantErrorTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);
        transport.failOnWrite = 1;

        transport.simulateConnect();
        session.start();

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
    }

    void testSynchronousAuthWriteFailureCannotAdvanceSession() {
        ReentrantErrorTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        transport.feedData(makeVersionResponseFrame(1, 7, 0x0000));
        transport.failOnWrite = transport.writeCount + 1;
        emit session.messenger()->handshakeComplete();

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
    }

    void testSynchronousDiscoveryWriteFailureCannotActivateSession() {
        ReentrantErrorTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        QSignalSpy disconnectSpy(&session, &oaa::AASession::disconnected);

        transport.simulateConnect();
        session.start();
        transport.feedData(makeVersionResponseFrame(1, 7, 0x0000));
        emit session.messenger()->handshakeComplete();
        QCOMPARE(session.state(), oaa::SessionState::ServiceDiscovery);
        transport.failOnWrite = transport.writeCount + 1;
        emit session.messenger()->messageReceived(0, 0x0005, QByteArray(), 0);

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(disconnectSpy.count(), 1);
    }

    void testSynchronousChannelResponseFailureCannotOpenHandler() {
        ReentrantErrorTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        MockChannelHandler handler(3);
        session.registerChannel(3, &handler);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);
        transport.failOnWrite = transport.writeCount + 1;

        oaa::proto::messages::ChannelOpenRequest request;
        request.set_channel_id(3);
        request.set_priority(1);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        emit session.messenger()->messageReceived(
            3, 0x0007, payload, 0, oaa::MessageType::Control);

        QCOMPARE(session.state(), oaa::SessionState::Disconnected);
        QCOMPARE(handler.openCount, 0);
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
                incomingChannel, 0x0007, payload, 0,
                oaa::MessageType::Control);

            QCOMPARE(handler.openCount, 1);
            QCOMPARE(transport.writtenData().size(), 1);
            const QByteArray response = transport.writtenData().first();
            QCOMPARE(static_cast<uint8_t>(response[0]), uint8_t(3));
            QCOMPARE(static_cast<uint8_t>(response[1]), uint8_t(0x07));
        }
    }

    void testMalformedChannelOpenIdsCannotAliasRegisteredChannel() {
        for (const uint8_t incomingChannel : {uint8_t(0), uint8_t(3)}) {
            for (const int requestedChannel : {-253, 259}) {
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
                request.set_channel_id(requestedChannel);
                request.set_priority(1);
                QByteArray payload(request.ByteSizeLong(), '\0');
                QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
                session.messenger()->messageReceived(
                    incomingChannel, 0x0007, payload, 0,
                    oaa::MessageType::Control);

                QCOMPARE(handler.openCount, 0);
                QCOMPARE(transport.writtenData().size(), 0);
            }
        }
    }

    void testServiceChannelOpenRequiresMatchingPayloadChannel() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        MockChannelHandler videoHandler(3);
        MockChannelHandler audioHandler(4);
        session.registerChannel(3, &videoHandler);
        session.registerChannel(4, &audioHandler);

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
            4, 0x0007, payload, 0, oaa::MessageType::Control);

        QCOMPARE(videoHandler.openCount, 0);
        QCOMPARE(audioHandler.openCount, 0);
        QCOMPARE(transport.writtenData().size(), 0);
    }

    void testClusterServiceChannelCloseAndDuplicateOpenPublishLifecycle() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        MockChannelHandler handler(oaa::ChannelId::ClusterVideo);
        session.registerChannel(oaa::ChannelId::ClusterVideo, &handler);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);

        oaa::proto::messages::ChannelOpenRequest request;
        request.set_channel_id(oaa::ChannelId::ClusterVideo);
        request.set_priority(1);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        QSignalSpy openedSpy(&session, &oaa::AASession::channelOpened);
        QSignalSpy closedSpy(&session, &oaa::AASession::channelClosed);

        session.messenger()->messageReceived(
            oaa::ChannelId::ClusterVideo, 0x0007, payload, 0,
            oaa::MessageType::Control);
        QCOMPARE(handler.openCount, 1);
        QCOMPARE(openedSpy.count(), 1);

        session.messenger()->messageReceived(
            oaa::ChannelId::ClusterVideo, 0x0009, QByteArray(), 0,
            oaa::MessageType::Control);
        QCOMPARE(handler.closeCount, 1);
        QCOMPARE(closedSpy.count(), 1);
        QCOMPARE(closedSpy[0][0].value<uint8_t>(),
                 oaa::ChannelId::ClusterVideo);

        session.messenger()->messageReceived(
            oaa::ChannelId::ClusterVideo, 0x0007, payload, 0,
            oaa::MessageType::Control);
        QCOMPARE(handler.openCount, 2);
        session.messenger()->messageReceived(
            oaa::ChannelId::ClusterVideo, 0x0007, payload, 0,
            oaa::MessageType::Control);
        QCOMPARE(handler.closeCount, 2);
        QCOMPARE(handler.openCount, 3);
        QCOMPARE(closedSpy.count(), 2);
        QCOMPARE(openedSpy.count(), 3);
    }

    void testLegacyServiceChannelCloseThenReopenAndDuplicateOpenIsIdempotent() {
        for (const uint8_t channelId : {
                 oaa::ChannelId::Video,
                 oaa::ChannelId::MediaAudio,
                 oaa::ChannelId::AVInput}) {
            oaa::ReplayTransport transport;
            oaa::SessionConfig config;
            oaa::AASession session(&transport, config);
            MockChannelHandler handler(channelId);
            session.registerChannel(channelId, &handler);

            transport.simulateConnect();
            session.start();
            advanceToActive(session);

            oaa::proto::messages::ChannelOpenRequest request;
            request.set_channel_id(channelId);
            request.set_priority(1);
            QByteArray payload(request.ByteSizeLong(), '\0');
            QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
            QSignalSpy openedSpy(&session, &oaa::AASession::channelOpened);
            QSignalSpy closedSpy(&session, &oaa::AASession::channelClosed);

            session.messenger()->messageReceived(
                channelId, 0x0007, payload, 0,
                oaa::MessageType::Control);
            session.messenger()->messageReceived(
                channelId, 0x0007, payload, 0,
                oaa::MessageType::Control);

            QCOMPARE(handler.openCount, 1);
            QCOMPARE(handler.closeCount, 0);
            QCOMPARE(openedSpy.count(), 1);
            QCOMPARE(closedSpy.count(), 0);

            session.messenger()->messageReceived(
                channelId, 0x0009, QByteArray(), 0,
                oaa::MessageType::Control);
            QCOMPARE(handler.closeCount, 1);
            QCOMPARE(closedSpy.count(), 1);

            session.messenger()->messageReceived(
                channelId, oaa::AVMessageId::START_INDICATION,
                QByteArray(), 0, oaa::MessageType::Specific);
            QCOMPARE(handler.messageCount, 0);

            session.messenger()->messageReceived(
                channelId, 0x0007, payload, 0,
                oaa::MessageType::Control);
            QCOMPARE(handler.openCount, 2);
            QCOMPARE(openedSpy.count(), 2);

            // The same numeric ID in a service-specific frame is handler data,
            // not a transport close notification.
            session.messenger()->messageReceived(
                channelId, 0x0009, QByteArray(), 0,
                oaa::MessageType::Specific);
            QCOMPARE(handler.closeCount, 1);
            QCOMPARE(closedSpy.count(), 1);
            QCOMPARE(handler.messageCount, 1);
            QCOMPARE(handler.lastMessageId, uint16_t(0x0009));

            session.messenger()->messageReceived(
                channelId, 0x0007, QByteArrayLiteral("service payload"), 0,
                oaa::MessageType::Specific);
            QCOMPARE(handler.openCount, 2);
            QCOMPARE(handler.messageCount, 2);
            QCOMPARE(handler.lastMessageId, uint16_t(0x0007));
        }
    }

    void testUnregisteredChannelIsRejectedOnBothDispatchPaths() {
        for (const uint8_t incomingChannel : {uint8_t(0), uint8_t(9)}) {
            oaa::ReplayTransport transport;
            oaa::SessionConfig config;
            oaa::AASession session(&transport, config);
            QSignalSpy rejectedSpy(&session, &oaa::AASession::channelOpenRejected);

            transport.simulateConnect();
            session.start();
            advanceToActive(session);
            transport.clearWritten();

            oaa::proto::messages::ChannelOpenRequest request;
            request.set_channel_id(9);
            request.set_priority(1);
            QByteArray payload(request.ByteSizeLong(), '\0');
            QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
            session.messenger()->messageReceived(
                incomingChannel, 0x0007, payload, 0,
                oaa::MessageType::Control);

            QCOMPARE(rejectedSpy.count(), 1);
            QCOMPARE(rejectedSpy[0][0].toInt(), 9);
            QCOMPARE(transport.writtenData().size(), 1);
            QCOMPARE(static_cast<uint8_t>(transport.writtenData().first()[0]),
                     uint8_t(9));
        }
    }

    void testReplacingHandlerDuringActiveCycleClosesOldHandler() {
        oaa::ReplayTransport transport;
        oaa::SessionConfig config;
        oaa::AASession session(&transport, config);
        MockChannelHandler oldHandler(3);
        MockChannelHandler replacementHandler(3);
        session.registerChannel(3, &oldHandler);

        transport.simulateConnect();
        session.start();
        advanceToActive(session);

        oaa::proto::messages::ChannelOpenRequest request;
        request.set_channel_id(3);
        request.set_priority(1);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        session.messenger()->messageReceived(
            3, 0x0007, payload, 0, oaa::MessageType::Control);
        QCOMPARE(oldHandler.openCount, 1);

        session.registerChannel(3, &replacementHandler);
        QCOMPARE(oldHandler.closeCount, 1);
        transport.clearWritten();

        emit oldHandler.sendRequested(3, 0x1234, QByteArray("old"));
        QCOMPARE(transport.writtenData().size(), 0);
        emit replacementHandler.sendRequested(3, 0x1234, QByteArray("new"));
        QCOMPARE(transport.writtenData().size(), 1);

        session.finalize();
        QCOMPARE(oldHandler.closeCount, 1);
        QCOMPARE(replacementHandler.closeCount, 1);
    }
};

QTEST_MAIN(TestSessionFSM)
#include "test_session_fsm.moc"
