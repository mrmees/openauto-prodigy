#include <QtTest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QWebSocket>
#include <QHostAddress>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QRegularExpression>
#include <QFile>

#include "core/api/ApiServer.hpp"
#include "core/api/ApiFramer.hpp"

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
// In-file wire helpers. TCP frames are length-prefixed (ApiFramer); WS frames
// are one serialized ApiMessage per binary message.
// -----------------------------------------------------------------------------
namespace {

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

} // namespace

// -----------------------------------------------------------------------------
class TestApiServer : public QObject {
    Q_OBJECT

    // Fresh service set per test. navigation/projection/bluetooth null.
    struct Fixture {
        oap::YamlConfig yaml;
        oap::ConfigService config{&yaml, "/tmp/oap_test_api_server.yaml"};
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
    void testStartsAndBindsEphemeral();
    void testTcpEndToEndHelloSubscribe();
    void testWsEndToEnd();
    void testDisabledDoesNotListen();
    void testPairingActionRegistered();
    void testStopCancelsPairingWindow();
    void testServerIdMintedAndStable();
    void testDoubleStartIsIdempotentNoOp();
    void testCorruptClientStoreDisablesPairing();
};

void TestApiServer::testStartsAndBindsEphemeral() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    ApiServer server(f.refs());
    QVERIFY(server.start());
    QVERIFY(server.tcpPort() != 0);
    QVERIFY(server.wsPort() != 0);
    server.stop();
}

void TestApiServer::testCorruptClientStoreDisablesPairing() {
    const QString path = "/tmp/oap_test_api_server_corrupt_clients.yaml";
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write("clients: [\n"), qint64(11));
    file.close();

    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    ApiServer server(f.refs());
    server.setStorePathForTest(path);
    QVERIFY(server.start());
    server.startPairing();
    QVERIFY(!server.pairingActive());
    QVERIFY(server.pairingCode().isEmpty());
    server.stop();
}

void TestApiServer::testTcpEndToEndHelloSubscribe() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    // Give media a live Bluetooth source so snapshots/deltas carry content.
    f.media.setBtConnected(true);
    f.media.updateBtMetadata("SnapshotSong", "Artist", "Album");

    ApiServer server(f.refs());
    server.setStorePathForTest("/tmp/oap_test_api_server_clients.yaml");
    QVERIFY(server.start());

    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, server.tcpPort());
    QVERIFY(sock.waitForConnected(3000));

    ApiFramer framer;
    QList<QByteArray> queue;

    // ClientHello -> ServerHello (localhost is trusted, no auth).
    sendFramed(sock, clientHello(1));
    pb::ApiMessage hello = readFramed(sock, framer, queue);
    QCOMPARE(hello.payload_case(), pb::ApiMessage::kServerHello);
    const pb::Capabilities& caps = hello.server_hello().capabilities();
    QVERIFY(caps.has_secure_pairing_code());
    QVERIFY(caps.secure_pairing_code());
    QVERIFY(capabilitiesHaveTopic(caps, pb::TOPIC_MEDIA));
    QVERIFY(!capabilitiesHaveTopic(caps, pb::TOPIC_NAVIGATION));

    // Subscribe(MEDIA) -> SubscribeResponse, then a MediaStatus snapshot.
    sendFramed(sock, subscribe(2, pb::TOPIC_MEDIA));
    pb::ApiMessage subResp = readFramed(sock, framer, queue);
    QCOMPARE(subResp.payload_case(), pb::ApiMessage::kSubscribeResponse);
    QCOMPARE(subResp.subscribe_response().results_size(), 1);
    QVERIFY(subResp.subscribe_response().results(0).accepted());

    pb::ApiMessage snapshot = readFramed(sock, framer, queue);
    QCOMPARE(snapshot.payload_case(), pb::ApiMessage::kMediaStatus);
    QCOMPARE(snapshot.request_id(), quint64(0));
    QCOMPARE(QString::fromStdString(snapshot.media_status().title()),
             QString("SnapshotSong"));

    // A provider change fans out a full-snapshot delta to the subscriber.
    f.media.updateBtMetadata("DeltaSong", "Artist2", "Album2");
    pb::ApiMessage delta = readFramed(sock, framer, queue);
    QCOMPARE(delta.payload_case(), pb::ApiMessage::kMediaStatus);
    QCOMPARE(QString::fromStdString(delta.media_status().title()),
             QString("DeltaSong"));

    server.stop();
}

