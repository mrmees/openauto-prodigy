#include <QTest>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QPointer>
#include <QTcpSocket>
#include <QtEndian>
#include "core/aa/AndroidAutoOrchestrator.hpp"
#include "core/aa/NightModeProvider.hpp"
#include "core/aa/WirelessAaConfig.hpp"
#include "core/YamlConfig.hpp"
#include "core/services/AudioService.hpp"
#include "core/services/IConfigService.hpp"
#include "core/services/NightModeService.hpp"
#ifdef HAS_BLUETOOTH
#include "core/aa/BluetoothDiscoveryService.hpp"
#endif
#include "oaa/sensor/SensorEventIndicationMessage.pb.h"
#include "oaa/sensor/SensorStartRequestMessage.pb.h"
#include "oaa/sensor/SensorTypeEnum.pb.h"
#include "oaa/av/AVInputOpenRequestMessage.pb.h"
#include "oaa/av/AVInputOpenResponseMessage.pb.h"
#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/AVChannelSetupResponseMessage.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"
#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/control/ServiceDiscoveryResponseMessage.pb.h"
#include "oaa/video/VideoFocusModeEnum.pb.h"

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

struct TestMicCaptureBackend {
    oap::AudioStreamHandle* openResult = nullptr;
    QString name;
    int sampleRate = 0;
    int channels = 0;
    int bitDepth = 0;
    QList<oap::IAudioService::CaptureCallback> callbacks;
    QList<std::function<void()>> errors;
    int closeCount = 0;
    oap::AudioStreamHandle* closedHandle = nullptr;
    bool bridgeActiveDuringClose = false;
};

