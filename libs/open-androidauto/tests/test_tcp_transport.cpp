#include <QtTest/QtTest>
#include <QTcpServer>
#include <QSignalSpy>
#include <oaa/Transport/TCPTransport.hpp>

class TestTCPTransport : public QObject {
    Q_OBJECT
private slots:
    void testConnectAndSend()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        quint16 port = server.serverPort();

        oaa::TCPTransport transport;
        QSignalSpy connectedSpy(&transport, &oaa::ITransport::connected);
        QSignalSpy dataReceivedSpy(&transport, &oaa::ITransport::dataReceived);

        transport.connectToHost(QHostAddress::LocalHost, port);
        transport.start();

        // Wait for server to accept connection
        QVERIFY(server.waitForNewConnection(3000));
        QTcpSocket* serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket);

        // Wait for connected signal
        QVERIFY(connectedSpy.wait(3000));
        QCOMPARE(connectedSpy.count(), 1);
        QVERIFY(transport.isConnected());

        // Client writes "hello", server receives it
        transport.write("hello");
        QCoreApplication::processEvents();
        QVERIFY(serverSocket->waitForReadyRead(3000));
        QCOMPARE(serverSocket->readAll(), QByteArray("hello"));

        // Server writes "world", client receives it
        serverSocket->write("world");
        QVERIFY(dataReceivedSpy.wait(3000));
        QCOMPARE(dataReceivedSpy.count(), 1);
        QCOMPARE(dataReceivedSpy.at(0).at(0).toByteArray(), QByteArray("world"));
    }

    void testDisconnect()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        quint16 port = server.serverPort();

        oaa::TCPTransport transport;
        QSignalSpy connectedSpy(&transport, &oaa::ITransport::connected);
        QSignalSpy disconnectedSpy(&transport, &oaa::ITransport::disconnected);

        transport.connectToHost(QHostAddress::LocalHost, port);
        transport.start();

        QVERIFY(server.waitForNewConnection(3000));
        QTcpSocket* serverSocket = server.nextPendingConnection();
        QVERIFY(serverSocket);
        QVERIFY(connectedSpy.wait(3000));
        QVERIFY(transport.isConnected());

        // Server closes connection
        serverSocket->close();
        QVERIFY(disconnectedSpy.wait(3000));
        QCOMPARE(disconnectedSpy.count(), 1);
        QVERIFY(!transport.isConnected());
    }
};

QTEST_MAIN(TestTCPTransport)
#include "test_tcp_transport.moc"
