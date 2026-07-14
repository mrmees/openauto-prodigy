// test_api_loopback — End-to-end integration suite for the External API v1,
// exercising the full server stack over REAL loopback sockets (design doc §15).
//
// Unlike the unit suites (test_api_session / test_api_request_handlers) which
// drive an in-file FakeTransport, every case here builds a live ApiServer bound
// to ephemeral ports, connects genuine QTcpSocket / QWebSocket clients, and
// asserts on bytes that crossed the kernel. A failure in this suite is a real
// integration bug, not a mock artifact.
//
// The fixture assembles the real service graph — YamlConfig + ConfigService,
// MediaStatusService, PhoneStateService, ThemeService, ActionRegistry,
// NotificationService — with navigation/projection/bluetooth deliberately null
// (so TOPIC_NAVIGATION is unavailable, which case 3 relies on). The paired
// client store is redirected to a scratch path that init() wipes per test.

#include <QtTest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QWebSocket>
#include <QHostAddress>
#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>

#include "core/api/ApiServer.hpp"
#include "core/api/ApiFramer.hpp"
#include "core/api/ApiInboundState.hpp"

#include "core/YamlConfig.hpp"
#include "core/services/ConfigService.hpp"
#include "core/services/MediaStatusService.hpp"
#include "core/services/ActionRegistry.hpp"
#include "core/services/NotificationService.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/PhoneStateService.hpp"

#include "api/api.pb.h"

namespace pb = prodigy::api::v1;
using oap::api::ApiServer;
using oap::api::ApiServiceRefs;
using oap::api::ApiFramer;

// -----------------------------------------------------------------------------
// In-file wire helpers (Task 12 pattern — copied because tests don't share
// headers). TCP frames are length-prefixed (ApiFramer); WS frames are one
// serialized ApiMessage per binary message.
// -----------------------------------------------------------------------------
namespace {

const char* const kStorePath = "/tmp/oap_test_loopback_clients.yaml";
const char* const kConfigPath = "/tmp/oap_test_loopback_config.yaml";

QByteArray serialize(const pb::ApiMessage& m) {
    std::string s;
    m.SerializeToString(&s);
    return QByteArray::fromStdString(s);
}

pb::ApiMessage clientHello(quint64 reqId, quint32 major = 1) {
    pb::ApiMessage m;
    m.set_request_id(reqId);
    auto* h = m.mutable_client_hello();
    h->set_requested_api_version_major(major);
    h->set_client_name("TestClient");
    h->set_client_kind(pb::CLIENT_KIND_DIAGNOSTIC);
    return m;
}

pb::ApiMessage subscribe(quint64 reqId, pb::Topic topic) {
    pb::ApiMessage m;
    m.set_request_id(reqId);
    m.mutable_subscribe_request()->add_topics(topic);
    return m;
}

bool capabilitiesHaveTopic(const pb::Capabilities& caps, pb::Topic t) {
    for (int i = 0; i < caps.supported_topics_size(); ++i)
        if (caps.supported_topics(i) == t) return true;
    return false;
}

void sendFramed(QTcpSocket& sock, const pb::ApiMessage& m) {
    sock.write(ApiFramer::encode(serialize(m)));
}

// Spins the shared event loop (server + client both live in this process)
// until one framed message is available or the deadline expires. Returns a
// default (empty) ApiMessage on timeout — callers assert on payload_case().
pb::ApiMessage readFramed(QTcpSocket& sock, ApiFramer& framer,
                          QList<QByteArray>& queue, int timeoutMs = 3000) {
    QDeadlineTimer deadline(timeoutMs);
    while (queue.isEmpty() && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (sock.bytesAvailable() > 0)
            queue.append(framer.feed(sock.readAll()));
    }
    pb::ApiMessage m;
    if (!queue.isEmpty()) {
        const QByteArray f = queue.takeFirst();
        m.ParseFromArray(f.constData(), f.size());
    }
    return m;
}

// Spins the loop for `quietMs`, feeding any bytes into the framer/queue.
// Returns true iff NO framed message arrived during the window (the queue is
// still empty). Used for the design's "no frame is delivered" assertions.
bool quietFor(QTcpSocket& sock, ApiFramer& framer, QList<QByteArray>& queue,
              int quietMs) {
    QDeadlineTimer deadline(quietMs);
    while (!deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (sock.bytesAvailable() > 0)
            queue.append(framer.feed(sock.readAll()));
        if (!queue.isEmpty()) return false;
    }
    return queue.isEmpty();
}

} // namespace

