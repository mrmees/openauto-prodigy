#include <QtTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocketServer>
#include <QWebSocket>
#include "core/api/ApiTransport.hpp"
#include "core/api/ApiFramer.hpp"

using oap::api::IApiTransport;
using oap::api::TcpApiTransport;
using oap::api::WsApiTransport;
using oap::api::ApiFramer;

namespace {
constexpr quint32 kMaxFrameBytes = 262144;
}

class TestApiTransports : public QObject {
    Q_OBJECT
private slots:
    void testTcpRoundTrip();
    void testTcpSplitDelivery();
    void testTcpFramingViolationCloses();
    void testWsRoundTrip();
    void testWsTextFrameCloses();
    void testWsOversizedMessageCloses();
};

void TestApiTransports::testTcpRoundTrip() {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    quint16 port = server.serverPort();

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(client.waitForConnected(3000));
    QVERIFY(server.waitForNewConnection(3000));

    QTcpSocket* serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket);
    TcpApiTransport transport(serverSocket, kMaxFrameBytes);

    QSignalSpy messageSpy(&transport, &IApiTransport::messageReceived);

    client.write(ApiFramer::encode("payload"));
    QTRY_COMPARE(messageSpy.count(), 1);
    QCOMPARE(messageSpy.at(0).at(0).toByteArray(), QByteArray("payload"));

    transport.sendMessage("reply");
    QTRY_VERIFY(client.bytesAvailable() >= 9);
    QByteArray received = client.readAll();
    QCOMPARE(received, ApiFramer::encode("reply"));
}

void TestApiTransports::testTcpSplitDelivery() {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    quint16 port = server.serverPort();

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(client.waitForConnected(3000));
    QVERIFY(server.waitForNewConnection(3000));

    QTcpSocket* serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket);
    TcpApiTransport transport(serverSocket, kMaxFrameBytes);

    QSignalSpy messageSpy(&transport, &IApiTransport::messageReceived);

    QByteArray frame = ApiFramer::encode("splitpayload");
    int half = frame.size() / 2;
    client.write(frame.left(half));
    QTest::qWait(50);
    QCOMPARE(messageSpy.count(), 0);
    client.write(frame.mid(half));

    QTRY_COMPARE(messageSpy.count(), 1);
    QCOMPARE(messageSpy.at(0).at(0).toByteArray(), QByteArray("splitpayload"));
}

void TestApiTransports::testTcpFramingViolationCloses() {
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    quint16 port = server.serverPort();

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY(client.waitForConnected(3000));
    QVERIFY(server.waitForNewConnection(3000));

    QTcpSocket* serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket);
    TcpApiTransport transport(serverSocket, kMaxFrameBytes);

    QSignalSpy closedSpy(&transport, &IApiTransport::closed);

    // Zero-length prefix is a framing violation.
    QByteArray zeroPrefix(4, char(0));
    client.write(zeroPrefix);

    QTRY_COMPARE(closedSpy.count(), 1);
}

void TestApiTransports::testWsRoundTrip() {
    QWebSocketServer server(QStringLiteral("t"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    quint16 port = server.serverPort();

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    client.open(QUrl(QString("ws://127.0.0.1:%1").arg(port)));

    QTRY_VERIFY(server.hasPendingConnections());
    QWebSocket* serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket);
    WsApiTransport transport(serverSocket, kMaxFrameBytes);

    QTRY_COMPARE(clientConnectedSpy.count(), 1);

    QSignalSpy messageSpy(&transport, &IApiTransport::messageReceived);
    client.sendBinaryMessage("payload");
    QTRY_COMPARE(messageSpy.count(), 1);
    QCOMPARE(messageSpy.at(0).at(0).toByteArray(), QByteArray("payload"));

    QSignalSpy clientBinarySpy(&client, &QWebSocket::binaryMessageReceived);
    transport.sendMessage("reply");
    QTRY_COMPARE(clientBinarySpy.count(), 1);
    QCOMPARE(clientBinarySpy.at(0).at(0).toByteArray(), QByteArray("reply"));
}

void TestApiTransports::testWsTextFrameCloses() {
    QWebSocketServer server(QStringLiteral("t"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    quint16 port = server.serverPort();

    QWebSocket client;
    client.open(QUrl(QString("ws://127.0.0.1:%1").arg(port)));

    QTRY_VERIFY(server.hasPendingConnections());
    QWebSocket* serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket);
    WsApiTransport transport(serverSocket, kMaxFrameBytes);

    QSignalSpy closedSpy(&transport, &IApiTransport::closed);
    client.sendTextMessage("nope");

    QTRY_COMPARE(closedSpy.count(), 1);
}

void TestApiTransports::testWsOversizedMessageCloses() {
    QWebSocketServer server(QStringLiteral("t"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    quint16 port = server.serverPort();

    QWebSocket client;
    client.open(QUrl(QString("ws://127.0.0.1:%1").arg(port)));

    QTRY_VERIFY(server.hasPendingConnections());
    QWebSocket* serverSocket = server.nextPendingConnection();
    QVERIFY(serverSocket);
    WsApiTransport transport(serverSocket, kMaxFrameBytes);

    QSignalSpy closedSpy(&transport, &IApiTransport::closed);
    QSignalSpy messageSpy(&transport, &IApiTransport::messageReceived);

    // One byte over the cap. The ctor-level setMaxAllowedIncomingMessageSize()
    // rejects this before Qt ever buffers the full message; the pre-existing
    // onBinaryMessageReceived() size check is a second line of defense that
    // would also close on this same input (see report for the RED caveat).
    const QByteArray oversized(int(kMaxFrameBytes) + 1, 'x');
    client.sendBinaryMessage(oversized);

    QTRY_COMPARE(closedSpy.count(), 1);
    QCOMPARE(messageSpy.count(), 0);
}

QTEST_MAIN(TestApiTransports)
#include "test_api_transports.moc"