class AndroidAutoOrchestratorTestAccess {
public:
    static quint16 listenerPort(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.listenerPort_;
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

    static oaa::hu::VideoChannelHandler& videoHandler(AndroidAutoOrchestrator& orchestrator)
    {
        return *orchestrator.mainDisplay_.videoHandler();
    }

    static VideoDecoder& videoDecoder(AndroidAutoOrchestrator& orchestrator)
    {
        return *orchestrator.mainDisplay_.decoder();
    }

    static ProjectedDisplaySession& mainDisplay(
        AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.mainDisplay_;
    }

    static ProjectedDisplaySession& clusterDisplay(
        AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.clusterDisplay_;
    }

    static void setConnectedForFocusTest(AndroidAutoOrchestrator& orchestrator)
    {
        orchestrator.setState(AndroidAutoOrchestrator::Connected,
                              QStringLiteral("test connected"));
    }

    static oaa::hu::AVInputChannelHandler& avInputHandler(
        AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.avInputHandler_;
    }

    static oaa::hu::AudioChannelHandler& mediaAudioHandler(
        AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.mediaAudioHandler_;
    }

    static void simulateAudioServiceTeardown(AndroidAutoOrchestrator& orchestrator)
    {
        orchestrator.onAudioServiceAboutToDestroy();
    }

    static void installMicCaptureBackend(AndroidAutoOrchestrator& orchestrator,
                                         TestMicCaptureBackend& backend)
    {
        orchestrator.micCaptureOpen_ = [&backend](const auto& request) {
            backend.name = request.name;
            backend.sampleRate = request.sampleRate;
            backend.channels = request.channels;
            backend.bitDepth = request.bitDepth;
            backend.callbacks.append(request.callback);
            backend.errors.append(request.onStreamError);
            return backend.openResult;
        };
        orchestrator.micCaptureClose_ = [&orchestrator, &backend](auto* handle) {
            ++backend.closeCount;
            backend.closedHandle = handle;
            backend.bridgeActiveDuringClose = orchestrator.micCaptureBridge_.isActive();
        };
    }

    static bool micBridgeActive(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.micCaptureBridge_.isActive();
    }

    static oap::AudioStreamHandle* micCaptureHandle(
        const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.micCaptureHandle_;
    }

    static bool watchdogActive(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.watchdogTimer_.isActive();
    }

    static bool admissionOpen(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.admissionOpen_;
    }

    static bool pendingReconnect(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.pendingReconnect_;
    }

    static const ProjectedClusterConfig& projectedClusterConfig(
        const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.projectedClusterConfig_;
    }

    static void notifyPhoneWillConnect(AndroidAutoOrchestrator& orchestrator)
    {
        orchestrator.onPhoneWillConnect();
    }

#ifdef HAS_BLUETOOTH
    static quint16 discoveryTcpPort(const AndroidAutoOrchestrator& orchestrator)
    {
        return orchestrator.btDiscovery_
            ? orchestrator.btDiscovery_->advertisedTcpPort() : 0;
    }
#endif
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

class FakePlaybackAudioService : public oap::IAudioService {
public:
    oap::AudioStreamHandle* createStream(const QString&, int, int, int,
                                         const QString&, int) override
    {
        if (created >= 3)
            return nullptr;
        return &handles[created++];
    }
    void destroyStream(oap::AudioStreamHandle*) override {}
    int writeAudio(oap::AudioStreamHandle*, const uint8_t*, int size) override
    {
        ++writes;
        return size;
    }
    void setMasterVolume(int) override {}
    int masterVolume() const override { return 100; }
    void requestAudioFocus(oap::AudioStreamHandle*, oap::AudioFocusType) override {}
    void releaseAudioFocus(oap::AudioStreamHandle*) override {}
    void setOutputDevice(const QString&) override {}
    void setInputDevice(const QString&) override {}
    QString outputDevice() const override { return QStringLiteral("auto"); }
    QString inputDevice() const override { return QStringLiteral("auto"); }

    oap::AudioStreamHandle handles[3];
    int created = 0;
    int writes = 0;
};

class TestAndroidAutoOrchestrator : public QObject {
    Q_OBJECT
private slots:
    void testTcpPortResolver_data() {
        QTest::addColumn<QVariant>("configured");
        QTest::addColumn<int>("expected");
        QTest::addColumn<bool>("fallback");
        QTest::newRow("missing") << QVariant{} << 5277 << false;
        QTest::newRow("valid-integer") << QVariant{15278} << 15278 << false;
        QTest::newRow("valid-text") << QVariant{QStringLiteral("15279")} << 15279 << false;
        QTest::newRow("ephemeral-integer") << QVariant{0} << 0 << false;
        QTest::newRow("ephemeral-text-invalid") << QVariant{QStringLiteral("0")} << 5277 << true;
        QTest::newRow("nonnumeric") << QVariant{QStringLiteral("nope")} << 5277 << true;
        QTest::newRow("negative") << QVariant{-1} << 5277 << true;
        QTest::newRow("oversized") << QVariant{70000} << 5277 << true;
    }

    void testTcpPortResolver() {
        QFETCH(QVariant, configured);
        QFETCH(int, expected);
        QFETCH(bool, fallback);
        bool usedFallback = false;
        QCOMPARE(static_cast<int>(oap::aa::resolveWirelessAaTcpPort(
                     configured, &usedFallback)), expected);
        QCOMPARE(usedFallback, fallback);
    }

    void testVideoStreamBoundaryPrecedesAlreadyEmittedFrames() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);

        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost,
                             oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch));
        QVERIFY(socket.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));

        auto& handler = oap::aa::AndroidAutoOrchestratorTestAccess::videoHandler(orch);
        auto& decoder = oap::aa::AndroidAutoOrchestratorTestAccess::videoDecoder(orch);
        QSignalSpy resetSpy(&decoder, &oap::aa::VideoDecoder::streamResetCompleted);
        QSignalSpy codecSpy(&decoder, &oap::aa::VideoDecoder::streamCodecDetected);
        const auto nal = [](char first) {
            auto data = std::make_shared<QByteArray>();
            data->append('\x00'); data->append('\x00');
            data->append('\x00'); data->append('\x01');
            data->append(first); data->append('\x01');
            return std::const_pointer_cast<const QByteArray>(data);
        };

        // The old H.265 packet is emitted before the new stream boundary. It
        // must not be delivered later through Qt behind that reset barrier.
        handler.videoFrameData(nal('\x42'), 0);
        handler.streamStarted(1, 0);
        handler.videoFrameData(nal('\x67'), 0);

        QTRY_COMPARE(resetSpy.count(), 1);
        QTRY_VERIFY(codecSpy.count() >= 1);
        bool sawNewStreamH264 = false;
        for (const auto& emission : codecSpy) {
            if (emission[0].toULongLong() == 1ULL) {
                QCOMPARE(emission[1].toInt(), static_cast<int>(AV_CODEC_ID_H264));
                sawNewStreamH264 = true;
            }
        }
        QVERIFY(sawNewStreamH264);
        orch.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void testInitialState() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
        QVERIFY(orch.videoDecoder() != nullptr);
        QVERIFY(orch.inputHandler() != nullptr);
    }

    void testLegacyConstructorKeepsMainCompatibilityAndDisablesCluster() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);

        QCOMPARE(orch.videoDecoder(),
                 oap::aa::AndroidAutoOrchestratorTestAccess::mainDisplay(orch).decoder());
        QCOMPARE(orch.inputHandler()->channelId(), oaa::ChannelId::Input);
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::videoHandler(orch).channelId(),
                 oaa::ChannelId::Video);
        QVERIFY(orch.clusterDisplay() != nullptr);
        QCOMPARE(orch.clusterDisplay()->state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Disabled));
        QCOMPARE(orch.clusterDisplay()->decoder(), nullptr);
    }

    void testProductionGalSelectionIsIndependentOfCluster_data() {
        QTest::addColumn<QString>("configuredVersion");
        QTest::addColumn<QByteArray>("expectedVersionRequest");

        QTest::newRow("explicit-legacy")
            << QStringLiteral("1.7")
            << QByteArray::fromHex("00030006000100010007");
        QTest::newRow("accepted-modern")
            << QStringLiteral("4.3")
            << QByteArray::fromHex("00030006000100040003");
    }

    void testProductionGalSelectionIsIndependentOfCluster() {
        QFETCH(QString, configuredVersion);
        QFETCH(QByteArray, expectedVersionRequest);

        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        oap::YamlConfig yaml;
        QVERIFY(yaml.setValueByPath("connection.gal_version",
                                    configuredVersion));
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, &yaml);
        QCOMPARE(orch.clusterDisplay()->state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Disabled));

        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);
        QTcpSocket socket;
        socket.connectToHost(
            QHostAddress::LocalHost,
            oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch));
        QVERIFY(socket.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));
        QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable()
                                    >= expectedVersionRequest.size(),
                                 1000);
        QCOMPARE(socket.read(expectedVersionRequest.size()),
                 expectedVersionRequest);

        orch.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void testEnabledClusterRegistrationAndGlobalIsolation() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        const oap::aa::ProjectedClusterConfig clusterConfig{
            true, oap::aa::ProjectedSetupFocus::ProjectedNoInput};
        oap::aa::AndroidAutoOrchestrator orch(
            &cfg, nullptr, nullptr, clusterConfig);
        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);

        QTcpSocket socket;
        socket.connectToHost(
            QHostAddress::LocalHost,
            oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch));
        QVERIFY(socket.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));
        auto* session = oap::aa::AndroidAutoOrchestratorTestAccess::session(orch);
        QVERIFY(session);
        QSignalSpy sentSpy(session->messenger(), &oaa::Messenger::messageSent);

        QByteArray versionResponse(6, '\0');
        qToBigEndian<uint16_t>(4, reinterpret_cast<uchar*>(versionResponse.data()));
        qToBigEndian<uint16_t>(3, reinterpret_cast<uchar*>(versionResponse.data() + 2));
        qToBigEndian<uint16_t>(0, reinterpret_cast<uchar*>(versionResponse.data() + 4));
        session->messenger()->messageReceived(
            0, 0x0002, versionResponse, 0, oaa::MessageType::Specific);
        session->messenger()->handshakeComplete();
        session->messenger()->messageReceived(
            0, 0x0005, QByteArray(), 0, oaa::MessageType::Specific);
        QCOMPARE(session->state(), oaa::SessionState::Active);

        QByteArray discoveryPayload;
        for (const auto& emission : sentSpy) {
            if (emission[0].value<uint8_t>() == oaa::ChannelId::Control
                && emission[1].value<uint16_t>() == 0x0006) {
                discoveryPayload = emission[2].toByteArray();
            }
        }
        QVERIFY(!discoveryPayload.isEmpty());
        oaa::proto::messages::ServiceDiscoveryResponse discovery;
        QVERIFY(discovery.ParseFromArray(discoveryPayload.constData(),
                                         discoveryPayload.size()));
        QList<int> channelIds;
        for (const auto& channel : discovery.channels())
            channelIds.append(channel.channel_id());
        QVERIFY(channelIds.contains(oaa::ChannelId::ClusterVideo));
        QVERIFY(channelIds.contains(oaa::ChannelId::ClusterInput));

        QSignalSpy openedSpy(session, &oaa::AASession::channelOpened);
        for (uint8_t channelId : {oaa::ChannelId::ClusterVideo,
                                  oaa::ChannelId::ClusterInput}) {
            oaa::proto::messages::ChannelOpenRequest request;
            request.set_channel_id(channelId);
            request.set_priority(1);
            QByteArray payload(request.ByteSizeLong(), '\0');
            QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
            session->messenger()->messageReceived(
                channelId, 0x0007, payload, 0, oaa::MessageType::Control);
        }
        QCOMPARE(openedSpy.count(), 2);
        QCOMPARE(orch.clusterDisplay()->state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::WaitingForFrames));

        auto* clusterHandler = orch.clusterDisplay()->videoHandler();
        QSignalSpy clusterSentSpy(clusterHandler,
                                  &oaa::IChannelHandler::sendRequested);
        oaa::proto::messages::AVChannelSetupRequest setup;
        setup.set_media_codec_type(
            oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP);
        QByteArray setupPayload(setup.ByteSizeLong(), '\0');
        QVERIFY(setup.SerializeToArray(setupPayload.data(), setupPayload.size()));
        clusterHandler->onMessage(oaa::AVMessageId::SETUP_REQUEST, setupPayload);
        QCOMPARE(clusterSentSpy.count(), 2);
        oaa::proto::messages::AVChannelSetupResponse setupResponse;
        const QByteArray responsePayload = clusterSentSpy[0][2].toByteArray();
        QVERIFY(setupResponse.ParseFromArray(responsePayload.constData(),
                                             responsePayload.size()));
        QCOMPARE(setupResponse.configs_size(), 1);

        oap::aa::AndroidAutoOrchestratorTestAccess::setConnectedForFocusTest(orch);
        const int connectedState = orch.connectionState();
        orch.clusterDisplay()->videoHandler()->videoFocusChanged(
            oaa::proto::enums::VideoFocusMode::NATIVE, false);
        QCOMPARE(orch.connectionState(), connectedState);

        session->channelOpenRejected(oaa::ChannelId::ClusterVideo);
        QCOMPARE(orch.clusterDisplay()->state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rejected));
        QCOMPARE(orch.connectionState(), connectedState);

        oap::aa::AndroidAutoOrchestratorTestAccess::mainDisplay(orch)
            .videoHandler()->videoFocusChanged(
                oaa::proto::enums::VideoFocusMode::NATIVE, false);
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Backgrounded));

        orch.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void testClusterProfileStagesDisconnectedAndActivatesOnConnection() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        const oap::aa::ProjectedClusterConfig clusterConfig{
            true, oap::aa::ProjectedSetupFocus::ProjectedNoInput};
        oap::aa::AndroidAutoOrchestrator orch(
            &cfg, nullptr, nullptr, clusterConfig);

        QVERIFY(orch.clusterDisplay()->applyClusterProfile({
            {QStringLiteral("resolution"), QStringLiteral("720p")},
            {QStringLiteral("content_width"), 600},
            {QStringLiteral("content_height"), 400},
            {QStringLiteral("native_turn_card_available"), true},
        }));
        QVERIFY(!oap::aa::AndroidAutoOrchestratorTestAccess::pendingReconnect(orch));
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::projectedClusterConfig(orch)
                     .profile.resolution,
                 QStringLiteral("720p"));
        QCOMPARE(orch.clusterDisplay()->viewportEncodedWidth(), 800);

        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);
        QTcpSocket socket;
        socket.connectToHost(
            QHostAddress::LocalHost,
            oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch));
        QVERIFY(socket.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));
        QTRY_VERIFY_WITH_TIMEOUT(socket.bytesAvailable() >= 10, 1000);
        QCOMPARE(socket.readAll(), QByteArray::fromHex("00030006000100040003"));
        QCOMPARE(orch.clusterDisplay()->viewportEncodedWidth(), 1280);
        QCOMPARE(orch.clusterDisplay()->viewportContentWidth(), 600);
        QCOMPARE(orch.clusterDisplay()->viewportContentHeight(), 400);
        QVERIFY(orch.clusterDisplay()->requestedNativeTurnCardAvailable());

        orch.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void testClusterProfileRequestsReconnectOnlyWhileActive() {
        StubConfigService cfg;
        const oap::aa::ProjectedClusterConfig clusterConfig{
            true, oap::aa::ProjectedSetupFocus::ProjectedNoInput};
        oap::aa::AndroidAutoOrchestrator orch(
            &cfg, nullptr, nullptr, clusterConfig);

        oap::aa::AndroidAutoOrchestratorTestAccess::setConnectedForFocusTest(orch);
        QVERIFY(orch.clusterDisplay()->applyClusterProfile({
            {QStringLiteral("dpi"), 160},
        }));
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::pendingReconnect(orch));
    }

    void testTeardownEndsBothDisplaysAndNextMainGenerationStarts() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        const oap::aa::ProjectedClusterConfig clusterConfig{
            true, oap::aa::ProjectedSetupFocus::ProjectedNoInput};
        oap::aa::AndroidAutoOrchestrator orch(
            &cfg, nullptr, nullptr, clusterConfig);
        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);

        auto connectOne = [&orch](QTcpSocket& socket) {
            socket.connectToHost(
                QHostAddress::LocalHost,
                oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch));
            QVERIFY(socket.waitForConnected());
            QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));
        };

        QTcpSocket firstSocket;
        connectOne(firstSocket);
        auto& mainDisplay =
            oap::aa::AndroidAutoOrchestratorTestAccess::mainDisplay(orch);
        auto& clusterDisplay =
            oap::aa::AndroidAutoOrchestratorTestAccess::clusterDisplay(orch);
        QSignalSpy mainReset(mainDisplay.decoder(),
                             &oap::aa::VideoDecoder::streamResetCompleted);
        QSignalSpy clusterReset(clusterDisplay.decoder(),
                                &oap::aa::VideoDecoder::streamResetCompleted);
        QSignalSpy mainEnded(mainDisplay.decoder(),
                             &oap::aa::VideoDecoder::streamEnded);
        QSignalSpy clusterEnded(clusterDisplay.decoder(),
                                &oap::aa::VideoDecoder::streamEnded);

        mainDisplay.videoHandler()->streamStarted(1, 0);
        clusterDisplay.videoHandler()->streamStarted(2, 0);
        QTRY_COMPARE(mainReset.count(), 1);
        QTRY_COMPARE(clusterReset.count(), 1);
        const quint64 oldMainGeneration = mainReset[0][0].toULongLong();
        const quint64 oldClusterGeneration = clusterReset[0][0].toULongLong();

        orch.stop();
        QTRY_VERIFY(mainEnded.count() >= 1);
        QTRY_VERIFY(clusterEnded.count() >= 1);
        mainDisplay.decoder()->frameReadyForGeneration(oldMainGeneration);
        clusterDisplay.decoder()->frameReadyForGeneration(oldClusterGeneration);
        QCoreApplication::processEvents();
        QCOMPARE(mainDisplay.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Disconnected));
        QCOMPARE(clusterDisplay.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Disconnected));

        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);
        QTcpSocket secondSocket;
        connectOne(secondSocket);
        mainDisplay.videoHandler()->streamStarted(3, 0);
        QTRY_COMPARE(mainReset.count(), 2);
        QVERIFY(mainReset[1][0].toULongLong() > oldMainGeneration);

        orch.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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