// -----------------------------------------------------------------------------
class TestApiLoopback : public QObject {
    Q_OBJECT

    // Fresh service set per test. navigation/projection/bluetooth null, so
    // TOPIC_NAVIGATION / TOPIC_PROJECTION are absent from Capabilities and
    // reject on subscribe.
    struct Fixture {
        oap::YamlConfig yaml;
        oap::ConfigService config{&yaml, kConfigPath};
        oap::MediaStatusService media;
        oap::ActionRegistry actions;
        oap::NotificationService notifications;
        oap::ThemeService theme;
        oap::PhoneStateService phone;

        ApiServiceRefs refs() {
            ApiServiceRefs r;
            r.media = &media;
            r.phone = &phone;
            r.theme = &theme;
            r.notifications = &notifications;
            r.actions = &actions;
            r.config = &config;
            return r;
        }
    };

private slots:
    void init();   // per-test: wipe the scratch paired-client store

    // Design §15 mandatory cases.
    void testHandshakeTcpAndWs();
    void testAuthRejectPaths();
    void testSnapshotOnSubscribe();
    void testDeltaDelivery();
    void testSlowConsumerDisconnect();
    void testClientActionLifecycle();

    // Design §15 extras.
    void testNotificationOwnershipE2E();
    void testPhoneUnavailableE2E();
    void testInboundReportsE2E();

    // Task 15 addendum: peer-admission policy edges (static seam, no sockets).
    void testPeerAdmissionPolicy();

    // QR pairing surface: payload contract + data-URI property lifecycle.
    void testPairingQrPayloadAndProperty();
};

void TestApiLoopback::init() {
    QFile::remove(QString::fromLatin1(kStorePath));
}

// Case 1 — ClientHello -> ServerHello over BOTH transports.
void TestApiLoopback::testHandshakeTcpAndWs() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    // ---- TCP transport ----
    QTcpSocket tcp;
    tcp.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(tcp.waitForConnected(3000));
    ApiFramer framer;
    QList<QByteArray> queue;
    sendFramed(tcp, clientHello(1));
    pb::ApiMessage tcpHello = readFramed(tcp, framer, queue);
    QCOMPARE(tcpHello.payload_case(), pb::ApiMessage::kServerHello);
    QCOMPARE(tcpHello.server_hello().api_version_major(), quint32(1));
    QVERIFY(!tcpHello.server_hello().session_id().empty());

    // ---- WebSocket transport ----
    QWebSocket ws;
    QSignalSpy wsConnected(&ws, &QWebSocket::connected);
    QSignalSpy wsBinary(&ws, &QWebSocket::binaryMessageReceived);
    ws.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.wsPort())));
    QTRY_COMPARE(wsConnected.count(), 1);
    ws.sendBinaryMessage(serialize(clientHello(1)));
    QTRY_COMPARE(wsBinary.count(), 1);
    pb::ApiMessage wsHello;
    const QByteArray wsBytes = wsBinary.at(0).at(0).toByteArray();
    wsHello.ParseFromArray(wsBytes.constData(), wsBytes.size());
    QCOMPARE(wsHello.payload_case(), pb::ApiMessage::kServerHello);
    QCOMPARE(wsHello.server_hello().api_version_major(), quint32(1));

    ws.close();
    server.stop();
}

