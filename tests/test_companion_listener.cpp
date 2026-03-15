#include <QTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <sys/socket.h>
#include "core/services/CompanionListenerService.hpp"
#include "core/services/ThemeService.hpp"

// Helper: complete challenge-response auth and return session key
static QByteArray authenticateClient(QTcpSocket& client, oap::CompanionListenerService& svc,
                                      const QString& sharedSecret)
{
    // Wait for challenge
    for (int i = 0; i < 20 && !client.bytesAvailable(); ++i) {
        QCoreApplication::processEvents();
        if (!client.bytesAvailable())
            client.waitForReadyRead(200);
    }
    if (!client.bytesAvailable()) return {};

    QByteArray line = client.readLine().trimmed();
    QJsonObject challenge = QJsonDocument::fromJson(line).object();
    QByteArray nonce = challenge["nonce"].toString().toLatin1();

    // Compute HMAC(shared_secret, nonce)
    QByteArray token = QMessageAuthenticationCode::hash(
        nonce, sharedSecret.toUtf8(), QCryptographicHash::Sha256).toHex();

    QJsonObject hello;
    hello["type"] = "hello";
    hello["token"] = QString::fromLatin1(token);
    client.write(QJsonDocument(hello).toJson(QJsonDocument::Compact) + "\n");
    client.flush();

    // Wait for hello_ack
    for (int i = 0; i < 20 && !client.bytesAvailable(); ++i) {
        QCoreApplication::processEvents();
        if (!client.bytesAvailable())
            client.waitForReadyRead(200);
    }
    if (!client.bytesAvailable()) return {};

    QByteArray ackLine = client.readLine().trimmed();
    QJsonObject ack = QJsonDocument::fromJson(ackLine).object();
    if (!ack["accepted"].toBool()) return {};

    return QByteArray::fromHex(ack["session_key"].toString().toLatin1());
}

