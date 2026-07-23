#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "core/services/SystemServiceClient.hpp"

using namespace oap;

class TestSystemServiceClient : public QObject {
    Q_OBJECT

    static QJsonObject takeRequest(QLocalSocket* peer)
    {
        if (!peer->canReadLine())
            return {};
        return QJsonDocument::fromJson(peer->readLine().trimmed()).object();
    }

    static void sendResult(QLocalSocket* peer, const QString& id,
                           const QJsonObject& result)
    {
        const QJsonObject response{{QStringLiteral("id"), id},
                                   {QStringLiteral("result"), result}};
        peer->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
        peer->flush();
    }

private slots:
    void recoversWhenServerAppears()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString socketPath = directory.filePath(QStringLiteral("system.sock"));

        SystemServiceClient client(socketPath, 20);
        QSignalSpy connectedSpy(&client, &SystemServiceClient::connectedChanged);

        // Let the initial connection fail before making the daemon available.
        QTest::qWait(60);
        QVERIFY(!client.isConnected());

        QLocalServer server;
        QVERIFY2(server.listen(socketPath), qPrintable(server.errorString()));

        QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1000);
        QCOMPARE(connectedSpy.count(), 1);

        QLocalSocket* peer = server.nextPendingConnection();
        QVERIFY(peer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(peer->canReadLine(), 1000);
        const QJsonObject request = takeRequest(peer);
        QCOMPARE(request.value(QStringLiteral("method")).toString(),
                 QStringLiteral("get_health"));

        const QJsonObject expectedHealth{{QStringLiteral("daemon"),
                                          QStringLiteral("ready")}};
        QSignalSpy healthSpy(&client, &SystemServiceClient::healthChanged);
        sendResult(peer, request.value(QStringLiteral("id")).toString(), expectedHealth);
        QTRY_COMPARE_WITH_TIMEOUT(healthSpy.count(), 1, 1000);
        QCOMPARE(client.health(), expectedHealth);
    }

    void reconnectsAfterServerDisconnect()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString socketPath = directory.filePath(QStringLiteral("system.sock"));

        QLocalServer server;
        QVERIFY2(server.listen(socketPath), qPrintable(server.errorString()));
        SystemServiceClient client(socketPath, 20);
        QSignalSpy connectedSpy(&client, &SystemServiceClient::connectedChanged);

        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1000);
        QLocalSocket* firstPeer = server.nextPendingConnection();
        QVERIFY(firstPeer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 1000);

        firstPeer->disconnectFromServer();
        QTRY_VERIFY_WITH_TIMEOUT(!client.isConnected(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1000);
        QLocalSocket* secondPeer = server.nextPendingConnection();
        QVERIFY(secondPeer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 1000);
        // The initial local connection can complete synchronously during the
        // client constructor, before the spy is installed. The observed
        // disconnect and reconnect edges must both still be delivered.
        QVERIFY(connectedSpy.count() >= 2);
    }

    void terminalErrorsShareOneRetryAndAbortTheSocket()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString socketPath = directory.filePath(QStringLiteral("system.sock"));

        QLocalServer server;
        QVERIFY2(server.listen(socketPath), qPrintable(server.errorString()));
        SystemServiceClient client(socketPath, 40);
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1000);
        QLocalSocket* firstPeer = server.nextPendingConnection();
        QVERIFY(firstPeer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 1000);

        const auto timers = client.findChildren<QTimer*>();
        QCOMPARE(timers.size(), 1);
        QTimer* retryTimer = timers.front();
        QVERIFY(!retryTimer->isActive());

        // SocketTimeoutError was not retried by the old selective policy. A
        // terminal error on an otherwise ambiguous socket is aborted first.
        QVERIFY(QMetaObject::invokeMethod(
            &client, "onError", Qt::DirectConnection,
            Q_ARG(QLocalSocket::LocalSocketError, QLocalSocket::SocketTimeoutError)));
        QVERIFY(!client.isConnected());
        QVERIFY(retryTimer->isActive());

        // Error and disconnect delivery can overlap; both must converge on the
        // same active timer instead of scheduling independent attempts.
        QVERIFY(QMetaObject::invokeMethod(
            &client, "onError", Qt::DirectConnection,
            Q_ARG(QLocalSocket::LocalSocketError, QLocalSocket::SocketAccessError)));
        QVERIFY(QMetaObject::invokeMethod(&client, "onDisconnected",
                                          Qt::DirectConnection));
        QCOMPARE(client.findChildren<QTimer*>().size(), 1);
        QVERIFY(retryTimer->isActive());

        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1000);
        QLocalSocket* secondPeer = server.nextPendingConnection();
        QVERIFY(secondPeer != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 1000);
        QVERIFY(!retryTimer->isActive());

        // No duplicate timer is left behind to create another connection.
        QTest::qWait(120);
        QVERIFY(!server.hasPendingConnections());
    }
};

QTEST_GUILESS_MAIN(TestSystemServiceClient)
#include "test_system_service_client.moc"