// Case 2 — auth reject paths. A loopback socket ALWAYS presents a trusted
// (127.0.0.1) peer, so it can never exercise the remote AuthRequired /
// AuthReject / PairingReject branches through a real socket. What we CAN assert
// end-to-end is the flip side of the same policy: a ClientHello carrying NO
// auth block still reaches Ready from localhost (localhost trust).
//
// Remote-path integration (unknown-client reject, bad-proof AuthReject, closed
// pairing window) is covered by the session unit tests, which use the
// setPeerTrustOverrideForTest(false) seam to force the non-trusted branch:
//   test_api_session.cpp:testRemoteAuthHappyPath / testRemoteAuthBadProof /
//   testPairingFlow. Loopback cannot present a remote peer, by design.
void TestApiLoopback::testAuthRejectPaths() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(sock.waitForConnected(3000));
    ApiFramer framer;
    QList<QByteArray> queue;

    // Hello with no auth block -> trusted localhost -> straight to ServerHello.
    sendFramed(sock, clientHello(1));   // clientHello() sets no auth field
    pb::ApiMessage hello = readFramed(sock, framer, queue);
    QCOMPARE(hello.payload_case(), pb::ApiMessage::kServerHello);
    QVERIFY(hello.server_hello().granted_client_id().empty());  // no pairing grant

    server.stop();
}

// Case 3 — snapshot-on-subscribe carries pre-set state; unavailable topic
// rejects. Metadata is set BEFORE start() so no publisher coalesce timer is
// pending (the snapshot is read straight from the provider's current state).
void TestApiLoopback::testSnapshotOnSubscribe() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    f.media.setBtConnected(true);
    f.media.updateBtMetadata("PreSetTitle", "PreArtist", "PreAlbum");

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(sock.waitForConnected(3000));
    ApiFramer framer;
    QList<QByteArray> queue;

    sendFramed(sock, clientHello(1));
    QCOMPARE(readFramed(sock, framer, queue).payload_case(),
             pb::ApiMessage::kServerHello);

    // Subscribe MEDIA -> accepted, then a full snapshot carrying the pre-set title.
    sendFramed(sock, subscribe(2, pb::TOPIC_MEDIA));
    pb::ApiMessage subResp = readFramed(sock, framer, queue);
    QCOMPARE(subResp.payload_case(), pb::ApiMessage::kSubscribeResponse);
    QCOMPARE(subResp.subscribe_response().results_size(), 1);
    QVERIFY(subResp.subscribe_response().results(0).accepted());

    pb::ApiMessage snapshot = readFramed(sock, framer, queue);
    QCOMPARE(snapshot.payload_case(), pb::ApiMessage::kMediaStatus);
    QCOMPARE(snapshot.request_id(), quint64(0));   // server-initiated snapshot
    QCOMPARE(QString::fromStdString(snapshot.media_status().title()),
             QString("PreSetTitle"));

    // Subscribe NAVIGATION (null provider -> no snapshot) -> rejected, no frame.
    sendFramed(sock, subscribe(3, pb::TOPIC_NAVIGATION));
    pb::ApiMessage navResp = readFramed(sock, framer, queue);
    QCOMPARE(navResp.payload_case(), pb::ApiMessage::kSubscribeResponse);
    QCOMPARE(navResp.subscribe_response().results_size(), 1);
    QVERIFY(!navResp.subscribe_response().results(0).accepted());
    QVERIFY2(quietFor(sock, framer, queue, 200),
             "rejected NAVIGATION subscribe must not deliver a snapshot");

    server.stop();
}

