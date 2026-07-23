#include <QTest>
#include <QSignalSpy>
#include <QPointer>
#include <QTcpSocket>
#include <QtEndian>
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/aa/NightModeProvider.hpp"
#include "core/services/IConfigService.hpp"
#include "core/services/NightModeService.hpp"
#include "oaa/sensor/SensorEventIndicationMessage.pb.h"
#include "oaa/sensor/SensorStartRequestMessage.pb.h"
#include "oaa/sensor/SensorTypeEnum.pb.h"

namespace oap::aa {

class TestNightModeProvider : public NightModeProvider {
public:
    bool isNight() const override { return state_; }
    bool hasValidState() const override { return valid_; }
    void start() override { ++startCount_; }
    void stop() override { ++stopCount_; }

    void publish(bool state)
    {
        state_ = state;
        valid_ = true;
        emit nightModeChanged(state);
    }

    int startCount() const { return startCount_; }
    int stopCount() const { return stopCount_; }

private:
    bool state_ = false;
    bool valid_ = false;
    int startCount_ = 0;
    int stopCount_ = 0;
};

class AndroidAutoOrchestratorTestAccess {
public:
    static quint16 listenerPort(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.tcpServer_.serverPort();
    }

    static oaa::AASession* session(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.session_;
    }

    static quint16 activePeerPort(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.activeSocket_ ? orchestrator.activeSocket_->peerPort() : 0;
    }

    static void disableAutomaticAccept(AndroidAutoOrchestrator& orchestrator)
    {
        QObject::disconnect(&orchestrator.tcpServer_, &QTcpServer::newConnection,
                            &orchestrator, &AndroidAutoOrchestrator::onNewConnection);
    }

    static bool acceptNextConnection(AndroidAutoOrchestrator& orchestrator)
    {
        if (!orchestrator.tcpServer_.hasPendingConnections()
            && !orchestrator.tcpServer_.waitForNewConnection(1000)) {
            return false;
        }
        orchestrator.onNewConnection();
        return true;
    }

    static oaa::hu::SensorChannelHandler& sensorHandler(AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.sensorHandler_;
    }
};

} // namespace oap::aa

// Stub IConfigService for testing
class StubConfigService : public oap::IConfigService {
public:
    QVariantMap values;

    QVariant value(const QString& key) const override {
        return values.value(key);
    }
    void setValue(const QString& key, const QVariant& v) override {
        values[key] = v;
    }
    QVariant pluginValue(const QString&, const QString&) const override { return {}; }
    void setPluginValue(const QString&, const QString&, const QVariant&) override {}
    void save() override {}
};

