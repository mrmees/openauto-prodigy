#include <QTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include "core/services/CompanionListenerService.hpp"

class TestCompanionListener : public QObject {
    Q_OBJECT
private slots:
    void constructionDoesNotCrash()
    {
        oap::CompanionListenerService svc;
        QVERIFY(!svc.isListening());
    }

    void startListeningOnPort()
    {
        oap::CompanionListenerService svc;
        QVERIFY(svc.start(19876));  // test port
        QVERIFY(svc.isListening());
        svc.stop();
        QVERIFY(!svc.isListening());
    }

    void rejectsConnectionWithoutAuth()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret-key");
        svc.start(19877);

        // Connect a raw TCP client
        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19877);
        QVERIFY(client.waitForConnected(1000));

        // Process events so the server sees the new connection
        QCoreApplication::processEvents();

        // Should receive a challenge
        QVERIFY(client.waitForReadyRead(2000));
        QByteArray challenge = client.readLine();
        QVERIFY(challenge.contains("\"type\":\"challenge\""));
        QVERIFY(challenge.contains("\"nonce\""));

        // Send garbage hello — should be rejected
        client.write("{\"type\":\"hello\",\"token\":\"bad\"}\n");
        client.flush();

        // Process events so the server reads and responds
        QCoreApplication::processEvents();

        QVERIFY(client.waitForReadyRead(2000));
        QByteArray response = client.readLine();
        QVERIFY(response.contains("\"accepted\":false"));

        svc.stop();
    }
};

QTEST_MAIN(TestCompanionListener)
#include "test_companion_listener.moc"