// Case 4 — a mutation fans a delta to every subscriber; a connected-but-
// unsubscribed client receives nothing.
void TestApiLoopback::testDeltaDelivery() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    f.media.setBtConnected(true);
    f.media.updateBtMetadata("Initial", "a", "b");

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    auto connectHello = [&](QTcpSocket& s, ApiFramer& fr, QList<QByteArray>& q) {
        s.connectToHost(QHostAddress::LocalHost, server.tcpPort());
        QVERIFY(s.waitForConnected(3000));
        sendFramed(s, clientHello(1));
        QCOMPARE(readFramed(s, fr, q).payload_case(), pb::ApiMessage::kServerHello);
    };
    auto subscribeMedia = [&](QTcpSocket& s, ApiFramer& fr, QList<QByteArray>& q) {
        sendFramed(s, subscribe(2, pb::TOPIC_MEDIA));
        QCOMPARE(readFramed(s, fr, q).payload_case(), pb::ApiMessage::kSubscribeResponse);
        // Drain the initial snapshot delivered on subscribe.
        QCOMPARE(readFramed(s, fr, q).payload_case(), pb::ApiMessage::kMediaStatus);
    };

    QTcpSocket a, b, c;
    ApiFramer fa, fb, fc;
    QList<QByteArray> qa, qb, qc;
    connectHello(a, fa, qa); subscribeMedia(a, fa, qa);
    connectHello(b, fb, qb); subscribeMedia(b, fb, qb);
    connectHello(c, fc, qc);   // C connects + handshakes but never subscribes

    // Mutate -> exactly one coalesced delta to A and B.
    f.media.updateBtMetadata("DeltaTitle", "a2", "b2");

    pb::ApiMessage da = readFramed(a, fa, qa);
    QCOMPARE(da.payload_case(), pb::ApiMessage::kMediaStatus);
    QCOMPARE(QString::fromStdString(da.media_status().title()), QString("DeltaTitle"));

    pb::ApiMessage db = readFramed(b, fb, qb);
    QCOMPARE(db.payload_case(), pb::ApiMessage::kMediaStatus);
    QCOMPARE(QString::fromStdString(db.media_status().title()), QString("DeltaTitle"));

    // C is unsubscribed: it must receive no delta.
    QVERIFY2(quietFor(c, fc, qc, 200),
             "unsubscribed client must not receive topic deltas");

    server.stop();
}

