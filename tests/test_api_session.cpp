#include <QtTest>
#include <QSignalSpy>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDeadlineTimer>

#include "core/api/ApiSession.hpp"
#include "core/api/ApiTransport.hpp"
#include "core/api/ApiAuth.hpp"
#include "core/api/PairingManager.hpp"
#include "api/api.pb.h"

namespace pb = prodigy::api::v1;
using oap::api::ApiFramer;
using oap::api::ApiSession;
using oap::api::ApiSessionDeps;
using oap::api::IApiTransport;
using oap::api::TcpApiTransport;
using oap::api::PairedClient;
using oap::api::PairedClientStore;
using oap::api::PairingManager;
using oap::api::deriveSecret;
using oap::api::hmacProof;

// -----------------------------------------------------------------------------
// In-file fake transport: records sent frames, has a settable peer address and
// settable bytesToWrite, and injects inbound messages by emitting the signal.
// close() emits closed() synchronously to exercise teardown reentrancy;
// injectMessageThenClose() delivers a message and an immediate close in ONE
// call stack (the same-read TCP violation scenario from the Task 8 review).
// -----------------------------------------------------------------------------
class FakeTransport : public IApiTransport {
    Q_OBJECT
public:
    QList<QByteArray> sent;
    QHostAddress peer = QHostAddress(QHostAddress::LocalHost);
    qint64 fakeBytesToWrite = 0;

    void sendMessage(const QByteArray& serialized) override { sent.append(serialized); }
    qint64 bytesToWrite() const override { return fakeBytesToWrite; }
    void close() override { emit closed(); }
    void abort() override { aborted = true; emit closed(); }
    QHostAddress peerAddress() const override { return peer; }
    bool aborted = false;

    void injectMessage(const QByteArray& bytes) { emit messageReceived(bytes); }
    void injectMessageThenClose(const QByteArray& bytes) {
        emit messageReceived(bytes);
        emit closed();
    }
};

// -----------------------------------------------------------------------------
namespace {

QByteArray serialize(const pb::ApiMessage& m) {
    std::string s;
    m.SerializeToString(&s);
    return QByteArray::fromStdString(s);
}

QByteArray clientHello(quint32 major, quint64 reqId = 1) {
    pb::ApiMessage m;
    m.set_request_id(reqId);
    auto* h = m.mutable_client_hello();
    h->set_requested_api_version_major(major);
    h->set_client_name("TestClient");
    h->set_client_kind(pb::CLIENT_KIND_DIAGNOSTIC);
    return serialize(m);
}

QByteArray ping(quint64 reqId) {
    pb::ApiMessage m;
    m.set_request_id(reqId);
    m.mutable_ping();
    return serialize(m);
}

pb::ApiMessage parse(const QByteArray& bytes) {
    pb::ApiMessage m;
    m.ParseFromArray(bytes.constData(), bytes.size());
    return m;
}

} // namespace

// -----------------------------------------------------------------------------
class TestApiSession : public QObject {
    Q_OBJECT
private slots:
    void testHelloTrustedGoesReady();
    void testBadVersionRejected();
    void testFirstMessageMustBeHello();
    void testHandshakeTimeout();
    void testAuthResponseGetsFreshDeadline();
    void testPairingResponseGetsFreshDeadline();
    void testLegacyCredentialRequiresUpgrade();
    void testRemoteAuthHappyPath();
    void testRemoteAuthBadProof();
    void testPairingFlow();
    void testPairingWindowClosedTypedError();
    void testPairingWindowClosedRealTcpWire();
    void testTransportAbortEmitsClosedExactlyOnce();
    void testSubscribeSnapshotAndAck();
    void testQueueCapDisconnects();
    void testPingPong();
    void testReentrantMessageThenClose();
    void testServerHelloCarriesServerId();
    void testServerHelloOmitsServerIdWhenEmpty();
};