void TestApiServer::testWsEndToEnd() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    f.media.setBtConnected(true);
    f.media.updateBtMetadata("SnapshotSong", "Artist", "Album");

    ApiServer server(f.refs());
    server.setStorePathForTest("/tmp/oap_test_api_server_clients.yaml");
    QVERIFY(server.start());

    QWebSocket client;
    QSignalSpy binarySpy(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.wsPort())));
    QSignalSpy connectedSpy(&client, &QWebSocket::connected);
    QTRY_COMPARE(connectedSpy.count(), 1);

    // ClientHello -> ServerHello.
    client.sendBinaryMessage(serialize(clientHello(1)));
    QTRY_COMPARE(binarySpy.count(), 1);
    pb::ApiMessage hello;
    hello.ParseFromArray(binarySpy.at(0).at(0).toByteArray().constData(),
                         binarySpy.at(0).at(0).toByteArray().size());
    QCOMPARE(hello.payload_case(), pb::ApiMessage::kServerHello);
    QVERIFY(capabilitiesHaveTopic(hello.server_hello().capabilities(),
                                  pb::TOPIC_MEDIA));

    // Subscribe -> SubscribeResponse + snapshot (2 more frames).
    client.sendBinaryMessage(serialize(subscribe(2, pb::TOPIC_MEDIA)));
    QTRY_COMPARE(binarySpy.count(), 3);
    pb::ApiMessage snapshot;
    const QByteArray snapBytes = binarySpy.at(2).at(0).toByteArray();
    snapshot.ParseFromArray(snapBytes.constData(), snapBytes.size());
    QCOMPARE(snapshot.payload_case(), pb::ApiMessage::kMediaStatus);

    // Delta.
    f.media.updateBtMetadata("DeltaSong", "Artist2", "Album2");
    QTRY_COMPARE(binarySpy.count(), 4);
    pb::ApiMessage delta;
    const QByteArray deltaBytes = binarySpy.at(3).at(0).toByteArray();
    delta.ParseFromArray(deltaBytes.constData(), deltaBytes.size());
    QCOMPARE(delta.payload_case(), pb::ApiMessage::kMediaStatus);
    QCOMPARE(QString::fromStdString(delta.media_status().title()),
             QString("DeltaSong"));

    client.close();
    server.stop();
}

void TestApiServer::testDisabledDoesNotListen() {
    Fixture f;
    f.config.setValue("api.enabled", false);
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    ApiServer server(f.refs());
    QVERIFY(!server.start());
    QCOMPARE(server.sessionCount(), 0);
}

void TestApiServer::testPairingActionRegistered() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    ApiServer server(f.refs());
    server.setStorePathForTest("/tmp/oap_test_api_server_clients.yaml");

    // Actions register in the ctor (dispatch works pre-start)...
    QVERIFY(f.actions.registeredActions().contains(QStringLiteral("api.pairing.start")));
    QVERIFY(f.actions.registeredActions().contains(QStringLiteral("api.pairing.cancel")));

    // ...but a window only OPENS on a running server — no listener means a
    // Code with nothing to pair through (2026-07-14 settings-merge finding).
    QVERIFY(!server.pairingActive());
    QVERIFY(f.actions.dispatch(QStringLiteral("api.pairing.start")));
    QVERIFY(!server.pairingActive());

    QVERIFY(server.start());
    QVERIFY(f.actions.dispatch(QStringLiteral("api.pairing.start")));
    QVERIFY(server.pairingActive());

    const QString code = server.pairingCode();
    QCOMPARE(code.size(), 29);  // 24 characters plus five separators
    QCOMPARE(code.count(QLatin1Char('-')), 5);

    QVERIFY(f.actions.dispatch(QStringLiteral("api.pairing.cancel")));
    QVERIFY(!server.pairingActive());
    server.stop();
}