// Case 5 — a subscriber that stops reading is DISCONNECTED (outbound byte cap),
// while a draining subscriber keeps receiving. See the extended comment on the
// backpressure mechanics inline below.
void TestApiLoopback::testSlowConsumerDisconnect() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    f.media.setBtConnected(true);
    f.media.updateBtMetadata("Initial", "a", "b");

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    // Client B: well-behaved consumer that keeps draining throughout the flood.
    QTcpSocket sockB;
    sockB.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(sockB.waitForConnected(3000));
    ApiFramer framerB;
    QList<QByteArray> qB;
    sendFramed(sockB, clientHello(1));
    QCOMPARE(readFramed(sockB, framerB, qB).payload_case(), pb::ApiMessage::kServerHello);
    sendFramed(sockB, subscribe(2, pb::TOPIC_MEDIA));
    QCOMPARE(readFramed(sockB, framerB, qB).payload_case(), pb::ApiMessage::kSubscribeResponse);
    readFramed(sockB, framerB, qB);   // drain snapshot

    // Client A: subscribes, then STOPS reading. A single-process loopback test
    // needs A to apply real TCP backpressure — Qt's default (unbounded) read
    // buffer would silently swallow the entire flood into client-side memory
    // and the server's bytesToWrite would never grow. A tiny Qt read buffer +
    // kernel SO_RCVBUF makes "stopped reading" propagate a closed TCP window
    // back to the server. (This bounds the CLIENT's absorption; it does NOT
    // touch the server's outbound byte cap, which stays at its 1 MiB default.)
    QTcpSocket sockA;
    sockA.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(sockA.waitForConnected(3000));
    sockA.setReadBufferSize(2048);
    sockA.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 2048);
    ApiFramer framerA;
    QList<QByteArray> qA;
    sendFramed(sockA, clientHello(1));
    QCOMPARE(readFramed(sockA, framerA, qA).payload_case(), pb::ApiMessage::kServerHello);
    sendFramed(sockA, subscribe(2, pb::TOPIC_MEDIA));
    QCOMPARE(readFramed(sockA, framerA, qA).payload_case(), pb::ApiMessage::kSubscribeResponse);
    readFramed(sockA, framerA, qA);   // drain snapshot — then never read A again

    // Flood BT metadata. Publishers coalesce all updates within one event-loop
    // turn into a single delta, so processEvents() every 100 iterations yields
    // ~one delta per batch. B is drained each batch; A is not, so A's outbound
    // queue on the server crosses the cap and the server tears A's session down
    // (sessionCount 2 -> 1) while B keeps receiving. Loop exits the instant that
    // happens; kFloodMax + a <10 s budget only bound the pathological case.
    // NOTE: the byte cap is enforced by the server writing into A's socket until
    // Qt's bytesToWrite exceeds the cap — but the loopback kernel send buffer
    // absorbs several MiB first, so a faithful flood of QString::number(i)-sized
    // frames needs millions of iterations. If this ever goes flaky the fix is to
    // RAISE kFloodMax, never to lower/raise the server byte cap.
    int bFrames = 0;
    int i = 0;
    const int kFloodMax = 20'000'000;
    QDeadlineTimer budget(9000);
    while (server.sessionCount() > 1 && i < kFloodMax && !budget.hasExpired()) {
        f.media.updateBtMetadata(QString::number(i), "a", "b");
        if (++i % 100 == 0) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
            if (sockB.bytesAvailable() > 0)
                bFrames += framerB.feed(sockB.readAll()).size();
        }
    }

    // The outbound cap tore down the non-reading consumer A, server-side, while
    // the draining consumer B stayed connected and kept receiving deltas.
    QVERIFY2(server.sessionCount() == 1,
             "slow consumer A was not disconnected by the outbound byte cap");
    QVERIFY2(bFrames > 0, "draining consumer B stopped receiving deltas");
    QCOMPARE(sockB.state(), QAbstractSocket::ConnectedState);

    // A's client socket observes the server-initiated close once it is allowed
    // to drain the megabytes buffered behind its closed window (its read
    // notifier was disabled while the tiny buffer sat full and unread). Lift the
    // cap and drain until the FIN/RST surfaces as a state change.
    sockA.setReadBufferSize(0);
    QDeadlineTimer drainDeadline(5000);
    while (sockA.state() == QAbstractSocket::ConnectedState && !drainDeadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        sockA.readAll();
    }
    QVERIFY2(sockA.state() != QAbstractSocket::ConnectedState,
             "client A never observed the server-initiated disconnect");

    server.stop();
}

// Case 6 — client-registered action: dispatch fans an ActionInvoked to the
// owner; disconnect auto-unregisters it; a later dispatch returns false.
void TestApiLoopback::testClientActionLifecycle() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(sock.waitForConnected(3000));
    ApiFramer framer;
    QList<QByteArray> queue;

    sendFramed(sock, clientHello(1));
    QCOMPARE(readFramed(sock, framer, queue).payload_case(), pb::ApiMessage::kServerHello);

    // Register "looptest.act" over the wire.
    pb::ApiMessage reg;
    reg.set_request_id(2);
    auto* spec = reg.mutable_register_actions_request()->add_actions();
    spec->set_id("looptest.act");
    spec->set_label("Loop Test");
    sendFramed(sock, reg);
    pb::ApiMessage regResp = readFramed(sock, framer, queue);
    QCOMPARE(regResp.payload_case(), pb::ApiMessage::kRegisterActionsResponse);
    QCOMPARE(regResp.register_actions_response().results_size(), 1);
    QVERIFY(regResp.register_actions_response().results(0).accepted());
    QVERIFY(f.actions.registeredActions().contains(QStringLiteral("looptest.act")));

    // Server-side dispatch -> ActionInvoked reaches the owning socket.
    QVERIFY(f.actions.dispatch(QStringLiteral("looptest.act"), QVariant(5)));
    pb::ApiMessage invoked = readFramed(sock, framer, queue);
    QCOMPARE(invoked.payload_case(), pb::ApiMessage::kActionInvoked);
    QCOMPARE(invoked.request_id(), quint64(0));   // server-initiated event
    QCOMPARE(QString::fromStdString(invoked.action_invoked().id()),
             QString("looptest.act"));
    QVERIFY(invoked.action_invoked().has_payload_json());
    QCOMPARE(QString::fromStdString(invoked.action_invoked().payload_json()),
             QString("5"));

    // Drop the socket -> session teardown -> auto-unregister.
    sock.abort();
    QTRY_VERIFY(!f.actions.registeredActions().contains(QStringLiteral("looptest.act")));

    // The action is gone: a later dispatch finds no handler.
    QVERIFY(!f.actions.dispatch(QStringLiteral("looptest.act"), QVariant(5)));

    server.stop();
}