void TestApiSession::testHelloTrustedGoesReady() {
    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.serverName = "HeadUnit";
    deps.appVersion = "1.2.3";
    ApiSession session(transport, deps);

    transport->injectMessage(clientHello(1));

    QCOMPARE(session.state(), ApiSession::State::Ready);
    QVERIFY(!transport->sent.isEmpty());
    pb::ApiMessage last = parse(transport->sent.last());
    QCOMPARE(last.payload_case(), pb::ApiMessage::kServerHello);
    QVERIFY(!last.server_hello().session_id().empty());
    QCOMPARE(last.server_hello().api_version_major(), quint32(1));
    QCOMPARE(last.server_hello().api_version_minor(), quint32(2));
}

void TestApiSession::testBadVersionRejected() {
    auto* transport = new FakeTransport();
    ApiSession session(transport, ApiSessionDeps{});
    QSignalSpy terminatedSpy(&session, &ApiSession::terminated);

    transport->injectMessage(clientHello(2));

    pb::ApiMessage last = parse(transport->sent.last());
    QCOMPARE(last.payload_case(), pb::ApiMessage::kError);
    QCOMPARE(last.error().code(), pb::ERROR_CODE_UNSUPPORTED_VERSION);
    QCOMPARE(terminatedSpy.count(), 1);
    QCOMPARE(session.state(), ApiSession::State::Closed);
}

void TestApiSession::testFirstMessageMustBeHello() {
    auto* transport = new FakeTransport();
    ApiSession session(transport, ApiSessionDeps{});
    QSignalSpy terminatedSpy(&session, &ApiSession::terminated);

    transport->injectMessage(ping(5));

    pb::ApiMessage last = parse(transport->sent.last());
    QCOMPARE(last.payload_case(), pb::ApiMessage::kError);
    QCOMPARE(last.error().code(), pb::ERROR_CODE_INVALID_REQUEST);
    QCOMPARE(terminatedSpy.count(), 1);
}

void TestApiSession::testHandshakeTimeout() {
    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.handshakeTimeoutMs = 50;
    ApiSession session(transport, deps);
    QSignalSpy terminatedSpy(&session, &ApiSession::terminated);

    QTest::qWait(120);

    QCOMPARE(terminatedSpy.count(), 1);
    QCOMPARE(session.state(), ApiSession::State::Closed);
}

void TestApiSession::testAuthResponseGetsFreshDeadline() {
    PairedClientStore store("/tmp/oap_test_session_stage_auth.yaml");
    PairedClient client;
    client.clientId = "stage-client";
    client.secret = QByteArray(32, 's');
    store.upsert(client);

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.store = &store;
    deps.handshakeTimeoutMs = 150;
    ApiSession session(transport, deps);
    session.setPeerTrustOverrideForTest(false);

    QTest::qWait(90);
    pb::ApiMessage hello;
    auto* h = hello.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->mutable_auth()->set_client_id("stage-client");
    transport->injectMessage(serialize(hello));
    QCOMPARE(session.state(), ApiSession::State::AuthPending);

    QTest::qWait(90);
    QCOMPARE(session.state(), ApiSession::State::AuthPending);
    QTRY_COMPARE_WITH_TIMEOUT(session.state(), ApiSession::State::Closed, 150);
}

void TestApiSession::testPairingResponseGetsFreshDeadline() {
    const QString path = "/tmp/oap_test_session_stage_pairing.yaml";
    QFile::remove(path);
    PairedClientStore store(path);
    QVERIFY(store.load());
    PairingManager pairing(&store);
    QVERIFY(pairing.startWindow(60));

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.store = &store;
    deps.pairing = &pairing;
    deps.handshakeTimeoutMs = 150;
    ApiSession session(transport, deps);
    session.setPeerTrustOverrideForTest(false);

    QTest::qWait(90);
    pb::ApiMessage hello;
    auto* h = hello.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->mutable_auth()->set_pairing_request(true);
    transport->injectMessage(serialize(hello));
    QCOMPARE(session.state(), ApiSession::State::PairingPending);

    QTest::qWait(90);
    QCOMPARE(session.state(), ApiSession::State::PairingPending);
    QTRY_COMPARE_WITH_TIMEOUT(session.state(), ApiSession::State::Closed, 150);
}