void TestApiServer::testStopCancelsPairingWindow() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    ApiServer server(f.refs());
    server.setStorePathForTest("/tmp/oap_test_api_server_clients.yaml");

    QVERIFY(server.start());
    server.startPairing();
    QVERIFY(server.pairingActive());
    QVERIFY(!server.pairingCode().isEmpty());

    // stop() must close the window (stale-code hazard) and notify QML so the
    // displayed code/QR clear (2026-07-14 pre-merge gate finding).
    QSignalSpy pairingSpy(&server, &ApiServer::pairingChanged);
    server.stop();
    QVERIFY(!server.pairingActive());
    QVERIFY(server.pairingCode().isEmpty());
    QVERIFY(pairingSpy.count() >= 1);

    // A restart within the old window's timeout must NOT resurrect it.
    QVERIFY(server.start());
    QVERIFY(!server.pairingActive());
    QVERIFY(server.pairingCode().isEmpty());
    server.stop();
}

void TestApiServer::testServerIdMintedAndStable() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);

    QVERIFY(f.config.value("identity.server_id").toString().isEmpty());

    ApiServer server(f.refs());
    server.setStorePathForTest("/tmp/oap_test_api_server_clients.yaml");
    QVERIFY(server.start());

    const QString mintedId = f.config.value("identity.server_id").toString();
    static const QRegularExpression kUuidRe(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    QVERIFY2(kUuidRe.match(mintedId).hasMatch(),
             qPrintable(QString("server_id '%1' is not a UUID").arg(mintedId)));

    server.stop();

    // A fresh ApiServer against the SAME (in-memory) config reuses the
    // already-minted id -- never re-mints on a subsequent start().
    ApiServer server2(f.refs());
    server2.setStorePathForTest("/tmp/oap_test_api_server_clients.yaml");
    QVERIFY(server2.start());

    QCOMPARE(f.config.value("identity.server_id").toString(), mintedId);
    server2.stop();
}

void TestApiServer::testDoubleStartIsIdempotentNoOp() {
    Fixture f;
    f.config.setValue("api.tcp_port", 0);
    f.config.setValue("api.ws_port", 0);
    f.media.setBtConnected(true);
    f.media.updateBtMetadata("SnapshotSong", "Artist", "Album");

    ApiServer server(f.refs());
    server.setStorePathForTest("/tmp/oap_test_api_server_clients.yaml");
    QVERIFY(server.start());
    const quint16 tcpPort = server.tcpPort();
    const quint16 wsPort = server.wsPort();

    // Second start() must be an idempotent no-op: same ports (a rebuilt
    // listener bound to config port 0 would pick a NEW ephemeral port), and
    // no duplicate publishers/listeners left behind.
    QVERIFY(server.start());
    QCOMPARE(server.tcpPort(), tcpPort);
    QCOMPARE(server.wsPort(), wsPort);

    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, tcpPort);
    QVERIFY(sock.waitForConnected(3000));

    ApiFramer framer;
    QList<QByteArray> queue;

    sendFramed(sock, clientHello(1));
    pb::ApiMessage hello = readFramed(sock, framer, queue);
    QCOMPARE(hello.payload_case(), pb::ApiMessage::kServerHello);

    sendFramed(sock, subscribe(2, pb::TOPIC_MEDIA));
    pb::ApiMessage subResp = readFramed(sock, framer, queue);
    QCOMPARE(subResp.payload_case(), pb::ApiMessage::kSubscribeResponse);
    pb::ApiMessage snapshot = readFramed(sock, framer, queue);
    QCOMPARE(snapshot.payload_case(), pb::ApiMessage::kMediaStatus);

    // A single provider update must fan out exactly ONE delta. Pre-fix, a
    // duplicate start() appends a second MediaPublisher wired to the same
    // provider, so this update would be delivered twice.
    f.media.updateBtMetadata("DeltaSong", "Artist2", "Album2");
    pb::ApiMessage delta = readFramed(sock, framer, queue);
    QCOMPARE(delta.payload_case(), pb::ApiMessage::kMediaStatus);
    QCOMPARE(QString::fromStdString(delta.media_status().title()),
             QString("DeltaSong"));

    // Drain anything else that shows up briefly, then confirm nothing did:
    // no second (duplicate) delta.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
    if (sock.bytesAvailable() > 0)
        queue.append(framer.feed(sock.readAll()));
    QCOMPARE(queue.size(), 0);

    server.stop();

    // Restart path: stop() clears started_, so a fresh start() must succeed.
    QVERIFY(server.start());
    QVERIFY(server.tcpPort() != 0);
    server.stop();
}

QTEST_MAIN(TestApiServer)
#include "test_api_server.moc"