// Case 7 — notification ownership across two clients: only the poster may
// dismiss; a non-owner gets NOT_FOUND (and stays alive).
void TestApiLoopback::testNotificationOwnershipE2E() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    QTcpSocket a, b;
    ApiFramer fa, fb;
    QList<QByteArray> qa, qb;
    a.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(a.waitForConnected(3000));
    b.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(b.waitForConnected(3000));
    sendFramed(a, clientHello(1));
    QCOMPARE(readFramed(a, fa, qa).payload_case(), pb::ApiMessage::kServerHello);
    sendFramed(b, clientHello(1));
    QCOMPARE(readFramed(b, fb, qb).payload_case(), pb::ApiMessage::kServerHello);

    // A posts a notification.
    pb::ApiMessage post;
    post.set_request_id(2);
    post.mutable_post_notification_request()->set_message("from A");
    sendFramed(a, post);
    pb::ApiMessage postResp = readFramed(a, fa, qa);
    QCOMPARE(postResp.payload_case(), pb::ApiMessage::kPostNotificationResponse);
    const std::string notifId = postResp.post_notification_response().notification_id();
    QVERIFY(!notifId.empty());
    QCOMPARE(f.notifications.active().size(), 1);

    // B tries to dismiss A's notification -> NOT_FOUND, still present, B alive.
    pb::ApiMessage disB;
    disB.set_request_id(3);
    disB.mutable_dismiss_notification_request()->set_notification_id(notifId);
    sendFramed(b, disB);
    pb::ApiMessage bResp = readFramed(b, fb, qb);
    QCOMPARE(bResp.payload_case(), pb::ApiMessage::kError);
    QCOMPARE(bResp.error().code(), pb::ERROR_CODE_NOT_FOUND);
    QCOMPARE(f.notifications.active().size(), 1);
    QCOMPARE(b.state(), QAbstractSocket::ConnectedState);

    // A dismisses its own -> Ack, gone.
    pb::ApiMessage disA;
    disA.set_request_id(4);
    disA.mutable_dismiss_notification_request()->set_notification_id(notifId);
    sendFramed(a, disA);
    QCOMPARE(readFramed(a, fa, qa).payload_case(), pb::ApiMessage::kAck);
    QCOMPARE(f.notifications.active().size(), 0);

    server.stop();
}