class TestAndroidAutoOrchestrator : public QObject {
    Q_OBJECT
private slots:
    void testInitialState() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
        QVERIFY(orch.videoDecoder() != nullptr);
        QVERIFY(orch.inputHandler() != nullptr);
    }

    void testStartListens() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 15277;  // Use non-standard port to avoid conflicts
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QSignalSpy stateSpy(&orch, &oap::aa::AndroidAutoOrchestrator::connectionStateChanged);

        orch.start();

        // Should transition to WaitingForDevice
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::WaitingForDevice));
        QVERIFY(stateSpy.count() >= 1);

        orch.stop();
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
    }

    void testStartUsesDefaultPortWhenNotConfigured() {
        // When connection.tcp_port is not set, should use default 5277
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        // Just verify it doesn't crash; the port is internal
        orch.start();
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::WaitingForDevice));
        orch.stop();
    }

    void testTcpPortFromConfigService() {
        // Verify the orchestrator reads connection.tcp_port from IConfigService
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 15278;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        orch.start();

        // Check status message includes our configured port
        QVERIFY(orch.statusMessage().contains("15278"));

        orch.stop();
    }

    void testStopWithoutStart() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        // Should not crash
        orch.stop();
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
    }

    void testVideoFocusWithoutConnection() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        // Should not crash when not connected
        orch.requestVideoFocus();
        orch.requestExitToCar();
    }

    void testPhonePropertiesDefaultValues() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QCOMPARE(orch.phoneBatteryLevel(), -1);
        QCOMPARE(orch.phoneSignalStrength(), -1);
    }

    void testAaConnectedFalseWhenDisconnected() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QVERIFY(!orch.isAaConnected());
    }

    void testForcedSessionReplacementClearsSensorWireState() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        orch.start();

        const quint16 port = oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch);
        QVERIFY(port != 0);

        QTcpSocket firstSocket;
        firstSocket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(firstSocket.waitForConnected());
        QTRY_VERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::session(orch) != nullptr);
        QTRY_COMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::activePeerPort(orch),
                     firstSocket.localPort());
        auto* firstSession = oap::aa::AndroidAutoOrchestratorTestAccess::session(orch);

        QByteArray versionResponse(6, '\0');
        qToBigEndian<uint16_t>(1, reinterpret_cast<uchar*>(versionResponse.data()));
        qToBigEndian<uint16_t>(7, reinterpret_cast<uchar*>(versionResponse.data() + 2));
        qToBigEndian<uint16_t>(0, reinterpret_cast<uchar*>(versionResponse.data() + 4));
        firstSession->messenger()->messageReceived(0, 0x0002, versionResponse, 0);
        QCOMPARE(firstSession->state(), oaa::SessionState::TLSHandshake);

        firstSession->messenger()->handshakeComplete();
        QCOMPARE(firstSession->state(), oaa::SessionState::ServiceDiscovery);
        firstSession->messenger()->messageReceived(0, 0x0005, QByteArray(), 0);
        QCOMPARE(firstSession->state(), oaa::SessionState::Active);

        auto& sensorHandler = oap::aa::AndroidAutoOrchestratorTestAccess::sensorHandler(orch);
        sensorHandler.onChannelOpened();
        oaa::proto::messages::SensorStartRequestMessage request;
        request.set_sensor_type(oaa::proto::enums::SensorType::NIGHT_DATA);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        sensorHandler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

        QSignalSpy sendSpy(&sensorHandler, &oaa::IChannelHandler::sendRequested);
        sensorHandler.pushNightMode(true);
        QCOMPARE(sendSpy.count(), 1);
        sendSpy.clear();

        QTcpSocket replacementSocket;
        replacementSocket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(replacementSocket.waitForConnected());
        QTRY_COMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::activePeerPort(orch),
                     replacementSocket.localPort());

        // The provider seed for the replacement session must only update the
        // cache; no sensor indication is legal until the new channel subscribes.
        sensorHandler.pushNightMode(false);
        QCOMPARE(sendSpy.count(), 0);

        orch.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void testSharedServiceSeedOverwritesRetainedStateBeforeSessionStart() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;

        auto provider = std::make_unique<oap::aa::TestNightModeProvider>();
        auto* providerPtr = provider.get();
        oap::NightModeService nightService(std::move(provider), nullptr);
        nightService.start();
        providerPtr->publish(false);

        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        auto& sensorHandler = oap::aa::AndroidAutoOrchestratorTestAccess::sensorHandler(orch);
        sensorHandler.pushNightMode(true);
        QSignalSpy sendSpy(&sensorHandler, &oaa::IChannelHandler::sendRequested);
        orch.setNightModeService(&nightService);

        orch.start();
        const quint16 port = oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch);
        QVERIFY(port != 0);

        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(socket.waitForConnected());
        QTRY_COMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::activePeerPort(orch),
                     socket.localPort());

        // Service attachment seeds the authoritative day state without
        // transmitting before the phone subscribes.
        QCOMPARE(sendSpy.count(), 0);
        sensorHandler.onChannelOpened();
        oaa::proto::messages::SensorStartRequestMessage request;
        request.set_sensor_type(oaa::proto::enums::SensorType::NIGHT_DATA);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        sensorHandler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 2);
        const QByteArray indicationPayload = sendSpy[1][2].toByteArray();
        oaa::proto::messages::SensorEventIndication indication;
        QVERIFY(indication.ParseFromArray(indicationPayload.constData(), indicationPayload.size()));
        QCOMPARE(indication.night_mode_size(), 1);
        QVERIFY(indication.night_mode(0).has_is_night());
        QCOMPARE(indication.night_mode(0).is_night(), false);

        orch.stop();
        QCOMPARE(providerPtr->stopCount(), 0); // session lifetime does not own it
        nightService.stop();
        QCOMPARE(providerPtr->stopCount(), 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void testPreActiveSessionIsDestroyedBeforeReplacementRegistration() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);

        const quint16 port = oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch);
        QVERIFY(port != 0);

        QTcpSocket firstSocket;
        firstSocket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(firstSocket.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));

        QPointer<oaa::AASession> firstSession(
            oap::aa::AndroidAutoOrchestratorTestAccess::session(orch));
        QVERIFY(!firstSession.isNull());
        QCOMPARE(firstSession->state(), oaa::SessionState::VersionExchange);

        auto& sensorHandler = oap::aa::AndroidAutoOrchestratorTestAccess::sensorHandler(orch);
        sensorHandler.onChannelOpened();
        oaa::proto::messages::SensorStartRequestMessage request;
        request.set_sensor_type(oaa::proto::enums::SensorType::NIGHT_DATA);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        sensorHandler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);
        sensorHandler.pushNightMode(true);

        QSignalSpy sendSpy(&sensorHandler, &oaa::IChannelHandler::sendRequested);
        QTcpSocket replacementSocket;
        replacementSocket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(replacementSocket.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));

        // onNewConnection() is invoked directly above, without returning to an
        // event loop that could service DeferredDelete. The old pre-active
        // session must already be gone before replacement handlers register.
        QVERIFY(firstSession.isNull());
        QCOMPARE(sendSpy.count(), 0);

        sensorHandler.onChannelOpened();
        sensorHandler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);
        QCOMPARE(sendSpy.count(), 2);
        const QByteArray indicationPayload = sendSpy[1][2].toByteArray();
        oaa::proto::messages::SensorEventIndication indication;
        QVERIFY(indication.ParseFromArray(indicationPayload.constData(), indicationPayload.size()));
        QCOMPARE(indication.night_mode_size(), 1);
        QCOMPARE(indication.night_mode(0).is_night(), true);

        sendSpy.clear();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        sensorHandler.pushNightMode(false);
        QCOMPARE(sendSpy.count(), 1);

        orch.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void testInvalidSharedServicePreservesCacheUntilValidRecovery() {
        StubConfigService cfg;
        auto provider = std::make_unique<oap::aa::TestNightModeProvider>();
        auto* providerPtr = provider.get();
        oap::NightModeService nightService(std::move(provider), nullptr);
        nightService.start();
        QCOMPARE(providerPtr->startCount(), 1);

        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        auto& sensorHandler = oap::aa::AndroidAutoOrchestratorTestAccess::sensorHandler(orch);
        sensorHandler.pushNightMode(true);
        QSignalSpy sendSpy(&sensorHandler, &oaa::IChannelHandler::sendRequested);

        orch.setNightModeService(&nightService);
        QCOMPARE(sendSpy.count(), 0);

        sensorHandler.onChannelOpened();
        oaa::proto::messages::SensorStartRequestMessage request;
        request.set_sensor_type(oaa::proto::enums::SensorType::NIGHT_DATA);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        sensorHandler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);
        QCOMPARE(sendSpy.count(), 2);
        oaa::proto::messages::SensorEventIndication indication;
        QByteArray indicationPayload = sendSpy[1][2].toByteArray();
        QVERIFY(indication.ParseFromArray(indicationPayload.constData(), indicationPayload.size()));
        QCOMPARE(indication.night_mode(0).is_night(), true);

        sensorHandler.onChannelClosed();
        sendSpy.clear();
        providerPtr->publish(false);
        QCOMPARE(sendSpy.count(), 0);

        sensorHandler.onChannelOpened();
        sensorHandler.onMessage(oaa::SensorMessageId::SENSOR_START_REQUEST, payload);
        QCOMPARE(sendSpy.count(), 2);
        indicationPayload = sendSpy[1][2].toByteArray();
        QVERIFY(indication.ParseFromArray(indicationPayload.constData(), indicationPayload.size()));
        QCOMPARE(indication.night_mode(0).is_night(), false);

        orch.stop();
        QCOMPARE(providerPtr->stopCount(), 0);
        nightService.stop();
        QCOMPARE(providerPtr->stopCount(), 1);
    }
};

QTEST_MAIN(TestAndroidAutoOrchestrator)
#include "test_aa_orchestrator.moc"