#ifdef HAS_BLUETOOTH
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::discoveryTcpPort(orch),
                 static_cast<quint16>(15277));
#endif

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

    void testActiveAndBackgroundedSessionsRejectReplacement() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);

        const quint16 port = oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch);
        QVERIFY(port != 0);
#ifdef HAS_BLUETOOTH
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::discoveryTcpPort(orch), port);
#endif

        QTcpSocket firstSocket;
        firstSocket.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(firstSocket.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));
        auto* firstSession = oap::aa::AndroidAutoOrchestratorTestAccess::session(orch);

        QByteArray versionResponse(6, '\0');
        qToBigEndian<uint16_t>(4, reinterpret_cast<uchar*>(versionResponse.data()));
        qToBigEndian<uint16_t>(3, reinterpret_cast<uchar*>(versionResponse.data() + 2));
        qToBigEndian<uint16_t>(0, reinterpret_cast<uchar*>(versionResponse.data() + 4));
        firstSession->messenger()->messageReceived(
            0, 0x0002, versionResponse, 0, oaa::MessageType::Specific);
        QCOMPARE(firstSession->state(), oaa::SessionState::TLSHandshake);

        firstSession->messenger()->handshakeComplete();
        QCOMPARE(firstSession->state(), oaa::SessionState::ServiceDiscovery);
        firstSession->messenger()->messageReceived(
            0, 0x0005, QByteArray(), 0, oaa::MessageType::Specific);
        QCOMPARE(firstSession->state(), oaa::SessionState::Active);
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Connected));
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::watchdogActive(orch));

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

        const QString connectedStatus = orch.statusMessage();
        oap::aa::AndroidAutoOrchestratorTestAccess::notifyPhoneWillConnect(orch);
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Connected));
        QCOMPARE(orch.statusMessage(), connectedStatus);
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::watchdogActive(orch));

        QTcpSocket activeReplacement;
        activeReplacement.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(activeReplacement.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::session(orch), firstSession);
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::activePeerPort(orch),
                 firstSocket.localPort());
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Connected));

        orch.requestExitToCar();
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Backgrounded));
        const QString backgroundedStatus = orch.statusMessage();
        oap::aa::AndroidAutoOrchestratorTestAccess::notifyPhoneWillConnect(orch);
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Backgrounded));
        QCOMPARE(orch.statusMessage(), backgroundedStatus);
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::watchdogActive(orch));

        QTcpSocket backgroundedReplacement;
        backgroundedReplacement.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(backgroundedReplacement.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::session(orch), firstSession);
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::activePeerPort(orch),
                 firstSocket.localPort());

        // Rejection must not clear persistent handler subscription/wire state.
        sensorHandler.pushNightMode(false);
        QCOMPARE(sendSpy.count(), 1);

        // Drain anything already in flight so the post-stop read observes only
        // the graceful shutdown traffic.
        while (firstSocket.waitForReadyRead(50)) {}
        firstSocket.readAll();

        QElapsedTimer stopElapsed;
        stopElapsed.start();
        orch.stop();
        QVERIFY2(stopElapsed.elapsed() < 500, "stop() reintroduced a blocking wait");
        // The buffered ShutdownRequest must be flushed to the wire before the
        // synchronous transport teardown aborts the socket.
        QVERIFY2(firstSocket.waitForReadyRead(1000),
                 "graceful ShutdownRequest never reached the wire");
        QVERIFY(!firstSocket.readAll().isEmpty());
        QCOMPARE(orch.connectionState(),
                 static_cast<int>(oap::aa::AndroidAutoOrchestrator::Disconnected));
        QVERIFY(!oap::aa::AndroidAutoOrchestratorTestAccess::admissionOpen(orch));
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch), 0);

        QTcpSocket afterStop;
        afterStop.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(!afterStop.waitForConnected(200));
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

    void testAssistantMicOpenCaptureGainAndClose() {
        StubConfigService cfg;
        oap::YamlConfig yaml;
        yaml.setMicrophoneGain(2.0);
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, &yaml);
        oap::AudioStreamHandle fakeHandle;
        oap::aa::TestMicCaptureBackend backend;
        backend.openResult = &fakeHandle;
        oap::aa::AndroidAutoOrchestratorTestAccess::installMicCaptureBackend(
            orch, backend);

        auto& handler = oap::aa::AndroidAutoOrchestratorTestAccess::avInputHandler(orch);
        handler.onChannelOpened();
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        oaa::proto::messages::AVInputOpenRequest open;
        open.set_open(true);
        open.set_max_unacked(1);
        QByteArray payload(open.ByteSizeLong(), '\0');
        QVERIFY(open.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 1);
        oaa::proto::messages::AVInputOpenResponse response;
        QByteArray responsePayload = sendSpy[0][2].toByteArray();
        QVERIFY(response.ParseFromArray(responsePayload.constData(), responsePayload.size()));
        QCOMPARE(response.value(), 0);
        QCOMPARE(backend.name, QStringLiteral("AA Assistant Microphone"));
        QCOMPARE(backend.sampleRate, 16000);
        QCOMPARE(backend.channels, 1);
        QCOMPARE(backend.bitDepth, 16);
        QCOMPARE(backend.callbacks.size(), 1);
        QCOMPARE(backend.errors.size(), 1);
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::micBridgeActive(orch));
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::micCaptureHandle(orch),
                 &fakeHandle);

        QByteArray pcm(oap::aa::AVInputCaptureBridge::FrameBytes, '\0');
        for (qsizetype offset = 0; offset < pcm.size(); offset += 2) {
            qToLittleEndian<qint16>(1000,
                reinterpret_cast<uchar*>(pcm.data()) + offset);
        }
        sendSpy.clear();
        backend.callbacks.front()(reinterpret_cast<const uint8_t*>(pcm.constData()),
                                  pcm.size());
        QTRY_COMPARE(sendSpy.count(), 1);
        const QByteArray mediaPayload = sendSpy[0][2].toByteArray();
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::AV_MEDIA_WITH_TIMESTAMP));
        QCOMPARE(qFromLittleEndian<qint16>(
                     reinterpret_cast<const uchar*>(mediaPayload.constData()) + 8),
                 qint16(2000));

        oaa::proto::messages::AVInputOpenRequest close;
        close.set_open(false);
        payload.resize(close.ByteSizeLong());
        QVERIFY(close.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);

        QCOMPARE(backend.closeCount, 1);
        QCOMPARE(backend.closedHandle, &fakeHandle);
        QVERIFY(backend.bridgeActiveDuringClose);
        QVERIFY(!oap::aa::AndroidAutoOrchestratorTestAccess::micBridgeActive(orch));
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::micCaptureHandle(orch),
                 nullptr);
    }

    void testAssistantMicImmediateOpenFailureIsHonest() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        oap::aa::TestMicCaptureBackend backend;
        oap::aa::AndroidAutoOrchestratorTestAccess::installMicCaptureBackend(
            orch, backend);

        auto& handler = oap::aa::AndroidAutoOrchestratorTestAccess::avInputHandler(orch);
        handler.onChannelOpened();
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        oaa::proto::messages::AVInputOpenRequest open;
        open.set_open(true);
        QByteArray payload(open.ByteSizeLong(), '\0');
        QVERIFY(open.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 1);
        oaa::proto::messages::AVInputOpenResponse response;
        const QByteArray responsePayload = sendSpy[0][2].toByteArray();
        QVERIFY(response.ParseFromArray(responsePayload.constData(), responsePayload.size()));
        QCOMPARE(response.value(), 1);
        QCOMPARE(backend.closeCount, 0);
        QVERIFY(!oap::aa::AndroidAutoOrchestratorTestAccess::micBridgeActive(orch));
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::micCaptureHandle(orch),
                 nullptr);
    }

    void testAssistantMicRuntimeErrorStopsCurrentGenerationOnly() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        oap::AudioStreamHandle firstHandle;
        oap::AudioStreamHandle secondHandle;
        oap::aa::TestMicCaptureBackend backend;
        backend.openResult = &firstHandle;
        oap::aa::AndroidAutoOrchestratorTestAccess::installMicCaptureBackend(
            orch, backend);

        auto& handler = oap::aa::AndroidAutoOrchestratorTestAccess::avInputHandler(orch);
        handler.onChannelOpened();
        oaa::proto::messages::AVInputOpenRequest open;
        open.set_open(true);
        QByteArray payload(open.ByteSizeLong(), '\0');
        QVERIFY(open.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);
        QCOMPARE(backend.errors.size(), 1);

        backend.openResult = &secondHandle;
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);
        QCOMPARE(backend.closeCount, 1);
        QCOMPARE(backend.closedHandle, &firstHandle);
        QCOMPARE(backend.errors.size(), 2);
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::micBridgeActive(orch));

        backend.errors[0]();
        QCOMPARE(backend.closeCount, 1);
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::micCaptureHandle(orch),
                 &secondHandle);

        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        backend.errors[1]();
        QCOMPARE(backend.closeCount, 2);
        QCOMPARE(backend.closedHandle, &secondHandle);
        QVERIFY(backend.bridgeActiveDuringClose);
        QVERIFY(!oap::aa::AndroidAutoOrchestratorTestAccess::micBridgeActive(orch));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 1));
        QCOMPARE(sendSpy.count(), 0); // no unsolicited failure response
    }

    void testAssistantMicTeardownClosesCaptureBeforeBridge() {
        StubConfigService cfg;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, nullptr, nullptr);
        oap::AudioStreamHandle fakeHandle;
        oap::aa::TestMicCaptureBackend backend;
        backend.openResult = &fakeHandle;
        oap::aa::AndroidAutoOrchestratorTestAccess::installMicCaptureBackend(
            orch, backend);

        auto& handler = oap::aa::AndroidAutoOrchestratorTestAccess::avInputHandler(orch);
        handler.onChannelOpened();
        oaa::proto::messages::AVInputOpenRequest open;
        open.set_open(true);
        QByteArray payload(open.ByteSizeLong(), '\0');
        QVERIFY(open.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);

        orch.stop();
        QCOMPARE(backend.closeCount, 1);
        QVERIFY(backend.bridgeActiveDuringClose);
        QVERIFY(!oap::aa::AndroidAutoOrchestratorTestAccess::micBridgeActive(orch));
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 1));
    }

    void testAudioServiceTeardownQuiescesAssistantCapture() {
        StubConfigService cfg;
        auto audioService = std::make_unique<oap::AudioService>();
        oap::aa::AndroidAutoOrchestrator orch(&cfg, audioService.get(), nullptr);
        oap::AudioStreamHandle fakeHandle;
        oap::aa::TestMicCaptureBackend backend;
        backend.openResult = &fakeHandle;
        oap::aa::AndroidAutoOrchestratorTestAccess::installMicCaptureBackend(
            orch, backend);

        auto& handler = oap::aa::AndroidAutoOrchestratorTestAccess::avInputHandler(orch);
        handler.onChannelOpened();
        oaa::proto::messages::AVInputOpenRequest open;
        open.set_open(true);
        QByteArray payload(open.ByteSizeLong(), '\0');
        QVERIFY(open.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::AVMessageId::INPUT_OPEN_REQUEST, payload);
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::micBridgeActive(orch));

        audioService.reset();

        QCOMPARE(backend.closeCount, 1);
        QCOMPARE(backend.closedHandle, &fakeHandle);
        QVERIFY(backend.bridgeActiveDuringClose);
        QVERIFY(!oap::aa::AndroidAutoOrchestratorTestAccess::micBridgeActive(orch));
        QCOMPARE(oap::aa::AndroidAutoOrchestratorTestAccess::micCaptureHandle(orch),
                 nullptr);
        QVERIFY(!handler.sendMicData(QByteArray(320, '\x42'), 1));

        orch.stop();
        QCOMPARE(backend.closeCount, 1);
    }

    void testQueuedPlaybackFrameIsHarmlessAfterAudioServiceTeardown() {
        StubConfigService cfg;
        cfg.values["connection.tcp_port"] = 0;
        FakePlaybackAudioService audioService;
        oap::aa::AndroidAutoOrchestrator orch(&cfg, &audioService, nullptr);
        orch.start();
        oap::aa::AndroidAutoOrchestratorTestAccess::disableAutomaticAccept(orch);

        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost,
            oap::aa::AndroidAutoOrchestratorTestAccess::listenerPort(orch));
        QVERIFY(socket.waitForConnected());
        QVERIFY(oap::aa::AndroidAutoOrchestratorTestAccess::acceptNextConnection(orch));
        QCOMPARE(audioService.created, 3);

        auto& handler =
            oap::aa::AndroidAutoOrchestratorTestAccess::mediaAudioHandler(orch);
        handler.audioDataReceived(QByteArray(64, '\x55'), 1);
        oap::aa::AndroidAutoOrchestratorTestAccess::simulateAudioServiceTeardown(orch);
        QCoreApplication::sendPostedEvents(&orch, QEvent::MetaCall);

        QCOMPARE(audioService.writes, 0);
        orch.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};

QTEST_MAIN(TestAndroidAutoOrchestrator)
#include "test_aa_orchestrator.moc"