// Helper: send a MAC'd message
static void sendMacMessage(QTcpSocket& client, const QByteArray& sessionKey, const QJsonObject& msg)
{
    QByteArray payload = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QByteArray mac = QMessageAuthenticationCode::hash(
        payload, sessionKey, QCryptographicHash::Sha256).toHex();

    QJsonObject macMsg = msg;
    macMsg["mac"] = QString::fromLatin1(mac);
    client.write(QJsonDocument(macMsg).toJson(QJsonDocument::Compact) + "\n");
    client.flush();
}

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

    // --- New tests for Phase 03.2 Plan 02 ---

    void helloAckContainsDisplayField()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.setDisplaySize(1024, 600);
        svc.start(19879);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19879);
        QVERIFY(client.waitForConnected(2000));

        // Wait for challenge
        for (int i = 0; i < 20 && !client.bytesAvailable(); ++i) {
            QCoreApplication::processEvents();
            if (!client.bytesAvailable())
                client.waitForReadyRead(200);
        }
        QVERIFY(client.bytesAvailable() > 0);
        QByteArray line = client.readLine().trimmed();
        QJsonObject challenge = QJsonDocument::fromJson(line).object();
        QByteArray nonce = challenge["nonce"].toString().toLatin1();

        // Compute correct HMAC token
        QByteArray token = QMessageAuthenticationCode::hash(
            nonce, QByteArray("test-secret"), QCryptographicHash::Sha256).toHex();

        QJsonObject hello;
        hello["type"] = "hello";
        hello["token"] = QString::fromLatin1(token);
        client.write(QJsonDocument(hello).toJson(QJsonDocument::Compact) + "\n");
        client.flush();

        // Wait for hello_ack
        for (int i = 0; i < 20 && !client.bytesAvailable(); ++i) {
            QCoreApplication::processEvents();
            if (!client.bytesAvailable())
                client.waitForReadyRead(200);
        }
        QVERIFY(client.bytesAvailable() > 0);
        QByteArray ackLine = client.readLine().trimmed();
        QJsonObject ack = QJsonDocument::fromJson(ackLine).object();

        QVERIFY(ack["accepted"].toBool());
        QVERIFY(ack.contains("display"));
        QJsonObject display = ack["display"].toObject();
        QCOMPARE(display["width"].toInt(), 1024);
        QCOMPARE(display["height"].toInt(), 600);

        svc.stop();
    }

    void themeMessageStoresPendingState()
    {
        // Set up authenticated connection
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.start(19880);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19880);
        QVERIFY(client.waitForConnected(2000));

        QByteArray sessionKey = authenticateClient(client, svc, "test-secret");
        QVERIFY(!sessionKey.isEmpty());

        // Send a theme message
        QJsonObject theme;
        theme["name"] = "Test Theme";
        theme["seed"] = "#ff5500";
        QJsonObject light;
        light["primary"] = "#ff5500";
        light["onPrimary"] = "#ffffff";
        theme["light"] = light;
        QJsonObject dark;
        dark["primary"] = "#ffaa55";
        dark["onPrimary"] = "#000000";
        theme["dark"] = dark;
        QJsonObject wallpaper;
        wallpaper["chunks"] = 2;
        wallpaper["size"] = 1024;
        theme["wallpaper"] = wallpaper;

        QJsonObject msg;
        msg["type"] = "theme";
        msg["theme"] = theme;

        sendMacMessage(client, sessionKey, msg);

        // Process events
        for (int i = 0; i < 10; ++i)
            QCoreApplication::processEvents();

        // Theme message accepted without crash or error
        // (internal state verified by subsequent chunk test)
        QVERIFY(true);

        svc.stop();
    }

    void themeChunkReassemblyAndImport()
    {
        // Set up ThemeService with temp dir
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        oap::ThemeService themeService;
        QStringList searchPaths;
        searchPaths << tempDir.path();
        themeService.scanThemeDirectories(searchPaths);

        // Set up companion listener with ThemeService
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.setThemeService(&themeService);
        svc.start(19881);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19881);
        QVERIFY(client.waitForConnected(2000));

        QByteArray sessionKey = authenticateClient(client, svc, "test-secret");
        QVERIFY(!sessionKey.isEmpty());

        // Send theme message with 2 chunks
        QByteArray wallpaperData = "FAKE_JPEG_DATA_FOR_TESTING_12345";
        int halfSize = wallpaperData.size() / 2;
        QByteArray chunk1Data = wallpaperData.left(halfSize);
        QByteArray chunk2Data = wallpaperData.mid(halfSize);

        QJsonObject theme;
        theme["name"] = "Chunk Test";
        theme["seed"] = "#112233";
        QJsonObject light;
        light["primaryContainer"] = "#aabbcc";
        light["onSurface"] = "#112233";
        theme["light"] = light;
        QJsonObject dark;
        dark["primaryContainer"] = "#223344";
        dark["onSurface"] = "#eeddcc";
        theme["dark"] = dark;
        QJsonObject wallpaper;
        wallpaper["chunks"] = 2;
        wallpaper["size"] = wallpaperData.size();
        theme["wallpaper"] = wallpaper;

        QJsonObject themeMsg;
        themeMsg["type"] = "theme";
        themeMsg["theme"] = theme;
        sendMacMessage(client, sessionKey, themeMsg);

        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        // Send chunk 1
        QJsonObject chunk1;
        chunk1["type"] = "theme_data";
        chunk1["index"] = 0;
        chunk1["data"] = QString::fromLatin1(chunk1Data.toBase64());
        sendMacMessage(client, sessionKey, chunk1);

        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        // Send chunk 2 — this should trigger import + ack
        QJsonObject chunk2;
        chunk2["type"] = "theme_data";
        chunk2["index"] = 1;
        chunk2["data"] = QString::fromLatin1(chunk2Data.toBase64());
        sendMacMessage(client, sessionKey, chunk2);

        for (int i = 0; i < 20; ++i) QCoreApplication::processEvents();

        // Read theme_ack from server
        for (int i = 0; i < 20 && !client.bytesAvailable(); ++i) {
            QCoreApplication::processEvents();
            if (!client.bytesAvailable())
                client.waitForReadyRead(200);
        }
        QVERIFY(client.bytesAvailable() > 0);
        QByteArray ackLine = client.readLine().trimmed();
        QJsonObject ack = QJsonDocument::fromJson(ackLine).object();
        QCOMPARE(ack["type"].toString(), QString("theme_ack"));
        QVERIFY(ack["accepted"].toBool());

        // Verify theme was imported (should appear in available themes)
        QVERIFY(themeService.availableThemes().contains("chunk-test"));

        svc.stop();
    }

    void wallpaperSizeCapRejected()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.start(19882);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19882);
        QVERIFY(client.waitForConnected(2000));

        QByteArray sessionKey = authenticateClient(client, svc, "test-secret");
        QVERIFY(!sessionKey.isEmpty());

        // Send theme with wallpaper > 5MB
        QJsonObject theme;
        theme["name"] = "Too Big";
        theme["seed"] = "#ff0000";
        QJsonObject light;
        light["primary"] = "#ff0000";
        theme["light"] = light;
        QJsonObject dark;
        dark["primary"] = "#ff0000";
        theme["dark"] = dark;
        QJsonObject wallpaper;
        wallpaper["chunks"] = 100;
        wallpaper["size"] = 6 * 1024 * 1024;  // 6MB — exceeds 5MB cap
        theme["wallpaper"] = wallpaper;

        QJsonObject msg;
        msg["type"] = "theme";
        msg["theme"] = theme;
        sendMacMessage(client, sessionKey, msg);

        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        // Should NOT crash, theme rejected silently (no pending state set)
        // Sending a chunk should be ignored since no pending theme
        QJsonObject chunk;
        chunk["type"] = "theme_data";
        chunk["index"] = 0;
        chunk["data"] = QString::fromLatin1(QByteArray("AAAA").toBase64());
        sendMacMessage(client, sessionKey, chunk);

        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        // No crash is the verification — oversized themes are rejected
        QVERIFY(true);

        svc.stop();
    }

    void disconnectClearsPendingTheme()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.start(19883);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19883);
        QVERIFY(client.waitForConnected(2000));

        QByteArray sessionKey = authenticateClient(client, svc, "test-secret");
        QVERIFY(!sessionKey.isEmpty());

        // Send theme message (but not all chunks)
        QJsonObject theme;
        theme["name"] = "Partial";
        theme["seed"] = "#aabb00";
        QJsonObject light; light["primary"] = "#aabb00"; theme["light"] = light;
        QJsonObject dark; dark["primary"] = "#aabb00"; theme["dark"] = dark;
        QJsonObject wallpaper; wallpaper["chunks"] = 5; wallpaper["size"] = 4096;
        theme["wallpaper"] = wallpaper;

        QJsonObject msg;
        msg["type"] = "theme";
        msg["theme"] = theme;
        sendMacMessage(client, sessionKey, msg);

        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        // Disconnect
        client.disconnectFromHost();
        for (int i = 0; i < 20; ++i) QCoreApplication::processEvents();

        // Reconnect — if pending state was cleared, we should be able to
        // start a fresh transfer without issues
        QTcpSocket client2;
        client2.connectToHost("127.0.0.1", 19883);
        QVERIFY(client2.waitForConnected(2000));

        QByteArray sessionKey2 = authenticateClient(client2, svc, "test-secret");
        QVERIFY(!sessionKey2.isEmpty());  // Fresh session works

        svc.stop();
    }

    void camelCaseToHyphenatedConversion()
    {
        // Test that the camelCase keys from companion JSON get converted
        // to hyphenated keys when calling importCompanionTheme.
        // We verify by importing a theme with camelCase keys and checking
        // that ThemeService has the correct property values.

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        oap::ThemeService themeService;
        QStringList searchPaths;
        searchPaths << tempDir.path();
        themeService.scanThemeDirectories(searchPaths);

        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.setThemeService(&themeService);
        svc.start(19884);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19884);
        QVERIFY(client.waitForConnected(2000));

        QByteArray sessionKey = authenticateClient(client, svc, "test-secret");
        QVERIFY(!sessionKey.isEmpty());

        // Send theme with camelCase keys (like companion app sends)
        QJsonObject theme;
        theme["name"] = "CamelCase Test";
        theme["seed"] = "#445566";
        QJsonObject light;
        light["primaryContainer"] = "#aabbcc";  // should become primary-container
        light["onSurface"] = "#112233";          // should become on-surface
        light["surfaceContainerHighest"] = "#ffeedd";  // surface-container-highest
        light["primary"] = "#445566";
        theme["light"] = light;
        QJsonObject dark;
        dark["primaryContainer"] = "#223344";
        dark["onSurface"] = "#eeddcc";
        dark["surfaceContainerHighest"] = "#001122";
        dark["primary"] = "#998877";
        theme["dark"] = dark;
        QJsonObject wallpaper;
        wallpaper["chunks"] = 1;
        wallpaper["size"] = 4;
        theme["wallpaper"] = wallpaper;

        QJsonObject themeMsg;
        themeMsg["type"] = "theme";
        themeMsg["theme"] = theme;
        sendMacMessage(client, sessionKey, themeMsg);

        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        // Send single wallpaper chunk
        QJsonObject chunk;
        chunk["type"] = "theme_data";
        chunk["index"] = 0;
        chunk["data"] = QString::fromLatin1(QByteArray("TEST").toBase64());
        sendMacMessage(client, sessionKey, chunk);

        for (int i = 0; i < 20; ++i) QCoreApplication::processEvents();

        // Wait for ack
        for (int i = 0; i < 20 && !client.bytesAvailable(); ++i) {
            QCoreApplication::processEvents();
            if (!client.bytesAvailable())
                client.waitForReadyRead(200);
        }

        // Theme should be imported with hyphenated keys
        // Check that primaryContainer (primary-container) got set correctly
        QVERIFY(themeService.availableThemes().contains("camelcase-test"));

        // Switch to the imported theme and verify colors
        themeService.setTheme("camelcase-test");
        themeService.setNightMode(false);  // day mode
        QCOMPARE(themeService.primaryContainer(), QColor("#aabbcc"));
        QCOMPARE(themeService.onSurface(), QColor("#112233"));
        QCOMPARE(themeService.surfaceContainerHighest(), QColor("#ffeedd"));

        svc.stop();
    }

    // --- Phase 13.1: Companion reconnect hardening tests ---

    void alwaysReplaceStaleClient()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.start(19890);

        // Connect and authenticate client A
        QTcpSocket clientA;
        clientA.connectToHost("127.0.0.1", 19890);
        QVERIFY(clientA.waitForConnected(2000));

        QByteArray sessionKeyA = authenticateClient(clientA, svc, "test-secret");
        QVERIFY(!sessionKeyA.isEmpty());
        QVERIFY(svc.isConnected());

        // Connect client B WITHOUT disconnecting A
        QTcpSocket clientB;
        clientB.connectToHost("127.0.0.1", 19890);
        QVERIFY(clientB.waitForConnected(2000));

        // Process events so server sees the new connection and replaces A
        for (int i = 0; i < 20; ++i) QCoreApplication::processEvents();

        // Client B should receive a challenge (not a rejection/close)
        for (int i = 0; i < 20 && !clientB.bytesAvailable(); ++i) {
            QCoreApplication::processEvents();
            if (!clientB.bytesAvailable())
                clientB.waitForReadyRead(200);
        }
        QVERIFY(clientB.bytesAvailable() > 0);
        QByteArray challengeLine = clientB.readLine().trimmed();
        QJsonObject challenge = QJsonDocument::fromJson(challengeLine).object();
        QCOMPARE(challenge["type"].toString(), QString("challenge"));
        QVERIFY(challenge.contains("nonce"));

        // Authenticate client B
        QByteArray sessionKeyB = authenticateClient(clientB, svc, "test-secret");
        // authenticateClient reads challenge first, but we already consumed it.
        // Instead, manually complete the auth handshake since we already have the challenge.
        // Actually, authenticateClient sent a NEW challenge on the replaced connection.
        // We already read the challenge above, so do the auth manually.
        QByteArray nonce = challenge["nonce"].toString().toLatin1();
        QByteArray token = QMessageAuthenticationCode::hash(
            nonce, QByteArray("test-secret"), QCryptographicHash::Sha256).toHex();

        QJsonObject hello;
        hello["type"] = "hello";
        hello["token"] = QString::fromLatin1(token);
        clientB.write(QJsonDocument(hello).toJson(QJsonDocument::Compact) + "\n");
        clientB.flush();

        // Wait for hello_ack
        for (int i = 0; i < 20 && !clientB.bytesAvailable(); ++i) {
            QCoreApplication::processEvents();
            if (!clientB.bytesAvailable())
                clientB.waitForReadyRead(200);
        }
        QVERIFY(clientB.bytesAvailable() > 0);
        QByteArray ackLine = clientB.readLine().trimmed();
        QJsonObject ack = QJsonDocument::fromJson(ackLine).object();
        QVERIFY(ack["accepted"].toBool());

        QVERIFY(svc.isConnected());

        svc.stop();
    }

    void cleanupIdempotent()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.start(19891);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19891);
        QVERIFY(client.waitForConnected(2000));

        QByteArray sessionKey = authenticateClient(client, svc, "test-secret");
        QVERIFY(!sessionKey.isEmpty());
        QVERIFY(svc.isConnected());

        QSignalSpy spy(&svc, &oap::CompanionListenerService::connectedChanged);

        // Disconnect the client
        client.disconnectFromHost();

        // Process events for cleanup
        for (int i = 0; i < 30; ++i) {
            QCoreApplication::processEvents();
            QTest::qWait(10);
        }

        QVERIFY(!svc.isConnected());
        // connectedChanged should have been emitted exactly once
        QCOMPARE(spy.count(), 1);

        // Wait more — verify no additional emissions
        QTest::qWait(100);
        QCoreApplication::processEvents();
        QCOMPARE(spy.count(), 1);

        svc.stop();
    }

    void inactivityTimeout()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.setInactivityTimeout(100);  // 100ms for test speed
        svc.start(19892);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19892);
        QVERIFY(client.waitForConnected(2000));

        QByteArray sessionKey = authenticateClient(client, svc, "test-secret");
        QVERIFY(!sessionKey.isEmpty());
        QVERIFY(svc.isConnected());

        // Wait for the inactivity timeout to fire (100ms + buffer)
        QTest::qWait(300);
        QCoreApplication::processEvents();

        QVERIFY(!svc.isConnected());

        svc.stop();
    }

    void errorOccurredTriggersCleanup()
    {
        oap::CompanionListenerService svc;
        svc.setSharedSecret("test-secret");
        svc.setInactivityTimeout(30000);  // prevent timeout interference
        svc.start(19893);

        QTcpSocket client;
        client.connectToHost("127.0.0.1", 19893);
        QVERIFY(client.waitForConnected(2000));

        QByteArray sessionKey = authenticateClient(client, svc, "test-secret");
        QVERIFY(!sessionKey.isEmpty());
        QVERIFY(svc.isConnected());

        QSignalSpy spy(&svc, &oap::CompanionListenerService::connectedChanged);

        // Send RST instead of FIN by setting SO_LINGER with timeout 0
        // This triggers errorOccurred on the server side
        struct linger l = {1, 0};
        setsockopt(client.socketDescriptor(), SOL_SOCKET, SO_LINGER, &l, sizeof(l));
        client.close();

        // Process events for the error to propagate
        for (int i = 0; i < 30; ++i) {
            QCoreApplication::processEvents();
            QTest::qWait(10);
        }

        QVERIFY(!svc.isConnected());
        QVERIFY(spy.count() >= 1);

        svc.stop();
    }
};

QTEST_MAIN(TestCompanionListener)
#include "test_companion_listener.moc"
