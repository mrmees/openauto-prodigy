#include <QtTest>
#include <QSignalSpy>
#include <QHostAddress>

#include "core/api/ApiSession.hpp"
#include "core/api/ApiTransport.hpp"
#include "core/api/ApiAuth.hpp"
#include "core/api/PairingManager.hpp"
#include "api/api.pb.h"

namespace pb = prodigy::api::v1;
using oap::api::ApiSession;
using oap::api::ApiSessionDeps;
using oap::api::IApiTransport;
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
    QHostAddress peerAddress() const override { return peer; }

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
    void testRemoteAuthHappyPath();
    void testRemoteAuthBadProof();
    void testPairingFlow();
    void testSubscribeSnapshotAndAck();
    void testQueueCapDisconnects();
    void testPingPong();
    void testReentrantMessageThenClose();
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
    PairingManager pairing(&store);
    pairing.startWindow(60);

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
    QCOMPARE(session.state(), ApiSession::State::PairingPending);

    // Compute proof from the displayed PIN + salt.
    QByteArray secret = deriveSecret(pairing.currentPin(), salt);
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