// Case 8 — phone command surface with telephony unavailable (v1 default):
// both DialRequest and AnswerCallRequest return UNAVAILABLE. The all-false
// static Capabilities.phone and the runtime result must agree.
void TestApiLoopback::testPhoneUnavailableE2E() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    // PhoneStateService defaults to telephonyAvailable() == false.

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(sock.waitForConnected(3000));
    ApiFramer framer;
    QList<QByteArray> queue;
    sendFramed(sock, clientHello(1));
    pb::ApiMessage hello = readFramed(sock, framer, queue);
    QCOMPARE(hello.payload_case(), pb::ApiMessage::kServerHello);
    // Static phone command surface is present but all-false in v1.
    QVERIFY(hello.server_hello().capabilities().has_phone());
    const auto& phoneCaps = hello.server_hello().capabilities().phone();
    QVERIFY(!phoneCaps.can_dial());
    QVERIFY(!phoneCaps.can_answer());
    QVERIFY(!phoneCaps.can_hangup());
    QVERIFY(!phoneCaps.can_send_dtmf());
    QVERIFY(!phoneCaps.can_hold_swap());
    QVERIFY(!phoneCaps.can_multiparty());

    pb::ApiMessage dial;
    dial.set_request_id(2);
    dial.mutable_dial_request()->set_number("5551234");
    sendFramed(sock, dial);
    pb::ApiMessage dialResp = readFramed(sock, framer, queue);
    QCOMPARE(dialResp.payload_case(), pb::ApiMessage::kPhoneCommandResponse);
    QCOMPARE(dialResp.phone_command_response().result(),
             pb::PHONE_COMMAND_RESULT_UNAVAILABLE);

    pb::ApiMessage answer;
    answer.set_request_id(3);
    answer.mutable_answer_call_request();
    sendFramed(sock, answer);
    pb::ApiMessage ansResp = readFramed(sock, framer, queue);
    QCOMPARE(ansResp.payload_case(), pb::ApiMessage::kPhoneCommandResponse);
    QCOMPARE(ansResp.phone_command_response().result(),
             pb::PHONE_COMMAND_RESULT_UNAVAILABLE);

    server.stop();
}

// Case 9 — inbound fire-and-forget reports: GPS + Time over the wire update
// ApiInboundState and fire timeReported, and produce NO response frames.
void TestApiLoopback::testInboundReportsE2E() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    oap::api::ApiInboundState* inbound = server.inboundState();
    QVERIFY(inbound != nullptr);
    QSignalSpy gpsSpy(inbound, &oap::api::ApiInboundState::gpsChanged);
    QSignalSpy timeSpy(inbound, &oap::api::ApiInboundState::timeReported);

    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(sock.waitForConnected(3000));
    ApiFramer framer;
    QList<QByteArray> queue;
    sendFramed(sock, clientHello(1));
    QCOMPARE(readFramed(sock, framer, queue).payload_case(), pb::ApiMessage::kServerHello);

    // GpsReport.
    pb::ApiMessage gps;
    gps.set_request_id(0);
    auto* g = gps.mutable_gps_report();
    g->set_latitude(45.5);
    g->set_longitude(-122.6);
    g->set_speed_mps(13.4);
    sendFramed(sock, gps);

    // TimeReport.
    pb::ApiMessage time;
    time.set_request_id(0);
    time.mutable_time_report()->set_unix_time_ms(Q_INT64_C(1751731200000));
    sendFramed(sock, time);

    QTRY_COMPARE(gpsSpy.count(), 1);
    QTRY_COMPARE(timeSpy.count(), 1);
    QVERIFY(inbound->gpsValid());
    QCOMPARE(inbound->gpsLat(), 45.5);
    QCOMPARE(inbound->gpsLon(), -122.6);
    QCOMPARE(inbound->gpsSpeedMps(), 13.4);
    QCOMPARE(timeSpy.takeFirst().at(0).toLongLong(), Q_INT64_C(1751731200000));

    // Reports are fire-and-forget: the server must send nothing back.
    QVERIFY2(quietFor(sock, framer, queue, 200),
             "inbound reports must not produce response frames");

    server.stop();
}