void TestApiSession::testLegacyCredentialRequiresUpgrade() {
    PairedClientStore store("/tmp/oap_test_session_legacy.yaml");
    PairedClient client;
    client.clientId = "legacy-1";
    client.secret = QByteArray(32, 'l');
    client.name = "Legacy";
    client.credentialGeneration = oap::api::kLegacyCredentialGeneration;
    store.upsert(client);

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.store = &store;
    ApiSession session(transport, deps);
    session.setPeerTrustOverrideForTest(false);

    pb::ApiMessage hello;
    hello.set_request_id(41);
    auto* h = hello.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->set_client_name("Legacy");
    h->mutable_auth()->set_client_id("legacy-1");
    transport->injectMessage(serialize(hello));

    const pb::ApiMessage rejected = parse(transport->sent.last());
    QCOMPARE(rejected.payload_case(), pb::ApiMessage::kAuthReject);
    QCOMPARE(rejected.auth_reject().code(),
             pb::AUTH_REJECT_CODE_CREDENTIAL_UPGRADE_REQUIRED);
    QCOMPARE(session.state(), ApiSession::State::Closed);
}

void TestApiSession::testRemoteAuthHappyPath() {
    QByteArray secret(32, 'k');
    PairedClientStore store("/tmp/oap_test_session_auth.yaml");
    PairedClient client;
    client.clientId = "dev-1";
    client.secret = secret;
    client.name = "DevTool";
    client.kind = pb::CLIENT_KIND_DIAGNOSTIC;
    store.upsert(client);

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.store = &store;
    deps.serverName = "HeadUnit";
    deps.appVersion = "1.0";
    ApiSession session(transport, deps);
    session.setPeerTrustOverrideForTest(false);

    // ClientHello carrying a known client_id -> AuthRequired with a 32-byte nonce.
    pb::ApiMessage hello;
    hello.set_request_id(1);
    auto* h = hello.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->set_client_name("DevTool");
    h->mutable_auth()->set_client_id("dev-1");
    transport->injectMessage(serialize(hello));

    pb::ApiMessage challenge = parse(transport->sent.last());
    QCOMPARE(challenge.payload_case(), pb::ApiMessage::kAuthRequired);
    QByteArray nonce = QByteArray::fromStdString(challenge.auth_required().nonce());
    QCOMPARE(nonce.size(), 32);
    QCOMPARE(session.state(), ApiSession::State::AuthPending);

    // AuthResponse with the correct proof -> ServerHello, Ready.
    pb::ApiMessage resp;
    resp.set_request_id(2);
    auto* ar = resp.mutable_auth_response();
    ar->set_client_id("dev-1");
    QByteArray proof = hmacProof(secret, nonce);
    ar->set_proof(proof.constData(), proof.size());
    transport->injectMessage(serialize(resp));

    QCOMPARE(session.state(), ApiSession::State::Ready);
    QCOMPARE(parse(transport->sent.last()).payload_case(), pb::ApiMessage::kServerHello);
    QCOMPARE(session.clientId(), QString("dev-1"));
}

