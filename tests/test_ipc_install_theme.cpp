#include <QTest>
#include <QTemporaryDir>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>
#include <QElapsedTimer>

#include "core/services/IpcServer.hpp"
#include "core/services/ThemeService.hpp"
#include "core/api/ApiInboundState.hpp"

using namespace oap;

class TestIpcInstallTheme : public QObject {
    Q_OBJECT

    // Send one newline-framed request, return the parsed JSON response object.
    // Server and client live in this same thread, so the client's blocking
    // waitFor* calls do NOT dispatch the server's newConnection/readyRead
    // signals — we must pump the event loop for the round-trip to complete
    // (same pattern as test_companion_listener.cpp).
    QJsonObject roundTrip(const QString& socketPath, const QJsonObject& request) {
        QLocalSocket sock;
        sock.connectToServer(socketPath);
        if (!sock.waitForConnected(2000)) return {};
        sock.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
        sock.flush();
        QElapsedTimer timer;
        timer.start();
        while (sock.bytesAvailable() == 0 && timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            sock.waitForReadyRead(20);
        }
        const QByteArray buf = sock.readAll();
        sock.disconnectFromServer();
        return QJsonDocument::fromJson(buf.trimmed()).object();
    }

private slots:
    void installsColorOnlyTheme() {
        QTemporaryDir themes, sockDir;
        ThemeService svc;
        svc.scanThemeDirectories({themes.path()});
        IpcServer server;
        server.setThemeService(&svc);
        const QString sockPath = sockDir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject light{{"primary", "#112233"}, {"onPrimary", "#ffffff"}};
        QJsonObject dark{{"primary", "#445566"}};
        QJsonObject req{
            {"command", "install_theme"},
            {"data", QJsonObject{{"name", "Sunset Vibes"}, {"seed", "#ff8a65"},
                                 {"light", light}, {"dark", dark}}}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QVERIFY(resp.value("ok").toBool());
        QCOMPARE(resp.value("slug").toString(), QString("sunset-vibes"));
        QVERIFY(QFile::exists(themes.path() + "/sunset-vibes/theme.yaml"));
    }

    void rejectsInvalidPayload() {
        QTemporaryDir themes, sockDir;
        ThemeService svc;
        svc.scanThemeDirectories({themes.path()});
        IpcServer server;
        server.setThemeService(&svc);
        const QString sockPath = sockDir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject req{
            {"command", "install_theme"},
            {"data", QJsonObject{{"name", ""}, {"light", QJsonObject{{"primary", "#111111"}}},
                                 {"dark", QJsonObject{{"primary", "#222222"}}}}}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QVERIFY(!resp.value("ok").toBool());
        QVERIFY(!resp.value("error").toString().isEmpty());
    }

    void companionStatusPrefersInboundStateWhenSet() {
        QTemporaryDir sockDir;
        oap::api::ApiInboundState inbound;
        IpcServer server;
        server.setInboundState(&inbound);
        inbound.setBattery(55, true);
        const QString sockPath = sockDir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject req{{"command", "companion_status"}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QCOMPARE(resp.value("battery").toInt(), 55);
        QVERIFY(resp.value("charging").toBool());
        QCOMPARE(resp.value("source").toString(), QString("api"));
        QVERIFY2(resp.contains("gps_stale"),
                 "companion_status must expose gps_stale (wishlist 2026-07-13)");
    }

    // Finding 2: after the GPS owner disconnects (clearGps), companion_status
    // must not leak the last coordinates — clearGps resets lat/lon to 0.
    void companionStatusClearsStaleGpsAfterOwnerDrop() {
        QTemporaryDir sockDir;
        oap::api::ApiInboundState inbound;
        IpcServer server;
        server.setInboundState(&inbound);
        inbound.setGps(45.5, -122.6, 13.4, 275.0, 4.2, 0);
        inbound.clearGps();   // owner disconnected
        const QString sockPath = sockDir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject req{{"command", "companion_status"}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QCOMPARE(resp.value("gps_lat").toDouble(), 0.0);
        QCOMPARE(resp.value("gps_lon").toDouble(), 0.0);
        QCOMPARE(resp.value("source").toString(), QString("api"));
    }
};

QTEST_MAIN(TestIpcInstallTheme)
#include "test_ipc_install_theme.moc"
