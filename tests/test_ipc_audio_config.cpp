#include <QTest>
#include <QTemporaryDir>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QElapsedTimer>

#include "core/services/IpcServer.hpp"
#include "core/services/AudioService.hpp"
#include "core/YamlConfig.hpp"

using namespace oap;

class TestIpcAudioConfig : public QObject {
    Q_OBJECT

    // Send one newline-framed request, return the parsed JSON response
    // object (same-thread QLocalSocket pattern — see test_ipc_install_theme).
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
    void setAudioConfigPersistsMicrophoneDevice() {
        QTemporaryDir dir;
        const QString yamlPath = dir.path() + "/config.yaml";
        YamlConfig cfg;
        AudioService audio;
        IpcServer server;
        server.setAudioService(&audio);
        server.setConfig(&cfg, yamlPath);
        const QString sockPath = dir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject req{{"command", "set_audio_config"},
                        {"data", QJsonObject{{"input_device", "alsa_input.usb-mic"}}}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QVERIFY(resp.value("ok").toBool());

        // Live service value applied (works without a PipeWire daemon)
        QCOMPARE(audio.inputDevice(), QString("alsa_input.usb-mic"));

        // Persisted on the CANONICAL key — reload from disk and check the
        // getter main.cpp uses at startup.
        YamlConfig reloaded;
        reloaded.load(yamlPath);
        QCOMPARE(reloaded.microphoneDevice(), QString("alsa_input.usb-mic"));
    }

    void setAudioConfigDoesNotPersistMasterVolume() {
        QTemporaryDir dir;
        const QString yamlPath = dir.path() + "/config.yaml";
        YamlConfig cfg;
        AudioService audio;
        IpcServer server;
        server.setAudioService(&audio);
        server.setConfig(&cfg, yamlPath);
        const QString sockPath = dir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        // One request carrying BOTH a device field (triggers the immediate
        // save) and master_volume (must NOT ride that save — the debounced
        // main.cpp path is the single writer for audio.master_volume).
        QJsonObject req{{"command", "set_audio_config"},
                        {"data", QJsonObject{{"input_device", "alsa_input.usb-mic"},
                                             {"master_volume", 55}}}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QVERIFY(resp.value("ok").toBool());

        // Applied live...
        QCOMPARE(audio.masterVolume(), 55);
        // ...but the file written by the device-field save keeps the default:
        // master_volume was NOT flushed by the IPC handler.
        YamlConfig reloaded;
        reloaded.load(yamlPath);
        QCOMPARE(reloaded.masterVolume(), 80);
    }

    void setAudioConfigReportsPersistenceFailure() {
        QTemporaryDir dir;
        YamlConfig cfg;
        AudioService audio;
        IpcServer server;
        server.setAudioService(&audio);
        // Parent directory does not exist — YamlConfig::save cannot create
        // its tmp file there and returns false.
        server.setConfig(&cfg, dir.path() + "/no-such-subdir/config.yaml");
        const QString sockPath = dir.path() + "/ipc.sock";
        QVERIFY(server.start(sockPath));

        QJsonObject req{{"command", "set_audio_config"},
                        {"data", QJsonObject{{"input_device", "alsa_input.usb-mic"}}}};
        const QJsonObject resp = roundTrip(sockPath, req);
        QVERIFY(!resp.value("ok").toBool());
        QVERIFY(!resp.value("error").toString().isEmpty());
        // Live apply still happened before the failed persist (deliberate).
        QCOMPARE(audio.inputDevice(), QString("alsa_input.usb-mic"));
    }
};

QTEST_MAIN(TestIpcAudioConfig)
#include "test_ipc_audio_config.moc"