void TestApiSession::testRemoteAuthBadProof() {
    QByteArray secret(32, 'k');
    PairedClientStore store("/tmp/oap_test_session_badproof.yaml");
    PairedClient client;
    client.clientId = "dev-1";
    client.secret = secret;
    client.name = "DevTool";
    client.kind = pb::CLIENT_KIND_DIAGNOSTIC;
    store.upsert(client);

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.store = &store;
    ApiSession session(transport, deps);
    session.setPeerTrustOverrideForTest(false);
    QSignalSpy terminatedSpy(&session, &ApiSession::terminated);

    pb::ApiMessage hello;
    hello.set_request_id(1);
    auto* h = hello.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->mutable_auth()->set_client_id("dev-1");
    transport->injectMessage(serialize(hello));
    QCOMPARE(session.state(), ApiSession::State::AuthPending);

    // Wrong proof (derived against a different secret).
    QByteArray challengeNonce =
        QByteArray::fromStdString(parse(transport->sent.last()).auth_required().nonce());
    QByteArray badProof = hmacProof(QByteArray(32, 'x'), challengeNonce);
    pb::ApiMessage resp;
    resp.set_request_id(2);
    auto* ar = resp.mutable_auth_response();
    ar->set_client_id("dev-1");
    ar->set_proof(badProof.constData(), badProof.size());
    transport->injectMessage(serialize(resp));

    QCOMPARE(parse(transport->sent.last()).payload_case(), pb::ApiMessage::kAuthReject);
    QCOMPARE(terminatedSpy.count(), 1);
    QCOMPARE(session.state(), ApiSession::State::Closed);
}

void TestApiSession::testPairingFlow() {
    PairedClientStore store("/tmp/oap_test_session_pairing.yaml");
    QFile::remove("/tmp/oap_test_session_pairing.yaml");
    QVERIFY(store.load());
    PairingManager pairing(&store);
    QVERIFY(pairing.startWindow(60));

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.store = &store;
    deps.pairing = &pairing;
    deps.serverName = "HeadUnit";
    deps.appVersion = "1.0";
    ApiSession session(transport, deps);
    session.setPeerTrustOverrideForTest(false);

    // ClientHello requesting pairing -> PairingChallenge{nonce, salt}.
    pb::ApiMessage hello;
    hello.set_request_id(1);
    auto* h = hello.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->set_client_name("NewPhone");
    h->set_client_kind(pb::CLIENT_KIND_COMPANION);
    h->mutable_auth()->set_pairing_request(true);
    transport->injectMessage(serialize(hello));

    pb::ApiMessage challenge = parse(transport->sent.last());
    QCOMPARE(challenge.payload_case(), pb::ApiMessage::kPairingChallenge);
    QByteArray nonce = QByteArray::fromStdString(challenge.pairing_challenge().nonce());
    QByteArray salt = QByteArray::fromStdString(challenge.pairing_challenge().salt());
    QCOMPARE(nonce.size(), 32);
    QCOMPARE(salt, pairing.currentSalt());
    QCOMPARE(challenge.pairing_challenge().secret_format(),
             pb::PAIRING_SECRET_FORMAT_BASE32_120);
    QCOMPARE(session.state(), ApiSession::State::PairingPending);

    // Compute proof from the canonical displayed code + salt.
    QByteArray secret = deriveSecret(pairing.currentCode(), salt);
    QByteArray proof = hmacProof(secret, nonce);
    pb::ApiMessage resp;
    resp.set_request_id(2);
    resp.mutable_pairing_response()->set_proof(proof.constData(), proof.size());
    transport->injectMessage(serialize(resp));

    pb::ApiMessage hello2 = parse(transport->sent.last());
    QCOMPARE(hello2.payload_case(), pb::ApiMessage::kServerHello);
    QVERIFY(hello2.server_hello().has_granted_client_id());
    QString grantedId = QString::fromStdString(hello2.server_hello().granted_client_id());
    QVERIFY(!grantedId.isEmpty());
    QVERIFY(store.find(grantedId).has_value());
    QCOMPARE(session.state(), ApiSession::State::Ready);
}