// Task 15 addendum — peer-admission policy edges. This exercises the pure
// static seam ApiServer::peerAllowed(addr, exposeLan) directly with constructed
// QHostAddress values (NO sockets), covering the v4-mapped-v6 normalization and
// the exposeLan branch that loopback tests can never reach. Refactored out of
// the previously untested private member; behavior is identical (the whole
// suite staying green is the proof).
void TestApiLoopback::testPeerAdmissionPolicy() {
    // v4-mapped-v6 inside the AP subnet -> admitted regardless of exposeLan.
    const QHostAddress mappedAp(QStringLiteral("::ffff:10.0.0.23"));
    QVERIFY(ApiServer::inApSubnet(mappedAp));
    QVERIFY(ApiServer::peerAllowed(mappedAp, false));
    QVERIFY(ApiServer::peerAllowed(mappedAp, true));

    // v4-mapped-v6 LAN address (192.168.x) -> only when exposeLan is on.
    const QHostAddress mappedLan(QStringLiteral("::ffff:192.168.1.50"));
    QVERIFY(!ApiServer::inApSubnet(mappedLan));
    QVERIFY(!ApiServer::peerAllowed(mappedLan, false));
    QVERIFY(ApiServer::peerAllowed(mappedLan, true));

    // IPv6 loopback -> always admitted (loopback trust).
    const QHostAddress loop6(QStringLiteral("::1"));
    QVERIFY(loop6.isLoopback());
    QVERIFY(ApiServer::peerAllowed(loop6, false));

    // Plain IPv4 in the AP subnet -> admitted.
    const QHostAddress plainAp(QStringLiteral("10.0.0.7"));
    QVERIFY(ApiServer::inApSubnet(plainAp));
    QVERIFY(ApiServer::peerAllowed(plainAp, false));

    // Genuine (non-mapped) IPv6 -> rejected unless exposeLan.
    const QHostAddress globalV6(QStringLiteral("2001:db8::1"));
    QVERIFY(!ApiServer::inApSubnet(globalV6));
    QVERIFY(!ApiServer::peerAllowed(globalV6, false));
    QVERIFY(ApiServer::peerAllowed(globalV6, true));
}

// QR pairing: the payload string is a stable contract with the companion
// app's scanner (prodigy://pair?host=&tcp=&ws=&pin=), and the data-URI
// property tracks the pairing window's lifecycle.
void TestApiLoopback::testPairingQrPayloadAndProperty() {
    QCOMPARE(ApiServer::pairingQrPayload(QStringLiteral("10.0.0.1"), 9810, 9811,
                                         QStringLiteral("123456")),
             QStringLiteral("prodigy://pair?host=10.0.0.1&tcp=9810&ws=9811&pin=123456"));

    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    ApiServer server(f.refs());
    server.setStorePathForTest(kStorePath);
    QVERIFY(server.start());

    QCOMPARE(server.pairingQrDataUri(), QString());   // window closed: no QR

    server.startPairing();
    QVERIFY(server.pairingActive());
    const QString uri = server.pairingQrDataUri();
    QVERIFY(uri.startsWith(QStringLiteral("data:image/png;base64,")));
    const QByteArray png =
        QByteArray::fromBase64(uri.section(QLatin1Char(','), 1).toLatin1());
    QVERIFY(png.startsWith(QByteArray("\x89PNG", 4)));

    // Scanner reliability: ISO 18004 wants a 4-module quiet zone. At 8 px
    // per module the outer 32 px band must be pure white, with real modules
    // (black pixels) further in.
    QImage img;
    QVERIFY(img.loadFromData(png));
    const int quiet = 4 * 8;
    bool quietZoneWhite = true;
    bool hasBlackModule = false;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const bool inBand = x < quiet || y < quiet
                                || x >= img.width() - quiet
                                || y >= img.height() - quiet;
            const bool white = qGray(img.pixel(x, y)) > 200;
            if (inBand && !white) quietZoneWhite = false;
            if (!inBand && !white) hasBlackModule = true;
        }
    }
    QVERIFY(quietZoneWhite);
    QVERIFY(hasBlackModule);

    server.cancelPairing();
    QCOMPARE(server.pairingQrDataUri(), QString());   // cancel clears it

    server.stop();

    // A pairing window without live listeners must not advertise dead
    // endpoints (tcp=0&ws=0): no QR until both transports are up. The PIN
    // path stays available for manual pairing.
    ApiServer unstarted(f.refs());
    unstarted.setStorePathForTest(kStorePath);
    unstarted.startPairing();
    QVERIFY(unstarted.pairingActive());
    QCOMPARE(unstarted.pairingQrDataUri(), QString());
}

QTEST_MAIN(TestApiLoopback)
#include "test_api_loopback.moc"
