#include <QTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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
        for (int i = 0; i < 10 && !client.bytesAvailable(); ++i) {
            QCoreApplication::processEvents();
            if (!client.bytesAvailable())
                client.waitForReadyRead(1000);
        }
        QVERIFY(client.bytesAvailable() > 0);
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

    void vehicleIdGeneratedAndPersisted()
    {
        QString idPath = QDir::homePath() + "/.openauto/vehicle.id";
        QFile::remove(idPath);  // clean slate

        oap::CompanionListenerService svc;
        QVERIFY(svc.vehicleId().isEmpty());

        svc.loadOrGenerateVehicleId();
        QString id1 = svc.vehicleId();
        QVERIFY(!id1.isEmpty());
        // UUID v4 format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx (36 chars)
        QCOMPARE(id1.length(), 36);
        QVERIFY(id1.contains('-'));

        // Second instance loads the same ID from disk
        oap::CompanionListenerService svc2;
        svc2.loadOrGenerateVehicleId();
        QCOMPARE(svc2.vehicleId(), id1);

        // Clean up
        QFile::remove(idPath);
    }

    void challengeContainsVehicleId()
    {
        QString idPath = QDir::homePath() + "/.openauto/vehicle.id";
        QFile::remove(idPath);

        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret-key");
        svc.loadOrGenerateVehicleId();
        svc.start(19878);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19878);
        QVERIFY(client.waitForConnected(2000));

        // Pump events so server accepts the connection and sends challenge
        for (int i = 0; i < 10 && !client.bytesAvailable(); ++i) {
            QCoreApplication::processEvents();
            if (!client.bytesAvailable())
                client.waitForReadyRead(200);
        }
        QVERIFY(client.bytesAvailable() > 0);
        QByteArray line = client.readLine().trimmed();
        QJsonObject challenge = QJsonDocument::fromJson(line).object();

        QVERIFY(challenge.contains("vehicle_id"));
        QCOMPARE(challenge["vehicle_id"].toString(), svc.vehicleId());

        svc.stop();
        QFile::remove(idPath);
    }
};

QTEST_MAIN(TestCompanionListener)
#include "test_companion_listener.moc"