// COMPANION WIRE CONTRACT: a pairing_request outside an open window (never
// opened, cancelled, or expired — all one branch) answers with the TYPED
// Error{ERROR_CODE_PAIRING_WINDOW_CLOSED, "Pairing window closed"} echoing
// the ClientHello's request_id, then closes the connection. The companion
// matches the code, not the message string.
void TestApiSession::testPairingWindowClosedTypedError() {
    PairedClientStore store("/tmp/oap_test_session_window_closed.yaml");
    QFile::remove("/tmp/oap_test_session_window_closed.yaml");
    PairingManager pairing(&store);   // window never opened = closed

    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.store = &store;
    deps.pairing = &pairing;
    deps.serverName = "HeadUnit";
    deps.appVersion = "1.0";
    ApiSession session(transport, deps);
    session.setPeerTrustOverrideForTest(false);
    QSignalSpy terminatedSpy(&session, &ApiSession::terminated);

    pb::ApiMessage hello;
    hello.set_request_id(7);
    auto* h = hello.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->set_client_name("NewPhone");
    h->set_client_kind(pb::CLIENT_KIND_COMPANION);
    h->mutable_auth()->set_pairing_request(true);
    transport->injectMessage(serialize(hello));

    pb::ApiMessage reject = parse(transport->sent.last());
    QCOMPARE(reject.payload_case(), pb::ApiMessage::kError);
    QCOMPARE(reject.request_id(), quint64(7));
    QCOMPARE(reject.error().code(), pb::ERROR_CODE_PAIRING_WINDOW_CLOSED);
    QCOMPARE(QString::fromStdString(reject.error().message()),
             QString("Pairing window closed"));
    QCOMPARE(terminatedSpy.count(), 1);
    QCOMPARE(session.state(), ApiSession::State::Closed);
}

// LIVE-BENCH REGRESSION (companion, 2026-07-13): the terminal Error frame
// must actually reach the wire. FakeTransport masked a real-socket bug: the
// frame was queued and teardown's QTcpSocket::close() discarded the
// still-unflushed write buffer in the same event-loop turn — a real client
// got EOF before any bytes. This test speaks REAL TCP end-to-end: the client
// must read the 4-byte prefix (00 00 00 1d) plus all 29 payload bytes of
// Error{PAIRING_WINDOW_CLOSED} (request_id=1) BEFORE the EOF.
void TestApiSession::testPairingWindowClosedRealTcpWire() {
    PairedClientStore store("/tmp/oap_test_session_wire_closed.yaml");
    QFile::remove("/tmp/oap_test_session_wire_closed.yaml");
    PairingManager pairing(&store);   // window never opened = closed

    QTcpServer srv;
    QVERIFY(srv.listen(QHostAddress::LocalHost, 0));
    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, srv.serverPort());
    QVERIFY(client.waitForConnected(3000));
    QVERIFY(srv.waitForNewConnection(3000));
    QTcpSocket* serverSide = srv.nextPendingConnection();
    QVERIFY(serverSide != nullptr);

    auto* transport = new TcpApiTransport(serverSide, 262144);
    ApiSessionDeps deps;
    deps.store = &store;
    deps.pairing = &pairing;
    deps.serverName = "HeadUnit";
    deps.appVersion = "1.0";
    // Mirror ApiServer::adoptSession EXACTLY: heap session + deleteLater on
    // terminated. The deferred delete races the socket's flush on the next
    // event-loop turn — destroying the socket mid-close aborts it and
    // discards the terminal frame; a stack session would mask that.
    auto* session = new ApiSession(transport, deps);
    session->setPeerTrustOverrideForTest(false);
    connect(session, &ApiSession::terminated, session,
            [session]() { session->deleteLater(); });

    pb::ApiMessage hello;
    hello.set_request_id(1);
    auto* h = hello.mutable_client_hello();
    h->set_requested_api_version_major(1);
    h->set_client_name("WireProbe");
    h->set_client_kind(pb::CLIENT_KIND_COMPANION);
    h->mutable_auth()->set_pairing_request(true);
    client.write(ApiFramer::encode(serialize(hello)));
    QVERIFY(client.waitForBytesWritten(3000));

    // Pump both endpoints until the full terminal frame arrives (33 bytes).
    QDeadlineTimer deadline(3000);
    while (client.bytesAvailable() < 33 && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        client.waitForReadyRead(10);
    }
    QCOMPARE(client.bytesAvailable(), qint64(33));

    const QByteArray prefix = client.read(4);
    QCOMPARE(prefix, QByteArray::fromHex("0000001d"));
    const QByteArray body = client.read(29);
    QCOMPARE(body.size(), 29);
    pb::ApiMessage reject;
    QVERIFY(reject.ParseFromArray(body.constData(), body.size()));
    QCOMPARE(reject.payload_case(), pb::ApiMessage::kError);
    QCOMPARE(reject.request_id(), quint64(1));
    QCOMPARE(reject.error().code(), pb::ERROR_CODE_PAIRING_WINDOW_CLOSED);
    QCOMPARE(QString::fromStdString(reject.error().message()),
             QString("Pairing window closed"));

    // ...and only THEN the EOF (server-initiated graceful close).
    deadline = QDeadlineTimer(3000);
    while (client.state() != QAbstractSocket::UnconnectedState
           && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        client.waitForReadyRead(10);
    }
    QCOMPARE(client.state(), QAbstractSocket::UnconnectedState);
    QCOMPARE(client.bytesAvailable(), qint64(0));
}

// abort() must produce exactly ONE closed() — Qt's own disconnected() and
// the transport's explicit emission funnel through a once-guard (duplicate
// lifecycle signals were only masked by the session's teardown idempotency).
void TestApiSession::testTransportAbortEmitsClosedExactlyOnce() {
    QTcpServer srv;
    QVERIFY(srv.listen(QHostAddress::LocalHost, 0));
    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, srv.serverPort());
    QVERIFY(client.waitForConnected(3000));
    QVERIFY(srv.waitForNewConnection(3000));
    QTcpSocket* serverSide = srv.nextPendingConnection();
    QVERIFY(serverSide != nullptr);

    TcpApiTransport transport(serverSide, 262144);
    QSignalSpy closedSpy(&transport, &IApiTransport::closed);

    transport.abort();
    for (int i = 0; i < 20; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);

    QCOMPARE(closedSpy.count(), 1);
}

void TestApiSession::testSubscribeSnapshotAndAck() {
    QByteArray cannedMedia("CANNED_MEDIA_SNAPSHOT");
    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.snapshotFor = [cannedMedia](pb::Topic t) -> QByteArray {
        return (t == pb::TOPIC_MEDIA) ? cannedMedia : QByteArray();
    };
    ApiSession session(transport, deps);

    // Reach Ready (trusted loopback).
    transport->injectMessage(clientHello(1));
    QCOMPARE(session.state(), ApiSession::State::Ready);

    pb::ApiMessage sub;
    sub.set_request_id(9);
    auto* sr = sub.mutable_subscribe_request();
    sr->add_topics(pb::TOPIC_MEDIA);
    sr->add_topics(pb::TOPIC_PHONE);
    transport->injectMessage(serialize(sub));

    // Last message is the canned snapshot; the one before it is SubscribeResponse.
    QCOMPARE(transport->sent.last(), cannedMedia);
    pb::ApiMessage subResp = parse(transport->sent.at(transport->sent.size() - 2));
    QCOMPARE(subResp.payload_case(), pb::ApiMessage::kSubscribeResponse);
    QCOMPARE(subResp.request_id(), quint64(9));
    QCOMPARE(subResp.subscribe_response().results_size(), 2);

    bool mediaAccepted = false, phoneRejected = false;
    for (const auto& r : subResp.subscribe_response().results()) {
        if (r.topic() == pb::TOPIC_MEDIA && r.accepted()) mediaAccepted = true;
        if (r.topic() == pb::TOPIC_PHONE && !r.accepted()) phoneRejected = true;
    }
    QVERIFY(mediaAccepted);
    QVERIFY(phoneRejected);
    QVERIFY(session.subscribedTo(pb::TOPIC_MEDIA));
    QVERIFY(!session.subscribedTo(pb::TOPIC_PHONE));
}

void TestApiSession::testQueueCapDisconnects() {
    QByteArray cannedMedia("CANNED_MEDIA_SNAPSHOT");
    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.maxQueueBytes = 4096;
    deps.snapshotFor = [cannedMedia](pb::Topic t) -> QByteArray {
        return (t == pb::TOPIC_MEDIA) ? cannedMedia : QByteArray();
    };
    ApiSession session(transport, deps);
    QSignalSpy terminatedSpy(&session, &ApiSession::terminated);

    transport->injectMessage(clientHello(1));
    QCOMPARE(session.state(), ApiSession::State::Ready);

    // Subscribe to MEDIA so deliver() is not dropped by the subscription gate.
    pb::ApiMessage sub;
    sub.set_request_id(1);
    sub.mutable_subscribe_request()->add_topics(pb::TOPIC_MEDIA);
    transport->injectMessage(serialize(sub));
    QVERIFY(session.subscribedTo(pb::TOPIC_MEDIA));

    // Now the outbound buffer is full: any further write must disconnect.
    transport->fakeBytesToWrite = deps.maxQueueBytes;

    pb::ApiMessage status;
    status.mutable_media_status();
    session.deliver(serialize(status));

    QCOMPARE(terminatedSpy.count(), 1);
    QCOMPARE(session.state(), ApiSession::State::Closed);
    // The kill must be the DISCARD path — a graceful close would wait
    // behind the very buffer the slow consumer isn't draining.
    QVERIFY(transport->aborted);
}

void TestApiSession::testPingPong() {
    auto* transport = new FakeTransport();
    ApiSession session(transport, ApiSessionDeps{});

    transport->injectMessage(clientHello(1));
    QCOMPARE(session.state(), ApiSession::State::Ready);

    transport->injectMessage(ping(7));

    pb::ApiMessage last = parse(transport->sent.last());
    QCOMPARE(last.payload_case(), pb::ApiMessage::kPong);
    QCOMPARE(last.request_id(), quint64(7));
}

void TestApiSession::testServerHelloCarriesServerId() {
    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.serverName = "HeadUnit";
    deps.appVersion = "1.2.3";
    deps.serverId = "hu-test-id";
    ApiSession session(transport, deps);

    transport->injectMessage(clientHello(1));

    pb::ApiMessage last = parse(transport->sent.last());
    QCOMPARE(last.payload_case(), pb::ApiMessage::kServerHello);
    QVERIFY(last.server_hello().has_server_id());
    QCOMPARE(QString::fromStdString(last.server_hello().server_id()), QString("hu-test-id"));
}

void TestApiSession::testServerHelloOmitsServerIdWhenEmpty() {
    auto* transport = new FakeTransport();
    ApiSessionDeps deps;
    deps.serverName = "HeadUnit";
    deps.appVersion = "1.2.3";
    // deps.serverId left empty (default).
    ApiSession session(transport, deps);

    transport->injectMessage(clientHello(1));

    pb::ApiMessage last = parse(transport->sent.last());
    QCOMPARE(last.payload_case(), pb::ApiMessage::kServerHello);
    QVERIFY(!last.server_hello().has_server_id());
}

void TestApiSession::testReentrantMessageThenClose() {
    // Transport delivers a message and then closes within one call stack.
    // The session must reach Ready, tear down cleanly, and emit terminated
    // exactly once without crashing.
    auto* transport = new FakeTransport();
    ApiSession session(transport, ApiSessionDeps{});
    QSignalSpy terminatedSpy(&session, &ApiSession::terminated);

    transport->injectMessageThenClose(clientHello(1));

    QCOMPARE(session.state(), ApiSession::State::Closed);
    QCOMPARE(terminatedSpy.count(), 1);
}

QTEST_MAIN(TestApiSession)
#include "test_api_session.moc"
